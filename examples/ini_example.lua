local source_file = fs.exists("config.ini") and "config.ini" or "examples/config.ini"
local target_file = "target.ini"

if not fs.exists(source_file) then
    print("Error: " .. source_file .. " not found")
    return
end

print("Loading from: " .. source_file)
local cfg, err = ini.load(source_file)
if not cfg then
    print("Error loading: " .. (err or "unknown"))
    return
end

print("Modifying data...")
cfg.server.port = "9090"
cfg.new_settings = {
    theme = "dark",
    notifications = "enabled"
}

print("Creating new file: " .. target_file)
local ok, save_err = ini.save(target_file, cfg)
if not ok then
    print("Error saving: " .. (save_err or "unknown"))
    return
end

print("\nVerifying " .. target_file .. " contents:")
local check = io.open(target_file, "r")
if check then
    print(check:read("*a"))
    check:close()
end

os.remove(target_file)
