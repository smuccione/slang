#include <filesystem>

#include "odbf.h"

#include "io.h"
#include "fcntl.h"
#include "time.h"
#include "Db.h"

#include "../bcVM/bcVM.h"


static void dbReindexSpecific ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t nFtx );
static size_t dbGotoSelf ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn );

static void dbGoHot ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	void const	*ptr;
	uint64_t	 rec;

	if ( dbConn->tblConn->isHot || dbConn->tblConn->phantom || _tbIsDeleted ( db->tbl, dbConn->tblConn ) )
	{
		return;
	}

	rec = _tbRecno ( db->tbl, dbConn->tblConn );

	// update the HOT keys for all the indexes
	for ( size_t loop = 0; loop < db->ftx.size(); loop++ )
	{
		if ( (ptr = (db->ftxCallBack[loop])(instance, db->tbl, &db->ftx[loop], &dbConn->ftxConn[loop])) )
		{
			// save off the index values before we modify them... we need this later when we delete the entry during the goCold process
			memcpy ( dbConn->ftxConn[loop].hotDep, &rec, sizeof ( long ) );
			memcpy ( dbConn->ftxConn[loop].hotKey, ptr, db->ftx[loop].getKeySize() );
		}
	}

	dbConn->tblConn->isHot = true;
}

static void dbGoWarm ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )

{
	void const	*ptr;
	uint64_t	 rec;

	if ( dbConn->tblConn->isHot || dbConn->tblConn->phantom || _tbIsDeleted ( db->tbl, dbConn->tblConn ) )
	{
		return;
	}

	rec = _tbRecno ( db->tbl, dbConn->tblConn );

	// update the HOT keys for all the indexes
	// we can't use an iterator here because ftx and ftx callback are linked by index... needs to be reworked... this is ancient code!
	for ( size_t loop = 0; loop < db->ftx.size ( ); loop++ )
	{
		if ( (ptr = (db->ftxCallBack[loop])(instance, db->tbl, &db->ftx[loop], &dbConn->ftxConn[loop])) )
		{
			// save off the index values before we modify them... we need this later when we delete the entry during the goCold process
			memcpy ( dbConn->ftxConn[loop].hotDep, &rec, sizeof ( long ) );
			memcpy ( dbConn->ftxConn[loop].hotKey, ptr, db->ftx[loop].getKeySize ( ) );
		}
	}

	dbConn->tblConn->isHot = 2;
}

void dbGoCold ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	void const	*ptr = nullptr;
	uint64_t	 rec{};
	FTX			*ftx;

	if ( !dbConn->tblConn->isHot || dbConn->tblConn->phantom )
	{
		return;
	}

	if ( dbConn->tblConn->isHot == 1 )
	{
		// hot
		dbConn->tblConn->isHot = 0;

		for ( size_t loop = 0; loop < db->ftx.size ( ); loop++ )
		{
			ftx = &db->ftx[loop];

			if ( _tbIsDeleted ( db->tbl, dbConn->tblConn ) )
			{
				try
				{
					_ftxKeyDelete ( ftx, &dbConn->ftxConn[loop], dbConn->ftxConn[loop].hotKey, dbConn->ftxConn[loop].hotDep );
				} catch ( ... )
				{
					dbReindexSpecific ( instance, db, dbConn, loop );
				}

				if ( !_tbIsDeleted ( db->tbl, dbConn->tblConn ) )
				{
					try
					{
						_ftxKeyAdd ( ftx, &dbConn->ftxConn[loop], ptr, &rec );
					} catch ( ... )
					{
						dbReindexSpecific ( instance, db, dbConn, loop );
					}
				}
			} else
			{
				if ( (ptr = (db->ftxCallBack[loop])(instance, db->tbl, ftx, &dbConn->ftxConn[loop])) )
				{
					if ( memcmp ( ptr, dbConn->ftxConn[loop].hotKey, ftx->getHeader ()->keySize ) != 0 )
					{
						try
						{
							_ftxKeyDelete ( ftx, &dbConn->ftxConn[loop], dbConn->ftxConn[loop].hotKey, dbConn->ftxConn[loop].hotDep );
						} catch ( ... )
						{
							dbReindexSpecific ( instance, db, dbConn, loop );
						}

						try
						{
							rec = _tbRecno ( db->tbl, dbConn->tblConn );

							_ftxKeyAdd ( ftx, &dbConn->ftxConn[loop], ptr, &rec );
						} catch ( ... )
						{
							dbReindexSpecific ( instance, db, dbConn, loop );
						}
					}
				}
			}
		}
	} else
	{
		// hot - new record
		dbConn->tblConn->isHot = 0;

		// warm - newly appended... just add in new keys
		for ( size_t loop = 0; loop < db->ftx.size ( ); loop++ )
		{
			ftx = &db->ftx[loop];

			if ( (ptr = (db->ftxCallBack[loop])(instance, db->tbl, ftx, &dbConn->ftxConn[loop])) )
			{
				rec = _tbRecno ( db->tbl, dbConn->tblConn );

				if ( !_ftxKeyAdd ( ftx, &dbConn->ftxConn[loop], ptr, &rec ) )
				{
					return;
				}
			}
		}
	}
	return;
}

