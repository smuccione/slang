class testClass
{
	local x
	method new()
	{
		x = 1;
	}
}

local x = testClass()
x.x++;
println ( "done" );
