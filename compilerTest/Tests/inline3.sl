function c(a, b)
{
	a = a + 1
	b = b + 1
	return a * b
}

function b(a, b)
{
	return c(a, b) + c(b, a)
}

println(b(1, 1))
