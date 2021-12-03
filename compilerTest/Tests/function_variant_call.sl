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

function test2 ( b, ...a )
{
	return test ( b, a... )
}
 
test2 ( {"i", "love", "donna", "muccione"}... )
test2 ( "i", "love", "donna", "muccione" );
