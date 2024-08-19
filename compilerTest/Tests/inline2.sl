function funcCall (  )
{
	local x
	for ( x = 0; x < 1; x++ )
	{
		println ( "simpleInline" )
	}
	return x;
}

local x
x = funcCall()
println ( x )

