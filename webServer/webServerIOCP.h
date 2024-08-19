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
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <rpc.h>
#include <ntdsapi.h>
#include <tchar.h>
#include <process.h>

#include "webServer.h"
#include "webSocket.h"
#include "webProtoSSL.h"
#include "webProtoH1.h"
#include "webProtoH2.h"

#include "Target/vmTask.h"
#include "Target/vmConf.h"

extern	HANDLE	serverIocpHandle;
extern	size_t	serverWorkerThreadCount;

class serverIOCP {
protected:
	HANDLE								 iocpHandle;
	SOCKET								 listenSocket;

	CRITICAL_SECTION					 protoFreeListAccess[serverProtoMax];
	serverProtocol						*protoFreeList[serverProtoMax];
	std::vector<serverProtocol *>		 protoArray[serverProtoMax];

public:
	class webServer						*server;

	volatile size_t						 numConnOutstanding;
	volatile size_t						 numAcceptsOutstanding;
	volatile size_t						 numCons;

	serverIOCP()
	{
		memset ( protoArray, 0, sizeof ( protoArray ) );
	}

	serverSocket						*getConn ( size_t index )
	{
		return dynamic_cast<serverSocket *>(protoArray[serverProtoSocket][index]);
	}

public:
	virtual	void	initConnectionArray ( void )
	{
		for ( uint32_t proto = 0; proto < serverProtoMax; proto++ )
		{
			InitializeCriticalSectionAndSpinCount( &protoFreeListAccess[proto], 4000 );
			protoArray[proto].resize ( server->maxConnections );
			protoFreeList[proto] = nullptr;
			for ( uint32_t conn = 0; conn < server->maxConnections; conn++ )
			{
				serverProtocol *p = 0;
				switch ( proto )
				{
					case serverProtoSocket:
						p = dynamic_cast<serverProtocol *>(new serverSocket);
						break;
					case serverProtoSSL:
						if ( server->isSecure )
						{
							p = dynamic_cast<serverProtocol *>(new webProtocolSSL);
						} else
						{
							p = 0;
						}
						break;
					case serverProtoH1:
						{
							auto o = new webProtocolH1 ( server );
							p = dynamic_cast<serverProtocol *>(o);
						}
						break;
					case serverProtoH2:
						p = dynamic_cast<serverProtocol *>(new webProtocolH2( server ));
						break;
				}

				if ( p )
				{
					p->type = (serverProtocolTypes)proto;
					p->server = this;
					p->nextFree = protoFreeList[proto];
					protoFreeList[proto] = p;
					protoArray[proto][conn] = p;
				}
			}
		}
	}

