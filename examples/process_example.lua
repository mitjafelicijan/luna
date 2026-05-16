print("--- Testing process.run ---")
local result = process.run("ls", {"-l"})
if result then
    print("Exit Code:", result.exit_code)
    print("Stdout:")
    print(result.stdout)
    if result.stderr ~= "" then
        print("Stderr:")
        print(result.stderr)
    end
end

print("\n--- Testing non-existent command ---")
local result2 = process.run("nonexistentcommand", {})
if result2 then
    print("Exit Code:", result2.exit_code)
end

print("\n--- Testing process.env ---")
local env = process.env()
print("PATH:", env.PATH)
print("USER:", env.USER)

print("\n--- Testing process.spawn ---")
local pid = process.spawn("sleep", {"1"})
if pid then
    print("Spawned process with PID:", pid)
end

print("\n--- Process Module Tests Complete ---")
