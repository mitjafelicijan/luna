print("Running JSON tests...")

local n = 123.45
local encoded = json.encode(n)
assert.equal("123.45", encoded, "Encode number")
assert.equal(n, json.decode(encoded), "Decode number")

local s = "hello world"
local encoded_s = json.encode(s)
assert.equal("\"hello world\"", encoded_s, "Encode string")
assert.equal(s, json.decode(encoded_s), "Decode string")

assert.equal("true", json.encode(true), "Encode true")
assert.equal(true, json.decode("true"), "Decode true")
assert.equal("false", json.encode(false), "Encode false")
assert.equal(false, json.decode("false"), "Decode false")

assert.equal("null", json.encode(nil), "Encode nil")
assert.equal(nil, json.decode("null"), "Decode null")

local arr = {1, 2, 3}
local encoded_arr = json.encode(arr)
assert.equal("[1,2,3]", encoded_arr, "Encode array")
local decoded_arr = json.decode(encoded_arr)
assert.type("table", decoded_arr, "Decoded array is table")
assert.equal(1, decoded_arr[1])
assert.equal(2, decoded_arr[2])
assert.equal(3, decoded_arr[3])

local obj = {a = 1, b = "test"}
local encoded_obj = json.encode(obj)
local decoded_obj = json.decode(encoded_obj)
assert.equal(1, decoded_obj.a, "Decode object field a")
assert.equal("test", decoded_obj.b, "Decode object field b")

local nested = {
    list = {1, {nested = true}},
    meta = "data"
}
local decoded_nested = json.decode(json.encode(nested))
assert.equal(1, decoded_nested.list[1])
assert.equal(true, decoded_nested.list[2].nested)
assert.equal("data", decoded_nested.meta)

print("JSON tests passed!")
