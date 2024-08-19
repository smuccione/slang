/*
	IPC Comms base DB interface

*/

#include "Utility/settings.h"

#include "stdafx.h"

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmNativeInterface.h"
#include <openssl/sha.h>
#include <openssl/evp.h>

#include "bcVM/bcVMBuiltin.h"

#include "fglDatabase/DbIoServer.h"

#define USE_CACHE 1

static uint32_t	cCtr = 0;

struct REMOTE_CONN_CACHE {
	struct	REMOTE_CONN_CACHE	*next;
	SOCKET						 socket;
	DWORD						 ip;
	short						 port;
};

struct REMOTE_DB {
	struct REMOTE_DB	*next;
	int					 cCtr;
	REMOTE_CONN_CACHE	*conn;
	BUFFER				 buffer;
	BUFFER				 recordBuff;
	int					 numFields;
	int					 isHot;
	int					 isValid;
	DBFFIELDS			*fields;
	databaseStatus		 status;
};

#pragma pack ( push, 1 )
struct DBFIELDINFO {
	int					 cCtr;
	int					 fieldNum;
	int					 fieldType;
	union
	{
		unsigned long	 pad;
		unsigned short	 fieldlen;
		struct
		{
			 char		 len;
			 char		 dec;
		}	num;
	} lenInfo;			
};
#pragma pack ( pop )

static CRITICAL_SECTION		 remoteConnCacheCtrl;
static REMOTE_CONN_CACHE	*remoteConnCache = 0;
static uint32_t				 remoteConnCacheCnt = 0;
static CRITICAL_SECTION		 dbRemoteListCtrl;
static REMOTE_DB			*dbRemoteList = 0;

class remoteDbInititializerClass {
public:
	remoteDbInititializerClass()
	{
		InitializeCriticalSectionAndSpinCount ( &remoteConnCacheCtrl, 4000 );
		InitializeCriticalSectionAndSpinCount ( &dbRemoteListCtrl, 4000 );
	}
	~remoteDbInititializerClass()
	{
		DeleteCriticalSection ( &remoteConnCacheCtrl );
		DeleteCriticalSection ( &dbRemoteListCtrl );
	}
};

static remoteDbInititializerClass	remoteInit;

static int remoteDbFieldInfoByName ( REMOTE_DB *db, DBS_MSG *msg )
{
	int loop;

	for ( loop = 0; loop < db->numFields; loop++ )
	{
		if ( !fglstrccmp ( msg->dat.req.dbFieldInfoByName.fieldName, db->fields[loop].name ) )
		{
			msg->dat.rsp.dbFieldInfo.fieldNum	= loop + 1;
			msg->dat.rsp.dbFieldInfo.fieldType	= (char)db->fields[loop].type;
			msg->dat.rsp.dbFieldInfo.fieldLen	= static_cast<unsigned char>(db->fields[loop].leninfo.num.len);
			msg->dat.rsp.dbFieldInfo.fieldDec	= static_cast<unsigned char>(db->fields[loop].leninfo.num.dec);
			memcpy ( msg->dat.req.dbFieldInfoByName.fieldName, db->fields[loop].name, db->fields[loop].nameLen );

			msg->status = 0;
			return 1 ;
		}
	}
	msg->status = int(errorNum::scFIELD_NOT_FOUND);
	return 0 ;
}

static int remoteDbFieldInfoByPos ( REMOTE_DB *db, DBS_MSG *msg  )
{
	int fNum;

	fNum = msg->dat.req.dbFieldInfo.fieldNum - 1;

	if ( (fNum >= 0) && (fNum < db->numFields) )
	{
		msg->dat.rsp.dbFieldInfo.fieldNum	= fNum + 1;
		msg->dat.rsp.dbFieldInfo.fieldType	= (char)db->fields[fNum].type;
		msg->dat.rsp.dbFieldInfo.fieldLen	= static_cast<unsigned char>(db->fields[fNum].leninfo.num.len);
		msg->dat.rsp.dbFieldInfo.fieldDec	= static_cast<unsigned char>(db->fields[fNum].leninfo.num.dec);
		memcpy ( msg->dat.req.dbFieldInfoByName.fieldName, db->fields[fNum].name, db->fields[fNum].nameLen );

		msg->status = 0;
		return 1 ;
	}
	msg->status = int(errorNum::scFIELD_NOT_FOUND);
	return 0 ;
}

static REMOTE_CONN_CACHE *remoteAllocateConnNC (  unsigned long ip, int port )
{
	REMOTE_CONN_CACHE	*conn;
	struct	sockaddr_in	 addr {};
	int value;

	conn = (REMOTE_CONN_CACHE *)malloc ( sizeof ( REMOTE_CONN_CACHE ) * 1 );
	if ( !conn ) throw errorNum::scMEMORY;
	conn->socket  = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );
	conn->ip = ip;
	conn->port = (short)port;
	addr.sin_port				= htons ( port );
	addr.sin_addr.S_un.S_addr	= htonl ( ip );
	addr.sin_family				= AF_INET;

	value = 1;
	setsockopt ( conn->socket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof ( value ) );

	value = 0;
//	setsockopt ( conn->socket, SOL_SOCKET, SO_SNDBUF, (char *)&value, sizeof ( value ) );

	if ( WSAConnect ( conn->socket, (SOCKADDR *)&addr, sizeof ( addr ), 0, 0, 0, 0 ) == SOCKET_ERROR )
	{
		free ( conn );
		throw errorNum::scDATABASE_REQUEST_TIMEOUT;
	}
	return conn ;
}

static REMOTE_CONN_CACHE *remoteAllocateConn (  unsigned long ip, int port )
{
	REMOTE_CONN_CACHE	*conn;
	REMOTE_CONN_CACHE	*connPrev;

	EnterCriticalSection ( &remoteConnCacheCtrl );

	// see if we have a cached connection to the database
	conn = remoteConnCache;
	connPrev = 0;
	while ( conn )
	{
		if ( (conn->port == port) && (conn->ip == ip) )
		{
			remoteConnCacheCnt--;
			if ( connPrev )
			{
				connPrev->next = conn->next;
			} else
			{
				remoteConnCache = conn->next;
			}
			LeaveCriticalSection ( &remoteConnCacheCtrl );
			return conn ;
		}
		connPrev = conn;
		conn = conn->next;
	}
	LeaveCriticalSection ( &remoteConnCacheCtrl );

	// nope, so allocate a new one
	return remoteAllocateConnNC ( ip, port ) ;
}

static int remoteDBResetConn ( REMOTE_CONN_CACHE *conn )
{
	struct	sockaddr_in	 addr {};
	int					 value;

	// close old socket
	closesocket ( conn->socket );

	// allocate a new one
	conn->socket  = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

	value = 1;
	setsockopt ( conn->socket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof ( value ) );

	value = 0;
//	setsockopt ( conn->socket, SOL_SOCKET, SO_SNDBUF, (char *)&value, sizeof ( value ) );

	// see if we can connect
	addr.sin_port				= htons ( conn->port );
	addr.sin_addr.S_un.S_addr	= htonl ( conn->ip );
	addr.sin_family				= AF_INET;
	if ( WSAConnect ( conn->socket, (SOCKADDR *)&addr, sizeof ( addr ), 0, 0, 0, 0 ) == SOCKET_ERROR )
	{ 
		return 0 ;
	}
	return 1 ;
}

static REMOTE_DB *remoteDBAllocate ( void )
{
	REMOTE_DB *db;

	EnterCriticalSection ( &dbRemoteListCtrl );
	if ( dbRemoteList )
	{
		db = dbRemoteList;
		dbRemoteList = dbRemoteList->next;
		LeaveCriticalSection ( &dbRemoteListCtrl );
	} else
	{
		LeaveCriticalSection ( &dbRemoteListCtrl );

		db = new REMOTE_DB ();
	}
	db->conn = 0;
	db->fields = 0;
	db->cCtr = InterlockedIncrement ( &cCtr );

	return db ;
}

static void remoteDBFree ( REMOTE_DB *db )
{
	if ( db->conn )
	{
		closesocket ( db->conn->socket );
		free ( db->conn );
	}
	if ( db->fields )
	{
		free ( db->fields );
	}

	EnterCriticalSection ( &dbRemoteListCtrl );
	db->next = dbRemoteList;
	dbRemoteList = db;
	LeaveCriticalSection ( &dbRemoteListCtrl );
}

static int remoteRequest ( REMOTE_DB *db, DBS_MSG *txMsg, DBS_MSG *rxMsg, void const *data, unsigned int dataLen )
{
	WSABUF		 wsaBuf[2]{};
	DWORD		 nWsaBuf;
	DWORD		 nBytesSent;
	DWORD		 nBytesRcvd;
	DWORD		 flags;
	DWORD		 len;
	fd_set		 readFds{};
	fd_set		 errorFds;
	int			 rc;

	flags = 0;
	nWsaBuf = 1;

	wsaBuf[0].buf = (char *)txMsg;
	wsaBuf[0].len = sizeof ( DBS_MSG );

	if ( data )
	{
		nWsaBuf = 2;
		wsaBuf[1].buf = (char *)data;
		wsaBuf[1].len = dataLen;
	}

	if ( WSASend ( db->conn->socket, wsaBuf, nWsaBuf, &nBytesSent, 0, 0, 0 ) )
	{
		throw errorNum::scDATABASE_REQUEST_TIMEOUT;
	}

	rxMsg->status = int(errorNum::scINTERNAL);

	len = sizeof ( *rxMsg );
	while ( len )
	{
		wsaBuf[0].buf = (char *)rxMsg;
		wsaBuf[0].len = len;

		flags = 0;

		FD_ZERO ( &readFds );
		FD_SET ( db->conn->socket, &readFds );
		errorFds = readFds;

		rc = select ( 1, &readFds, 0, &errorFds, &vmConf.dbReqTimeout );

		if ( !rc )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		if ( rc == SOCKET_ERROR )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}

		if ( WSARecv ( db->conn->socket, wsaBuf, 1, &nBytesRcvd, &flags, 0, 0 ) )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		if ( !nBytesRcvd )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		len -= nBytesRcvd;
		rxMsg = (DBS_MSG *)((char *)rxMsg + nBytesRcvd);
	}

	return 1 ;
}

static int remoteRequestLong ( REMOTE_DB *db, DBS_MSG *txMsg, DBS_MSG *rxMsg, void const *data, unsigned int dataLen )
{
	WSABUF		 wsaBuf[2]{};
	DWORD		 nWsaBuf;
	DWORD		 nBytesSent;
	DWORD		 nBytesRcvd;
	DWORD		 flags;
	DWORD		 len;
	fd_set		 readFds{};
	fd_set		 errorFds;
	int			 rc;

	flags = 0;
	nWsaBuf = 1;

	wsaBuf[0].buf = (char *)txMsg;
	wsaBuf[0].len = sizeof ( DBS_MSG );

	if ( data )
	{
		nWsaBuf = 2;
		wsaBuf[1].buf = (char *)data;
		wsaBuf[1].len = dataLen;
	}

	if ( WSASend ( db->conn->socket, wsaBuf, nWsaBuf, &nBytesSent, 0, 0, 0 ) )
	{
		throw errorNum::scDATABASE_REQUEST_TIMEOUT;
	}

	rxMsg->status = int(errorNum::scINTERNAL);

	len = sizeof ( *rxMsg );
	while ( len )
	{
		wsaBuf[0].buf = (char *)rxMsg;
		wsaBuf[0].len = len;

		flags = 0;

		FD_ZERO ( &readFds );
		FD_SET ( db->conn->socket, &readFds );
		errorFds = readFds;

		rc = select ( 1, &readFds, 0, &errorFds, &vmConf.dbReqBigTimeout );

		if ( !rc )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		if ( rc == SOCKET_ERROR )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}

		if ( WSARecv ( db->conn->socket, wsaBuf, 1, &nBytesRcvd, &flags, 0, 0 ) )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		if ( !nBytesRcvd )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		len -= nBytesRcvd;
		rxMsg = (DBS_MSG *)((char *)rxMsg + nBytesRcvd);
	}

	return 1 ;
}

