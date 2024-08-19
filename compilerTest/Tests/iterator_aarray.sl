local a = 	{
				"steve" : "steve muccione",
				"donna" : "donna muccione",
				"justin" : "justin heddden",
				"morganne" : "morganne hedden",
				"sam" : "sam muccione",
				"aislinn" : "aislinn muccione",
				"kiersten" : "kiersten muccione",
				"richard" : "richard muccione"
			}
	

foreach ( value in a )
{
	println ( value )
}

println()

foreach ( value, name in a )
{
	println ( name, " : ", value )
}

