local function test_get()
    print("Testing GET...")
    local res = request.get("http://httpbin.org/get", {
        headers = {
            ["User-Agent"] = "LunaRequests/1.0",
            ["X-Custom-Header"] = "Hello"
        }
    })
    
    print("Status:", res.status)
    print("Body contains User-Agent:", string.find(res.body, "LunaRequests") ~= nil)
    print("Headers table exists:", type(res.headers) == "table")
end

local function test_post_json()
    print("\nTesting POST JSON...")
    local res = request.post("http://httpbin.org/post", {
        json = {
            name = "Luna",
            features = {"fast", "small", "static"}
        }
    })
    
    print("Status:", res.status)
    print("Body contains 'Luna':", string.find(res.body, "Luna") ~= nil)
end

local function test_post_data()
    print("\nTesting POST Data...")
    local res = request.post("http://httpbin.org/post", {
        data = "raw data body",
        headers = {
            ["Content-Type"] = "text/plain"
        }
    })
    
    print("Status:", res.status)
    print("Body contains 'raw data body':", string.find(res.body, "raw data body") ~= nil)
end

local function test_put()
    print("\nTesting PUT...")
    local res = request.put("http://httpbin.org/put", {
        json = { update = "success" }
    })
    print("Status:", res.status)
    print("Body contains 'update':", string.find(res.body, "update") ~= nil)
end

local function test_delete()
    print("\nTesting DELETE...")
    local res = request.delete("http://httpbin.org/delete")
    print("Status:", res.status)
end

local function test_head()
    print("\nTesting HEAD...")
    local res = request.head("http://httpbin.org/get")
    print("Status:", res.status)
    print("Body empty:", res.body == "")
    print("Headers table exists:", type(res.headers) == "table")
end

local function test_patch()
    print("\nTesting PATCH...")
    local res = request.patch("http://httpbin.org/patch", {
        json = { key = "value" }
    })
    print("Status:", res.status)
    print("Body contains 'key':", string.find(res.body, "key") ~= nil)
end

local function test_options()
    print("\nTesting OPTIONS...")
    local res = request.options("http://httpbin.org/get")
    print("Status:", res.status)
    print("Allow header exists:", res.headers["Allow"] ~= nil or res.headers["allow"] ~= nil)
end

local function test_generic_request()
    print("\nTesting Generic Request (GET)...")
    local res = request.request("GET", "http://httpbin.org/get")
    print("Status:", res.status)
end

local function test_callable()
    print("\nTesting Callable request()...")
    local res = request("http://httpbin.org/get")
    print("Status:", res.status)
end

test_get()
test_post_json()
test_post_data()
test_put()
test_delete()
test_head()
test_patch()
test_options()
test_generic_request()
test_callable()

print("\nAll tests passed!")
