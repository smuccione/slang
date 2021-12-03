
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

namespace simpleRpc
{
	__declspec(align(64)) uint8_t	paramBuff[4096];

	std::unordered_map<std::string, rpcDispatcher *>	rpcFuncMap;

	static void rcpServerMainLoop ( SOCKET connSocket)
	{

		while (1)
		{
			char		funcName[256];
			uint32_t	len;
			uint32_t	pLen;

			// --------------------------------------- read in the function name
			len = recv (connSocket, (char *)&len, sizeof (len), 0);
			if (len < 0)
			{
				// error on the socket or closed, don't care which, just close it and wait for a new connection
				closesocket (connSocket);
				break;
			}

			if (len > sizeof (funcName) - 1)
			{
				// protocol error, function name is bigger than we allow
				closesocket (connSocket);
				break;
			}

			len = recv (connSocket, funcName, len, 0);
			if (len < 0)
			{
				closesocket (connSocket);
				break;
			}

			funcName[len] = 0;

			// -------------------------------------- read in the parameters
			len = recv (connSocket, (char *)&pLen, sizeof (len), 0);
			if (len < 0)
			{
				// error on the socket or closed, don't care which, just close it and wait for a new connection
				closesocket (connSocket);
				break;
			}

			if (pLen > sizeof (paramBuff))
			{
				// protocol error, function name is bigger than we allow
				closesocket (connSocket);
				break;
			}

			len = recv (connSocket, (char *)paramBuff, pLen, 0);
			if (len < 0)
			{
				closesocket (connSocket);
				break;
			}


			auto it = rpcFuncMap.find (std::string (funcName));
			if (it != rpcFuncMap.end ())
			{
				// and here is where the magic happens... just call the function dispatcher, the generated functor knows how to build a function call
				// that takes it's parameters from the param buffer.
				len = (*it->second)();	// dispatch the function
			}
			else
			{
				// something went wrong, we don't have this function available.
				closesocket (connSocket);
				break;
			}

			if (len)
			{
				// send any response data if we have some (e.g. not a void return type)

				// length first
				send (connSocket, (char *)&len, sizeof (uint32_t), 0);
				// now the data
				send (connSocket, (char *)paramBuff, len, 0);
			}
		}

	}

	void rpcServer( void )
	{
		WORD				 wVersionRequested;
		WSADATA				 wsaData;

		/* startup winsock */
		wVersionRequested = MAKEWORD( 2, 2 );
		WSAStartup( wVersionRequested, &wsaData );

		SOCKET	listenSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

		struct	sockaddr_in	 addr = { 0 };
		addr.sin_port = htons( 1967 );
		addr.sin_addr.S_un.S_addr = htonl( INADDR_LOOPBACK );
		addr.sin_family = AF_INET;

		bind( listenSocket, (sockaddr *)&addr, sizeof( SOCKADDR_IN ) );

		listen( listenSocket, 1 );

		while ( 1 )
		{
			SOCKET connSocket = accept( listenSocket, 0, 0 );


			int value = 1;
			setsockopt( connSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof( value ) );

			// start up the server
			std::thread server (rcpServerMainLoop, connSocket );
		}
	}
};