void const *dbEval ( vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn )
{
	void const *res;

	instance->stack = instance->eval + 1;
	instance->interpretBC ( &ftx->cbFunc, 0, 0);

	switch ( TYPE ( &(instance->result) ) )
	{
		case slangType::eSTRING:
			res = instance->result.dat.str.c;
			break;
		case slangType::eLONG:
			res = (char const *)&(instance->result.dat.l);
			break;
		case slangType::eDOUBLE:
			res = (char const *)&(instance->result.dat.d);
			break;
		default:
			res = 0L;
			break;
	}
	return res;
}

bool dbFilterEval ( vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn )
{
	instance->stack = instance->eval + 1;
	instance->interpretBC ( &ftx->cbFunc, 0, 0 );

	switch ( TYPE ( &instance->result) )
	{
		case slangType::eSTRING:
			return instance->result.dat.str.len ? true : false;
			break;
		case slangType::eLONG:
			return instance->result.dat.l ? true : false;
		case slangType::eDOUBLE:
			return (bool) instance->result.dat.d;
		default:
			return false;
	}
}

void dbSyncIndex ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	void const	*ptr;
	uint64_t	 rec;

	rec = _tbRecno ( db->tbl, dbConn->tblConn );

	if ( db->ftx.size ( ) != dbConn->ftxConn.size ( ) )
	{
		/* these messages require any indexes to be initialized... */
		/* set the connection structures for each open index in this table */

		dbConn->ftxConn.reserve ( db->ftx.size ( ) );

		if ( !_tbIsDeleted ( db->tbl, dbConn->tblConn ) )
		{
			for ( size_t loop = dbConn->ftxConn.size (); loop < db->ftx.size (); loop++ )
			{
				/* this connection has no index connection... allocate and init a new one */
				dbConn->ftxConn.push_back ( FTX_CONNECTION {} );

				/* set it to be invalid... starts at 1 so 0 is always invalid */
				dbConn->ftxConn[loop].lastUpdate = 0;

				if ( dbConn->tblConn->isHot )
				{
					if ( (ptr = (db->ftxCallBack[loop])(instance, db->tbl, &db->ftx[loop], &dbConn->ftxConn[loop])) )
					{
						// save off the index values before we modify them... we need this later when we delete the entry during the goCold process
						memcpy ( dbConn->ftxConn[loop].hotDep, &rec, sizeof ( long ) );
						memcpy ( dbConn->ftxConn[loop].hotKey, ptr, db->ftx[loop].getKeySize () );
					}
				}
			}
		}
	}

	if ( !dbConn->tblConn->isHot && dbConn->userFtx && !_tbIsDeleted ( db->tbl, dbConn->tblConn ) )
	{
		if ( dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].lastUpdate != db->ftx[dbConn->ftxXRef[dbConn->userFtx]].lastUpdate )
		{
			/* re-seek on this ftx in order to update our stack */
			dbGotoSelf ( instance, db, dbConn );
			dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].lastUpdate = db->ftx[dbConn->ftxXRef[dbConn->userFtx]].lastUpdate;
		}
	}
}

static void dbReindexSpecific ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t nFtx )

{
	char const *ptr;
	uint64_t  lastRec;
	uint64_t  currentRec;
	uint64_t  startRec;
	
	dbGoCold ( instance, db, dbConn );
	
	_ftxClear ( &db->ftx[nFtx], &dbConn->ftxConn[nFtx] );
	
	startRec = _tbRecno ( db->tbl, dbConn->tblConn );
	
	currentRec = 1;
	lastRec    = _tbLastrec ( db->tbl, dbConn->tblConn );
	
	_ftxAddKeys ( &db->ftx[nFtx],&dbConn->ftxConn[nFtx], FTX_LEAF_TYPE, (unsigned long)lastRec );

	while ( currentRec <= lastRec )
	{
		if ( !_tbGoto ( db->tbl, dbConn->tblConn, currentRec ) )
		{
			return;
		}

		if ( !_tbIsDeleted ( db->tbl, dbConn->tblConn ) )
		{
			/* got to add each record over every index */
			if ( (ptr = (char const *)(db->ftxCallBack[nFtx])(instance, db->tbl, &db->ftx[nFtx], &dbConn->ftxConn[nFtx])) )
			{
				_ftxKeyAddToIndex ( &db->ftx[nFtx], &dbConn->ftxConn[nFtx], ptr, (char const *) &currentRec );
			}
		}
		
		currentRec++;
	}
	
	printf ( "sorting\r\n" );
	// adjust the file size... we may have had deleted records
	_ftxAdjustFileSize ( &db->ftx[nFtx], &dbConn->ftxConn[nFtx] );

	_ftxKeySort ( &db->ftx[nFtx], &dbConn->ftxConn[nFtx] );
	
	dbGoto ( instance, db, dbConn, (unsigned long)startRec );
}

static size_t dbGotoSelf ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	void const	*ptr;
	uint64_t	 recNo;
		
	recNo = _tbRecno ( db->tbl, dbConn->tblConn );
	
	if ( dbConn->userFtx && !_tbIsDeleted ( db->tbl, dbConn->tblConn ) )
	{
		ptr = (db->ftxCallBack[dbConn->ftxXRef[dbConn->userFtx]])(instance, db->tbl, &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]]);
		if ( ptr )
		{
			try 
			{
				_ftxGoto ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], ptr, &recNo );
			} catch ( ... )
			{
				dbReindexSpecific ( instance,  db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
				_ftxGoto ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], ptr, &recNo );
			}
		}
	}
