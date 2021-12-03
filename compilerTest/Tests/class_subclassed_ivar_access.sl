class common
{
	local a
	
	method new()
	{
		printf ( "in common constructor\r\n" )
		a = 3
	}
}


class base1
{
	inherit common
	
	method new()
	{
		printf ( "in base1 constructor\r\n" )
		a = 1
	}
	method print()
	{
		printf ( "1: a = %i\r\n", a )
	}
}

class base2
{
	virtual inherit common
	
	method new()
	{
		printf ( "in base2 constructor\r\n" )
		a = 2
	}
	method print()
	{
		printf ( "2: a = %i\r\n", a )
	}
}


class super
{
	inherit base1
	inherit base2
	
	method print()
	{
		base1.print()
		base2.print()
		printf ( "a = %i\r\n", a )
		printf ( "a = %i\r\n", base2.a )
	}
	
	method new()
	{
		printf ( "in super constructor\r\n" )
	}
}

local x =new ( "super" )
x.print()
printf ( "------------------------------\r\n" )

printf ( "x.a = %i\r\n", x.a )
printf ( "a:base2:a = %i\r\n", x.base2.a )
printf ( "------------------------------\r\n" )

x.a = 69
printf ( "x.a = %i\r\n", x.a )
printf ( "x.base2.a = %i\r\n", x.base2.a )
printf ( "------------------------------\r\n" )

x.base2.a = 96

printf ( "x.a = %i\r\n", x.a )
printf ( "x.base2.a = %i\r\n", x.base2.a )

printf ( "------------------------------\r\n" )
println ( "type of x = ", type ( x ) )
println ( "type of x.base2 = ", type ( x.base2 ) )
