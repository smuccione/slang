
function x(func)
{
	println(func(1, 2))
}

x((x, y) => x - y + 1)
