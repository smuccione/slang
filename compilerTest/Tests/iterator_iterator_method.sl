class test
{
	local	x = "test"

	iterator getCount ( start, stop )
	{
		println ( x );
		for ( local loop = start; loop < stop; loop++ )
		{
			yield loop;
		}
	}
}


foreach ( x in new test().getCount ( 10, 20 ) )
{
	println ( x )
}
