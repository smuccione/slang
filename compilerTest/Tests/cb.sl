class test
{
	local x = "this is a test";

	method getFunc()
	{
		return {|a|println ( self.x, " ", a ) }
	}

	method getFunc2()
	{
		return &'{|a|println ( self.x, " ", a ) }'
	}
}

local o = new test();
o.getFunc()("of the american broadcasting system" )
o.getFunc2()("of the american broadcasting system" )

local	c
local	y
local	z
local	q

y = 3

c = {|a,b|a - y - b}

z = c(5,1)

println ( z )

q = &"z+y"

println ( q )

q = &"(c)(5,1)+y"

println ( q )
