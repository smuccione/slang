function f ( a )
{
	return a + 3
}

function high ( x, a )
{
	return x(a) * x(a)
}

println ( typex ( f ) )
println ( high ( f, 7 ) )   // print 100 (10 * 10)