//printf ( "goto self: %u\r\n", recNo );
	
	return recNo;
}

size_t dbExprType ( vmInstance *instance, stringi const &expr, size_t &keyLen )
{
	FTX ftx;
	size_t keyType;

	try
	{
		ftx.compileKey ( instance, expr );
		instance->interpretBC ( &ftx.cbFunc, 0, 0 );

		switch ( TYPE ( &instance->result ) )
		{
			case slangType::eSTRING:
				keyType = FTX_CHAR_EXPR;
				keyLen = instance->result.dat.str.len;
				break;
			case slangType::eLONG:
				keyType = FTX_LONG_EXPR;
				keyLen = sizeof ( instance->result.dat.l );
				break;
			case slangType::eDOUBLE:
				keyType = FTX_DOUBLE_EXPR;
				keyLen = sizeof ( instance->result.dat.d );
				break;
			default:
				// damn.. .error.. what to do?
				keyType = FTX_ERROR_EXPR;
				keyLen = 0;
				break;
		}
		return keyType;
	} catch ( ... )
	{
		return 	FTX_ERROR_EXPR;
	}
}


DATABASE *dbOpen ( char const *filename )
{
	DATABASE	*db;
	
	db = new DATABASE ( );
	
	try
	{
		if ( !(db->tbl = _tbOpen ( filename, FILE_FLAG_WRITE_THROUGH )) )
		{
			delete db;
			return 0;
		}
	} catch ( errorNum )
	{
		delete db;
		return 0;
	}

	return ( db );
}

size_t dbCheck ( vmInstance *instance, char const *filename )
{
	int					 field;
	size_t				 res;
	uint64_t			 numDeleted;
	DATABASE  db;

	printf ( "Checking Database: %s\r\n", filename );
	
	if ( !(db.tbl = _tbOpen ( filename, FILE_FLAG_WRITE_THROUGH )) )
	{
		return 1;
	}

	DATABASE_CONNECTION dbConn;
	

	dbConn.ftxConn.clear ( );
	dbConn.ftxXRef.clear ( );
	dbConn.recLock.clear ( );
	dbConn.sFtx = 0;
	dbConn.userFtx = 0;
	dbConn.tblConn = 0;
	dbConn.inUse = 1;

	dbConn.ftxXRef.push_back ( 0 );

	TABLE_CONNECTION tblConn; 
	dbConn.tblConn = &tblConn;

	if ( db.tbl->blob )
	{
		printf ( "  Checking blob file\r\n" );
		if ( !blobCheck ( db.tbl->blob ) )
		{
			printf ( "    ***** ERROR: Blob integrity failure\r\n" );
			delete db.tbl;
			return 3;
		}
	}

	dbGoTop ( instance, &db, &dbConn );

	numDeleted = 0;
	while ( !dbEOF ( instance, &db, &dbConn ) )
	{
		if ( !_tbIsDeleted ( db.tbl, dbConn.tblConn ) )
		{
			for ( field = 1; field <= _tbFieldCount ( db.tbl, dbConn.tblConn ); field++ )
			{
				switch ( _tbFieldType ( db.tbl, dbConn.tblConn, field ) )
				{
					case 'B':
						if ( !_tbFieldCheck ( db.tbl, dbConn.tblConn, field ) )
						{
							delete db.tbl;
							return 5;
						}
						break;
				}
			}
		} else
		{
			// count the number of deleted entries to compaire with free list to make sure they're all there
			numDeleted++;
		}
		dbSkip ( instance, &db, &dbConn, 1 );
	}

	// walk free list
	res = _tbFreeListCheck ( db.tbl, dbConn.tblConn );

	if ( res == -1 )
	{
		delete db.tbl;
		return 6;
	}

	if ( res != numDeleted )
	{
		delete db.tbl;
		return 7;
	}
	
	delete db.tbl;
	return 0;
}

static bool dbOpenIndex ( vmInstance *instance, DATABASE *db, 
						  const char *name, 
						  void const *( *cb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn),
						  bool ( *filterCb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn) )
{
	SRRWLocker l1 ( db->lock, true );

	if ( db->ftxLookup.find ( stringi ( name ) ) != db->ftxLookup.end ( ) )
	{
		return true;
	}

	try
	{
		FTX ftx = _ftxOpen ( name, _O_RDWR | _O_BINARY,  db->tbl->getUpdateCount() );

		ftx.which = db->ftx.size ( );
		ftx.compileKey ( instance, ftx.getHeader ()->keyExpr );
		db->ftxLookup[stringi ( name )] = db->ftx.size ( );
		db->ftx.push_back ( ftx );
		db->ftxCallBack.push_back ( cb );
		return true;
	} catch ( errorNum )
	{
		return false;
	}

}

