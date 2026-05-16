#include "luna.h"
#include <time.h>

static int use_colors = 0;

static void get_timestamp(char *buf, size_t len) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
}

static int l_log(lua_State *L, const char *level, const char *color) {
	int n = lua_gettop(L);
	char timestamp[32];
	get_timestamp(timestamp, sizeof(timestamp));

	if (use_colors) {
		printf("%s[%s] [%s]%s ", color, timestamp, level, "\x1b[0m");
	} else {
		printf("[%s] [%s] ", timestamp, level);
	}

	for (int i = 1; i <= n; i++) {
		if (i > 1) {
			printf("\t");
		}
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TSTRING:
				printf("%s", lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				printf("%s", lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:
				printf("%g", lua_tonumber(L, i));
				break;
			default:
				printf("%s: %p", lua_typename(L, type), lua_topointer(L, i));
				break;
		}
	}
	printf("\n");
	fflush(stdout);
	return 0;
}

static int l_info(lua_State *L) {
	return l_log(L, "INFO ", "\x1b[32m"); // Green
}

static int l_warning(lua_State *L) {
	return l_log(L, "WARN ", "\x1b[33m"); // Yellow
}

static int l_error(lua_State *L) {
	int n = lua_gettop(L);
	char timestamp[32];
	get_timestamp(timestamp, sizeof(timestamp));

	if (use_colors) {
		fprintf(stderr, "\x1b[31m[%s] [ERROR]\x1b[0m ", timestamp);
	} else {
		fprintf(stderr, "[%s] [ERROR] ", timestamp);
	}

	for (int i = 1; i <= n; i++) {
		if (i > 1) {
			fprintf(stderr, "\t");
		}
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TSTRING:
				fprintf(stderr, "%s", lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				fprintf(stderr, "%s", lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:
				fprintf(stderr, "%g", lua_tonumber(L, i));
				break;
			default:
				fprintf(stderr, "%s: %p", lua_typename(L, type), lua_topointer(L, i));
				break;
		}
	}
	fprintf(stderr, "\n");
	return 0;
}

static int l_use_colors(lua_State *L) {
	use_colors = lua_toboolean(L, 1);
	return 0;
}

static const struct luaL_Reg log_lib[] = {
	{"info", l_info},
	{"warning", l_warning},
	{"error", l_error},
	{"use_colors", l_use_colors},
	{NULL, NULL}
};

int luaopen_log(lua_State *L) {
	luaL_newlib(L, log_lib);
	return 1;
}
