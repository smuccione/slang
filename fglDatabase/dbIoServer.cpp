/*
	HTTP SERVER

*/

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "Utility/settings.h"

#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mswsock.h>
#include <process.h>
#include <stdint.h>
#include <thread>
#include <tuple>

#include <utility>
#include <vector>
#include <unordered_map>

#include "odbf.h"
#include "dbIoServer.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmDebug.h"
#include "Target/vmConf.h"

HANDLE dbIocpCompletionPort = CreateIoCompletionPort ( INVALID_HANDLE_VALUE, NULL, 0, 0 );	// NOLINT (performance-no-int-to-ptr)

struct DB_IO_SERVER_PARAM {
	int			 port;
	HANDLE		 serverThreadHandle;
	DB_SERVER	*dbServer;
	HANDLE		 startedEvent;
};

static char const *ioName[] = { "Accept", "Read_Connect", "Read", "Write", "Disconnect", "Shutdown" };

int dbServerLog ( DB_SERVER_DB *dbs, char const *fmt, ... )
{
	char				 output[256];
	va_list				 varArgs;
	int					 len;
	struct tm			 utcTime;
	time_t				 lTime;

	lTime = time ( 0 );
	localtime_s ( &utcTime , &lTime );

	len = sprintf_s ( output, sizeof ( output ), "[%02u/%s/%04u %02u:%02u:%02u] %s%s ",
					utcTime.tm_mday,
					aShortMonths[utcTime.tm_mon],
					utcTime.tm_year + 1900,
					utcTime.tm_hour,
					utcTime.tm_min,
					utcTime.tm_sec,
					dbs ? dbs->db->tbl->alias : "",
					dbs ? ":" : ""
	);

	va_start ( varArgs, fmt );
	len += _vsnprintf_s ( output + len, sizeof ( output ) - len - 4, _TRUNCATE, fmt, varArgs );	/* 22 needs to be changed to match above sprintf */
	va_end ( varArgs );

	output[len++] = 0;

	return (printf ( "%s\r\n", output ));
}

void dbServerResetFD ( DB_SERVER *server, DB_SERVER_FD *fd )
{
	// only close it if it's really a socket

	EnterCriticalSection ( &server->dbFDAccessCtrl );
	if ( fd->socket )
	{
		if ( closesocket ( fd->socket ) )
		{
			printf ( "=================  CloseSocket: %u     res: %i	fd: %8p\r\n", (uint32_t) fd->socket, WSAGetLastError ( ), fd );
		}
		fd->socket = 0;
	}
	LeaveCriticalSection ( &server->dbFDAccessCtrl );
}