static REMOTE_DB *remoteRequestSpecialProcess ( REMOTE_DB *db, DB_CONNECT_MSG *dbConnMsg, DBS_MSG *txMsg, DBS_MSG *rxMsg, void *data, unsigned int dataLen )
{
	WSABUF		 wsaBuf[3]{};
	DWORD		 flags;
	DWORD		 nWsaBuf;
	DWORD		 nBytesSent;
	DWORD		 nBytesRcvd;
	DWORD		 len;
	fd_set		 readFds{};
	fd_set		 errorFds;
	int			 rc;

	nWsaBuf = 2;
	wsaBuf[0].len = sizeof ( *dbConnMsg );
	wsaBuf[0].buf = (char *)dbConnMsg;
	wsaBuf[1].len = sizeof ( *txMsg );
	wsaBuf[1].buf = (char *)txMsg;

	if ( data )
	{
		nWsaBuf = 3;
		wsaBuf[2].buf = (char *)data;
		wsaBuf[2].len = dataLen;
	}

	if ( WSASend ( db->conn->socket, wsaBuf, nWsaBuf, &nBytesSent, 0, 0, 0 ) == SOCKET_ERROR )
	{
		remoteDBResetConn ( db->conn );

		if ( WSASend ( db->conn->socket, wsaBuf, nWsaBuf, &nBytesSent, 0, 0, 0 ) == SOCKET_ERROR )
		{
			remoteDBFree ( db );
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
	}

	len = sizeof ( *rxMsg );
	while ( len )
	{
		wsaBuf[0].buf = (char *)rxMsg;
		wsaBuf[0].len = len;

		flags = 0;

		FD_ZERO ( &readFds );
		FD_SET ( db->conn->socket, &readFds );
		errorFds = readFds;

		rc = select ( 1, &readFds, 0, &errorFds, &vmConf.dbReqBigTimeout );

		if ( !rc )
		{
			remoteDBFree ( db );
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		if ( rc == SOCKET_ERROR )
		{
			remoteDBFree ( db );
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}

		if ( WSARecv ( db->conn->socket, wsaBuf, 1, &nBytesRcvd, &flags, 0, 0 ) )
		{
			remoteDBFree ( db );
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		if ( !nBytesRcvd )
		{
			remoteDBFree ( db );
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		len -= nBytesRcvd;
		rxMsg = (DBS_MSG *)((char *)rxMsg + nBytesRcvd);
	}

	return db ;
}

static REMOTE_DB *remoteRequestSpecial( unsigned long ip, int port, DB_CONNECT_MSG *dbConnMsg, DBS_MSG *txMsg, DBS_MSG *rxMsg, void *data, unsigned int dataLen )
{
	REMOTE_DB	*db;

	db = remoteDBAllocate();

	if ( !(db->conn = remoteAllocateConn ( ip, port )) )
	{
		remoteDBFree ( db );
		return 0 ;
	}
	return remoteRequestSpecialProcess ( db, dbConnMsg, txMsg, rxMsg, data, dataLen ) ;
}

static int 	remoteReadData ( REMOTE_DB *db, int len )

{
	WSABUF		 wsaBuf[1]{};
	DWORD		 nBytesRcvd;
	DWORD		 flags;
	fd_set		 readFds{};
	fd_set		 errorFds;
	int			 rc;

	bufferReset ( &db->buffer );
	bufferMakeRoom ( &db->buffer, len );

	while ( len )
	{
		FD_ZERO ( &readFds );
		FD_SET ( db->conn->socket, &readFds );
		errorFds = readFds;

		rc = select ( 1, &readFds, 0, &errorFds, &vmConf.dbReqTimeout );

		if ( !rc )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}
		if ( rc == SOCKET_ERROR )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}

		flags = 0;

		wsaBuf[0].buf = bufferBuff ( &db->buffer ) + bufferSize ( &db->buffer );
		wsaBuf[0].len =	len;

		if ( WSARecv ( db->conn->socket, wsaBuf, 1, &nBytesRcvd, &flags, 0, 0 ) )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}

		if ( !nBytesRcvd )
		{
			throw errorNum::scDATABASE_REQUEST_TIMEOUT;
		}

		bufferAssume ( &db->buffer, nBytesRcvd );
		len -= (int)nBytesRcvd;
	}

	return 1 ;
}

static int remoteDbGoCold ( REMOTE_DB *db )
{
	DBS_MSG		 recPut{};

	if ( db->isHot )
	{
		recPut.msgType = DBS_MSG_RECORD_PUT;
		recPut.dat.req.dbRecordPut.dataLen = (int)bufferSize ( &db->recordBuff );

		if ( !remoteRequest ( db, &recPut, &recPut, bufferBuff ( &db->recordBuff ), (int)bufferSize ( &db->recordBuff ) ) )
		{
			return 0 ;
		}

		if ( recPut.status )
		{
			throw (errorNum) recPut.status;
		}

		db->isHot = 0;
	}

	return 1 ;
}

static int remoteDbFieldGet ( REMOTE_DB *db, int num )
{
	DBS_MSG		 recGet{};
	size_t		 len;
	int			 offset;

#if 0
recGet.msgType = DBS_MSG_FIELD_GET;
recGet.dat.req.dbFieldGet.fieldNum = num;
remoteRequest ( db, &recGet, &recGet, 0, 0 );
remoteReadData ( db, recGet.dat.rsp.dbFieldGet.lenData );
return 1 ;
#endif

	if ( !db->isValid )
	{
		/* build a fieldGet structure for ourselves */
		recGet.msgType	= DBS_MSG_RECORD_GET;

		if ( !remoteRequest ( db, &recGet, &recGet, 0, 0 ) )
		{
			return 0 ;
		}
		if ( !remoteReadData ( db, recGet.dat.rsp.dbRecordGet.recSize ) )
		{
			return 0 ;
		}

		if ( bufferSize ( &db->buffer ) > bufferFree ( &db->buffer ) )
		{
			throw errorNum::scINTERNAL;
		}
		memcpy ( bufferBuff ( &db->recordBuff ), bufferBuff ( &db->buffer ), bufferSize ( &db->buffer ) );

		db->isValid = 1;
	}

	if ( (num > 0) && (num <= db->numFields) )
	{
		num--;

		switch(db->fields[num].type)
		{
			case 'B':
				recGet.msgType = DBS_MSG_FIELD_GET;
				recGet.dat.req.dbFieldGet.fieldNum = num +  1;

				if ( !remoteRequest ( db, &recGet, &recGet, 0, 0 ) )
				{
					return 0 ;
				}
				if ( !remoteReadData ( db, recGet.dat.rsp.dbFieldGet.lenData ) )
				{
					return 0 ;
				}
				return 1 ;
			case 'N' :
				len = (unsigned char)db->fields[num].leninfo.num.len;
				offset = static_cast<int>(db->fields[num].offset);
				break;
			case 'S':
				len = strlen ( (bufferBuff ( &db->recordBuff ) + db->fields[num].offset) ) + 1;
				offset = static_cast<int>(db->fields[num].offset);
				break;
			case 'C' :
			case 'D':
			case 'L':
			default:
				len = db->fields[num].leninfo.fieldlen;
				offset = static_cast<int>(db->fields[num].offset);
				break;
		}
		
		bufferReset ( &db->buffer );
		bufferWrite ( &db->buffer, bufferBuff ( &db->recordBuff ) + offset, len );
		return 1 ;
	} else
	{
		return 0 ;
	}
}

static int remoteDBMakeValid_ ( REMOTE_DB *db )
{
	DBS_MSG		 recGet{};

	// if we don't have the record data already then get it.
	if ( !db->isValid )
	{
		/* build a fieldGet structure for ourselves */
		recGet.msgType	= DBS_MSG_RECORD_GET;

		if ( !remoteRequest ( db, &recGet, &recGet, 0, 0 ) )
		{
			return 0 ;
		}
		if ( !remoteReadData ( db, recGet.dat.rsp.dbRecordGet.recSize ) )
		{
			return 0 ;
		}

		if ( bufferSize ( &db->buffer ) > bufferFree ( &db->recordBuff ) )
		{
			throw errorNum::scINTERNAL;
		}
		memcpy ( bufferBuff ( &db->recordBuff ), bufferBuff ( &db->buffer ), bufferSize ( &db->buffer ) );

		db->isValid = 1;
	}
	return 1 ;
}

static int remoteDbFieldPut_ ( REMOTE_DB *db, int num, char const *buff, int len )
{
	DBS_MSG		 fieldPut{};
	uint32_t	 fieldLen;
	char const	*buffer;

#if 0
	fieldPut.msgType = DBS_MSG_FIELD_PUT;
	fieldPut.dat.req.dbFieldGet.fieldNum	= num +  1;
	fieldPut.dat.req.dbFieldPut.dataLen		= len;

	remoteRequest ( db, &fieldPut, &fieldPut, buff, len );

	if ( fieldPut.status )
	{
		throw (errorNum)fieldPut.status;
	}
	return 1 ;
#endif

	// validate field number is in range
	if ( (num < 0) || (num > db->numFields) )
	{
		return 0 ;
	}

	// 0-align the field number
	num--;

	// check to see if we need to go hot (e.g. something changed)
	if ( !db->isHot )
	{
		db->isHot = 1;
	}

	// if we're hot then we need to udpate
	if ( db->isHot )
	{
		switch(db->fields[num].type)
		{
			case 'B':
				// don't buffer blobs
				fieldPut.msgType = DBS_MSG_FIELD_PUT;
				fieldPut.dat.req.dbFieldGet.fieldNum	= num +  1;
				fieldPut.dat.req.dbFieldPut.dataLen		= len;

				if ( !remoteRequest ( db, &fieldPut, &fieldPut, buff, len ) )
				{
					return 0 ;
				}

				if ( fieldPut.status )
				{
					throw (errorNum)fieldPut.status;
				}
				return 1 ;
			case 'N' :
				len = (int)strlen( (buffer = _firstchar ( const_cast<char *>(buff) )));

				if( len > db->fields[num].leninfo.fieldlen )
				{
					memset ( bufferBuff ( &db->recordBuff ) + db->fields[num].offset, '*', db->fields[num].leninfo.num.len);
					break;
				}
				
				memset( bufferBuff ( &db->recordBuff ) + db->fields[num].offset, ' ', db->fields[num].leninfo.num.len);
				memcpy( bufferBuff ( &db->recordBuff ) + db->fields[num].offset + db->fields[num].leninfo.num.len - len, buffer, len);
				break;
			case 'S':
				fieldLen = (uint32_t)strlen ( (bufferBuff ( &db->recordBuff ) + db->fields[num].offset) ) + 1;
				memcpyset ( bufferBuff ( &db->recordBuff ) + db->fields[num].offset, buff, fieldLen, len, ' ' );
				break;
			case 'L':
				*(bufferBuff ( &db->recordBuff ) + db->fields[num].offset) = ((*(unsigned char *) buff) | 0x20) == 't' ? (unsigned char)'T' : (unsigned char)'F';
				break;
			case 'C' :
			case 'D':
				fieldLen = db->fields[num].leninfo.fieldlen;
				memcpyset ( bufferBuff ( &db->recordBuff ) + db->fields[num].offset, buff, fieldLen, len, ' ' );
				break;
			default:
				if( len > db->fields[num].leninfo.fieldlen )
				{
					len = db->fields[num].leninfo.fieldlen;
				}

				memcpy( bufferBuff ( &db->recordBuff ) + db->fields[num].offset, buff, len );
				break;
		}
	}
	return 1 ;
}

static int remoteDbInvalidate ( REMOTE_DB *db )
{
	if ( db->isHot )
	{
		if ( !remoteDbGoCold ( db ) )
		{
			return 0 ;
		}
	}

	db->isValid = 0;
	return 1 ;
}

static int remoteDbFillFieldCache ( class vmInstance *instance, REMOTE_DB *db, char const *name, DBFIELDINFO *dbFieldInfo );

// this really SHUTSDOWN the remote engine... it does not release any resources (
static int remoteDbEnd ( void )
{
#if 0
	REMOTE_DB				*db;
	DBS_MSG					 dbsMsg;
	DB_CONNECT_MSG			 dbConnMsg;
	char					*userId;
	ENGINE_FGL_DB_LIST		*dbList;

	userId = "test";

//	EVP_Digest ( userId, strlen ( userId ), (unsigned char *)dbConnMsg.passHash, NULL, EVP_sha1(), NULL );
	strncpy_s ( dbConnMsg.userId, sizeof ( dbConnMsg.userId ), userId, _TRUNCATE );

	dbsMsg.msgType	= DBS_MSG_SHUTDOWN;

	// close any on ports other then 3102
	for ( dbList = vmConf.dbList; dbList; dbList = dbList->next )
	{
		if ( dbList->used )
		{
			if ( db = remoteRequestSpecialNC ( 0x7F000001, dbList->port, &dbConnMsg, &dbsMsg, &dbsMsg, 0, 0 ) )
			{
				remoteDBFree ( db );
			}
		}
	}
#endif
	return 1 ;
}

static int remoteDbCloseConn ( vmInstance *instance, REMOTE_DB *db )

{
	DBS_MSG		dbsMsg{};

	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	EnterCriticalSection ( &remoteConnCacheCtrl );

	if ( remoteConnCacheCnt > vmConf.dbMaxConnections )
	{
		LeaveCriticalSection ( &remoteConnCacheCtrl );

		// we already have enough cache connections
		dbsMsg.msgType	= DBS_MSG_CLOSE_DB_CONN;

		if ( !remoteRequest ( db, &dbsMsg, &dbsMsg, 0, 0 ) )
		{
			// failed... hard shutdown the database connection
			remoteDBFree ( db );
			instance->workareaTable->free ( db );

			return 0 ;
		}

		remoteDBFree ( db );

		instance->workareaTable->free ( db );

		if ( dbsMsg.status )
		{
			throw (errorNum)dbsMsg.status;
		}
	} else
	{
		LeaveCriticalSection ( &remoteConnCacheCtrl );

		dbsMsg.msgType	= DBS_MSG_RESET_CONN;

		if ( !remoteRequest ( db, &dbsMsg, &dbsMsg, 0, 0 ) )
		{
			// failed... hard shutdown the database connection
			remoteDBFree ( db );
			instance->workareaTable->free ( db );

			return 0 ;
		}

		if ( dbsMsg.status )
		{
			remoteDBFree ( db );
			instance->workareaTable->free ( db );

			if ( dbsMsg.status == int(errorNum::scDATABASE_HARD_CLOSE) )
			{
				throw (errorNum)dbsMsg.status;
			}
		} else
		{
			free ( db->fields );

			// add it to the free list
			EnterCriticalSection ( &remoteConnCacheCtrl );
			db->conn->next = remoteConnCache;
			remoteConnCache = db->conn;
			remoteConnCacheCnt++;
			LeaveCriticalSection ( &remoteConnCacheCtrl );

			EnterCriticalSection ( &dbRemoteListCtrl );
			db->next = dbRemoteList;
			dbRemoteList = db;
			LeaveCriticalSection ( &dbRemoteListCtrl );
			instance->workareaTable->free ( db );
		}
	}	
	
	return 1 ;
}

static int remoteDbAssign ( class vmInstance *instance, REMOTE_DB *db, char const *field, VAR *var, DBFIELDINFO *fInfo )

{
	int			 pos;
	char		*ptr;
	int			 fType;
	int			 fDec;
	int			 fLen;
	size_t		 len;
	int			 dec;
	int			 sign;
	int			 r;
	int64_t		 lTmp;
	double		 dTmp;
	char		 numBuff[33]{};
	DBS_MSG		 fieldInfo{};

	remoteDBMakeValid_ ( db );

	if ( fInfo )
	{
		if ( !fInfo->fieldNum || (db->cCtr != fInfo->cCtr) )
		{
			remoteDbFillFieldCache ( instance, db, field, fInfo );
			if ( !fInfo->fieldNum )
			{
				throw errorNum::scFIELD_NOT_FOUND;
			}
		}

		pos = fInfo->fieldNum;
		fType = fInfo->fieldType;
		if ( fType == 'N' )
		{
			fLen  = (unsigned char)fInfo->lenInfo.num.len;
			fDec  = (unsigned char)fInfo->lenInfo.num.dec;
		} else
		{
			fLen  = fInfo->lenInfo.fieldlen;
		}
	} else
	{
		fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
		_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, (char *)field, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );
		
		remoteDbFieldInfoByName ( db, &fieldInfo );
	
		pos   = fieldInfo.dat.rsp.dbFieldInfo.fieldNum;
		fType = (unsigned char)fieldInfo.dat.rsp.dbFieldInfo.fieldType;
		fLen  = fieldInfo.dat.rsp.dbFieldInfo.fieldLen;
		fDec  = fieldInfo.dat.rsp.dbFieldInfo.fieldDec;
	}

	bufferReset ( &db->buffer );

	switch ( fType )
	{
		case 'N':
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
				case slangType::eSTRING:
					switch ( TYPE ( var ) )
					{
						case slangType::eDOUBLE:
							dTmp = var->dat.d;
							break;
						case slangType::eLONG:
							dTmp = (double)var->dat.l;
							break;
						case slangType::eSTRING:
							dTmp = atof ( _firstchar ( var->dat.str.c ) );
							break;
						default:
							throw errorNum::scINVALID_PARAMETER;
					}
					
					*numBuff = 0;
					
					_snprintf_s ( numBuff, sizeof ( numBuff ), _TRUNCATE, "%*.*f", fLen, fDec, dTmp );

					bufferWrite ( &db->buffer, numBuff, fLen );
					bufferPut8 ( &db->buffer, (char)0 );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case 'L':
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					if ( var->dat.l )
					{
						bufferPut8 ( &db->buffer, 'T' );
					} else
					{
						bufferPut8 ( &db->buffer, 'F' );
					}
					break;
				case slangType::eDOUBLE:
					if ( var->dat.d != 0 )
					{
						bufferPut8 ( &db->buffer, 'T' );
					} else
					{
						bufferPut8 ( &db->buffer, 'F' );
					}
					break;
				case slangType::eSTRING:
					switch ( *(var->dat.str.c) )
					{
						case 'T':
						case 't':
						case 'Y':
						case 'y':
						case '1':
							bufferPut8 ( &db->buffer, 'T' );
							break;
						default:
							bufferPut8 ( &db->buffer, 'F' );
							break;
					}
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			bufferPut8 ( &db->buffer, (char)0 );
			break;
		case 'S':
			switch ( TYPE ( var ) )
			{
				case slangType::eDOUBLE:
				case slangType::eLONG:
					if ( TYPE ( var ) == slangType::eDOUBLE )
					{
						ptr   = _ecvt ( var->dat.d, 20, &dec, &sign );
						
						*numBuff = 0;
						if ( sign )
						{
							strcpy ( numBuff, "-" );
						}
						if ( dec > 0 )
						{
							strncat ( numBuff, ptr, dec );
							strncat ( numBuff, ".", 1 );
							strcat_s  ( numBuff, sizeof ( numBuff ), ptr + dec );
						} else	if ( dec < 0 )
						{
							strncat ( numBuff, ptr, 1 );
							strncat ( numBuff, ".", 1 );
							strcat_s  ( numBuff, sizeof ( numBuff ), ptr + 1 );
						} else
						{
							strncat ( numBuff, "0.", 2 );
							strcat_s ( numBuff, sizeof ( numBuff ), ptr + dec );
						}
						
						len = strlen ( numBuff ) - 1;
						while ( ((int)len > dec) && (*(numBuff + len) == '0') )
						{
							len--;
						}
						if ( *(numBuff + len) == '.' )
						{
							len++;
							*(numBuff + len) = 0;
							strcat_s ( numBuff, sizeof ( numBuff ), "0" );
						} else
						{
							len++;
							*(numBuff + len) = 0;
						}
						
						if ( dec < 0 )
						{
							strcat_s ( numBuff, sizeof ( numBuff ), "e" );
							_itoa ( dec - 1, numBuff + strlen ( numBuff ), 10 );
						}
					} else
					{
						_i64toa ( var->dat.l, numBuff, 10 );
					}
					
					numBuff[fLen] = 0;
					
					bufferWrite ( &db->buffer, numBuff, strlen ( numBuff ) + 1 );
					break;
				case slangType::eSTRING:
					bufferWrite ( &db->buffer, var->dat.str.c, var->dat.str.len );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case 'F':
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					dTmp = (double)var->dat.l;
					break;
				case slangType::eDOUBLE:
					dTmp = var->dat.d;
					break;
				case slangType::eSTRING:
					dTmp = atof ( _firstchar ( var->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			bufferWrite ( &db->buffer, &dTmp, sizeof ( double ) );
			break;
		case '4':
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					lTmp = var->dat.l;
					break;
				case slangType::eDOUBLE:
					lTmp = (long)(var->dat.d);
					break;
				case slangType::eSTRING:
					lTmp = atol ( _firstchar ( var->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			bufferPut8 ( &db->buffer, (uint32_t)lTmp );
			break;
		case '2':
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					lTmp = var->dat.l;
					break;
				case slangType::eDOUBLE:
					lTmp = (long)(var->dat.d);
					break;
				case slangType::eSTRING:
					lTmp = atol ( _firstchar ( var->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			bufferPut16 ( &db->buffer, (unsigned short)lTmp );
			break;
		case '1':
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					lTmp = var->dat.l;
					break;
				case slangType::eDOUBLE:
					lTmp = (long)(var->dat.d);
					break;
				case slangType::eSTRING:
					lTmp = atol ( _firstchar ( var->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			bufferPut8 ( &db->buffer, (char)lTmp );
			break;
		case 'D':
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					lTmp = var->dat.l;
					break;
				case slangType::eDOUBLE:
					lTmp = (long)(var->dat.d);
					break;
				case slangType::eSTRING:
					lTmp = datej ( _firstchar ( var->dat.str.c ) );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			bufferWrite ( &db->buffer, &lTmp, sizeof ( long ) );
			break;
		case 'C':
			switch ( TYPE ( var ) )
			{
				case slangType::eDOUBLE:
				case slangType::eLONG:
					if ( TYPE ( var ) == slangType::eDOUBLE )
					{
						ptr   = _ecvt ( var->dat.d, 20, &dec, &sign );
						
						*numBuff = 0;
						if ( sign )
						{
							strcpy ( numBuff, "-" );
						}
						if ( dec > 0 )
						{
							strncat ( numBuff, ptr, dec );
							strncat ( numBuff, ".", 1 );
							strcat_s ( numBuff, sizeof ( numBuff ), ptr + dec );
						} else if ( dec < 0 )
						{
							strncat ( numBuff, ptr, 1 );
							strncat ( numBuff, ".", 1 );
							strcat_s  ( numBuff, sizeof ( numBuff ), ptr + 1 );
						} else
						{
							strncat ( numBuff, "0.", 2 );
							strcat_s  ( numBuff, sizeof ( numBuff ), ptr + dec );
						}
						
						len = strlen ( numBuff ) - 1;
						while ( ((int)len > dec) && (*(numBuff + len) == '0') )
						{
							len--;
						}
						if ( *(numBuff + len) == '.' )
						{
							len++;
							*(numBuff + len) = 0;
							strcat_s ( numBuff, sizeof ( numBuff ), "0" );
						} else
						{
							len++;
							*(numBuff + len) = 0;
						}
						
						if ( dec < 0 )
						{
							strcat_s ( numBuff, sizeof ( numBuff ), "e" );
							_itoa ( dec - 1, numBuff + strlen ( numBuff ), 10 );
						}
					} else
					{
						_i64toa ( var->dat.l, numBuff, 10);
					}
					
					len = strlen ( numBuff );
					
					if ( (int)len < (r = fLen) )
					{
						db->buffer.makeRoom ( r );
						memset ( db->buffer.data<char *> (), ' ', r );
						memcpy ( db->buffer.data<char *> (), numBuff, len );
						db->buffer.assume ( r );
					} else
					{
						bufferWrite ( &db->buffer, numBuff, fLen );
					}
					break;
				case slangType::eSTRING:
					bufferWrite ( &db->buffer, var->dat.str.c, var->dat.str.len );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		case 'B':
			switch ( TYPE ( var ) )
			{
				case slangType::eSTRING:
					bufferWrite ( &db->buffer, var->dat.str.c, var->dat.str.len );
					break;
				case slangType::eNULL:
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
		default:
			switch ( TYPE ( var ) )
			{
				case slangType::eSTRING:
					bufferWrite ( &db->buffer, var->dat.str.c, var->dat.str.len );
					break;
				default:
					throw errorNum::scTYPE_CONVERSION;
			}
			break;
	}

	if ( !remoteDbFieldPut_ ( db, pos, bufferBuff ( &db->buffer ), (int)bufferSize ( &db->buffer ) ) )
	{
		throw 0;
	}
#if 0
	remoteRequest ( db, &fieldPut, &fieldPut, bufferBuff ( &db->buffer ), bufferSize ( &db->buffer ) );

	if ( fieldPut.status )
	{
		throw fieldPut.status;
	}
#endif
	return 1 ;
}

static VAR *remoteDbAccess ( class vmInstance *instance, REMOTE_DB *db, char const *field, VAR *resVal, DBFIELDINFO *fInfo )

{
	int			 fLen;
	int			 fType;
	double		 dTmp;
	char		*res;
	char		 dateTmp[9]{};
	char		 numBuff[31]{};
	DBS_MSG		 fieldInfo{};
	char		*fieldData;

	if ( fInfo )
	{
		if ( !fInfo->fieldNum || (db->cCtr != fInfo->cCtr) )
		{
			remoteDbFillFieldCache ( instance, db, field, fInfo );
			if ( !fInfo->fieldNum )
			{
				throw errorNum::scFIELD_NOT_FOUND;
			}
		}
		fType = fInfo->fieldType;

		/* build a fieldGet structure for ourselves */
		if ( !remoteDbFieldGet ( db, fInfo->fieldNum ) )
		{
			throw errorNum::scFIELD_NOT_FOUND;
		}

		fLen = (int)bufferSize ( &db->buffer );
	} else
	{
		fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
		_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName,(char *) field, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

		remoteDbFieldInfoByName ( db, &fieldInfo );

		fType = (unsigned char)fieldInfo.dat.rsp.dbFieldInfo.fieldType;

		if ( !remoteDbFieldGet ( db, fieldInfo.dat.rsp.dbFieldInfo.fieldNum ) )
		{
			throw errorNum::scFIELD_NOT_FOUND;
		}

		fLen = (int)bufferSize ( &db->buffer );
	}

	fieldData = bufferBuff ( &db->buffer );

	switch ( fType )
	{
		case '4':
		case '2':
		case '1':
			resVal->type  = slangType::eLONG;
			resVal->dat.l = (unsigned char)*(fieldData);
			break;
		case 'F':
			resVal->type  = slangType::eDOUBLE;
			resVal->dat.d = *((double *)fieldData);
			break;
		case 'N':					
			fLen = (int)(fLen < sizeof ( numBuff ) - 1 ? fLen : sizeof ( numBuff ) - 1 );
			
			memcpy ( numBuff, fieldData, fLen );
			numBuff[fLen] = 0;
			
			resVal->type  = slangType::eDOUBLE;
			resVal->dat.d = atof ( _firstchar ( numBuff ) );
			
			if ( (resVal->dat.d < (double) 0x7FFFFFFF) && (modf ( resVal->dat.d, &dTmp ) == 0) )
			{
				resVal->type  = slangType::eLONG;
				resVal->dat.l = (long)resVal->dat.d;
			}
			break;
		case 'S':
			res = (char *)instance->om->alloc ( sizeof ( char ) * (static_cast<size_t>(fLen) + 1) );
			memcpy ( res, fieldData, fLen );
			res[fLen] = 0;
			
			resVal->type        = slangType::eSTRING;
			resVal->dat.str.len = strlen ( res ); // len;
			resVal->dat.str.c   = res;
			break;
		case 'L':
			resVal->type  = slangType::eLONG;
			resVal->dat.l = (*fieldData == 'T');
			break;
		case 'D':
			resVal->type  = slangType::eLONG;
			resVal->dat.str.len   = 0;
			resVal->dat.l =datej ( dateTmp );	/* MISSING */
			break;
		case 'B':
		default:
			res = (char *)instance->om->alloc ( sizeof ( char ) * (static_cast<size_t>(fLen) + 1) );
			
			memcpy ( res, fieldData, fLen );
			res[fLen] = 0;
			
			resVal->type        = slangType::eSTRING;
			resVal->dat.str.len = fLen; // strlen ( res );
			resVal->dat.str.c   = res;
			break;
	}
	return resVal ;
}

static void readCB ( class vmInstance *instance, void *db, char const *field, VAR *dest )
{
	remoteDbAccess ( instance, (REMOTE_DB *)db, field, dest, 0 );
}

static void writeCB ( class vmInstance *instance, void *db, char const *field, VAR *value )
{
	remoteDbAssign ( instance, (REMOTE_DB *)db, field, value, 0 );
}

static void closeCB ( class vmInstance *instance, void *db )
{
	remoteDbCloseConn ( instance, (REMOTE_DB *)db );
}

static int64_t remoteDbOpen ( class vmInstance *instance, char const *fName, char const *alias, int isNew )
{
	REMOTE_DB				*db;
	DB_CONNECT_MSG			 dbConnMsg{};
	DBS_MSG					 dbsMsg{};
	char const				*userId;
	int						 ip;
	int						 port;

	userId = "test";

	auto tableIt = vmConf.dbList.find(fName);
	
	if ( tableIt == vmConf.dbList.end() )
	{
		// not found
		ip = (int)vmConf.dbDefaultDb.ip;
		port = (int)vmConf.dbDefaultDb.port;
		vmConf.dbDefaultDb.used = true;
	} else
	{
		ip = (int)tableIt->second.ip;
		port = (int)tableIt->second.port;
		tableIt->second.used = true;
	}

//	EVP_Digest ( userId, strlen ( userId ), (unsigned char *)dbConnMsg.passHash, NULL, EVP_sha1(), NULL );
	strncpy_s ( dbConnMsg.userId, sizeof ( dbConnMsg.userId ), userId, _TRUNCATE );

	dbsMsg.msgType = DBS_MSG_OPEN_DB;
	strncpy_s ( dbsMsg.dat.req.dbOpen.tableName, sizeof ( dbsMsg.dat.req.dbOpen.tableName ), fName, _TRUNCATE );

	if ( !(db = remoteRequestSpecial ( ip, port, &dbConnMsg, &dbsMsg, &dbsMsg, 0, 0 )) )
	{
		throw errorNum::scDATABASE_REQUEST_TIMEOUT;
	}

	if ( dbsMsg.status )
	{
		remoteDBFree ( db );
		throw (errorNum)dbsMsg.status;
	}

	db->isHot		= 0;
	db->isValid		= 0;
	db->fields		= (DBFFIELDS *)malloc ( sizeof ( DBFFIELDS ) * dbsMsg.dat.rsp.dbInfo.fieldCount );
	db->numFields	= dbsMsg.dat.rsp.dbInfo.fieldCount;
	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbsMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		remoteDBFree ( db );
		return 0 ;
	}

	memcpy ( db->fields, bufferBuff ( &db->buffer ), sizeof ( DBFFIELDS ) * dbsMsg.dat.rsp.dbInfo.fieldCount );

	// make sure record buff can contain the entire record
	bufferReset ( &db->recordBuff );

	switch ( db->fields[db->numFields-1].type )
	{
		case 'N':
			bufferMakeRoom ( &db->recordBuff, static_cast<size_t>(db->fields[static_cast<size_t>(db->numFields) -1].offset) + db->fields[static_cast<size_t>(db->numFields) - 1].leninfo.num.len );
			bufferAssume ( &db->recordBuff, static_cast<size_t>(db->fields[static_cast<size_t>(db->numFields)-1].offset) + db->fields[static_cast<size_t>(db->numFields) - 1].leninfo.num.len );
			break;
		default:
			bufferMakeRoom ( &db->recordBuff, static_cast<size_t>(db->fields[static_cast<size_t>(db->numFields)-1].offset) + db->fields[static_cast<size_t>(db->numFields) - 1].leninfo.fieldlen );
			bufferAssume ( &db->recordBuff, static_cast<size_t>(db->fields[static_cast<size_t>(db->numFields)-1].offset) + db->fields[static_cast<size_t>(db->numFields) - 1].leninfo.fieldlen );
			break;
	}

	DBS_MSG		 dbMsg{};
	dbMsg.msgType = DBS_MSG_GET_STATUS;
	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0;
	}
	db->status = dbMsg.dat.rsp.dbStatus;

	return instance->workareaTable->alloc ( alias, db, readCB, writeCB, closeCB );
}

static int64_t remoteDbCreate ( class vmInstance *instance, char const *fName, VAR *var )

{
	TBFIELD			*fields;
	int64_t			 numFields;
	int64_t			 ind[3]{};
	VAR				*var2;
	REMOTE_DB		*db;
	DB_CONNECT_MSG	 dbConnMsg{};
	DBS_MSG			 dbsMsg{};
	char const		*userId;
	uint32_t		 ip;
	uint32_t		 port;

	if ( TYPE ( var ) != slangType::eARRAY_ROOT )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	var = var->dat.ref.v;

	switch ( TYPE ( var ) )
	{
		case slangType::eARRAY_SPARSE:	
			numFields = var->dat.aSparse.maxI;
			ind[0]    = 1;
			break;
		case slangType::eARRAY_FIXED:
			numFields = var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1;
			ind[0] = var->dat.arrayFixed.startIndex;
			break;
		default:
			throw errorNum::scINTERNAL;
			break;
	}
	
	if ( !(fields = (TBFIELD *)malloc ( sizeof ( TBFIELD ) * numFields )) )
	{
		throw errorNum::scMEMORY;
	}
	
	ind[0]    = 1;
	numFields = 0;
	
	while ( 1 )
	{
		ind[1] = 1;
		
		if ( !_arrayIsElem ( instance, var, 2, ind ) )
		{
			break;
		}
		
		if ( !(var2 = _arrayGet ( instance, var, 2, ind )) )
		{
			free ( fields );
			throw errorNum::scINVALID_PARAMETER;
		}
		
		if ( !var2 || (TYPE ( var2 ) != slangType::eSTRING) )
		{
			free ( fields );
			throw errorNum::scINVALID_PARAMETER;
		} else
		{
			if ( var2->dat.str.len > TB_NAMESIZE )
			{
				free ( fields );
				throw errorNum::scINVALID_PARAMETER;
			}
			
			memset ( fields[numFields].name, ' ', TB_NAMESIZE );
			fields[numFields].name[TB_NAMESIZE-1] = 0;
			memcpy ( fields[numFields].name, var2->dat.str.c, var2->dat.str.len );
		}
		
		ind[1]++;
		if ( !(var2 = _arrayGet ( instance, var, 2, ind )) )
		{
			free ( fields );
			throw 0;
		}
		
		if ( !var2 || (TYPE ( var2 ) != slangType::eSTRING) )
		{
			free ( fields );
			throw errorNum::scINVALID_PARAMETER;
		}
		
		if ( !var2->dat.str.len )
		{
			free ( fields );
			throw errorNum::scINVALID_PARAMETER;
		}
		
		if ( _chrany ( "CNS124BDdL~", var2->dat.str.c ) )
		{
			fields[numFields].type = *(var2->dat.str.c);
		} else
		{
			free ( fields );
			throw errorNum::scINVALID_FIELD;
		}
		
		if ( _chrany ( "CS", var2->dat.str.c ) )
		{
			ind[1]++;
			if ( !(var2 = _arrayGet ( instance, var, 2, ind )) )
			{
				free ( fields );
				throw 0;
			}
			
			if ( !var2 || (TYPE ( var2 ) != slangType::eLONG) )
			{
				free ( fields );
				throw errorNum::scINVALID_PARAMETER;
			}
			
			fields[numFields].length = (unsigned) var2->dat.l;
		} else if ( _chrany ( "N", var2->dat.str.c ) )
		{
			ind[1]++;
			if ( !(var2 = _arrayGet ( instance, var, 2, ind )) )
			{
				free ( fields );
				throw 0;
			}
			
			if ( !var2 || (TYPE ( var2 ) != slangType::eLONG) )
			{
				free ( fields );
				throw errorNum::scINVALID_PARAMETER;
			}
			
			fields[numFields].length = (unsigned) var2->dat.l;
			
			ind[1]++;
			if ( !(var2 = _arrayGet ( instance, var, 2, ind )) )
			{
				free ( fields );
				throw 0;
			}
			
			if ( !var2 || (TYPE ( var2 ) != slangType::eLONG) )
			{
				free ( fields );
				throw errorNum::scINVALID_PARAMETER;
			}
			
			fields[numFields].decimals = (unsigned) var2->dat.l;
		}
		ind[0]++;
		numFields++;
	}

	userId = "test";

	auto tableIt  = vmConf.dbList.find ( fName );

	if ( tableIt == vmConf.dbList.end() )
	{
		// not found
		ip = vmConf.dbDefaultDb.ip;
		port = vmConf.dbDefaultDb.port;
		vmConf.dbDefaultDb.used = true;
	} else
	{
		ip = tableIt->second.ip;
		port = tableIt->second.port;
		tableIt->second.used = true;
	}

	// connection header
//	EVP_Digest ( userId, strlen ( userId ), (unsigned char *)dbConnMsg.passHash, NULL, EVP_sha1(), NULL );
	strncpy_s ( dbConnMsg.userId, sizeof ( dbConnMsg.userId ), userId, _TRUNCATE );

	// creation message
	dbsMsg.msgType = DBS_MSG_CREATE_DB;
	strncpy_s ( dbsMsg.dat.req.dbCreateDB.fName, sizeof ( dbsMsg.dat.req.dbCreateDB.fName ), fName, MAX_NAME_SZ );
	dbsMsg.dat.req.dbCreateDB.numFields = (int)numFields;

	if ( !(db = remoteRequestSpecial ( ip, (int)port, &dbConnMsg, &dbsMsg, &dbsMsg, fields, (unsigned int)(sizeof ( TBFIELD) * numFields) )) )
	{
		free ( fields );
		throw errorNum::scDATABASE_REQUEST_TIMEOUT;
	}
	free ( fields );
	closesocket ( db->conn->socket );

	remoteDBFree ( db );

	if ( dbsMsg.status )
	{
		throw (errorNum)dbsMsg.status;
	}
	return 1;
}

static int64_t remoteDbOpenMulti ( class vmInstance *instance, char const *fName, VAR *indexList, char const *alias, int isNew )
{
	BUFFER					 indexBuff;
	VAR						*elem;
	int64_t					 ind[1]{ 1 };
	REMOTE_DB				*db;
	DB_CONNECT_MSG			 dbConnMsg{};
	DBS_MSG					 dbsMsg{};
	char const				*userId;
	uint32_t				 ip;
	uint32_t				 port;

	uint64_t nIndex = 0;
	while ( 1 )
	{
		if ( !_arrayIsElem ( instance, indexList, 1, ind ) )
		{
			break;
		}

		elem = _arrayGet ( instance, indexList, 1, ind );
		while ( TYPE ( elem ) == slangType::eREFERENCE )
		{
			elem = elem->dat.ref.v;
		}

		if ( TYPE ( elem ) != slangType::eSTRING )
		{
			throw errorNum::scINVALID_PARAMETER;
		}

		bufferWrite ( &indexBuff, elem->dat.str.c, elem->dat.str.len + 1);

		ind[0]++;
		nIndex++;
	}
	bufferPut8( &indexBuff, (char)0 );

	userId = "test";

	auto tableIt = vmConf.dbList.find ( fName );
	if ( tableIt == vmConf.dbList.end() )
	{
		// not found
		ip = vmConf.dbDefaultDb.ip;
		port = vmConf.dbDefaultDb.port;
		vmConf.dbDefaultDb.used = true;
	} else
	{
		ip = tableIt->second.ip;
		port = tableIt->second.port;
		tableIt->second.used = true;
	}

//	EVP_Digest ( userId, strlen ( userId ), (unsigned char *)dbConnMsg.passHash, NULL, EVP_sha1(), NULL );
	strncpy_s ( dbConnMsg.userId, sizeof ( dbConnMsg.userId ), userId, _TRUNCATE );

	dbsMsg.msgType = DBS_MSG_OPEN_DB_MULTI;
	strncpy_s ( dbsMsg.dat.req.dbOpen.tableName, sizeof ( dbsMsg.dat.req.dbOpen.tableName ), fName, _TRUNCATE );

	dbsMsg.dat.req.dbOpenMulti.nIndex = (int)nIndex;
	dbsMsg.dat.req.dbOpenMulti.dataLen = (unsigned int)bufferSize ( &indexBuff );

	if ( !(db = remoteRequestSpecial ( ip, (int)port, &dbConnMsg, &dbsMsg, &dbsMsg, bufferBuff ( &indexBuff ), (int)bufferSize ( &indexBuff ) )) )
	{
		throw errorNum::scDATABASE_REQUEST_TIMEOUT;
	}

	if ( dbsMsg.status )
	{
		remoteDBFree ( db );
		throw (errorNum)dbsMsg.status;
	}

	db->isHot		= 0;
	db->isValid		= 0;
	db->fields		= (DBFFIELDS *)malloc ( sizeof ( DBFFIELDS ) * dbsMsg.dat.rsp.dbInfo.fieldCount );
	if ( !db->fields ) throw errorNum::scMEMORY;
	db->numFields	= dbsMsg.dat.rsp.dbInfo.fieldCount;
	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbsMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		remoteDBFree ( db );
		return 0 ;
	}

	memcpy ( db->fields, bufferBuff ( &db->buffer ), sizeof ( DBFFIELDS ) * dbsMsg.dat.rsp.dbInfo.fieldCount );

	// make sure record buff can contain the entire record
	bufferReset ( &db->recordBuff );

	switch ( db->fields[db->numFields-1].type )
	{
		case 'N':
			bufferMakeRoom ( &db->recordBuff, static_cast<size_t>(db->fields[db->numFields-1].offset) + db->fields[db->numFields - 1].leninfo.num.len );
			bufferAssume ( &db->recordBuff, static_cast<size_t>(db->fields[db->numFields-1].offset) + db->fields[db->numFields - 1].leninfo.num.len );
			break;
		default:
			bufferMakeRoom ( &db->recordBuff, static_cast<size_t>(db->fields[db->numFields-1].offset) + db->fields[db->numFields - 1].leninfo.fieldlen );
			bufferAssume ( &db->recordBuff, static_cast<size_t>(db->fields[db->numFields-1].offset) + db->fields[db->numFields - 1].leninfo.fieldlen );
			break;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_GET_STATUS };
	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0;
	}
	db->status = dbMsg.dat.rsp.dbStatus;

	return instance->workareaTable->alloc ( alias, db, readCB, writeCB, closeCB );
}

static int remoteDbOpenIndex ( class vmInstance *instance, REMOTE_DB *db, char const *fName )

{
	DBS_MSG		 dbMsg{ DBS_MSG_OPEN_INDEX };

	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	strncpy_s ( dbMsg.dat.req.dbOpenIndex.fName, MAX_NAME_SZ, fName, _TRUNCATE );

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( dbMsg.status )
	{
		throw (errorNum)dbMsg.status;
	}

	return 1 ;
}

static int remoteDbCloseIndex ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_CLOSE_INDEX };

	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( dbMsg.status )
	{
		throw (errorNum)dbMsg.status;
	}

	return 1 ;
}

static long remoteDbSkip ( class vmInstance *instance, REMOTE_DB *db, int num )

{
	DBS_MSG		 dbMsg{ DBS_MSG_SKIP };
	
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	dbMsg.dat.req.dbSkip.count = num;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	db->status = dbMsg.dat.rspStatus;

	return dbMsg.status ;
}

static long remoteDbSeek ( class vmInstance *instance, REMOTE_DB *db, VAR *var, int softSeek )
{
	void const	*seekPtr;
	size_t		 seekLen;
	
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

 	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}

	switch ( TYPE ( var ) )
	{
		case slangType::eSTRING:
			seekPtr = var->dat.str.c;
			seekLen = var->dat.str.len;
			break;
		case slangType::eLONG:
			seekPtr = &var->dat.l;
			seekLen = sizeof ( long );
			break;
		case slangType::eDOUBLE:
			seekPtr = &var->dat.d;
			seekLen = sizeof ( double );
			break;
		default:
			throw errorNum::scINVALID_KEY;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_SEEK };
	dbMsg.dat.req.dbSeek.dataLen	= (int)seekLen;
	dbMsg.dat.req.dbSeek.softSeek	= softSeek;

	if ( !remoteRequestLong ( db, &dbMsg, &dbMsg, seekPtr, (uint32_t)seekLen ) )
	{
		return 0 ;
	}

	db->status = dbMsg.dat.rspStatus;

	return dbMsg.status ;
}

static long remoteDbSeekLast ( class vmInstance *instance, REMOTE_DB *db, VAR *var, int softSeek )
{
	void const	*seekPtr;
	size_t		 seekLen;

	if ( !remoteDbInvalidate ( db ) )
	{
		return 0;
	}

	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}

	switch ( TYPE ( var ) )
	{
		case slangType::eSTRING:
			seekPtr = var->dat.str.c;
			seekLen = var->dat.str.len;
			break;
		case slangType::eLONG:
			seekPtr = &var->dat.l;
			seekLen = sizeof ( long );
			break;
		case slangType::eDOUBLE:
			seekPtr = &var->dat.d;
			seekLen = sizeof ( double );
			break;
		default:
			throw errorNum::scINVALID_KEY;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_SEEK_LAST };
	dbMsg.dat.req.dbSeek.dataLen = (int)seekLen;
	dbMsg.dat.req.dbSeek.softSeek = softSeek;

	if ( !remoteRequestLong ( db, &dbMsg, &dbMsg, seekPtr, (uint32_t)seekLen ) )
	{
		return 0;
	}

	db->status = dbMsg.dat.rspStatus;

	return dbMsg.status;
}

static int remoteDbCmpKey ( class vmInstance *instance, REMOTE_DB *db, VAR *var )
{
	void const	*seekPtr;
	size_t		 seekLen;
	
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

 	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}

	switch ( TYPE ( var ) )
	{
		case slangType::eSTRING:
			seekPtr = var->dat.str.c;
			seekLen = var->dat.str.len;
			break;
		case slangType::eLONG:
			seekPtr = &var->dat.l;
			seekLen = sizeof ( long );
			break;
		case slangType::eDOUBLE:
			seekPtr = &var->dat.d;
			seekLen = sizeof ( double );
			break;
		default:
			throw errorNum::scINVALID_KEY;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_CMP_KEY };
	dbMsg.dat.req.dbCmpKey.dataLen	= (int)seekLen;

	if ( !remoteRequestLong ( db, &dbMsg, &dbMsg, seekPtr, (uint32_t)seekLen ) )
	{
		return 0 ;
	}

	return dbMsg.status ;
}

static long remoteDbFindSubset ( class vmInstance *instance, REMOTE_DB *db, VAR *var, int every )
{
	void const	*seekPtr;
	size_t		 seekLen;
	int			 loop;
	long		*recArray;
	
	if ( !remoteDbGoCold ( db ) )
	{
		return 0 ;
	}

	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}
	
	switch ( TYPE ( var ) )
	{
		case slangType::eSTRING:
			seekPtr = var->dat.str.c;
			seekLen = var->dat.str.len;
			break;
		case slangType::eLONG:
			seekPtr = &var->dat.l;
			seekLen = sizeof ( long );
			break;
		case slangType::eDOUBLE:
			seekPtr = &var->dat.d;
			seekLen = sizeof ( double );
			break;
		default:
			throw errorNum::scINVALID_KEY;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_FIND_SUBSET };
	dbMsg.dat.req.dbFindSubset.dataLen	= (int)seekLen;
	dbMsg.dat.req.dbFindSubset.every	= every;

	if ( !remoteRequestLong ( db, &dbMsg, &dbMsg, seekPtr, (uint32_t)seekLen ) )
	{
		return 0 ;
	}

	if ( !dbMsg.status )
	{
		if ( !remoteReadData ( db, (int)sizeof ( long ) * dbMsg.dat.rsp.dbFindSubset.nRecords ) )
		{
			return 0 ;
		}

		recArray = (long *)bufferBuff ( &db->buffer );

		/* got our records... now build an array containing them */
		arrayFixed2 ( instance, 1, dbMsg.dat.rsp.dbFindSubset.nRecords, VAR_NULL() );

		/* stick each array entry with one */

		/* get the array into var */
		var = instance->result.dat.ref.v;

		for ( loop = 0; loop < dbMsg.dat.rsp.dbFindSubset.nRecords; loop++ )
		{
			var[loop + 1].type		= slangType::eLONG;
			var[loop + 1].dat.l	= recArray[loop];
		}
	}

	return dbMsg.status ;
}

static long remoteDbCountMatching ( class vmInstance *instance, REMOTE_DB *db, VAR *var )
{
	void const	*seekPtr;
	size_t		 seekLen;
		
	if ( !remoteDbGoCold ( db ) )
	{
		return 0 ;
	}

	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}
	
	switch ( TYPE ( var ) )
	{
		case slangType::eSTRING:
			seekPtr = var->dat.str.c;
			seekLen = var->dat.str.len;
			break;
		case slangType::eLONG:
			seekPtr = &var->dat.l;
			seekLen = sizeof ( long );
			break;
		case slangType::eDOUBLE:
			seekPtr = &var->dat.d;
			seekLen = sizeof ( double );
			break;
		default:
			throw errorNum::scINVALID_KEY;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_COUNT_MATCHING };
	dbMsg.dat.req.dbCountMatching.dataLen	= (int)seekLen;

	if ( !remoteRequestLong (db, &dbMsg, &dbMsg, seekPtr, (uint32_t)seekLen ) )
	{
		return 0 ;
	}

	if ( !dbMsg.status )
	{
		return dbMsg.dat.rsp.dbCountMatching.nRecords ;
	} else
	{
		return 0 ;
	}
}

