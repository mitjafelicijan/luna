#include "luna.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/coding.h>

#define MAX_ROUTES 100
#define MAX_MIDDLEWARES 100
#define INITIAL_BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE (10 * 1024 * 1024)

typedef struct {
	char *method;
	char *path;
	int callback_ref;
} HTTPRoute;

typedef struct {
	char *path;
	int callback_ref;
} WSRoute;

typedef struct {
	ev_io io;
	lua_State *L;
	char *buffer;
	size_t buffer_len;
	size_t buffer_cap;
	int headers_parsed;
	size_t body_start;
	long content_length;

	int status_code;
	int headers_ref; // Reference to response headers
	int response_sent;

	int is_websocket;
	int ws_callback_ref;
	int ws_on_message_ref;
	int ws_on_close_ref;
} HTTPClient;

// Static storage
static HTTPRoute routes[MAX_ROUTES];
static int route_count = 0;
static WSRoute ws_routes[MAX_ROUTES];
static int ws_route_count = 0;
static int middleware_refs[MAX_MIDDLEWARES];
static int middleware_count = 0;
static char *static_dir = NULL;

static const char *get_mime_type(const char *path) {
	const char *ext = strrchr(path, '.');
	if (!ext) {
		return "application/octet-stream";
	}
	if (strcasecmp(ext, ".html") == 0) {
		return "text/html";
	}
	if (strcasecmp(ext, ".css") == 0) {
		return "text/css";
	}
	if (strcasecmp(ext, ".js") == 0) {
		return "application/javascript";
	}
	if (strcasecmp(ext, ".png") == 0) {
		return "image/png";
	}
	if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
		return "image/jpeg";
	}
	if (strcasecmp(ext, ".gif") == 0) {
		return "image/gif";
	}
	if (strcasecmp(ext, ".svg") == 0) {
		return "image/svg+xml";
	}
	if (strcasecmp(ext, ".json") == 0) {
		return "application/json";
	}
	return "application/octet-stream";
}

static void free_client(HTTPClient *client) {
	if (client->buffer) {
		free(client->buffer);
	}
	if (client->headers_ref != LUA_REFNIL) {
		luaL_unref(client->L, LUA_REGISTRYINDEX, client->headers_ref);
	}
	if (client->ws_on_message_ref != LUA_REFNIL) {
		luaL_unref(client->L, LUA_REGISTRYINDEX, client->ws_on_message_ref);
	}
	if (client->ws_on_close_ref != LUA_REFNIL) {
		luaL_unref(client->L, LUA_REGISTRYINDEX, client->ws_on_close_ref);
	}
	if (client->ws_callback_ref != LUA_REFNIL) {
		luaL_unref(client->L, LUA_REGISTRYINDEX, client->ws_callback_ref);
	}
	free(client);
}

static void send_response(HTTPClient *client, const char *body, size_t body_len, const char *content_type) {
	lua_State *L = client->L;
	luaL_Buffer b;
	luaL_buffinit(L, &b);

	char status_line[128];
	snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", client->status_code, get_status_message(client->status_code));
	luaL_addstring(&b, status_line);

	if (content_type) {
		luaL_addstring(&b, "Content-Type: ");
		luaL_addstring(&b, content_type);
		luaL_addstring(&b, "\r\n");
	}

	char content_len_hdr[64];
	snprintf(content_len_hdr, sizeof(content_len_hdr), "Content-Length: %zu\r\n", body_len);
	luaL_addstring(&b, content_len_hdr);

	// Append user-defined headers
	lua_rawgeti(L, LUA_REGISTRYINDEX, client->headers_ref);
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		luaL_addstring(&b, lua_tostring(L, -2));
		luaL_addstring(&b, ": ");
		luaL_addstring(&b, lua_tostring(L, -1));
		luaL_addstring(&b, "\r\n");
		lua_pop(L, 1);
	}
	lua_pop(L, 1); // Headers table

	luaL_addstring(&b, "\r\n");
	if (body) {
		luaL_addlstring(&b, body, body_len);
	}

	luaL_pushresult(&b);
	size_t resp_len;
	const char *resp = lua_tolstring(L, -1, &resp_len);
	send(client->io.fd, resp, resp_len, 0);
	lua_pop(L, 1);
	client->response_sent = 1;
}

static int get_res_idx(lua_State *L) {
	// Shift index if called as method
	if (lua_gettop(L) > 1 && lua_type(L, 1) == LUA_TTABLE) {
		return 2;
	}
	return 1;
}

