iterator t ()
{
	yield 1;
	yield 2;
	yield 3;
}

iterator t2( it, pred )
{
	foreach ( a in it )
	{
		if ( pred ( a ) )
		{
			yield a
		}
	}
}

iterator test ( it )
{
	local x = { 5, 6, 7 };

	local a = t2 ( t(), i => x[i] > 5 )
	foreach ( it in a )
	{
		yield it
	}
}

foreach ( it in test() )
{
	println ( it );
}