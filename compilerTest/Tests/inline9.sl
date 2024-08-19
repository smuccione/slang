class test
{
	local _a
	local _b

	assign a ( value )
	{
		_a = value
	}

	assign b ( value )
	{
		_b = value
	}

	method print ()
	{
		return _a + " " + _b
	}
}

local o = new test()

o.a = "hello"
o.b = "world"

println ( o.print() )
