/*
HTTP Client functions

*/

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <winsock2.h>
#include <Windows.h>
#include <windns.h>
#include <winhttp.h>
#include <WinDns.h>
#include <IpExport.h>

#include <string.h>
#include <set>
#include <codecvt>

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"

class hCloser {
	HINTERNET *handle;
	public:
	hCloser ( HINTERNET *handle ) : handle ( handle )
	{
	}
	hCloser ( ) : handle ( 0 )
	{
	}
	~hCloser ( )
	{
		if ( *handle ) WinHttpCloseHandle ( *handle );
		*handle = 0;
	}
	hCloser &operator = ( HINTERNET *h )
	{
		handle = h;
		return *this;
	}
	operator HINTERNET ( )
	{
		return *handle;
	}
};

VAR webBuildResultArray ( vmInstance *instance, BUFFER &headerBuff, BUFFER &dataBuff )
{
	int64_t			 loop;
	char			*tmpPtr;
	char			*end;
	char			*start;
	char			*buffer;

	/* may not have an entity body */
	end = bufferBuff ( &headerBuff ) + bufferSize ( &headerBuff );
	start = bufferBuff ( &headerBuff );

	VAR	resArr = VAR_ARRAY ( instance );

	buffer = start;

	loop = 1;

	// header's
	while ( *buffer && (buffer < end) )
	{
		tmpPtr = buffer;
		while ( tmpPtr < end )
		{
			if ( tmpPtr[0] == '\r' && tmpPtr[1] == '\n' )
			{
				break;
			}
			tmpPtr++;
		}
		if ( tmpPtr == end )
		{
			throw errorNum::scINVALID_PARAMETER;
		}

		arrayGet ( instance, &resArr, loop ) = VAR_STR ( instance, buffer, tmpPtr - buffer );

		loop++;
		buffer = tmpPtr + 2;
	}

	arrayGet ( instance, &resArr, loop ) = VAR_STR ( instance, bufferBuff ( &dataBuff ), bufferSize ( &dataBuff ) );

	return resArr;
}

