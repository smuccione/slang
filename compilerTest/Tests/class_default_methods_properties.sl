class common
{
	method print( x )
	{
		println ( 'print ', x )
	}
	
	method default ( name, x = "works", y = "good" )
	{
		printf ( "default method called with %s - %s %s\r\n", name, x, y )
	}
	
	assign default ( name, value )
	{
		printf ( "assignment to %s with value %s\r\n", name, value )
	}
	
	assign val ( value )
	{
		printf ( "val with %s\r\n", value )
	}
	
	access val ()
	{
		printf ( "accessing val as 69\r\n" )
		return 69
	}
	
	access default ( name )
	{
		printf ( "accessing default with %s as 96\r\n", name )
		return 96
	}
}

local x = new ( "common" )

x.val		= 1
x.val2		= 2
x."val"		= 3
x."val2"	= 4

printf ( "x.val = %i\r\n", x.val )
printf ( "x.val2 = %i\r\n", x.val2 )
printf ( "x.\"val\" = %i\r\n", x."val" )
printf ( "x.\"val2\" = %i\r\n", x."val2" )

// iterate through all 4 possibilities, none, one less, equal, and more of both default and non-default method calls
x.print3()
x.print3("test")
x.print3("i love", "donna")
x.print3("i love", "donna", "test")

x.print()
x.print("test")
x.print("i love", "donna")
x.print("i love", "donna", "test")



