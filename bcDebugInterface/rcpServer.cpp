
// rcpServer.cpp : Defines the entry point for the console application.
//

#include <WinSock2.h>
#include <Windows.h>

#include <stdint.h>
#include <functional>
#include <type_traits>
#include <thread>
#include <unordered_map>

#include "rpcServer.h"

#include "..\common\commonHeader.h"

// this function will be made callable by an outside process - nothing to do
double test( uint32_t p1, float p2 )
{
	printf( "in server function test\\n" );
	return p1 + p2;
}

int main()
{
	// make the functions available to the outside world
	// all it takes is just calling the EXPORT() macro and passing in the function
	EXPORT( test );

	// start up the server
	std::thread server( simpleRpc::rpcServer );

	// detach it, keep it, up to you
	server.detach();

	// do whatever else your program wants to do
	Sleep( 1000000 );

    return 0;
}

