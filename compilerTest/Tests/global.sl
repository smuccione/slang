global g1 = "g1"
global g2 = "g2"


function x ( y )
{
	println ( " g1 = ", g1 )
	println ( " y       = ", y )
	println ( " g2 = ", g2 )
}

class test
{
	local z

	method x ( y )
	{
		println ( " g1 = ", g1 )
		println ( " y       = ", y )
		println ( " z       = ", z )
		println ( " g2 = ", g2 )
	}

	method new()
	{
		z = "z" 
	}
}

println ( g1, " ", g2 )
g1 = "g11"
g2 = "g22"

local o = new ( "test" )

x ( "y1" )
o.x ( "y2" )