static long remoteDbGoto ( class vmInstance *instance, REMOTE_DB *db, long rec )
{
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_GOTO };
	dbMsg.dat.req.dbGoto.recNo = rec;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	db->status = dbMsg.dat.rspStatus;

	return dbMsg.status ;
}

static long remoteDbGoTop ( class vmInstance *instance, REMOTE_DB *db )
{
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_GO_TOP };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	db->status = dbMsg.dat.rspStatus;

	return dbMsg.status ;
}

static long remoteDbGoBottom ( class vmInstance *instance, REMOTE_DB *db )
{
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_GO_BOTTOM };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	db->status = dbMsg.dat.rspStatus;

	return dbMsg.status ;
}

static long remoteDbEOF ( class vmInstance *instance, REMOTE_DB *db )
{
#if defined ( USE_CACHE )
	return db->status.isEof;
#else
	DBS_MSG		 dbMsg;

	dbMsg.msgType = DBS_MSG_GET_STATUS;


	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbStatus.isEof ;
#endif
}

static long remoteDbBOF ( class vmInstance *instance, REMOTE_DB *db )
{
#if defined ( USE_CACHE )
	return db->status.isBof;
#else
	DBS_MSG		 dbMsg;

	dbMsg.msgType = DBS_MSG_GET_STATUS;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbStatus.isBof ;
#endif
}

