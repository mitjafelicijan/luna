local db, err = sqlite.open(":memory:")
if not db then
    error("Could not open database: " .. err)
end

db:exec([[
    CREATE TABLE users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        age INTEGER,
        email TEXT
    );
]])
db:exec([[
    CREATE TABLE tags (
        user_id INTEGER,
        tag TEXT
    );
]])

db:exec("INSERT INTO users (name, age, email) VALUES ('Alice', 25, 'alice@example.com');")
db:exec("INSERT INTO users (name, age, email) VALUES ('Bob', 30, 'bob@example.com');")
db:exec("INSERT INTO users (name, age, email) VALUES ('Charlie', 35, 'charlie@example.com');")

db:exec("INSERT INTO tags (user_id, tag) VALUES (1, 'Admin');")
db:exec("INSERT INTO tags (user_id, tag) VALUES (1, 'Developer');")
db:exec("INSERT INTO tags (user_id, tag) VALUES (2, 'User');")
db:exec("INSERT INTO tags (user_id, tag) VALUES (3, 'Manager');")
db:exec("INSERT INTO tags (user_id, tag) VALUES (3, 'Sales');")

local tpl = [[
<!DOCTYPE html>
<html>
<head>
    <title>{{ site.title }}</title>
</head>
<body>
    <h1>{{ site.header }}</h1>
    <p>Version: {{ site.info.version }}</p>

    <h2>User Profiles</h2>
    <table border="1" cellpadding="5" cellspacing="0">
        <tr>
            <th>Name</th>
            <th>Age</th>
            <th>Email</th>
            <th>Tags</th>
        </tr>
        {% for user in users %}
        <tr>
            <td>{{ user.profile.name }}</td>
            <td>{{ user.profile.age }}</td>
            <td>{{ user.profile.email }}</td>
            <td>
                {% for tag in user.tags %}
                [{{ tag }}] 
                {% endfor %}
            </td>
        </tr>
        {% endfor %}
    </table>

    <hr>
    <p>{{ site.footer.text }} ({{ site.footer.year }})</p>
</body>
</html>
]]

http.route("GET", "/", function(req, res)
    local user_rows, uerr = db:query("SELECT * FROM users;")
    if not user_rows then
        res:status(500):send("Database error: " .. (uerr or "unknown"))
        return
    end
    
    local users_data = {}
    for i, row in ipairs(user_rows) do
        local tags_rows, terr = db:query("SELECT tag FROM tags WHERE user_id = ?;", row.id)
        local tags = {}
        if tags_rows then
            for j, t in ipairs(tags_rows) do tags[j] = t.tag end
        end

        users_data[i] = {
            profile = {
                name = row.name,
                age = row.age,
                email = row.email
            },
            tags = tags
        }
    end

    local context = {
        site = {
            title = "Luna CMS",
            header = "User Management System",
            info = {
                version = "1.2.3",
            },
            footer = {
                text = "Built with Luna Runtime",
                year = 2026
            }
        },
        users = users_data
    }

    local html = template.render(tpl, context)
    if not html then
        res:status(500):send("Template rendering error")
    else
        res:status(200):send(html)
    end
end)

http.listen(8080)
http.run()
