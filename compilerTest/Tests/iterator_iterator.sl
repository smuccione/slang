iterator getCount ( start, stop )
{
	for ( local loop = start; loop < stop; loop++ )
	{
		yield loop;
	}
}

foreach ( x in getCount ( 10, 20 ) )
{
	println ( x )
}
