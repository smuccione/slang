/*
	WEB Server header file

*/

#pragma once

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "Utility/settings.h"

#include <winsock2.h>
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <unordered_map>
#include <Utility/stringi.h>
#include <Utility/buffer.h>
#include <Utility/util.h>
#include <Target/vmTask.h>

#include <openssl/sha.h>

#include "dbConfig.h"
#include "dbDef.h"
#include "ftx.h"
#include "odbf.h"
#include "Db.h"

#define MAX_NUM_INDEX_PER_DB	256

struct databaseStatus
{
	int				recNo;
	int				isEof;
	int				isBof;
	int				isDeleted;
	int				isFound;
	int				isDirty;
};

enum DBS_MSG_TYPE {
	DBS_MSG_OPEN_DB,
	DBS_MSG_CLOSE_DB,
	DBS_MSG_OPEN_DB_MIRROR,
	DBS_MSG_RESET_CONN,
	DBS_MSG_CLOSE_DB_CONN,
	DBS_MSG_OPEN_INDEX,
	DBS_MSG_CLOSE_INDEX,
	DBS_MSG_OPEN_FILTER,
	DBS_MSG_CLOSE_FILTER,

	DBS_MSG_CACHE_POLICY,

	DBS_MSG_CREATE_DB,
	DBS_MSG_CREATE_INDEX,
	DBS_MSG_CREATE_FILTER,

	DBS_MSG_GET_STATUS,

	DBS_MSG_GO_TOP,
	DBS_MSG_GO_BOTTOM,
	DBS_MSG_SKIP,
	DBS_MSG_GOTO,

	DBS_MSG_DELETE,
	DBS_MSG_RECALL,

	DBS_MSG_FIELD_INFO,
	DBS_MSG_FIELD_INFO_BY_NAME,
	DBS_MSG_FIELD_GET,
	DBS_MSG_FIELD_GET_BY_NAME,
	DBS_MSG_FIELD_PUT,

	DBS_MSG_RECORD_GET,
	DBS_MSG_RECORD_PUT,

	DBS_MSG_SET_ORDER,
	DBS_MSG_SEEK,
	DBS_MSG_REINDEX,	
	DBS_MSG_FIND_SUBSET,
	DBS_MSG_COUNT_MATCHING,

	DBS_MSG_APPEND,

	DBS_MSG_DB_RLOCK,
	DBS_MSG_DB_RUNLOCK,
	DBS_MSG_DB_COMMIT,

	DBS_MSG_DB_INFO,
	DBS_MSG_DB_COPY,
	DBS_MSG_DB_DELETE,

	DBS_MSG_TRAN_REBUILD,

	DBS_MSG_SHUTDOWN,
	DBS_MSG_RESTART,

	DBS_MSG_DB_BACKUP,
	DBS_MSG_DB_BACKUP_RECORD,
	DBS_MSG_DB_BACKUP_STATUS,
	DBS_MSG_DB_BACKUP_REBUILD,

	DBS_MSG_DB_GET_SERVER_TIME,

	DBS_MSG_DB_INDEX_INFO,

	DBS_MSG_CMP_KEY,

	DBS_MSG_OPEN_DB_MULTI,

	DBS_MSG_GET_REC_COUNT,

	DBS_MSG_SEEK_LAST,
};

#pragma pack ( push, 4 )

struct DBS_MSG {
	DBS_MSG_TYPE	msgType;

	int				status;			/* status... 0 = success... else error num */

	enum deleteTypes {
		DB_DELETE_ALL = 1,
		DB_DELETE_ALL_INDEX = 2,
		DB_DELETE_INDEX = 3
	};

	union {
		union {
			struct {
				char			tableName[MAX_PATH];
			} dbOpen;
			struct {
				char			tableName[MAX_PATH];
				int				nIndex;
				unsigned int	dataLen;
			} dbOpenMulti;
			struct {
				char			tableName[MAX_PATH];
				int				denyPendingOpen;
			} dbClose;
			struct {
				char			tableName[MAX_PATH];
			} dbOpenMirror;

			/* open/close structures */
			struct {
				char			fName[MAX_PATH];
			} dbOpenIndex;
			struct {
				char			fName[MAX_PATH];
				int				include;
			} dbOpenFilter;

