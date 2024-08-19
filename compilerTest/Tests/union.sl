#if 0

union id {
	person:
		local name
		local person
	town:
		local city
		local state
}

id.setTag ( person )
<tagId> = id.getTag()

id.person.name = "test"
id.town.city = "test"		// error

#endif

println ( "todo" )
