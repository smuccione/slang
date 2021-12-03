class test
{
	local a = 1
	local b = 2

	access c ()
	{
		return 4
	}

	method getFunc()
	{
		return  function ( x )
				{
					return a + b + x + c
				}
	}
}


local o = new test()
local f = o.getFunc()
println ( f(3) )
