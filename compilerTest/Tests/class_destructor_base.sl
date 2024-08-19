class common
{
	method release()
	{
		println ( "in common destructor" )
	}
}

class base1
{
	virtual inherit common
	
	method release()
	{
		println ( "in base1 destructor" )
	}
}

class base2
{
	virtual inherit common
	
	method release()
	{
		println ( "in base2 destructor" )
	}
}

class super
{
	inherit base1
	inherit base2
}

local x = new ( "super" )
x.release()