static int remoteDbCachePolicy ( class vmInstance *instance, REMOTE_DB *db, int policy )
{
	DBS_MSG		 dbMsg{ DBS_MSG_CACHE_POLICY };
	dbMsg.dat.req.dbCachePolicy.policy = policy;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( dbMsg.status )
	{
		throw (errorNum)dbMsg.status;
	}
	return 1 ;
}

static int remoteDbCreateIndex ( class vmInstance *instance, REMOTE_DB *db, char const *name, char const *key, int isDescending )
{
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_CREATE_INDEX };
	strncpy_s ( dbMsg.dat.req.dbCreateIndex.fName, MAX_NAME_SZ, name, _TRUNCATE );
	dbMsg.dat.req.dbCreateIndex.isDescending = isDescending;
	dbMsg.dat.req.dbCreateIndex.dataLen = (int)strlen ( key );

	if ( !remoteRequestLong ( db, &dbMsg, &dbMsg, key, dbMsg.dat.req.dbCreateIndex.dataLen ) )
	{
		return 0 ;
	}

	if ( dbMsg.status )
	{
		throw (errorNum)dbMsg.status;
	}
	return 1 ;
}

static int remoteDbOrder ( class vmInstance *instance, REMOTE_DB *db, VAR *order )
{
	while ( TYPE ( order ) == slangType::eREFERENCE )
	{
		order = order->dat.ref.v;
	}

	if ( TYPE ( order ) != slangType::eLONG )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	
	DBS_MSG		 dbMsg{ DBS_MSG_SET_ORDER };
	dbMsg.dat.req.dbOrder.orderNum = (int)order->dat.l;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbOrder.oldOrder ;
}

