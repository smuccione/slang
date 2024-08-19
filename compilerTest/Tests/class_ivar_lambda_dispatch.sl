class test
{
	local	f

	method new()
	{
		f = function ( x )
			{
				println ( "hello " + x )
			}
	}
}

local o = new test()

o.f ( "there" )
