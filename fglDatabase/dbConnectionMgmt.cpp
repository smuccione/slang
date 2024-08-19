/*

	DATABASE Server

*/

#include "dbIoServer.h"
#include "odbf.h"
#include <mstcpip.h>
#include <mswsock.h>

#include "Utility/funcky.h"
#include <filesystem>


#if 0
static char const *ioName[] = { "Accept", "Read_Connect", "Read", "Write", "Disconnect", "Shutdown" };
#endif

/* Allocate and initialize everything necessary for a new database FD connection and return the connection structure */
DB_SERVER_FD *DB_SERVER::dbAllocateFD ( HANDLE iocp )

{
	DB_SERVER_FD	*dbFd = nullptr;
	int				 value;

	// access the free list
	EnterCriticalSection ( &dbFDAccessCtrl );

	/* get one off the free list */
	if ( dbFDFree )
	{
		dbFd = dbFDFree;
		dbFDFree = dbFDFree->next;

		dbFd->buffers = 0;

		dbFd->prev = 0;
		dbFd->next = dbFDActiveList;
		if ( dbFDActiveList )
		{
			dbFDActiveList->prev = dbFd;
		}
		dbFDActiveList = dbFd;

		LeaveCriticalSection ( &dbFDAccessCtrl );

		if ( !dbFd->socket )
		{
			dbFd->socket = WSASocket ( AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED );

			struct tcp_keepalive vals;

			value = 1;
			setsockopt ( dbFd->socket, IPPROTO_TCP, SO_KEEPALIVE, (char *)&value, sizeof ( value ) );

			memset ( &vals, 0, sizeof ( vals ) );
			vals.onoff = 1;						// non-zero means "enable"
			vals.keepalivetime = 5000;			// milliseconds
			vals.keepaliveinterval = 5000;		// milliseconds
			DWORD numBytesReturned = 0;			// not really used AFAICT
			// send a keep-alive every 5 seconds
			WSAIoctl ( dbFd->socket, SIO_KEEPALIVE_VALS, &vals, sizeof ( vals ), NULL, 0, &numBytesReturned, NULL, NULL );

			value = 1;
			setsockopt ( dbFd->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &value, sizeof ( value ) );

			value = 0;
//			setsockopt ( dbFd->socket, SOL_SOCKET, SO_SNDBUF, (char *)&value, sizeof ( value ) );

			CreateIoCompletionPort ( (HANDLE) dbFd->socket, dbIocpCompletionPort, (ULONG_PTR)dbFd, 0);	// NOLINT(performance-no-int-to-ptr)
		} else
		{
			printf ( "Error: critical socket failure" );
			/* damn... no free ones */
			return nullptr;
		}

		dbFd->freeOnComplete	= 0;
		dbFd->dbs				= 0;
		dbFd->inUse				= 1;	/* mark as being in use for shutdown purposes */
		dbFd->dbServer = this;
		return dbFd;
	} else
	{
		LeaveCriticalSection ( &dbFDAccessCtrl );
		printf ( "Error: Number of simultaneous database connections exceed maximum allowed" );
		/* damn... no free ones */
		return nullptr;
	}
}

/* free the connection structure and return it to the free pool */
bool DB_SERVER::dbFreeFD ( vmInstance *instance, DB_SERVER_FD *dbFd, DB_BUFFER *buffer )
{
	EnterCriticalSection ( &dbFDAccessCtrl );

	dbFreeBuffer ( dbFd, buffer );

	if ( dbFd->buffers )
	{
		dbFd->freeOnComplete = 1;
		LeaveCriticalSection ( &dbFDAccessCtrl );
		return false;
	}

	/* remove it from the server */
	if ( dbFd->prev )
	{
		dbFd->prev->next = dbFd->next;
	} else
	{
		dbFDActiveList = dbFd->next;
	}
	if ( dbFd->next )
	{
		dbFd->next->prev = dbFd->prev;
	}

	// free any resources involved with this FD
	if ( dbFd->dbs && dbFd->dbConn )
	{
		for ( auto it : dbFd->dbConn->recLock )
		{
			dbUnlockRec ( instance, dbFd->dbs->db, dbFd->dbConn, it );
		}
		dbFd->dbConn->recLock.clear ( );

		dbFd->dbConn->ftxConn.clear ( );

		if ( dbFd->dbConn->tblConn ) dbFreeTableConnection ( dbFd->dbConn->tblConn );
		dbFreeConnection ( dbFd->dbConn );
	}

	// mark it as free (this is really just a sanity check... nothing more... it is unused
	dbFd->inUse	= 0;

	if ( dbFd->socket )
	{
		closesocket ( dbFd->socket );
		dbFd->socket = 0;
	}
	/* add it to the global free list */
	dbFd->next	= dbFDFree;
	dbFDFree	= dbFd;
	LeaveCriticalSection ( &dbFDAccessCtrl );
	return true;
}

