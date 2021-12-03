class base1
{
	local	a
	local	c
	
	method new ( a, v2 ) : a ( a + 1 ), c ( v2 )
	{
		return 1
	}
}

class base2
{
	local	b
	
	method new ( x ) 
	{
		b = x
		return 1
	}
}

class super
{
	inherit base1
	inherit base2
	
	method print()
	{
		printf ( "%i + %i + %i = %i\r\n", a, b, c, a + b + c )
	}
	
	method new( x ) : base1 ( 1, 2 ) , base2 ( 3 + x )
	{
		return 1
	}
}

local loop
local a
for ( loop = 0; loop < 1000; loop++ )
{
	a = new ( "super", 2 )
}
