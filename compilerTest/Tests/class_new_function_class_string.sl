class common
{
	local	a
	local	b
	local	c
	local	d
	
	method meth1()
	{
		println ( "in 1" )
	}
	
	method meth2()
	{
		println ( "in 2" )
	}
	
	method new()
	{
		a = "a"
		b = "b"
		c = "c"
		d = "d"
	}
}

local o = new ( "common" )
println ( o.b )
o.b = "changed"
println ( o.b )


local x = "common"
local p = new ( x )
println ( p.b )
p.b = "changed"
println ( p.b )
