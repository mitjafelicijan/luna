#ifndef LUNA_H
#define LUNA_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <ev.h>
#include <cJSON.h>

const char *get_status_message(int code);
void set_nonblocking(int fd);

cJSON *lua_to_cjson(lua_State *L, int idx);
void cjson_to_lua(lua_State *L, cJSON *item);
int luaopen_json(lua_State *L);

int luaopen_core(lua_State *L);
int luaopen_log(lua_State *L);
int luaopen_assert(lua_State *L);
int luaopen_http(lua_State *L);
int luaopen_request(lua_State *L);
int luaopen_env(lua_State *L);
int luaopen_crypto(lua_State *L);
int luaopen_sqlite(lua_State *L);
int luaopen_fs(lua_State *L);
int luaopen_ini(lua_State *L);
int luaopen_process(lua_State *L);
int luaopen_template(lua_State *L);
int luaopen_path(lua_State *L);
int luaopen_stash(lua_State *L);

#endif
