#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <Mswsock.h>
#include <rpc.h>
#include <ntdsapi.h>
#include <tchar.h>
#include <process.h>

#include "webServer.h"
#include "webSocket.h"
#include "webServerIOCP.h"
#include "webServerTaskControl.h"
#include "webServerGlobalTables.h"

#include "Target/vmTask.h"
#include "Target/vmConf.h"
#include "bcVM/bcVMBuiltin.h"
#include "debugAdapter/debugAdapter.h"

HANDLE	serverIocpHandle		= 0;
size_t	serverWorkerThreadCount	= 0;

char const *iocpProtocolTypes[] = {"socket", "SSL", "H1", "H2", "error"};
char const *iocpStateNames[] = { "ssAcceptSocket", "ssSendSocket", "ssRecvSocket", "ssDisconnectSocket", "ssWriteFile", "ssReadFile", "ssCheckTimeout" };

void serverIOCP::handleConn ( serverSocket *sock )
{
	serverIOType			 ioState;
	webBuffs				*buffs;

	sock->checkCount = 0;
	sock->doTimeoutCheck = true;

	ioState = sock->getIoState ( );

#if 1
	static char const *webServerIONamesxx[] =	{	"needNone",
													"needRecv",
													"needSend",
													"needWrite",
													"needRead",
													"needClose",
												};

	printf ( "%8p     socket: %4d   state: %19s                      async: %8p\r\n", sock, (int)(SOCKET) *sock, webServerIONamesxx[ioState], (OVERLAPPED *) sock->asyncSocket );
#endif
	switch ( ioState )
	{
		case needNone:
			break;
		case needRecv:
			buffs = sock->getBuffs ( );
			sock->wsaFlags = 0;
			if ( WSARecv ( *sock, *buffs, *buffs, 0, (DWORD *) &sock->wsaFlags, sock->asyncSocket, 0 ) == SOCKET_ERROR )
			{
				if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
				{
					PostQueuedCompletionStatus ( iocpHandle, -1, (ULONG_PTR)this, sock->asyncSocket );
				}
			}
			break;
		case needSend:
			buffs = sock->getBuffs ( );
			if ( WSASend ( *sock, *buffs, *buffs, 0, 0, sock->asyncSocket, 0 ) == SOCKET_ERROR )
			{
				if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
				{
					PostQueuedCompletionStatus ( iocpHandle, -1, (ULONG_PTR)this, sock->asyncSocket );
				}
			}
			break;
		case needWrite:
			buffs = sock->getBuffs ( );
			sock->wsaFlags = 0;
			sock->setOverlapped ( sock->asyncFile );
			if ( !WriteFile ( (HANDLE) *sock, buffs->nextBuff ( ), buffs->nextLen ( ), 0, sock->asyncFile ) )
			{
				if ( GetLastError ( ) != ERROR_IO_PENDING )
				{
					PostQueuedCompletionStatus ( iocpHandle, -1, (ULONG_PTR)this, sock->asyncSocket );
				}
			}
			break;
		case needRead:
			buffs = sock->getBuffs ( );
			sock->wsaFlags = 0;
			sock->setOverlapped ( sock->asyncFile );
			if ( !ReadFile ( *sock, buffs->nextBuff ( ), buffs->nextLen ( ), 0, sock->asyncFile ) )
			{
				if ( GetLastError ( ) != ERROR_IO_PENDING )
				{
					PostQueuedCompletionStatus ( iocpHandle, -1, (ULONG_PTR)this, sock->asyncSocket );
				}
			}
			break;
		case needClose:
			disconnect ( sock );
			break;
	}
}

size_t	volatile workerThreadRunning = 0;

