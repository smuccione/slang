local x
local y

local z = { "a", "b", "c", "d" }
foreach ( it in z )
{
	switch ( upper ( it ) )
	{
		case "A":
			println ( "A" );
			break;
		case "B":
			println ( "B" );
			break;
		case "C":
			println ( "C" );
			break;
		default:
			println ( "DEFAULT" );
			break;
	}
}

for ( x = 1; x <= 10; x++ )
{
	switch ( x )
	{
		case 1
			y = 1
			break
		case 2
		case 3
			y = 2
			break
		case 4
			y = 4
		case 5
			y = 5
			break
		case 6
		default
			y = 999
			break
		case 7
			y = 7
			break
	}
	printf ( "x: %i  y: %i\r\n", x, y )
}

for ( x = 1; x <= 10; x++ )
{
	switch ( x )
	{
		case 1
			y = 1
			break
		case 2
		case 3
			y = 2
			break
		case 4
			y = 4
		case 5
			y = 5
			break
		case 6
			y = 999
			break
		case 7
			y = 7
			break
	}
	printf ( "x: %i  y: %i\r\n", x, y )
}

