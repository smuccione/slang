local a

a = {
		"steve" : "muccione",
		"donna" : "muccione",
		"justin" : "heddden",
		"morganne" : "hedden",
		"sam" : "muccione",
		"aislinn" : "muccione",
		"kiersten" : "muccione",
		"richard" : "muccione"
	}

println ( "steve = ", a["steve"] )			// muccione
println ( "sam = ", a["sam"] )				// muccione
println ( "richard = ", a["richard"] )		// muccione

a["steve"] = "loves donna"
println ( a["steve"] );
