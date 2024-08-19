#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <winsock2.h>
#include <Windows.h>

#include "webServer.h"
#include "webSocket.h"
#include "webServerIOCP.h"

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmInstance.h"
#include "Target/vmTask.h"
#include "Utility/itoa.h"
#include "Utility/funcky.h"

#include "webTransactionLog.h"
#include "webBuffer.h"

static LARGE_INTEGER perfFreq;
static BOOL configured = (QueryPerformanceFrequency ( &perfFreq ), perfFreq.QuadPart = perfFreq.QuadPart / 1000, true);

static inline void genDate ( char *date, tm *t )
{
	static char const *nums = "00010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960";

	memcpy ( date, aShortDays[t->tm_wday], 3 );
	date[3] = ',';
	date[4] = ' ';
	*((short *)(date + 5)) = *((short *)&nums[(t->tm_mday - 1) << 1]);
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
	memcpy ( date + 25, " GMT", 4 ); // NOLINT (bugprone-not-null-terminated-result)
}

transLog::transLog ( char const *format, char const *fName, bool console )
{
	stringi					curStatic;		// accumulator for static text... gets cleared after being generated as a log entity
	std::vector<bool>		cond;			// vector of conditions that are valid for the next log entity
	uint32_t				value;
	std::string				foo;
	bool					origReq;

	cond.resize( 999 );
	while ( _isspace ( format ) ) format++;
	while ( *format )
	{
		switch ( *format )
		{
			case '%':
				format++;
				value = 0;
				if ( *format == '%' )
				{
					format++;
					curStatic += '%';
				} else
				{
					if ( curStatic.size() )
					{
						logElems.push_back ( transLogEntry ( curStatic ) );
						curStatic.clear();
					}
					if ( _isnum( format ) )
					{
						for ( size_t loop = 0; loop < 999; loop++ )
						{
							cond[loop] = false;
						}
						while ( _isnum( format ) )
						{
							while ( _isnum( format ) )
							{
								value *= 10;
								value += '0' - *format;
								format++;
							}
							cond.resize( value + 1 );
							cond[value] = true;
							if ( *format != ',' ) break;
							format++;
						}
					} else
					{
						for ( size_t loop = 0; loop < 999; loop++ )
						{
							cond[loop] = true;
						}
					}
					switch ( *format )
					{
						case '>':
							format++;
							origReq = true;
							break;
						case '<':
							origReq = false;
							format++;
							break;
						default:
							switch ( *format )
							{
								case 's':
								case 'U':
								case 'T':
								case 'D':
								case 'r':
									origReq = true;
									break;
								default:
									origReq = false;
									break;
							}
					}
					switch ( *format )
					{
						case 'a':
							logElems.push_back ( transLogEntry ( transLogEntry::tlRemoteIP, cond, origReq ) );
							break;
						case 'A':
							logElems.push_back ( transLogEntry ( transLogEntry::tlLocalIp, cond, origReq ) );
							break;
						case 'B':
							logElems.push_back ( transLogEntry ( transLogEntry::tlBytesSent, cond, origReq ) );
							break;
						case 'b':
							logElems.push_back ( transLogEntry ( transLogEntry::tlBytesSentCLF, cond, origReq ) );
							break;
						case 'c':
							logElems.push_back ( transLogEntry ( transLogEntry::tlConnectionStatus, cond, origReq ) );
							break;
						case 'f':
							logElems.push_back ( transLogEntry ( transLogEntry::tlFullName, cond, origReq ) );
							break;
						case 'h':
							logElems.push_back ( transLogEntry ( transLogEntry::tlHost, cond, origReq ) );
							break;
						case 'H':
							logElems.push_back ( transLogEntry ( transLogEntry::tlProtocol, cond, origReq ) );
							break;
						case 'i':
							logElems.push_back ( transLogEntry ( transLogEntry::tlHeader, cond, origReq ) );
							break;
						case 'l':
							logElems.push_back ( transLogEntry ( transLogEntry::tlRemoteLogname, cond, origReq ) );
							break;
						case 'm':
							logElems.push_back ( transLogEntry ( transLogEntry::tlMethod, cond, origReq ) );
							break;
						case 'q':
							logElems.push_back ( transLogEntry ( transLogEntry::tlQuery, cond, origReq ) );
							break;
						case 'r':
							logElems.push_back ( transLogEntry ( transLogEntry::tlRequestLine, cond, origReq ) );
							break;
						case 's':
							logElems.push_back ( transLogEntry ( transLogEntry::tlStatus, cond, origReq ) );
							break;
						case 't':
							logElems.push_back ( transLogEntry ( transLogEntry::tlTime, cond, origReq ) );
							break;
						case 'T':
							logElems.push_back ( transLogEntry ( transLogEntry::tlServiceTime, cond, origReq ) );
							break;
						case 'u':
							logElems.push_back ( transLogEntry ( transLogEntry::tlUserName, cond, origReq ) );
							break;
						case 'U':
							logElems.push_back ( transLogEntry ( transLogEntry::tlPageName, cond, origReq ) );
							break;
						case 'v':
						case 'V':
							logElems.push_back ( transLogEntry ( transLogEntry::tlServerName, cond, origReq ) );
							break;
						case 'D':
							switch ( format[1] )
							{
								case 'R':
									format++;
									logElems.push_back ( transLogEntry ( transLogEntry::tlRunTimeuSecs, cond, origReq ) );
									break;
								case 'L':
									format++;
									logElems.push_back ( transLogEntry ( transLogEntry::tlLoadTimeuSecs, cond, origReq ) );
									break;
								default:
									logElems.push_back ( transLogEntry ( transLogEntry::tlServiceTimeuSecs, cond, origReq ) );
									break;
							}
							break;
						case 'O':
							logElems.push_back ( transLogEntry ( transLogEntry::tlBytesSentNoHeaders, cond, origReq ) );
							break;
						case 'I':
							logElems.push_back ( transLogEntry ( transLogEntry::tlBytesRcvd, cond, origReq ) );
							break;
						case '{':
							foo.clear();
							while ( *format && (*format != '}') )
							{
								foo += *format;
								format++;
							}
							if ( !*format )
							{
								printf ( "Invalid log format directive around {\r\n" );
								return;
							}
							switch ( *format )
							{
								case 'C':
									logElems.push_back ( transLogEntry ( transLogEntry::tlServiceTimeuSecs, cond, origReq, foo ) );
									break;
								case 'e':
									logElems.push_back ( transLogEntry ( transLogEntry::tlEnvironmentVar, cond, origReq, foo ) );
									break;
								case 'o':
									logElems.push_back ( transLogEntry ( transLogEntry::tlHeaders, cond, origReq, foo ) );
									break;
								case'i':
									if ( foo == "X-Forwarded-For" || foo == "X-Forward-For" )
									{
										logElems.push_back( transLogEntry( transLogEntry::tlXForwardedFor, cond, origReq, foo ) );
									}
									break;
								case 'p':
									if ( foo == "local" )
									{
										logElems.push_back ( transLogEntry ( transLogEntry::tlLocalPort, cond, origReq ) );
									} else if ( foo == "remote" )
									{
										logElems.push_back ( transLogEntry ( transLogEntry::tlRemotePort, cond, origReq ) );
									} else if ( foo == "canonical" )
									{
										logElems.push_back ( transLogEntry ( transLogEntry::tlCanonicalPort, cond, origReq ) );
									}
									break;
								case 'P':
									if ( foo == "tid" )
									{
										logElems.push_back ( transLogEntry ( transLogEntry::tlProcessId, cond, origReq ) );
									} else if ( foo == "pid" )
									{
										logElems.push_back ( transLogEntry ( transLogEntry::tlThreadId, cond, origReq ) );
									} else if ( foo == "hextid" )
									{
										logElems.push_back ( transLogEntry ( transLogEntry::tlHexTID, cond, origReq ) );
									}
									break;
								case 't':
									logElems.push_back ( transLogEntry ( transLogEntry::tlTime, cond, origReq, foo ) );
									break;
							}
							break;

						default:
							printf ( "Invalid log format directive %c\r\n", *format );
							return;
					}
					format++;
				}
				break;
			case '\\':
				format++;
				if ( *format )
				{
					curStatic += *format;
					format++;
				}
				break;
			default:
				curStatic += *format;
				format++;
				break;
		}
	}
	if ( curStatic.size() )
	{
		logElems.push_back ( transLogEntry ( curStatic ) );
	}

	if ( !strccmp ( fName, "console" ) )
	{
		handle = CreateFile ( _T ( "CONOUT$" ), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL );
	} else
	{
		handle = CreateFile( _T( fName ), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL ); 
	}
}