	void init ( HANDLE iocpHandle, char const *configFile, vmInstance *instance )
	{
		SOCKADDR_STORAGE serverAddr = {0};
		unsigned long			 sockP;

		numConnOutstanding = 0;
		numAcceptsOutstanding = 0;
		numCons = 0;

		this->iocpHandle = iocpHandle;

		try
		{
			server = new webServer ( configFile, iocpHandle );
		} catch ( errorNum )
		{
			printf ( "unable to start server\r\n" );
			return;
		}
		server->instance = dynamic_cast<vmTaskInstance *>(instance);

		// initialize the connections (may be superclassed to sll connections)
		initConnectionArray();

		/* create the socket for communicating with clients */
		listenSocket = WSASocket ( AF_INET6, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

		/* set the socket to dual-stack (IPv4/IPv6) */
		sockP = 0;
		setsockopt ( listenSocket, IPPROTO_IPV6,IPV6_V6ONLY,(char *)&sockP, sizeof(sockP));

		/* set the socket to non-blocking */
		sockP = 1;
		ioctlsocket ( listenSocket, FIONBIO, &sockP );

		serverAddr.ss_family = AF_INET6;
		INETADDR_SETANY((SOCKADDR *)&serverAddr);
		SS_PORT(&serverAddr) = htons( (unsigned short)server->listenPort );
	
		/* bind it to our port */
		if ( bind ( listenSocket, (SOCKADDR *)&serverAddr, (int)INET_SOCKADDR_LENGTH(serverAddr.ss_family) ) == SOCKET_ERROR )
		{
			printf ( "errno %d\r\n", WSAGetLastError ( ) );
			closesocket( listenSocket );
			throw 500;
		}
		if ( listen ( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
		{
			closesocket ( listenSocket );
			throw 500;
		} 

		CreateIoCompletionPort ( (HANDLE)listenSocket, iocpHandle, (ULONG_PTR)this, vmConf.serverConcurrency );

		for ( auto &virtServer : server->virtualServers )
		{
			for ( auto &log : virtServer->logs )
			{
				log.bindCompletionPort ( iocpHandle, (ULONG_PTR)this );
			}
		}

		printf ( "Web Server - %s - starting on port %i using %s\r\n", server->serverName.c_str ( ), server->listenPort, server->configFile.c_str() );

		newAccept();
	}

	virtual ~serverIOCP()
	{
		closesocket ( listenSocket );
		for ( uint32_t proto = 0; proto < serverProtoMax; proto++ )
		{
			DeleteCriticalSection( &protoFreeListAccess[proto] );
			for ( auto &it : protoArray[proto] )
			{
				delete it;
			}
		}
		delete server;
	}

	serverProtocol *allocProto ( serverProtocolTypes proto, serverProtocol *parent )
	{
		serverProtocol *conn;

		EnterCriticalSection ( &protoFreeListAccess[proto] );
		if ( protoFreeList[proto] )
		{
			conn = protoFreeList[proto];
			protoFreeList[proto] = protoFreeList[proto]->nextFree;
			if ( conn->inUse == true )
			{
				printf ( "ERROR: conn structure not free\r\n" );
				conn = protoFreeList[proto];
				protoFreeList[proto] = protoFreeList[proto]->nextFree;
			}
			LeaveCriticalSection ( &protoFreeListAccess[proto] );
		} else
		{
			LeaveCriticalSection ( &protoFreeListAccess[proto] );
			switch ( proto )
			{
				case serverProtoSocket:
					conn = dynamic_cast<serverProtocol *>(new serverSocket);
					break;
				case serverProtoSSL:
					conn = dynamic_cast<serverProtocol *>(new webProtocolSSL);
					break;
				case serverProtoH1:
					conn = dynamic_cast<serverProtocol *>(new webProtocolH1 ( server ));
					break;
				case serverProtoH2:
					conn = dynamic_cast<serverProtocol *>(new webProtocolH2 ( server ));
					break;
				default:
					throw 500;
			}
			conn->type = serverProtoSocket;
			conn->server = this;
		}

		InterlockedIncrementNoFence ( &numConnOutstanding );
		conn->protoParent = parent;
		conn->protoChild = 0;
		conn->inUse = true;
		return conn;
	}

	void freeProto ( serverProtocol *conn )
	{
		EnterCriticalSection ( &protoFreeListAccess[(uint32_t)conn->type] );
		if ( !conn->inUse )
		{
			printf ( "ERROR - Double Freeing protocol structure\r\n" );
			LeaveCriticalSection ( &protoFreeListAccess[(uint32_t) conn->type] );
			return;
		}
		conn->inUse = false;
		if ( conn->protoChild )
		{
			freeProto ( conn->protoChild );
		}
		conn->nextFree = protoFreeList[(uint32_t)conn->type];
		protoFreeList[(uint32_t)conn->type] = conn;
		InterlockedDecrementNoFence ( &numConnOutstanding );
		LeaveCriticalSection ( &protoFreeListAccess[(uint32_t)conn->type] );
	}
public:

	void newAccept ( void )
	{
		serverSocket	*conn;
		WSABUF			*buffs;
		DWORD			 nBytesTransferred;

		while ( numAcceptsOutstanding < server->minAcceptsOutstanding )
		{
			conn = dynamic_cast<serverSocket *>(allocProto( serverProtoSocket, 0 ));

			if ( !conn ) 
			{
				// would exceed our maximum connections - none left
				return;
			}

			conn->state = ssAcceptSocket;
			conn->doTimeoutCheck = false;

			if ( server->isSecure )
			{
				conn->protoChild = allocProto( serverProtoSSL, conn );
			} else
			{
				conn->protoChild = allocProto( serverProtoH1, conn );
			}

			conn->reset();

			buffs = *conn->getBuffs();

			if ( !conn->socket )
			{
				int	 value;
				conn->socket  = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

				value = 1;
				setsockopt ( conn->socket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof ( value ) );			// disable nagle...

				value = 0;
				setsockopt ( conn->socket, IPPROTO_TCP, SO_SNDBUF, (char *)&value, sizeof ( value ) );				// disable send buffers 

				CreateIoCompletionPort ( (HANDLE)conn->socket, iocpHandle, (ULONG_PTR)this, vmConf.serverConcurrency );
			}

			conn->asyncSocket.reset();
			if ( !AcceptEx ( listenSocket, conn->socket, buffs[0].buf, buffs[0].len - ((sizeof( struct sockaddr_in6 ) + 16) * 2), sizeof ( conn->clientAddr ) + 16, sizeof ( conn->localAddr ) + 16, &nBytesTransferred, conn->asyncSocket ) )
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
							freeProto ( conn );
						} else
						{
							// listen socket was shut down... 
							conn->resetSocket ();
							freeProto ( conn );
							return;
						}
						break;
					default:
						// even this accept failed... why???
						conn->resetSocket();
						freeProto ( conn );
						break;
				}
			}
		}
	}

	void disconnect ( serverSocket *sock )
	{
		sock->doTimeoutCheck = false;
		sock->state = ssDisconnectSocket;
		if ( !TransmitFile ( sock->socket, 0, 0, 0, sock->asyncSocket, 0, TF_DISCONNECT | TF_REUSE_SOCKET ) )
		{
			if ( WSAGetLastError() != ERROR_IO_PENDING )
			{
				sock->resetSocket ( );
				PostQueuedCompletionStatus ( iocpHandle, 0, (ULONG_PTR)this, sock->asyncSocket );
			}
		}
	}

	void handleConn ( serverSocket *sock );

	void shutdown()
	{
	}

	friend void __cdecl webServerWorkerThread ( void *params );
};

extern	void			 webServerInit			( class webServerTaskControl *ctrl, char const *configFile, vmInstance *instance );
extern	taskControl		*webServerStart			( size_t numWorkerThreads, vmInstance *instance );
extern	void	__cdecl  webServerCheckTimeout	( void *params );
