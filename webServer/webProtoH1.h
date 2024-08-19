/*
	WEB Server header file

*/

#pragma once

#include "Utility/settings.h"
#include <vector>
#include <string>
#include <map>
#include <memory>

#include <memory.h>

#include "webServer.h"
#include "webBuffer.h"

#include <winsock2.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Target/vmTask.h"
#include "webSocket.h"

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

struct webServerRange
{
	size_t	rangeLow;
	size_t	rangeHigh;
	webServerRange( size_t rangeLow, size_t rangeHigh ) : rangeLow( rangeLow ), rangeHigh( rangeHigh )
	{
	}

};

class webServerRequest
{
	public:
	class webProtocolH1						*conn;

	webServerPageState						 state;
	bool									 needMoreInput;

	LARGE_INTEGER							 reqStartTime;

	char									*fName;						// valid only for active pages during execution (points to a stack buffer)

	size_t									 bytesRcvd;

	uint8_t									*headerBuffer;
	size_t									 headerBufferSize;			// total allocated size of header buffer
	uint8_t									*headerBufferPointer;
	size_t									 headerBufferLen;			// amount in use for this request

	bool									 keepAlive;

	size_t									 compress;					// bit field of webServerCompression values
	webServerTransferEncodings				 encode;

	webServerMethods						*method;					// post, get, head, etc.
	webServerHeader							 methodHeader;

	webMimeType								*mimeType;

	uint8_t									 version[3];

	// chunked transfer stuff
	bool									 chunked;
	webChunkedRxBuff						*chunkBuff;

	// contentLength stuff
	bool									 contentLengthValid;
	size_t									 contentLength;
	size_t									 contentRcvd;

	// transfer encoding stuff
	size_t									 multiPartBoundary;			// also servers as a bool if multipart rx is enabled
	webServerMultipartState					 multiPartState;
	size_t									 multiPartStart;
	size_t									 multiPartScanOffset;

	// file streaming stuff
	HANDLE									 streamHandle;
	size_t									 streamStartOffset;

	// quick access headers to allow future fast access
	size_t									 firstLine;
	size_t									 shortUrl;
	size_t									 url;
	size_t									 host;
	size_t									 from;
	size_t									 accept;
	size_t									 ifModifiedSince;
	size_t									 ifMatch;
	size_t									 ifNoneMatch;
	size_t									 ifRange;
	size_t									 ifUnmodifiedSince;
	size_t									 origin;
	size_t									 authorization;
	size_t									 referer;
	size_t									 contentType;
	size_t									 expect;
	size_t									 range;
	size_t									 te;
	size_t									 query;
	size_t									 userAgent;
	size_t									 xForwardFor;

	class webServer							*server;
	class webVirtualServer					*virtualServer;			// server that owns this request

	std::vector<webServerHeader>			 headers;
	std::vector<webServerRequestCookie>		 cookies;
	std::vector<webServerMultipart>			 multiParts;
	std::vector<webServerRange>				 ranges;

	serverLargeBuffer						 rxBuffer;

	webBuffs								 rxBufferList;

	size_t									 parseHeaders( uint8_t **basePtr, uint8_t *ptr, uint8_t *buffEnd );
	size_t									 parseHeaders( std::vector<webServerHeader> *headers, uint8_t **basePtr, uint8_t *ptr, uint8_t *buffEnd );
	public:
	webServerRequest( class webProtocolH1 *conn, size_t headerPages );
	~webServerRequest ();

	webServerPageState parseRequest( webServerPageState state );

	void clear( void );

	void reset( void )
	{
		clear();
		headerBufferPointer = 0;
		headerBufferLen = 0;
		state = wsParseURL;
	}

	webBuffs	*getBuffer( void );

	webServerPageState processBuffer( size_t len );

	serverIOType	getIoOp( void )
	{
		switch ( state )
		{
			case wsParseURL:
			case wsParseHeaders:
				return needRecv;
			case wsReceiveBody:
				return needRecv;
			case wsProcess:		// processing may generate output (chunked, etc.) which may need tx prior to exiting process state... will never read however
				return needSend;
			case wsSendResponse:
				return needSend;
			default:
				printf ( "ERROR: invalid request state in getIoOp - %i", state );
				throw 500;
		}
	}
};

enum webServerResponseState
{
	webServerResponseStart,
	webServerResponseMemory,
	webserverResponseMemoryRangeSingle,
	webServerResponseMemoryRangeHeaders,
	webServerResponseMemoryRangeMimeHeaders,
	webServerResponseMemoryRangeBody,
	webServerResponseMemoryRangeEnd,
	webServerResponseChunkedHeaders,
	webServerResponseChunkedFirstLength,
	webServerResponseChunkedLength,
	webServerResponseChunkedBody,
};


enum webCachePolicyType
{
	webCachePolicy_Public = 1,
	webCachePolicy_Private,
	webCachePolicy_NoCache,
	webCachePolicy_User,
	webCachePolicy_None					// no cache policy specified
};