static long remoteDbAppend ( class vmInstance *instance, REMOTE_DB *db )
{
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_APPEND };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	db->status = dbMsg.dat.rspStatus;

	return dbMsg.status ;
}

static int remoteDbFieldCount ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_INFO };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbInfo.fieldCount ;
}

static int remoteDbFieldPos ( class vmInstance *instance, REMOTE_DB *db, char const *name )
{
	DBS_MSG		 fieldInfo{ DBS_MSG_FIELD_INFO_BY_NAME };
	_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, name, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );
	
	remoteDbFieldInfoByName ( db, &fieldInfo );

	if ( fieldInfo.status )
	{
		throw fieldInfo.status;
	}

	return fieldInfo.dat.rsp.dbFieldInfo.fieldNum ;
}

static VAR_STR remoteDbFieldType ( class vmInstance *instance, REMOTE_DB *db, VAR *index )
{
	DBS_MSG		 fieldInfo{};

	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
			_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, index->dat.str.c, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

			remoteDbFieldInfoByName ( db, &fieldInfo );
			break;
		case slangType::eLONG:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO;
			fieldInfo.dat.req.dbFieldInfo.fieldNum = (int)index->dat.l;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}


	if ( fieldInfo.status )
	{
		throw fieldInfo.status;
	}

	return VAR_STR ( instance, &fieldInfo.dat.rsp.dbFieldInfo.fieldType, 1 );
}