static int res_status(lua_State *L) {
	HTTPClient *client = *(HTTPClient **)lua_touserdata(L, lua_upvalueindex(1));
	int idx = get_res_idx(L);
	client->status_code = luaL_checkinteger(L, idx);
	lua_pushvalue(L, lua_upvalueindex(2)); // Return res
	return 1;
}

static int res_header(lua_State *L) {
	HTTPClient *client = *(HTTPClient **)lua_touserdata(L, lua_upvalueindex(1));
	int idx = get_res_idx(L);
	const char *name = luaL_checkstring(L, idx);
	const char *value = luaL_checkstring(L, idx + 1);

	lua_rawgeti(L, LUA_REGISTRYINDEX, client->headers_ref);
	lua_pushstring(L, value);
	lua_setfield(L, -2, name);
	lua_pop(L, 1);

	lua_pushvalue(L, lua_upvalueindex(2)); // Return res
	return 1;
}

static int res_send(lua_State *L) {
	HTTPClient *client = *(HTTPClient **)lua_touserdata(L, lua_upvalueindex(1));
	int idx = get_res_idx(L);
	size_t len;
	const char *body = lua_tolstring(L, idx, &len);
	send_response(client, body, len, "text/html");
	return 0;
}

static int res_json(lua_State *L) {
	HTTPClient *client = *(HTTPClient **)lua_touserdata(L, lua_upvalueindex(1));
	int idx = get_res_idx(L);
	luaL_checktype(L, idx, LUA_TTABLE);

	cJSON *json = lua_to_cjson(L, idx);
	char *json_str = cJSON_PrintUnformatted(json);

	send_response(client, json_str, strlen(json_str), "application/json");

	free(json_str);
	cJSON_Delete(json);
	return 0;
}

static int req_json(lua_State *L) {
	lua_getfield(L, 1, "body");
	size_t len;
	const char *body = lua_tolstring(L, -1, &len);
	if (!body || len == 0) {
		lua_pushnil(L);
		return 1;
	}

	cJSON *json = cJSON_ParseWithLength(body, len);
	if (!json) {
		lua_pushnil(L);
		return 1;
	}

	cjson_to_lua(L, json);
	cJSON_Delete(json);
	return 1;
}

