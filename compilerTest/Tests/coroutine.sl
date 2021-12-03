function funcA()
{
	return yieldReturn ( "from a" );
}

function funcB(loop)
{
	return yieldReturn ( "from b " + loop );
}

function gen ( a, b, c )
{
	println ( "a: " + a + "  b: " + b + "   c: " + c )
	local loop = funcA()
	loop = funcB( loop )
	loop = yieldReturn ( "first " + loop )
	loop = yieldReturn ( "second "+ loop  )
	loop = yieldReturn ( "third "+ loop  )
	return "}"
}

local x = new coRoutine ( gen )

x ( 1, 2, "test" )
local loop = 0;

while ( x.isReady )
{
	println ( x( ++loop) )
}
