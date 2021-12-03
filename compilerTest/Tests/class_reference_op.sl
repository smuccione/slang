class test
{
	local _a = 0;

	property a {
		get () => @_a;
		set ( a ) => _a = a;
	}
	method get ()
	{
		return @_a;
	}
	access default ( name )
	{
		return @_a;
	}
	assign default ( name, value )
	{
		return _a = value;
	}
}

local o = new test;
println ( o._a, " should be 0" )
o.a++;
println ( o._a, " should be 1" )
o.a--;
println ( o._a, " should be 0" )
o.a += 2;
println ( o._a, " should be 2" )
o.get()++;
println ( o._a, " should be 3" )
o.z++;
println ( o._a, " should be 4" )
o.z += 2;
println ( o.z, " should be 6" )