enum webCacheExpiresType
{
	webCacheExpiresType_Fixed,			// uses the expires[] gmt string
	webCacheExpiresType_At,				// computed to expire at a specific time (say 2 am) the next time this would happen
	webCacheExpiresType_Every,			// does not emit an expires time, but instead uses the maxAge field of the cache control header
};

struct webCachePolicy
{
	webCachePolicyType			policyType;
	webCacheExpiresType			expiresType;
	union
	{
		size_t					maxAge;
		char					expires[30];				// size of _webGmt() result
		char					user[256];
		size_t					expiresEvery;				// expires every 
	};
};

struct webExtraHeaders
{
	stringi headerLine;

	webExtraHeaders()
	{
		headerLine.reserve ( 256 );
	}
	webExtraHeaders( char const *header, char const *value )
	{
		headerLine = header;
		headerLine += ": ";
		headerLine += value;
	}
	~webExtraHeaders()
	{
	}
	webExtraHeaders( webExtraHeaders &&old )
	{
		std::swap ( headerLine, old.headerLine );
	}
	webExtraHeaders( const webExtraHeaders &old )
	{
		headerLine = old.headerLine;
	}
};

struct webSetCookies
{
	char		*cookie;

	webSetCookies()
	{
		cookie = 0;
	}
	webSetCookies( char const *name, char const *value, char const *expires, char const *path, bool isHttp, bool isSecure )
	{
		size_t	 nameLen = 0;
		size_t	 valueLen = 0;
		size_t	 expiresLen = 0;
		size_t	 pathLen = 0;
		size_t	 httpLen = 0;
		size_t	 secureLen = 0;
		char	*tmpPtr;

		if ( !value ) throw 500;
		if ( !name ) throw 500;

		valueLen = strlen( value );
		nameLen = strlen( name ) + 1;
		if ( expires ) expiresLen = strlen( expires ) + 10;
		if ( path ) pathLen += strlen( path ) + 7;
		if ( isHttp ) httpLen += 10;
		if ( isSecure ) secureLen = 8;

		cookie = (char *)malloc( nameLen + valueLen + expiresLen + pathLen + httpLen + secureLen + 1 );
		tmpPtr = cookie;

		memcpy( tmpPtr, name, nameLen );
		tmpPtr += nameLen;
		tmpPtr[0] = '=';
		tmpPtr++;
		if ( value )
		{
			memcpy( tmpPtr, value, valueLen );
			tmpPtr += valueLen;
		}

		if ( expires )
		{
			memcpy( tmpPtr, "; expires=", 10 );
			memcpy( tmpPtr + 10, expires, expiresLen - 10 );
			tmpPtr += expiresLen;
		}

		if ( path )
		{
			memcpy( tmpPtr, "; path=", 7 );
			memcpy( tmpPtr + 7, path, pathLen - 7 );
			tmpPtr += pathLen;
		}

		if ( isHttp )
		{
			memcpy( tmpPtr, "; httponly", 10 );
			tmpPtr += 10;
		}

		if ( isSecure )
		{
			memcpy( tmpPtr, "; secure", 8 );
			tmpPtr += 9;
		}
		tmpPtr[0] = 0;
	}
	~webSetCookies()
	{
		if ( cookie ) free( cookie );
	}
};

class webServerResponse
{
	public:
	webCachePolicy							 cachePolicy;

	uint32_t								 rspCode;
	bool									 isCompressed;

	serverBuffer							 headers;

	webBuffs								 txBuffList;

	LARGE_INTEGER							 reqEndTime;
	LARGE_INTEGER							 loadStartTime;
	LARGE_INTEGER							 loadEndTime;
	LARGE_INTEGER							 runStartTime;
	LARGE_INTEGER							 runEndTime;
	bool									 activePage;
	size_t									 bytesSent;

	time_t									 modifiedTime;		// either render or modified time depending on body time... we dont' care so long as it's set properly, this is returned in the http date header
	bool									 keepAlive;			// does response desire a keep alive (is anded with request to determine if we are keep-alive or not)...

	webMimeType								*mimeType;

	size_t									 multiPartBoundaryLen;
	char									 multiPartBoundary[256];

	bool									 extraDisposition;
	std::vector<webExtraHeaders>			 extraHeaders;
	std::vector<webSetCookies>				 cookies;

	webServerResponseState					 state;

	webProtocolH1							*conn;

	std::vector<webServerRange>::iterator	 rangeIt;

	virtual ~webServerResponse()
	{
	}

	webServerResponse( class webProtocolH1 *conn ) : conn( conn )
	{
		state = webServerResponseStart;
		bytesSent = 0;
		txBuffList.reset();
	}

	void reset( void )
	{
		activePage = 0;
		rspCode = 200;
		state = webServerResponseStart;
		bytesSent = 0;
		extraDisposition = false;
		extraHeaders.clear ( );
		cookies.clear ( );
		txBuffList.reset();
	}

	class body
	{
		public:
		
		class chunked {
			public:
			virtual ~chunked ( ) {};
			virtual operator size_t ( ) = 0;
			virtual operator uint8_t *() = 0;
			virtual void reset ( ) = 0;
			virtual void processBuffer ( size_t len ) = 0;
		};