bool dbOpenIndex ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, stringi const &name, void const *(*cb)(vmInstance *instance, TABLE *db, FTX*ftx, FTX_CONNECTION *ftxConn), bool ( *filterCb )(vmInstance *instance, TABLE *db, FTX*ftx, FTX_CONNECTION *ftxConn) )
{
	{
		SRRWLocker l1 ( db->lock, false );

		auto it = db->ftxLookup.find ( stringi ( name.c_str ( ) ) );
		if ( it != db->ftxLookup.end ( ) )
		{
			dbConn->ftxXRef.push_back ( db->ftx[it->second].which );
			dbConn->userFtx = dbConn->ftxXRef.size ( ) - 1;
			return true;
		}
	}

	SRRWLocker l2 ( db->lock, true);
	// why do we do this again?  Ok... here's the deal... there's a SMALL but, still non zero chance that another worker thread got in here in between to open the index
	// we *could* always just get a write lock to the entire database, but this would slow down everything... better to get a read lock initially, and only get a write
	// lock if it truely appears that we will need to do a *real* open of the index...
	auto it = db->ftxLookup.find ( stringi ( name.c_str ( ) ) );
	if ( it != db->ftxLookup.end ( ) )
	{
		dbConn->ftxXRef.push_back ( db->ftx[it->second].which );
		dbConn->userFtx = dbConn->ftxXRef.size ( ) - 1;
		return true;
	}
	/* didn't find it so create a new one */
	if ( !dbOpenIndex ( instance, db, name.c_str(), dbEval, dbFilterEval ) )
	{
		return false;
	} else
	{
		/* save it off in our connection structure... others will create their own */
		dbConn->ftxConn.push_back ( FTX_CONNECTION{} );
		dbConn->ftxXRef.push_back ( db->ftx.back().which );
		dbConn->userFtx = dbConn->ftxXRef.size ( ) - 1;
	}
	return true;
}

void dbCommit ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );
}

void dbClose ( DATABASE *db )
{
	db->tbl->setUpdateCount ( db->tbl->getUpdateCount() + 1 );

	for ( auto &it : db->ftx )
	{
		it.setUpdateCount ( db->tbl->getUpdateCount () );
	}

	if ( db->tbl )
	{
		delete db->tbl;
	}

	delete db->dbDef;
	delete db;
}

uint64_t dbSeek ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key )
{
	SRRWLocker l1 ( db->lock, false );

	uint64_t *lp;
	uint64_t *lp2;
	
	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );
	
	try 
	{				
		lp = (uint64_t *)_ftxSeek ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, strlen ( (char *)key ) );
	} catch ( ... )
	{
		dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
		lp = (uint64_t *) _ftxSeek ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, strlen ( (char *) key ) );
	}
	
	if ( lp )
	{
		dbConn->tblConn->isFound = true;
		if ( !_tbGoto ( db->tbl, dbConn->tblConn, *lp ) )
		{
			dbConn->tblConn->isFound = false;
			return 0;
		}
		return *lp;
	} else
	{
		dbConn->tblConn->isFound = true;
		lp = (uint64_t *)(dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownDep);
		
		if ( (db->ftx[dbConn->ftxXRef[dbConn->userFtx]].cmpRtn) ( dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownKey, (char *)key, strlen ( (char *) key ) ) > 0 )
		{
			_tbGoto ( db->tbl, dbConn->tblConn, *lp );
		} else
		{
			if ( !(lp2 = (uint64_t*)_ftxSkip ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], 1 )) )
			{
				_tbGoto ( db->tbl, dbConn->tblConn, _tbLastrec ( db->tbl, dbConn->tblConn ) + 1 );
			} else
			{
				_tbGoto ( db->tbl, dbConn->tblConn, *lp2 );
			}
		}
		return 0;
	}
}

uint64_t dbSeekLen ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key, size_t len )
{
	SRRWLocker l1 ( db->lock, false );

	uint64_t *lp;
	uint64_t *lp2;

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );
	
	try 
	{				
		lp = (uint64_t *)_ftxSeek ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len );
	} catch (... )
	{
		dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
		lp = (uint64_t *) _ftxSeek ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len );
	}
	
	if ( lp )
	{
		dbConn->tblConn->isFound = true;
		if ( !_tbGoto ( db->tbl, dbConn->tblConn, *lp ) )
		{
			dbConn->tblConn->isFound = false;
			return 0;
		}
		return ( *lp );
	} else
	{
		dbConn->tblConn->isFound = false;
		lp = (uint64_t *) (dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownDep);
		
		if ( (db->ftx[dbConn->ftxXRef[dbConn->userFtx]].cmpRtn) ( dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownKey, (char *)key, len ) > 0 )
		{
			_tbGoto ( db->tbl, dbConn->tblConn, *lp );
		} else
		{
			if ( !(lp2 = ( uint64_t * )_ftxSkip ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], 1 )) )
			{
				_tbGoto ( db->tbl, dbConn->tblConn, _tbLastrec ( db->tbl, dbConn->tblConn ) + 1 );
			} else
			{
				_tbGoto ( db->tbl, dbConn->tblConn, *lp2 );
			}
		}
		return 0;
	}
}

