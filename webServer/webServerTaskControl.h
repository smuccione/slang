#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <memory>
#include <vector>
#include <string>

#include <winsock2.h>
#include <windows.h>
#include <Mswsock.h>
#include <mstcpip.h>
#include <ws2tcpip.h>
#include <rpc.h>
#include <ntdsapi.h>
#include <tchar.h>
#include <process.h>

#include "webServer.h"
#include "webSocket.h"
#include "webServer.h"

#include "Target/vmTask.h"

class webServerTaskControl	: public taskControl
{
	HANDLE		 handles[128];
	uint32_t	 maxHandles;

public:
	HANDLE			 shutDown;

	std::vector<serverIOCP *>	servers;

	webServerTaskControl ( HANDLE shutDown )
	{
		maxHandles = 0;
		this->shutDown = shutDown;
	}

	void end ( void )
	{
		CloseHandle ( shutDown );
		WaitForMultipleObjects ( maxHandles, handles, true, INFINITE );

		for ( auto &it : servers )
		{
			delete it;
		}
		servers.clear ( );
	}

	void operator += ( HANDLE handle )
	{
		if ( maxHandles < sizeof ( handles ) / sizeof ( handles[0] ) )
		{
			handles[maxHandles++] = handle;
		} else
		{
			printf ( "error: to many worker threads" );
			throw errorNum::scINTERNAL;
		}
	}

	void operator += ( serverIOCP *server )
	{
		servers.push_back ( server );
	}
};
