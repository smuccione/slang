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

local a = new multimap ()
foreach ( value, index in b )
{
	a.insert ( index, value )
}

println ( "-------------- multi-map -------------- ")
foreach ( value, index in a )
{
	println ( $"{index} : {value}" );
}


a.insert ( "richard", "sedner" )
a.insert ( "richard", "sedner-muccione" )

println ( "-------------- map -------------- ")
foreach ( value, index in a.find("richard") )
{
	println ( $"{index} : {value}" );
}
