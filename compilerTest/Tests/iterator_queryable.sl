iterator gen ()
{
	local loop
	for ( loop = 0; loop < 17; loop++ )
	{
		yield loop
	}
}

local v

v = (from gen()).select ( x => x * 2 ).where ( x => (x % 4 == 0) ).sum()

println ( v );

v = (from x in gen()
	 select x * 2 into y
	 where y % 4 == 0
	 select y).sum()

println ( v );
