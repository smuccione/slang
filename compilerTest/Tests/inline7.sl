class test
{
	access a ()
	{
		return "hello"
	}

	access b ()
	{
		return "world"
	}

	method print ()
	{
		local c = " "
		return a + c + b
	}
}

println ( new test().print() )
