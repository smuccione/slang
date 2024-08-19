function x( integer a )
{
	local x
	local y
	
	y = 0
	for ( x = 0; x < 10; x++ )
	{
		y += x
	}
	return  function ( c = 4 )
			{
				printf ( "a: %i  c: %i\r\n", a, c )
				return a + c
			}
}

function x2 ( f, val )
{
	return f ( val )
}

local a
local b

a = x ( 1 )
b = a(2)
println ( b )
b = a()
println ( b )
b = a(2, 3)
println ( b )
println ( x2 ( x, 2 ) )


