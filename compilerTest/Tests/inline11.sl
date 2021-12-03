class test
{
private:
	local _a
	local _b

public:
	assign a ( value )
	{
		_a = value
	}

	assign b ( value )
	{
		_b = value
	}

	access a ( )
	{
		return _a;
	}

	access b ( )
	{
		return _b;
	}

	method print()
	{
		println ( a + " " + b )
	}
}

local o = new test()

o.a = "hello"
o.b = "world"

o.print()
