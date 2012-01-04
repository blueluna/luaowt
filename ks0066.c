/*
 * Lua module for KS0066 over GPIO
 *
 * Copyright 2011 Erik Svensson (erik.public@gmail.com)
 * Licensed under the MIT license.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lua.h"
#include "lauxlib.h"

typedef struct ks0066_ctx_ {
	uint16_t	gpio_en;
	uint16_t	gpio_wr;
	uint16_t	gpio_rs;
	uint16_t	gpio_d[8];
	char        buffer[40];
} ks0066_t;

#define KS0066_LINES	2
#define KS0066_CHARS	20
#define KS0066_MAXCHARS	(KS0066_LINES * KS0066_CHARS)

#define KS0066_METANAME	"net.coldstar.ks0066"
#define KS0066_INITFILE	"/tmp/ks0066_init"

#define KS0066_DIR_IN	0x01
#define KS0066_DIR_OUT	0x02

static void delay(uint32_t useconds)
{
	struct timeval to;
	to.tv_sec = 0;
	to.tv_usec = useconds;
	select(0, 0, 0, 0, &to);
}

static FILE* ks0066_gpio_open_dir(uint16_t index, const char *mode)
{
	char name[256];
	if (snprintf(name, 255, "/sys/class/gpio/gpio%d/direction", index) > 0) {
		return fopen(name, mode);
	}
	else {
		return 0;
	}
}

static int ks0066_gpio_get_dir(uint16_t index, uint8_t *dir)
{
	FILE *fp = ks0066_gpio_open_dir(index, "r");
	char dir_string[5] = {0};
	int result = -1;
	if (fp) {
		if (fgets(dir_string, 3, fp)) {
			if (strncmp(dir_string, "out", 3) == 0) {
				*dir = KS0066_DIR_OUT;
				result = 0;
			}
			else if (strncmp(dir_string, "in", 2) == 0) {
				*dir = KS0066_DIR_IN;
				result = 0;
			}
		}
		fclose(fp);
	}
	return result;
}

static int ks0066_gpio_set_dir(uint16_t index, const uint8_t dir)
{
	FILE *fp = ks0066_gpio_open_dir(index, "w");
	int result = -1;
	if (fp) {
		if (dir == KS0066_DIR_IN) {
			fprintf(fp, "in");
			result = 0;
		}
		else if (dir == KS0066_DIR_OUT) {
			fprintf(fp, "out");
			result = 0;
		}
		fclose(fp);
	}
	return result;
}

static FILE* ks0066_gpio_open_value(uint16_t index, const char *mode)
{
	char name[256];
	if (snprintf(name, 255, "/sys/class/gpio/gpio%d/value", index) > 0) {
		return fopen(name, mode);
	}
	else {
		return 0;
	}
}

static int ks0066_gpio_get(uint16_t index, uint8_t *value)
{
	FILE *fp = ks0066_gpio_open_value(index, "r");
	int int_value;
	if (fp) {
		if (fscanf(fp, "%d", &int_value) == 1) {
			*value = int_value; /* 1,0 */
		}
		fclose(fp);
		return 0;
	}
	return -1;
}

static int ks0066_gpio_set(uint16_t index, const uint8_t value)
{
	FILE *fp = ks0066_gpio_open_value(index, "w");
	if (fp) {
		fprintf(fp, "%d", value > 0 ? 1 : 0);
		fclose(fp);
		return 0;
	}
	return -1;
}

static int ks0066_read(ks0066_t *ctx, uint8_t rs, uint8_t *data)
{
	uint8_t shift = 0, value = 0;
	int i = 0;
	for (i = 0; i < 8; i++) {
		ks0066_gpio_set_dir(ctx->gpio_d[i], KS0066_DIR_IN);
	}
	ks0066_gpio_set(ctx->gpio_rs, rs > 0 ? 1 : 0);
	ks0066_gpio_set(ctx->gpio_wr, 1);
	ks0066_gpio_set(ctx->gpio_en, 1);
	delay(1);
	for (i = 0; i < 8; i++) {
		ks0066_gpio_get(ctx->gpio_d[i], &value);
		shift = ((value & 0x01) | shift) << 1;
	}
	ks0066_gpio_set(ctx->gpio_en, 0);
	
	delay(1);
	*data = shift;
	return 0;
}

static int ks0066_prepare_write(ks0066_t *ctx)
{
	int i = 0;
	for (i = 0; i < 8; i++) {
		ks0066_gpio_set_dir(ctx->gpio_d[i], KS0066_DIR_OUT);
	}
	return 0;
}

