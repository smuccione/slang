class test
{
	inherit classA
	inherit classB
	
	
	method new (a, b) : classA ( a ), classB ( b )
	{
		local c
		
		c = a + b
		return 1
	}

	assign x ( value )
	{
		::iVar = value
	}
	
	access x()
	{
		1/0
		return ( ::iVar )
	} 
	
	private:
		local	iVar
		
}

function main ( p1, p2 = "this", p3 = funcCall() )
{
	local	a
	
	local	b = 2
	
	local	c
	
	c = 5
	
	x = 1 / 0
	
	a = b + c
	
	return ( a )
}
