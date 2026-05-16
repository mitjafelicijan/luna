#include "luna.h"

typedef struct {
	ev_timer timer;
	lua_State *L;
	int callback_ref;
} Timer;

static void timer_cb(struct ev_loop *loop, ev_timer *w, int revents) {
	Timer *lt = (Timer *)w;
	lua_State *L = lt->L;
	lua_rawgeti(L, LUA_REGISTRYINDEX, lt->callback_ref);
	if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
		fprintf(stderr, "Error calling timer callback: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
	if (!ev_is_active(w)) {
		luaL_unref(L, LUA_REGISTRYINDEX, lt->callback_ref);
		free(lt);
	}
}

static int l_set_timeout(lua_State *L) {
	double delay = luaL_checknumber(L, 1) / 1000.0;
	luaL_checktype(L, 2, LUA_TFUNCTION);
	Timer *lt = malloc(sizeof(Timer));
	lt->L = L;
	lua_pushvalue(L, 2);
	lt->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	ev_timer_init(&lt->timer, timer_cb, delay, 0.);
	ev_timer_start(EV_DEFAULT, &lt->timer);
	return 0;
}

static const struct luaL_Reg core_lib[] = {
	{"set_timeout", l_set_timeout},
	{NULL, NULL}
};

int luaopen_core(lua_State *L) {
	luaL_newlib(L, core_lib);

	lua_pushstring(L, "0.1.0");
	lua_setfield(L, -2, "version");

#ifdef __linux__
	lua_pushstring(L, "linux");
#elif defined(__APPLE__)
	lua_pushstring(L, "darwin");
#elif defined(_WIN32)
	lua_pushstring(L, "windows");
#else
	lua_pushstring(L, "unknown");
#endif
	lua_setfield(L, -2, "platform");

#if defined(__x86_64__) || defined(_M_X64)
	lua_pushstring(L, "x64");
#elif defined(__i386__) || defined(_M_IX86)
	lua_pushstring(L, "x86");
#elif defined(__arm__) || defined(_M_ARM)
	lua_pushstring(L, "arm");
#elif defined(__aarch64__)
	lua_pushstring(L, "arm64");
#else
	lua_pushstring(L, "unknown");
#endif
	lua_setfield(L, -2, "arch");

	return 1;
}
