class common
{
	local a = 0;
	
	property x {
		get () => testAccess;
		set ( value ) => testAssign = value;
	}

	assign testAssign ( v ) => a = v;
	access testAccess()
	{
			return a;
	}
}

function main ()
{
	local x = new common;

	x.x = 1
	printf ( "x.a = %i   (should be 1) \r\n", x.a )
}