_inline void dbServerDisconnect ( DB_SERVER_FD *fd, DB_BUFFER *buffer )
{
	buffer->ioState = DBS_IO_DISCONNECT;
	if ( !TransmitFile ( fd->socket, 0, 0, 0, (OVERLAPPED *) buffer, 0, TF_DISCONNECT | TF_REUSE_SOCKET ) )
	{
		if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
		{
			dbServerResetFD ( fd->dbServer, fd );
			PostQueuedCompletionStatus ( dbIocpCompletionPort, 0, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
		}
	} else
	{
		PostQueuedCompletionStatus ( dbIocpCompletionPort, 0, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
	}
}

static _inline unsigned dbServerCheckDisconnect ( DB_SERVER_FD *fd, DB_BUFFER *buffer, DWORD *nBytesTransferred )

{
	if ( !*nBytesTransferred || !fd->dbServer->listenSocket || fd->freeOnComplete )
	{
		dbServerDisconnect ( fd, buffer );
		return (1);
	}
	if ( *nBytesTransferred == -1 )
	{
		// this was a pipelined request
		*nBytesTransferred = 0;
	}
	return (0);
}


unsigned dbSend ( void *params )

{
	WSABUF					 wsaBuff[2]{};
	DB_BUFFER				*newBuffer;
	DB_SERVER_FD			*fd;
	DWORD					 nBytesTransferred;

	fd = ((DB_SEND_PARAMS *) params)->fd;
	newBuffer = ((DB_SEND_PARAMS *) params)->buffer;

	((DB_SEND_PARAMS *) params)->sent = 1;

	wsaBuff[0].buf = newBuffer->buff.data<char *> ( );
	wsaBuff[0].len = (ULONG)newBuffer->buff.size();
	newBuffer->transferSize = wsaBuff[0].len;

	if ( WSASend ( fd->socket, wsaBuff, 1, &nBytesTransferred, 0, (OVERLAPPED *) newBuffer, 0 ) )
	{
		if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
		{
			dbServerDisconnect ( fd, newBuffer );
		}
	}

	return (1);
}

static void readCB ( class vmInstance *instance, void *data, char const *field, VAR *dest )
{
	std::pair<DATABASE *, DATABASE_CONNECTION *>	*localConn = (std::pair<DATABASE *, DATABASE_CONNECTION *> *)data;
	DATABASE			*db = std::get<0> ( *localConn );
	DATABASE_CONNECTION	*dbConn = std::get<1> ( *localConn );
	char				 dateTmp[9];
	char				 numBuff[31]{};
	char				 cTmp;
	double				 dTmp;

	size_t pos;

	if ( !(pos = dbFieldPos ( instance, db, dbConn, field )) )
	{
		throw errorNum::scFIELD_NOT_FOUND;
	}
	auto fType = dbFieldType ( instance, db, dbConn, pos );
	auto fLen = dbFieldLen ( instance, db, dbConn, pos );

	dest->type = slangType::eNULL;

	switch ( fType )
	{
		case '8':
		case '4':
		case '2':
		case '1':
			dest->type = slangType::eLONG;
			dbFieldGet ( instance, db, dbConn, pos, &(dest->dat.l) );
			break;
		case 'F':
			dest->type = slangType::eDOUBLE;
			dbFieldGet ( instance, db, dbConn, pos, &(dest->dat.d) );
			break;
		case 'N':
			fLen = (fLen < sizeof ( numBuff ) - 1 ? fLen : sizeof ( numBuff ) - 1);

			numBuff[fLen] = 0;
			dbFieldGet ( instance, db, dbConn, pos, numBuff );

			dest->type = slangType::eDOUBLE;
			dest->dat.d = atof ( _firstchar ( numBuff ) );

			if ( (dest->dat.d < (double) 0x7FFFFFFFFFFFFFFF) && !(bool)modf ( dest->dat.d, &dTmp ) )
			{
				dest->type = slangType::eLONG;
				dest->dat.l = (long) dest->dat.d;
			}
			break;
		case 'S':
			{
				auto ret = (char *) instance->om->alloc ( fLen + 1 );
				dbFieldGet ( instance, db, dbConn, pos, ret );
				ret[fLen] = 0;
				*dest = VAR_STR ( ret, fLen );
			}
			break;
		case 'L':
			dbFieldGet ( instance, db, dbConn, pos, &cTmp );
			dest->type = slangType::eLONG;
			dest->dat.l = (cTmp == 'T');
			break;
		case 'D':
			dest->type = slangType::eLONG;
			dest->dat.str.len = 0;
			dbFieldGet ( instance, db, dbConn, pos, &dateTmp );
			dateTmp[8] = 0;
			//		dest->dat.l =_dtoj ( dateTmp );	/* TODO: implment this */
			break;
		case 'B':
		default:
			{
				auto ret = (char *) instance->om->alloc ( fLen + 1 );
				dbFieldGet ( instance, db, dbConn, pos, ret );
				ret[fLen] = 0;
				*dest = VAR_STR ( ret, fLen );
			}
			break;
	}
}

static void writeCB ( class vmInstance *instance, void *data, char const *field, VAR *value )
{
	size_t			 pos;
	size_t			 fType;
	size_t			 fDec;
	size_t			 fLen;
	size_t			 len;
	int				 dec;
	int				 sign;
	size_t			 r;

	int64_t		 lTmp;
	double		 dTmp;
	char		 numBuff[66]{};

	std::pair<DATABASE *, DATABASE_CONNECTION *>	*localConn = (std::pair<DATABASE *, DATABASE_CONNECTION *> *)data;
	DATABASE			*db = std::get<0> ( *localConn );
	DATABASE_CONNECTION	*dbConn = std::get<1> ( *localConn );

	pos = dbFieldPos ( instance, db, dbConn, field );
	fType = dbFieldType ( instance, db, dbConn, pos );
	fLen = dbFieldLen ( instance, db, dbConn, pos );
	if ( fType == 'N' )
	{
		fDec = dbFieldDec ( instance, db, dbConn, pos );
	}
	switch ( fType )
	{
		case 'N':
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
				case slangType::eSTRING:
					switch ( TYPE ( value ) )
					{
						case slangType::eDOUBLE:
							dTmp = value->dat.d;
							break;
						case slangType::eLONG:
							dTmp = (double)value->dat.l;
							break;
						case slangType::eSTRING:
							dTmp = atof ( _firstchar ( value->dat.str.c ) );
							break;
						default:
							throw errorNum::scINTERNAL;
					}

					*numBuff = 0;

					_snprintf_s ( numBuff, sizeof ( numBuff ), _TRUNCATE, "%*.*f", (int) fLen, (int)fDec, dTmp );

					dbFieldPut ( instance, db, dbConn, pos, numBuff, fLen );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case 'L':
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
					if ( value->dat.l )
					{
						dbFieldPut ( instance, db, dbConn, pos, "T", 1 );
					} else
					{
						dbFieldPut ( instance, db, dbConn, pos, "F", 1 );
					}
					break;
				case slangType::eDOUBLE:
					if ( (bool)value->dat.d )
					{
						dbFieldPut ( instance, db, dbConn, pos, "T", 1 );
					} else
					{
						dbFieldPut ( instance, db, dbConn, pos, "F", 1 );
					}
					break;
				case slangType::eSTRING:
					switch ( *(value->dat.str.c) )
					{
						case 'T':
						case 't':
						case 'Y':
						case 'y':
						case '1':
							dbFieldPut ( instance, db, dbConn, pos, "T", 1 );
							break;
						default:
							dbFieldPut ( instance, db, dbConn, pos, "F", 1 );
							break;
					}
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case 'S':
			switch ( TYPE ( value ) )
			{
				case slangType::eDOUBLE:
				case slangType::eLONG:
					if ( TYPE ( value ) == slangType::eDOUBLE )
					{
						_ecvt_s ( numBuff, sizeof ( numBuff ), value->dat.d, 20, &dec, &sign );
					} else
					{
						_i64toa_s ( value->dat.l, numBuff, sizeof ( numBuff ), 10 );
					}

					len = strlen ( numBuff );

					if ( (len + 1L) < (long) (r = (int) dbFieldLen ( instance, db, dbConn, pos )) )
					{
						dbFieldPut ( instance, db, dbConn, pos, numBuff, len + 1 );
					} else
					{
						dbFieldPut ( instance, db, dbConn, pos, numBuff, r );
					}
					break;
				case slangType::eSTRING:
					r = (int) dbFieldLen ( instance, db, dbConn, pos );
					dbFieldPut ( instance, db, dbConn, pos, value->dat.str.c, r );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case 'F':
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
					dTmp = (double)value->dat.l;
					break;
				case slangType::eDOUBLE:
					dTmp = value->dat.d;
					break;
				case slangType::eSTRING:
					dTmp = atof ( _firstchar ( value->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			dbFieldPut ( instance, db, dbConn, pos, &dTmp, sizeof ( double ) );
			break;
		case '4':
		case '2':
		case '1':
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
					lTmp = value->dat.l;
					break;
				case slangType::eDOUBLE:
					lTmp = (long) (value->dat.d);
					break;
				case slangType::eSTRING:
					lTmp = atol ( _firstchar ( value->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			dbFieldPut ( instance, db, dbConn, pos, &lTmp, 4 );
			break;
		case 'D':
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
					lTmp = value->dat.l;
					break;
				case slangType::eDOUBLE:
					lTmp = (long) (value->dat.d);
					break;
				case slangType::eSTRING:
					lTmp = atol ( _firstchar ( value->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			//			_century ( 0 );					/* MISSING */
			//			_jtod ( lTmp, numBuff );
			dbFieldPut ( instance, db, dbConn, pos, numBuff, 8 );
			break;
		case 'C':
			switch ( TYPE ( value ) )
			{
				case slangType::eDOUBLE:
				case slangType::eLONG:
					if ( TYPE ( value ) == slangType::eDOUBLE )
					{
						_ecvt_s ( numBuff, sizeof ( numBuff ), value->dat.d, 20, &dec, &sign );
					} else
					{
						_i64toa_s ( value->dat.l, numBuff, sizeof ( numBuff ), 10 );
					}

					len = strlen ( numBuff );

					if ( (len) < (r = (int) fLen) )
					{
						dbFieldPut ( instance, db, dbConn, pos, numBuff, len + 1 );
					} else
					{
						dbFieldPut ( instance, db, dbConn, pos, numBuff, r );
					}
					break;
				case slangType::eSTRING:
					dbFieldPut ( instance, db, dbConn, pos, value->dat.str.c, value->dat.str.len );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		default:
			switch ( TYPE ( value ) )
			{
				case slangType::eSTRING:
					dbFieldPut ( instance, db, dbConn, pos, value->dat.str.c, value->dat.str.len );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
	}
}

static void closeCB ( class vmInstance *instance, void *db )
{
}

void __cdecl dbServerWorkerThread ( void *params )
{
	DB_SERVER				*dbServer;
	DB_SERVER_FD			*fd{};
	WSABUF					 wsaBuff[2]{};
	DWORD					 nBytesTransferred;
	BOOL					 result;
	DB_BUFFER				*newBuffer;
	DB_BUFFER				*buffer{};
	struct sockaddr_in		*addrIn{};
	int						 addrInLen;
	struct sockaddr_in		*addrOut{};
	int						 addrOutLen;
	int64_t					 needLen;
	DB_SEND_PARAMS			 earlyOutParams{};
	int						 optval;
	char					 name[256];

	// we need to do this for EACH worker thread... they all have their own independant workarea stacks
	sprintf_s ( name, sizeof ( name ), "dbWorker - %u", GetCurrentThreadId ( ) );

	vmTaskInstance instance ( name );

	builtinInit ( &instance, builtinFlags::builtin_RegisterOnly );

	instance.duplicate ( (vmTaskInstance *)params );

	for ( auto &[len,sharedPtr] : instance.preLoadedCode )
	{
		std::shared_ptr<uint8_t> dup ( (uint8_t *) instance.malloc ( len ), [&]( auto p ) { instance.free ( p ); } );
		memcpy ( &*dup, &*sharedPtr, len );
		instance.load ( dup, "builtIn", true, true );
	}

	instance.workareaTable->alloc ( "db", 0, readCB, writeCB, closeCB );
	instance.workareaTable->push ( "db" );

	printf ( "%s running\r\n", name );

	while ( 1 )
	{
		result = GetQueuedCompletionStatus ( dbIocpCompletionPort, &nBytesTransferred, (PULONG_PTR) &fd, (OVERLAPPED **) &buffer, INFINITE );
//		printf ( "%08lx  result=%4i   state: %13s   fd:%8p   buffer: %8p\r\n.", GetCurrentThreadId ( ), result, buffer ? ioName[buffer->ioState - 1] : "(none)", fd, buffer );

		if ( result )
		{
			dbServer = buffer->fd->dbServer;
			if ( dbServer->dbConfig->traceOutput )
			{
				printf ( "%08lx  result=%4i   state: %13s   fd:%8p   buffer: %8p\r\n", GetCurrentThreadId (), result, buffer ? ioName[buffer->ioState - 1] : "(none)", fd, buffer );
			}
			switch ( buffer->ioState )
			{
				case DBS_IO_ACCEPT:
					fd = buffer->fd;		// accept doesn't give us the FD we want

					// copy over the socket state from the listening socket
					setsockopt ( fd->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *) &dbServer->listenSocket, sizeof ( dbServer->listenSocket ) );

					optval = 1;
					setsockopt ( fd->socket, SOL_SOCKET, SO_KEEPALIVE, (char *) &optval, sizeof ( optval ) );

					// get the socket addresses
					GetAcceptExSockaddrs ( buffer->buff.data<char *>(),
//										   0,
											(DWORD)(buffer->buff.avail()- ((sizeof ( struct sockaddr_in) + 16) * 2)), 
											sizeof ( struct sockaddr_in ) + 16,
											sizeof ( struct sockaddr_in ) + 16,
											(LPSOCKADDR *) &addrIn,
											&addrInLen,
											(LPSOCKADDR *) &addrOut,
											&addrOutLen
											);

					/* get the info from the passed parameter structure */
					_ipToA ( ntohl ( addrOut->sin_addr.S_un.S_addr ), fd->clientIp );
					fd->clientPort = htons ( addrOut->sin_port );

					buffer->ioState = DBS_IO_READ_CONNECT;

					dbServer->dbServerNewAccept ( &instance );

					if ( nBytesTransferred )
					{
						PostQueuedCompletionStatus ( dbIocpCompletionPort, nBytesTransferred, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
					} else
					{
						// no data... so do a receive
						buffer->buff.reset ( );
						wsaBuff[0].buf = buffer->buff.data<char *>();
						wsaBuff[0].len = sizeof ( DB_CONNECT_MSG );

						fd->wsaFlags = 0;
						if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
						{
							if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
							{
								dbServerDisconnect ( fd, buffer );
							}
						}
					}
					break;
				case DBS_IO_READ_CONNECT:
					if ( dbServerCheckDisconnect ( fd, buffer, &nBytesTransferred ) )
					{
						break;
					}

					/* since we received directly into the buffer we need to advance it's used pointer
					to take into account the amount we received */
					buffer->buff.assume ( nBytesTransferred );
					if (buffer->buff.size() >= sizeof ( DB_CONNECT_MSG ) + sizeof ( DBS_MSG ) )
					{
						/* MISSING CODE! */
						/*  need to authenticate user */
						/* MISSING CODE! */

						// allocate a buffer for returning result
						newBuffer = dbServer->dbAllocateBuffer ( fd );
						newBuffer->ioState = DBS_IO_WRITE;

						if ( (needLen = dbProcessConn ( &instance, dbServer, fd, &buffer->buff, &newBuffer->buff )) )
						{
							dbServer->dbFreeBuffer ( fd, newBuffer );
							buffer->buff.makeRoom ( needLen );
							wsaBuff[0].buf = buffer->buff.data<char *>() +buffer->buff.size();
							wsaBuff[0].len = (ULONG)needLen;
							fd->wsaFlags = 0;
							if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
							{
								if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
								{
									dbServerDisconnect ( fd, buffer );
								}
							}
						} else
						{
							// write the response
							wsaBuff[0].buf = newBuffer->buff.data<char*> ( );
							wsaBuff[0].len = (ULONG) newBuffer->buff.size ( );
							newBuffer->transferSize = wsaBuff[0].len;
							if ( WSASend ( fd->socket, wsaBuff, 1, &nBytesTransferred, 0, (OVERLAPPED *) newBuffer, 0 ) )
							{
								if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
								{
									dbServer->dbFreeBuffer ( fd, newBuffer );
									dbServerDisconnect ( fd, buffer );
									break;
								}
							}

							if ( fd->dbs )
							{
								/* ok... we've established a connection so read the first message */
								buffer->buff.reset ( );
								buffer->ioState = DBS_IO_READ;
								wsaBuff[0].buf = buffer->buff.data<char*> ( );
								wsaBuff[0].len = (ULONG)sizeof ( DBS_MSG );
								fd->wsaFlags = 0;
								if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
								{
									if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
									{
										dbServerDisconnect ( fd, buffer );
									}
								}
							} else
							{
								buffer->buff.reset();
								buffer->ioState = DBS_IO_READ_CONNECT;
								wsaBuff[0].buf = buffer->buff.data<char*> ( );
								wsaBuff[0].len= (ULONG)sizeof ( DB_CONNECT_MSG ) + (ULONG)sizeof ( DBS_MSG );
								fd->wsaFlags = 0;
								if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
								{
									if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
									{
										dbServerDisconnect ( fd, buffer );
									}
								}
							}
						}
					} else
					{
						/* not enough data received... get some more */
						wsaBuff[0].buf = buffer->buff.data<char*> ( ) + buffer->buff.size ( );
						wsaBuff[0].len = (ULONG) ((sizeof ( DB_CONNECT_MSG ) + sizeof ( DBS_MSG )) - buffer->buff.size ( ));
						fd->wsaFlags = 0;
						if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
						{
							if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
							{
								dbServerDisconnect ( fd, buffer );
							}
						}
					}
					break;
				case DBS_IO_READ:
					if ( dbServerCheckDisconnect ( fd, buffer, &nBytesTransferred ) )
					{
						break;
					}

					/* since we received directly into the buffer we need to advance it's used pointer
					to take into account the amount we received */
					buffer->buff.assume ( nBytesTransferred );

					instance.workareaTable->setData ( fd->dbs->workAreaNum, &fd->localDb );

					newBuffer = dbServer->dbAllocateBuffer ( fd );
					newBuffer->ioState = DBS_IO_WRITE;

					earlyOutParams.buffer = newBuffer;
					earlyOutParams.fd = fd;
					earlyOutParams.dbServer = dbServer;
					earlyOutParams.sent = 0;

					if ( (needLen = dbProcessMessage ( &instance, fd->dbs, fd->dbConn, &buffer->buff, &newBuffer->buff, dbSend, &earlyOutParams )) > 0 )
					{
						// need more data, not enough to satisfy request
						dbServer->dbFreeBuffer ( fd, newBuffer );

						// make room for it
						buffer->buff.makeRoom ( needLen );

						// receive what we need;
						wsaBuff[0].buf = buffer->buff.data<char *>() +buffer->buff.size();
						wsaBuff[0].len = (ULONG) needLen;

						fd->wsaFlags = 0;
						if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
						{
							if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
							{
								dbServerDisconnect ( fd, buffer );
								break;
							}
						}
					} else
					{
						if ( !earlyOutParams.sent )
						{
							// we have data to send back, but have NOT already sent it
							wsaBuff[0].buf = newBuffer->buff.data<char *> ( );
							wsaBuff[0].len = (ULONG) newBuffer->buff.size ( );
							newBuffer->transferSize = wsaBuff[0].len;
							if ( WSASend ( fd->socket, wsaBuff, 1, &nBytesTransferred, 0, (OVERLAPPED *) newBuffer, 0 ) )
							{
								if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
								{
									dbServer->dbFreeBuffer ( fd, newBuffer );
									dbServerDisconnect ( fd, buffer );
									break;
								}
							}
						}

						switch ( needLen )
						{
							case -1:
								// this is a soft-close, reuse the connection for another dbOpen() call
								dbServer->dbReleaseTable ( fd->dbs );
								dbServer->dbFreeConnection ( fd->dbConn );

								fd->dbs = 0;
								fd->dbConn = 0;

								// bring us back into the read-connection state
								buffer->ioState = DBS_IO_READ_CONNECT;
								buffer->buff.reset();
								wsaBuff[0].buf = buffer->buff.data<char *>();
								wsaBuff[0].len = sizeof ( DBS_MSG );
								fd->wsaFlags = 0;
								if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
								{
									if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
									{
										dbServerDisconnect ( fd, buffer );
									}
								}
								break;
							case -2:
								// this is a hard-close
								dbServer->dbReleaseTable ( fd->dbs );
								// connection is closed... no more receives
								buffer->ioState = DBS_IO_DISCONNECT;
								PostQueuedCompletionStatus ( dbIocpCompletionPort, 0, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
								break;
							default:
								buffer->buff.reset();
								wsaBuff[0].buf = buffer->buff.data<char *>();
								wsaBuff[0].len = sizeof ( DBS_MSG );
								fd->wsaFlags = 0;
								if ( WSARecv ( fd->socket, wsaBuff, 1, &nBytesTransferred, &fd->wsaFlags, (OVERLAPPED *) buffer, 0 ) )
								{
									if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
									{
										dbServerDisconnect ( fd, buffer );
									}
								}
						}
					}
					break;
				case DBS_IO_WRITE:
					if ( dbServerCheckDisconnect ( fd, buffer, &nBytesTransferred ) )
					{
						break;
					}

					/* update sent bytes count */
					if ( (int) nBytesTransferred == buffer->transferSize )
					{
						// normal processing
						if ( fd->freeOnComplete )
						{
							instance.workareaTable->setData ( fd->dbs->workAreaNum, &fd->localDb );
							if ( dbServer->dbFreeFD ( &instance, fd, buffer ) )
							{
								dbServer->dbServerNewAccept ( &instance );
							}
						} else
						{
							dbServer->dbFreeBuffer ( fd, buffer );
						}
					} else
					{
						buffer->buff.remove ( nBytesTransferred );
						nBytesTransferred = 0;

						if (buffer->buff.size() )
						{
							wsaBuff[0].buf = buffer->buff.data<char *> ( );
							wsaBuff[0].len = (ULONG) buffer->buff.size ( );
							buffer->transferSize = wsaBuff[0].len;
							if ( WSASend ( fd->socket, wsaBuff, 1, &nBytesTransferred, 0, (OVERLAPPED *) buffer, 0 ) )
							{
								if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
								{
									dbServerDisconnect ( fd, buffer );
									break;
								}
							}
						}
					}
					break;
				case DBS_IO_DISCONNECT:
					if ( fd->dbs )
					{
						instance.workareaTable->setData ( fd->dbs->workAreaNum, &fd->localDb );
					}
					fd->freeOnComplete = 1;
					dbServer->dbFreeFD ( &instance, fd, buffer );
					if ( !dbServer->listenSocket )
					{
						if ( !dbServer->dbFDActiveList )
						{
							// listen socket has been closed and we no longer have any active FD's
							// post enough shutdowns so that all the other workers exit.
							EnterCriticalSection ( &dbServer->dbConnAccessCtrl );
							if ( !dbServer->shutdownSent )
							{
								dbServer->shutdownSent = true;
								for ( size_t loop = 0; loop < vmConf.dbServerThreads; loop++ )
								{
									newBuffer = dbServer->dbAllocateBuffer ( fd );
									newBuffer->ioState = DBS_IO_SHUTDOWN;
									PostQueuedCompletionStatus ( dbIocpCompletionPort, 0, (ULONG_PTR)fd, (OVERLAPPED *)newBuffer );
								}
							}
							LeaveCriticalSection ( &dbServer->dbConnAccessCtrl );
						} else 
						{
							// nothing to do... wait until all our open connections have closed
							// maybe some timeout for shutdown?
						}
					}
					break;

				case DBS_IO_SHUTDOWN:
					{
						// any FD's that are currently doing stuff?
						EnterCriticalSection ( &dbServer->dbFDAccessCtrl );
						auto fdScan = dbServer->dbFDActiveList;
						bool shutdown = true;
						while ( fdScan )
						{
							// any buffers with outstanding transactions?
							EnterCriticalSection ( &fdScan->bufferAccess );
							newBuffer = fdScan->buffers;
							bool fdBusy = true;
							while ( newBuffer )
							{
								switch ( newBuffer->ioState )
								{
									case DBS_IO_SHUTDOWN:
									case DBS_IO_READ_CONNECT:
									case DBS_IO_ACCEPT:
									case DBS_IO_DISCONNECT:
										break;
									default:
										printf ( "waiting... fd:%8p   buffer:%8p  state: %s\r\n", fdScan, newBuffer, newBuffer ? ioName[newBuffer->ioState - 1] : "(none)" );
										shutdown = false;
										fdBusy = true;
										break;
								}
								newBuffer = newBuffer->next;
							}
							if ( !fdBusy )
							{
								dbServerDisconnect ( fd, dbServer->dbAllocateBuffer ( fd ) );
							}
							LeaveCriticalSection ( &fdScan->bufferAccess );
							fdScan = fdScan->next;
						}
						LeaveCriticalSection ( &dbServer->dbFDAccessCtrl );
						dbServer->dbFreeFD ( &instance, fd, buffer );

						// did we find any that need to complete?
						if ( !shutdown )
						{
							// repost the buffer so we get another try in the future... hopefully when all has completed
							PostQueuedCompletionStatus ( dbIocpCompletionPort, -1, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
						} else
						{
							// repost buffer so next thread get's a chance to shutdown
							PostQueuedCompletionStatus ( dbIocpCompletionPort, -1, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
							goto Shutdown_Complete;
						}
						break;
					}
				default:
					break;
			}
		} else
		{
			if ( buffer )
			{
				// if no FD then this was on a listen socket....
				fd = buffer->fd;
				dbServer = fd->dbServer;
				switch ( buffer->ioState )
				{
					case DBS_IO_ACCEPT:
						// accept failed... nuke the socket, disconnect the fd, allocate a brand new accept socket
						if ( dbServer->listenSocket )
						{
							dbServer->dbFDReset ( fd );
							buffer->ioState = DBS_IO_DISCONNECT;
							PostQueuedCompletionStatus ( dbIocpCompletionPort, 0, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
							dbServer->dbServerNewAccept ( &instance );
						} else
						{
							buffer->ioState = DBS_IO_DISCONNECT;
							PostQueuedCompletionStatus ( dbIocpCompletionPort, 0, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
						}
						break;
					default:
						buffer->ioState = DBS_IO_DISCONNECT;
						dbServer->dbFDReset ( fd );
						PostQueuedCompletionStatus ( dbIocpCompletionPort, 0, (ULONG_PTR) fd, (OVERLAPPED *) buffer );
						break;
				}
			} else
			{
				// iocp error... shutdown
				break;
			}
		}
	}
	Shutdown_Complete:
	printf ( "%s stopped\r\n", name );
}

class dbServerTaskControl : public taskControl
{
	HANDLE			handles[128];
	uint32_t		maxHandles = 0;

	public:
	std::vector<DB_SERVER *>	servers;

	dbServerTaskControl ()
	{
	}

	void end ( void )
	{
		for ( auto &it : servers )
		{
			// close socket so we don't accept any more connections
			auto sock = it->listenSocket;
			it->listenSocket = 0;
			closesocket ( sock );

			// send a shutdown message to each of the worker threads.   They will exit ONLY when all outstanding connections have been closed (this doesn't mean closed, but in a resuable state with no more message handling)
			auto fd = it->dbAllocateFD ( dbIocpCompletionPort );
			auto buffer = it->dbAllocateBuffer ( fd );
			buffer->ioState = DBS_IO_SHUTDOWN;
			PostQueuedCompletionStatus ( dbIocpCompletionPort, -1, (ULONG_PTR) fd, (OVERLAPPED *) buffer );

			for ( auto &table : it->dbs )
			{
				printf ( "Shutting down: %s\r\n", table.first.c_str () );
				dbClose ( table.second->db );
			}
		}
		WaitForMultipleObjects ( maxHandles, handles, true, INFINITE );

		for ( auto it = servers.begin ( ); it != servers.end ( );)
		{
			delete *it;
			it = servers.erase ( it );
		}
	}

	void operator += ( HANDLE handle )
	{
		if ( maxHandles < sizeof ( handles ) / sizeof ( handles[0] ) )
		{
			handles[maxHandles++] = handle;
		} else
		{
			printf ( "error: to many worker threads" );
			throw errorNum::scINTERNAL;
		}
	}

	void operator += ( DB_SERVER *server )
	{
		servers.push_back ( server );
	}
};

/* initialize invidual database servers (probably never have more than one, but this is the mechanism in place for task control (orderly shutdown) */
void dbServerInit ( taskControl *ctrl, char const *configFile, vmInstance *instance )
{
	SOCKET					 listenSocket;
	u_long					 r;
	struct	sockaddr_in		 addr{};
	DB_BUFFER				*newBuffer;
	DB_SERVER_FD			*fd;
	DWORD					 nBytesTransferred;

	auto dbServer = new DB_SERVER ( configFile );

	/* create the socket for communicating with clients */
	listenSocket = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

	/* set the socket to non-blocking */
	r = 1;
	ioctlsocket ( listenSocket, FIONBIO, &r );

	addr.sin_port = htons ( dbServer->dbConfig->port );
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	addr.sin_family = AF_INET;

	/* bind it to our port */
	if ( bind ( listenSocket, (SOCKADDR *) &addr, sizeof ( addr ) ) == SOCKET_ERROR )
	{
		dbServerLog ( 0, "Web Server is unable to bind to socket %i... closing down", dbServer->dbConfig->port );
		closesocket ( listenSocket );
		throw WSAGetLastError ( );
	}

	if ( listen ( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
	{
		dbServerLog ( 0, "Database Server is unable to listen to socket %i... closing down", dbServer->dbConfig->port );
		closesocket ( listenSocket );
		throw WSAGetLastError ( );
	}

	dbServer->listenSocket = listenSocket;

	CreateIoCompletionPort ( (HANDLE) listenSocket, dbIocpCompletionPort, 0, 0 );	// NOLINT(performance-no-int-to-ptr)

	for ( size_t loop = 0; loop < static_cast<size_t>(vmConf.dbServerThreads) * 4; loop++ )
	{
		fd = dbServer->dbAllocateFD ( dbIocpCompletionPort );
		newBuffer = dbServer->dbAllocateBuffer ( fd );
		newBuffer->ioState = DBS_IO_ACCEPT;
		if ( !AcceptEx ( dbServer->listenSocket, fd->socket, newBuffer->buff.data<char*>(), (DWORD) newBuffer->buff.avail()- ((sizeof ( struct sockaddr_in ) + 16) * 2), sizeof ( struct sockaddr_in ) + 16, sizeof ( struct sockaddr_in ) + 16, &nBytesTransferred, (OVERLAPPED *) newBuffer ) )
		{
			if ( WSAGetLastError ( ) != ERROR_IO_PENDING )
			{
				dbServerLog ( 0, "Unable to start worker thread.  Listen socket closed prematurely\r\n" );
				return;
			}
		}
	}

	dbServerLog ( 0, "Database Server started on port %i", dbServer->dbConfig->port );

	*dynamic_cast<dbServerTaskControl *>(ctrl) += dbServer;
}

/* fire up the worker threads for ALL database instances */
taskControl	*dbServerStart ( size_t numWorkerThreads, vmInstance *instance )
{
	dbServerTaskControl	*ctrl;

	ctrl = new dbServerTaskControl ();

	while ( numWorkerThreads-- )
	{
		*ctrl += (HANDLE) _beginthread ( dbServerWorkerThread, CPU_STACK_SIZE, dynamic_cast<vmTaskInstance *>(instance) );	// NOLINT(performance-no-int-to-ptr)
	}

	return ctrl;
}


