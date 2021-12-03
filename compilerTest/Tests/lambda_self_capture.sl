class test 
{
	local x = "hello"
	local y = "there"
	method test ()
	{
		local z = ( who ) => x + " " + y + " " + who
		return z;
	}
}

local o = new test();
local z = o.test()
println ( z ( "donna" ) )
