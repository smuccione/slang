class testCls
{
	local _val = {}
	
	operator [ ( index )
	{
		return _val[index]
	}

	operator [ ( index, value )
	{
		_val[index] = value;
		return value;
	}

}



local x = new testCls ()

x[1] = "hello"
x[2] = "world"

println ( x[1], ' ', x[2] )

