#include "luna.h"
#include <sqlite3.h>
#include <time.h>

typedef struct {
	sqlite3 *db;
	ev_timer cleanup_watcher;
	int interval;
} MemDB;

static MemDB mdb = {0};

static void cleanup_cb(struct ev_loop *loop, ev_timer *w, int revents) {
	if (!mdb.db) {
		return;
	}

	sqlite3_stmt *stmt;
	const char *sql = "DELETE FROM kv WHERE e > 0 AND e < ?;";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

static int check_init(lua_State *L) {
	if (mdb.db) {
		return 0;
	}

	if (sqlite3_open(":memory:", &mdb.db) != SQLITE_OK) {
		return luaL_error(L, "failed to open in-memory database: %s", sqlite3_errmsg(mdb.db));
	}

	const char *schema = "CREATE TABLE IF NOT EXISTS kv (k TEXT PRIMARY KEY, v TEXT, e INTEGER);"
		"CREATE INDEX IF NOT EXISTS idx_expiry ON kv(e);";
	if (sqlite3_exec(mdb.db, schema, NULL, NULL, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to create schema: %s", sqlite3_errmsg(mdb.db));
	}

	if (mdb.interval <= 0) {
		mdb.interval = 60;
	}

	mdb.cleanup_watcher.data = &mdb;
	ev_timer_init(&mdb.cleanup_watcher, cleanup_cb, mdb.interval, mdb.interval);
	ev_timer_start(EV_DEFAULT, &mdb.cleanup_watcher);

	return 0;
}

static int l_stash_cleanup(lua_State *L) {
	int interval = (int)luaL_checkinteger(L, 1);
	if (interval <= 0) {
		return luaL_error(L, "interval must be greater than 0");
	}

	mdb.interval = interval;

	if (mdb.db) {
		ev_timer_stop(EV_DEFAULT, &mdb.cleanup_watcher);
		ev_timer_set(&mdb.cleanup_watcher, interval, interval);
		ev_timer_start(EV_DEFAULT, &mdb.cleanup_watcher);
	} else {
		check_init(L);
	}

	return 0;
}

static int l_stash_set(lua_State *L) {
	check_init(L);
	const char *key = luaL_checkstring(L, 1);
	int ttl = (int)luaL_optinteger(L, 3, 0);

	cJSON *json = lua_to_cjson(L, 2);
	char *value_str = cJSON_PrintUnformatted(json);
	cJSON_Delete(json);

	sqlite3_stmt *stmt;
	const char *sql = "INSERT OR REPLACE INTO kv (k, v, e) VALUES (?, ?, ?);";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		free(value_str);
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}

	sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, value_str, -1, SQLITE_TRANSIENT);

	sqlite3_int64 expiry = 0;
	if (ttl > 0) {
		expiry = (sqlite3_int64)time(NULL) + ttl;
	}
	sqlite3_bind_int64(stmt, 3, expiry);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		free(value_str);
		return luaL_error(L, "failed to execute statement: %s", sqlite3_errmsg(mdb.db));
	}

	sqlite3_finalize(stmt);
	free(value_str);
	return 0;
}

static int l_stash_get(lua_State *L) {
	check_init(L);
	const char *key = luaL_checkstring(L, 1);

	sqlite3_stmt *stmt;
	const char *sql = "SELECT v FROM kv WHERE k = ? AND (e > ? OR e = 0);";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}

	sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *value_str = (const char *)sqlite3_column_text(stmt, 0);
		cJSON *json = cJSON_Parse(value_str);
		if (json) {
			cjson_to_lua(L, json);
			cJSON_Delete(json);
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}

	sqlite3_finalize(stmt);
	return 1;
}

static int l_stash_del(lua_State *L) {
	check_init(L);
	const char *key = luaL_checkstring(L, 1);

	sqlite3_stmt *stmt;
	const char *sql = "DELETE FROM kv WHERE k = ?;";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}

	sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return luaL_error(L, "failed to execute statement: %s", sqlite3_errmsg(mdb.db));
	}

	sqlite3_finalize(stmt);
	return 0;
}

static int l_stash_exists(lua_State *L) {
	check_init(L);
	const char *key = luaL_checkstring(L, 1);

	sqlite3_stmt *stmt;
	const char *sql = "SELECT 1 FROM kv WHERE k = ? AND (e > ? OR e = 0);";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}
	sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));

	int exists = (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);
	lua_pushboolean(L, exists);
	return 1;
}

