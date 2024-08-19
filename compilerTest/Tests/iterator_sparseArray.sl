local a = {}

a[1] = "steve"
a[10] = "donna"
a[20] = "justin"
a[25] = "morganne"
a[30] = "sam"
a[35] = "aislinn"
a[50] = "kiersten"
a[100] = "richard"

foreach ( value, index in a )
{
	println ( index, ": ", value )
}