TABLE_CONNECTION *DB_SERVER::dbAllocateTableConnection ( void )
{
	TABLE_CONNECTION	*tblConn;

	EnterCriticalSection ( &dbTBLConnAccessCtrl );

	if ( !tblConnFreeList )
	{
		/* have to allocate a new one */
		LeaveCriticalSection ( &dbTBLConnAccessCtrl );
		tblConn = new TABLE_CONNECTION ( );
	} else
	{
		/* remove one from the free list */
		tblConn = tblConnFreeList;
		tblConnFreeList = tblConnFreeList->next;
		LeaveCriticalSection ( &dbTBLConnAccessCtrl );
	}

	/* initialize the control connection*/
	tblConn->isHot		= 0;
	tblConn->phantom	= 1;
	tblConn->isFound	= 0;
	tblConn->lastField	= 0;
	tblConn->eof		= 0;
	tblConn->bof		= 1;
	tblConn->recNo		= 0;
	tblConn->inUse		= 1;
	tblConn->compressBuff.reset ( );
	return tblConn;
}

void DB_SERVER::dbFreeTableConnection ( TABLE_CONNECTION *tblConn )
{
	EnterCriticalSection ( &dbTBLConnAccessCtrl );

	tblConn->inUse		= 0;
	tblConn->next		= tblConnFreeList;
	tblConnFreeList		= tblConn;

	LeaveCriticalSection ( &dbTBLConnAccessCtrl );
}


DATABASE_CONNECTION *DB_SERVER::dbAllocateConnection ( void )

{
	DATABASE_CONNECTION	*dbConn;

	EnterCriticalSection ( &dbConnAccessCtrl );

	if ( !dbConnFreeList )
	{
		/* have to allocate a new one */
		LeaveCriticalSection ( &dbConnAccessCtrl );
		dbConn = new DATABASE_CONNECTION ( );
	} else
	{
		/* remove one from the free list */
		dbConn = dbConnFreeList;
		dbConnFreeList = dbConnFreeList->next;

		_ASSERT ( !dbConn->inUse );

		LeaveCriticalSection ( &dbConnAccessCtrl );
	}
	dbConn->ftxConn.clear ( );
	dbConn->ftxXRef.clear ( );
	dbConn->recLock.clear ( );
	dbConn->sFtx = 0;
	dbConn->userFtx = 0;
	dbConn->tblConn = 0;
	dbConn->inUse = 1;

	dbConn->ftxXRef.push_back ( 0 );

	return ( dbConn );
}

void DB_SERVER::dbFreeConnection ( DATABASE_CONNECTION *dbConn )
{
	EnterCriticalSection ( &dbConnAccessCtrl );

	dbConn->inUse	= 0;
	dbConn->next	= dbConnFreeList;
	dbConnFreeList	= dbConn;

	LeaveCriticalSection ( &dbConnAccessCtrl );
}

DB_BUFFER *DB_SERVER::dbAllocateBuffer ( DB_SERVER_FD *fd )

{
	DB_BUFFER	*buffer;

	EnterCriticalSection ( &dbbufferAccessCtrl );

	if ( dbBufferFree )
	{
		buffer =  dbBufferFree;
		dbBufferFree = dbBufferFree->next;

		_ASSERT ( !buffer->inUse );

		buffer->inUse	= 1;
		LeaveCriticalSection ( &dbbufferAccessCtrl );
	} else
	{
		LeaveCriticalSection ( &dbbufferAccessCtrl );

		buffer = new DB_BUFFER ( );
		buffer->inUse = 1;
	}

	buffer->buff.reset ( );
	buffer->fd		= fd;
	memset ( &buffer->ov, 0, sizeof ( buffer->ov ) );

	if ( fd )
	{
		EnterCriticalSection ( &fd->bufferAccess );

		fd->buffCount++;

		// attach it to the FD
		buffer->prev = 0;
		buffer->next = fd->buffers;
		if ( fd->buffers )
		{
			fd->buffers->prev = buffer;
		}
		fd->buffers = buffer;

		LeaveCriticalSection ( &fd->bufferAccess );
	}

	return buffer;
}