uint64_t dbSeekLast ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key )
{
	SRRWLocker l1 ( db->lock, false );

	uint64_t *lp;
	uint64_t *lp2;

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );

	try
	{
		lp = (uint64_t *)_ftxSeekLast ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, strlen ( (char *)key ) );
	}
	catch ( ... )
	{
		dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
		lp = (uint64_t *)_ftxSeekLast ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, strlen ( (char *)key ) );
	}

	if ( lp )
	{
		dbConn->tblConn->isFound = true;
		if ( !_tbGoto ( db->tbl, dbConn->tblConn, *lp ) )
		{
			dbConn->tblConn->isFound = false;
			return 0;
		}
		return *lp;
	} else
	{
		dbConn->tblConn->isFound = true;
		lp = (uint64_t *)(dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownDep);

		if ( (db->ftx[dbConn->ftxXRef[dbConn->userFtx]].cmpRtn) (dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownKey, (char *)key, strlen ( (char *)key )) > 0 )
		{
			_tbGoto ( db->tbl, dbConn->tblConn, *lp );
		} else
		{
			if ( !(lp2 = (uint64_t*)_ftxSkip ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], 1 )) )
			{
				_tbGoto ( db->tbl, dbConn->tblConn, _tbLastrec ( db->tbl, dbConn->tblConn ) + 1 );
			} else
			{
				_tbGoto ( db->tbl, dbConn->tblConn, *lp2 );
			}
		}
		return 0;
	}
}

uint64_t  dbSeekLastLen ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key, size_t len )
{
	SRRWLocker l1 ( db->lock, false );

	uint64_t *lp;
	uint64_t *lp2;

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );

	try
	{
		lp = (uint64_t *)_ftxSeekLast ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len );
	}
	catch ( ... )
	{
		dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
		lp = (uint64_t *)_ftxSeekLast ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len );
	}

	if ( lp )
	{
		dbConn->tblConn->isFound = true;
		if ( !_tbGoto ( db->tbl, dbConn->tblConn, *lp ) )
		{
			dbConn->tblConn->isFound = false;
			return 0;
		}
		return (*lp);
	} else
	{
		dbConn->tblConn->isFound = false;
		lp = (uint64_t *)(dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownDep);

		if ( (db->ftx[dbConn->ftxXRef[dbConn->userFtx]].cmpRtn) (dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownKey, (char *)key, len) > 0 )
		{
			_tbGoto ( db->tbl, dbConn->tblConn, *lp );
		} else
		{
			if ( !(lp2 = (uint64_t *)_ftxSkip ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], 1 )) )
			{
				_tbGoto ( db->tbl, dbConn->tblConn, _tbLastrec ( db->tbl, dbConn->tblConn ) + 1 );
			} else
			{
				_tbGoto ( db->tbl, dbConn->tblConn, *lp2 );
			}
		}
		return 0;
	}
}

size_t dbFindSubset ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void const *key, size_t len, size_t every, void **subset )
{
	size_t		 numRecs;
	uint64_t	 curRec;
	
	{
		SRRWLocker l1 ( db->lock, false );

		dbSyncIndex ( instance, db, dbConn );
		dbGoCold ( instance, db, dbConn );

		curRec = _tbRecno ( db->tbl, dbConn->tblConn );

		try
		{
			numRecs = _ftxFindSubset ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len, every, subset );
		} catch ( ... )
		{
			dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
			numRecs = _ftxFindSubset ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len, every, subset );
		}
	}

	dbGoto ( instance, db, dbConn, (unsigned long)curRec );
	return numRecs;
}

size_t dbCountMatching ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void const *key, size_t len )
{
	size_t		 numRecs;
	uint64_t	 curRec;
	
	{
		SRRWLocker l1 ( db->lock, false );

		dbSyncIndex ( instance, db, dbConn );
		dbGoCold ( instance, db, dbConn );

		curRec = _tbRecno ( db->tbl, dbConn->tblConn );

		try
		{
			numRecs = _ftxCountMatching ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len );
		} catch ( ... )
		{
			dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
			numRecs = _ftxCountMatching ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], key, len );
		}
	}
	dbGoto ( instance, db, dbConn, curRec );

	return numRecs;
}

uint64_t dbGoto ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno )
{
	void const	*ptr;
	uint64_t	 rec;

	rec = recno;
	
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );

	if ( !_tbGoto ( db->tbl, dbConn->tblConn, rec ) )
	{
		return 0;
	}
	
	if ( dbConn->ftxXRef[dbConn->userFtx] && !_tbIsDeleted ( db->tbl, dbConn->tblConn ) )
	{
		ptr = (db->ftxCallBack[dbConn->ftxXRef[dbConn->userFtx]])(instance, db->tbl, &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]]);
		if ( ptr )
		{
			try 
			{
				_ftxGoto ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], ptr, &rec );
			} catch ( ... )
			{
				dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
				_ftxGoto ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], ptr, &rec );
			}
		}
	}
	return recno;
}

