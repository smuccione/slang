local outer = { 5, 3, 7 }; 
local inner = { "bee", "giraffe", "tiger", "badger", "ox", "cat", "dog" }; 
local query

query = (from outer).join ( inner, (x) => x, (y) => len ( y ), (x, y) => "" + x + ":" + y );

foreach ( elem in query )
{
	println ( elem )
}

 query = from x in outer 
         join y in inner on x equals len ( y )
         select "" + x + ":" + y; 

foreach ( elem in query )
{
	println ( elem )
}
