function test ( a, b)
{
	return { a, "dummy", a + b }
}

function main ()
{
	local first
	local second

	{first, _, second} = test ( 2, 1 )

	printf ( "first: %d   second: %d\r\n", first, second );
}
