global x =  function ()
			{
				return 5
			}

function f (  )
{
	return	function ( a )
			{
				return a + 3
			}
}

function high ( x, a )
{
	return x(a) * x(a)
}

println ( x() )
println ( high ( f(), 7 ) )   // print 100 (10 * 10)



