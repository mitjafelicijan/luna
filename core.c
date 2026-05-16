#include "luna.h"

int main(int argc, char **argv) {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	// Register core library (timers, metadata)
	luaopen_core(L);
	lua_setglobal(L, "core");

	// Register json library
	luaopen_json(L);
	lua_setglobal(L, "json");

	// Register http library
	luaopen_http(L);
	lua_setglobal(L, "http");

	// Register request library
	luaopen_request(L);
	lua_setglobal(L, "request");

	// Register env library
	luaopen_env(L);
	lua_setglobal(L, "env");

	// Register crypto library
	luaopen_crypto(L);
	lua_setglobal(L, "crypto");

	// Register sqlite library
	luaopen_sqlite(L);
	lua_setglobal(L, "sqlite");

	// Register log library
	luaopen_log(L);
	lua_setglobal(L, "log");

	// Register assert library
	luaopen_assert(L);
	lua_setglobal(L, "assert");

	// Register fs library
	luaopen_fs(L);
	lua_setglobal(L, "fs");

	// Register ini library
	luaopen_ini(L);
	lua_setglobal(L, "ini");

	// Register process library
	luaopen_process(L);
	lua_setglobal(L, "process");

	// Register template library
	luaopen_template(L);
	lua_setglobal(L, "template");

	// Register path library
	luaopen_path(L);
	lua_setglobal(L, "path");

	// Register stash library
	luaopen_stash(L);
	lua_setglobal(L, "stash");

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <script.lua>\n", argv[0]);
		return 1;
	}

	// Execute lua script
	if (luaL_dofile(L, argv[1]) != LUA_OK) {
		fprintf(stderr, "Error: %s\n", lua_tostring(L, -1));
		return 1;
	}

	// Run event loop
	ev_run(EV_DEFAULT, 0);

	lua_close(L);
	return 0;
}
