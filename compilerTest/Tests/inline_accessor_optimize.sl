class common
{
	access x()
	{
		return "there"
	}

	method test()
	{
		println ( " hello " + x );
	}
}

local x = new ( "common" )

x.test()
println ( " hello " + x.x );

