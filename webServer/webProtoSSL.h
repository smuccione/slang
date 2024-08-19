#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

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

#include <openssl/rsa.h>       /* SSLeay stuff */
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

enum webProtocolSSLState {
	webProtocolSSL_Accept,
	webProtocolSSL_RecvProtDet,
	webProtocolSSL_Recv,
	webProtocolSSL_RecvNeedSend,
	webProtocolSSL_Send,
	webProtocolSSL_SendNeedRecv,
	webProtocolSSL_Read,
	webProtocolSSL_Write,
	webProtocolSSL_Close,
};

class webProtocolSSL: public serverProtocol
{
public:
	serverProtocolTypes	 selectedProtocol = serverProtocolTypes::serverProtoNone;

private:
	webBuffs			 wsaBuff;
	webProtocolSSLState	 sslState = webProtocolSSL_Accept;
	SSL					*ssl = nullptr;
	BIO					*pBioRead = nullptr;
	BIO					*pBioWrite = nullptr;

	serverBuffer		 sslBuff;

	void				 accept		( void ) override;
	serverIOType		 getIoOp	( void ) override;
	webBuffs			*getBuffs	( void ) override;
	void				 reset		( void ) override;

	// handlers for the 4 primary ssl states (write, write_need_send, send, send_need_read )
	void				 Send			( class vmInstance *instance, int32_t len );
	void				 SendNeedRecv	( class vmInstance *instance, int32_t len );
	void				 Recv			( class vmInstance *instance, int32_t len );
	void				 RecvNeedSend	( class vmInstance *instance, int32_t len );

	virtual void		 processBuffer	( class vmInstance *instance, int32_t len ) override;

	void				 process		( class vmInstance *instance );

	// next protocol in the stack
	webBuffs			*activeBuffs = nullptr;				/* proto's active buffers */
public:
	webProtocolSSL();
	void resetSocket( void );
	operator HANDLE ()  override
	{ 
		return *protoChild; 
	}
	void setOverlapped( OVERLAPPED *ov ) override
	{
		protoChild->setOverlapped( ov );
	}

	friend int alpn_select_proto_cb( SSL *ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg );
};

extern	bool	webServerSSLInit			( class webServerTaskControl *ctrl, char const *configFile, class vmInstance *instance );
extern	bool	webServerSLLAddContext		( webVirtualServer	*virtServer, char const *certFile, char const *keyFile, char const *chainFile, char const *cipherList, char const *cipherSuite, char const *password );
