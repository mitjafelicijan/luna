print("Testing HTTPS GET (google.com)...")
local res = request.get("https://www.google.com")
if res then
    print("Status:", res.status)
    print("Body length:", #res.body)
else
    print("HTTPS request failed")
end