static int ks0066_fast_write(ks0066_t *ctx, uint8_t rs, uint8_t data)
{
	uint8_t shift = data;
	int i = 0;
	for (i = 0; i < 8; i++) {
		ks0066_gpio_set(ctx->gpio_d[i], shift & 0x01);
		shift = shift >> 1;
	}
	ks0066_gpio_set(ctx->gpio_rs, rs > 0 ? 1 : 0);
	ks0066_gpio_set(ctx->gpio_wr, 0);
	ks0066_gpio_set(ctx->gpio_en, 1);
	delay(1);
	ks0066_gpio_set(ctx->gpio_en, 0);
	delay(45);
	return 0;
}

static int ks0066_write(ks0066_t *ctx, uint8_t rs, uint8_t data)
{
	uint8_t shift = data;
	int i = 0;
	for (i = 0; i < 8; i++) {
		ks0066_gpio_set_dir(ctx->gpio_d[i], KS0066_DIR_OUT);
		ks0066_gpio_set(ctx->gpio_d[i], shift & 0x01);
		shift = shift >> 1;
	}
	ks0066_gpio_set(ctx->gpio_rs, rs > 0 ? 1 : 0);
	ks0066_gpio_set(ctx->gpio_wr, 0);
	ks0066_gpio_set(ctx->gpio_en, 1);
	delay(1);
	ks0066_gpio_set(ctx->gpio_en, 0);
	delay(45);
	return 0;
}

static int ks0066_write_instruction(ks0066_t *ctx, uint8_t s)
{
	return ks0066_write(ctx, 0, s);
}

static int ks0066_fast_instruction(ks0066_t *ctx, uint8_t s)
{
	return ks0066_fast_write(ctx, 0, s);
}

static int ks0066_read_data(ks0066_t *ctx, uint8_t *s)
{
	return ks0066_read(ctx, 1, s);
}

static int ks0066_write_data(ks0066_t *ctx, uint8_t s)
{
	return ks0066_write(ctx, 1, s);
}

static int ks0066_fast_write_data(ks0066_t *ctx, uint8_t s)
{
	return ks0066_fast_write(ctx, 1, s);
}

static inline int ks0066_clear(ks0066_t *ctx)
{
	int result = ks0066_write_instruction(ctx, 0x01);
	delay(1600);
	return result;
}

static inline int ks0066_return(ks0066_t *ctx)
{
	int result = ks0066_write_instruction(ctx, 0x02);
	delay(1600);
	return result;
}

static inline int ks0066_fast_return(ks0066_t *ctx)
{
	int result = ks0066_fast_instruction(ctx, 0x02);
	delay(1600);
	return result;
}

static int ks0066_char(ks0066_t *ctx, char c)
{
	uint8_t s = 0x20; /* space */
	if (c >= 0x20 && c < 0x7f) {
		s = c;
	}
	return ks0066_write_data(ctx, s);
}

static int ks0066_fast_char(ks0066_t *ctx, char c)
{
	uint8_t s = 0x20; /* space */
	if (c >= 0x20 && c < 0x7f) {
		s = c;
	}
	return ks0066_fast_write_data(ctx, s);
}

static int ks0066_text(ks0066_t *ctx, const char *text, const int len)
{
	const char *ptr = 0, *end = 0;
	int i = 0, max_len;
	if (len <= 0) {
		return -1;
	}
	max_len = len > KS0066_MAXCHARS ? KS0066_MAXCHARS : len;
	ptr = text;
	end = text + max_len;
	ks0066_prepare_write(ctx);
	ks0066_fast_instruction(ctx, 0x06); /* increment */
	ks0066_fast_return(ctx);
	while (ptr < end) {
		if (i == KS0066_CHARS && KS0066_LINES > 1) {
			ks0066_fast_instruction(ctx, 0xC0); /* move to second line */
		}
		ks0066_fast_char(ctx, *ptr);
		ptr++;
		i++;
	}
	return 0;
}

static int ks0066_init(ks0066_t *ctx)
{
	FILE *fp;
	ks0066_prepare_write(ctx);
	ks0066_fast_instruction(ctx, 0x3C); /* 8-bit, 2-line, 5x11 font */
	ks0066_fast_instruction(ctx, 0x0C); /* display on */
	ks0066_fast_instruction(ctx, 0x01); /* clear */
	ks0066_fast_instruction(ctx, 0x06); /* increment */
	fp = fopen(KS0066_INITFILE, "w");
	fprintf(fp, "OK");
	fclose(fp);
	return 0;
}

