-- Example showing path manipulation, system metadata, and FS enhancements

log.info("Runtime Version: " .. core.version)
log.info("Platform: " .. core.platform)
log.info("Arch: " .. core.arch)

-- Path manipulation
local root = fs.cwd()
local target_dir = path.join(root, "temp_example")
local target_file = path.join(target_dir, "hello.txt")
local copy_file = path.join(target_dir, "hello_copy.txt")

log.info("Target file: " .. target_file)

-- Create directory
if not fs.exists(target_dir) then
    log.info("Creating directory: " .. target_dir)
    fs.mkdir(target_dir, true)
end

-- Write file
log.info("Writing file...")
fs.writefile(target_file, "Hello from Luna path and fs example!")

-- Copy file
log.info("Copying file...")
fs.copy(target_file, copy_file)

-- Stat file
local st = fs.stat(copy_file)
log.info("Copied file size: " .. st.size .. " bytes")

-- List directory
log.info("Contents of " .. target_dir .. ":")
local files = fs.ls(target_dir)
for _, f in ipairs(files) do
    print("  - " .. f)
end

-- Asynchronous cleanup after 2 seconds
core.set_timeout(2000, function()
    log.info("Cleaning up...")
    fs.remove(target_file)
    fs.remove(copy_file)
    fs.remove(target_dir)
    log.info("Done!")
end)

log.info("Waiting 2 seconds before cleanup...")