static void generate_ws_accept(const char *key, char *out) {
	char combined[256];
	snprintf(combined, sizeof(combined), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

	unsigned char sha1[WC_SHA_DIGEST_SIZE];
	wc_ShaHash((const byte*)combined, (word32)strlen(combined), sha1);

	word32 out_len = 128;
	Base64_Encode_NoNl(sha1, WC_SHA_DIGEST_SIZE, (byte*)out, &out_len);
	out[out_len] = '\0';
}

static int ws_send(lua_State *L) {
	HTTPClient *client = *(HTTPClient **)luaL_checkudata(L, 1, "Luna.WebSocket");
	size_t len;
	const char *msg = luaL_checklstring(L, 2, &len);

	unsigned char header[10];
	int header_len = 0;
	header[0] = 0x81; // FIN + Text frame

	if (len < 126) {
		header[1] = (unsigned char)len;
		header_len = 2;
	} else if (len < 65536) {
		header[1] = 126;
		header[2] = (len >> 8) & 0xFF;
		header[3] = len & 0xFF;
		header_len = 4;
	} else {
		header[1] = 127;
		for (int i = 0; i < 8; i++) {
			header[2 + i] = (len >> (56 - i * 8)) & 0xFF;
		}
		header_len = 10;
	}

	send(client->io.fd, header, header_len, 0);
	send(client->io.fd, msg, len, 0);

	return 0;
}

static int ws_on(lua_State *L) {
	HTTPClient *client = *(HTTPClient **)luaL_checkudata(L, 1, "Luna.WebSocket");
	const char *event = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);

	if (strcmp(event, "message") == 0) {
		if (client->ws_on_message_ref != LUA_REFNIL) {
			luaL_unref(L, LUA_REGISTRYINDEX, client->ws_on_message_ref);
		}
		lua_pushvalue(L, 3);
		client->ws_on_message_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	} else if (strcmp(event, "close") == 0) {
		if (client->ws_on_close_ref != LUA_REFNIL) {
			luaL_unref(L, LUA_REGISTRYINDEX, client->ws_on_close_ref);
		}
		lua_pushvalue(L, 3);
		client->ws_on_close_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

static const struct luaL_Reg ws_methods[] = {
	{"send", ws_send},
	{"on", ws_on},
	{NULL, NULL}
};

static void setup_ws_meta(lua_State *L) {
	if (luaL_newmetatable(L, "Luna.WebSocket")) {
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		luaL_setfuncs(L, ws_methods, 0);
	}
	lua_pop(L, 1);
}
static void url_decode(char *str) {
	char *src = str, *dst = str;
	while (*src) {
		if (*src == '+') {
			*dst++ = ' ';
			src++;
		} else if (*src == '%' && src[1] && src[2]) {
			char hex[3] = { src[1], src[2], '\0' };
			*dst++ = (char)strtol(hex, NULL, 16);
			src += 3;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';
}

static void parse_query_params(lua_State *L, char *query) {
	lua_newtable(L);
	if (!query || *query == '\0') {
		return;
	}

	char *saveptr;
	char *pair = strtok_r(query, "&", &saveptr);
	while (pair) {
		char *sep = strchr(pair, '=');
		if (sep) {
			*sep = '\0';
			char *key = pair;
			char *val = sep + 1;
			url_decode(key);
			url_decode(val);
			lua_pushstring(L, val);
			lua_setfield(L, -2, key);
		} else {
			url_decode(pair);
			lua_pushboolean(L, 1);
			lua_setfield(L, -2, pair);
		}
		pair = strtok_r(NULL, "&", &saveptr);
	}
}

static int match_path(const char *route_path, const char *req_path, lua_State *L) {
	char *rp = strdup(route_path);
	char *pp = strdup(req_path);
	char *r_save, *p_save;
	char *r_seg = strtok_r(rp, "/", &r_save);
	char *p_seg = strtok_r(pp, "/", &p_save);

	int match = 1;
	lua_newtable(L); // Params table

	while (r_seg || p_seg) {
		if (!r_seg || !p_seg) {
			match = 0;
			break;
		}
		if (r_seg[0] == ':') {
			lua_pushstring(L, p_seg);
			lua_setfield(L, -2, r_seg + 1);
		} else if (strcmp(r_seg, p_seg) != 0) {
			match = 0;
			break;
		}
		r_seg = strtok_r(NULL, "/", &r_save);
		p_seg = strtok_r(NULL, "/", &p_save);
	}

	free(rp);
	free(pp);

	if (!match) {
		lua_pop(L, 1);
		return 0;
	}
	return 1;
}

static int l_next(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1)); // Req
	lua_pushvalue(L, lua_upvalueindex(2)); // Res
	lua_pushvalue(L, lua_upvalueindex(3)); // Handlers
	lua_pushvalue(L, lua_upvalueindex(4)); // State table

	lua_getfield(L, -1, "index");
	int index = lua_tointeger(L, -1) + 1;
	lua_pop(L, 1);

	lua_pushinteger(L, index);
	lua_setfield(L, -2, "index");

	lua_rawgeti(L, -2, index); // Get handler
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return 0;
	}

	lua_pushvalue(L, lua_upvalueindex(1)); // Req
	lua_pushvalue(L, lua_upvalueindex(2)); // Res
	lua_pushvalue(L, lua_upvalueindex(5)); // Next (self)

	if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
		fprintf(stderr, "Error in handler: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	return 0;
}

static void handle_ws_frames(struct ev_loop *loop, HTTPClient *client) {
	unsigned char *data = (unsigned char *)client->buffer;
	while (client->buffer_len >= 2) {
		unsigned char first_byte = data[0];
		unsigned char second_byte = data[1];
		// int fin = (first_byte >> 7) & 0x01; // Currently unused
		int opcode = first_byte & 0x0F;
		int masked = (second_byte >> 7) & 0x01;
		uint64_t payload_len = second_byte & 0x7F;
		size_t header_len = 2;

		if (payload_len == 126) {
			if (client->buffer_len < 4) {
				return;
			}
			payload_len = (data[2] << 8) | data[3];
			header_len = 4;
		} else if (payload_len == 127) {
			if (client->buffer_len < 10) {
				return;
			}
			payload_len = 0;
			for (int i = 0; i < 8; i++) {
				payload_len = (payload_len << 8) | data[2 + i];
			}
			header_len = 10;
		}

		if (masked) {
			if (client->buffer_len < header_len + 4) {
				return;
			}
			header_len += 4;
		}

		if (client->buffer_len < header_len + payload_len) {
			return;
		}

		unsigned char *mask = NULL;
		if (masked) {
			mask = data + header_len - 4;
		}

		unsigned char *payload = data + header_len;
		if (masked) {
			for (size_t i = 0; i < payload_len; i++) {
				payload[i] ^= mask[i % 4];
			}
		}

		if (opcode == 0x01 || opcode == 0x02) { // Text or Binary
			if (client->ws_on_message_ref != LUA_REFNIL) {
				lua_rawgeti(client->L, LUA_REGISTRYINDEX, client->ws_on_message_ref);
				lua_pushlstring(client->L, (const char *)payload, payload_len);
				if (lua_pcall(client->L, 1, 0, 0) != LUA_OK) {
					fprintf(stderr, "WS message callback error: %s\n", lua_tostring(client->L, -1));
					lua_pop(client->L, 1);
				}
			}
		} else if (opcode == 0x08) { // Close
			if (client->ws_on_close_ref != LUA_REFNIL) {
				lua_rawgeti(client->L, LUA_REGISTRYINDEX, client->ws_on_close_ref);
				if (lua_pcall(client->L, 0, 0, 0) != LUA_OK) {
					fprintf(stderr, "WS close callback error: %s\n", lua_tostring(client->L, -1));
					lua_pop(client->L, 1);
				}
			}
			ev_io_stop(loop, &client->io);
			close(client->io.fd);
			free_client(client);
			return;
		} else if (opcode == 0x09) { // Ping
			// Send Pong
			unsigned char pong[2] = {0x8A, 0x00};
			send(client->io.fd, pong, 2, 0);
		}

		// Remove processed frame from buffer
		size_t total_frame_len = header_len + payload_len;
		memmove(client->buffer, client->buffer + total_frame_len, client->buffer_len - total_frame_len);
		client->buffer_len -= total_frame_len;
		data = (unsigned char *)client->buffer;
	}
}

static void read_cb(struct ev_loop *loop, ev_io *w, int revents) {
	HTTPClient *client = (HTTPClient *)w;
	lua_State *L = client->L;

	if (client->buffer_len + 1024 > client->buffer_cap) {
		size_t new_cap = client->buffer_cap * 2;
		if (new_cap > MAX_BUFFER_SIZE) {
			new_cap = MAX_BUFFER_SIZE;
		}
		if (client->buffer_len + 1024 > new_cap) {
			ev_io_stop(loop, w);
			close(w->fd);
			free_client(client);
			return;
		}
		client->buffer = realloc(client->buffer, new_cap);
		client->buffer_cap = new_cap;
	}

	ssize_t nread = recv(w->fd, client->buffer + client->buffer_len, client->buffer_cap - client->buffer_len - 1, 0);
	if (nread <= 0) {
		ev_io_stop(loop, w);
		close(w->fd);
		free_client(client);
		return;
	}

	client->buffer_len += nread;
	client->buffer[client->buffer_len] = '\0';

	if (client->is_websocket) {
		handle_ws_frames(loop, client);
		return;
	}

	if (!client->headers_parsed) {
		// Find separator
		char *end = strstr(client->buffer, "\r\n\r\n");
		if (end) {
			client->headers_parsed = 1;
			client->body_start = (end - client->buffer) + 4;

			// Find content-length
			char *cl = strcasestr(client->buffer, "Content-Length:");
			if (cl) {
				client->content_length = atol(cl + 15);
			} else {
				client->content_length = 0;
			}
		}
	}

	if (client->headers_parsed) {
			// Check if body received
		if (client->buffer_len - client->body_start >= (size_t)client->content_length) {
			char method[16], full_path[1024], protocol[16];
			if (sscanf(client->buffer, "%15s %1023s %15s", method, full_path, protocol) == 3) {
				char *path = full_path;
				char *query_str = strchr(full_path, '?');
				if (query_str) {
					*query_str = '\0';
					query_str++;
				}

				// Check for WebSocket upgrade
				char *upg = strcasestr(client->buffer, "Upgrade: websocket");
				if (upg && strcasecmp(method, "GET") == 0) {
					int found_ws_idx = -1;
					for (int i = 0; i < ws_route_count; i++) {
						if (match_path(ws_routes[i].path, path, L)) {
							found_ws_idx = i;
							break;
						}
					}

					if (found_ws_idx != -1) {
						char *key_hdr = strcasestr(client->buffer, "Sec-WebSocket-Key:");
						if (key_hdr) {
							char key[128];
							sscanf(key_hdr + 18, "%127s", key);
							char accept_val[128];
							generate_ws_accept(key, accept_val);

							char response[512];
							snprintf(response, sizeof(response),
								"HTTP/1.1 101 Switching Protocols\r\n"
								"Upgrade: websocket\r\n"
								"Connection: Upgrade\r\n"
								"Sec-WebSocket-Accept: %s\r\n\r\n",
								accept_val);
							send(w->fd, response, strlen(response), 0);

							client->is_websocket = 1;
							size_t extra = client->buffer_len - client->body_start;
							if (extra > 0) {
								memmove(client->buffer, client->buffer + client->body_start, extra);
							}
							client->buffer_len = extra;

							// Create WS object for Lua
							HTTPClient **ud = lua_newuserdata(L, sizeof(HTTPClient *));
							*ud = client;
							luaL_getmetatable(L, "Luna.WebSocket");
							lua_setmetatable(L, -2);

							lua_rawgeti(L, LUA_REGISTRYINDEX, ws_routes[found_ws_idx].callback_ref);
							lua_pushvalue(L, -2); // WS object

							if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
								fprintf(stderr, "WS connection callback error: %s\n", lua_tostring(L, -1));
								lua_pop(L, 1);
							}

							return;
						}
					}
				}

				lua_settop(L, 0);


				int found_route_idx = -1;
				for (int i = 0; i < route_count; i++) {
					if (strcasecmp(method, routes[i].method) == 0) {
						if (match_path(routes[i].path, path, L)) {
							found_route_idx = i;
							break;
						}
					}
				}

				// Stack: [params] or empty
				if (found_route_idx == -1) {
					lua_newtable(L); // Dummy params
				}
				int params_idx = 1;

				// Create query table
				parse_query_params(L, query_str);
				int query_idx = 2;

				// Create handlers table
				lua_newtable(L);
				int handlers_idx = 3;
				int h_idx = 1;
				for (int i = 0; i < middleware_count; i++) {
					lua_rawgeti(L, LUA_REGISTRYINDEX, middleware_refs[i]);
					lua_rawseti(L, handlers_idx, h_idx++);
				}
				if (found_route_idx != -1) {
					lua_rawgeti(L, LUA_REGISTRYINDEX, routes[found_route_idx].callback_ref);
					lua_rawseti(L, handlers_idx, h_idx++);
				}

				// Create req table
				lua_newtable(L);
				int req_idx = 4;
				lua_pushstring(L, method);
				lua_setfield(L, req_idx, "method");
				lua_pushstring(L, path);
				lua_setfield(L, req_idx, "path");
				lua_pushlstring(L, client->buffer + client->body_start, client->content_length);
				lua_setfield(L, req_idx, "body");
				lua_pushvalue(L, params_idx);
				lua_setfield(L, req_idx, "params");
				lua_pushvalue(L, query_idx);
				lua_setfield(L, req_idx, "query");
				lua_pushcfunction(L, req_json);
				lua_setfield(L, req_idx, "json");

				// Create res table
				lua_newtable(L);
				int res_idx = 5;
				HTTPClient **ud = lua_newuserdata(L, sizeof(HTTPClient *));
				*ud = client;

				lua_pushvalue(L, -1);
				lua_pushvalue(L, res_idx);
				lua_pushcclosure(L, res_status, 2);
				lua_setfield(L, res_idx, "status");

				lua_pushvalue(L, -1);
				lua_pushvalue(L, res_idx);
				lua_pushcclosure(L, res_header, 2);
				lua_setfield(L, res_idx, "header");

				lua_pushvalue(L, -1);
				lua_pushvalue(L, res_idx);
				lua_pushcclosure(L, res_send, 2);
				lua_setfield(L, res_idx, "send");

				lua_pushvalue(L, -1);
				lua_pushvalue(L, res_idx);
				lua_pushcclosure(L, res_json, 2);
				lua_setfield(L, res_idx, "json");
				lua_pop(L, 1); // Userdata

				// State table
				lua_newtable(L);
				int state_idx = 6;
				lua_pushinteger(L, 0);
				lua_setfield(L, state_idx, "index");

				// Push next closure with 5 upvalues
				lua_pushvalue(L, req_idx);
				lua_pushvalue(L, res_idx);
				lua_pushvalue(L, handlers_idx);
				lua_pushvalue(L, state_idx);
				lua_pushnil(L); // Placeholder for next (self)
				lua_pushcclosure(L, l_next, 5);

				lua_pushvalue(L, -1);
				lua_setupvalue(L, -2, 5); // Self-reference

				// Call next()
				if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
					fprintf(stderr, "Error starting middleware chain: %s\n", lua_tostring(L, -1));
				}

				if (!client->response_sent) {
					int found = 0;
					if (static_dir && strcasecmp(method, "GET") == 0) {
						char full_path_buf[1024];
						if (!strstr(path, "..")) {
							snprintf(full_path_buf, sizeof(full_path_buf), "%s%s", static_dir, path);
							struct stat st;
							if (stat(full_path_buf, &st) == 0 && S_ISDIR(st.st_mode)) {
								strncat(full_path_buf, "/index.html", sizeof(full_path_buf) - strlen(full_path_buf) - 1);
							}
							FILE *fp = fopen(full_path_buf, "rb");
							if (fp) {
								fseek(fp, 0, SEEK_END);
								size_t fsize = ftell(fp);
								fseek(fp, 0, SEEK_SET);
								char *fcontent = malloc(fsize + 1);
								if (fcontent) {
									if (fread(fcontent, 1, fsize, fp) == fsize) {
										const char *mime = get_mime_type(full_path_buf);
										client->status_code = 200;
										send_response(client, fcontent, fsize, mime);
										found = 1;
									}
									free(fcontent);
								}
								fclose(fp);
							}
						}
					}
					if (!found) {
						const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
						send(w->fd, not_found, strlen(not_found), 0);
					}
				}
				lua_settop(L, 0);
			}

			ev_io_stop(loop, w);
			close(w->fd);
			free_client(client);
		}
	}
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(w->fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_fd >= 0) {
		set_nonblocking(client_fd);
		HTTPClient *client = calloc(1, sizeof(HTTPClient));
		client->L = (lua_State *)w->data;
		client->buffer = malloc(INITIAL_BUFFER_SIZE);
		client->buffer_cap = INITIAL_BUFFER_SIZE;
		client->status_code = 200;

		lua_newtable(client->L);
		client->headers_ref = luaL_ref(client->L, LUA_REGISTRYINDEX);
		client->ws_callback_ref = LUA_REFNIL;
		client->ws_on_message_ref = LUA_REFNIL;
		client->ws_on_close_ref = LUA_REFNIL;

		ev_io_init(&client->io, read_cb, client_fd, EV_READ);
		ev_io_start(loop, &client->io);
	}
}

