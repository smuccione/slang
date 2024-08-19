local x = { "a", "z", "c", "d" }

variant b = new set ( (a,b) => a > b )
b.add ( x )

foreach ( it in b )
{
	println ( it )
}