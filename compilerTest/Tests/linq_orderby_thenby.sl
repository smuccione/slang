local a = 	{ 
				{ first: "stephen", last : "muccione" },
				{ first: "megan", last : "hedden" },
				{ first: "donna", last : "muccione" },
				{ first: "steve", last : "repetti" },
				{ first: "sam", last : "muccione" },
				{ first: "kiersten", last : "muccione" },
				{ first: "aislinn", last : "muccione" },
				{ first: "richard", last : "muccione" },
				{ first: "angie", last : "repetti" },
				{ first: "justin", last : "hedden" },
				{ first: "morganne", last : "hedden" },
				{ first: "grayson", last : "hedden" },
				{ first: "finley", last : "hedden" },
			}
	
local y = from x in a
	where x.last == "muccione"
	orderby x.last, x.first
	select x;

foreach ( o in y )
{
	println ( o.first, " ", o.last )
}




