class test
{
	method a()
	{
		print ( "a" )
		return self
	}
	method b()
	{
		print ( "b" )
		return self
	}
	method c()
	{
		print ( "c" )
		return self
	}
	method d()
	{
		print ( "d" )
		return self
	}
}

local x = new test;

x.a()
 .b()
 .c()
 .d()
println ( "\r\ndone")
