local env_path = fs.exists(".env") and ".env" or "examples/.env"
print("Loading " .. env_path .. "...")
local vars = env.load(env_path)

if vars then
    print("Variables loaded successfully:")
    for k, v in pairs(vars) do
        print(string.format("  %s = %s", k, v))
    end
else
    print("Failed to load .env file")
end

print("\nVerifying with os.getenv:")
print("PORT:", os.getenv("PORT"))
print("DB_URL:", os.getenv("DB_URL"))
print("DEBUG:", os.getenv("DEBUG"))
print("APP_NAME:", os.getenv("APP_NAME"))

print("\nLoading custom.env...")
local f = io.open("custom.env", "w")
f:write("CUSTOM_VAR=hello\n")
f:close()

local custom_vars = env.load("custom.env")
print("CUSTOM_VAR:", os.getenv("CUSTOM_VAR"))

os.remove("custom.env")
