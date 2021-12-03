local b =  {
		"steve" : "muccione",
		"donna" : "muccione",
		"justin" : "heddden",
		"morganne" : "hedden",
		"sam" : "muccione",
		"aislinn" : "muccione",
		"kiersten" : "muccione",
		"richard" : "muccione",
	}

println ( "-------------- aArray -------------- ")
foreach ( value, index in b )
{
	println ( $"{index} : {value}" );
}

local a = new multiset ()
foreach ( value, index in b )
{
	a.insert ( index )
}

a.insert ( "richard" )

println ( "-------------- map -------------- ")
foreach ( value in a.find("richard") )
{
	println ( $"{value}" );
}
