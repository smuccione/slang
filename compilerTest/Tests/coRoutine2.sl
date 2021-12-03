function main()
{
	variant c = "test"
	local loop = 0;

	local x = new coRoutine ( 
								function ( a, b ) 
								{
									println ( "a: " + a + "  b: " + b + "   c: " + c )
									loop = funcA()
									loop = funcB( loop )
									loop = yieldReturn ( "first " + loop )
									loop = yieldReturn ( "second "+ loop  )
									loop = yieldReturn ( "third "+ loop  )
									return "}"
								}
							)
	x ( 1, 2 )

	while ( x.isReady )
	{
		println ( x( ++loop) )
	}
}

function funcA()
{
	return yieldReturn ( "from a - should not print, this is from the first x ( 1, 2 ) call" );
}

function funcB(loop)
{
	return yieldReturn ( "from b " + loop );
}