static int remoteDbFieldDec ( class vmInstance *instance, REMOTE_DB *db, VAR *index )
{
	DBS_MSG		 fieldInfo{};

	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
			_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, index->dat.str.c, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

			remoteDbFieldInfoByName ( db, &fieldInfo );
			break;
		case slangType::eLONG:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO;
			fieldInfo.dat.req.dbFieldInfo.fieldNum = (int)index->dat.l;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}


	if ( fieldInfo.status )
	{
		throw fieldInfo.status;
	}

	return fieldInfo.dat.rsp.dbFieldInfo.fieldDec ;
}

static int remoteDbFieldSize ( class vmInstance *instance, REMOTE_DB *db, VAR *index )
{
	DBS_MSG		 fieldInfo{};

	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
			_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, index->dat.str.c, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

			remoteDbFieldInfoByName ( db, &fieldInfo );
			break;
		case slangType::eLONG:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO;
			fieldInfo.dat.req.dbFieldInfo.fieldNum = (int)index->dat.l;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}
	
	if ( fieldInfo.status )
	{
		throw fieldInfo.status;
	}

	return fieldInfo.dat.rsp.dbFieldInfo.fieldLen ;
}

static int remoteDbFieldLen ( class vmInstance *instance, REMOTE_DB *db, VAR *index )
{
	DBS_MSG		 fieldInfo{};

	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
			_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, index->dat.str.c, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

			remoteDbFieldInfoByName ( db, &fieldInfo );
			break;
		case slangType::eLONG:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO;
			fieldInfo.dat.req.dbFieldInfo.fieldNum = (int)index->dat.l;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}

	if ( fieldInfo.status )
	{
		throw fieldInfo.status;
	}

	return fieldInfo.dat.rsp.dbFieldInfo.fieldLen ;
}

/* this version does NOT cause an exception */

