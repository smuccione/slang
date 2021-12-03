local x = "this is a test "
local y = strswap ( x, " ", "%20" )
local z = webDecodeUrl ( y )

if ( x == z )
	printf ( "passed\r\n" )
else
	printf ( "failed\r\n" )