			/* database configuration */
			struct {
				int				policy;
			} dbCachePolicy;

			/* navigation structures */
			struct {
				int				count;
			} dbSkip;
			struct {
				int				recNo;
			} dbGoto;
			struct {
				int				dataLen;
				int				softSeek;
			} dbSeek;
			struct {
				int				orderNum;
			} dbOrder;
			struct {
				int				dataLen;
				int				every;
			} dbFindSubset;
			struct {
				int				dataLen;
			} dbCountMatching;

			/* assign/access structures */
			struct {
				int				fieldNum;
			} dbFieldInfo;
			struct {
				char			fieldName[241];			/* look up the field info */
			} dbFieldInfoByName;
			struct {
				int				fieldNum;
			} dbFieldGet;
			struct {
				char			fieldName[241];			/* look up the field info */
			} dbFieldGetByName;
			struct {
				int				fieldNum;
				int				dataLen;
			} dbFieldPut;
			struct {
				int				dataLen;
			} dbRecordPut;

			/* locking structures */
			struct {
				long			recno;					/* record number to lock, 0 = current rec */
			} dbLockRec;
			struct {
				long			recno;					/* record number to lock, 0 = current rec */
			} dbUnlockRec;

			/* creation structures */
			struct {
				char			fName[MAX_PATH];
				int				numFields;
			} dbCreateDB;
			struct {
				char			fName[MAX_PATH];
				int				isDescending;
				int				dataLen;
			} dbCreateIndex;
			struct {
				char			fName[MAX_PATH];
				int				dataLen;
			} dbCreateFilter;

			/* db global structures */
			struct {
				char			fPath[MAX_PATH];		/* destination directory to copy to */
				int				dataLen;
			} dbCopy;
			struct {
				char			fPath[MAX_PATH];			/* destination directory to copy to */
				int				dataLen;
			} dbBackup;
			struct {
				deleteTypes		deleteType;
				char			fName[MAX_PATH];
			} dbDelete;
			struct {
				int	order;
			} dbIndexInfo;
			struct {
				int				dataLen;
				int				softSeek;
			} dbCmpKey;
		} req;
		union {
			struct {
				int				fieldCount;
			} dbOpen;
			struct {
				int				conId;					/* connnection identifier... * to DBS_CON structure */
			} dbAllocConn;
			struct {
				int				lenData;
			} dbFieldGet;
			struct {
				int				fieldNum;
				int				fieldLen;
				int				fieldDec;
				char			fieldType;
			} dbFieldGetByName;
			struct {
				int				fieldLen;
			} dbFieldGetLen;
			struct {
				char			fieldName[241];
				int				fieldNum;
				int				fieldLen;
				int				fieldDec;
				char			fieldType;
			} dbFieldInfo;
			struct {
				int				recSize;
			} dbRecordGet;
			databaseStatus		dbStatus;
			struct {
				int				oldOrder;
			} dbOrder;
			struct {
				int				fieldCount;
				int				recSize;
				__int64			tableSize;
				__int64			numRecords;
				__int64			lastRecord;
			} dbInfo;
			struct {
				int				isDescending;
				char			key[221];
			} dbIndexInfo;
			struct {
				int				nRecords;
			} dbFindSubset;
			struct {
				int				nRecords;
			} dbCountMatching;
			struct {
				time_t			endTime;
			} dbTranRebuild;
			struct {
				unsigned long	nSeeks;
				unsigned long	nBytesRead;			
				unsigned long	nBytesWritten;
				unsigned long	nAppendedRecords;
				unsigned long	nRecordsModified;
				unsigned long	nRecordsAccessed;
				unsigned long	nConnections;
				unsigned long	nSecsAlive;
			} dbStats;
			struct {
				unsigned long	maxBackupRecord;
				unsigned long	currentBackupRecord;
			} dbBackupStatus;
			struct {
				double			time;
			} dbGetServerTime;
			struct  {
				unsigned long	recCount;
			} dbGetRecCount;
		} rsp;
		databaseStatus rspStatus;
	} dat;
};

struct DB_CONNECT_MSG {
	char			userId[64];
	char			passHash[SHA_DIGEST_LENGTH];
};