static int l_http_ws(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (ws_route_count < MAX_ROUTES) {
		ws_routes[ws_route_count].path = strdup(path);
		lua_pushvalue(L, 2);
		ws_routes[ws_route_count].callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
		ws_route_count++;
	} else {
		return luaL_error(L, "Too many WebSocket routes");
	}
	return 0;
}

static int l_http_route(lua_State *L) {
	const char *method = luaL_checkstring(L, 1);
	const char *path = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);

	if (route_count < MAX_ROUTES) {
		routes[route_count].method = strdup(method);
		routes[route_count].path = strdup(path);
		lua_pushvalue(L, 3);
		routes[route_count].callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
		route_count++;
	} else {
		return luaL_error(L, "Too many routes");
	}
	return 0;
}

static int l_http_use(lua_State *L) {
	luaL_checktype(L, 1, LUA_TFUNCTION);
	if (middleware_count < MAX_MIDDLEWARES) {
		lua_pushvalue(L, 1);
		middleware_refs[middleware_count++] = luaL_ref(L, LUA_REGISTRYINDEX);
	} else {
		return luaL_error(L, "Too many middlewares");
	}
	return 0;
}

static int l_http_static(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	if (static_dir) {
		free(static_dir);
	}
	static_dir = strdup(path);
	return 0;
}

static int l_http_listen(lua_State *L) {
	int port = luaL_checkinteger(L, 1);
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		return luaL_error(L, "Socket creation failed");
	}

	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		return luaL_error(L, "Bind failed");
	}

	if (listen(server_fd, 10) < 0) {
		return luaL_error(L, "Listen failed");
	}

	set_nonblocking(server_fd);

	static ev_io server_watcher;
	server_watcher.data = L;
	ev_io_init(&server_watcher, accept_cb, server_fd, EV_READ);
	ev_io_start(EV_DEFAULT, &server_watcher);

	return 0;
}

static int l_http_run(lua_State *L) {
	ev_run(EV_DEFAULT, 0);
	return 0;
}

static const struct luaL_Reg http_lib[] = {
	{"route", l_http_route},
	{"ws", l_http_ws},
	{"use", l_http_use},
	{"static", l_http_static},
	{"listen", l_http_listen},
	{"run", l_http_run},
	{NULL, NULL}
};

int luaopen_http(lua_State *L) {
	setup_ws_meta(L);
	luaL_newlib(L, http_lib);
	return 1;
}
