class common
{
	local a
	
	method new()
	{
		variant x
		
		x = 5;
		
		printf ( "in common constructor\r\n" );
		a = { 1, 2 }
		a = { "steve": "muccione", 1 : "hello", 2.3: "donna" }
		a = 3
	}
}


class base1
{
	inherit common
	
	method new()
	{
		printf ( "in base1 constructor\r\n" );
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
		printf ( "in base2 constructor\r\n" );
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
		printf ( "base1.a = %i\r\n", base1.a )
		printf ( "base1.a = %i\r\n", base2.a )
	}
	
	method new()
	{
		printf ( "in super constructor\r\n" );
	}
}

new ( "super" ).print()

local x = new ( "serverbuffer" );
x += "hello"

println ( x.str )