void DB_SERVER::dbFreeBuffer ( DB_SERVER_FD *fd, DB_BUFFER *buffer )
{
	if ( fd )
	{
		// detach it from the FD
		EnterCriticalSection ( &fd->bufferAccess );

		_ASSERT ( fd->buffCount );
		_ASSERT ( buffer->inUse );

		fd->buffCount--;

		if ( buffer->prev )
		{
			buffer->prev->next = buffer->next;
		} else
		{
			fd->buffers = buffer->next;
		}
		if ( buffer->next )
		{
			buffer->next->prev = buffer->prev;
		}

		LeaveCriticalSection ( &fd->bufferAccess );
	}

	buffer->inUse = 0;

	// attach it to the server free buffer list
	EnterCriticalSection ( &dbbufferAccessCtrl );

	buffer->next = dbBufferFree;
	dbBufferFree = buffer;

	LeaveCriticalSection ( &dbbufferAccessCtrl );
}

void DB_SERVER::dbFDReset ( DB_SERVER_FD *fd )
{
	// only close it if it's really a socket
	if ( fd->socket )
	{
		EnterCriticalSection ( &fd->bufferAccess );
		closesocket ( fd->socket );
		fd->socket  = 0;
		LeaveCriticalSection ( &fd->bufferAccess );
	}
}

void DB_SERVER::dbServerNewAccept ( vmInstance *instance )
{
	DB_SERVER_FD	*fd;
	DB_BUFFER		*buffer;
	DWORD			 nBytesTransferred;

	while ( 1 )
	{
		// make a new fd ready to be accepted
		if ( !(fd = dbAllocateFD ( dbIocpCompletionPort )) )
		{
			printf ( "---------------------------- out of db connections -------------------------\r\n" );
			return;
		}
		buffer = dbAllocateBuffer ( fd );
		buffer->ioState = DBS_IO_ACCEPT;
		if ( !AcceptEx ( listenSocket, fd->socket, buffer->buff.data<char *>(), 0, sizeof ( struct sockaddr_in ) + 16, sizeof ( struct sockaddr_in) + 16, &nBytesTransferred, (OVERLAPPED *)buffer ) )
		{
			switch ( WSAGetLastError() )
			{
				case ERROR_IO_PENDING:
					// we'll get the result in a future iocp call
					buffer = 0;
					break;
				case WSAENOTSOCK:
					if ( listenSocket )
					{
						// even this accept failed... why???
						dbFDReset ( fd );
						dbFreeFD ( instance, fd, buffer );
					} else
					{
						// listen socket was shut down... 
						dbFreeFD ( instance, fd, buffer );
						return;
					}
					break;
				default:
					// even this accept failed... why???
					dbFDReset ( fd );
					dbFreeFD ( instance, fd, buffer );
					break;
			}
			if ( !buffer )
			{
				// we just null buffer to use as a flag to indicate success
				break;
			}
		}
	}
	return;
}

