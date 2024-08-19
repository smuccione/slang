iterator gen ()
{
	yield "hello "
	yield "world "
	yield "this "
	yield "is "
	yield "cool "
	return
	yield "ugg"
}

foreach ( x in gen() )
{
	print  ( x )
}
println();

