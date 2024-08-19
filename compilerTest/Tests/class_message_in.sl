class a
{
	method print ()
	{
		println ( "in a" );
	}
}

class b
{
	inherit a;
	message printa is print in a
	method print()
	{
		printa();
		println ( "in b" );
	}
}

local bO = new b;
bO.print()


