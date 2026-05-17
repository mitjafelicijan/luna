log.use_colors(true)

log.info("--- Testing stash.cleanup ---")
stash.cleanup(1) -- cleanup every 1s

log.info("--- Testing stash.set and stash.get ---")
stash.set("name", "Luna", 0)
stash.set("version", 1.0, 0)
stash.set("config", { port = 8080, debug = true }, 0)

assert.equal("Luna", stash.get("name"))
assert.equal(1.0, stash.get("version"))
local config = stash.get("config")
assert.equal(8080, config.port)
assert.equal(true, config.debug)
log.info("Basic set/get passed")

log.info("--- Testing stash.del ---")
stash.del("name")
assert.equal(nil, stash.get("name"))
log.info("Delete passed")

log.info("--- Testing TTL ---")
stash.set("temp", "volatile", 2) -- 2s TTL
assert.equal("volatile", stash.get("temp"))

log.info("Waiting 3 seconds for TTL expiry...")
core.set_timeout(3000, function()
    local val = stash.get("temp")
    if val == nil then
        log.info("TTL expiry passed")
    else
        log.error("TTL expiry failed: key still exists")
        os.exit(1)
    end

    log.info("--- Testing stash.exists ---")
    stash.set("exists_test", true, 0)
    assert.equal(true, stash.exists("exists_test"))
    assert.equal(false, stash.exists("non_existent"))

    log.info("--- Testing stash.incr/decr ---")
    assert.equal(1, stash.incr("counter"))
    assert.equal(5, stash.incr("counter", 4))
    assert.equal(3, stash.decr("counter", 2))
    assert.equal(2, stash.decr("counter"))
    log.info("Incr/Decr passed")

    log.info("--- Testing stash.keys ---")
    stash.set("user:1", "alice")
    stash.set("user:2", "bob")
    stash.set("other", "data")
    local keys = stash.keys("user:%")
    assert.equal(2, #keys)
    log.info("Keys passed")

    log.info("--- Testing stash.ttl/expire ---")
    stash.set("ttl_test", "data", 10)
    local ttl = stash.ttl("ttl_test")
    assert.truthy(ttl > 0 and ttl <= 10)
    stash.expire("ttl_test", 60)
    assert.truthy(stash.ttl("ttl_test") > 50)
    log.info("TTL/Expire passed")

    log.info("--- Testing stash.count/clear ---")
    stash.clear()
    assert.equal(0, stash.count())
    log.info("Count/Clear passed")

    log.info("All stash module tests passed!")
    os.exit(0)
end)
