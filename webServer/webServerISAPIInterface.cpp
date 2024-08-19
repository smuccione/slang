
#include "webServer.h"
#include "webSocket.h"
#include "webProtoH1.h"
#include "webProtoH2.h"
#include "webProtoSSL.h"

#include "webServerGlobalTables.h"
#include "Target/vmTask.h"
#include "Utility/buffer.h"
#include "Utility/funcky.h"
#include "Utility/itoa.h"

static _inline int CopyBuffer ( char const *src, size_t srcLen, void *dest, size_t &destLen )

{
	if ( srcLen > destLen )
	{
		destLen = srcLen;
		SetLastError ( ERROR_INSUFFICIENT_BUFFER );
		return 1;
	}
	destLen = srcLen;
	memcpy ( (char *)dest, (char *)src, srcLen );
	return ( 1 );
}

static _inline int CopyBufferNull ( char const *src, size_t srcLen, void *dest, size_t &destLen )

{
	if ( srcLen + 1 > destLen )
	{
		destLen = srcLen + 1;
		SetLastError ( ERROR_INSUFFICIENT_BUFFER );
		return 1;
	}
	memcpy ( (char *)dest, (char *)src, srcLen );
	((char*)dest)[srcLen] = 0;
	destLen = srcLen + 1;
	return ( 1 );
}

static _inline int LongToBuffer ( DWORD num, void *dest, size_t &destLen )

{
	if ( destLen < 33 )
	{
		destLen = 33;
		SetLastError ( ERROR_INSUFFICIENT_BUFFER );
		return 1;
	}
	_itoa ( static_cast<int>(num), (char *)dest, 10 );
	return ( 1 );
}

static _inline int LongLongToBuffer ( size_t num, LPVOID dest, size_t &destLen )

{
	if ( destLen < 33 )
	{
		destLen = 33;
		SetLastError ( ERROR_INSUFFICIENT_BUFFER );
		return 1;
	}
	_i64toa ( static_cast<int>(num), (char *)dest, 10 );
	return ( 1 );
}

BOOL WINAPI GetServerVariable ( vmInstance *instance, char const *lpszVariableName, void *lpvBuffer, size_t &sizeofBuffer )

