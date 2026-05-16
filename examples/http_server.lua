http.route("GET", "/", function(req, res)
    res:status(200):send("<h1>Welcome to Luna with cJSON</h1>")
end)

http.route("POST", "/echo", function(req, res)
    local data = req:json()
    if data then
        res:status(200):json({
            status = "echo",
            received = data,
            method = req.method
        })
    else
        res:status(400):json({ error = "Invalid or missing JSON body" })
    end
end)

http.route("GET", "/utils", function(req, res)
    local encoded = json.encode({ a = 1, b = { 2, 3 } })
    local decoded = json.decode(encoded)
    res:status(201):json({
        encoded = encoded,
        decoded = decoded
    })
end)


http.listen(8080)
http.run()
