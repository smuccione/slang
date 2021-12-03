class common
{
	local	a
	
	const	d = 100
	
	assign x ( v )
	{
		a = v
		printf ( "assigning %i to a\r\n", v )
	}
	
	access x ()
	{
		return a
	}
	
	method testAssign ( v )
	{
		x = v
	}
	
	method testAccess()
	{
		return x
	}
}

local x = new ( "common" )

x.x = 1
printf ( "x.a = %i\r\n", x.a )

x.testAssign ( 2 )
printf ( "x.a = %i\r\n", x.a )

printf ( "x.x = %i\r\n", x.x )
printf ( "x.testAccess() = %i\r\n", x.testAccess() )

printf ( "=================\r\n" )
printf ( "common.d = %i\r\n", common.d )