static int ks0066_lua_new(lua_State *L)
{
	int result;
	struct stat st;
	ks0066_t *ctx = 0;
	ctx = (ks0066_t *)lua_newuserdata(L, sizeof(ks0066_t));
	ctx->gpio_en = 66;
	ctx->gpio_wr = 65;
	ctx->gpio_rs = 64;
	ctx->gpio_d[0] = 67;
	ctx->gpio_d[1] = 68;
	ctx->gpio_d[2] = 69;
	ctx->gpio_d[3] = 72;
	ctx->gpio_d[4] = 73;
	ctx->gpio_d[5] = 74;
	ctx->gpio_d[6] = 75;
	ctx->gpio_d[7] = 80;
	memset(ctx->buffer, 0, 40);

	ks0066_gpio_set_dir(ctx->gpio_en, KS0066_DIR_OUT);
	ks0066_gpio_set_dir(ctx->gpio_wr, KS0066_DIR_OUT);
	ks0066_gpio_set_dir(ctx->gpio_rs, KS0066_DIR_OUT);

	result = stat(KS0066_INITFILE, &st);
	if (result != 0) {
		ks0066_init(ctx);
	}

	luaL_getmetatable(L, KS0066_METANAME);
	lua_setmetatable(L, -2);

	return 1;
}

static inline ks0066_t* ks0066_lua_userdata(lua_State *L, const int index)
{
	return (ks0066_t *)luaL_checkudata(L, index, KS0066_METANAME);
}

static int ks0066_lua_write(lua_State *L)
{
	int32_t text_len = 0;
	ks0066_t *self = ks0066_lua_userdata(L, 1);
	const char* text = luaL_checkstring(L, 2);

	text_len = strlen(text);

	luaL_argcheck(L, self != NULL, 1, "'ks0066' expected");
	luaL_argcheck(L, text_len > 0 && text_len <= 40, 2, "invalid text length");

	ks0066_text(self, text, text_len);

	return 0;
}

static int ks0066_lua_clear(lua_State *L)
{
	ks0066_t *self = ks0066_lua_userdata(L, 1);
	
	luaL_argcheck(L, self != NULL, 1, "'ks0066' expected");
	
	ks0066_clear(self);

	return 0;
}

static int ks0066_lua_return(lua_State *L)
{
	ks0066_t *self = ks0066_lua_userdata(L, 1);
	
	luaL_argcheck(L, self != NULL, 1, "'ks0066' expected");
	
	ks0066_return(self);

	return 0;
}

static int ks0066_lua_get_data(lua_State *L)
{
	ks0066_t *self = ks0066_lua_userdata(L, 1);
	uint8_t data;
	
	luaL_argcheck(L, self != NULL, 1, "'ks0066' expected");
	
	ks0066_read_data(self, &data);

	lua_pushinteger(L, (int)(data));

	return 1;
}

static int ks0066_lua_set_data(lua_State *L)
{
	ks0066_t *self = ks0066_lua_userdata(L, 1);
	int data = luaL_checkinteger(L, 2);
	
	luaL_argcheck(L, self != NULL, 1, "'ks0066' expected");
	luaL_argcheck(L, data >= 0 && data < 256, 2, "integer between 0 and 255 expected");
	
	ks0066_write_data(self, data);

	return 0;
}

static int ks0066_lua_instruction(lua_State *L)
{
	ks0066_t *self = ks0066_lua_userdata(L, 1);
	int instruction = luaL_checkinteger(L, 2);
	
	luaL_argcheck(L, self != NULL, 1, "'ks0066' expected");
	luaL_argcheck(L, instruction >= 0 && instruction < 256, 2, "integer between 0 and 255 expected");
	
	ks0066_write_instruction(self, instruction);

	return 0;
}

static const struct luaL_Reg ks0066_lua_reg_f[] = {
	{"new", ks0066_lua_new},
	{NULL, NULL}
};

static const struct luaL_Reg ks0066_lua_reg_m[] = {
	{"clear", ks0066_lua_clear},
	{"return", ks0066_lua_return},
	{"get_data", ks0066_lua_get_data},
	{"set_data", ks0066_lua_set_data},
	{"instruction", ks0066_lua_instruction},
	{"write", ks0066_lua_write},
	{NULL, NULL}
};

int luaopen_ks0066(lua_State *L);
int luaopen_ks0066(lua_State *L)
{
	luaL_newmetatable(L, KS0066_METANAME);

	/* metatable.__index = metatable */
	lua_pushvalue(L, -1); /* duplicates the metatable */
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, ks0066_lua_reg_m);

	luaL_register(L, "ks0066", ks0066_lua_reg_f);
	return 1;
}
