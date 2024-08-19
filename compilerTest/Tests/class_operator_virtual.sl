class common
{
	local	a = 1
	
	virtual operator - ( val )
	{
		a = a + val + 1
	}
}

class super
{
	inherit common
	
	virtual operator - ( val )
	{
		a = a + val + 2
	}
}



local x = new ( "common" )

printf ( "x.a=%i\r\n", x.a )
x - 4
printf ( "x.a=%i\r\n", x.a )

local y = new ( "super" )

printf ( "y.a=%i\r\n", y.a )
y - 4
printf ( "y.a=%i\r\n", y.a )

