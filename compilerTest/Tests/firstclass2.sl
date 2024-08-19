local loop

for ( loop = 1; loop < 10; loop++ )
{
	if (	function ( val ) 
			{
				if ( val % 2 )
				{
					println ( "odd" )
				} else
				{
					println ( "even" )
				}
				return 1
			} ( loop )
		)
	{
	}
}