static int remoteDbFillFieldCache ( class vmInstance *instance, REMOTE_DB *db, char const *name, DBFIELDINFO *dbFieldInfo )
{
	DBS_MSG		 fieldInfo{};

	if ( dbFieldInfo->fieldNum && (dbFieldInfo->cCtr == db->cCtr) )
	{
		return 1 ;
	}

	fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
	_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, (char *)name, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

	remoteDbFieldInfoByName ( db, &fieldInfo );

	if ( fieldInfo.status )
	{
		dbFieldInfo->fieldNum = 0;
		return 0 ;
	}

	dbFieldInfo->fieldNum = fieldInfo.dat.rsp.dbFieldInfo.fieldNum;
	if ( (dbFieldInfo->fieldType = (unsigned char)fieldInfo.dat.rsp.dbFieldInfo.fieldType) == 'N' )
	{
		dbFieldInfo->lenInfo.num.len = (char)fieldInfo.dat.rsp.dbFieldInfo.fieldLen;
		dbFieldInfo->lenInfo.num.dec = (char)fieldInfo.dat.rsp.dbFieldInfo.fieldDec;
	} else
	{
		dbFieldInfo->lenInfo.fieldlen = fieldInfo.dat.rsp.dbFieldInfo.fieldLen;
	}

	dbFieldInfo->cCtr = db->cCtr;
	return 1 ;
}

static int remoteDbReindex ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_REINDEX };

	if ( !remoteRequestLong ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( dbMsg.status )
	{
		throw (errorNum)dbMsg.status;
	}
	return 1 ;
}

static long remoteDbRecCount ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_INFO };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		return 0 ;
	}

	return (long) dbMsg.dat.rsp.dbInfo.numRecords ;
}

static long remoteDbRecno ( class vmInstance *instance, REMOTE_DB *db )
{
#if defined ( USE_CACHE )
	return db->status.recNo;
#else
	DBS_MSG		 dbMsg;
	
	dbMsg.msgType = DBS_MSG_GET_STATUS;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbStatus.recNo ;
#endif
}

static int remoteDbSeekFound ( class vmInstance *instance, REMOTE_DB *db )
{
#if defined ( USE_CACHE )
	return db->status.isFound;
#else
	DBS_MSG		 dbMsg;
	
	dbMsg.msgType = DBS_MSG_GET_STATUS;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbStatus.isFound ;
#endif
}

static int remoteDbRecUnlock ( class vmInstance *instance, REMOTE_DB *db )
{
	if ( !remoteDbGoCold ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_DB_RUNLOCK };
	dbMsg.dat.req.dbUnlockRec.recno = 0;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.status ;
}

static int remoteDbRecLock ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_RLOCK };
	dbMsg.dat.req.dbLockRec.recno = 0;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.status ;
}

static int remoteDbRecDelete ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DELETE };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.status ;
}

static int remoteDbRecUndelete ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_RECALL };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.status ;
}

static int remoteDbRecIsDeleted ( class vmInstance *instance, REMOTE_DB *db )
{
#if defined ( USE_CACHE )
	return db->status.isDeleted;
#else
	DBS_MSG		 dbMsg;
	
	dbMsg.msgType = DBS_MSG_GET_STATUS;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbStatus.isDeleted ;
#endif
}

static int remoteDbFieldPut ( class vmInstance *instance, REMOTE_DB *db, VAR *index, VAR *value )
{
	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			remoteDbAssign ( instance, db, index->dat.str.c, value, 0 );
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			{
				int			 pos;

				switch ( TYPE ( index ) )
				{
					case slangType::eLONG:
						pos = (int)index->dat.l;
						break;
					case slangType::eDOUBLE:
						pos = (int)index->dat.d;
						break;
					default:
						throw errorNum::scINVALID_PARAMETER;
				}
				DBS_MSG		 fieldInfo{ DBS_MSG_FIELD_INFO };
				fieldInfo.dat.req.dbFieldInfo.fieldNum = pos;

				remoteDbFieldInfoByPos ( db, &fieldInfo );

				remoteDbAssign ( instance, db, fieldInfo.dat.rsp.dbFieldInfo.fieldName, value, 0 );
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	return 1 ;
}

static VAR vmRemoteDbFieldGet ( class vmInstance *instance, REMOTE_DB *db, VAR *index )
{
	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			remoteDbAccess ( instance, db, index->dat.str.c, &instance->result, 0 );
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			{
				int			 pos;
				switch ( TYPE ( index ) )
				{
					case slangType::eLONG:
						pos = (int)index->dat.l;
						break;
					case slangType::eDOUBLE:
						pos = (int)index->dat.d;
						break;
					default:
						throw errorNum::scINVALID_PARAMETER;
				}
				DBS_MSG		 fieldInfo{ DBS_MSG_FIELD_INFO };
				fieldInfo.dat.req.dbFieldInfo.fieldNum = pos;

				remoteDbFieldInfoByPos ( db, &fieldInfo );
				remoteDbAccess ( instance, db, fieldInfo.dat.rsp.dbFieldInfo.fieldName, &instance->result, 0 );
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return instance->result;
}

static VAR_STR remoteDbFieldName ( class vmInstance *instance, REMOTE_DB *db, VAR *index )
{
	DBS_MSG		 fieldInfo{};
	int			 pos;

	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
			_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, index->dat.str.c, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

			remoteDbFieldInfoByName ( db, &fieldInfo );
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			switch ( TYPE ( index ) )
			{
				case slangType::eLONG:
					pos = (int)index->dat.l;
					break;
				case slangType::eDOUBLE:
					pos = (int)index->dat.d;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO;
			fieldInfo.dat.req.dbFieldInfo.fieldNum = pos;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	
	if ( fieldInfo.status )
	{
		throw fieldInfo.status;
	}

	return VAR_STR ( instance, fieldInfo.dat.rsp.dbFieldInfo.fieldName );
}

static double remoteDbDatabaseSize( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_INFO };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		return 0 ;
	}

	return (double) dbMsg.dat.rsp.dbInfo.tableSize ;
}

static int remoteDbRecSize ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_INFO };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}
	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		return 0 ;
	}

	return dbMsg.dat.rsp.dbInfo.recSize ;
}

static VAR remoteDbStruct ( class vmInstance *instance, REMOTE_DB *db )
{
	DBFFIELDS	*fields;

	instance->result.type = slangType::eNULL;
		
	DBS_MSG		 dbMsg{ DBS_MSG_DB_INFO };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		throw errorNum::scDB_CONNECTION;
	}

	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		throw errorNum::scDB_CONNECTION;
	}

	auto arr = VAR_ARRAY ( instance );

	fields = (DBFFIELDS *)bufferBuff ( &db->buffer );
	
	for ( int64_t loop = 0; loop < dbMsg.dat.rsp.dbInfo.fieldCount; loop++ )
	{
		arrayGet ( instance, &arr, loop + 1, 1 ) = VAR_STR ( instance, fields[loop].name );
		arrayGet ( instance, &arr, loop + 1, 2 ) = VAR_STR ( instance, reinterpret_cast<char *>(&fields[loop].type), 1 );
		
		if ( fields[loop].type == 'N' )
		{
			arrayGet ( instance, &arr, loop + 1, 3 ) = VAR ( fields[loop].leninfo.num.len );
			arrayGet ( instance, &arr, loop + 1, 4 ) = VAR ( fields[loop].leninfo.num.dec );
		} else
		{
			arrayGet ( instance, &arr, loop + 1, 3 ) = VAR ( fields[loop].leninfo.fieldlen );
		}
	}
	return arr;
}

static VAR remoteDbFieldInfo ( class vmInstance *instance, REMOTE_DB *db, VAR *index )
{
	DBS_MSG		 fieldInfo{};
	int			 pos;
	char		 typeStr[2]{};

	switch ( TYPE ( index ) )
	{
		case slangType::eSTRING:
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO_BY_NAME;
			_uppern ( fieldInfo.dat.req.dbFieldInfoByName.fieldName, index->dat.str.c, sizeof ( fieldInfo.dat.req.dbFieldInfoByName.fieldName ) );

			remoteDbFieldInfoByName ( db, &fieldInfo );
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			switch ( TYPE ( index ) )
			{
				case slangType::eLONG:
					pos = (int)index->dat.l;
					break;
				case slangType::eDOUBLE:
					pos = (int)index->dat.d;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			fieldInfo.msgType	= DBS_MSG_FIELD_INFO;
			fieldInfo.dat.req.dbFieldInfo.fieldNum = pos;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	if ( fieldInfo.status )
	{
		throw fieldInfo.status;
	}

	// let's create a class and set it's elements to our field info data
	classNew ( instance, "FIELDINFO", 0 );

	typeStr[0] = fieldInfo.dat.rsp.dbFieldInfo.fieldType;
	typeStr[1] = 0;
	*classIVarAccess ( instance->result.dat.ref.v, "TYPE" ) = VAR_STR ( instance, typeStr, 2 );
	*classIVarAccess ( instance->result.dat.ref.v, "NAME" ) = VAR_STR ( instance, fieldInfo.dat.rsp.dbFieldInfo.fieldName );
	*classIVarAccess ( instance->result.dat.ref.v, "LENGTH" ) = VAR ( fieldInfo.dat.rsp.dbFieldInfo.fieldLen );
	*classIVarAccess ( instance->result.dat.ref.v, "DECIMALS" ) = VAR ( fieldInfo.dat.rsp.dbFieldInfo.fieldDec );
	*classIVarAccess ( instance->result.dat.ref.v, "ORDINALPOSITION" ) = VAR ( fieldInfo.dat.rsp.dbFieldInfo.fieldNum );

	return instance->result;
}

static int remoteDbBackup ( class vmInstance *instance, REMOTE_DB *db, char const *path )
{
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_DB_BACKUP };
	strncpy_s ( dbMsg.dat.req.dbBackup.fPath, sizeof ( dbMsg.dat.req.dbBackup.fPath ), path, _TRUNCATE );

	if ( !remoteRequest( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( dbMsg.status )
	{
		throw (errorNum)dbMsg.status;
	}
	return 1 ;
}

static long remoteDbBackupRecord ( class vmInstance *instance, REMOTE_DB *db )
{
	if ( !remoteDbInvalidate ( db ) )
	{
		return 0 ;
	}

	DBS_MSG		 dbMsg{ DBS_MSG_DB_BACKUP_RECORD };
	
	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	return dbMsg.status ;
}

static double remoteDbBackupStatus ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_BACKUP_STATUS };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		return 0 ;
	}

	return (double) dbMsg.dat.rsp.dbBackupStatus.currentBackupRecord / (double) dbMsg.dat.rsp.dbBackupStatus.maxBackupRecord ;
}

static int remoteDbBackupRebuild ( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_BACKUP_REBUILD };

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}

	if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
	{
		return 0 ;
	}

	return dbMsg.status ;
}

static double remoteDbGetServerTime( class vmInstance *instance, REMOTE_DB *db )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_GET_SERVER_TIME };
	
	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}
	return dbMsg.dat.rsp.dbGetServerTime.time ;
}

static double remoteDbIndexGetServerTime( class vmInstance *instance, REMOTE_DB *db )

{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_GET_SERVER_TIME };
	
	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}
	return dbMsg.dat.rsp.dbGetServerTime.time ;
}

static VAR_STR remoteDbIndexKey( class vmInstance *instance, REMOTE_DB *db, int order )
{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_INDEX_INFO };
	dbMsg.dat.req.dbIndexInfo.order = order;
	
	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return VAR_STR ();
	}

	return VAR_STR ( instance, dbMsg.dat.rsp.dbIndexInfo.key );
}

