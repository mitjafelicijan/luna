#include "luna.h"
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

extern char **environ;

static char **lua_to_argv(lua_State *L, const char *command, int table_idx) {
	int n = 0;
	if (lua_istable(L, table_idx)) {
		n = lua_objlen(L, table_idx);
	}

	char **argv = malloc((n + 2) * sizeof(char *));
	argv[0] = strdup(command);
	for (int i = 1; i <= n; i++) {
		lua_rawgeti(L, table_idx, i);
		argv[i] = strdup(luaL_checkstring(L, -1));
		lua_pop(L, 1);
	}
	argv[n + 1] = NULL;
	return argv;
}

static void free_argv(char **argv) {
	for (int i = 0; argv[i]; i++) {
		free(argv[i]);
	}
	free(argv);
}

static int l_process_run(lua_State *L) {
	const char *command = luaL_checkstring(L, 1);
	char **argv = lua_to_argv(L, command, 2);

	int stdout_pipe[2];
	int stderr_pipe[2];

	if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
		free_argv(argv);
		lua_pushnil(L);
		lua_pushstring(L, "failed to create pipes");
		return 2;
	}

	pid_t pid = fork();
	if (pid == -1) {
		free_argv(argv);
		lua_pushnil(L);
		lua_pushstring(L, "failed to fork");
		return 2;
	}

	if (pid == 0) { // Child
		dup2(stdout_pipe[1], STDOUT_FILENO);
		dup2(stderr_pipe[1], STDERR_FILENO);
		// Close all pipe fds in child
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		close(stderr_pipe[0]);
		close(stderr_pipe[1]);

		execvp(command, argv);
		exit(127);
	}

	// Parent
	free_argv(argv);
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);

	char buffer[4096];
	size_t out_cap = 4096, out_len = 0;
	size_t err_cap = 4096, err_len = 0;
	char *out_buf = malloc(out_cap);
	char *err_buf = malloc(err_cap);

	int stdout_eof = 0, stderr_eof = 0;
	while (!stdout_eof || !stderr_eof) {
		// Read until both pipes close
		if (!stdout_eof) {
			ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
			if (n > 0) {
				if (out_len + n > out_cap) {
					out_cap *= 2;
					out_buf = realloc(out_buf, out_cap);
				}
				memcpy(out_buf + out_len, buffer, n);
				out_len += n;
			} else if (n == 0 || (n == -1 && errno != EINTR)) {
				stdout_eof = 1;
			}
		}
		if (!stderr_eof) {
			ssize_t n = read(stderr_pipe[0], buffer, sizeof(buffer));
			if (n > 0) {
				if (err_len + n > err_cap) {
					err_cap *= 2;
					err_buf = realloc(err_buf, err_cap);
				}
				memcpy(err_buf + err_len, buffer, n);
				err_len += n;
			} else if (n == 0 || (n == -1 && errno != EINTR)) {
				stderr_eof = 1;
			}
		}
	}

	close(stdout_pipe[0]);
	close(stderr_pipe[0]);

	int status;
	waitpid(pid, &status, 0);

	lua_newtable(L);
	lua_pushlstring(L, out_buf, out_len);
	lua_setfield(L, -2, "stdout");
	lua_pushlstring(L, err_buf, err_len);
	lua_setfield(L, -2, "stderr");
	lua_pushinteger(L, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
	lua_setfield(L, -2, "exit_code");

	free(out_buf);
	free(err_buf);

	return 1;
}

static int l_process_spawn(lua_State *L) {
	const char *command = luaL_checkstring(L, 1);
	char **argv = lua_to_argv(L, command, 2);

	pid_t pid = fork();
	if (pid == -1) {
		free_argv(argv);
		lua_pushnil(L);
		lua_pushstring(L, "failed to fork");
		return 2;
	}

	if (pid == 0) { // Child
		execvp(command, argv);
		exit(127);
	}

	free_argv(argv);
	lua_pushinteger(L, pid);
	return 1;
}

static int l_process_env(lua_State *L) {
	lua_newtable(L);
	for (char **env = environ; *env; env++) {
		char *s = *env;
		char *sep = strchr(s, '=');
		if (sep) {
			lua_pushlstring(L, s, sep - s);
			lua_pushstring(L, sep + 1);
			lua_settable(L, -3);
		}
	}
	return 1;
}

static const struct luaL_Reg process_lib[] = {
	{"run", l_process_run},
	{"spawn", l_process_spawn},
	{"env", l_process_env},
	{NULL, NULL}
};

int luaopen_process(lua_State *L) {
	luaL_newlib(L, process_lib);
	return 1;
}
