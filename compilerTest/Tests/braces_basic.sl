class common 
{
	local a

	const d = 100

	property x
	{
		get ( )=> a;
		set ( value )=> a = value;
	}

	method testAssign ( v )=> x = v;
	method testAccess ( )
	{
		return x;
	}
}

function double ( x					  /* value to double */ )=> x * 2;

function stringBuild ( sep, iter )
{
	local ret
	local first = true;

	foreach (i in iter)
	{
		if (first)
		{
			first = false;
				ret = i;
		} else
		{
			ret = ret + sep + i
		}
	}
	return ret;
}

function main ( )
{
	local source = { "abc", "hello", "def", "there", "four"  } ;
	local query = from x in source
				  group x by len ( x ) into values
				  select "" + values.key + ":" + stringBuild ( ";", values ); 

	foreach (elem in query)
	{
		println ( elem, " " )
	}

	for (local loop = 0; loop < 5; loop++)
		{ x=>println ( x )  } [1] ( "hello - " + loop );

	for (local loop = 1; loop < 10; loop++)
		if (loop % 2 == 1)
			printf ( "%i  odd\r\n", loop );
		else
			printf ( "%i  even\r\n", loop )

	foreach (loop in 1..10)
		switch (loop % 2)
		{
			case 1:
				printf ( "%i  odd\r\n", loop );
				break;
			case 0:
				printf ( "%i  even\r\n", loop )
				break;
		}

	local a = 99
	for (local a = 0; a < 10; a++)
	{
		println ( a )
	}
		println ( a, " should be 99" )

	local first = "steve";
	local last = "muccione";
	{
		local first;
			first = "Donna";
			println ( $"first = {first}, last = {last}" );
	}
	println ( $"first = {first}, last = {last}" );

	local x = new common;

		x.x = 1
		printf ( "x.a = %i\r\n", x.a )

		println ( $"double(4) = {double ( 4 )}\r\n" );
}

