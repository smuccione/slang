class fib
{
	integer method fib ( integer x )
	{
		if ( x < 2 )
		{
			return 1
		}
		return fib ( x - 1 ) + fib ( x - 2 )
	}
}

println ( "result = ", new ( "fib" ).fib ( 10 ) )
