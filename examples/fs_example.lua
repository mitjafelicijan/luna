print("Creating directories...")
local ok, err = fs.mkdir("example_dir/data/logs", true)
if not ok then
    print("Error creating directory: " .. err)
    return
end

local test_file = "example_dir/data/test.txt"
print("Writing to file: " .. test_file)
local success, werr = fs.writefile(test_file, "Luna Runtime - File System Example\nGenerated at: " .. os.date())
if not success then
    print("Error writing file: " .. werr)
end

if fs.exists(test_file) then
    print("Verification: " .. test_file .. " exists.")
end

local st = fs.stat(test_file)
if st then
    print(string.format("File Stats - Size: %d bytes, Type: %s, Modified: %s", 
        st.size, st.type, os.date("%Y-%m-%d %H:%M:%S", st.mtime)))
end

local content = fs.readfile(test_file)
if content then
    print("File Content:")
    print("--------------------")
    print(content)
    print("--------------------")
end

print("Listing contents of example_dir/data:")
local files = fs.ls("example_dir/data")
if files then
    for i, name in ipairs(files) do
        print(string.format("  [%d] %s", i, name))
    end
end

fs.remove(test_file)
print("Removed " .. test_file)
