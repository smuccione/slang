function push ( a, value )
{
	a[len(a) + 1] = value;
	return a;
}


local x = {}

x.push ( "test" )

println ( x[1] );

local y;

y.push ( "test" )

println ( y[1] )

(@y).push ( "test" )

println ( y[1] )


