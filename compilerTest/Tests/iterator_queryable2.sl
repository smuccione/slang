
class person
{
	local first
	local last
	method new ( first, last ) : first ( first ), last ( last )
	{
	}
	method print ()
	{
		printf ( "first: %s   last: %s\r\n", first, last );
	}
}

local v

local a = { 
			"12312412" :	{
								first : "steve",
								last : "muccione"
							},
			"43553434" :	{
								first : "donna",
								last : "muccione"
							},
			"77585843" :	{
								first : "steve",
								last : "repetti"
							},
			"56456444" :	{
								first : "angie",
								last : "repetti"
							},
		   }
	
v = (from a).where ( x => x.first == "steve" ).select ( y => new person ( y.first, y.last ) )

foreach ( x in v )
{
	x.print()
}
