#include "luna.h"
#include <curl/curl.h>
#include <ctype.h>

typedef struct {
	char *data;
	size_t size;
} CurlBuffer;

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t real_size = size * nmemb;
	CurlBuffer *mem = (CurlBuffer *)userp;

	char *ptr = realloc(mem->data, mem->size + real_size + 1);
	if (!ptr) {
		return 0;
	}

	mem->data = ptr;
	memcpy(&(mem->data[mem->size]), contents, real_size);
	mem->size += real_size;
	mem->data[mem->size] = 0;

	return real_size;
}

static size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
	lua_State *L = (lua_State *)userdata;
	size_t total = size * nitems;

	// Find separator
	char *sep = memchr(buffer, ':', total);
	if (sep) {
		int key_len = sep - buffer;
		int val_start = key_len + 1;
		// Trim whitespace
		while (val_start < total && isspace(buffer[val_start])) val_start++;
		int val_len = total - val_start;
		while (val_len > 0 && isspace(buffer[val_start + val_len - 1])) val_len--;

		lua_pushlstring(L, buffer, key_len);
		lua_pushlstring(L, buffer + val_start, val_len);
		lua_settable(L, -3);
	}

	return total;
}

static size_t write_file_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
	return fwrite(ptr, size, nmemb, (FILE *)stream);
}

static int perform_request(lua_State *L, const char *method) {
	const char *url = luaL_checkstring(L, 1);
	int options_idx = 2;
	int has_options = !lua_isnoneornil(L, options_idx);

	CURL *curl = curl_easy_init();
	if (!curl) {
		return luaL_error(L, "Failed to initialize curl");
	}

	CurlBuffer chunk = {malloc(1), 0};
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);

	if (strcmp(method, "HEAD") == 0) {
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
	}

	struct curl_slist *headers = NULL;
	if (has_options && lua_type(L, options_idx) == LUA_TTABLE) {
		// Headers
		lua_getfield(L, options_idx, "headers");
		if (lua_type(L, -1) == LUA_TTABLE) {
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				const char *key = lua_tostring(L, -2);
				const char *val = lua_tostring(L, -1);
				char header_str[512];
				snprintf(header_str, sizeof(header_str), "%s: %s", key, val);
				headers = curl_slist_append(headers, header_str);
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		// Body (data)
		lua_getfield(L, options_idx, "data");
		if (lua_type(L, -1) == LUA_TSTRING) {
			size_t len;
			const char *data = lua_tolstring(L, -1, &len);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)len);
		} else {
			lua_pop(L, 1);
			// Body (json)
			lua_getfield(L, options_idx, "json");
			if (lua_type(L, -1) == LUA_TTABLE) {
				cJSON *json = lua_to_cjson(L, -1);
				char *json_str = cJSON_PrintUnformatted(json);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
				// Add Content-Type header if not already set
				headers = curl_slist_append(headers, "Content-Type: application/json");
				cJSON_Delete(json);
				// We need to keep json_str until perform is done, but curl copies POSTFIELDS
				// Actually curl doesn't copy it unless we use CURLOPT_COPYPOSTFIELDS
				curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, json_str);
				free(json_str);
			}
		}
		lua_pop(L, 1);
	}

	if (headers) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	// Response headers table
	lua_newtable(L);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, L);

	CURLcode res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		free(chunk.data);
		if (headers) {
			curl_slist_free_all(headers);
		}
		curl_easy_cleanup(curl);
		return luaL_error(L, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
	}

	long status_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

	// Create result table
	lua_newtable(L);
	lua_pushinteger(L, status_code);
	lua_setfield(L, -2, "status");
	lua_pushlstring(L, chunk.data, chunk.size);
	lua_setfield(L, -2, "body");
	lua_pushvalue(L, -2); // Push the headers table we were filling
	lua_setfield(L, -2, "headers");

	free(chunk.data);
	if (headers) {
		curl_slist_free_all(headers);
	}
	curl_easy_cleanup(curl);

	return 1;
}

static int l_get(lua_State *L) { return perform_request(L, "GET"); }
static int l_post(lua_State *L) { return perform_request(L, "POST"); }
static int l_put(lua_State *L) { return perform_request(L, "PUT"); }
static int l_delete(lua_State *L) { return perform_request(L, "DELETE"); }
static int l_head(lua_State *L) { return perform_request(L, "HEAD"); }
static int l_patch(lua_State *L) { return perform_request(L, "PATCH"); }
static int l_options(lua_State *L) { return perform_request(L, "OPTIONS"); }

static int l_download(lua_State *L) {
	const char *url = luaL_checkstring(L, 1);
	const char *filename = luaL_checkstring(L, 2);
	int options_idx = 3;
	int has_options = !lua_isnoneornil(L, options_idx);

	CURL *curl = curl_easy_init();
	if (!curl) {
		return luaL_error(L, "Failed to initialize curl");
	}

	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		curl_easy_cleanup(curl);
		return luaL_error(L, "Failed to open file for writing: %s", filename);
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	struct curl_slist *headers = NULL;
	if (has_options && lua_type(L, options_idx) == LUA_TTABLE) {
		// Headers
		lua_getfield(L, options_idx, "headers");
		if (lua_type(L, -1) == LUA_TTABLE) {
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				const char *key = lua_tostring(L, -2);
				const char *val = lua_tostring(L, -1);
				char header_str[512];
				snprintf(header_str, sizeof(header_str), "%s: %s", key, val);
				headers = curl_slist_append(headers, header_str);
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
	}

	if (headers) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	// Response headers table
	lua_newtable(L);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, L);

	CURLcode res = curl_easy_perform(curl);
	fclose(fp);

	if (res != CURLE_OK) {
		if (headers) {
			curl_slist_free_all(headers);
		}
		curl_easy_cleanup(curl);
		return luaL_error(L, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
	}

	long status_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

	// Create result table
	lua_newtable(L);
	lua_pushinteger(L, status_code);
	lua_setfield(L, -2, "status");
	lua_pushstring(L, filename);
	lua_setfield(L, -2, "path");
	lua_pushvalue(L, -2); // Push the headers table we were filling
	lua_setfield(L, -2, "headers");

	if (headers) {
		curl_slist_free_all(headers);
	}
	curl_easy_cleanup(curl);

	return 1;
}

static int l_request_generic(lua_State *L) {
	const char *method = luaL_checkstring(L, 1);
	lua_remove(L, 1);
	return perform_request(L, method);
}

static int l_call(lua_State *L) {
	lua_remove(L, 1); // Remove the table itself
	return l_get(L);
}

static const struct luaL_Reg request_lib[] = {
	{"get", l_get},
	{"post", l_post},
	{"put", l_put},
	{"delete", l_delete},
	{"head", l_head},
	{"patch", l_patch},
	{"options", l_options},
	{"download", l_download},
	{"request", l_request_generic},
	{NULL, NULL}
};

int luaopen_request(lua_State *L) {
	curl_global_init(CURL_GLOBAL_DEFAULT);
	luaL_newlib(L, request_lib);

	// Create metatable for the library
	lua_newtable(L);
	lua_pushcfunction(L, l_call);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);

	return 1;
}
