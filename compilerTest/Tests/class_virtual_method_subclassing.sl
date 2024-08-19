class common
{
	local   a
	local	b

	virtual method print ()
	{
		printf ( "1: %s\r\n", a )
	}
	
	method doPrint()
	{
		print()
	}
	
	method new()
	{
		a = "in common"
		b = "inherited" 
	}
}


class base1
{
	inherit common

	local a

	virtual method print ()
	{
		printf ( "2: %s   %s\r\n", a, b )
		common::print()
	}
	
	method new()
	{
		a = "in base1"
	}
}

class super
{
	inherit base1
}

local x = new ( "super" )

x.base1::common.print()
x.base1::common::print()
