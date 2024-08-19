
function test ()
{
	return { a: 1, b: 2 }
}


local x 
local y = 1

if ( y == 1 )
{
	if ( y == 1)
	{
		local status = "inner cleanup";
		println ( status );
		goto proc_end
	}
	label proc_end
}

x = test()


println("hello")