static VAR webSend ( vmInstance *instance, VAR_STR const &method, VAR_STR const &domain, VAR_STR const &url, int64_t port, VAR_STR const &headers, VAR_STR const &postData, bool isSSL, VAR_STR const &userId, VAR_STR const &password, DWORD authScheme )
{
	BUFFER			 headerBuffer;
	BUFFER			 dataBuffer;
	DWORD			 dwSize;
	DWORD			 dwDownloaded;
	HINTERNET		 hSession = 0;
	HINTERNET 		 hConnect = 0;
	HINTERNET		 hRequest = 0;
	char const		*postDataStr;
	size_t			 postDataLen;
	hCloser			 h1 ( &hSession );
	hCloser			 h2 ( &hConnect );
	hCloser			 h3 ( &hRequest );

	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

	postDataStr = postData.dat.str.c;
	postDataLen = postData.dat.str.len;

	// Use WinHttpOpen to obtain a session handle.
#if 1
	if ( !(hSession = WinHttpOpen ( L"Engine", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 )) )
	{
		throw GetLastError ( );
	}
#else
	if ( !(hSession = WinHttpOpen ( L"Engine", WINHTTP_ACCESS_TYPE_NAMED_PROXY, L"localhost.:8888", WINHTTP_NO_PROXY_BYPASS, 0 )) )
	{
		throw GetLastError ( );
	}
#endif
	// Specify an HTTP server.
	if ( !(hConnect = WinHttpConnect ( hSession, converter.from_bytes ( domain.dat.str.c ).c_str(), (INTERNET_PORT)port, 0 )) )
	{
		throw GetLastError ( );
	}

	// Create an HTTP request handle.
	if ( !(hRequest = WinHttpOpenRequest ( hConnect, converter.from_bytes ( method.dat.str.c ).c_str ( ), converter.from_bytes ( url.dat.str.c ).c_str ( ), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isSSL ? WINHTTP_FLAG_SECURE : 0 )) )
	{
		throw GetLastError ( );
	}

	if ( headers.dat.str.len )
	{
		if ( !WinHttpAddRequestHeaders ( hRequest, converter.from_bytes ( headers.dat.str.c ).c_str(), -1, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD ) )
		{
			throw GetLastError ( );
		}
	}

	if ( authScheme )
	{
		if ( !WinHttpSetCredentials ( hRequest, WINHTTP_AUTH_TARGET_SERVER, authScheme, converter.from_bytes ( userId.dat.str.c ).c_str(), converter.from_bytes ( password.dat.str.c ).c_str ( ), NULL ) )
		{
			throw GetLastError ( );
		}
	}

	// PayPal server requests for client certificate ('Certificate Request') regardless.
	// Set the WINHTTP_NO_CLIENT_CERT_CONTEXT for WinHTTP when using 3-token authentication. 
	// This will let the server know the client doesn't have a certificate.
	WinHttpSetOption ( hRequest,
								  WINHTTP_OPTION_CLIENT_CERT_CONTEXT,
								  WINHTTP_NO_CLIENT_CERT_CONTEXT,
								  0 );

	DWORD dwOptionValue = WINHTTP_DISABLE_REDIRECTS;
	WinHttpSetOption ( hRequest,
								  WINHTTP_OPTION_DISABLE_FEATURE,
								  &dwOptionValue,
								  sizeof ( dwOptionValue ) );

	DWORD compOption = WINHTTP_DECOMPRESSION_FLAG_ALL;
	WinHttpSetOption ( hRequest,
								  WINHTTP_OPTION_DECOMPRESSION,
								  &compOption,
								  sizeof ( compOption ) );



	// Send a request.
	if ( !WinHttpSendRequest ( hRequest, 0, 0, const_cast<char *>(postDataStr), (DWORD)postDataLen, (DWORD) postDataLen, 0 ) )
	{
		throw GetLastError ( );
	}

	// End the request.
	if ( !WinHttpReceiveResponse ( hRequest, NULL ) )
	{
		throw GetLastError ( );
	}

	WinHttpQueryHeaders ( hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &dwSize, WINHTTP_NO_HEADER_INDEX );

	// Allocate memory for the buffer.
	if ( GetLastError ( ) == ERROR_INSUFFICIENT_BUFFER )
	{
		headerBuffer.makeRoom ( dwSize );

		// Now, use WinHttpQueryHeaders to retrieve the header.
		if ( !WinHttpQueryHeaders ( hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, bufferBuff ( &headerBuffer ), &dwSize, WINHTTP_NO_HEADER_INDEX ) )
		{
			throw GetLastError ( );
		}
		headerBuffer.assume ( dwSize );
	} else
	{
		throw GetLastError ( );
	}

	// make sure buffer starts at beginning - should be but this guarantees.

	headerBuffer.buffPos = wcstombs ( bufferBuff ( &headerBuffer ), (WCHAR *)bufferBuff ( &headerBuffer ), bufferSize ( &headerBuffer ) );

	BUFFER convertedHeaders;
	convertedHeaders.makeRoom ( headerBuffer.size () );
	wcstombs_s ( &convertedHeaders.buffPos, convertedHeaders.data<char *>(), convertedHeaders.avail(), headerBuffer.data<wchar_t *>(), _TRUNCATE );

	// Keep checking for data until there is nothing left.
	while ( 1 )
	{
		if ( !WinHttpQueryDataAvailable ( hRequest, &dwSize ) )
		{
			throw GetLastError ( );
		}
		if ( !dwSize )
		{
			break;
		}

		dataBuffer.makeRoom ( dwSize );

		if ( !WinHttpReadData ( hRequest, bufferBuff ( &dataBuffer ) + bufferSize ( &dataBuffer ), dwSize, &dwDownloaded ) )
		{
			throw GetLastError ( );
		}
		dataBuffer.assume ( dwDownloaded );
	}

	return webBuildResultArray ( instance, convertedHeaders, dataBuffer );
}

