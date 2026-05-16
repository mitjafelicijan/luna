local db, err = sqlite.open(":memory:")
if not db then
    error("Could not open database: " .. err)
end

db:exec([[
    CREATE TABLE users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        level INTEGER,
        score REAL
    );
]])

print("--- Inserting Data ---")
db:exec("INSERT INTO users (name, level, score) VALUES (?, ?, ?)", "Alice", 10, 95.5)
db:exec("INSERT INTO users (name, level, score) VALUES (?, ?, ?)", {"Bob", 5, 88.0})

print("--- Named Parameters ---")
local rows = db:query("SELECT * FROM users WHERE name = :name", { [":name"] = "Alice" })
if rows and rows[1] then
    print("Found Alice, Score: " .. rows[1].score)
end

print("--- Querying All ---")
local all = db:query("SELECT * FROM users ORDER BY score DESC")
for i, row in ipairs(all) do
    print(string.format("[%d] %s (Level %d) - Score: %g", row.id, row.name, row.level, row.score))
end

db:close()
print("SQLite example complete.")