size_t dbOrder ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t order )
{
	size_t		 oldOrder;
	void const	*ptr;
	uint64_t	 recNo;

	if ( order == dbConn->userFtx )
	{
		return ( order );
	}

	oldOrder = dbConn->userFtx;

	if ( (order >= 0) && (order <= dbConn->ftxConn.size()) )
	{
		// we need to resynchronize on the current index in order to ensure we have proper dependency data for a change
		SRRWLocker l1 ( db->lock, false );

		dbSyncIndex ( instance, db, dbConn );
		dbGoCold ( instance, db, dbConn );

		/* save off the newly selected order number */
		dbConn->userFtx = order;

		recNo = _tbRecno ( db->tbl, dbConn->tblConn );

		if ( dbConn->ftxXRef[dbConn->userFtx] && !_tbIsDeleted ( db->tbl, dbConn->tblConn ))
		{
			ptr = (db->ftxCallBack[dbConn->ftxXRef[dbConn->userFtx]])(instance, db->tbl, &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]]);
			if ( ptr )
			{
				try 
				{
					_ftxGoto ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], ptr, &recNo );
				} catch ( ... )
				{
					dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
					_ftxGoto ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], ptr, &recNo );
				}
			}
		}
	}
	return oldOrder;
}

uint64_t dbGoTop ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	uint64_t	*rp;
	uint64_t	 r;
	
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );

	if ( dbConn->ftxXRef[dbConn->userFtx] )
	{
		try 
		{
			rp = (uint64_t *)_ftxGoTop ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		} catch ( ... )
		{
			dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
			rp = (uint64_t *) _ftxGoTop ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		}
		if ( rp )
		{
			return ( _tbGoto ( db->tbl, dbConn->tblConn, *rp ) );
		} else
		{
			r = _tbGoto ( db->tbl, dbConn->tblConn, 0 );
			dbConn->tblConn->eof = 1;
			return r;
		}
	} else
	{
		return ( _tbGoto ( db->tbl, dbConn->tblConn, 1 ) );
	}
}

uint64_t dbGoBottom ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	uint64_t	*rp;
	uint64_t	 r;
	
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );

	if ( dbConn->ftxXRef[dbConn->userFtx] )
	{
		try 
		{
			rp = (uint64_t *)_ftxGoBottom ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		} catch ( ... )
		{
			dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
			rp = (uint64_t *) _ftxGoBottom ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		}
		if ( rp )
		{
			return ( _tbGoto ( db->tbl, dbConn->tblConn, *rp ) );
		} else
		{
			r = _tbGoto ( db->tbl, dbConn->tblConn, 0 );
			dbConn->tblConn->eof = 1;
			return ( r );
		}
	} else
	{
		return ( _tbGoto ( db->tbl, dbConn->tblConn, _tbLastrec ( db->tbl, dbConn->tblConn ) ) );
	}
}

bool dbIsBottom ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );

	if ( dbConn->ftxXRef[dbConn->userFtx] )
	{
		try {
			return _ftxIsBottom ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		} catch ( ... )
		{
			dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
			return _ftxIsBottom ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		}
	} else
	{
		if ( _tbRecno ( db->tbl, dbConn->tblConn ) == _tbLastrec ( db->tbl, dbConn->tblConn ) )
		{
			return true;
		} else
		{
			return false;
		}
	}
}

bool dbIsTop ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );

	if ( dbConn->ftxXRef[dbConn->userFtx] )
	{
		try 
		{
			return _ftxIsTop ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		} catch ( ... )
		{
			dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
			return _ftxIsTop ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]] );
		}
	} else
	{
		if ( _tbRecno ( db->tbl, dbConn->tblConn ) == 1 )
		{
			return true;
		} else
		{
			return false;
		}
	}
}

bool dbEOF ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return dbConn->tblConn->eof;
}

bool dbBOF ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )

{
	return dbConn->tblConn->bof;
}

uint64_t dbAppend ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	SRRWLocker l1 ( db->lock, true );

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );

	dbLockAppend ( instance, db, dbConn );

	dbGoWarm ( instance, db, dbConn );

	return _tbRecno ( db->tbl, dbConn->tblConn );
}

size_t dbFieldGet ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num, void *dest )
{
	return _tbFieldGet ( db->tbl, dbConn->tblConn, num, (char *)dest );
}

void dbFieldPut ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num, void const *src, size_t len )
{
	if ( !dbConn->tblConn->isHot )
	{
		SRRWLocker l1 ( db->lock, false );

		dbSyncIndex ( instance, db, dbConn );
		dbGoHot( instance, db, dbConn );
	}
	_tbFieldPut ( db->tbl, dbConn->tblConn, num, (char *)src, len );
}

uint64_t dbSkip ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, int64_t count )
{
	uint64_t	*recno;
	
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );
	dbGoCold ( instance, db, dbConn );

	if ( dbConn->ftxXRef[dbConn->userFtx] )
	{
		try 
		{
			recno = (uint64_t *)_ftxSkip ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], count );
		} catch ( ... )
		{
			dbReindexSpecific ( instance, db, dbConn, dbConn->ftxXRef[dbConn->userFtx] );
			recno = (uint64_t *) _ftxSkip ( &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]], count );
		}
		
		if ( recno )
		{
			if ( !_tbGoto ( db->tbl, dbConn->tblConn, *recno ) )
			{
				return 0;
			}
			return *recno;
		} else
		{
			if ( count > 0 )
			{
				if ( !_tbGoto ( db->tbl, dbConn->tblConn, _tbLastrec ( db->tbl, dbConn->tblConn ) + 1 ) )
				{
					return 0;
				}
				return _tbRecno ( db->tbl, dbConn->tblConn );
			} else if ( count < 0 )
			{
				if ( !_tbGoto ( db->tbl, dbConn->tblConn, 0 ) )
				{
					return 0;
				}
				return _tbRecno ( db->tbl, dbConn->tblConn );
			} else
			{
				return 0;
			}
		}
	} else
	{
		return _tbSkip ( db->tbl, dbConn->tblConn, count );
	}
}

