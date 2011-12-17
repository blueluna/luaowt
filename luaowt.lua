#!/usr/bin/lua

local io = require("io")
local lfs = require("lfs")
local http = require("socket.http")
local ltn12 = require("ltn12")
local lpeg = require("lpeg")
local json = require("json")
require("logging.file")

local logger = logging.file("/tmp/luaowt.log", "!%Y-%m-%d %H:%M:%S")
logger:setLevel(logging.INFO)

local pattern_hex = lpeg.R('09', 'AF', 'af')
local pattern_hex_byte = lpeg.R('09', 'AF', 'af')
local device_pattern = pattern_hex^2 * lpeg.P('-') * pattern_hex^12
local pattern_hex_block = pattern_hex^2 * lpeg.P(' ')^1
local w1_pattern1 = pattern_hex_block^9 * lpeg.P(': crc=') * pattern_hex^2 * lpeg.P(' ') * lpeg.C('YES') + lpeg.C('NO')
local w1_pattern2 = pattern_hex_block^9 * lpeg.P('t=') * lpeg.C(lpeg.R('09')^1)
local pattern_name_mangler = lpeg.C(pattern_hex^2) * lpeg.P('-') * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2) * lpeg.C(pattern_hex^-2)

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

function jsonrpc_query(url, id, method, params)
	q = json.encode({jsonrpc="2.0", id=id, method=method, params=params})
	return jsonrpc_post(url, q)
end

function send_temperature(url, device, temperature, timestamp)
	d, h, c, m = jsonrpc_query(url, 0, 'upload_temperature', {device=device, temperature=temperature, timestamp=timestamp})
	if c ~= 200 then
		logger:warn('Failed to upload: '..m)
	end
end

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

function name_mangler(device)
	a, b, c, d, e, f, g = pattern_name_mangler:match(device)
	return string.upper(a..'.'..g..f..e..d..c..b)
end

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
	for name, result in pairs(results) do
		if result.found then
			if result.crc then
				local device_name = name_mangler(name)
				logger:info('Push tempterature '..result.value..' for '..name..' also called '..device_name)
				send_temperature('http://coldstar.mine.nu/owtweb/rpc', device_name, result.value, result.timestamp)
			else
				logger:warn('CRC failed for '..name)
			end
		end
	end
	logger:info('done')
end

read_devices('/sys/devices/w1 bus master')
read_devices('.')

