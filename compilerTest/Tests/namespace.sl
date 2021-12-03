namespace test1
{
	function test() 
	{
		printf ( "bad\r\n" );
	}
}
namespace test2
{
	function test() 
	{
		printf ( "good\r\n" );
	}
}

namespace test3
{
	function test() 
	{
		printf ( "bad\r\n" );
	}
}

using test2

test()