static int l_stash_incr(lua_State *L) {
	check_init(L);
	const char *key = luaL_checkstring(L, 1);
	double amount = luaL_optnumber(L, 2, 1);

	sqlite3_stmt *stmt;
	const char *get_sql = "SELECT v, e FROM kv WHERE k = ?;";
	if (sqlite3_prepare_v2(mdb.db, get_sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}
	sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);

	double current_val = 0;
	sqlite3_int64 expiry = 0;

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		expiry = sqlite3_column_int64(stmt, 1);
		if (expiry == 0 || expiry > time(NULL)) {
			current_val = atof((const char *)sqlite3_column_text(stmt, 0));
		}
	}
	sqlite3_finalize(stmt);

	double new_val = current_val + amount;
	char val_buf[64];
	snprintf(val_buf, sizeof(val_buf), "%g", new_val);

	const char *set_sql = "INSERT OR REPLACE INTO kv (k, v, e) VALUES (?, ?, ?);";
	if (sqlite3_prepare_v2(mdb.db, set_sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}
	sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, val_buf, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 3, expiry);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	lua_pushnumber(L, new_val);
	return 1;
}

static int l_stash_decr(lua_State *L) {
	double amount = luaL_optnumber(L, 2, 1);
	lua_settop(L, 1);
	lua_pushnumber(L, -amount);
	return l_stash_incr(L);
}

static int l_stash_keys(lua_State *L) {
	check_init(L);
	const char *pattern = luaL_optstring(L, 1, "%");

	sqlite3_stmt *stmt;
	const char *sql = "SELECT k FROM kv WHERE k LIKE ? AND (e > ? OR e = 0);";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}
	sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));

	lua_newtable(L);
	int i = 1;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		lua_pushstring(L, (const char *)sqlite3_column_text(stmt, 0));
		lua_rawseti(L, -2, i++);
	}
	sqlite3_finalize(stmt);
	return 1;
}

static int l_stash_ttl(lua_State *L) {
	check_init(L);
	const char *key = luaL_checkstring(L, 1);

	sqlite3_stmt *stmt;
	const char *sql = "SELECT e FROM kv WHERE k = ? AND (e > ? OR e = 0);";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}
	sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_int64 expiry = sqlite3_column_int64(stmt, 0);
		if (expiry == 0) {
			lua_pushnumber(L, -1);
		} else {
			lua_pushnumber(L, (double)(expiry - time(NULL)));
		}
	} else {
		lua_pushnumber(L, -2);
	}
	sqlite3_finalize(stmt);
	return 1;
}

static int l_stash_expire(lua_State *L) {
	check_init(L);
	const char *key = luaL_checkstring(L, 1);
	int ttl = (int)luaL_checkinteger(L, 2);

	sqlite3_stmt *stmt;
	const char *sql = "UPDATE kv SET e = ? WHERE k = ? AND (e > ? OR e = 0);";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}

	sqlite3_int64 expiry = (ttl > 0) ? (time(NULL) + ttl) : 0;
	sqlite3_bind_int64(stmt, 1, expiry);
	sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));

	sqlite3_step(stmt);
	int changes = sqlite3_changes(mdb.db);
	sqlite3_finalize(stmt);

	lua_pushboolean(L, changes > 0);
	return 1;
}

static int l_stash_clear(lua_State *L) {
	check_init(L);
	sqlite3_exec(mdb.db, "DELETE FROM kv;", NULL, NULL, NULL);
	return 0;
}

static int l_stash_count(lua_State *L) {
	check_init(L);
	sqlite3_stmt *stmt;
	const char *sql = "SELECT COUNT(*) FROM kv WHERE (e > ? OR e = 0);";
	if (sqlite3_prepare_v2(mdb.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return luaL_error(L, "failed to prepare statement: %s", sqlite3_errmsg(mdb.db));
	}
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		lua_pushinteger(L, sqlite3_column_int64(stmt, 0));
	} else {
		lua_pushinteger(L, 0);
	}
	sqlite3_finalize(stmt);
	return 1;
}

static const struct luaL_Reg stash_lib[] = {
	{"cleanup", l_stash_cleanup},
	{"set", l_stash_set},
	{"get", l_stash_get},
	{"del", l_stash_del},
	{"exists", l_stash_exists},
	{"incr", l_stash_incr},
	{"decr", l_stash_decr},
	{"keys", l_stash_keys},
	{"ttl", l_stash_ttl},
	{"expire", l_stash_expire},
	{"clear", l_stash_clear},
	{"count", l_stash_count},
	{NULL, NULL}
};

int luaopen_stash(lua_State *L) {
	luaL_newlib(L, stash_lib);
	return 1;
}
