function generator ()
{
	for ( local x = 0; x <= 100; x += 5 )
	{
		yieldReturn ( x )
	}
	return "<done>"
}

local x

x = new coRoutine ( generator )

foreach ( it in x )
{
	println ( it )
}