class test 
{
	method test ( a, b, ...c )
	{
		print ( a, " " );
		print ( b, " " );
		foreach ( x in c )
		{
			print ( x, " " );
		}
		println();
	}

	method test2 ( b, ...a )
	{
		return test ( b, a... )
	}
}

local x = new test()

x.test2 ( {"i", "love", "donna", "muccione"}... )
x.test2 ( "i", "love", "donna", "muccione" );
