function test ( which )
{
	switch ( which )
	{
		case 1:
			return "one"
		case 2:
			return "two"
		default:
			return "other"
	}
}

function dbl ( x ) => x * 2;

println ( test ( 1 ) )
println ( test ( 2 ) )
println ( test ( 3 ) )
dbl ( 4 )
println ( dbl ( 4 ) )


