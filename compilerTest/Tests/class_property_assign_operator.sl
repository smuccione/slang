class test
{
	local val;
	property x {
		get ()
		{
			return @val;
		}
		set ( value )
		{
			val = value;
		}
	}
}

local o = new test;

o.x = 0;
println ( o.x )
o.x++;
println ( o.x )
o.x--;
println ( o.x )
o.x += 2;
println ( o.x )



