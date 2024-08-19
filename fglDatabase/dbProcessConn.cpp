/*
	dbProcess	-	process buffers received by the dbIoServer

*/

#include "dbIoServer.h"
#include "Db.h"
#include "odbf.h"

int64_t dbProcessConn ( vmInstance *instance, DB_SERVER *dbServer, DB_SERVER_FD *fd, BUFFER *rxBuff, BUFFER *txBuff )
{
	DB_CONNECT_MSG			*dbCMsg = nullptr;
	DBS_MSG					*dbsMsg = nullptr;
	DBS_MSG					*rspMsg = nullptr;
	TABLE					*tbl = nullptr;
	DB_SERVER_DB			*dbs = nullptr;
	char					*extraData = nullptr;
	size_t					 extraDataLen{};
	size_t					 index{};
	char					*indexName = nullptr;

	if ( bufferSize ( rxBuff ) < sizeof ( *dbCMsg ) + sizeof ( *dbsMsg ) )
	{
		return (int64_t)( (sizeof ( *dbCMsg ) + sizeof ( *dbsMsg )) - rxBuff->size() );
	}

	dbsMsg			= (DBS_MSG *)(bufferBuff ( rxBuff ) + sizeof ( *dbCMsg ));
	extraData		= bufferBuff ( rxBuff ) + sizeof ( *dbsMsg ) + sizeof ( *dbCMsg );
	extraDataLen	= bufferSize ( rxBuff ) - (sizeof ( *dbsMsg ) + sizeof ( *dbCMsg ));

	// allocate response message
	bufferMakeRoom ( txBuff, sizeof ( *rspMsg ) );
	rspMsg = (DBS_MSG *)bufferBuff ( txBuff );
	bufferAssume ( txBuff, sizeof ( *rspMsg ) );

	rspMsg->msgType	= dbsMsg->msgType;
	rspMsg->status	= 0;

	switch ( dbsMsg->msgType )
	{
		case DBS_MSG_OPEN_DB:
			if ( !(fd->dbs = dbServer->dbGetTable ( dbsMsg->dat.req.dbOpen.tableName )) )
			{
				// can we open it
				if ( !(fd->dbs = dbServer->dbOpenTable (  instance, dbsMsg->dat.req.dbOpen.tableName )) )
				{
					rspMsg->status = static_cast<int>(GetLastError());
					if ( !rspMsg->status )
					{
						rspMsg->status = int(errorNum::scDATABASE_INTEGRITY);
					}
					break;
				}
			}

			// allocate a new database connection structure
			if ( !(fd->dbConn = dbServer->dbAllocateConnection()) )
			{
				fd->dbs = 0;
				rspMsg->status = int(errorNum::scTO_MANY_DB_CONNECTIONS);
				break;
			}
			if ( !(fd->dbConn->tblConn = dbServer->dbAllocateTableConnection()) )
			{
				dbServer->dbFreeConnection ( fd->dbConn );
				fd->dbs = 0;
				rspMsg->status = int(errorNum::scTO_MANY_DB_CONNECTIONS);
				break;
			}

			dbGoto ( instance, fd->dbs->db, fd->dbConn, 1 );

			rspMsg->dat.rsp.dbInfo.fieldCount	= (int) dbFieldCount ( instance, fd->dbs->db, fd->dbConn );
			txBuff->write ( fd->dbs->db->tbl->fields, sizeof ( DBFFIELDS ) * rspMsg->dat.rsp.dbInfo.fieldCount );

			fd->localDb = std::make_pair( fd->dbs->db, fd->dbConn );
			rxBuff->reset ( );
			break;

		case DBS_MSG_OPEN_DB_MULTI:
			if ( extraDataLen < dbsMsg->dat.req.dbOpenMulti.dataLen )
			{
				return ( dbsMsg->dat.req.dbOpenMulti.dataLen );
			}

			if ( !(fd->dbs = dbServer->dbGetTable ( dbsMsg->dat.req.dbOpenMulti.tableName )) )
			{
				// can we open it
				if ( !(fd->dbs = dbServer->dbOpenTable ( instance, dbsMsg->dat.req.dbOpenMulti.tableName )) )
				{
					// damn... error
					rspMsg->status = static_cast<int>(GetLastError());
					if ( !rspMsg->status )
					{
						rspMsg->status = int(errorNum::scDATABASE_INTEGRITY);
					}
					break;
				}
			}

			// allocate a new database connection structure
			if ( !(fd->dbConn = dbServer->dbAllocateConnection()) )
			{
				fd->dbs = 0;
				rspMsg->status = int(errorNum::scTO_MANY_DB_CONNECTIONS);
				break;
			} else if ( !(fd->dbConn->tblConn = dbServer->dbAllocateTableConnection()) )
			{
				dbServer->dbFreeConnection ( fd->dbConn );
				fd->dbs = 0;
				rspMsg->status = int(errorNum::scTO_MANY_DB_CONNECTIONS);
				break;
			}

			dbGoto ( instance, fd->dbs->db, fd->dbConn, 1 );

			rspMsg->dat.rsp.dbInfo.fieldCount	= (int)dbFieldCount ( instance, fd->dbs->db, fd->dbConn );
			bufferWrite ( txBuff, fd->dbs->db->tbl->fields, sizeof ( DBFFIELDS ) * rspMsg->dat.rsp.dbInfo.fieldCount );
			rspMsg = (DBS_MSG *) bufferBuff ( txBuff );

			// allocate a local database connection
			fd->localDb = std::make_pair ( fd->dbs->db, fd->dbConn );

			indexName = extraData;

			for ( index = 0; index < dbsMsg->dat.req.dbOpenMulti.nIndex; index++ )
			{
				std::string index ( indexName );
				if ( !dbOpenIndex ( instance, fd->dbs->db, fd->dbConn, index, dbEval, dbFilterEval ) )
				{
					rspMsg->status = static_cast<int>(GetLastError());
					goto multi_open_error;
				}
				while ( *indexName ) indexName++;
				indexName++;
			}
			rxBuff->reset ( );
			break;

multi_open_error:
			// free what this connection has allocated, which *may* not be the same as what the ftx has
			fd->dbConn->ftxConn.clear ( );
			dbServer->dbFreeTableConnection ( fd->dbConn->tblConn );
			fd->dbConn->tblConn = 0;
			dbServer->dbFreeConnection ( fd->dbConn );
			fd->dbConn = 0;
			fd->dbs = 0;
			if ( !rspMsg->status )
			{
				rspMsg->status = int(errorNum::scDATABASE_INTEGRITY);
			}
			break;
		case DBS_MSG_CLOSE_DB:
			dbServer->dbCloseDB ( dbsMsg->dat.req.dbClose.tableName, dbsMsg->dat.req.dbClose.denyPendingOpen ? true : false );
			break;
		case DBS_MSG_CREATE_DB:
			if ( extraDataLen < dbsMsg->dat.req.dbCreateDB.numFields * sizeof ( TBFIELD ) )
			{
				return static_cast<int64_t>(dbsMsg->dat.req.dbCreateDB.numFields * sizeof ( TBFIELD ) - extraDataLen);
			} else
			{
				SRRWLocker l1 ( dbServer->dbsAccess, true );

				// make double sure some other thread hasn't already built it!
				if ( !(dbs = dbServer->dbGetTable ( dbsMsg->dat.req.dbOpen.tableName )) )
				{
					// can we open it
					if ( !dbServer->dbOpenTable ( instance, dbsMsg->dat.req.dbOpen.tableName ) )
					{
						// nope.. doesn't exist so really build t
						if ( !(tbl = _tbBuild ( dbsMsg->dat.req.dbCreateDB.fName, (TBFIELD *) extraData, dbsMsg->dat.req.dbCreateDB.numFields )) )
						{
							rspMsg->status = int(errorNum::scINVALID_PARAMETER);
						} else
						{
							try
							{
								_tbCreate ( tbl );
							} catch ( ... )
							{
								rspMsg->status = static_cast<int>(GetLastError());
							}
							delete tbl;
					}
					}
				} else
				{
					// we actually did get it so we have to release our reference to it... create is NOT the same as open
					dbServer->dbReleaseTable ( dbs );
				}
			}
			bufferReset ( rxBuff );
			break;

		case DBS_MSG_SHUTDOWN:
			printf ( "Shutting down database engine:\r\n" );
			GenerateConsoleCtrlEvent ( CTRL_CLOSE_EVENT, 0 );
			break;
		case DBS_MSG_DB_COPY:
			if ( extraDataLen < (unsigned int)dbsMsg->dat.req.dbCopy.dataLen )
			{
				// don't have the full request yet
				return dbsMsg->dat.req.dbCopy.dataLen;
			} else
			{
					/* acquire a lock on database addition into the server */
				SRRWLocker l1 ( dbServer->dbsAccess, false );

				auto it = dbServer->dbs.find ( stringi ( extraData ) );
				if ( it != dbServer->dbs.end ( ) )
				{
					dbCopy ( instance, it->second->db, dbsMsg->dat.req.dbCopy.fPath );
				}
			}
			break;

		default:
			/* invalid message here... damn... */
			rspMsg->msgType  = dbsMsg->msgType;
			rspMsg->status = int ( errorNum::scINTERNAL );
	}

	return ( 0 );
}