function one ( p )
{
	return p + "1"
}

function two ( p )
{
	return p + "2"
}

function three ( p )
{
	return p + "3"
}

function append ( p, p2 )
{
	return p + p2
}

local x = "test"

x.one().two().three().append ( "4" ).println()
"test".one().two().three().append ( "4" ).println()


