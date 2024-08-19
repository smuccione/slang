class test
{
	local hi = 4
	local lo = 2
}

function main ()
{
	local x = new test()

	printf ( "hi: %d   low: %d\r\n", x.hi, x.lo );

	{x.hi, x.lo} = {x.lo, x.hi};

	printf ( "hi: %d   low: %d\r\n", x.hi, x.lo );
}
