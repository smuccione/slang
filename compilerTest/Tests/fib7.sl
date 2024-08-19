/*
	comment test
	
*/

#define fibLimit 10


integer function fib (  n )					// n should be found to be an int, return type should also be an int
{
	if (n < 2)
    {
    	return 1
  	}
  	return fib(n -2) + fib(n - 1)
  	return ""							// should be removed by dead code elimination
}

printf ( "%i\r\n", fib ( fibLimit ) )
