class common
{
	local	d
	local	e
	local	f
	local	g

	method new()
	{
		d = 2
		e = 2.0
		f = "in common"
		g = {{ 
				"test" : "this",
				"isLoggedOn" : 1
			}};
	}
}

class test
{
	local a
	local b
	local c
	inherit	common
	
	method new()
	{
		a = 1
		b = 1.0
		c = "works"
	}
}

local o = new ( 'test' )

println ( o.a, " ", o.b, " ", o.c )
println ( o.d, " ", o.e, " ", o.f )

local x = pack ( o )
local y = unpack ( x )

println ( y.a, " ", y.b, " ", y.c )
println ( y.d, " ", y.e, " ", y.f )

local z = objToArray ( y, false, false )

foreach ( i in z )
{
	printf ( $"{i[1]}={i[2]}\r\n" );
}

local o2 = varCopy ( o )
z = objToArray ( o2, false, false )
foreach ( i in z )
{
	printf ( $"{i[1]}={i[2]}\r\n" );
}

x = objToXml ( o )
println ( $"{x}\r\n");
