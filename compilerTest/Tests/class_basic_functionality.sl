class test
{
	local c
	
	method new (a, b)
	{
		c = a + b
		return 1
	}
	
	method test2 ( a, b )
	{
		printf ( "%s %s  %i\r\n", a, b, c )
	}
	
	method test ( a, b )
	{
		test2 ( a, b )
	}

	assign x ( value )
	{
		iVar = value
	}
	
	access x()
	{
		return ( iVar )
	} 
	
	private:
		local	iVar
		
}

local x = new ( "test", 1, 2 )

x.test ( "hello", "world" )
x.x = 4
println ( x.x )