void __cdecl webServerWorkerThread ( void *params )
{
	BOOL					 result;
	DWORD					 nBytesTransferred;
	serverIOCP				*server = nullptr;
	serverSocket			*sock;
	struct sockaddr_in6		*localAddr{};
	struct sockaddr_in6		*clientAddr{};
	size_t					 addrInLen = 0;
	size_t					 addrOutLen = 0;
	size_t					 optval;
	webBuffs				*buffs;
	char					 name[256]{};
	serverAsync				*asyncOp = nullptr;

	InterlockedIncrement ( &workerThreadRunning );

	sprintf ( name, "WorkerThread - %lu", GetCurrentThreadId() );

	vmTaskInstance instance ( name );

	debugServerInit ( &instance );
	if ( !vmConf.debuggerEnabled )
	{
		instance.debug->Disable ();
	}

	instance.duplicate ( (vmTaskInstance *)params );

	while ( 1 )
	{
		result = GetQueuedCompletionStatus( serverIocpHandle, &nBytesTransferred, (PULONG_PTR)&server, (OVERLAPPED **)&asyncOp, INFINITE );

		if ( result )
		{
			sock = *asyncOp;
			sock->doTimeoutCheck = false;		// ignore timeouts when WE are doing processing

//			printf ( "%8p     socket: %4d   type: %5s    state: %19s    nBytes: %9li\r\n", sock, (uint32_t)sock->socket, sock->protoChild && sock->protoChild->protoChild ? iocpProtocolTypes[sock->protoChild->protoChild->type] : "", iocpStateNames[sock->state], nBytesTransferred );
	
			switch ( sock->state )
			{
				case ssAcceptSocket:
					InterlockedDecrementNoFence ( &server->numAcceptsOutstanding );
					InterlockedIncrementNoFence ( &server->numCons );
					InterlockedIncrementNoFence ( &server->server->totalConnections );

					server->newAccept ( );

					// copy over the socket state from the listening socket
					setsockopt ( sock->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *) &server->listenSocket, sizeof ( server->listenSocket ) );

					optval = 1;
					setsockopt ( sock->socket, SOL_SOCKET, SO_KEEPALIVE, (char *) &optval, sizeof ( optval ) );

					// get the socket addresses
					GetAcceptExSockaddrs ( sock->activeBuffs->buff[0].buf,
											sock->activeBuffs->buff[0].len - (sizeof ( sock->localAddr ) + 16 + sizeof ( sock->clientAddr ) + 16),
											sizeof ( sock->localAddr ) + 16,
											sizeof ( sock->clientAddr ) + 16,
											(LPSOCKADDR *) &localAddr,
											(int *) &addrInLen,
											(LPSOCKADDR *) &clientAddr,
											(int *) &addrOutLen
					);

					sock->localAddr = *localAddr;
					sock->clientAddr = *clientAddr;

					sock->accept ( );

					sock->doTimeoutCheck = true;
					sock->checkCount = 0;

					sock->state = ssRecvSocket;
					if ( nBytesTransferred )
					{
						sock->processBuffer ( &instance, (int32_t)nBytesTransferred );
						server->handleConn ( sock );
					} else
					{
						buffs = sock->getBuffs ( );
						if ( WSARecv ( sock->socket, *buffs, *buffs, (DWORD *) &nBytesTransferred, (DWORD *) &sock->wsaFlags, sock->asyncSocket, 0 ) )
						{
							if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
							{
								PostQueuedCompletionStatus ( serverIocpHandle, 0, (ULONG_PTR) server, sock->asyncSocket );
							}
						}
					}
					break;
				case ssRecvSocket:
					InterlockedAddNoFence64 ( &server->server->totalBytesReceived, nBytesTransferred );
					sock->processBuffer ( &instance, (int32_t)nBytesTransferred );
					server->handleConn ( sock );
					break;
				case ssSendSocket:
					InterlockedAddNoFence64 ( &server->server->totalBytesSent, nBytesTransferred );
					sock->processBuffer ( &instance, (int32_t)nBytesTransferred );
					server->handleConn ( sock );
					break;
				case ssWriteFile:
				case ssReadFile:
					sock->processBuffer ( &instance, (int32_t)nBytesTransferred );
					server->handleConn ( sock );
					break;
				case ssDisconnectSocket:
					sock->reset ( );
					server->freeProto ( sock );
					InterlockedDecrement ( &server->numCons );
					server->newAccept ( );
					break;
				case ssCheckTimeout:
					for ( size_t loop = 0; loop < server->server->maxConnections; loop++ )
					{
						switch ( sock->state )
						{
							case ssAcceptSocket:
							case ssRecvSocket:
							case ssSendSocket:
								if ( server->getConn ( loop )->doTimeoutCheck )
								{
									if ( (++(server->getConn ( loop )->checkCount)) > server->server->keepAliveTimeout )
									{
										// yank the socket out from under it...
										server->getConn ( loop )->resetSocket ( );
									}
								}
								break;
							case ssWriteFile:
							case ssReadFile:
							case ssDisconnectSocket:
							default:
								break;
						}
					}
					break;
				default:
					break;
			}
		} else
		{
			// error handling... something went wrong with a socket operation
			if ( asyncOp )
			{
				sock = *asyncOp;
				if ( !server ) server = sock->server;
				sock->processBuffer ( &instance, -1 );
				server->handleConn ( sock );
			} else
			{
				break;
			}
		}
	}
	globalFileCache.flush ( );
	InterlockedDecrement ( &workerThreadRunning );
	while ( workerThreadRunning )
	{
		Sleep ( 0 );
	}
}

void __cdecl webServerCheckTimeout ( void *params )
{
	serverSocket		 socket;
	serverIOCP	*server = (serverIOCP *)params;

	socket.state = ssCheckTimeout;
	for ( ;; )
	{
		Sleep ( 1000 );
		PostQueuedCompletionStatus ( serverIocpHandle, 0, (ULONG_PTR) server, socket.asyncSocket );
	}
}

taskControl	*webServerStart ( size_t numWorkerThreads, vmInstance *instance )
{
	webServerTaskControl	*ctrl;

	if ( !serverIocpHandle )
	{
		serverIocpHandle = CreateIoCompletionPort ( INVALID_HANDLE_VALUE, NULL, 0, vmConf.serverConcurrency );	// NOLINT (performance-no-int-to-ptr)
	}

	ctrl = new webServerTaskControl ( serverIocpHandle );

	serverWorkerThreadCount	= numWorkerThreads;

	while ( numWorkerThreads-- )
	{
		*ctrl += (HANDLE)_beginthread ( webServerWorkerThread, CPU_STACK_SIZE, dynamic_cast<vmTaskInstance *>( instance ) ); // NOLINT (performance-no-int-to-ptr)
	}

	return ctrl;
}

void webServerInit ( webServerTaskControl *ctrl, char const *configFile, vmInstance *instance )
{
	serverIOCP *server;

	webServerGlobalTablesInit();

	*ctrl += (server = new serverIOCP());
	server->init ( serverIocpHandle, configFile, instance );

#if defined ( NDEBUG )
	_beginthread ( webServerCheckTimeout, CPU_STACK_SIZE, server );
#endif
}

