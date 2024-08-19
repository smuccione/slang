
// rcpClient.cpp : Defines the entry point for the console application.
//

#include <stdint.h>
#include <string>
#include <functional>
#include <type_traits>

#include "rpcClient.h"

// supply the acutal implentations of our imported functor objects
#undef IMPORT
#define IMPORT(func) rpcClient::rpcClientType<decltype(rpcImport::func)> func;
IMPORT_LIST

void rpcClient::rpcClientInit( void )
{
	WORD				 wVersionRequested;
	WSADATA				 wsaData;

	/* startup winsock */
	wVersionRequested = MAKEWORD( 2, 2 );
	WSAStartup( wVersionRequested, &wsaData );

	struct	sockaddr_in	 addr = { 0 };
	addr.sin_port = htons( 1967 );
	addr.sin_addr.S_un.S_addr = htonl( INADDR_LOOPBACK );
	addr.sin_family = AF_INET;

	serverSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	while ( connect( serverSocket, (sockaddr *)&addr, sizeof( addr ) ) < 0 )
	{
		Sleep( 1 );
	}

	// assign the function name to our object (so we can send it to the server so it knows what function to actualy call)
#undef IMPORT
#define IMPORT(func) func.name = #func;
	IMPORT_LIST
}

