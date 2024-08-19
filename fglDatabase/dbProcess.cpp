/*
	dbProcess	-	process buffers received by the dbIoServer

*/

#include "dbIoServer.h"
#include "Db.h"
#include "odbf.h"

#if 1 || _DEBUG
static char	const *msgTypeNames[] = {
	"DBS_MSG_OPEN_DB",
	"DBS_MSG_CLOSE_DB",
	"DBS_MSG_OPEN_DB_MIRROR",
	"DBS_MSG_RESET_CONN",
	"DBS_MSG_CLOSE_DB_CONN",
	"DBS_MSG_OPEN_INDEX",
	"DBS_MSG_CLOSE_INDEX",
	"DBS_MSG_OPEN_FILTER",
	"DBS_MSG_CLOSE_FILTER",

	"DBS_MSG_CACHE_POLICY",

	"DBS_MSG_CREATE_DB",
	"DBS_MSG_CREATE_INDEX",
	"DBS_MSG_CREATE_FILTER",

	"DBS_MSG_GET_STATUS",

	"DBS_MSG_GO_TOP",
	"DBS_MSG_GO_BOTTOM",
	"DBS_MSG_SKIP",
	"DBS_MSG_GOTO",

	"DBS_MSG_DELETE",
	"DBS_MSG_RECALL",

	"DBS_MSG_FIELD_INFO",
	"DBS_MSG_FIELD_INFO_BY_NAME",
	"DBS_MSG_FIELD_GET",
	"DBS_MSG_FIELD_GET_BY_NAME",
	"DBS_MSG_FIELD_PUT",

	"DBS_MSG_RECORD_GET",
	"DBS_MSG_RECORD_PUT",
	
	"DBS_MSG_SET_ORDER",
	"DBS_MSG_SEEK",
	"DBS_MSG_REINDEX,",
	"DBS_MSG_FIND_SUBSET",
	"DBS_MSG_COUNT_MATCHING",
	
	"DBS_MSG_APPEND",
	
	"DBS_MSG_DB_RLOCK",
	"DBS_MSG_DB_RUNLOCK",
	"DBS_MSG_DB_COMMIT",

	"DBS_MSG_DB_INFO",
	"DBS_MSG_DB_COPY",
	"DBS_MSG_DB_DELETE",

	"DBS_MSG_TRAN_REBUILD",

	"DBS_MSG_SHUTDOWN",
	"DBS_MSG_RESTART",

	"DBS_MSG_DB_BACKUP",
	"DBS_MSG_DB_BACKUP_RECORD",
	"DBS_MSG_DB_BACKUP_STATUS",
	"DBS_MSG_DB_BACKUP_REBUILD",

	"DBS_MSG_DB_GET_SERVER_TIME",

	"DBS_MSG_DB_INDEX_INFO",

	"DBS_MSG_CMP_KEY",

	"DBS_MSG_OPEN_DB_MULTI",

	"DBS_MSG_SEEK_LAST",
};
#endif

uint64_t getFileSize ( HANDLE handle )
{
	LARGE_INTEGER	 newSize;
	GetFileSizeEx ( handle, &newSize );
	return newSize.QuadPart;
}

void dbSetStatus ( vmInstance *instance, DB_SERVER_DB *dbs, DATABASE_CONNECTION *dbConn, DBS_MSG *rspMsg )
{
	auto db = dbs->db;
	rspMsg->dat.rspStatus.recNo = (int) dbRecno ( instance, db, dbConn );
	rspMsg->dat.rspStatus.isEof = dbEOF ( instance, db, dbConn );
	rspMsg->dat.rspStatus.isBof = dbBOF ( instance, db, dbConn );
	rspMsg->dat.rspStatus.isDeleted = dbIsDeleted ( instance, db, dbConn );
	rspMsg->dat.rspStatus.isFound = dbFound ( instance, db, dbConn );
	rspMsg->dat.rspStatus.isDirty = dbConn->tblConn->isHot;
}

