
#include "webServer.h"
#include "webSocket.h"
#include "webProtoH1.h"

#include "Utility/funcky.h"
#include "Target/fileCache.h"
#include "bcVM/vmInstance.h"
#include "Utility/itoa.h"

// create_vec allows us to handle sparse vector initialization.   the http status strings are sparse and this lets us initialize them easily
template <typename U>
class create_vec
{
private:
    std::vector<U> m_vec;
public:
    create_vec(const size_t& key, const U& val)
    {
		m_vec.resize( key + 1 );
        m_vec[key] = val;
    }

    create_vec<U>& operator()(const size_t& key, const U& val)
    {
		m_vec.resize( key + 1 );
		m_vec[key] = val;
        return *this;
    }

    operator std::vector<U>()
    {
        return m_vec;
    }
};

std::vector<statString> httpStatusMap = create_vec<statString>		( 100, "100 Continue" )
																	( 101, "101 Switching Protocols" )
																	( 200, "200 OK" )
																	( 201, "201 Created" )
																	( 202, "202 Accepted" )
																	( 203, "203 Non-Authoritative Information	" )
																	( 204, "204 No Content" )
																	( 205, "205 Reset Content" )
																	( 206, "206 Partial Content" )
																	( 207, "207 Multi-status" )
																	( 300, "300 Multiple Choices" )
																	( 301, "301 Moved Permanently" )
																	( 302, "302 Found" )
																	( 303, "303 See Other" )
																	( 304, "304 Not Modified" )
																	( 305, "305 Use Proxy" )
																	( 306, "306 (Unused)" )
																	( 307, "307 Temprary Redirect" )
																	( 400, "400 Bad Request" )
																	( 401, "401 Unauthorized" )
																	( 402, "402 Payment Required" )
																	( 403, "403 Forbidden" )
																	( 404, "404 Not Found" )
																	( 405, "405 Method Not Allowed" )
																	( 406, "406 Not Acceptable" )
																	( 407, "407 Proxy Authentication Required" )
																	( 408, "408 Request Timeout" )
																	( 409, "409 Conflict" )
																	( 410, "410 Gone" )
																	( 411, "411 Length Required" )
																	( 412, "412 Precondition Failed" )
																	( 413, "413 Request Entity Too Large" )
																	( 414, "414 Request URI Too Long" )
																	( 415, "415 Unsupported Media Type" )
																	( 416, "416 Request Range Not Satisfiable" )
																	( 417, "417 Expectation Failed" )
																	( 419, "419 Authentication Timeout" )
																	( 422, "422 Unprocessable Entity" )
																	( 423, "423 Locked" )
																	( 424, "424 Failed Dependency" )
																	( 426, "426 Upgrade Required" )
																	( 428, "428 Precondition Required" )
																	( 429, "429 Too Many Requests" )
																	( 431, "431 Request Header Fields Too Large" )
																	( 440, "440 Session Expired" )
																	( 450, "450 Blocked" )
																	( 500, "500 Internal Server Error" )
																	( 501, "501 Not Implemented" )
																	( 502, "502 Bad Gateway" )
																	( 503, "503 Service Unavailable" )
																	( 504, "504 Gateway Timeout" )
																	( 505, "505 HTTP Version Not Supported" )
																	( 511, "511 Network Authentication Required" );


webServerPageState webServerProcess::process ( class vmInstance *instance, class webProtocolH1 *conn )
{
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	endTime;

	QueryPerformanceCounter ( &startTime );
	conn->req.method->handler ( instance, conn );
	QueryPerformanceCounter ( &endTime );

	return wsSendResponse;
}

void webServerResponse::buildCookies ( class webProtocolH1 *conn )
{
	for ( auto &it : conn->rsp.cookies )
	{
		conn->rsp.headers.write ( "Set-Cookie: ", 12 );
		conn->rsp.headers.write ( it.cookie, strlen ( it.cookie ) );
		conn->rsp.headers.write ( "\r\n", 2 );
	}
}

static inline void genDate ( char *date, tm *t )
{
	static char const *nums = "00010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960";

	memcpy ( date, aShortDays[t->tm_wday], 3 );
	date[3] = ',';
	date[4] = ' ';
	*((short *)(date + 5)) = *((short *)&nums[(t->tm_mday) << 1]);
	date[7] = ' ';
	memcpy ( date + 8, aShortMonths[t->tm_mon], 3 );
	date[11] = ' ';
	fastIToA ( t->tm_year + 1900, date + 12 );
	date[16] = ' ';
	*((short *)(date + 17)) = *((short *)&nums[(t->tm_hour) << 1]);
	date[19] = ':';
	*((short *)(date + 20)) = *((short *)&nums[(t->tm_min) << 1]);
	date[22] = ':';
	*((short *)(date + 23)) = *((short *)&nums[(t->tm_sec) << 1]);
	memcpy ( date + 25, " GMT", 5 );
}

