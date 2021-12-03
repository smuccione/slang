function stringBuild ( sep, iter )
{
	local ret
	local first = true;

	foreach ( i in iter )
	{
		if ( first )
		{
			first = false;
			ret = i;
		} else
		{
			ret = ret + sep + i
		}
	}
	return ret
}


local outer = { 5, 3, 7 }; 
local inner = { "bee", "giraffe", "tiger", "badger", "ox", "cat", "dog" }; 
local query



query = from x in outer 
        join y in inner on x equals len ( y ) into matches 
		select "" + x + ":" + stringBuild (";", matches); 


foreach ( elem in query )
{
	println ( elem )
}
