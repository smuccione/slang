#pragma once

#include <stdlib.h>
#include <stdint.h>
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

#include "Target\vmTask.h"

HANDLE		serverIocpHandle;
uint32_t	serverWorkerThreadCount;

class webServer {
protected:
	SOCKET								 listenSocket;

	CRITICAL_SECTION					 freeListAccess;
	webSocket							*connectionFreeList;
	webSocket							*connArray;

	HANDLE								 iocpHandle;

public:
	webServer							*server;

	uint32_t							 numConnOutstanding;
	uint32_t							 numAcceptsOutstanding;
	uint32_t							 numCons;

public:
	virtual	void	initConnectionArray()
	{
		uint32_t	loop;

		connArray = new webSocket[server->maxConnections]();

		// build the free list of connections
		connectionFreeList = 0;
		for ( loop = 0; loop < server->maxConnections; loop++ )
		{
			connArray[loop].server = this;
			connArray[loop].conn.req.server = server;
			connArray[loop].conn.server = server;
			connArray[loop].nextFree = connectionFreeList;
			connectionFreeList = &connArray[loop];
		}
	}

	webServer ( HANDLE iocpHandle, char *configFile, vmInstance *instance )
	{
		struct	sockaddr_in		 addr;
		unsigned long			 sockP;

		InitializeCriticalSectionAndSpinCount ( &freeListAccess, 40000 );

		numConnOutstanding = 0;
		numAcceptsOutstanding = 0;
		numCons = 0;

		this->iocpHandle = iocpHandle;

		server = new webServer ( configFile );
		server->instance = dynamic_cast<vmTaskInstance *>(instance);

		// initialize the connections (may be superclassed to sll connections)
		initConnectionArray();

		/* create the socket for communicating with clients */
		listenSocket = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

		/* set the socket to non-blocking */
		sockP = 1;
		ioctlsocket ( listenSocket, FIONBIO, &sockP );

		// must be called before the bind
		//WSASetSocketSecurity 

		memset ( &addr, 0, sizeof ( addr ) );
		addr.sin_port				= htons( (unsigned short)server->listenPort );
		addr.sin_addr.S_un.S_addr	= INADDR_ANY;
		addr.sin_family				= AF_INET;

		/* bind it to our port */
		if ( bind ( listenSocket, (SOCKADDR *)&addr, sizeof ( addr ) ) == SOCKET_ERROR )
		{
			closesocket( listenSocket );
			throw 500;
		}
		if ( listen ( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
		{
			closesocket ( listenSocket );
			throw 500;
		} 

		CreateIoCompletionPort ( (HANDLE)listenSocket, iocpHandle, (ULONG_PTR)this, 0 );

		accept();
	}

	virtual ~webServer()
	{
		std::vector<webSocket>::iterator	 it;

		closesocket ( listenSocket );
		DeleteCriticalSection ( &freeListAccess );

		delete []connArray;
		delete server;
	}

	webSocket				*allocConn ( void )
	{
		webSocket *conn;

		EnterCriticalSection ( &freeListAccess );
		if ( connectionFreeList )
		{
			conn = connectionFreeList;
			connectionFreeList = connectionFreeList->nextFree;
			numConnOutstanding++;
		} else
		{
			conn = 0;
		}
		LeaveCriticalSection ( &freeListAccess );

		return conn;
	}

	void					 freeConn ( webSocket *conn )
	{
		EnterCriticalSection ( &freeListAccess );
		conn->nextFree = connectionFreeList;
		connectionFreeList = conn;
		numConnOutstanding--;
		LeaveCriticalSection ( &freeListAccess );
	}
public:

	void					 newAccept ( void )
	{
		webSocket	*conn;
		WSABUF		*buffs;
		DWORD		 nBytesTransferred;

		while ( numAcceptsOutstanding < server->minAcceptsOutstanding )
		{
			conn = allocConn();

			if ( !conn ) 
			{
				// would exceed our maximum connections - none left
				return;
			}

			conn->state = wssAccept;
			conn->conn.state = connStateParseRequest;
			conn->conn.req.reset();
			buffs = *conn->getBuffs();

			if ( !conn->socket )
			{
				int	 value;
				conn->socket  = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

				value = 1;
				setsockopt ( conn->socket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof ( value ) );			// disable nagle... 

				CreateIoCompletionPort ( (HANDLE)conn->socket, iocpHandle, (ULONG_PTR)this, 0 );
			}

			memset ( &conn->ov, 0, sizeof ( conn->ov ) );
			if ( !AcceptEx ( listenSocket, conn->socket, buffs[0].buf, buffs[0].len, sizeof ( struct sockaddr_in ) + 16, sizeof ( struct sockaddr_in) + 16, &nBytesTransferred, (LPOVERLAPPED)conn ) )
			{
				switch ( int r = WSAGetLastError() )
				{
					case ERROR_IO_PENDING:
						InterlockedIncrement ( &numAcceptsOutstanding );		// got one
						// we'll get the result in a future iocp call
						break;
					case WSAENOTSOCK:
						if ( listenSocket )
						{
							// can happen if the socket isn't reset correctly due to an error.
							conn->resetSocket();
						} else
						{
							// listen socket was shut down... 
							freeConn ( conn );
							return;
						}
						break;
					default:
						// even this accept failed... why???
						conn->resetSocket();
						freeConn ( conn );
						break;
				}
			}
		}
	}

	void disconnect ( webSocket *sock )
	{
		sock->resetConn();
		if ( !TransmitFile ( sock->socket, 0, 0, 0, (OVERLAPPED *)sock, 0, TF_DISCONNECT | TF_REUSE_SOCKET ) )
		{
			if ( WSAGetLastError() != ERROR_IO_PENDING )
			{
				sock->resetSocket();
			}
		}
		sock->state = wssDisconnect;
	}

	void reset ( webSocket *sock )
	{
		sock->resetSocket();
		sock->state = wssDisconnect;
		PostQueuedCompletionStatus ( iocpHandle, 0, (ULONG_PTR)this, (OVERLAPPED *)sock );
	}

	void	handleConn ( webSocket *sock )
	{
		webServerIO				 ioState;
		webWSABuffs				*buffs;
		DWORD					 nBytesTransferred;

		ioState = sock->conn.getIoState();
		do {
			switch ( ioState )
			{
				case needNone:
					break;
				case needRecv:
					sock->state = wssRead;
					buffs = sock->getBuffs();
					if ( WSARecv ( sock->socket, *buffs, buffs->nBuffs, (DWORD *)&nBytesTransferred, (DWORD *)&sock->wsaFlags, (OVERLAPPED *)sock, 0 ) == SOCKET_ERROR )
					{
						if ( WSAGetLastError() != ERROR_IO_PENDING )
						{
							reset ( sock );
						}
					}
					break;
				case needSend:
					sock->state = wssWrite;
 					buffs = sock->getBuffs();
					if ( WSASend ( sock->socket, *buffs, buffs->nBuffs, (DWORD *)&nBytesTransferred, 0, (OVERLAPPED *)sock, 0 ) == SOCKET_ERROR )
					{
						if ( WSAGetLastError() != ERROR_IO_PENDING )
						{
							reset ( sock );
						}
					}
					break;
				case needClose:
					disconnect ( sock );
					break;
			}
			ioState = sock->conn.getIoState();
		} while ( ioState  == needNone );
	}

	void shutdown()
	{
	}

	friend void __cdecl webServerWorkerThread ( void *params );
};

extern taskControl	*webServerStart ( char *configFile );

extern taskControl		*webServerStart ( uint32_t numWorkerThreads, vmInstance *instance );
extern void				 webServerInit	( class webServerTaskControl *ctrl, char *configFile, class vmInstance *instance );
