function doit ( r )
{
	foreach ( loop in r )
	{
		println ( loop )
	}
}

foreach ( loop in 1..10 )
	if ( loop % 2 == 1 )
		printf ( "%i  odd\r\n", loop );
	else
		printf ( "%i  even\r\n", loop )

x = 1..10
doit ( x )


local b = 1
local c = 10
doit ( b..c )
