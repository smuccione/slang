local a

local x = "how"

a = {
		hello	:	function ( x ) 
					{
						return println ( "hello " + x ); 
					},
		(x)		:	function ( x ) 
					{
						return println ( "how " + x ); 
					}
	}


a.hello ( "there" )
a.how ( "are you" )

