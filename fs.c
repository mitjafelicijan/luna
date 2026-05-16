#include "luna.h"
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

static int l_fs_exists(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	lua_pushboolean(L, access(path, F_OK) == 0);
	return 1;
}

static int l_fs_isdir(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	struct stat st;
	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

static int l_fs_isfile(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	struct stat st;
	if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

static int recursive_mkdir(const char *path, mode_t mode) {
	char tmp[PATH_MAX];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if (tmp[len - 1] == '/') {
		tmp[len - 1] = 0;
	}
	// Create parent directories one by one
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
				return -1;
			}
			*p = '/';
		}
	}
	if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
		return -1;
	}
	return 0;
}

static int l_fs_mkdir(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	int recursive = 0;
	if (lua_gettop(L) >= 2) {
		recursive = lua_toboolean(L, 2);
	}

	int res;
	if (recursive) {
		res = recursive_mkdir(path, 0777);
	} else {
		res = mkdir(path, 0777);
	}

	if (res == 0) {
		lua_pushboolean(L, 1);
		return 1;
	} else {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return 2;
	}
}

static int l_fs_readdir(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	DIR *dir = opendir(path);
	if (!dir) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return 2;
	}

	lua_newtable(L);
	struct dirent *entry;
	int i = 1;
	while ((entry = readdir(dir)) != NULL) {
		// Ignore dot files
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {

			continue;
		}
		lua_pushstring(L, entry->d_name);
		lua_rawseti(L, -2, i++);
	}

	closedir(dir);
	return 1;
}

static int l_fs_stat(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	struct stat st;
	if (stat(path, &st) != 0) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return 2;
	}

	lua_newtable(L);
	lua_pushinteger(L, st.st_size);
	lua_setfield(L, -2, "size");

	const char *type = "unknown";
	if (S_ISREG(st.st_mode)) {
		type = "file";
	} else if (S_ISDIR(st.st_mode)) {
		type = "directory";
	} else if (S_ISLNK(st.st_mode)) {
		type = "link";
	}

	lua_pushstring(L, type);
	lua_setfield(L, -2, "type");
	lua_pushinteger(L, st.st_mtime);
	lua_setfield(L, -2, "mtime");

	return 1;
}

static int l_fs_remove(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	if (remove(path) == 0) {
		lua_pushboolean(L, 1);
		return 1;
	} else {
		lua_pushboolean(L, 0);
		lua_pushstring(L, strerror(errno));
		return 2;
	}
}

static int l_fs_readfile(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return 2;
	}

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (size < 0) {
		fclose(fp);
		lua_pushnil(L);
		lua_pushstring(L, "invalid file size");
		return 2;
	}

	char *buf = malloc(size);
	if (!buf && size > 0) {
		fclose(fp);
		lua_pushnil(L);
		lua_pushstring(L, "out of memory");
		return 2;
	}

	if (size > 0 && fread(buf, 1, size, fp) != (size_t)size) {
		free(buf);
		fclose(fp);
		lua_pushnil(L);
		lua_pushstring(L, "read error");
		return 2;
	}

	lua_pushlstring(L, buf, size);
	free(buf);
	fclose(fp);
	return 1;
}

static int l_fs_writefile(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	size_t len;
	const char *content = luaL_checklstring(L, 2, &len);

	FILE *fp = fopen(path, "wb");
	if (!fp) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, strerror(errno));
		return 2;
	}

	if (len > 0 && fwrite(content, 1, len, fp) != len) {
		fclose(fp);
		lua_pushboolean(L, 0);
		lua_pushstring(L, "write error");
		return 2;
	}

	fclose(fp);
	lua_pushboolean(L, 1);
	return 1;
}

static int l_fs_cwd(lua_State *L) {
	char buf[PATH_MAX];
	if (getcwd(buf, sizeof(buf))) {
		lua_pushstring(L, buf);
		return 1;
	}
	lua_pushnil(L);
	lua_pushstring(L, strerror(errno));
	return 2;
}

static int l_fs_chdir(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	if (chdir(path) == 0) {
		lua_pushboolean(L, 1);
		return 1;
	}
	lua_pushboolean(L, 0);
	lua_pushstring(L, strerror(errno));
	return 2;
}

static int l_fs_rename(lua_State *L) {
	const char *old = luaL_checkstring(L, 1);
	const char *new = luaL_checkstring(L, 2);
	if (rename(old, new) == 0) {
		lua_pushboolean(L, 1);
		return 1;
	}
	lua_pushboolean(L, 0);
	lua_pushstring(L, strerror(errno));
	return 2;
}

static int l_fs_copy(lua_State *L) {
	const char *src = luaL_checkstring(L, 1);
	const char *dest = luaL_checkstring(L, 2);

	FILE *fsrc = fopen(src, "rb");
	if (!fsrc) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, strerror(errno));
		return 2;
	}

	FILE *fdest = fopen(dest, "wb");
	if (!fdest) {
		fclose(fsrc);
		lua_pushboolean(L, 0);
		lua_pushstring(L, strerror(errno));
		return 2;
	}

	char buf[8192];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
		if (fwrite(buf, 1, n, fdest) != n) {
			fclose(fsrc);
			fclose(fdest);
			lua_pushboolean(L, 0);
			lua_pushstring(L, "write error during copy");
			return 2;
		}
	}

	fclose(fsrc);
	fclose(fdest);
	lua_pushboolean(L, 1);
	return 1;
}

static const struct luaL_Reg fs_lib[] = {
	{"exists", l_fs_exists},
	{"isdir", l_fs_isdir},
	{"isfile", l_fs_isfile},
	{"mkdir", l_fs_mkdir},
	{"readdir", l_fs_readdir},
	{"ls", l_fs_readdir},
	{"stat", l_fs_stat},
	{"remove", l_fs_remove},
	{"readfile", l_fs_readfile},
	{"writefile", l_fs_writefile},
	{"cwd", l_fs_cwd},
	{"chdir", l_fs_chdir},
	{"rename", l_fs_rename},
	{"copy", l_fs_copy},
	{NULL, NULL}
};

int luaopen_fs(lua_State *L) {
	luaL_newlib(L, fs_lib);
	return 1;
}
