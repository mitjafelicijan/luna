#include "luna.h"
#include <ctype.h>

static char *trim_whitespace(char *str) {
	char *end;
	while (isspace((unsigned char)*str)) str++;
	if (*str == 0) {
		return str;
	}
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;
	end[1] = '\0';
	return str;
}

static int l_env_load(lua_State *L) {
	const char *filename = luaL_optstring(L, 1, ".env");
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "could not open %s", filename);
		return 2;
	}

	char line[1024];
	lua_newtable(L);
	while (fgets(line, sizeof(line), fp)) {
		char *trimmed = trim_whitespace(line);
		if (trimmed[0] == '\0' || trimmed[0] == '#') {
			continue;
		}

		char *sep = strchr(trimmed, '=');
		if (sep) {
			*sep = '\0';
			char *key = trim_whitespace(trimmed);
			char *val = trim_whitespace(sep + 1);

			// Remove quotes if present
			if ((val[0] == '"' && val[strlen(val) - 1] == '"') || (val[0] == '\'' && val[strlen(val) - 1] == '\'')) {
				val[strlen(val) - 1] = '\0';
				val++;
			}

			setenv(key, val, 1);
			lua_pushstring(L, val);
			lua_setfield(L, -2, key);
		}
	}

	fclose(fp);
	return 1;
}

static int l_env_get(lua_State *L) {
	const char *name = luaL_checkstring(L, 1);
	const char *val = getenv(name);
	if (val) {
		lua_pushstring(L, val);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static const struct luaL_Reg env_lib[] = {
	{"load", l_env_load},
	{"get", l_env_get},
	{NULL, NULL}
};

int luaopen_env(lua_State *L) {
	luaL_newlib(L, env_lib);
	return 1;
}
