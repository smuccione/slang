function x( integer a )
{
	local x = 0;
	local z = 0;

	x++;
	z++;

	return  function ( c = 4 )
			{
				printf ( "a: %i  c: %i\r\n", a, c )
				return a + c
			}
}

local a
local b

a = x ( 1 )
b = a(2)
