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

function main()
{
	local source = { "abc", "hello", "def", "there", "four" }; 
	local query

	query = from x in source 
			group x[1] by len ( x ) into values
			select "" + values.key + ":" + stringBuild ( ";", values);

	foreach ( elem in query )
	{
		println ( elem, " " )
	}
}
