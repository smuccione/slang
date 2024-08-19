/*
	WEB Server header file

*/

#pragma once

#include "Utility/settings.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <map>

#include <memory.h>

#include "webServer.h"
#include "webBuffer.h"

#include <winsock2.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Target/vmTask.h"

enum serverSocketState
{
	ssAcceptSocket,
	ssSendSocket,
	ssRecvSocket,
	ssDisconnectSocket,
	ssWriteFile,
	ssReadFile,
	ssCheckTimeout,
};

struct serverAsync
{
	OVERLAPPED				 ov;
	class serverSocket		*serverSocket;

	void reset( void )
	{
		memset( &ov, 0, sizeof( ov ) );
	}
	operator class serverSocket * () { return serverSocket; };
	operator OVERLAPPED *() { return &ov; };
};

enum serverProtocolTypes
{
	serverProtoNone,
	serverProtoSocket,
	serverProtoSSL,
	serverProtoH1,
	serverProtoH2,
	serverProtoError,
	serverProtoMax,		// must be the LAST protocol, array of protocol structures is sized on this
};

// protocol handlers inheret from this interface
class serverProtocol
{
	public:
	bool inUse = false;

	serverProtocolTypes		 type = serverProtoSocket;
	serverProtocol			*protoParent = 0;
	serverProtocol			*protoChild = 0;

//	class serverSocket		*sock;

	serverProtocol			*nextFree = nullptr;		// for free list
	serverSocketState		 state = ssAcceptSocket;

	class serverIOCP		*server = nullptr;

	virtual ~serverProtocol ()
	{};

	virtual operator HANDLE () = 0;
	virtual void setOverlapped( OVERLAPPED *ov ) = 0;

	// returns a pointer to a webBuff's structure that has been filled in with the buffers to send/receive
	virtual webBuffs *getBuffs() = 0;
	virtual void accept( void )
	{
		// override if needed, but many things don't need to do anything here
	}
	virtual void reset( void ) = 0;

	// returns the next operation to perform against this protocol
	virtual serverIOType	getIoOp( void ) = 0;
	// called to process the last operation
	virtual void processBuffer( class vmInstance *instance, int32_t len ) = 0;
};

class serverSocket : public serverProtocol 
{
	private:
	serverAsync				 asyncSocket;
	serverAsync				 asyncFile;

	bool					 doTimeoutCheck = false;;
	size_t					 checkCount = 0;;

	protected:
	SOCKET					 socket = 0;

	public:
	struct sockaddr_in6 	 clientAddr = { 0 };
	struct sockaddr_in6 	 localAddr = { 0 };

	DWORD					 wsaFlags = 0;

	webBuffs				*activeBuffs = nullptr;

	serverSocketState		 state = ssAcceptSocket;

	class serverIOCP		*server = nullptr;

	serverSocket()
	{
		doTimeoutCheck = false;
		asyncSocket.serverSocket = this;
		asyncFile.serverSocket = this;

		asyncSocket.reset();
		asyncFile.reset();
		socket = 0;
	}

	virtual ~serverSocket()
	{
		if ( socket )
		{
			closesocket( socket );
		}
	}

	operator SOCKET () { return socket; }
	operator HANDLE () override { return *protoChild; }

	void setOverlapped( OVERLAPPED *ov ) override
	{
		protoChild->setOverlapped( ov );
	}

	OVERLAPPED *getFileOv( void )
	{
		return &asyncFile.ov;
	}

	webBuffs *getBuffs() override
	{
		activeBuffs = protoChild->getBuffs();
		return activeBuffs;
	}
	void accept( void ) override
	{
		protoChild->accept();
	}

	void resetSocket( void )
	{
		if ( socket )
		{
			shutdown( socket, SD_BOTH );
			closesocket( socket );
			socket = 0;
		}
	}

	void reset( void ) override
	{
		asyncSocket.reset();
		asyncFile.reset();
		if ( protoChild )
		{
			protoChild->reset ();
		}
	}

	serverIOType	getIoState( void )
	{
		serverIOType	ioState = protoChild->getIoOp();

		switch ( ioState )
		{
			case needRecv:
				state = ssRecvSocket;
				break;
			case needSend:
				state = ssSendSocket;
				break;
			case needClose:
				state = ssDisconnectSocket;
				break;
			case needWrite:
				state = ssWriteFile;
				break;
			case needRead:
				state = ssReadFile;
				break;
			default:
				printf ( "ERROR: invalid state in serverProto::getIoState - %i", ioState );
				throw 500;
		}

		return ioState;
	}

	serverIOType	getIoOp( void ) override
	{
		printf ( "ERROR: illeagl call serverProto::getIoOp\r\n" );
		throw errorNum::scINTERNAL;
	}

	void allocSocket( HANDLE iocp )
	{
		int	 value;

		if ( socket ) return;

		socket = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

		value = 1;
		setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof( value ) );

		CreateIoCompletionPort( (HANDLE)socket, iocp, (ULONG_PTR)this, 0 );
	}

	void processBuffer( class vmInstance *instance, int32_t len ) override
	{
		protoChild->processBuffer( instance, len );
	}

	friend void __cdecl webServerWorkerThread( void *params );
	friend void __cdecl webServerCheckTimeout( void *params );
	friend class serverIOCP;
	friend class webServerIOCPSSL;
};

