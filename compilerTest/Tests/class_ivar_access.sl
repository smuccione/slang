class test
{
private:
	local	a = 1
	local	b = 2
public:
	method sum ( c )
	{
		return a + b + c
	}
}

local o = new test()

println ( o.sum ( 3 ) )