void webServerResponse::buildRspHeaders ( class webProtocolH1 *conn )
{
	tm			 utcTime;
	char		 modifiedDate[32];
	char		 date[32];
	time_t		 currentTime;
	size_t	 len;

	if ( conn->rsp.body.bodyType == body::btObjectPageComplete )
	{
		return;
	}

	conn->rsp.headers.reset ( );

	if ( conn->rsp.body.bodyType != body::btNone )
	{
		if ( conn->req.ranges.size() && (conn->rsp.cachePolicy.policyType != webCachePolicy_NoCache) && !conn->rsp.isCompressed )
		{
			conn->rsp.rspCode = 206;
		}
	}

	currentTime = time ( 0 );

	gmtime_s ( &utcTime, &currentTime );
	genDate ( date, &utcTime );

	switch ( conn->rsp.rspCode )
	{
		case 200:
		case 201:
		case 202:
		case 204:
		case 205:
		case 206:
		case 207:		// webDAV
		case 208:		// webDAV
			gmtime_s ( &utcTime, &conn->rsp.modifiedTime );
			genDate ( modifiedDate, &utcTime );

			if ( conn->req.ifModifiedSince && conn->rsp.cachePolicy.policyType != webCachePolicy_NoCache )
			{
				if ( !conn->req.headers[conn->req.ifModifiedSince].cmpValue ( modifiedDate ) )
				{
					headers.write  ( "HTTP/1.1 304 Unmodified\r\n", 25 );
					buildCookies ( conn );
					headers.write  ( "Server: Slang Server\r\n", 22 );
					headers.write  ( "Content-Length: 0\r\n", 19 );
					headers.write ( "Last-Modified: ", 15 );
					headers.write ( modifiedDate, 29 );
					headers.write ( "\r\nDate: ", 8 );
					headers.write ( date, 29 );

					switch ( conn->rsp.cachePolicy.policyType )
					{
						case webCachePolicy_Public:
							switch ( conn->rsp.cachePolicy.expiresType )
							{
								case webCacheExpiresType_Fixed:
									headers.write ( "\r\nCache-Control: public\r\nExpires: ", 34 );
									headers.write ( conn->rsp.cachePolicy.expires, strlen ( conn->rsp.cachePolicy.expires ) );
									break;
								case webCacheExpiresType_At:
									break;
								case webCacheExpiresType_Every:
									headers.write ( "\r\nCache-Control: public, max-age=", 33 );
									headers.write ( conn->rsp.cachePolicy.maxAge );
									break;
							}
							break;
						case webCachePolicy_Private:
							switch ( conn->rsp.cachePolicy.expiresType )
							{
								case webCacheExpiresType_Fixed:
									headers.write ( "\r\nCache-Control: private\r\nExpires: ", 35 );
									headers.write ( conn->rsp.cachePolicy.expires, strlen ( conn->rsp.cachePolicy.expires ) );
									break;
								case webCacheExpiresType_At:
									break;
								case webCacheExpiresType_Every:
									headers.write ( "\r\nCache-Control: private, max-age=", 34 );
									headers.write ( conn->rsp.cachePolicy.maxAge );
									break;
							}
							break;
						case webCachePolicy_NoCache:
							headers.write ( "\r\nCache-Control: no-cache, no-store", 35 );
							break;
						case webCachePolicy_User:
							headers.write ( "\r\nCache-Control: ", 17 );
							headers.write ( conn->rsp.cachePolicy.user, strlen ( conn->rsp.cachePolicy.user ) );
							break;
						case webCachePolicy_None:
							headers.write ( conn->req.virtualServer->defaultCache.c_str ( ), conn->req.virtualServer->defaultCache.size ( ) );
							break;
					}

					if ( conn->rsp.mimeType->contentDisposition.length() && !conn->rsp.extraDisposition )
					{
						headers.write ( "\r\nContent-Disposition: ", 23 );
						headers.write ( conn->rsp.mimeType->contentDisposition.c_str(), conn->rsp.mimeType->contentDisposition.length() );
					}

					if ( conn->rsp.keepAlive )
					{
						headers.write ( "\r\nConnection: Keep-Alive\r\n", 26 );
					} else
					{
						headers.write ( "\r\nConnection: Close\r\n", 21 );
					}

					headers.write ( "\r\n", 2 );
					conn->rsp.body.release();
					goto printResponse;
				}
			}
			if ( conn->req.ifUnmodifiedSince )
			{
				if ( conn->req.headers[conn->req.ifUnmodifiedSince] == modifiedDate )
				{
					headers.write  ( "HTTP/1.1 412 Precondition Failed\r\n", 35 );
					buildCookies ( conn );
					headers.write  ( "Server: Slang Server\r\n", 22 );
					headers.write  ( "Content-Length: 0\r\n", 19 );
					headers.write ( "Last-Modified: ", 15 );
					headers.write ( modifiedDate, 29 );
					headers.write ( "\r\nDate: ", 8 );
					headers.write ( date, 29 );

					if ( conn->rsp.mimeType->contentDisposition.length() && !conn->rsp.extraDisposition )
					{
						headers.write ( "\r\nContent-Disposition: ", 23 );
						headers.write ( conn->rsp.mimeType->contentDisposition.c_str(), conn->rsp.mimeType->contentDisposition.length() );
					}

					if ( conn->rsp.keepAlive )
					{
						headers.write ( "\r\nConnection: Keep-Alive\r\n", 26 );
					} else
					{
						headers.write ( "\r\nConnection: Close\r\n", 21 );
					}

					headers.write ( "\r\n", 2 );
					conn->rsp.body.release();
					goto printResponse;
				}
			}
			if ( conn->req.ifRange )
			{
				// TODO: validate range requests
			}

			headers.write ( "HTTP/1.1 ", 9 );

			if ( conn->rsp.rspCode < httpStatusMap.size() )
			{
				auto &respIt = httpStatusMap[conn->rsp.rspCode];
				headers.write ( respIt.c_str(), respIt.length() );
				headers.write ( "\r\n", 2 );
			} else
			{
				headers.write ( conn->rsp.rspCode );
				headers.write ( " ", 1 );
				headers.write ( "Unknown\r\n", 9 );
			}

			headers.write  ( "Server: Slang Server\r\n", 22 );
			buildCookies ( conn );
			headers.write ( "Last-Modified: ", 15 );
			headers.write ( modifiedDate, 29  );
			headers.write ( "\r\nDate: ", 8 );
			headers.write ( date, 29 );
			headers.write  ( "\r\nAccept-Ranges: bytes", 22 );

			if ( conn->rsp.keepAlive )
			{
				headers.write ( "\r\nConnection: Keep-Alive", 24 );
			} else
			{
				headers.write ( "\r\nConnection: Close", 19 );
			}

			if ( (conn->rsp.body.bodyType == body::btChunked) || (size_t)body )
			{
				if ( conn->rsp.mimeType->contentDisposition.length ( ) && !conn->rsp.extraDisposition )
				{
					headers.write ( "\r\nContent-Disposition: ", 23 );
					headers.write ( conn->rsp.mimeType->contentDisposition.c_str ( ), conn->rsp.mimeType->contentDisposition.length ( ) );
				}
				/* is this un-cachable */
				switch ( conn->rsp.cachePolicy.policyType )
				{
					case webCachePolicy_Public:
						switch ( conn->rsp.cachePolicy.expiresType )
						{
							case webCacheExpiresType_Fixed:
								headers.write ( "\r\nCache-Control: public\r\nExpires: ", 34 );
								headers.write ( conn->rsp.cachePolicy.expires, strlen ( conn->rsp.cachePolicy.expires ) );
								break;
							case webCacheExpiresType_At:
								break;
							case webCacheExpiresType_Every:
								headers.write ( "\r\nCache-Control: public, max-age=", 33 );
								headers.write ( conn->rsp.cachePolicy.maxAge );
								break;
						}
						break;
					case webCachePolicy_Private:
						switch ( conn->rsp.cachePolicy.expiresType )
						{
							case webCacheExpiresType_Fixed:
								headers.write ( "\r\nCache-Control: private\r\nExpires: ", 35 );
								headers.write ( conn->rsp.cachePolicy.expires, strlen ( conn->rsp.cachePolicy.expires ) );
								break;
							case webCacheExpiresType_At:
								break;
							case webCacheExpiresType_Every:
								headers.write ( "\r\nCache-Control: private, max-age=", 34 );
								headers.write ( conn->rsp.cachePolicy.maxAge );
								break;
						}
						break;
					case webCachePolicy_NoCache:
						headers.write ( "\r\nCache-Control: no-cache, no-store", 35 );
						break;
					case webCachePolicy_User:
						headers.write ( "\r\nCache-Control: ", 17 );
						headers.write ( conn->rsp.cachePolicy.user, strlen ( conn->rsp.cachePolicy.user ) );
						break;
					case webCachePolicy_None:
						headers.write ( conn->req.virtualServer->defaultCache.c_str ( ), conn->req.virtualServer->defaultCache.size ( ) );
						break;
				}

				switch ( conn->rsp.body.bodyType )
				{
					case body::btNone:
						// this is here if we have a chunked response... no body to send as it will be sent programatically
						if ( conn->rsp.rspCode == 200 ) conn->rsp.rspCode = 204;
						break;
					case body::btFileCacheEntry:
					case body::btServerBuffer:
					case body::btPointer:
					case body::btObjectPage:
					case body::btObjectPageComplete:
						len = body;

						if ( conn->req.ranges.size ( ) && (conn->rsp.cachePolicy.policyType != webCachePolicy_NoCache) )
						{
							/* output the range blocks */
							size_t								startRange;
							size_t								endRange;
							int64_t									lenRange;
							char									tmpBuff[1024];

							for ( auto &it : conn->req.ranges )
							{
								if ( it.rangeHigh > len )
								{
									it.rangeHigh = len - 1;
								}
							}

							if ( conn->req.ranges.size ( ) > 1 )
							{
								// generate the multi-part boundary
								conn->rsp.multiPartBoundaryLen = _snprintf_s ( conn->rsp.multiPartBoundary, sizeof ( conn->rsp.multiPartBoundary ), _TRUNCATE, "fgl%08zu%08ufgl", (size_t) time ( 0 ), GetCurrentProcessId ( ) );

								headers.write ( "\r\nContent-Type: multipart/byteranges; boundary=", 47 );
								headers.write ( conn->rsp.multiPartBoundary, conn->rsp.multiPartBoundaryLen );

								// we have to loop through all the ranges and add the extra body size that is taken up by the multipart stuff... that's the whole purpose of this loop right now
								// we need to do this so we can create the proper content length

								lenRange = -2;	// to compensate for the extra \r\n at the start (which wouldn't be there in the normal output stream, but which makes this computation easier
								for ( auto &it : conn->req.ranges )
								{
									endRange = it.rangeHigh;
									startRange = it.rangeLow;

									if ( endRange > len - 1 )
									{
										endRange = len - 1;
									}
									if ( startRange < 0 )
									{
										/* start from the end */
										startRange = (endRange - 1) - startRange;
										if ( startRange < 0 )
										{
											startRange = 0;
										}
									}
									if ( startRange < len - 1 )
									{
										lenRange += _snprintf_s ( tmpBuff, sizeof ( tmpBuff ), _TRUNCATE, "\r\n--%s\r\n", conn->req.headers[conn->req.multiPartBoundary].name );
										lenRange += _snprintf_s ( tmpBuff, sizeof ( tmpBuff ), _TRUNCATE, "Content-Type: %s\r\n", conn->rsp.mimeType->contentType.c_str ( ) );
										lenRange += _snprintf_s ( tmpBuff, sizeof ( tmpBuff ), _TRUNCATE, "Content-Range: bytes %I64u-%I64u/%I64u\r\n\r\n", startRange, endRange, len );
										lenRange += (int64_t)(endRange - startRange + 1);
									}
								}

								if ( lenRange != -2 )
								{
									lenRange += _snprintf_s ( tmpBuff, sizeof ( tmpBuff ), _TRUNCATE, "\r\n--%s--\r\n", conn->req.headers[conn->req.multiPartBoundary].name );
									/* write out the total length of our ranges */
									headers.write ( "\r\nContent-Length: " );
									headers.write ( (uint32_t) lenRange );
								} else
								{
									conn->req.ranges.clear ( );
									conn->rsp.headers.reset ( );
									headers.write ( "HTTP/1.1 416 Range Not Satisfieable\r\n", 37 );
									headers.write ( "Server: Slang Server\r\n", 22 );
									headers.write ( "Date: ", 6 );
									headers.write ( date, 29 );
									headers.write ( "\r\nContent-Length: 0\r\n", 21 );
									buildCookies ( conn );
									if ( conn->rsp.keepAlive )
									{
										headers.write ( "Connection: Keep-Alive\r\n\r\n", 26 );
									} else
									{
										headers.write ( "Connection: Close\r\n\r\n", 21 );
									}
									conn->rsp.rspCode = 416;
									conn->rsp.body.release ( );
								}
							} else
							{
								// single range - must NOT use a multi-part breakdown										
								endRange = conn->req.ranges[0].rangeHigh;
								startRange = conn->req.ranges[0].rangeLow;

								if ( endRange > len - 1 )
								{
									endRange = len - 1;
								}
								if ( startRange < 0 )
								{
									startRange = (endRange - 1) - startRange;
									if ( startRange < 0 )
									{
										startRange = 0;
									}
								}
								if ( startRange < len - 1 )
								{
									/* this is something we can send */

									headers.printf ( "\r\nContent-Range: bytes %I64i-%I64i/%I64i", startRange, endRange, len );
									/* write out the total length of our ranges */
									headers.write ( "\r\nContent-Length: ", 18 );
									headers.write ( endRange - startRange + 1 );
									headers.write ( "\r\nContent-Type: ", 16 );
									headers.write ( conn->rsp.mimeType->contentType.c_str ( ), conn->rsp.mimeType->contentType.length ( ) );
								} else
								{
									conn->req.ranges.clear ( );
									conn->rsp.headers.reset ( );
									headers.write ( "HTTP/1.1 416 Range Not Satisfieable\r\n", 37 );
									headers.write ( "Server: Slang Server\r\n", 22 );
									headers.write ( "Date: ", 6 );
									headers.write ( date, 29 );
									headers.write ( "\r\nContent-Length: 0\r\n", 21 );
									buildCookies ( conn );
									if ( conn->rsp.keepAlive )
									{
										headers.write ( "Connection: Keep-Alive\r\n\r\n", 26 );
									} else
									{
										headers.write ( "Connection: Close\r\n\r\n", 21 );
									}
									conn->rsp.rspCode = 416;
									conn->rsp.body.release ( );
								}
							}
						} else
						{
							headers.write ( "\r\nContent-Type: ", 16 );
							headers.write ( conn->rsp.mimeType->contentType.c_str ( ), conn->rsp.mimeType->contentType.length ( ) );

							headers.write ( "\r\nContent-Length: ", 18 );
							headers.write ( len );
						}

						// content-encoding header
						if ( conn->rsp.isCompressed )
						{
							headers.write ( "\r\nContent-Encoding: gzip", 24 );
						}
						break;
					case body::btChunked:
						headers.write ( "\r\nContent-Type: ", 16 );
						headers.write ( conn->rsp.mimeType->contentType.c_str (), conn->rsp.mimeType->contentType.length () );
						// content-encoding header
						if ( conn->rsp.isCompressed )
						{
							headers.write ( "\r\nContent-Encoding: gzip", 24 );
						}
						headers.write ( "\r\nTransfer-Encoding: chunked", 28 );
						break;
					default:
						throw 500;
				}
			} else
			{
				if ( conn->rsp.rspCode == 200 ) conn->rsp.rspCode = 204;
				headers.write ( "\r\nContent-Length: 0", 19 );
			}

			if ( conn->rsp.extraHeaders.size() )
			{
				for ( auto &it : conn->rsp.extraHeaders )
				{
					headers.write ( "\r\n", 2 );
					headers.write ( it.headerLine );
				}
			}
			headers.write ( "\r\n\r\n", 4 );
			break;
		case 301:
		case 302:
			headers.write ( "HTTP/1.1 ", 9 );
			headers.write ( httpStatusMap[conn->rsp.rspCode].c_str(),  httpStatusMap[conn->rsp.rspCode].length() );
			headers.write  ( "\r\nServer: Slang Server\r\n", 24 );
			buildCookies ( conn );
			headers.write ( "Date: ", 6 );
			headers.write ( date, 29 );
			headers.write  ( "\r\nContent-Length: 0\r\n", 21 );
			if ( conn->rsp.extraHeaders.size() )
			{
				for ( auto &it : conn->rsp.extraHeaders )
				{
					headers.write ( it.headerLine );
					headers.write ( "\r\n", 2 );
				}
			}

			if ( conn->rsp.keepAlive )
			{
				headers.write ( "Connection: Keep-Alive\r\n", 24 );
			} else
			{
				headers.write ( "Connection: Close\r\n", 19 );
			}
			headers.write ( "Location: ", 10 );
			headers.write ( conn->rsp.body, conn->rsp.body );
			headers.write ( "\r\n", 2);
			headers.write ("\r\n", 2);
			conn->rsp.body.release();
			goto printResponse;
		case 401:
			headers.write ( "HTTP/1.1 401 Unauthorized\r\n", 29 );
			headers.write  ( "Server: Slang Server\r\n", 22 );
			headers.write ( "Date: ", 6 );
			headers.write ( date, 29 );
			headers.write  ( "\r\nContent-Length: 0\r\n", 21 );
			buildCookies ( conn );
			if ( conn->rsp.extraHeaders.size ( ) )
			{
				for ( auto &it : conn->rsp.extraHeaders )
				{
					headers.write ( it.headerLine );
					headers.write ( "\r\n", 2 );
				}
			}
			if ( conn->rsp.keepAlive )
			{
				headers.write ( "Connection: Keep-Alive\r\n", 24 );
			} else
			{
				headers.write ( "Connection: Close\r\n", 19 );
			}
			headers.write ( "WWW-Authenticate: Basic ", 14 );
			headers.write ( conn->rsp.body, conn->rsp.body );
			headers.write ( "\r\n", 2 );
			headers.write ("\r\n", 2);
			conn->rsp.body.release();
			goto printResponse;
		case 404:
			headers.write ( "HTTP/1.1 404 Not Found\r\n", 24);
			headers.write  ( "Server: Slang Server\r\n", 22 );
			headers.write ( "Date: ", 6 );
			headers.write ( date, 29 );
			headers.write  ( "\r\nContent-Length: 0\r\n", 21 );
			buildCookies ( conn );
			if ( conn->rsp.extraHeaders.size ( ) )
			{
				for ( auto &it : conn->rsp.extraHeaders )
				{
					headers.write ( it.headerLine );
					headers.write ( "\r\n", 2 );
				}
			}
			if ( conn->rsp.keepAlive )
			{
				headers.write ( "Connection: Keep-Alive\r\n\r\n", 26 );
			} else
			{
				headers.write ( "Connection: Close\r\n\r\n", 21 );
			}
			conn->rsp.body.release();
			goto printResponse;
		default:
			headers.write ( "HTTP/1.1 ", 9 );

			if ( conn->rsp.rspCode < httpStatusMap.size ( ) )
			{
				auto &respIt = httpStatusMap[conn->rsp.rspCode];
				headers.write ( respIt.c_str ( ), respIt.length ( ) );
				headers.write ( "\r\n", 2 );
			} else
			{
				headers.write ( conn->rsp.rspCode );
				headers.write ( " ", 1 );
				headers.write ( "Unknown\r\n", 9 );
			}
			headers.write ( "Server: Slang Server\r\n", 22 );
			headers.write ( "Date: ", 6 );
			headers.write ( date, 29 );
			headers.write ( "\r\nContent-Length: 0\r\n", 21 );
			buildCookies ( conn );
			if ( conn->rsp.extraHeaders.size ( ) )
			{
				for ( auto &it : conn->rsp.extraHeaders )
				{
					headers.write ( it.headerLine );
					headers.write ( "\r\n", 2 );
				}
			}
			if ( conn->rsp.keepAlive )
			{
				headers.write ( "Connection: Keep-Alive\r\n\r\n", 26 );
			} else
			{
				headers.write ( "Connection: Close\r\n\r\n", 21 );
			}
			conn->rsp.body.release ( );
			goto printResponse;
	}
printResponse:
#ifdef _DEBUG
	if ( conn->server->printResponseHeaders )
	{
		printf ( "-------------------------------RESPONSE--------------------------------------\r\n" );
		printf ( "%.*s", (int)headers.getSize(), headers.getBuffer() );
		printf ( "-------------------------------RESPONSE--------------------------------------\r\n" );
	}
#endif
	return;
}

void webServerResponse::body::release ()
{
	switch ( bodyType )
	{
		case btObjectPage:
		case btObjectPageComplete:
			if ( cMemoryPage )
			{
				cInstance->om->freeMemoryPage ( cMemoryPage );
			}
			break;
		case btFileCacheEntry:
			entry->free();
			break;
		case btServerBuffer:
			buff.reset();
			break;
		case btPointer:
			cInstance->free ( cFree );
			break;
		case btChunked:
			chunker.reset ();
		default:
			break;
	}
	bodyType = btNone;
}
