#define fibLimit 46

function fib ( array a, n )
{	
	if ( a[n] ) return a[n]
	if ( n < 2 ) return 1
  	return a[n] = fib(a, n - 1) + fib(a, n - 2)
}

local a = array(0,50);

printf ( "%i\r\n", fib ( a, fibLimit ) )