		std::unique_ptr<chunked>	 chunker;
		serverBuffer				 buff;
		struct fileCacheEntry		*entry;
		size_t						 cLen;
		uint8_t						*cPtr;
		void						*cFree;
		class objectMemoryPage		*cMemoryPage;
		class vmInstance			*cInstance;

		uint8_t						 chunkedLength[16];	// 999 terrabytes + null  do we need bigger?   doubt it for the next decade or so 

		enum
		{
			btNone,
			btFileCacheEntry,
			btServerBuffer,
			btPointer,
			btStaticPointer,
			btObjectPage,
			btObjectPageComplete,
			btChunked,
		}							 bodyType;

		operator uint8_t *()
		{
			switch ( bodyType )
			{
				case btFileCacheEntry:
					if ( entry->isDataCached() )
					{
						return entry->data;
					} else
					{
						return 0;
					}
				case btServerBuffer:
					return buff.getBuffer();
				case btPointer:
				case btStaticPointer:
				case btObjectPage:
				case btObjectPageComplete:
					return cPtr;
				case btChunked:
					return (uint8_t *) *chunker;
				default:
					return (uint8_t *)"bad";
			}
		}
		operator size_t ()
		{
			switch ( bodyType )
			{
				case btFileCacheEntry:
					return (size_t)entry->getDataLen();
				case btServerBuffer:
					return buff.getSize();
				case btPointer:
				case btStaticPointer:
				case btObjectPage:
				case btObjectPageComplete:
					return cLen;
				case btChunked:
					return (size_t)*chunker;
				default:
					return 0;
			}
		}
		void release( void );
	}									body;

	void buildRspHeaders( class webProtocolH1 *conn );
	void buildCookies( class webProtocolH1 *conn );

	serverIOType	getIoOp( void );
	void setOverlapped( OVERLAPPED *ov )
	{
		ov->Offset = 0xFFFFFFFF;
		ov->OffsetHigh = 0xFFFFFFFF;
	}
	webBuffs			 *getBuffer( void );
	webBuffs			 *getBufferLog( void );
	webServerPageState	  processBuffer( size_t len );
};

class webServerProcess
{

	public:
	char		fileNameBase[MAX_PATH];
	size_t		fileNameBaseLen;

	size_t		serviceTime;

	public:
	serverIOType getIoOp( void )
	{
		return needNone;
	}

	webBuffs *getBuffer()
	{
		throw 500;
	}

	webServerPageState	process( class vmInstance *instance, class webProtocolH1 *conn );
};

enum webServerConnState
{
	connStateParseRequest,
	connStateExecuteRequest,
	connStateSendResponse,
	connStateLogWrite,
	connStateLogWriteReset,
	connStateReset,
};

class webProtocolH1 : public serverProtocol
{
	public:
	webServerConnState		 state;

	webServerRequest		 req;
	webServerProcess		 proc;
	webServerResponse		 rsp;

	uint32_t				 logIndex;					// index of last written log

	LONGLONG				 lastActivityTime;

	class webServer			*server;

	public:
#pragma warning (disable:4355)
	webProtocolH1 ( class webServer *server ) : req( this, server->numHeaderPages ), rsp( this )		// this is a bit dangerous... this isn't yet valid and we must not use it in the req constructor (which we don't)
	{
		this->server = server;
		req.server = server;
		webProtocolH1::reset ( );
	}

	void reset()
	{
		logIndex = 0;
		req.reset();
		rsp.reset();
		state = connStateParseRequest;
	}

	serverIOType	getIoOp( void )
	{
		switch ( state )
		{
			case connStateParseRequest:
				return req.getIoOp();
			case connStateExecuteRequest:
				return proc.getIoOp();
			case connStateSendResponse:
				return rsp.getIoOp();
			case connStateReset:
				return needClose;
			case connStateLogWrite:
			case connStateLogWriteReset:
				if ( logIndex )
				{
					return needWrite;
				} else
				{
					return needNone;
				}
			default:
				printf ( "ERROR: invalid state in webProtoH1::getIoOp - %i", state );
				throw 500;
		}
	}

	webBuffs *getBuffs ( void );
	void processBuffer( class vmInstance *instance, int32_t len );

	void process( class vmInstance *instance )
	{
		switch ( state )
		{
			case connStateExecuteRequest:
				proc.process( instance, this );
			default:
				printf ( "ERROR: invalid state in webProtoH1::process - %i", state );
				throw 500;
		}
	}

	void setOverlapped( OVERLAPPED *ov )
	{
		switch ( state )
		{
			// both of these are simply append operations (large file or log write)
			case connStateParseRequest:
			case connStateLogWrite:
			case connStateLogWriteReset:
				ov->Offset = 0xFFFFFFFF;
				ov->OffsetHigh = 0xFFFFFFFF;
				break;
			case connStateSendResponse:
				rsp.setOverlapped( ov );
				break;
			default:
				printf ( "ERROR: invalid state in webProtoH1::setOverlapped - %i", state );
				throw 500;
		}
	}

	operator HANDLE ();
};