static VAR webSendBasic ( vmInstance *instance, VAR_STR const *method, VAR_STR const *domain, VAR_STR const *url, int64_t port, VAR_STR const *headers, VAR_STR const *postData, bool isSSL, VAR_STR const *userId, VAR_STR const *password )
{
	return webSend ( instance, *method, *domain, *url, port, *headers, *postData, isSSL, *userId, *password, WINHTTP_AUTH_SCHEME_BASIC );
}

static VAR webSendDigest ( vmInstance *instance, VAR_STR const *method, VAR_STR const *domain, VAR_STR const *url, int64_t port, VAR_STR const *headers, VAR_STR const *postData, bool isSSL, VAR_STR const *userId, VAR_STR const *password )
{
	return webSend ( instance, *method, *domain, *url, port, *headers, *postData, isSSL, *userId, *password, WINHTTP_AUTH_SCHEME_DIGEST );
}

static VAR vmWebSend ( vmInstance *instance, VAR_STR const *method, VAR_STR const *domain, VAR_STR const *url, int64_t port, VAR_STR const *headers, VAR_STR const *postData, bool isSSL )
{
	return webSend ( instance, *method, *domain, *url, port, *headers, *postData, isSSL, "", "", 0 );
}

VAR webSend ( vmInstance *instance, VAR_STR const &method, VAR_STR const &domain, VAR_STR const &url, int64_t port, VAR_STR const &headers, VAR_STR const &postData, bool isSSL )
{
	return webSend ( instance, method, domain, url, port, headers, postData, isSSL, "", "", 0 );
}

VAR_STR vmParseUserid ( vmInstance *instance, char const *url )
{
	char const*start;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;

	}
	start = url;
	while ( *url )
	{
		if ( *url == '/' )
		{
			return VAR_STR ( "" );
		}
		if ( *url == ':' || *url == '@' )
		{
			return VAR_STR ( instance, start, url - start );
		}
		url++;
	}
	return VAR_STR ( "" );
}

VAR_STR vmParsePassword ( vmInstance *instance, char const *url )
{
	char const	*start;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;

	}
	start = 0;
	while ( *url )
	{
		if ( *url == '/' )
		{
			return VAR_STR ( "" );
		}
		if ( *url == ':' )
		{
			start = url + 1;
		}
		if ( *url == '@' )
		{
			return VAR_STR ( instance, start, url - start );
		}
		url++;
	}
	return VAR_STR ( "" );
}

VAR_STR vmParsedDomain ( vmInstance *instance, char const *url )
{
	char const	*start;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;

	}
	start = 0;
	while ( *url )
	{
		if ( *url == '/' )
		{
			if ( start )
			{
				return VAR_STR ( instance, start, url - start );
			} else
			{
				break;
			}
		}
		if ( *url == '@' )
		{
			start = url + 1;
		}
		url++;
	}
	return VAR_STR ( "" );
}

VAR_STR vmParsedUri ( vmInstance *instance, char const *url )
{
	char const	*start;

	if ( !memcmpi ( url, "http://", 7 ) )
	{
		url += 7;
	} else if ( !memcmpi ( url, "https://", 8 ) )
	{
		url += 8;

	}
	start = 0;
	while ( *url )
	{
		if ( *url == '/' )
		{
			start = url + 1;
			while ( url[0] ) url = url + 1;
			return VAR_STR ( instance, start, (url - 1) - start );
		}
		url++;
	}
	return VAR_STR ( "" );
}

void builtinHTTPClientInit(class vmInstance *instance, opFile *file)
{
	REGISTER ( instance, file );
		FUNC ( "webSend", vmWebSend );
		FUNC ( "webSendEx", vmWebSend );
		FUNC ( "webSendDigest", webSendDigest );
		FUNC ( "webSendBasic", webSendBasic );
		FUNC ( "webAddrFromHost", webAddrFromHost );

		FUNC ( "UrlUserId", vmParseUserid );
		FUNC ( "UrlPassword", vmParsePassword );
		FUNC ( "UrlDomain", vmParsedDomain );
		FUNC ( "Domain", vmParsedDomain ) CONST PURE;
		FUNC ( "UrlPath", vmParsedUri ) CONST PURE;
		FUNC ( "uri", vmParsedUri ) CONST PURE;
	END;
}
