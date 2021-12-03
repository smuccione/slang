/*
	comment test
	
*/

#define fibLimit 10

function fibI ( n )
{
    local cur = 0;
	local last = 1;

    while ( n >= 0 )
    {
    	n -= 1;
        {cur, last} = {last, cur + last}
    }
    return cur;
}

function fibR ( n )
{
	if (n < 2)
    {
    	return 1
  	}
  	return fibR(n -2) + fibR(n - 1)
}

println ("fib: ", fibR(fibLimit),  " = ", fibI(fibLimit));

