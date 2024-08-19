class functor
{
	local test

	method new ( test ) : test ( test )
	{
	}
	operator () ( str )
	{
		return println ( str, " ", test )
	}
}

local test = "wrong"
local a = new functor( @test )
test = "world"
a ( "hello" )


