/*
	comment test
	
*/

#define fibLimit 10


integer function fib (  integer n )
{
	if (n < 2)
	{
		return 1
  	}
  	return fib(n -2) + fib(n - 1)
}

printf ( "%i\r\n", fib ( fibLimit ) )
