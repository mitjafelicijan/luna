#include "luna.h"

cJSON *lua_to_cjson(lua_State *L, int idx) {
	int type = lua_type(L, idx);
	switch (type) {
		case LUA_TNIL: return cJSON_CreateNull();
		case LUA_TBOOLEAN: return cJSON_CreateBool(lua_toboolean(L, idx));
		case LUA_TNUMBER: return cJSON_CreateNumber(lua_tonumber(L, idx));
		case LUA_TSTRING: return cJSON_CreateString(lua_tostring(L, idx));
		case LUA_TTABLE:
		  {
			  // Check index 1 to distinguish array from object
			  int is_array = 0;
			  lua_rawgeti(L, idx, 1);
			  if (!lua_isnil(L, -1)) {
				  is_array = 1;
			  }
			  lua_pop(L, 1);

			  if (is_array) {
				  cJSON *arr = cJSON_CreateArray();
				  int n = lua_objlen(L, idx);
				  for (int i = 1; i <= n; i++) {
					  lua_rawgeti(L, idx, i);
					  cJSON_AddItemToArray(arr, lua_to_cjson(L, -1));
					  lua_pop(L, 1);
				  }
				  return arr;
			  } else {
				  cJSON *obj = cJSON_CreateObject();
				  lua_pushnil(L);
				  while (lua_next(L, idx < 0 ? idx - 1 : idx) != 0) {
					  const char *key;
					  if (lua_type(L, -2) == LUA_TSTRING) {
						  key = lua_tostring(L, -2);
					  } else {
						  lua_pushvalue(L, -2);
						  key = lua_tostring(L, -1);
						  lua_pop(L, 1);
					  }
					  cJSON_AddItemToObject(obj, key, lua_to_cjson(L, -1));
					  lua_pop(L, 1);
				  }
				  return obj;
			  }
		  }
		default: return cJSON_CreateNull();
	}
}

void cjson_to_lua(lua_State *L, cJSON *item) {
	if (cJSON_IsNull(item)) {
		lua_pushnil(L);
	} else if (cJSON_IsBool(item)) {
		lua_pushboolean(L, cJSON_IsTrue(item));
	} else if (cJSON_IsNumber(item)) {
		lua_pushnumber(L, item->valuedouble);
	} else if (cJSON_IsString(item)) {
		lua_pushstring(L, item->valuestring);
	} else if (cJSON_IsArray(item)) {
		lua_newtable(L);
		int i = 1;
		cJSON *child;
		cJSON_ArrayForEach(child, item) {
			cjson_to_lua(L, child);
			lua_rawseti(L, -2, i++);
		}
	} else if (cJSON_IsObject(item)) {
		lua_newtable(L);
		cJSON *child;
		cJSON_ArrayForEach(child, item) {
			cjson_to_lua(L, child);
			lua_setfield(L, -2, child->string);
		}
	} else {
		lua_pushnil(L);
	}
}

static int l_json_encode(lua_State *L) {
	cJSON *json = lua_to_cjson(L, 1);
	char *json_str = cJSON_PrintUnformatted(json);
	lua_pushstring(L, json_str);
	free(json_str);
	cJSON_Delete(json);
	return 1;
}

static int l_json_decode(lua_State *L) {
	const char *s = luaL_checkstring(L, 1);
	cJSON *json = cJSON_Parse(s);
	if (!json) {
		lua_pushnil(L);
		return 1;
	}
	cjson_to_lua(L, json);
	cJSON_Delete(json);
	return 1;
}

static const struct luaL_Reg json_lib[] = {
	{"encode", l_json_encode},
	{"decode", l_json_decode},
	{NULL, NULL}
};

int luaopen_json(lua_State *L) {
	luaL_newlib(L, json_lib);
	return 1;
}
