#include "luna.h"
#include <jinjac.h>
#include <parameter.h>

static lua_State *tpl_L = NULL;
static int tpl_table_idx = 0;
static int in_search = 0;

static int push_cache_val(lua_State *L, int val_idx) {
	if (val_idx < 0) {
		val_idx = lua_gettop(L) + val_idx + 1;
	}
	// Store value in registry to prevent GC during rendering
	lua_getfield(L, LUA_REGISTRYINDEX, "LUNA_TPL_CACHE");
	int next_idx = lua_objlen(L, -1) + 1;
	lua_pushvalue(L, val_idx);
	lua_rawseti(L, -2, next_idx);
	lua_pop(L, 1); // Cache table
	return next_idx;
}

static int p_search(char* key, int32_t* priv_key, int* is_array) {
	if (!tpl_L || in_search) {
		return 0;
	}

	char *buf = strdup(key);
	char *saveptr;
	char *first = strtok_r(buf, ".", &saveptr);
	char *rest = strtok_r(NULL, "", &saveptr);

	int found = 0;
	lua_getfield(tpl_L, tpl_table_idx, first);
	if (lua_isnil(tpl_L, -1)) {
		lua_pop(tpl_L, 1);
		int64_t pk;
		in_search = 1; // Prevent recursion
		int s_found = parameter_search(first, &pk, NULL);
		in_search = 0;
		if (s_found) {
			jinjac_parameter p;
			if (parameter_get(pk, &p) == J_OK) {
				if (p.type == TYPE_STRING && strncmp(p.value.type_string, "__LUA_REF__", 11) == 0) {
					int cache_idx = atoi(p.value.type_string + 11);
					lua_getfield(tpl_L, LUA_REGISTRYINDEX, "LUNA_TPL_CACHE");
					lua_rawgeti(tpl_L, -1, cache_idx);
					lua_remove(tpl_L, -2);
					found = 1;
				} else if (!rest) {
					if (p.type == TYPE_INT) {
						lua_pushinteger(tpl_L, p.value.type_int);
					} else if (p.type == TYPE_DOUBLE) {
						lua_pushnumber(tpl_L, p.value.type_double);
					} else if (p.type == TYPE_STRING) {
						lua_pushstring(tpl_L, p.value.type_string);
					} else {
						lua_pushnil(tpl_L);
					}
					if (!lua_isnil(tpl_L, -1)) {
						found = 1;
					}
				}
			}
		}
	} else {
		found = 1;
	}

	if (found && rest) {
		char *rest_buf = strdup(rest);
		char *r_saveptr;
		char *token = strtok_r(rest_buf, ".", &r_saveptr);
		while (token) {
			if (lua_istable(tpl_L, -1)) {
				lua_getfield(tpl_L, -1, token);
				lua_remove(tpl_L, -2);
			} else {
				lua_pop(tpl_L, 1);
				lua_pushnil(tpl_L);
				break;
			}
			if (lua_isnil(tpl_L, -1)) {
				break;
			}
			token = strtok_r(NULL, ".", &r_saveptr);
		}
		free(rest_buf);
		if (lua_isnil(tpl_L, -1)) {
			found = 0;
		}
	}

	free(buf);
	if (found && !lua_isnil(tpl_L, -1)) {
		*priv_key = push_cache_val(tpl_L, -1);
		if (is_array) {
			*is_array = lua_istable(tpl_L, -1);
		}
		lua_pop(tpl_L, 1);
		return 1;
	}
	if (!lua_isnil(tpl_L, -1)) {
		lua_pop(tpl_L, 1);
	}
	return 0;
}