DB_SERVER_DB *DB_SERVER::dbOpenTable ( vmInstance *instance, char const *tableName )
{
	DATABASE_CONNECTION		*dbConn;
	DATABASE				*db;
	DBDEF					*dbDef;

	std::filesystem::path p ( tableName );
	auto ap = std::filesystem::absolute ( p );
	if ( !ap.has_extension () ) ap.replace_extension ( "DB" );

	{
		SRRWLocker l1 ( dbsAccess, false );

		auto it = dbs.find ( stringi ( ap.generic_string () ) );
		if ( it != dbs.end ( ) )
		{
			if ( it->second->denyPendingOpen )
			{
				return nullptr;
			}
			return it->second;
		}
	}

	SRRWLocker l1 ( dbsAccess, true );

	// need to do it again since we don't have a lock promortion and don't want to double open
	auto it = dbs.find ( stringi ( ap.generic_string () ) );
	if ( it != dbs.end ( ) )
	{
		if ( it->second->denyPendingOpen )
		{
			return nullptr;
		}
		return it->second;
	}

	auto newDbs = new DB_SERVER_DB ( );

	/************************************ open the table  *****************************************/
	dbDef = dbDefRead ( this, tableName );

	newDbs->copyMode = 0;

	if ( dbDef->integrityCheck )
	{
		if ( auto rc = dbCheck ( instance, tableName ) )
		{
			if ( !dbRepair ( instance, tableName ) )
			{
				delete dbDef;
				delete newDbs;
				return nullptr;
			}
		}
	}

	if ( !(db = dbOpen ( ap.generic_string ().c_str() )) )
	{
		delete dbDef;
		delete newDbs;
		return nullptr;
	}

	newDbs->db = db;
	newDbs->db->dbDef = dbDef;

	// auto-open and re-create any missing index's
	dbConn = dbAllocateConnection ( );
	dbConn->tblConn = dbAllocateTableConnection ( );

	for ( auto &it : dbDef->indexs )
	{
		if ( !dbOpenIndex ( instance, db, dbConn, it.fName, dbEval, dbFilterEval ) )
		{
			printf ( "creating index %s as %s\r\n", it.fName.c_str ( ), it.expr.c_str ( ) );

			if ( !dbCreateIndexExtended ( instance, db, it.fName, it.expr, it.isDescending, dbEval, it.filter, dbFilterEval ) )
			{
				// can't create the index... what to do?
				break;
			}
			if ( !dbOpenIndex ( instance, db, dbConn, it.fName, dbEval, dbFilterEval ) )
			{
				// can't open the newly created index... what to do?
				break;
			}
		}
	}

	dbFreeTableConnection ( dbConn->tblConn );
	dbFreeConnection ( dbConn );

	newDbs->name = ap.generic_string ().c_str();
	dbs[stringi ( ap.generic_string () )] = newDbs;

	newDbs->workAreaNum = 1;
	newDbs->refCount = 1;
	newDbs->pendingClose = 0;
	newDbs->server = this;

	dbServerLog ( newDbs, "Opening Table %s\r\n", tableName );

	return newDbs;
}

DB_SERVER_DB *DB_SERVER::dbGetTable ( char const *tableName )
{
	SRRWLocker l1 ( dbsAccess, false );

	std::filesystem::path p ( tableName );
	auto ap = std::filesystem::absolute ( p );
	if ( !ap.has_extension () ) ap.replace_extension ( "DB" );

	auto it = dbs.find ( stringi ( ap.generic_string () ) );

	if ( it == dbs.end ( ) ) return nullptr;
	return it->second;
}

void DB_SERVER::dbReleaseTable ( DB_SERVER_DB *cDbs )
{
	SRRWLocker l1 ( dbsAccess, true );

	InterlockedDecrement ( &cDbs->refCount );

	if ( cDbs->pendingClose )
	{
		if ( !cDbs->refCount )
		{
			dbServerLog ( cDbs, "Closing Table %s\r\n", cDbs->db->tbl->alias );

			dbClose ( cDbs->db );
			dbs.erase ( dbs.find ( stringi ( cDbs->name ) ) );
		} else
		{
			cDbs->refCount--;
		}
	}
}

void DB_SERVER::dbCloseDB ( char const *tableName, bool denyPendingOpen )
{
	SRRWLocker l1 ( dbsAccess, true );

	std::filesystem::path p ( tableName );
	auto ap = std::filesystem::absolute ( tableName );

	if ( !ap.has_extension () ) ap.replace_extension ( "DB" );

	auto it = dbs.find ( stringi ( ap.generic_string () ) );
	if ( it == dbs.end ( ) ) return;

	if ( it->second->refCount )
	{
		it->second->pendingClose = true;
		it->second->denyPendingOpen = denyPendingOpen;
		return;
	}

	dbServerLog ( it->second, "Closing Table %s\r\n", it->second->db->tbl->alias );

	dbClose ( it->second->db );

	dbs.erase ( it );
}

