class common
{
	method print ()
	{
		println ( "correct" )
	}
}

class base1
{
	inherit common
	
	method print()
	{
		println ( "wrong" )
	}
}

class base2
{
	inherit base1
	
	method print()
	{
		println ( "wrong" )
	}
}

class base3
{
	inherit base2
	
	method print()
	{
		println ( "wrong" )
	}
}

class base4
{
	inherit base3
	
	method print()
	{
		println ( "wrong" )
	}
}

class super
{
	inherit base4
	
	method print()
	{
		base4.base3.base2.base1.common.print()
	}
}

local x = new ( "super" )
x.print()
x.base4.base3.base2.base1.common.print()
x.base4::base3::base2::base1::common::print()

local z = "super"
local y = new ( z )
y.print()
y.base4.base3.base2.base1.common.print()
y.base4::base3::base2::base1::common::print()
