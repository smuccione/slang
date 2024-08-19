
function funcCall ( x )
{
	println ( "in def param" )
	return 2 + x
}

class common
{
	method test ( p1, p2 = 2, p3 = funcCall ( 1 ) )
	{
		return p1 + p2 + p3
	}
	
	method doIt()
	{
		println ( test ( 4, , , "a", "b" ) )
		println ( test ( 4 ) )
	}
}

local x = new ( "common" )
x.doIt()

