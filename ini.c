#include "luna.h"
#include <iniparser.h>

static int l_ini_load(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	dictionary *ini = iniparser_load(filename);
	if (!ini) {
		lua_pushnil(L);
		lua_pushstring(L, "failed to load ini file");
		return 2;
	}

	lua_newtable(L);
	int nsec = iniparser_getnsec(ini);
	for (int i = 0; i < nsec; i++) {
		const char *secname = iniparser_getsecname(ini, i);
		lua_newtable(L);

		int nkeys = iniparser_getsecnkeys(ini, secname);
		const char **keys = malloc(nkeys * sizeof(char *));
		iniparser_getseckeys(ini, secname, keys);

		for (int j = 0; j < nkeys; j++) {
			const char *val = iniparser_getstring(ini, keys[j], NULL);
			// Iniparser keys are returned as "section:key"
			const char *colon = strchr(keys[j], ':');
			const char *key = colon ? colon + 1 : keys[j];

			lua_pushstring(L, val);
			lua_setfield(L, -2, key);
		}

		free(keys);
		lua_setfield(L, -2, secname);
	}

	iniparser_freedict(ini);
	return 1;
}

static int l_ini_save(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	dictionary *ini = dictionary_new(0);

	lua_pushnil(L);
	while (lua_next(L, 2) != 0) {
		if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TTABLE) {
			const char *section = lua_tostring(L, -2);

			// Create section
			iniparser_set(ini, section, NULL);

			int sec_idx = lua_gettop(L);
			lua_pushnil(L);
			while (lua_next(L, sec_idx) != 0) {
				const char *key = NULL;
				if (lua_type(L, -2) == LUA_TSTRING) {
					key = lua_tostring(L, -2);
				} else {
					lua_pushvalue(L, -2);
					key = lua_tostring(L, -1);
					lua_pop(L, 1);
				}

				const char *val = NULL;
				if (lua_type(L, -1) == LUA_TSTRING) {
					val = lua_tostring(L, -1);
				} else {
					lua_pushvalue(L, -1);
					val = lua_tostring(L, -1);
					lua_pop(L, 1);
				}

				char entry[512];
				snprintf(entry, sizeof(entry), "%s:%s", section, key);
				iniparser_set(ini, entry, val);

				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}

	FILE *f = fopen(filename, "w");
	if (!f) {
		dictionary_del(ini);
		lua_pushboolean(L, 0);
		lua_pushstring(L, "failed to open file for writing");
		return 2;
	}

	iniparser_dump_ini(ini, f);
	fclose(f);
	dictionary_del(ini);

	lua_pushboolean(L, 1);
	return 1;
}

static const struct luaL_Reg ini_lib[] = {
	{"load", l_ini_load},
	{"save", l_ini_save},
	{NULL, NULL}
};

int luaopen_ini(lua_State *L) {
	luaL_newlib(L, ini_lib);
	return 1;
}
