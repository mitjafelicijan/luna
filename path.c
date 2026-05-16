#include "luna.h"
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>

static int l_path_join(lua_State *L) {
	int n = lua_gettop(L);
	if (n == 0) {
		lua_pushstring(L, "");
		return 1;
	}

	char result[PATH_MAX] = "";
	size_t current_len = 0;

	for (int i = 1; i <= n; i++) {
		size_t len;
		const char *part = luaL_checklstring(L, i, &len);
		if (len == 0) {
			continue;
		}

		if (current_len > 0 && result[current_len - 1] != '/' && part[0] != '/') {
			if (current_len + 1 >= PATH_MAX) {
				break;
			}
			result[current_len++] = '/';
			result[current_len] = '\0';
		}

		if (current_len + len >= PATH_MAX) {
			len = PATH_MAX - current_len - 1;
		}

		strncat(result, part, len);
		current_len += len;
	}

	lua_pushstring(L, result);
	return 1;
}

static int l_path_basename(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	char tmp[PATH_MAX];
	snprintf(tmp, sizeof(tmp), "%s", path);
	lua_pushstring(L, basename(tmp));
	return 1;
}

static int l_path_dirname(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	char tmp[PATH_MAX];
	snprintf(tmp, sizeof(tmp), "%s", path);
	lua_pushstring(L, dirname(tmp));
	return 1;
}

static int l_path_extname(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	const char *dot = strrchr(path, '.');
	if (!dot || dot == path) {
		lua_pushstring(L, "");
	} else {
		lua_pushstring(L, dot);
	}
	return 1;
}

static int l_path_resolve(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	char resolved[PATH_MAX];
	if (realpath(path, resolved)) {
		lua_pushstring(L, resolved);
		return 1;
	}
	lua_pushnil(L);
	lua_pushstring(L, strerror(errno));
	return 2;
}

static const struct luaL_Reg path_lib[] = {
	{"join", l_path_join},
	{"basename", l_path_basename},
	{"dirname", l_path_dirname},
	{"extname", l_path_extname},
	{"resolve", l_path_resolve},
	{NULL, NULL}
};

int luaopen_path(lua_State *L) {
	luaL_newlib(L, path_lib);
	return 1;
}
