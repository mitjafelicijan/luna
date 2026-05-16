local tpl = [[
Hello {{ name }}!
{% if age >= 18 %}
You are an adult ({{ age }}).
{% else %}
You are a minor ({{ age }}).
{% endif %}
]]

local data = {
    name = "Luna User",
    age = 25
}

local result = template.render(tpl, data)
print("Result 1:")
print(result)

data.age = 15
result = template.render(tpl, data)
print("Result 2:")
print(result)
