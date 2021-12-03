#define MAX_ITERATIONS 1000
#define BAILOUT 16

function iterate(x,y)
{
	local cr = y-0.5
	local ci = x
	local zi = 0.0
	local zr = 0.0
	local i = 0
	while (1) 
	{
		i++
		local temp = zr * zi
		local zr2 = zr * zr
		local zi2 = zi * zi
		zr = zr2 - zi2 + cr
		zi = temp + temp + ci
		if (zi2 + zr2 > BAILOUT)
		{
			return i
		}
		if (i > MAX_ITERATIONS)
		{
			return 0
		}
	}
}

function Mandelbrot()
{
	double x
	double y
	for (y = -39; y < 39; y++)
	{
//		printf("\n")
		for (x = -39; x < 39; x++)
		{
			if ( iterate(x/40.0,y/40.0) == 0)
			{
//				printf("*")
			} else 
			{
//				printf(" ")
			}
		}
	}
}

Mandelbrot()
