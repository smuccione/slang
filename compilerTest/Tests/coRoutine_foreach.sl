function test()
{
	local loop
	for ( loop = 0; loop < 10; loop++ )
	{
		yieldReturn ( loop )
	}
}


local x

x = new coRoutine ( test )

foreach ( i in x )
{
	println ( i )
}