#pragma pack ( pop )

union DBS_CONN_ID {
	struct {
		uint16_t	connStruct	: 16;		/* index into connection structure array for this connection */
		uint16_t	connEnum	: 16;		/* enumerator... detects stale connections */
	} p;
	uint32_t	conId;
};

enum DBS_IO_ACTION {
	DBS_IO_ACCEPT	= 1,
	DBS_IO_READ_CONNECT,
	DBS_IO_READ,
	DBS_IO_WRITE,
	DBS_IO_DISCONNECT,
	DBS_IO_SHUTDOWN
};

struct DB_SERVER_DB 
{
	std::string				 name;
	DATABASE				*db;						/* normal database									*/
	DATABASE				*dbCopy;					/* new hot-backup database structure				*/		
	int						 copyMode;					/* number of opens in mirror mode					*/
	uint64_t				 copyMax;					/* max record to copy								*/
	uint64_t				 copyCurrent;				/* current copy point								*/

	int64_t					 workAreaNum;				/* workarea allocated for this table				*/
	char					 workAreaName[32];			/* name of workarea									*/
	bool					 pendingClose;				/* close the database when no longer in use			*/
	bool					 denyPendingOpen;
	size_t					 refCount;					/* number of open referenes to this db				*/

	struct DB_SERVER		*server;					/* db server this db belongs to						*/
};

struct DB_BUFFER {
				OVERLAPPED				 ov;

				DBS_IO_ACTION			 ioState;

				BUFFER					 buff;

				DWORD					 transferSize;			/* for writes		*/

				bool					 inUse = false;					/* in use flag		*/
	struct		DB_SERVER_FD			*fd = 0;					/* used with accept as we get the listen FD back, NOT the accepted FD */

	struct		DB_BUFFER				*next;
	struct		DB_BUFFER				*prev;
};

struct DB_SERVER_FD
{
	struct DB_SERVER_FD *prev = nullptr;						/* doubly linked list to other sockets we're dealing with	*/
	struct DB_SERVER_FD *next = nullptr;

	bool				 inUse = false;						/* allocated?	for debugging`								*/

	DWORD				 wsaFlags;

	SOCKET				 socket = 0;				/* socket we're dealing with */
	char				 clientIp[16] = { 0 };
	uint32_t			 clientPort = 0;

	bool				 freeOnComplete;			/* free the descriptor when the last buffer is freed		*/

	CRITICAL_SECTION	 bufferAccess;
	DB_BUFFER			*buffers = nullptr;
	size_t				 buffCount = 0;					/* buffer count */

	struct DB_SERVER	*dbServer = nullptr;

	DB_SERVER_DB		*dbs = 0;						/* the database this descripter is connected to				*/
	DATABASE_CONNECTION *dbConn = 0;					/* database connection structure							*/
	std::pair<DATABASE *, DATABASE_CONNECTION *>	localDb;

	DB_SERVER_FD ( )
	{
		InitializeCriticalSectionAndSpinCount ( &bufferAccess, 4000 );
	}
	
	~DB_SERVER_FD ( )
	{
		DeleteCriticalSection ( &bufferAccess );
	}
};

struct DB_SERVER {
	struct DB_SERVER		*next = nullptr;						/* so we can have multiple servers					*/

	SOCKET					 listenSocket = 0;				/* listen socket for new connections				*/

	HANDLE					 serverTerminateEvent = 0;		/* terminate the server?							*/
	DB_CONFIG				*dbConfig = nullptr;

	SRRWLOCK						 dbsAccess;
	std::unordered_map<stringi, DB_SERVER_DB *>	dbs;						/* tree of opened tables							*/

	CRITICAL_SECTION			 dbFDAccessCtrl;			/* IOCP FD list Access Critical Section				*/
	DB_SERVER_FD				*dbFDFree = nullptr;					/* available database FD pool						*/
	DB_SERVER_FD				*dbFDActiveList = nullptr;					/* list of active fd connections					*/
	bool						 shutdownSent = false;
	std::vector<DB_SERVER_FD>	 dbFD;

	CRITICAL_SECTION			 dbbufferAccessCtrl;
	DB_BUFFER					*dbBufferFree = nullptr;

