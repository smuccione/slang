function funcCall ( x )
{
	println ( "in def param" )
	return 2 + x
}

function test ( p1, p2 = 2, p3 = funcCall ( 1 ) )
{
	return p1 + p2 + p3
}

println ( test ( 4, "a",  "a") )
println ( test ( 4 ) )

