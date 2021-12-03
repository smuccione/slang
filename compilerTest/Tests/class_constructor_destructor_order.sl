//  this is an important test... not only does it ensure that we construct all the inherited classes in the correct order, but it also ensures that we have the vtables and object base offsets all computed correctly
//     it is also a critical inline function test as it ensures that we're properly handling context switches amongst inlined function calls
//
//		constructor order:
//			b1						// b1 && b2 are both virtual... however b1 must come first as b2's bases must be complete before being called
//			b2
//			b4	
//			b5
//			b6
//			b3
//			b7
//			super
//
//
//		destructor order:
//			super
//			b7
//			b3
//			b6
//			b5
//			b4
//			b2
//			b1
//
//
//								b1
//
//
//
//				b2							    b3
//
//
//
//			b4			b5
//
//
//
//				b6								b7
//
//
//
//
//								super
//
//


function main()
{
	local o = new ( "super" )
	omcollect()
	o.release()
	omcollect()
	o = new ( "super" )
	omcollect()
	o.b5.release()
	omcollect()
}


class b1
{
	local id = "b1"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}

class b2
{
	virtual inherit b1

	local id = "b2"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}

class b3
{
	virtual inherit b1

	local id = "b3"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}

class b4
{
	virtual inherit b2

	local id = "b4"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}

class b5
{
	virtual inherit b2

	local id = "b5"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}

class b6
{
	inherit b4
	inherit b5

	local id = "b6"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}


class b7
{
	inherit b3

	local id = "b7"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}

class super
{
	inherit b6
	inherit b7

	local id = "super"
	method release()
	{
		println ( id, " release" )
	}

	method new()
	{
		println ( id, " new" )
	}
}
