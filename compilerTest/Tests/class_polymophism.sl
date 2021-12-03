class a
{
	method print ()
	{
		println ( "in a" );
	}
}

class b
{
	method print()
	{
		println ( "in b" );
	}
}

function p ( obj )
{
	obj.print()
}

local aO = new a;
local bO = new b;
p(aO)
p(bO)