bool emitConditionalHeader ( webServerHeader const *header, size_t &buffLen, uint8_t *&buff )
{
	if ( header )
	{
		if ( buffLen > (uint32_t) *header)
		{
			memcpy ( buff, (char const *) *header, (uint32_t) *header );
			buff += (uint32_t) *header;
			buffLen -= (uint32_t) *header;
		} else
		{
			return false;
		}
	} else
	{
		if ( buffLen  )
		{
			buff[0] = '-';
			buff++;
			buffLen--;
		} else
		{
			return false;
		}
	}
	return true;
}

size_t transLog::writeLog ( class webProtocolH1 *conn, uint8_t *buff, size_t buffLen )
{
	size_t	origLen = buffLen;
	size_t	size;

	serverSocket *sock = dynamic_cast<serverSocket *>(conn);
	for ( auto p = conn->protoParent; p; p = p->protoParent )
	{
		if ( p->type == serverProtoSocket )
		{
			sock = dynamic_cast<serverSocket *>(p);
			break;
		}
	}

	buffLen -= 2;	// for trailing /r/n

	for ( auto &it : logElems )
	{
		// see if we should emit this or not
		if ( it.condition.size() && !it.condition[conn->rsp.rspCode] ) continue;

		switch ( it.type )
		{
			case transLogEntry::tlText:
				if ( buffLen > it.text.size() )
				{
					memcpy ( buff, it.text.c_str(), it.text.size() );
					buffLen -= it.text.size();
					buff += it.text.size();
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlRemoteIP:
				if ( sock && (buffLen > INET6_ADDRSTRLEN + 1) )
				{
					DWORD strLen = (DWORD) buffLen;
					if ( WSAAddressToString ( (LPSOCKADDR) &sock->clientAddr, sizeof ( sock->clientAddr ), NULL, (char *) buff, &strLen ) >= 0 )
					{
						buff += strLen - 1;
						buffLen -= strLen - 1;
					}
				}
				break;
			case transLogEntry::tlLocalIp:
				if ( sock && (buffLen > INET6_ADDRSTRLEN) )
				{
					DWORD strLen = (DWORD) buffLen;
					if ( WSAAddressToString ( (LPSOCKADDR) &sock->localAddr, sizeof ( sock->localAddr ), NULL, (char *) buff, &strLen )  >= 0 )
					{
						buff += strLen - 1;
						buffLen -= strLen - 1;
					}
				}
				break;
			case transLogEntry::tlBytesSent:
				if ( buffLen > 16 )
				{  
					size = fastIToA ( conn->rsp.bytesSent - conn->rsp.headers.getSize(), (char *)buff );
					buffLen -= size;
					buff += size;
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlBytesSentCLF:
				if ( buffLen > 16 )
				{
					if ( conn->rsp.bytesSent - conn->rsp.headers.getSize() )
					{
						size = fastIToA ( conn->rsp.bytesSent - conn->rsp.headers.getSize(), (char *)buff );
						buffLen -= size;
						buff += size;
					} else
					{
						buff[0] = '-';
						buff++;
						buffLen--;
					}
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlBytesRcvd:
				if ( buffLen > 16 )
				{
					size = fastIToA ( (uint32_t)conn->req.bytesRcvd, (char *)buff );
					buffLen -= size;
					buff += size;
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlCookie:
				break;
			case transLogEntry::tlServiceTime:
				size = fastIToA ((size_t)((conn->rsp.reqEndTime.QuadPart - conn->req.reqStartTime.QuadPart) / perfFreq.QuadPart), (char *)buff );
				buffLen -= size;
				buff += size;
				break;
			case transLogEntry::tlFullName:
				break;
			case transLogEntry::tlHost:
				if ( !emitConditionalHeader ( &conn->req.headers[conn->req.host], buffLen, buff ) ) goto tooBig;
				break;
			case transLogEntry::tlXForwardedFor:
				if ( buffLen > conn->req.headers[conn->req.xForwardFor] )
				{
					memcpy( buff, conn->req.headers[conn->req.xForwardFor], conn->req.headers[conn->req.xForwardFor] );
					buffLen -= conn->req.headers[conn->req.xForwardFor];
					buff += conn->req.headers[conn->req.xForwardFor];
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlEnvironmentVar:
				break;
			case transLogEntry::tlProtocol:
				if ( buffLen < 5 )
				{
					goto tooBig;
				}
				if ( conn->req.server->isSecure )
				{
					memcpy ( buff, "https", 5 );
					buff += 5;
					buffLen -= 5;
				} else
				{
					memcpy ( buff, "http", 4 );
					buff += 4;
					buffLen -= 4;
				}
				break;
			case transLogEntry::tlHeader:
				for ( size_t loop = 0; loop < conn->req.headers.size ( ); loop++ )
				{
					if ( conn->req.headers[loop] == it.foo )
					{
						if ( !emitConditionalHeader ( &conn->req.headers[loop], buffLen, buff ) ) goto tooBig;
					}

				}
				break;
			case transLogEntry::tlHTTPType:
			case transLogEntry::tlHeaders:
				break;
			case transLogEntry::tlReferer:
				if ( conn->req.referer && (buffLen > (size_t) conn->req.headers[conn->req.referer]) )
				{
					memcpy ( buff, (char const *) conn->req.headers[conn->req.referer], (size_t) conn->req.headers[conn->req.referer] );
					buff += (size_t) conn->req.headers[conn->req.referer];
					buffLen -= (size_t) conn->req.headers[conn->req.referer];
				}
				break;
			case transLogEntry::tlUserAgent:
				if ( !emitConditionalHeader ( &conn->req.headers[conn->req.userAgent], buffLen, buff ) ) goto tooBig;
				break;
			case transLogEntry::tlMethod:
				if ( conn->req.method )
				{
					if ( buffLen > (size_t) conn->req.method->name.size ( ) )
					{
						memcpy ( buff, conn->req.method->name.c_str(), conn->req.method->name.size() );
						buff += (size_t) conn->req.method->name.length ( );
						buffLen -= (size_t) conn->req.method->name.length ( );
					} else
					{
						goto tooBig;
					}
				} else
				{
					if ( buffLen < 1 )
					{
						buff[0] = '-';
						buff++;
						buffLen--;
					} else
					{
						goto tooBig;
					}
				}
				break;
			case transLogEntry::tlClientPort:
				if ( sock && (buffLen > 5) )
				{
					size = itostr<uint32_t>( sock->clientAddr.sin6_port, buff );
					buffLen -= size;
					buff += size;
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlQuery:
				if ( !emitConditionalHeader ( &conn->req.headers[conn->req.query], buffLen, buff ) ) goto tooBig;
				break;
			case transLogEntry::tlRequestLine:
				if ( !emitConditionalHeader ( &conn->req.headers[conn->req.firstLine], buffLen, buff ) ) goto tooBig;
				break;
			case transLogEntry::tlStatus:
				if ( buffLen < 5 ) goto tooBig;
				size = itostr<uint32_t> ( conn->rsp.rspCode, buff );
				buffLen -= size;
				buff += size;
				break;
			case transLogEntry::tlTime:
				if ( buffLen < 30 ) goto tooBig;
				{
					tm		utcTime;
					time_t	currentTime;

					currentTime = time ( 0 );
					gmtime_s ( &utcTime, &currentTime );

					genDate ( (char *)buff, &utcTime );
					buffLen -= 29;
					buff += 29;
				}
				break;
			case transLogEntry::tlUserName:
				if ( !emitConditionalHeader ( &conn->req.headers[conn->req.authorization], buffLen, buff ) ) goto tooBig;
				break;
			case transLogEntry::tlPageName:
				break;
			case transLogEntry::tlServerName:
				if ( (buffLen > (size_t) conn->req.virtualServer->serverName.size ( )) )
				{
					memcpy ( buff, conn->req.virtualServer->serverName.c_str ( ), conn->req.virtualServer->serverName.length ( ) );
					buff += conn->req.virtualServer->serverName.length ( );
					buffLen -= conn->req.virtualServer->serverName.length ( );
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlConnectionStatus:
				if ( conn->rsp.bytesSent )
				{
					if ( conn->rsp.keepAlive )
					{
						buff[0] = '+';
						buffLen--;
						buff++;
					} else
					{
						buff[0] = '-';
						buffLen--;
						buff++;
					}
				} else
				{
					buff[0] = 'X';
					buffLen--;
					buff++;
				}
				break;
			case transLogEntry::tlRemoteLogname:
				break;
			case transLogEntry::tlRemotePort:
				if ( sock && (buffLen < 5) )
				{
					size = itostr<uint32_t> ( sock->clientAddr.sin6_port, buff );
					buffLen -= size;
					buff += size;
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlLocalPort:
				if ( sock && (buffLen < 5) )
				{
					size = itostr<uint32_t> ( sock->localAddr.sin6_port, buff );
					buffLen -= size;
					buff += size;
				} else
				{
					goto tooBig;
				}
				break;
			case transLogEntry::tlProcessId:
				if ( buffLen < 5 ) goto tooBig;
				size = itostr<DWORD> ( GetCurrentProcessId(), buff );
				buffLen -= size;
				buff += size;
				break;
			case transLogEntry::tlThreadId:
				if ( buffLen < 5 ) goto tooBig;
				size = itostr<DWORD> ( GetCurrentThreadId(), buff );
				buffLen -= size;
				buff += size;
				break;
			case transLogEntry::tlHexTID:
				if ( buffLen < 5 ) goto tooBig;
				size = strlen ( _ultoa ( GetCurrentThreadId(), (char *)buff, 16 ) );
				buffLen -= size;
				buff += size;
				break;
			case transLogEntry::tlServiceTimeuSecs:
				if ( buffLen < 5 ) goto tooBig;
				size = itostr<uint32_t> ( (uint32_t) ((conn->rsp.reqEndTime.QuadPart - conn->req.reqStartTime.QuadPart) / perfFreq.QuadPart), buff );
				buffLen -= size;
				buff += size;
				break;
			case transLogEntry::tlLoadTimeuSecs:
				if ( buffLen < 5 ) goto tooBig;
				if ( conn->rsp.activePage )
				{
					size = itostr<uint32_t> ( (uint32_t)((conn->rsp.loadEndTime.QuadPart - conn->rsp.loadStartTime.QuadPart) / perfFreq.QuadPart), buff );
					buffLen -= size;
					buff += size;
				} else
				{
					buff[0] = '0';
					buffLen--;
					buff++;
				}
				break;
			case transLogEntry::tlRunTimeuSecs:
				if ( buffLen < 5 ) goto tooBig;
				if ( conn->rsp.activePage )
				{
					size = itostr<uint32_t> ( (uint32_t)((conn->rsp.runEndTime.QuadPart - conn->rsp.runStartTime.QuadPart) / perfFreq.QuadPart), buff );
					buffLen -= size;
					buff += size;
				} else
				{
					buff[0] = '0';
					buffLen--;
					buff++;
				}
				break;
			default:
				printf ( "missing handler for log type %i\r\n", it.type );
				break;
		}
	}

tooBig:

	memcpy ( buff, "\r\n", 2 );

	return origLen - buffLen;
}

size_t transLog::allocateLogEntry ( size_t size )
{
	size_t	prevEntry;

	while ( 1 )
	{
		prevEntry = currentLogOffset;
		if ( InterlockedCompareExchange ( &currentLogOffset, prevEntry + size, prevEntry ) == prevEntry )
		{
			return prevEntry;
		}
	}
	return 0;
}