static int remoteDbIndexDescend ( class vmInstance *instance, REMOTE_DB *db, int order )

{
	DBS_MSG		 dbMsg{ DBS_MSG_DB_INDEX_INFO };
	dbMsg.dat.req.dbIndexInfo.order = order;

	if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
	{
		return 0 ;
	}
	return dbMsg.dat.rsp.dbIndexInfo.isDescending ;
}

class debugRemoteDBVar : public vmDebugVar {
	class vmInstance	*instance;
	REMOTE_DB	*db;
	DWORD		 index;
	stringi		 type;
	bool		 isField;
	bool		 isString;
	size_t		 valueLen;
	char const	*value = nullptr;
	char		 fieldName[sizeof (((DBS_MSG *)(0))->dat.rsp.dbFieldInfo.fieldName)];
	char		 valueTmp[256];

public:
	debugRemoteDBVar ( class vmInstance *instance, REMOTE_DB *db, DWORD index, bool isField = true )
	{
		this->instance	= instance;
		this->db		= db;
		this->index		= index;
		this->isField	= isField;
		isString		= false;
		valueLen		= -1;
	}

	char const *getName ( void )
	{
		if ( isField )
		{
			DBS_MSG		 fieldInfo{ DBS_MSG_FIELD_INFO };
			fieldInfo.dat.req.dbFieldInfo.fieldNum = (int)index;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			strcpy_s ( fieldName, sizeof ( fieldName ), fieldInfo.dat.rsp.dbFieldInfo.fieldName );
			return fieldName;
		} else
		{
			return instance->workareaTable->getAlias ( index );
		}
	}
	size_t getNameLen ( void )
	{
		return strlen ( getName() );
	}
	char const *getValue ( void )
	{
		type = "null";
		if ( valueLen != -1 ) return value;
		if ( isField )
		{
			DBS_MSG		 fieldInfo{ DBS_MSG_FIELD_INFO };
			fieldInfo.dat.req.dbFieldInfo.fieldNum	= (int)index;

			remoteDbFieldInfoByPos ( db, &fieldInfo );
			remoteDbAccess ( instance, db, fieldInfo.dat.rsp.dbFieldInfo.fieldName, &instance->result, 0);

			switch ( TYPE ( &instance->result ) )
			{
				case slangType::eSTRING:
					isString = true;
					valueLen = (uint32_t)instance->result.dat.str.len;
					value = instance->result.dat.str.c;
					type = "string";
					return instance->result.dat.str.c;
				default:
					isString = false;
					switch ( TYPE ( &instance->result  ) )
					{
						case slangType::eLONG:
							valueLen = _snprintf_s ( valueTmp, sizeof ( valueTmp ), _TRUNCATE, "%zi", instance->result.dat.l );
							type = "integer";
							break;
						case slangType::eDOUBLE:
							valueLen = _snprintf_s ( valueTmp, sizeof ( valueTmp ), _TRUNCATE, "%f", instance->result.dat.d );
							type = "double";
							break;
						default:
							throw errorNum::scINTERNAL;
					}
					value = valueTmp;
					return valueTmp;
			}
		} else
		{
			int			 isEof;
			int			 isBof;
			int			 isDeleted;
			int			 recNo;
			int			 isFound;
			int			 order;

			isEof = remoteDbEOF ( instance, db );
			isBof = remoteDbBOF ( instance, db );
			isDeleted = remoteDbRecIsDeleted ( instance, db );
			recNo = remoteDbRecno ( instance, db );
			isFound	= remoteDbSeekFound ( instance, db );
			instance->result.type = slangType::eLONG;
			instance->result.dat.l = -1;
			order	= remoteDbOrder ( instance, db, &instance->result ); 

			_snprintf_s ( valueTmp, sizeof (valueTmp), _TRUNCATE, "eof = %u, bof = %u, deleted = %u, recNo = %u, isFound = %u, order = %u", 
						isEof,
						isBof,
						isDeleted,
						recNo,
						isFound,
						order
					  );
			valueLen = strlen ( valueTmp );
			value = valueTmp;
			return valueTmp;
		}
	}
	size_t getValueLen ( void )
	{
		if ( valueLen == -1 )
		{
			getValue ( );
		}
		return valueLen;
	}
	stringi getType ()
	{
		return type;
	}
	bool isStringValue ( void )
	{
		return isString;
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
	{

		DWORD			 loop;
		vmInspectList	*iList;

		iList = new vmInspectList();

		if ( isField ) return iList;

		DBS_MSG			 dbMsg{ DBS_MSG_DB_INFO };

		if ( !remoteRequest ( db, &dbMsg, &dbMsg, 0, 0 ) )
		{
			return iList;
		}

		if ( !remoteReadData ( db, (int)sizeof ( DBFFIELDS ) * dbMsg.dat.rsp.dbInfo.fieldCount ) )
		{
			return iList;
		}

		for ( loop = 0; loop < (DWORD)dbMsg.dat.rsp.dbInfo.fieldCount; loop++ )
		{
			iList->entry.push_back ( static_cast<vmDebugVar *>(new debugRemoteDBVar ( instance, db, loop + 1 )) );
		}		

		return iList;
	}

	bool hasChildren ( void )
	{
		if ( isField ) return false;
		return true;
	}
};

static vmInspectList *remoteDbInspector ( class vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack, vmDebugVar *var )
{
	vmInspectList	*vars;
	DWORD			 loop;

	vars = new vmInspectList();

	EnterCriticalSection ( &dbRemoteListCtrl );

	for ( loop = 0; loop < instance->workareaTable->getNumAreas(); loop++ )
	{
		if ( instance->workareaTable->inUse ( loop ) )
		{
			vars->entry.push_back ( static_cast<vmDebugVar *>(new debugRemoteDBVar ( instance, (REMOTE_DB *)instance->workareaTable->getData( loop ), loop, false ) ) );
		}
	}
	LeaveCriticalSection ( &dbRemoteListCtrl );
	return vars;
}

/*
	additional special cases for conversion into REMOTE_DB/workarea types to allow native calls using our template metaprogram conversion code
*/

template<>
struct getPType <REMOTE_DB *> 
{
	static inline bcFuncPType toPType ( void )
	{
		return bcFuncPType::bcFuncPType_Workarea;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct vmInstance::varConvert <REMOTE_DB *> 
{
	static inline REMOTE_DB *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		REMOTE_DB *db = (REMOTE_DB *) instance->workareaTable->getData ();
		if ( !db ) throw errorNum::scINVALID_PARAMETER;
		return db;
	}
};

void builtinRemoteDBInit ( class vmInstance *instance, opFile *file )
{
	instance->debug->RegisterInspector ( "Database", remoteDbInspector );

	REGISTER( instance, file );
		NAMESPACE ("fgl" )
			CLASS ( "fieldInfo" );
				IVAR ( "name" );
				IVAR ( "type" );
				IVAR ( "length" );
				IVAR ( "decimals" );
				IVAR ( "ordinalPosition" );
			END;

			FUNC ( "dbOpen", remoteDbOpen, DEF ( 2, "''" ), DEF ( 3, "1" ) );
			FUNC ( "dbUse", remoteDbOpen, DEF ( 2, "''" ), DEF ( 3, "1" ) );
			FUNC ( "dbOpenMulti", remoteDbOpenMulti, DEF ( 3, "''" ), DEF ( 4, "1" ) );
			FUNC ( "dbCreate", remoteDbCreate );
			FUNC ( "dbIsDeleted", remoteDbRecIsDeleted );
			FUNC ( "deleted", remoteDbRecIsDeleted );
			FUNC ( "dbRecno", remoteDbRecno );
			FUNC ( "recno", remoteDbRecno );
			FUNC ( "dbEOF", remoteDbEOF );
			FUNC ( "dbBOF", remoteDbBOF );
			FUNC ( "dbGoTop", remoteDbGoTop );
			FUNC ( "dbGoBottom", remoteDbGoBottom );
			FUNC ( "dbGoto", remoteDbGoto );
			FUNC ( "dbSkip", remoteDbSkip, DEF ( 1, "1" ) );
			FUNC ( "dbDelete", remoteDbRecDelete );
			FUNC ( "dbUndelete", remoteDbRecUndelete );
			FUNC ( "dbRecall", remoteDbRecUndelete );
			FUNC ( "dbRecLock", remoteDbRecLock );
			FUNC ( "dbRLock", remoteDbRecLock );
			FUNC ( "dbRecUnlock", remoteDbRecUnlock );
			FUNC ( "dbUnlock", remoteDbRecUnlock );
			FUNC ( "dbRUnlock", remoteDbRecUnlock );
			FUNC ( "dbIndexKey", remoteDbIndexKey );
			FUNC ( "dbIndexDescend", remoteDbIndexDescend );
			FUNC ( "dbIndexGetServerTime", remoteDbIndexGetServerTime );
			FUNC ( "dbGetServerTime", remoteDbGetServerTime );
			FUNC ( "dbBackupRebuild", remoteDbBackupRebuild );
			FUNC ( "dbBackupStatus", remoteDbBackupStatus );
			FUNC ( "dbBackupRecord", remoteDbBackupRecord );
			FUNC ( "dbBackup", remoteDbBackup );
			FUNC ( "dbFieldInfo", remoteDbFieldInfo );
			FUNC ( "dbStruct", remoteDbStruct );
			FUNC ( "dbRecSize", remoteDbRecSize );
			FUNC ( "dbDatabaseSize", remoteDbDatabaseSize );
			FUNC ( "dbFieldName", remoteDbFieldName );
			FUNC ( "dbFieldGet", vmRemoteDbFieldGet );
			FUNC ( "dbFieldPut", remoteDbFieldPut );
			FUNC ( "dbSeekFound", remoteDbSeekFound );
			FUNC ( "dbRecCount", remoteDbRecCount );
			FUNC ( "dbReindex", remoteDbReindex );
			FUNC ( "dbOpenIndex", remoteDbOpenIndex );
			FUNC ( "dbCloseIndex", remoteDbCloseIndex );
			FUNC ( "dbCachePolicy", remoteDbCachePolicy );
			FUNC ( "dbCreateIndex", remoteDbCreateIndex, DEF ( 3, "0" ) );
			FUNC ( "dbSeek", remoteDbSeek, DEF ( 2, "1" ) );
			FUNC ( "dbSeekLast", remoteDbSeekLast, DEF ( 2, "1" ) );
			FUNC ( "dbCmpKey", remoteDbCmpKey );
			FUNC ( "dbFindSubset", remoteDbFindSubset, DEF ( 2, "1" ) );
			FUNC ( "dbCountMatchhing", remoteDbCountMatching );
			FUNC ( "dbOrder", remoteDbOrder );
			FUNC ( "dbAppend", remoteDbAppend );
			FUNC ( "dbFieldCount", remoteDbFieldCount );
			FUNC ( "dbFound", remoteDbSeekFound );
			
			FUNC ( "dbFieldType", remoteDbFieldType );
			FUNC ( "dbFieldPos", remoteDbFieldPos );
			FUNC ( "dbFieldDec", remoteDbFieldDec );
			FUNC ( "dbFieldLen", remoteDbFieldLen );
			FUNC ( "dbFieldize", remoteDbFieldSize );

			FUNC ( "dbFileSize", remoteDbDatabaseSize );
			FUNC ( "dbEnd", remoteDbEnd );
			FUNC ( "dbClose", remoteDbCloseConn );

		END;
	END;
}
