local data = "Hello, Luna!"
local key = "secret-key"

print("Original data: " .. data)

local md5 = crypto.md5(data)
local sha256 = crypto.sha256(data)
local sha512 = crypto.sha512(data)

print("MD5:    " .. crypto.hex_encode(md5))
print("SHA256: " .. crypto.hex_encode(sha256))
print("SHA512: " .. crypto.hex_encode(sha512))

local hmac_sha256 = crypto.hmac("sha256", key, data)
print("HMAC-SHA256: " .. crypto.hex_encode(hmac_sha256))

local b64 = crypto.base64_encode(data)
print("Base64: " .. b64)
local decoded = crypto.base64_decode(b64)
print("Decoded: " .. decoded)
assert.equal(data, decoded)

local hex = crypto.hex_encode(data)
print("Hex: " .. hex)
local unhex = crypto.hex_decode(hex)
print("Unhex: " .. unhex)
assert.equal(data, unhex)

local rand = crypto.random(16)
print("Random (16 bytes, hex): " .. crypto.hex_encode(rand))
assert.equal(16, #rand)

print("\nAll crypto tests passed!")
