iterator gen ()
{
	local x;

	for ( x =  0; x < 10; x++ )
	{
		yield x * 2, x;
	}
}

foreach ( value, name in gen() )
{
	println ( "name=", name, "  value=", value );
}
println();

foreach ( value in gen() )
{
	println ( "value=", value );
}
println();
