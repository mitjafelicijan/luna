# Luna

Luna is a Lua runtime environment that integrates the LuaJIT JIT compiler with a libev-based event loop. It is designed for building asynchronous system utilities and web services using a built-in standard library.

## Architecture

- **Execution Engine**: Powered by **LuaJIT** for Lua code execution.
- **Event Loop**: Uses **libev** to manage asynchronous I/O operations, timers, and signal handling, enabling non-blocking application patterns.
- **Static Compilation**: Built using the **Zig** toolchain (`zig cc`) targeting `x86_64-linux-musl`. This results in a single, zero-dependency static binary with no external shared library requirements.
- **Standard Library**: A collection of C-based modules registered globally, providing low-level access to system resources and common protocols.

## Standard Library

Luna includes the following built-in modules:

- `core`: Core runtime utilities, asynchronous timers, and system metadata.
- `path`: Cross-platform path manipulation (join, basename, etc).
- `http`: Event-driven web server with routing, middleware, static file serving, and WebSocket support.
- `request`: Synchronous HTTP client powered by `libcurl` and `wolfSSL`.
- `sqlite`: Integrated SQLite3 database engine for persistence.
- `crypto`: Cryptographic primitives including hashing (MD5, SHA), HMAC, and secure random number generation via `wolfSSL`.
- `fs`: Synchronous and asynchronous file system operations.
- `process`: System process management and execution.
- `json`: JSON serialization and deserialization via `cJSON`.
- `env`: Environment variable management and `.env` file loading.
- `ini`: INI configuration parsing and generation.
- `template`: Jinja2-style string templating.
- `log`: Structured, level-based logging with ANSI color support.
- `stash`: In-memory key-value store with TTL support.
- `assert`: Unit testing and validation utilities.

Detailed documentation for each module is available in [DOCS.md](DOCS.md).

## Building from Source

### Prerequisites

To compile Luna and its dependencies, the following tools are required:

- **Zig** (for the compiler toolchain)
- **GNU Make** & **CMake**
- **Flex** & **Bison** (required for the template engine parser)
- **GCC** (as a host compiler for building LuaJIT's build tools)

### Build Process

1. **Compile Dependencies**: This step builds all required libraries (LuaJIT, libev, wolfSSL, curl, etc.) as static archives.
   ```bash
   make requirements
   ```

2. **Compile Luna**: Build the main runtime binary.
   ```bash
   make
   ```

The resulting `luna` binary is located in the root directory.

## Quick Example

```lua
-- An asynchronous timer and logging example
log.use_colors(true)

log.info("Starting Luna runtime...")

core.set_timeout(1000, function()
    log.info("One second has elapsed.")
end)

-- Starting a simple web server
http.route("GET", "/", function(req, res)
    res:json({ status = "ok", runtime = "luna" })
end)

http.listen(8080)
http.run()
```

## Libraries

Luna is made possible by these open-source libraries:

- https://github.com/enki/libev
- https://github.com/LuaJIT/LuaJIT
- https://github.com/DaveGamble/cJSON
- https://github.com/miko53/jinjac
- https://github.com/wolfSSL/wolfssl
- https://github.com/ndevilla/iniparser
- https://sqlite.org/
