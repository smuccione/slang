/*
	WEB Server header file

*/

#pragma once

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

#include "Target\vmTask.h"

#if defined ( OPEN_SSL )

#include <openssl/rsa.h>       /* SSLeay stuff */
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#if defined(OPENSSL_THREADS)
#else
#error MUST HAVE TREAD SUPPORT FOR SSL
#endif

#endif /* OPEN_SSL */

using std::string;
using std::vector;

typedef enum webSocketState {
	wssAcceptSocket,
	wssWriteSocketSocket,
	wssReadSocket,
	wssDisconnectSocket
} webSocketState;

class webSocket {
protected:
	OVERLAPPED				 ov;

	webSocket				*nextFree;

public:
	webSocketState			 state;

	webServerConnection		 conn;

	SOCKET					 socket;
	size_t				 clientPort;
	size_t				 clientIp;
	DWORD					 wsaFlags;

	webBuffs				*activeBuffs;

	class webServerIOCP		*server;

	webSocket()
	{
		state = wssAcceptSocket;
		socket = 0;
	}

	virtual ~webSocket()
	{
		if ( socket )
		{
			closesocket ( socket );
		}
	}

	virtual webBuffs *getBuffs()
	{
		switch ( state )
		{
			case wssAcceptSocket:
			case wssReadSocket:
				return activeBuffs = conn.getRxBuffer();
			case wssWriteSocketSocket:
				return activeBuffs = conn.getTxBuffer();
			case wssDisconnectSocket:
			default:
				throw 500;
		}
	}

	virtual void accept				( void )
	{
		return;
	}

	virtual void resetSocket		( void )
	{
		if ( socket )
		{
			shutdown ( socket, SD_BOTH  );
			closesocket ( socket );
			socket = 0;
		}
	}

	virtual void resetConn			( void )
	{
		conn.req.reset();
	}

	virtual webServerIO	getIoState ( void )
	{
		webServerIO	ioState = conn.getIoState();

		switch ( ioState )
		{
			case needRecv:
				state = wssReadSocket;
				break;
			case needSend:
				state = wssWriteSocketSocket;
				break;
			case needClose:
				state = wssDisconnectSocket;
				break;
			default:
				throw scINTERNAL;
		}
			
		return ioState;
	}

	virtual void rxBuffer ( class vmInstance *instance, size_t len )
	{
		conn.rxBuffer ( instance, len );
	}

	virtual void txBuffer ( class vmInstance *instance, size_t len )
	{
		conn.txBuffer ( len );
	}

	virtual void allocSocket ( HANDLE iocp );
	

	friend void __cdecl webServerWorkerThread ( void *params );
	friend class webServerIOCP;
	friend class webServerIOCPSSL;
};

