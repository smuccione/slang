class test 
{
	method t()
	{
		return "test"
	}
	operator () ( a, b, c, d = " default" )
	{
		println ( "calling test with ", a, b, c, d )
	}
}

function test2 ( a, b, c, d = " default2" )
{
	println ( "calling test2 with ", a, b, c, d )
}

// direct dispatch
local o = new ( "test" )
	o ( 1, 2, 3 )
	o ( "hello ", "world" )
	o ( "hello ", "world ", "how ", "are ", "you " )

	// variant dispatch
local a = new ( "test" )
	a ( 1, 2, 3 )
	a = test2
	a ( 1, 2, 3 )