int64_t dbProcessMessage ( vmInstance *instance, DB_SERVER_DB *dbs, DATABASE_CONNECTION *dbConn, BUFFER *rxBuff, BUFFER *txBuff, DB_SEND dbSend, void *param )
{
	DATABASE			*db;
	DBS_MSG				*dbsMsg;
	DBS_MSG				*rspMsg;
	char				*extraData;
	size_t				 extraDataLen;
	size_t				 numRecs;
	void				*tmpPtr;
	int					 fieldNum;
	int64_t				 res = 0;
	int					 recSize;
	
	if ( bufferSize ( rxBuff ) < sizeof ( *dbsMsg ) )
	{
		// not enought for a message header... return our needed length.
		return int64_t ( sizeof ( DBS_MSG ) - bufferSize ( rxBuff ) );
	}

	dbsMsg			= (DBS_MSG *)bufferBuff ( rxBuff );
	extraData		=rxBuff->data<char *>()+ sizeof ( *dbsMsg );
	extraDataLen	= rxBuff->size() - sizeof ( *dbsMsg );

	// allocate response message
	bufferMakeRoom ( txBuff, sizeof ( *rspMsg ) );
	rspMsg = (DBS_MSG *)bufferBuff ( txBuff );
	bufferAssume ( txBuff, sizeof ( *rspMsg ) );

	rspMsg->msgType	= dbsMsg->msgType;
	rspMsg->status	= 0;

	db = dbs->db;

	if ( db->dbDef->traceOutput )
	{
		printf ( "dbConn %p: db:%40s    found=%i  recNo=%4I64u  b/eof(%i,%i)  hot=%i  order=%I64u  msg: %s\r\n",
					dbConn,
					db->tbl->alias,
					dbConn->tblConn->isFound, 
					dbConn->tblConn->recNo,
					dbConn->tblConn->bof,
					dbConn->tblConn->eof,
					dbConn->tblConn->isHot,
					dbConn->userFtx,
					msgTypeNames[dbsMsg->msgType] );
	}
	
#if 0
	printf ( "dbConn %p: db:%40s    found=%i  recNo=%4I64u  b/eof(%i,%i)  hot=%i  order=%I64u  msg: %s\r\n",
			 dbConn,
			 db->tbl->alias,
			 dbConn->tblConn->isFound,
			 dbConn->tblConn->recNo,
			 dbConn->tblConn->bof,
			 dbConn->tblConn->eof,
			 dbConn->tblConn->isHot,
			 dbConn->userFtx,
			 msgTypeNames[dbsMsg->msgType] );
#endif

	switch ( dbsMsg->msgType )
	{
		case DBS_MSG_GO_TOP:
			dbGoTop ( instance, db, dbConn );
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;
		case DBS_MSG_GO_BOTTOM:
			dbGoBottom ( instance, db, dbConn );
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;
		case DBS_MSG_SKIP:
			dbSkip ( instance, db, dbConn, dbsMsg->dat.req.dbSkip.count );
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;
		case DBS_MSG_GOTO:
			dbGoto ( instance, db, dbConn, dbsMsg->dat.req.dbGoto.recNo );
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;
		case DBS_MSG_APPEND:
			rspMsg->status = (int)dbAppend ( instance, db, dbConn );
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;
		case DBS_MSG_OPEN_INDEX:
			if ( !dbOpenIndex ( instance, db, dbConn, std::string ( dbsMsg->dat.req.dbOpenIndex.fName ), dbEval, dbFilterEval ) )
			{
				rspMsg->status =(int)(int64_t)GetLastError();
				if ( !rspMsg->status )
				{
					rspMsg->status = int(errorNum::scDATABASE_INTEGRITY);
				}
			}
			break;
		case DBS_MSG_SEEK:
			if ( extraDataLen < dbsMsg->dat.req.dbSeek.dataLen )
			{
				// don't have the full request yet
				res = (int64_t)dbsMsg->dat.req.dbSeek.dataLen - (int64_t)extraDataLen;
				break;
			}

			if ( dbSeekLen ( instance, db, dbConn, extraData, dbsMsg->dat.req.dbSeek.dataLen ) )
			{
				rspMsg->status = 1;
			} else
			{
				if ( !dbsMsg->dat.req.dbSeek.softSeek )
				{
					rspMsg->status = 0;
					dbGoto ( instance, db, dbConn, dbLastRec ( instance, db, dbConn ) + 1 );
				}
			}
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;
		case DBS_MSG_SEEK_LAST:
			if ( extraDataLen < dbsMsg->dat.req.dbSeek.dataLen )
			{
				// don't have the full request yet
				res = (int64_t)dbsMsg->dat.req.dbSeek.dataLen - (int64_t)extraDataLen;
				break;
			}

			if ( dbSeekLastLen ( instance, db, dbConn, extraData, dbsMsg->dat.req.dbSeek.dataLen ) )
			{
				rspMsg->status = 1;
			} else
			{
				if ( !dbsMsg->dat.req.dbSeek.softSeek )
				{
					rspMsg->status = 0;
					dbGoto ( instance, db, dbConn, dbLastRec ( instance, db, dbConn ) + 1 );
				}
			}
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;
		case DBS_MSG_CMP_KEY:
			if ( extraDataLen < dbsMsg->dat.req.dbSeek.dataLen )
			{
				// don't have the full request yet
				res = (int64_t)dbsMsg->dat.req.dbSeek.dataLen - (int64_t)extraDataLen;
				break;
			}

			rspMsg->status = dbCmpKey ( instance, db, dbConn, extraData, dbsMsg->dat.req.dbSeek.dataLen );
			break;
		case DBS_MSG_FIND_SUBSET:
			if ( extraDataLen < dbsMsg->dat.req.dbFindSubset.dataLen )
			{
				// don't have the full request yet
				res = (int64_t)dbsMsg->dat.req.dbFindSubset.dataLen - (int64_t)extraDataLen;
				break;
			}

			// null terminate the key
			rxBuff->put ( 0 );
			dbsMsg		= rxBuff->data<DBS_MSG *> ();
			extraData	= rxBuff->data<char *>() + sizeof ( *dbsMsg );

			if ( (numRecs = dbFindSubset ( instance, db, dbConn, extraData, dbsMsg->dat.req.dbFindSubset.dataLen, dbsMsg->dat.req.dbFindSubset.every, &tmpPtr )) )
			{
				rspMsg->status = 0;
				rspMsg->dat.rsp.dbFindSubset.nRecords = (int)numRecs;
				bufferWrite ( txBuff, tmpPtr, sizeof ( long *) * numRecs );

				free ( tmpPtr );
			} else
			{
				rspMsg->dat.rsp.dbFindSubset.nRecords = 0;
				rspMsg->status = 0;
			}
			break;
		case DBS_MSG_COUNT_MATCHING:
			if ( extraDataLen < dbsMsg->dat.req.dbCountMatching.dataLen )
			{
				// don't have the full request yet
				res = (int64_t)dbsMsg->dat.req.dbCountMatching.dataLen - (int64_t)extraDataLen;
				break;
			}
			// null terminate the key
			rxBuff->put ( 0 );
			dbsMsg		= rxBuff->data<DBS_MSG *> ( );
			extraData	= rxBuff->data<char *>() + sizeof ( *dbsMsg );
			if ( (numRecs = dbCountMatching ( instance, db, dbConn, extraData, dbsMsg->dat.req.dbCountMatching.dataLen )) )
			{
				rspMsg->status = 0;
				rspMsg->dat.rsp.dbCountMatching.nRecords = (int)numRecs;
			} else
			{
				rspMsg->dat.rsp.dbCountMatching.nRecords = 0;
				rspMsg->status = 0;
			}
			break;
		case DBS_MSG_REINDEX:
			if ( !dbReindex ( instance, db, dbConn ) )
			{
				rspMsg->status = int(errorNum::scINTERNAL);
			}
			break;
		case DBS_MSG_CREATE_INDEX:							
			if ( extraDataLen < dbsMsg->dat.req.dbCreateIndex.dataLen )
			{
				// don't have the full request yet
				res = (int64_t)dbsMsg->dat.req.dbCreateIndex.dataLen - (int64_t)extraDataLen;
				break;
			} else
			{
				SRRWLocker l1 ( db->lock, true );
				
				auto it = db->ftxLookup.find ( stringi ( dbsMsg->dat.req.dbCreateIndex.fName ) );
				if ( it == db->ftxLookup.end ( ) )
				{
					// doesn't exist
					// null terminate the key
					rxBuff->put ( 0 );
					dbsMsg = (DBS_MSG *) bufferBuff ( rxBuff );
					extraData = bufferBuff ( rxBuff ) + sizeof ( *dbsMsg );
					if ( !rspMsg->status )
					{
						if ( !dbCreateIndexExtended ( instance, db, std::string ( dbsMsg->dat.req.dbCreateIndex.fName ), std::string (  extraData ), dbsMsg->dat.req.dbCreateIndex.isDescending ? true : false, dbEval, std::string (), dbFilterEval ) )
						{
							rspMsg->status = int(errorNum::scINVALID_KEY);
						} else
						{
							if ( !dbOpenIndex ( instance, db, dbConn, std::string ( dbsMsg->dat.req.dbOpenIndex.fName ), dbEval, dbFilterEval ) )
							{
								rspMsg->status = (int)GetLastError();
								if ( !rspMsg->status )
								{
									rspMsg->status = int(errorNum::scDATABASE_INTEGRITY);
								}
							} else
							{
								dbDefWrite ( db );
							}
						}
					}
				}
			}
			break;

		case DBS_MSG_SET_ORDER:
			rspMsg->dat.rsp.dbOrder.oldOrder = (int)dbOrder ( instance, db, dbConn, dbsMsg->dat.req.dbOrder.orderNum );
			break;

		case DBS_MSG_RESET_CONN:
			dbCommit ( instance, db, dbConn );

			while ( dbConn->recLock.size() )
			{
				dbUnlockRecAll ( instance, db, dbConn, *dbConn->recLock.begin() );
			}
			dbConn->recLock.clear ( );

			// free what this connection has allocated, which *may* not be the same as what the database has
			dbConn->ftxConn.clear ( );

			if ( dbConn->tblConn ) dbs->server->dbFreeTableConnection ( dbConn->tblConn );
			dbConn->tblConn = 0;
			res = -1;
			break;

		case DBS_MSG_CLOSE_DB_CONN:
			// force a disconnect
			res = -2;
			break;

		case DBS_MSG_DB_INFO:
			rspMsg->dat.rsp.dbInfo.fieldCount	= (int)dbFieldCount ( instance, db, dbConn );
			rspMsg->dat.rsp.dbInfo.tableSize	= (int64_t)getFileSize ( db->tbl->fileHandle );
			if ( db->tbl->blob )
			{
				/* add in the size of the blob file */
				rspMsg->dat.rsp.dbInfo.tableSize	+= (int64_t)getFileSize ( db->tbl->blob->fileHandle );
			}
			/* add in the size of any index files */
			for ( auto &it : db->ftx )
			{
				rspMsg->dat.rsp.dbInfo.tableSize += (int64_t)getFileSize ( it.fileHandle );
			}
			rspMsg->dat.rsp.dbInfo.numRecords	= (int64_t)db->tbl->getHeader()->nAllocatedRecords;
			rspMsg->dat.rsp.dbInfo.lastRecord	= (int64_t)db->tbl->getHeader ( )->lastRecord;
			rspMsg->dat.rsp.dbInfo.recSize		= static_cast<int>(db->tbl->getHeader ( )->recDataSize);
			bufferWrite ( txBuff, db->tbl->fields, sizeof ( DBFFIELDS ) * db->tbl->fcount );
			break;
		case DBS_MSG_CLOSE_INDEX:
			/* no op... silly really... all index's are ALWAYS open for consistency
				this is an operation message NOT a maintenance message
			*/
			break;

		case DBS_MSG_DB_RLOCK:
			if ( dbsMsg->dat.req.dbLockRec.recno )
			{
				rspMsg->status = dbLockRec ( instance, db, dbConn, dbsMsg->dat.req.dbLockRec.recno );
			} else
			{
				rspMsg->status = dbLockRec ( instance, db, dbConn, dbRecno ( instance, db, dbConn ) );
			}
			break;

		case DBS_MSG_DB_RUNLOCK:
			if ( dbsMsg->dat.req.dbUnlockRec.recno )
			{
				rspMsg->status = dbUnlockRec ( instance, db, dbConn, dbsMsg->dat.req.dbLockRec.recno );
			} else
			{
				rspMsg->status = dbUnlockRec ( instance, db, dbConn, dbRecno ( instance, db, dbConn ) );
			}
			break;

		case DBS_MSG_DB_COMMIT:				
			// actually unlocking a record will do a commit of that record... no reason to force it
			for ( auto &it : dbConn->recLock )
			{
				while ( dbUnlockRec ( instance, db, dbConn, it ) )
				{
					// TODO make a function to force this!
					/* keep unlocking repeatedly to eat up all the ref counts*/
				}
			}
			break;

		case DBS_MSG_DELETE:
			if ( !dbIsRecLocked ( instance, db, dbRecno ( instance, db, dbConn ) )  )
			{
				if ( !dbLockRec ( instance, db, dbConn, dbRecno ( instance, db, dbConn ) ) )
				{
					/* someone else own's the lock... damn... can't do the update */
					rspMsg->status = int(errorNum::scDATABASE_RECORD_LOCKED_BY_OTHER);
					break;
				}
			}

			// deletion must be an atomic operation... wrap it around the protection structure to keep dbAppend from trying to use a record that is locked, deleted, but not yet unlocked
			{
				SRRWLocker l1 ( db->lock, true );
				dbDelete ( instance, db, dbConn );
				dbUnlockRec ( instance, db, dbConn, dbRecno ( instance, db, dbConn ) );
			}
			dbSetStatus ( instance, dbs, dbConn, rspMsg );
			break;

		case DBS_MSG_RECALL:
			rspMsg->status = int(errorNum::scUNSUPPORTED);
			break;

		case DBS_MSG_FIELD_PUT:
			if ( extraDataLen < dbsMsg->dat.req.dbFieldPut.dataLen )
			{
				// not enough data
				res = (int64_t)dbsMsg->dat.req.dbFieldPut.dataLen - (int64_t)extraDataLen;
				break;
			}

			if ( !dbIsRecLocked ( instance, db, dbRecno ( instance, db, dbConn ) ) )
			{
				if ( !dbLockRec ( instance, db, dbConn, dbRecno ( instance, db, dbConn ) ) )
				{
					/* someone else own's the lock... damn... can't do the update */
					rspMsg->status = int(errorNum::scDATABASE_RECORD_LOCKED_BY_OTHER);
					break;
				}
			}

			// early out
			dbSend ( param );

			dbFieldPut	(  instance, 
						   db,
							dbConn,
							dbsMsg->dat.req.dbFieldPut.fieldNum,
							extraData,
							dbsMsg->dat.req.dbFieldPut.dataLen
						);
			break;			

		case DBS_MSG_FIELD_INFO_BY_NAME:
			if ( !(rspMsg->dat.rsp.dbFieldInfo.fieldNum = (int)dbFieldPos  ( instance, db, dbConn, dbsMsg->dat.req.dbFieldInfoByName.fieldName )) )
			{
				rspMsg->status = int(errorNum::scFIELD_NOT_FOUND);
			} else
			{
				strncpy_s ( rspMsg->dat.rsp.dbFieldInfo.fieldName, sizeof ( rspMsg->dat.rsp.dbFieldInfo.fieldName ), dbsMsg->dat.req.dbFieldInfoByName.fieldName, _TRUNCATE );
				rspMsg->dat.rsp.dbFieldInfo.fieldType	= (char) dbFieldType ( instance, db, dbConn, rspMsg->dat.rsp.dbFieldInfo.fieldNum );
				rspMsg->dat.rsp.dbFieldInfo.fieldLen	= (int) dbFieldLen  ( instance, db, dbConn, rspMsg->dat.rsp.dbFieldInfo.fieldNum );
				rspMsg->dat.rsp.dbFieldInfo.fieldDec	= (int) dbFieldDec  ( instance, db, dbConn, rspMsg->dat.rsp.dbFieldInfo.fieldNum );
			}
			break;

		case DBS_MSG_FIELD_INFO:
			if ( (dbsMsg->dat.req.dbFieldInfo.fieldNum >= 0) && (dbsMsg->dat.req.dbFieldInfo.fieldNum <= db->tbl->fcount) )
			{
				dbFieldName ( instance, db, dbConn, dbsMsg->dat.req.dbFieldInfo.fieldNum, rspMsg->dat.rsp.dbFieldInfo.fieldName );
				rspMsg->dat.rsp.dbFieldInfo.fieldNum	= dbsMsg->dat.req.dbFieldInfo.fieldNum;
				rspMsg->dat.rsp.dbFieldInfo.fieldType	= (char) dbFieldType ( instance, db, dbConn, rspMsg->dat.rsp.dbFieldInfo.fieldNum );
				rspMsg->dat.rsp.dbFieldInfo.fieldLen	= (int) dbFieldLen  ( instance, db, dbConn, rspMsg->dat.rsp.dbFieldInfo.fieldNum );
				rspMsg->dat.rsp.dbFieldInfo.fieldDec	= (int) dbFieldDec  ( instance, db, dbConn, rspMsg->dat.rsp.dbFieldInfo.fieldNum );
			} else
			{
				rspMsg->status = int(errorNum::scFIELD_NOT_FOUND);
			}
			break;

		case DBS_MSG_FIELD_GET:
			bufferMakeRoom ( txBuff, dbFieldLen ( instance, db, dbConn, dbsMsg->dat.req.dbFieldGet.fieldNum ) );
			rspMsg = (DBS_MSG *)bufferBuff ( txBuff );
			rspMsg->dat.rsp.dbFieldGet.lenData = (int) dbFieldGet ( instance, db, dbConn, dbsMsg->dat.req.dbFieldGet.fieldNum, bufferBuff ( txBuff ) + bufferSize ( txBuff ) );
			bufferAssume ( txBuff, rspMsg->dat.rsp.dbFieldGet.lenData );
			break;

		case DBS_MSG_FIELD_GET_BY_NAME:
			if ( !(fieldNum = (int) dbFieldPos ( instance, db, dbConn, dbsMsg->dat.req.dbFieldGetByName.fieldName )) )
			{
				rspMsg->status = int(errorNum::scINVALID_FIELD);
			} else
			{
				bufferMakeRoom ( txBuff, dbFieldLen  ( instance, db, dbConn, rspMsg->dat.rsp.dbFieldInfo.fieldNum ) + 1 );		// add 1 for terminating null for 'S' type
				rspMsg = (DBS_MSG *)bufferBuff ( txBuff );

				rspMsg->dat.rsp.dbFieldGetByName.fieldType	= (char) dbFieldType ( instance, db, dbConn, fieldNum );
				rspMsg->dat.rsp.dbFieldGetByName.fieldDec	= (int) dbFieldDec  ( instance, db, dbConn, fieldNum );
				rspMsg->dat.rsp.dbFieldGetByName.fieldLen	= (int) dbFieldGet ( instance, db, dbConn, fieldNum, bufferBuff ( txBuff ) + bufferSize ( txBuff )  );

				bufferAssume ( txBuff, rspMsg->dat.rsp.dbFieldGetByName.fieldLen );
			}
			break;

		case DBS_MSG_RECORD_GET:
			recSize = (int) dbRecordGet ( instance, db, dbConn, txBuff );
			rspMsg = (DBS_MSG *)bufferBuff ( txBuff );
			rspMsg->dat.rsp.dbRecordGet.recSize = recSize;
			break;

		case DBS_MSG_RECORD_PUT:
			if ( extraDataLen < dbsMsg->dat.req.dbRecordPut.dataLen )
			{
				// not enough data
				res = static_cast<int64_t>(dbsMsg->dat.req.dbRecordPut.dataLen - extraDataLen);
				break;
			}

			if ( !dbIsRecLocked ( instance, db, dbRecno ( instance, db, dbConn ) ) )
			{
				if ( !dbLockRec ( instance, db, dbConn, dbRecno ( instance, db, dbConn ) )  )
				{
					/* someone else own's the lock... damn... can't do the update */
					rspMsg->status = int(errorNum::scDATABASE_RECORD_LOCKED_BY_OTHER);
					break;
				}
			}

			// early out
			dbSend ( param );

			dbRecordPut ( instance, db, dbConn, extraData, extraDataLen );
			break;

		case DBS_MSG_DB_BACKUP_STATUS:
			rspMsg->dat.rsp.dbBackupStatus.maxBackupRecord		= (long)dbs->copyMax;
			rspMsg->dat.rsp.dbBackupStatus.currentBackupRecord	= (long)dbs->copyCurrent;
			break;

		case DBS_MSG_GET_STATUS:
			rspMsg->dat.rsp.dbStatus.recNo		= (int) dbRecno ( instance, db, dbConn );
			rspMsg->dat.rsp.dbStatus.isEof		= dbEOF ( instance, db, dbConn );
			rspMsg->dat.rsp.dbStatus.isBof		= dbBOF ( instance, db, dbConn );
			rspMsg->dat.rsp.dbStatus.isDeleted	= dbIsDeleted ( instance, db, dbConn );
			rspMsg->dat.rsp.dbStatus.isFound	= dbFound ( instance, db, dbConn );
			rspMsg->dat.rsp.dbStatus.isDirty	= dbConn->tblConn->isHot;
			break;

		case DBS_MSG_GET_REC_COUNT:
			rspMsg->dat.rsp.dbGetRecCount.recCount	= (int) dbRecno ( instance, db, dbConn );
			break;

		case DBS_MSG_CLOSE_FILTER:
			break;

		case DBS_MSG_CREATE_FILTER:
		case DBS_MSG_DB_DELETE:
			rspMsg->status = int(errorNum::scUNSUPPORTED);
			break;

		case DBS_MSG_DB_GET_SERVER_TIME:
			{
				SYSTEMTIME		systemTime;
				FILETIME		fileTime;
				ULARGE_INTEGER	uLarge{};

				GetSystemTime (	&systemTime );
				SystemTimeToFileTime ( &systemTime, &fileTime );

				uLarge.HighPart = fileTime.dwHighDateTime;
				uLarge.LowPart	= fileTime.dwLowDateTime;

				rspMsg->dat.rsp.dbGetServerTime.time = (double) (signed __int64) (uLarge.QuadPart / 100000);
			}
			break;

		default:
			/* invalid message here... damn... */
			rspMsg->status = int ( errorNum::scINTERNAL );
	}
	return res;
}