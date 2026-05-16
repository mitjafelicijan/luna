#include "luna.h"

static int l_assert_equal(lua_State *L) {
	int n = lua_gettop(L);
	if (n < 2) {
		return luaL_error(L, "assert.equal expects at least 2 arguments");
	}

	if (!lua_equal(L, 1, 2)) {
		const char *msg = (n >= 3) ? luaL_checkstring(L, 3) : "Assertion failed";

		lua_getglobal(L, "tostring");
		lua_pushvalue(L, 1);
		lua_call(L, 1, 1);
		const char *s1 = lua_tostring(L, -1);

		lua_getglobal(L, "tostring");
		lua_pushvalue(L, 2);
		lua_call(L, 1, 1);
		const char *s2 = lua_tostring(L, -1);

		return luaL_error(L, "%s: expected %s, got %s", msg, s1, s2);
	}
	return 0;
}

static int l_assert_type(lua_State *L) {
	const char *expected = luaL_checkstring(L, 1);
	const char *actual = lua_typename(L, lua_type(L, 2));
	const char *msg = (lua_gettop(L) >= 3) ? luaL_checkstring(L, 3) : "Type assertion failed";

	if (strcmp(expected, actual) != 0) {
		return luaL_error(L, "%s: expected type %s, got %s", msg, expected, actual);
	}
	return 0;
}

static int l_assert_truthy(lua_State *L) {
	if (!lua_toboolean(L, 1)) {
		const char *msg = (lua_gettop(L) >= 2) ? luaL_checkstring(L, 2) : "Expected value to be truthy";
		return luaL_error(L, msg);
	}
	return 0;
}

static const struct luaL_Reg assert_lib[] = {
	{"equal", l_assert_equal},
	{"type", l_assert_type},
	{"truthy", l_assert_truthy},
	{NULL, NULL}
};

int luaopen_assert(lua_State *L) {
	luaL_newlib(L, assert_lib);
	return 1;
}