static J_STATUS p_get(int32_t priv_key, jinjac_parameter* param) {
	lua_getfield(tpl_L, LUA_REGISTRYINDEX, "LUNA_TPL_CACHE");
	lua_rawgeti(tpl_L, -1, priv_key);
	int type = lua_type(tpl_L, -1);
	if (type == LUA_TNUMBER) {
		double val = lua_tonumber(tpl_L, -1);
		if (val == (int32_t)val) {
			param->type = TYPE_INT;
			param->value.type_int = (int32_t)val;
		} else {
			param->type = TYPE_DOUBLE;
			param->value.type_double = val;
		}
	} else if (type == LUA_TSTRING) {
		param->type = TYPE_STRING;
		param->value.type_string = (char *)lua_tostring(tpl_L, -1);
	} else if (type == LUA_TBOOLEAN) {
		param->type = TYPE_INT;
		param->value.type_int = lua_toboolean(tpl_L, -1);
	} else if (type == LUA_TTABLE) {
		param->type = TYPE_STRING;
		lua_pushfstring(tpl_L, "__LUA_REF__%d", priv_key);
		param->value.type_string = (char *)lua_tostring(tpl_L, -1);
	} else {
		lua_pop(tpl_L, 2);
		return J_ERROR;
	}
	return J_OK;
}

static int p_array_getProperties(int32_t priv_key, jinjac_parameter_type* type, int32_t* nb_item) {
	lua_getfield(tpl_L, LUA_REGISTRYINDEX, "LUNA_TPL_CACHE");
	lua_rawgeti(tpl_L, -1, priv_key);
	if (!lua_istable(tpl_L, -1)) {
		lua_pop(tpl_L, 2);
		return 0;
	}
	*nb_item = lua_objlen(tpl_L, -1);
	if (*nb_item > 0) {
		lua_rawgeti(tpl_L, -1, 1);
		int elem_type = lua_type(tpl_L, -1);
		if (elem_type == LUA_TNUMBER) {
			*type = TYPE_DOUBLE;
		} else if (elem_type == LUA_TSTRING) {
			*type = TYPE_STRING;
		} else if (elem_type == LUA_TTABLE) {
			*type = TYPE_STRING;
		} else {
			*type = TYPE_INT;
		}
		lua_pop(tpl_L, 1);
	} else {
		*type = TYPE_INT;
	}
	lua_pop(tpl_L, 2);
	return 1;
}

static J_STATUS p_array_getValue(int32_t priv_key, int32_t offset, jinjac_parameter_value* v) {
	lua_getfield(tpl_L, LUA_REGISTRYINDEX, "LUNA_TPL_CACHE");
	lua_rawgeti(tpl_L, -1, priv_key);
	lua_rawgeti(tpl_L, -1, offset + 1);
	int type = lua_type(tpl_L, -1);
	if (type == LUA_TNUMBER) {
		v->type_double = lua_tonumber(tpl_L, -1);
	} else if (type == LUA_TSTRING) {
		v->type_string = (char *)lua_tostring(tpl_L, -1);
	} else if (type == LUA_TBOOLEAN) {
		v->type_int = lua_toboolean(tpl_L, -1);
	} else if (type == LUA_TTABLE) {
		int next_idx = push_cache_val(tpl_L, -1);
		lua_pushfstring(tpl_L, "__LUA_REF__%d", next_idx);
		v->type_string = (char *)lua_tostring(tpl_L, -1);
	}
	return J_OK;
}

static int l_template_render(lua_State *L) {
	const char *template_str = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	int top = lua_gettop(L);

	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "LUNA_TPL_CACHE");

	tpl_L = L;
	tpl_table_idx = 2;
	in_search = 0;

	jinjac_init();
	jinjac_parameter_callback cb = {
		.search = p_search,
		.get = p_get,
		.array_getProperties = p_array_getProperties,
		.array_getValue = p_array_getValue
	};
	jinjac_parameter_register(&cb);

	char *result = NULL;
	int32_t result_size = 0;
	jinjac_render_with_buffer((char *)template_str, strlen(template_str), &result, &result_size);

	if (result) {
		lua_pushlstring(L, result, result_size);
		free(result);
	} else {
		lua_pushnil(L);
	}

	jinjac_destroy();

	// Cleanup cache
	lua_pushnil(L);

	lua_setfield(L, LUA_REGISTRYINDEX, "LUNA_TPL_CACHE");
	tpl_L = NULL;

	lua_insert(L, top + 1);
	lua_settop(L, top + 1);

	return 1;
}

static const struct luaL_Reg template_lib[] = {
	{"render", l_template_render},
	{NULL, NULL}
};

int luaopen_template(lua_State *L) {
	luaL_register(L, "template", template_lib);
	return 1;
}
