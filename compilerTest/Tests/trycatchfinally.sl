local a
local e
local z

try {
	try {
		// should execute both nested finally blocks
		goto here
	} 
	finally 
	{
		print ( "hello " )
	}
} 
finally 
{
	println ( "world" )
}
label here;

try {
	try {
		// should execute both nested finally blocks
	} 
	finally 
	{
		print ( "hello2 " )
	}
} 
finally 
{
	println ( "world2" )
}

try {
	try {
		// should execute both nested finally blocks
		goto here2
	} 
	finally 
	{
		print ( "hello3 " )
	}
	label here2;
} 
finally 
{
	println ( "world3" )
}

try
{
	printf ( "throwing\r\n" )
	throw ( "test" )
	printf ( "failed\r\n" )
} catch ( e )
{
	printf ( "caught: %s\r\n" , e)
} finally
{
	printf ( "finally\r\n" )
	return
}

printf ( "failed\r\n" ) 

