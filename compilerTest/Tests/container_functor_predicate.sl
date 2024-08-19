local x = { "a", "z", "c", "d" }

local b = new set ( (a,b) => a > b )
b.add ( x )

foreach ( it in b )
{
	println ( it )
}