bool dbCreateIndexExtended ( vmInstance *instance, DATABASE *db, stringi const &name, stringi const &keyExpr, bool isDescending, void const *( *cb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn), stringi const &filterExpr, bool ( *cbFilter )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn) )
{
	uint64_t			 lastRec;
	uint64_t			 currentRec;
	size_t				 keyType;
	size_t				 keyLen;
	bool				 isFilter;
	void const			*key;
	LARGE_INTEGER		 startTime;
	LARGE_INTEGER		 endTime;
	LARGE_INTEGER		 frequency;
	uint64_t			 nextPrintRec;

	SRRWLocker l1 ( db->lock, false );

	// this doesn't actually ever attach the index to the datbase... so no need to do any protection of the database structures!

	// we need to do all this so the engine expression evaluator is connected to the passed in database
	DATABASE_CONNECTION dbConn;
	TABLE_CONNECTION tblConn;
	dbConn.tblConn = &tblConn;

	FTX_CONNECTION ftxConn;

	std::pair<DATABASE *, DATABASE_CONNECTION	*>	localDb ( db, &dbConn );

	instance->workareaTable->setData ( 1, &localDb );

	// test
	isFilter = 0;
	// must come before the indexKey evaluation as we reuse keyType (we don't need it... they do).
	if ( filterExpr.size() )
	{
		if ( dbExprType ( instance, filterExpr, keyLen ) == FTX_ERROR_EXPR )
		{
			return false;
		}
		isFilter = 1;
	}

	// what does the expression evaluate to
	if ( (keyType = dbExprType ( instance, keyExpr, keyLen )) == FTX_ERROR_EXPR )
	{
		return false;
	}

	QueryPerformanceCounter ( &startTime );

	try
	{
		auto ftx = _ftxCreate ( name.c_str ( ), keyExpr.c_str ( ), _O_RDWR | _O_BINARY, keyLen, sizeof ( currentRec ), isDescending, (char) keyType, filterExpr.c_str ( ), db->tbl->getUpdateCount()  );

		ftx.compileKey ( instance, ftx.getHeader ( )->keyExpr );

		currentRec = 1;
		lastRec = _tbLastrec ( db->tbl, dbConn.tblConn );

		if ( !_tbGoto ( db->tbl, dbConn.tblConn, 1 ) )
		{
			return false;
		}

		printf ( "indexing: \"%s\":  building: ", name.c_str ( ) );

		_ftxAddKeys ( &ftx, &ftxConn, FTX_LEAF_TYPE, (unsigned long) lastRec );

		nextPrintRec = lastRec / 10;
		while ( currentRec <= lastRec )
		{
			if ( nextPrintRec == currentRec )
			{
				printf ( "*" );
				nextPrintRec += lastRec / 10;
			}
			if ( !_tbGoto ( db->tbl, dbConn.tblConn, currentRec ) )
			{
				return false;
			}

			if ( !_tbIsDeleted ( db->tbl, dbConn.tblConn ) )
			{
				if ( isFilter )
				{
					if ( (cbFilter) (instance, db->tbl, &ftx, &ftxConn) )
					{
						key = (cb) (instance, db->tbl, &ftx, &ftxConn);
						_ftxKeyAddToIndex ( &ftx, &ftxConn, (char *) key, (char *) &currentRec );
					}
				} else
				{
					key = (cb) (instance, db->tbl, &ftx, &ftxConn);
					_ftxKeyAddToIndex ( &ftx, &ftxConn, (char *) key, (char *) &currentRec );
				}
			}
			currentRec++;
		}

		// adjust the file size... we may have had deleted records
		_ftxAdjustFileSize ( &ftx, &ftxConn );

		printf ( " sorting... " );

		_ftxKeySort ( &ftx, &ftxConn );

		QueryPerformanceCounter ( &endTime );

		QueryPerformanceFrequency ( &frequency );

		printf ( "done in %.2fs\r\n", static_cast<double>(endTime.QuadPart - startTime.QuadPart) / static_cast<double>(frequency.QuadPart) );
		return true;
	} catch ( errorNum e )
	{
		printf ( "error creating index file: %s\r\n", scCompErrorAsText ( int(e) ).c_str() );
		return false;
	}
}

bool dbCreateIndex ( vmInstance *instance, DATABASE *db, stringi const &name, stringi const &keyExpr, bool isDescending, void const *( *cb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn), stringi const &filterExpr, bool ( *filterCb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn) )
{
	return dbCreateIndexExtended ( instance, db, name, keyExpr, isDescending, cb, filterExpr, filterCb );
}

