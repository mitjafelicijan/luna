print("Luna Example Suite")

local dir = fs.isdir("examples") and "examples" or "."
local files = fs.ls(dir)
if not files then
    print("Error: Could not list examples directory")
    return
end

for _, file in ipairs(files) do
    if file:match("%.lua$") and file ~= "run_all.lua" 
       and file ~= "http_server.lua" 
       and file ~= "sqlite_http_template.lua" then
        print("Running " .. file .. "...")
        local path = dir == "." and file or dir .. "/" .. file
        dofile(path)
        print("----------------------------------------")
    end
end
