http.ws("/ws", function(ws)
	print("New WebSocket connection!")

	ws:on("message", function(msg)
		print("Received message:", msg)
		ws:send("Echo: " .. msg)
	end)

	ws:on("close", function()
		print("WebSocket connection closed")
	end)
end)

http.listen(8080)
print("Server listening on port 8080")
http.run()
