class base1
{
	local	a
	local	c
	
	method new ( a, v2 ) : a ( a + 1 ), c ( v2 )
	{
		println ( "in base1 constructor: ",  a, " : ", self.a, " : ", c )
	}
}

class base2
{
	local	b
	
	method new ( x ) 
	{
		b = x
		println ( "in base2 constructor: ", b )
	}
}

class base3
{
	local  d
	
	method new ( x = 4 )
	{
		d = x
	}
}

class super
{
	inherit base1
	inherit base2
	inherit	base3
	
	method print()
	{
		printf ( "%i + %i + %i + %i = %i\r\n", a, b, c, d, a + b + c + d )
	}
	
	method new( x = 1 ) : base1 ( x, 5 ) , base2 ( 3 )
	{
		println ( "in super constructor: ", x )
	}
}

new ( "super" ).print()