{
	vmTaskInstance		*task;
	webProtocolH1		*conn;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *>();

	auto reqIt = reqMap.find ( lpszVariableName );

	if ( reqIt != reqMap.end() )
	{
		switch ( reqIt->second )
		{
			case req_ALL_HTTP:
			case req_ALL_RAW:
				CopyBufferNull ( (char *)conn->req.headerBuffer, conn->req.headerBufferLen, lpvBuffer, sizeofBuffer );
				break;
			case req_APPL_MD_PATH:
			case req_APPL_PHYSICAL_PATH:
				CopyBufferNull ( (char *)conn->req.virtualServer->rootName.c_str(), (uint32_t)conn->req.virtualServer->rootName.length(), lpvBuffer, sizeofBuffer );
				break;
			case req_AUTH_PASSWORD:
	#if 0
				if ( conn->req *header->pw.pw )
				{
					CopyBufferNull ( header->pw.pw, strlen ( header->pw.pw ), lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
	#endif
				break;
			case req_AUTH_TYPE_ISAPI:
				if ( conn->req.authorization )
				{
					CopyBufferNull ( "Basic", 5, lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
				break;
			case req_AUTH_USER:
	#if 0
				if ( *header->pw.name )
				{
					CopyBufferNull ( header->pw.name, strlen ( header->pw.name ), lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
	#endif
				break;
			case req_URL:
				if ( CopyBufferNull ( conn->req.headers[conn->req.url], conn->req.headers[conn->req.url], lpvBuffer, sizeofBuffer ) )
				{
					_fwebname ( (char *) lpvBuffer, (char *)lpvBuffer );
					if ( checkScript( (char *)lpvBuffer ) )
					{
						strcpy_s ( (char *)lpvBuffer, sizeofBuffer, "" );
					}
				}
				break;
			case req_SCRIPT_NAME:
				CopyBufferNull ( conn->req.fName, *conn->req.fName , lpvBuffer, sizeofBuffer );
				break;
			case req_HTTP_HOST:
				if ( conn->req.host )
				{
					CopyBuffer ( conn->req.headers[conn->req.host], conn->req.headers[conn->req.host], lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
				break;
			case req_REFERER:
				if ( conn->req.referer)
				{
					CopyBuffer ( conn->req.headers[conn->req.referer], conn->req.headers[conn->req.referer], lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
				break;
			case req_USERAGENT:
				if ( conn->req.userAgent )
				{
					CopyBuffer ( conn->req.headers[conn->req.userAgent], conn->req.headers[conn->req.userAgent], lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
				break;
			case req_HTTPS:
				if ( conn->req.server->isSecure  )
				{
					CopyBufferNull ( "on", 2, lpvBuffer, sizeofBuffer );
				} else
				{
					CopyBufferNull ( "off", 3, lpvBuffer, sizeofBuffer );
				}
				break;
			case req_REQUEST_METHOD:
				CopyBufferNull ( (char *)conn->req.method->name.c_str(), (uint32_t)conn->req.method->name.length(), lpvBuffer, sizeofBuffer );
				break;
			case req_CONTENT_LENGTH:
				if ( conn->req.contentLengthValid )
				{
					LongLongToBuffer ( conn->req.contentLength, lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
				break;
			case req_CONTENT_TYPE:
				if ( conn->req.contentType )
				{
					CopyBufferNull ( conn->req.headers[conn->req.contentType], conn->req.headers[conn->req.contentType], lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
				break;
			case req_INSTANCE_ID:
			case req_INSTANCE_META_PATH:
				CopyBufferNull ( (char *)conn->req.virtualServer->name.c_str(), (uint32_t)conn->req.virtualServer->name.length(), lpvBuffer, sizeofBuffer );
				CopyBufferNull ( (char *)conn->req.virtualServer->name.c_str(), (uint32_t)conn->req.virtualServer->name.length(), lpvBuffer, sizeofBuffer );
				break;
			case req_HTTP_ACCEPT:
				if ( conn->req.accept )
				{
					CopyBuffer ( conn->req.headers[conn->req.accept], conn->req.headers[conn->req.accept], lpvBuffer, sizeofBuffer );
				} else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
				break;
			case req_PATH_INFO:
			case req_PATH_TRANSLATED:
				// TODO: should be isapi.dll path
				CopyBufferNull ( conn->req.headers[conn->req.shortUrl], conn->req.headers[conn->req.shortUrl], lpvBuffer, sizeofBuffer );
				break;
			case req_QUERY_STRING:
				if ( conn->req.query )
				{
					CopyBufferNull ( conn->req.headers[conn->req.query], conn->req.headers[conn->req.query], lpvBuffer, sizeofBuffer );
				} else
				{
					CopyBuffer ( "", 1, lpvBuffer, sizeofBuffer );
				}
				break;
			case req_SERVER_NAME:
				CopyBufferNull ( (char *)conn->req.virtualServer->name.c_str(), (uint32_t)conn->req.virtualServer->name.length(), lpvBuffer, sizeofBuffer );
				break;
			case req_SERVER_PORT:
				LongToBuffer ( conn->req.server->listenPort, lpvBuffer, sizeofBuffer );
				break;
			case req_SERVER_PORT_SECURE:
				LongToBuffer ( conn->req.server->isSecure ? 1 : 0, lpvBuffer, sizeofBuffer );
				break;
			case req_SERVER_PROTOCOL:
				char tmpVersion[9];
				sprintf ( tmpVersion, "HTTP/%.3s", (char *)conn->req.version );
				CopyBufferNull ( (char *)tmpVersion, 8, lpvBuffer, sizeofBuffer );
				break;
			case req_SERVER_SOFTWARE:
				CopyBufferNull ( "Slang Server", 12, lpvBuffer, sizeofBuffer );
				break;
			case req_REMOTE_PORT:
				{
					serverSocket *sock = 0;
					for ( auto p = conn->protoParent; p; p = p->protoParent )
					{
						if ( p->type == serverProtoSocket )
						{
							sock = dynamic_cast<serverSocket *>(p);
							break;
						}
					}
					if ( sock )
					{
						uint8_t buff[14];
						auto size = itostr<uint32_t> ( sock->clientAddr.sin6_port, buff );
						CopyBufferNull ( reinterpret_cast<char *>(buff), static_cast<DWORD>(size), lpvBuffer, sizeofBuffer );
					} else
					{
						CopyBuffer ( "", 1, lpvBuffer, sizeofBuffer );
					}
				}
				break;
			case req_REMOTE_ADDR:
				{
					serverSocket *sock = 0;
					for ( auto p = conn->protoParent; p; p = p->protoParent )
					{
						if ( p->type == serverProtoSocket )
						{
							sock = dynamic_cast<serverSocket *>(p);
							break;
						}
					}
					if ( sock )
					{
						uint8_t buff[46] = {};
						DWORD strLen = sizeof ( buff );
						WSAAddressToString ( (LPSOCKADDR) &sock->clientAddr, sizeof ( sock->clientAddr ), NULL, (char *) buff, &strLen );
						CopyBufferNull ( (char *) buff, (DWORD) strlen ( (char *) buff ), lpvBuffer, sizeofBuffer );
					}
				}
				break;
			case req_REMOTE_HOST:
				CopyBufferNull ( conn->req.headers[conn->req.host], conn->req.headers[conn->req.host], lpvBuffer, sizeofBuffer );
				break;
			case req_REMOTE_USER:
	#if 0
				if ( *header->pw.name )
				{
					CopyBufferNull ( header->pw.name, strlen ( header->pw.name ), lpvBuffer, sizeofBuffer );
				} else
				{
					CopyBufferNull ( "", 0, lpvBuffer, sizeofBuffer );
				}
	#endif
				CopyBufferNull ( "", 0, lpvBuffer, sizeofBuffer );
				break;
			case req_HTTP_COOKIE:
				{
					BUFFER	buff;

					bool first;
					first = true;
					for ( auto &cookieIt : conn->req.cookies )
					{
						if ( first )
						{
							first = false;
						} else
						{
							bufferWrite( &buff, "; ", 2 );
						}
						bufferMakeRoom( &buff, cookieIt.getNameLen() );
						cookieIt.getName( bufferBuff( &buff ) );
						bufferAssume( &buff, cookieIt.getNameLen() );
						bufferPut8( &buff, '=' );
						bufferMakeRoom( &buff, cookieIt.getValueLen() );
						cookieIt.getValue( (uint8_t *)bufferBuff( &buff ) );
						bufferAssume( &buff, cookieIt.getValueLen() );
					}
					CopyBufferNull( bufferBuff( &buff ), (DWORD)bufferSize( &buff ), lpvBuffer, sizeofBuffer );
				}
				break;
			case req_HTTPS_SERVER_ISSUER:
			case req_CERT_SERVER_ISSUER:
			case req_CERT_SUBJECT:
			case req_HTTPS_SERVER_SUBJECT:
#if defined ( OPEN_SSL )
				if ( header->serverVirtual->server->isSSL )
				{
					char			 out[512];
					X509_NAME		*x509Name;

					X509 *cert = SSL_get_certificate( header->fd->ssl );

					x509Name = X509_get_issuer_name ( cert );
					X509_NAME_get_text_by_NID( x509Name, NID_commonName, out, sizeof ( out ));

					CopyBufferNull ( out, strlen ( (char *)out ), lpvBuffer, sizeofBuffer );
				} else

				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
	#else
				{
					SetLastError ( ERROR_NO_DATA );
					return ( false );
				}
	#endif
				break;
			case req_CERT_COOKIE:
			case req_CERT_FLAGS:
			case req_CERT_ISSUER:
			case req_CERT_KEYSIZE:
			case req_CERT_SECRETKEYSIZE:
			case req_CERT_SERIALNUMBER:
			case req_CERT_SERVER_SUBJECT:
			case req_HTTPS_KEYSIZE:
			case req_HTTPS_SECRETKEYSIZE:
				SetLastError ( ERROR_NO_DATA );
	//printf ( "(UNHANDLED)\r\n", *sizeofBuffer, lpvBuffer );
	//					printf ( "unhandled = %s\r\n", lpszVariableName );
				return false;
			default:
				return false;
		}
		return true;
	}

	for ( auto &it : conn->req.headers )
	{
		if ( lpszVariableName == it )
		{
			CopyBuffer ( it, it, lpvBuffer, sizeofBuffer );
		}
	}

	// below would make server variables available.. do we want to do this?
#if 0
	size_t	loop;
	for ( loop = 0; loop < conn->req.virtualServer->vars.size(); loop++ )
	{
		if ( !strccmp ( conn->req.virtualServer->vars[loop].name.c_str(), lpszVariableName ) )
		{
			CopyBufferNull ( (*it), (*it), lpvBuffer, sizeofBuffer );
			return ( true );
		}
	}
#endif
	SetLastError ( ERROR_INVALID_INDEX );
	return ( false );
}





