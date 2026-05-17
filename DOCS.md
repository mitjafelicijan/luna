# Luna Documentation

Luna is a lightweight Lua runtime powered by LuaJIT and libev. It includes a custom standard library designed for building modern applications, including web servers, database-driven tools, and system utilities.

## Building

Luna is built using the **Zig** compiler to ensure consistent static compilation with `musl`.

### Prerequisites

Ensure you have the following tools installed:
- **Zig** (latest stable version)
- **GNU Make**
- **CMake**
- **Flex** and **Bison**
- **Standard Build Tools** (`gcc`, `bash`, `coreutils`)

### Build Steps

1. **Build Dependencies**:
   Luna relies on several libraries (LuaJIT, libev, wolfSSL, etc.) that must be compiled first.
   ```bash
   make requirements
   ```
   *Note: This step may take several minutes as it compiles all sub-dependencies statically.*

2. **Compile Luna**:
   Once requirements are met, compile the main binary:
   ```bash
   make
   ```

3. **Verify Build**:
   The resulting `luna` binary is a standalone static executable.
   ```bash
   ./luna examples/log.lua
   ```

## Getting Started

To run a Lua script with Luna:

```bash
./luna your_script.lua
```

### Hello World Example
```lua
-- hello.lua
log.use_colors(true)
log.info("Hello, Luna!")

local name = env.get("USER") or "World"
print("Welcome, " .. name)
```

## Standard Library

### Table of Contents