	DATABASE_CONNECTION			*dbConnFreeList = nullptr;
	CRITICAL_SECTION			 dbConnAccessCtrl;

	TABLE_CONNECTION			*tblConnFreeList = nullptr;
	CRITICAL_SECTION			 dbTBLConnAccessCtrl;

	DB_SERVER ( char const *configFile )
	{
		dbConfig = dbConfigRead ( configFile );

		InitializeCriticalSectionAndSpinCount ( &dbFDAccessCtrl, 4000 );
		InitializeCriticalSectionAndSpinCount ( &dbbufferAccessCtrl, 4000 );
	
		InitializeCriticalSectionAndSpinCount ( &dbConnAccessCtrl, 4000 );
		InitializeCriticalSectionAndSpinCount ( &dbTBLConnAccessCtrl, 4000 );

		// allocate the connection descriptors
		dbFD.resize ( dbConfig->maxConnections );

		// build a free list of connection descriptors
		for ( auto &it : dbFD )
		{
			it.next = dbFDFree;
			dbFDFree = &it;
		}
	}
	~DB_SERVER ( )
	{
		DeleteCriticalSection ( &dbFDAccessCtrl );
		DeleteCriticalSection ( &dbbufferAccessCtrl );
		DeleteCriticalSection ( &dbConnAccessCtrl );
		DeleteCriticalSection ( &dbTBLConnAccessCtrl );

		while ( tblConnFreeList )
		{
			auto next = tblConnFreeList->next;
			delete tblConnFreeList;
			tblConnFreeList = next;
		}
		while ( dbConnFreeList )
		{
			auto next = dbConnFreeList->next;
			delete dbConnFreeList;
			dbConnFreeList = next;
		}
		while ( dbBufferFree )
		{
			auto next = dbBufferFree->next;
			delete dbBufferFree;
			dbBufferFree = next;
		}
		delete dbConfig;
	}

	// initialize connection descriptors
	DB_SERVER_FD		*dbAllocateFD ( HANDLE iocp );
	bool				 dbFreeFD ( vmInstance *instance, DB_SERVER_FD *dbFd, DB_BUFFER *buffer );
	TABLE_CONNECTION	*dbAllocateTableConnection ( void );
	void				 dbFreeTableConnection ( TABLE_CONNECTION *tblConn );
	DATABASE_CONNECTION	*dbAllocateConnection ( void );
	void				 dbFreeConnection ( DATABASE_CONNECTION *dbConn );
	DB_BUFFER			*dbAllocateBuffer ( DB_SERVER_FD *fd );
	void				 dbFreeBuffer ( DB_SERVER_FD *fd, DB_BUFFER *buffer );
	void				 dbFDReset ( DB_SERVER_FD *fd );
	void				 dbServerNewAccept ( vmInstance *instance );


	DB_SERVER_DB		*dbOpenTable ( vmInstance *instance, char const *tableName );
	void				 dbCloseDB ( char const *tableName, bool denyPendingOpen );
	DB_SERVER_DB		*dbGetTable ( char const *tableName );
	void				 dbReleaseTable ( DB_SERVER_DB *dbs );

};

typedef unsigned (*DB_SEND)( void *params );

struct DB_SEND_PARAMS {
	DB_SERVER				*dbServer;
	DB_SERVER_FD			*fd;
	DB_BUFFER				*buffer;
	unsigned int			 sent;
};

extern	int64_t					 dbProcessMessage		( vmInstance *instance, DB_SERVER_DB *dbs, DATABASE_CONNECTION *dbConn, BUFFER *rxBuffer, BUFFER *txBuffer, DB_SEND dbSend, void *dbSendParam );
extern	int64_t					 dbProcessConn			( vmInstance *instance, DB_SERVER *dbServer, DB_SERVER_FD *fd, BUFFER *rxBuff, BUFFER *txBuff );

extern	void					 dbServerInit			( taskControl *ctrl, char const *configFile, vmInstance *instance );
extern	taskControl				*dbServerStart			( size_t numWorkerThreads, vmInstance *instance );

extern int						 dbServerLog			( DB_SERVER_DB *dbs, char const *fmt, ... );

extern HANDLE					 dbIocpCompletionPort;

