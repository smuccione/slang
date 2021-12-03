function test ( a, b, ...c )
{
	print ( a, " " );
	print ( b, " " );
	foreach ( x in c )
	{
		print ( x, " " );
	}
	println();
}

function test2 ( a, ...c )
{
	print ( a, " " )
	if ( c )
		test2 ( c... )
	else
		println();
}

 
test ( "i", "love", "donna", "muccione" );
test2 ( "i", "love", "donna", "muccione" );
