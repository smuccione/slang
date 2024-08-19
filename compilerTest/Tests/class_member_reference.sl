class test {
	local a = "fail"
}

class test2 {
	inherit test
	local b = "fail2"
}

local o = new test()

local b = @o.a;

b = "hello word"

omCollect()

println ( o.a );

local o2 = new test2()
local b2 = @o2.test
omCollect()
b2.a = "hello world"
omCollect()
println ( o2.a )