- [core](#core)
- [path](#path)
- [fs](#fs-file-system)
- [http](#http-web-server)
- [request](#request-http-client)
- [crypto](#crypto)
- [sqlite](#sqlite)
- [process](#process)
- [json](#json)
- [env](#env)
- [ini](#ini)
- [template](#template)
- [log](#log)
- [stash](#stash)
- [assert](#assert)

---

## core
Core runtime utilities, primarily for asynchronous operations and system metadata.

- `core.set_timeout(ms, callback)`: Executes the callback after `ms` milliseconds.
- `core.version`: The version string of the Luna runtime.
- `core.platform`: The target platform (e.g., `"linux"`, `"darwin"`).
- `core.arch`: The target architecture (e.g., `"x64"`, `"arm64"`).

```lua
print("Luna v" .. core.version .. " on " .. core.platform .. " " .. core.arch)

core.set_timeout(1000, function()
    print("One second passed")
end)
```

---

## path
Cross-platform path manipulation.

- `path.join(...)`: Joins multiple path segments using the platform separator.
- `path.basename(path)`: Returns the last portion of a path.
- `path.dirname(path)`: Returns the directory name of a path.
- `path.extname(path)`: Returns the extension of the path (including the dot).
- `path.resolve(path)`: Resolves a relative path to an absolute path.

```lua
local p = path.join("src", "components", "Button.lua")
print(path.dirname(p))  --> src/components
print(path.basename(p)) --> Button.lua
print(path.extname(p))  --> .lua

local full_path = path.resolve(".")
print("Absolute CWD: " .. full_path)
```

---

## fs (File System)
Comprehensive file and directory management.

- `fs.exists(path)`: Returns true if the path exists.
- `fs.isfile(path)`: Returns true if the path is a regular file.
- `fs.isdir(path)`: Returns true if the path is a directory.
- `fs.mkdir(path, [recursive])`: Creates a directory.
- `fs.ls(path)` / `fs.readdir(path)`: Returns a table of file names in the directory.
- `fs.readfile(path)`: Reads entire file into a string.
- `fs.writefile(path, content)`: Writes string to a file.
- `fs.remove(path)`: Deletes a file or directory.
- `fs.stat(path)`: Returns a table with `size`, `type`, and `mtime`.
- `fs.cwd()`: Returns the current working directory.
- `fs.chdir(path)`: Changes the current working directory.
- `fs.rename(old, new)`: Renames or moves a file or directory.
- `fs.copy(src, dest)`: Copies a file from `src` to `dest`.

### Example: File Operations
```lua
local filename = "data.json"

if not fs.exists(filename) then
    local initial_data = json.encode({ version = 1, users = {} })
    fs.writefile(filename, initial_data)
end

local content = fs.readfile(filename)
local data = json.decode(content)
log.info("Loaded data, version: " .. data.version)

-- List files in current directory
local files = fs.ls(".")
for _, file in ipairs(files) do
    local info = fs.stat(file)
    print(string.format("%-20s %d bytes", file, info.size))
end
```

---

## http (Web Server)
An event-driven web server with routing and middleware support.

- `http.listen(port)`: Starts listening on the specified port.
- `http.route(method, path, handler)`: Defines a route. Path supports parameters (e.g., `:id`).
- `http.use(middleware)`: Registers a global middleware function.
- `http.static(directory)`: Serves static files from the given directory.
- `http.ws(path, callback)`: Defines a WebSocket route. The callback receives a `ws` object.
- `http.run()`: Starts the event loop.

### Request Object (`req`)
- `req.method`: The HTTP method (e.g., `"GET"`, `"POST"`).
- `req.path`: The requested path.
- `req.query`: A table containing URL query parameters.
- `req.params`: A table containing route parameters (e.g., `:id`).
- `req.body`: The raw request body as a string.
- `req:json()`: Parses the request body as JSON and returns a Lua table (or `nil` if invalid).

### Response Object (`res`)
- `res:status(code)`: Sets the HTTP status code (e.g., `200`, `404`). Returns `res` for chaining.
- `res:header(name, value)`: Sets a response header. Returns `res` for chaining.
- `res:send(body)`: Sends the response body with `text/html` content type.
- `res:json(table)`: Serializes the table to JSON and sends it with `application/json` content type.
### WebSocket Object (`ws`)
- `ws:send(message)`: Sends a text message to the client.
- `ws:on(event, callback)`: Registers an event listener. Supported events: `"message"`, `"close"`.

### Example: WebSocket Echo Server
```lua
http.ws("/ws", function(ws)
    log.info("New WebSocket connection")

    ws:on("message", function(msg)
        log.info("Received:", msg)
        ws:send("Echo: " .. msg)
    end)

    ws:on("close", function()
        log.info("WebSocket closed")
    end)
end)

http.listen(8080)
http.run()
```

### Example: REST API

```lua
-- Global logger middleware
http.use(function(req, res, next)
    log.info(req.method, req.path)
    next() -- Continue to next handler
end)

-- Route with parameters
http.route("GET", "/user/:id", function(req, res)
    local userId = req.params.id
    res:status(200):json({
        id = userId,
        name = "Luna User",
        query = req.query -- Access ?key=value
    })
end)

-- Handling POST JSON
http.route("POST", "/echo", function(req, res)
    local data = req:json()
    if not data then
        return res:status(400):json({ error = "Invalid JSON" })
    end
    res:json({ received = data })
end)

-- Serve static files from 'public' directory
http.static("public")

http.listen(8080)
log.info("Server started on :8080")
http.run()
```

---

## request (HTTP Client)
A synchronous HTTP client powered by libcurl.

- `request.get(url, [options])`
- `request.post(url, [options])`
- `request.put(url, [options])`
- `request.delete(url, [options])`
- `request.download(url, filename)`

Options can include `headers` (table), `data` (string), or `json` (table).
Returns a table with `status`, `body`, and `headers`.

### Example: API Consumption
```lua
-- Simple GET
local res = request.get("https://jsonplaceholder.typicode.com/posts/1")
if res.status == 200 then
    local post = json.decode(res.body)
    print("Title: " .. post.title)
end

-- POST with JSON
local create_res = request.post("https://jsonplaceholder.typicode.com/posts", {
    json = {
        title = "Hello Luna",
        body = "This is a post from Luna runtime",
        userId = 1
    },
    headers = {
        ["Content-Type"] = "application/json; charset=UTF-8"
    }
})
log.info("Created post status: " .. create_res.status)

-- Download a file
request.download("https://www.lua.org/images/lua-logo.gif", "lua-logo.gif")
```

---

## process
System process management.

- `process.run(command, [args_table])`: Runs a command and returns `{stdout, stderr, exit_code}`.
- `process.spawn(command, [args_table])`: Spawns a background process and returns its PID.
- `process.env()`: Returns a table of all environment variables.

### Example: Running Commands
```lua
-- Execute 'git status'
local res = process.run("git", {"status", "--short"})
if res.exit_code == 0 then
    print("Git changes:")
    print(res.stdout)
else
    print("Not a git repository or error occurred")
end

-- Get environment variable
local home = process.env().HOME
print("Home directory: " .. home)
```


---

## crypto
Cryptographic primitives powered by wolfSSL.

- `crypto.md5(data)`: Returns binary MD5 hash.
- `crypto.sha256(data)`: Returns binary SHA-256 hash.
- `crypto.hmac(algo, key, data)`: Computes HMAC (algo: "md5", "sha1", "sha256", "sha512").
- `crypto.random(n)`: Returns `n` secure random bytes.
- `crypto.base64_encode(data)` / `crypto.base64_decode(data)`
- `crypto.hex_encode(data)` / `crypto.hex_decode(data)`

### Example: MD5 Hashing
```lua
local data = "luna runtime"
local hash = crypto.md5(data)
local hex_hash = crypto.hex_encode(hash)
print("MD5: " .. hex_hash)
```

### Example: Secure Hashing
```lua
local password = "my_secret_password"
local salt = crypto.random(16)

-- Create a salted hash
local hash = crypto.hmac("sha256", salt, password)
local hex_hash = crypto.hex_encode(hash)
local hex_salt = crypto.hex_encode(salt)

print("Salt: " .. hex_salt)
print("Hash: " .. hex_hash)
```

---

## sqlite
Simple SQLite3 database integration.

- `sqlite.open(filename)`: Returns a database connection object.
- `db:exec(sql, [...params])`: Executes a statement. Supports positional `?` or named `:name` parameters.
- `db:query(sql, [...params])`: Returns a table of rows. Each row is a table.
- `db:close()`: Closes the connection.

### Example: CRUD Operations
```lua
local db, err = sqlite.open("app.db")
if not db then error(err) end

-- Execute with positional parameters
db:exec([[
    CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY,
        name TEXT,
        qty INTEGER
    )
]])

-- Insert using positional params
db:exec("INSERT INTO items (name, qty) VALUES (?, ?)", "Apple", 10)

-- Insert using named params
db:exec("INSERT INTO items (name, qty) VALUES (:name, :qty)", {
    [":name"] = "Orange",
    [":qty"] = 5
})

-- Query data
local items = db:query("SELECT * FROM items WHERE qty > ?", 0)
for _, item in ipairs(items) do
    print(item.name .. ": " .. item.qty)
end

db:close()
```

---

## json
Fast JSON serialization using cJSON.

- `json.encode(table)`: Converts a Lua table to a JSON string.
- `json.decode(string)`: Converts a JSON string to a Lua table.

### Example: Serialization
```lua
local user = {
    id = 1,
    username = "luna_dev",
    roles = {"admin", "developer"},
    profile = {
        bio = "Lua enthusiast",
        verified = true
    }
}

-- Encode to string
local json_str = json.encode(user)
print(json_str)

-- Decode back to table
local decoded = json.decode(json_str)
print(decoded.username .. " is " .. decoded.roles[1])
```

---

## env
Environment variable management.

- `env.load([filename])`: Loads a `.env` file (defaults to `.env`).
- `env.get(name)`: Retrieves an environment variable.

### Example: Configuration via .env
```lua
-- Assuming a .env file exists with:
-- API_KEY=secret_123
-- DEBUG=true

env.load(".env")
local api_key = env.get("API_KEY")
local is_debug = env.get("DEBUG") == "true"

if is_debug then
    log.info("Debug mode enabled with key: " .. api_key)
end
```

---

## ini
INI configuration file handling.

- `ini.load(filename)`: Returns a nested table of sections and keys.
- `ini.save(filename, table)`: Saves a nested table to an INI file.

### Example: Settings management
```lua
local settings = {
    server = {
        host = "localhost",
        port = "8080"
    },
    database = {
        path = "data/app.db",
        timeout = "30"
    }
}

-- Save to file
ini.save("settings.ini", settings)

-- Load from file
local loaded = ini.load("settings.ini")
print("Connecting to " .. loaded.server.host .. ":" .. loaded.server.port)
```

---

## template
Jinja2-style string templating.

- `template.render(template_string, data_table)`: Renders the template with the provided data.

### Example: HTML Rendering
```lua
local html_tpl = [[
<html>
    <body>
        <h1>User List</h1>
        <ul>
            {% for user in users %}
            <li>{{ user.name }} ({{ user.role }})</li>
            {% endfor %}
        </ul>
    </body>
</html>
]]

local data = {
    users = {
        { name = "Alice", role = "Admin" },
        { name = "Bob", role = "User" },
        { name = "Charlie", role = "Guest" }
    }
}

local output = template.render(html_tpl, data)
print(output)
```

---

## log
Level-based logging with optional colors.

- `log.info(...)`
- `log.warning(...)`
- `log.error(...)`
- `log.use_colors(boolean)`: Enables or disables ANSI color output.

### Example
```lua
log.use_colors(true)
log.info("Server started", "v1.0.0")
log.warning("High memory usage detected")
log.error("Failed to connect to database", "timeout")
```

---

## stash
In-memory key-value store with TTL support, powered by SQLite.

- `stash.cleanup(interval)`: Sets how often (in seconds) expired keys are purged (default 60s).
- `stash.set(key, value, [ttl])`: Stores a value. `ttl` is in seconds. If `0` or omitted, the key never expires.
- `stash.get(key)`: Retrieves a value. Returns `nil` if the key is missing or expired.
- `stash.del(key)`: Deletes a key.
- `stash.exists(key)`: Returns `true` if the key exists and hasn't expired.
- `stash.incr(key, [amount])`: Increments a numeric value. Defaults to 1.
- `stash.decr(key, [amount])`: Decrements a numeric value. Defaults to 1.
- `stash.keys([pattern])`: Returns an array of keys matching the SQL LIKE pattern (default `"%"`).
- `stash.ttl(key)`: Returns remaining seconds until expiry. `-1` means no expiry, `-2` means key not found.
- `stash.expire(key, ttl)`: Updates the TTL of an existing key. Returns `true` if successful.
- `stash.clear()`: Removes all keys.
- `stash.count()`: Returns the number of non-expired keys.

### Example: Rate Limiting
```lua
local key = "limit:" .. "127.0.0.1" -- Example IP
local count = stash.incr(key, 1)

if count == 1 then
    stash.expire(key, 60) -- First hit, set 1 minute window
end

if count > 100 then
    print("Rate limit exceeded")
end
```

### Example: Caching
```lua
-- Set cleanup every 30 seconds
stash.cleanup(30)

-- Store a table with 5 second TTL
stash.set("user:session", { id = 42, active = true }, 5)

-- Retrieve it
local session = stash.get("user:session")
if session then
    print("User ID: " .. session.id)
end

-- Wait for expiry
core.set_timeout(6000, function()
    print(stash.get("user:session")) --> nil
end)
```

---

## assert
Test utility functions.

- `assert.equal(expected, actual, [message])`
- `assert.type(expected_type, value, [message])`
- `assert.truthy(value, [message])`

### Example
```lua
local user = { id = 1, name = "Luna" }

assert.equal(1, user.id, "User ID must be 1")
assert.type("table", user)
assert.truthy(user.name)
```

---

## Common Patterns

### Database-backed Web API
Combining `http`, `sqlite`, and `json`.

```lua
local db = sqlite.open("api.db")
db:exec("CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY, msg TEXT, ts DATETIME)")

http.route("POST", "/log", function(req, res)
    local data = req:json()
    if data and data.message then
        db:exec("INSERT INTO logs (msg, ts) VALUES (?, datetime('now'))", data.message)
        return res:status(201):json({ status = "saved" })
    end
    res:status(400):json({ error = "Missing message" })
end)

http.route("GET", "/logs", function(req, res)
    local rows = db:query("SELECT * FROM logs ORDER BY ts DESC LIMIT 10")
    res:json(rows)
end)

http.listen(8080)
http.run()
```

### Loading Config and Environment
```lua
-- Load .env file
env.load(".env")
local port = tonumber(env.get("APP_PORT")) or 3000

-- Load INI config
local config = ini.load("config.ini")
local db_path = config.database.path or "default.db"

log.info(string.format("Starting on port %d with db %s", port, db_path))
```