bool dbReindex ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	size_t		 loop;
	void const	*ptr;
	uint64_t	 lastRec;
	uint64_t	 currentRec;
	uint64_t	 startRec;

	{
		SRRWLocker l1 ( db->lock, true );

		dbSyncIndex ( instance, db, dbConn );
		dbGoCold ( instance, db, dbConn );

		startRec = _tbRecno ( db->tbl, dbConn->tblConn );

		currentRec = 1;
		lastRec = _tbLastrec ( db->tbl, dbConn->tblConn );

		for ( loop = 0; loop < db->ftx.size ( ); loop++ )
		{
			// reset the index back to nothing
			_ftxClear ( &db->ftx[loop], &dbConn->ftxConn[loop] );
			// make room for the maximum number of possible keys
			_ftxAddKeys ( &db->ftx[loop], &dbConn->ftxConn[loop], FTX_LEAF_TYPE, (unsigned long) lastRec );
		}

		while ( currentRec <= lastRec )
		{
			if ( !_tbGoto ( db->tbl, dbConn->tblConn, currentRec ) )
			{
				return false;
			}

			if ( !_tbIsDeleted ( db->tbl, dbConn->tblConn ) )
			{
				/* got to add each record over every index */
				for ( loop = 0; loop < db->ftx.size ( ); loop++ )
				{
					ptr = (db->ftxCallBack[loop])(instance, db->tbl, &db->ftx[loop], &dbConn->ftxConn[loop]);
					if ( ptr )
					{
						_ftxKeyAddToIndex ( &db->ftx[loop], &dbConn->ftxConn[loop], (char *) ptr, (char *) &currentRec );
					}
				}
			}

			currentRec++;
		}

		for ( loop = 0; loop < db->ftx.size ( ); loop++ )
		{
			// adjust the file size... we may have had deleted records
			_ftxAdjustFileSize ( &db->ftx[loop], &dbConn->ftxConn[loop] );
			// now, sort the index and generate branch keys
			_ftxKeySort ( &db->ftx[loop], &dbConn->ftxConn[loop] );
		}
	}

	dbGoto ( instance, db, dbConn, startRec );

	return true;
}

size_t dbFieldLen ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num )
{
	return _tbFieldSize ( db->tbl, dbConn->tblConn, num );
}

size_t dbFieldDec ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num )
{
	return _tbFieldDec ( db->tbl, dbConn->tblConn, num );
}

uint8_t dbFieldType ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num )
{
	return _tbFieldType ( db->tbl, dbConn->tblConn, num );
}

char *dbFieldName ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num, char *dest )
{
	return _tbFieldName ( db->tbl, dbConn->tblConn, num, dest );
}

size_t dbFieldPos ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, char const *name )
{
	return  _tbFieldPos ( db->tbl, dbConn->tblConn, name );
}

uint64_t dbLastRec ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return _tbLastrec ( db->tbl, dbConn->tblConn );
}

size_t dbFieldCount ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return _tbFieldCount ( db->tbl, dbConn->tblConn );
}

uint64_t dbRecno (vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return _tbRecno ( db->tbl, dbConn->tblConn );
}

size_t dbRecCount ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return _tbRecCount ( db->tbl, dbConn->tblConn );
}

bool dbIsDeleted ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return _tbIsDeleted ( db->tbl, dbConn->tblConn );
}

bool dbRecall ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return false;
}

bool dbFound ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	return dbConn->tblConn->isFound;
}

void dbCopy ( vmInstance *instance, DATABASE *db, char const *pathName )
{
	return _tbCopy ( db->tbl, pathName );
}

size_t dbRecordGet	( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, BUFFER *buffer )
{
	return _tbRecordGet ( db->tbl, dbConn->tblConn, buffer );
}

void dbRecordPut	( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, char *data, size_t dataLen )
{
	if ( !dbConn->tblConn->isHot )
	{
		SRRWLocker l1 ( db->lock, false );

		dbSyncIndex ( instance, db, dbConn );
		dbGoHot ( instance, db, dbConn );
	}
	_tbRecordPut ( db->tbl, dbConn->tblConn, data, dataLen );
}

size_t dbFreeListRebuild ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );
	dbGoHot ( instance, db, dbConn );
	return _tbFreeListBuild ( db->tbl, dbConn->tblConn );
}

int dbCmpKey ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, char const *key, size_t len )
{
	void const *ptr;
	int			res;

	SRRWLocker l1 ( db->lock, false );

	if ( !dbConn->ftxXRef[dbConn->userFtx] )
	{
		return 0;
	}

	ptr = (db->ftxCallBack[dbConn->ftxXRef[dbConn->userFtx]])(instance, db->tbl, &db->ftx[dbConn->ftxXRef[dbConn->userFtx]], &dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]]);
	if ( ptr )
	{
		// no need to limit length (one or the other ptr's will fail the match prior to overrun)
		res = (db->ftx[dbConn->ftxXRef[dbConn->userFtx]].cmpRtn) (dbConn->ftxConn[dbConn->ftxXRef[dbConn->userFtx]].knownKey, key, len);
		return (res);
	}

	return 0;
}

void dbDelete ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn )
{
	SRRWLocker l1 ( db->lock, false );

	dbSyncIndex ( instance, db, dbConn );
	dbGoHot ( instance, db, dbConn );

	_tbDelete ( db->tbl, dbConn->tblConn );
}
