function tail ( x )
{
	if ( x < 100000 )
	{
		return tail ( x + 1 )
	} else
	{
		return x
	}
}


println ( tail ( 0 ) )
