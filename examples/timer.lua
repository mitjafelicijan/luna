local timer1_fired = false
local timer2_fired = false
local order = {}

core.set_timeout(100, function()
    timer1_fired = true
    table.insert(order, 1)
    print("Timer 1 fired (100ms)")
end)

core.set_timeout(50, function()
    timer2_fired = true
    table.insert(order, 2)
    print("Timer 2 fired (50ms)")
end)

core.set_timeout(200, function()
    assert.truthy(timer1_fired, "Timer 1 should have fired")
    assert.truthy(timer2_fired, "Timer 2 should have fired")
    assert.equal(2, order[1], "Timer 2 should fire first")
    assert.equal(1, order[2], "Timer 1 should fire second")
    print("Timer tests passed!")
end)
