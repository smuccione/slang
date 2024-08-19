local a

a = {
		hello	:	function ( x )
					{
						return println ( "hello " + x ); 
					},
		how		:	function ( x ) 
					{
						return println ( "how " + x ); 
					}
	}


a.hello ( "there" )
println ( a.how ( "are you" ) )

