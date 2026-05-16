#include "luna.h"
#include <sqlite3.h>

#define SQLITE_METATABLE "luna.sqlite"

typedef struct {
	sqlite3 *db;
} SqliteDB;

static int l_sqlite_close(lua_State *L) {
	SqliteDB *ud = luaL_checkudata(L, 1, SQLITE_METATABLE);
	if (ud->db) {
		sqlite3_close(ud->db);
		ud->db = NULL;
	}
	return 0;
}

static int bind_value(lua_State *L, sqlite3_stmt *stmt, int sqlite_idx, int lua_idx) {
	int type = lua_type(L, lua_idx);
	switch (type) {
		case LUA_TNIL:
			return sqlite3_bind_null(stmt, sqlite_idx);
		case LUA_TNUMBER:
			return sqlite3_bind_double(stmt, sqlite_idx, lua_tonumber(L, lua_idx));
		case LUA_TBOOLEAN:
			return sqlite3_bind_int(stmt, sqlite_idx, lua_toboolean(L, lua_idx));
		case LUA_TSTRING: {
			  size_t len;
			  const char *str = lua_tolstring(L, lua_idx, &len);
			  return sqlite3_bind_text(stmt, sqlite_idx, str, (int)len, SQLITE_TRANSIENT);
		}
		default:
			return -1; // Unsupported type
	}
}

static int bind_params(lua_State *L, sqlite3_stmt *stmt, int start_idx) {
	int top = lua_gettop(L);
	if (top < start_idx) {
		return SQLITE_OK;
	}

	// Handle named params if single table passed
	if (top == start_idx && lua_istable(L, start_idx)) {
		lua_pushnil(L);
		while (lua_next(L, start_idx) != 0) {
			int rc = SQLITE_OK;
			if (lua_type(L, -2) == LUA_TNUMBER) {
				int sqlite_idx = (int)lua_tointeger(L, -2);
				rc = bind_value(L, stmt, sqlite_idx, -1);
			} else if (lua_type(L, -2) == LUA_TSTRING) {
				const char *name = lua_tostring(L, -2);
				int sqlite_idx = sqlite3_bind_parameter_index(stmt, name);
				if (sqlite_idx > 0) {
					rc = bind_value(L, stmt, sqlite_idx, -1);
				}
			}
			if (rc != SQLITE_OK) {
				return rc;
			}
			lua_pop(L, 1);
		}
		return SQLITE_OK;
	}

	// Positional varargs
	for (int i = start_idx; i <= top; i++) {
		int rc = bind_value(L, stmt, i - start_idx + 1, i);
		if (rc != SQLITE_OK) {
			return rc;
		}
	}
	return SQLITE_OK;
}

static int l_sqlite_exec(lua_State *L) {
	SqliteDB *ud = luaL_checkudata(L, 1, SQLITE_METATABLE);
	const char *sql = luaL_checkstring(L, 2);

	if (!ud->db) {
		return luaL_error(L, "database is closed");
	}

	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare_v2(ud->db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		lua_pushnil(L);
		lua_pushstring(L, sqlite3_errmsg(ud->db));
		return 2;
	}

	rc = bind_params(L, stmt, 3);
	if (rc != SQLITE_OK) {
		lua_pushnil(L);
		lua_pushfstring(L, "error binding parameters: %d", rc);
		sqlite3_finalize(stmt);
		return 2;
	}

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
		lua_pushnil(L);
		lua_pushstring(L, sqlite3_errmsg(ud->db));
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}

static int l_sqlite_query(lua_State *L) {
	SqliteDB *ud = luaL_checkudata(L, 1, SQLITE_METATABLE);
	const char *sql = luaL_checkstring(L, 2);

	if (!ud->db) {
		return luaL_error(L, "database is closed");
	}

	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare_v2(ud->db, sql, -1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		lua_pushnil(L);
		lua_pushstring(L, sqlite3_errmsg(ud->db));
		return 2;
	}

	rc = bind_params(L, stmt, 3);
	if (rc != SQLITE_OK) {
		lua_pushnil(L);
		lua_pushfstring(L, "error binding parameters: %d", rc);
		sqlite3_finalize(stmt);
		return 2;
	}

	lua_newtable(L); // Result table
	int row_idx = 1;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		lua_newtable(L); // Row table
		int cols = sqlite3_column_count(stmt);
		for (int i = 0; i < cols; i++) {
			const char *name = sqlite3_column_name(stmt, i);
			int type = sqlite3_column_type(stmt, i);

			switch (type) {
				case SQLITE_INTEGER:
					lua_pushinteger(L, sqlite3_column_int64(stmt, i));
					break;
				case SQLITE_FLOAT:
					lua_pushnumber(L, sqlite3_column_double(stmt, i));
					break;
				case SQLITE_TEXT:
					lua_pushstring(L, (const char *)sqlite3_column_text(stmt, i));
					break;
				case SQLITE_BLOB:
					lua_pushlstring(L, sqlite3_column_blob(stmt, i), sqlite3_column_bytes(stmt, i));
					break;
				case SQLITE_NULL:
				default:
					lua_pushnil(L);
					break;
			}
			lua_setfield(L, -2, name);
		}
		lua_rawseti(L, -2, row_idx++);
	}

	sqlite3_finalize(stmt);
	return 1;
}

static int l_sqlite_open(lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	SqliteDB *ud = lua_newuserdata(L, sizeof(SqliteDB));
	ud->db = NULL;

	luaL_getmetatable(L, SQLITE_METATABLE);
	lua_setmetatable(L, -2);

	int rc = sqlite3_open(filename, &ud->db);
	if (rc != SQLITE_OK) {
		const char *err = sqlite3_errmsg(ud->db);
		lua_pushnil(L);
		lua_pushstring(L, err);
		if (ud->db) {
			sqlite3_close(ud->db);
			ud->db = NULL;
		}
		return 2;
	}

	return 1;
}

static const struct luaL_Reg sqlite_methods[] = {
	{"exec", l_sqlite_exec},
	{"query", l_sqlite_query},
	{"close", l_sqlite_close},
	{"__gc", l_sqlite_close},
	{NULL, NULL}
};

static const struct luaL_Reg sqlite_lib[] = {
	{"open", l_sqlite_open},
	{NULL, NULL}
};

int luaopen_sqlite(lua_State *L) {
	luaL_newmetatable(L, SQLITE_METATABLE);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, sqlite_methods, 0);
	lua_pop(L, 1);

	luaL_newlib(L, sqlite_lib);
	return 1;
}
