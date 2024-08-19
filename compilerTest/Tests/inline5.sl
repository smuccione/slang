/*
	comment test
	
*/

#define fibLimit 10

function fibTail ( n, res, next )
{
	if ( n == 1 )
	{
		return res
	}
	return fibTail ( n - 1, next, res + next )
}

function fib ( n )
{
	if ( !n )
	{
		return 0;
	}
	return fibTail ( n + 1, 1, 1 )
}

printf ( "%i\r\n", fib ( fibLimit ) )
