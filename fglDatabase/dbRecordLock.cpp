/*

	DATABASE Server

*/

#include "dbIoServer.h"
#include "Db.h"
#include "odbf.h"


bool dbIsRecLocked ( vmInstance *instance, DATABASE *db, uint64_t recno )
{
	SRRWLocker l1 ( db->lock, false );

	if ( db->recLock.find ( recno ) != db->recLock.end ( ) ) return true;
	return false;
}

bool dbLockRec ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno )
{
	SRRWLocker l1 ( db->lock, true );

	auto it = db->recLock.find ( recno );
	if ( it == db->recLock.end ( ) )
	{
		// new lock
		db->recLock.insert ( std::pair<size_t, DBS_REC_LOCK> ( recno, DBS_REC_LOCK { recno, 1, dbConn } ) );
		dbConn->recLock.insert ( recno );
		return true;
	} else
	{
		// existing lock, check ownership
		if ( it->second.dbConn == dbConn )
		{
			it->second.cnt++;
			return true;
		}
		return  false;
	}
}

void dbLockAppend ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	SRRWLocker l1 ( db->lock, true );

	size_t	 recno ;

	_tbAppend ( db->tbl, dbConn->tblConn );

	recno =(unsigned long)dbConn->tblConn->recNo;

	db->recLock.insert ( std::pair<size_t, DBS_REC_LOCK> ( recno, DBS_REC_LOCK{ recno, 0, dbConn } ) );
	dbConn->recLock.insert ( recno );
}

bool dbUnlockRec ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno )
{
	SRRWLocker l1 ( db->lock, true );

	auto it = db->recLock.find ( recno );
	if ( it == db->recLock.end ( ) )
	{
		return false;
	} else
	{
		// existing lock, check ownership
		if ( it->second.dbConn == dbConn )
		{
			it->second.cnt--;

			if ( !it->second.cnt )
			{
				// last lock on the record, so go cold
				dbSyncIndex ( instance, db, dbConn );
				dbGoCold ( instance, db, dbConn );

				db->recLock.erase ( it );
				dbConn->recLock.erase ( recno );
			}
			return true;
		}
		return  false;
	}
}

bool dbUnlockRecAll (vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno)
{
	SRRWLocker l1 ( db->lock, true );

	auto it = db->recLock.find ( recno );
	if ( it == db->recLock.end ( ) )
	{
		return false;
	} else
	{
		// existing lock, check ownership
		if ( it->second.dbConn == dbConn )
		{
			dbSyncIndex ( instance, db, dbConn );
			dbGoCold ( instance, db, dbConn );

			db->recLock.erase ( it );
			dbConn->recLock.erase ( recno );
			return true;
		}
		return  false;
	}
}

