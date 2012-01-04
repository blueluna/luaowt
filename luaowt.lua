#!/usr/bin/lua

-------------------------------------------------------------------------------
-- Reading kernel 1w therm devices
--
-- @author Erik Svensson (erik.public@gmail.com)
-- @copyright 2011 Erik Svensson
-- Licensed under the MIT license.
-------------------------------------------------------------------------------

local io = require("io")
local lfs = require("lfs")
local http = require("socket.http")
local ltn12 = require("ltn12")
local lpeg = require("lpeg")
local json = require("json")
require("logging.rolling_file")
require("ks0066")
require("sysinfo")

-- logger
local logger = logging.rolling_file("/tmp/luaowt.log", 65536, 3, "!%Y-%m-%d %H:%M:%S")
logger:setLevel(logging.WARN)

-- PEGs
-- A hexadecimal number
local pattern_hex = lpeg.R('09', 'AF', 'af')
-- A 1w device as found in /sys/devices/w1 bus master, 'xx-xxxxxxxxxxxx'
local device_pattern = pattern_hex^2 * lpeg.P('-') * pattern_hex^12
-- A hex byte with a space 'xx '
local pattern_hex_block = pattern_hex^2 * lpeg.P(' ')^1
-- xx xx xx xx xx xx xx xx xx : crc=xx YES
local w1_pattern1 = pattern_hex_block^9 * lpeg.P(': crc=') * pattern_hex^2 * lpeg.P(' ') * lpeg.C('YES') + lpeg.C('NO')
-- xx xx xx xx xx xx xx xx xx t=nnnn
local w1_pattern2 = pattern_hex_block^9 * lpeg.P('t=') * lpeg.C(lpeg.P('-')^-1 * lpeg.R('09')^1)
-- helper for 11-223344556677 -> 11.776655443322
local pattern_name_mangler = lpeg.C(pattern_hex^2) * lpeg.P('-') * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2)
	* lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2)

-- post jsonrpc over http
function jsonrpc_post(url, data)
	local sink_t = {}
	local o, c, h, m = http.request{
		url = url,
		method = "POST",
		headers = {["Content-Type"]="application/json; charset=UTF-8", ["Content-Length"]=""..string.len(data)},
		source = ltn12.source.string(data),
		sink = ltn12.sink.table(sink_t)
	}
	return table.concat(sink_t), h, c, m
end

-- a jsonrpc 2.0 query over http
function jsonrpc_query(url, id, method, params)
	q = json.encode({jsonrpc="2.0", id=id, method=method, params=params})
	return jsonrpc_post(url, q)
end

-- send temperature to webservice
function send_temperature(url, device, temperature, timestamp)
	d, h, c, m = jsonrpc_query(url, 0, 'upload_temperature', {device=device, temperature=temperature
		, timestamp=timestamp})
	if c ~= 200 then
		logger:warn('Failed to upload: '..m)
	end
end

-- enumerate 1w devices
function enumerate_devices(path)
	devices = {}
	attr = lfs.attributes(path)	
	if attr and attr.mode == 'directory' then
		for file in lfs.dir(path) do
			if device_pattern:match(file) then
				attr = lfs.attributes(path..'/'..file..'/w1_slave')
				if attr then
					table.insert(devices, file)
				else
					logger:warn('Possible enumeration problem with '..file)
				end
			end
		end
	end
	return devices
end

-- fix the device name to match the web service
function name_mangler(device)
	-- 11-223344556677 -> 11.776655443322
	a, b, c, d, e, f, g = pattern_name_mangler:match(device)
	return string.upper(a..'.'..g..f..e..d..c..b)
end

function get_ipaddress()
	local ipaddress = ''
	local as = sysinfo.ipaddresses('eth0')
	for k,v in pairs(as) do
		ipaddress = v
	end
	return ipaddress
end

function lcd_write(temps)
	local ipaddress = 'IP: '..get_ipaddress()
	local string_diff = 20 - string.len(ipaddress)
	ipaddress = ipaddress..string.rep(' ', string_diff)
	local lcd = ks0066.new()
	lcd:write(ipaddress..string.sub(temps, 0, 20))
end

-- read 1w devices
function read_devices(path)
	results = {}
	logger:info("querying devices...")
	devices = enumerate_devices(path)
	for index, device in ipairs(devices) do
		fh = io.open(path..'/'..device..'/w1_slave', 'r')
		results[device] = {found=false, crc=false, value=0, timestamp=os.date('!%Y-%m-%dT%H:%M:%SZ')}
		for line in fh:lines() do
			local match = w1_pattern1:match(line)
			if match == 'YES' then
				results[device].crc = true
				results[device].found = true
			elseif match == 'NO' then
				results[device].found = true
			end
			local match = w1_pattern2:match(line)
			if match then
				results[device].value = match/1000.0
				results[device].found = true
			end
		end
	end
	local temps = 'Temp:'
	for name, result in pairs(results) do
		if result.found then
			if result.crc then
				local device_name = name_mangler(name)
				temps = temps..string.format('%5.1f ', result.value)
				logger:info('Push tempterature '..result.value..' for '..name..' also called '..device_name)
				send_temperature('http://coldstar.mine.nu/owtweb/rpc', device_name, result.value, result.timestamp)
			else
				logger:warn('CRC failed for '..name)
			end
		end
	end
	lcd_write(temps)
	logger:info('done')
end

read_devices('/sys/devices/w1 bus master')
