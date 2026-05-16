local url = "https://www.google.com/images/branding/googlelogo/1x/googlelogo_color_272x92dp.png"
local filename = "google_logo.png"

print("Downloading " .. url .. " to " .. filename .. "...")
local res = request.download(url, filename)

if res then
    print("Status:", res.status)
    print("Saved to:", res.path)
    
    local f = io.open(filename, "rb")
    if f then
        local size = f:seek("end")
        f:close()
        print("File size:", size, "bytes")
        if size > 0 then
            print("Download successful!")
        else
            print("Error: Downloaded file is empty.")
        end
        os.remove(filename)
    else
        print("Error: Could not open downloaded file.")
    end
else
    print("Download failed.")
end
