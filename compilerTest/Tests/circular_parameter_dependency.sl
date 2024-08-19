FUNCTION objCopy ( o1, o2 )
{
	return (varCopy ( o1, o2 ))
}

class test3 
{
	local descProperty = "test"
}

class test1 
{
	local data

	method descShort ( o )
	{
		local s = ""

		if ( o.data )
		{
			o = objcopy ( o.data )
		}
		if ( o.descProperty )
		{
			s = o.descProperty + ", "
			println ( o.descProperty )
		}
	}
}

class test2 
{
	method render ( incident )
	{
		local prop = new test1()

		prop.data = incident

		prop.descShort ( prop )
	}
}

local o = new test2()

o.render ( new test3() )

