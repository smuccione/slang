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
		a = "hello"
		b = "world"
		return _a + " " + _b
	}
}

println ( new test().print() )
