/*
 * Lua module for system information
 *
 * Copyright 2011 Erik Svensson (erik.public@gmail.com)
 * Licensed under the MIT license.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "lua.h"
#include "lauxlib.h"

static int sysinfo_lua_ipaddresses(lua_State *L)
{
   	struct ifreq *ifr = 0;
   	struct ifreq *ifrptr = 0;
   	struct ifconf ifc;
   	struct sockaddr_in *sin;
	int s, i, numif, iface_len, new_table;
	const char* iface;
	iface_len = 0;
	
	/* use argument if it is a string */
	if (lua_type(L, 1) == LUA_TSTRING) {
		iface = lua_tostring(L, 1);
		iface_len = strlen(iface);
	}
	
	/* clear data structure */
   	memset(&ifc, 0, sizeof(ifc));
   	ifc.ifc_ifcu.ifcu_req = NULL;
   	ifc.ifc_len = 0;

   	/* create a socket */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
   		return 0;
	}
	/* request data, to get size */
	if ((ioctl(s, SIOCGIFCONF, &ifc)) < 0) {
   		return 0;
	}
	/* allocate buffer, use double the size if the next call to ioctl should be bigger. */
	ifr = malloc(ifc.ifc_len*2);
	if (ifr == NULL) {
   		return 0;
	}
	/* add buffer to structure and request data again */
   	ifc.ifc_ifcu.ifcu_req = ifr;
	if ((ioctl(s, SIOCGIFCONF, &ifc)) < 0) {
   		return 0;
	}
	/* close socket */
	close(s);

	/* parse data and fill lua table */
	lua_newtable(L);
	numif = ifc.ifc_len / sizeof(struct ifreq);
	ifrptr = ifr;
	for (i = 0; i < numif; i++) {
		if (iface_len == 0 || strncmp(ifrptr->ifr_name, iface, iface_len) == 0) {
			sin = (struct sockaddr_in *)(&(ifrptr->ifr_addr));
			lua_pushstring(L, inet_ntoa(sin->sin_addr));
			lua_setfield(L, -2, ifrptr->ifr_name);
		}
		ifrptr++;
	}
	free(ifr);
	return 1;
}

static const struct luaL_Reg sysinfo_lua_reg_f[] = {
	{"ipaddresses", sysinfo_lua_ipaddresses},
	{NULL, NULL}
};

int luaopen_sysinfo(lua_State *L);
int luaopen_sysinfo(lua_State *L)
{
	luaL_register(L, "sysinfo", sysinfo_lua_reg_f);
	return 1;
}
