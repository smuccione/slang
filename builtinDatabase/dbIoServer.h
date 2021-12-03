/*
	WEB Server header file

*/

#pragma once

#include <openssl/sha.h>

#define FIELD_SMALL_SIZE	 11
#define FIELD_LARGE_SIZE	241

#define TB_NAMESIZE 	(FIELD_LARGE_SIZE - 1)
#define TB_DATESIZE 	8
#define TB_LOGSIZE		1
#define TB_MEMOSIZE 	10
#define TB_BLOBSIZE 	20
#define TB_VIRTUALSIZE	0
#define TB_CRC32SIZE	4

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
};

enum DBS_DELETE_TYPE {
	DB_DELETE_ALL		= 1,
	DB_DELETE_ALL_INDEX	= 2,
	DB_DELETE_INDEX		= 3
};

#pragma pack ( push, 4 )

struct DBS_MSG {
	DBS_MSG_TYPE	msgType;

	int				status;			/* status... 0 = success... else error num */

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
			} dbCopy;
			struct {
				char			fPath[MAX_PATH];		/* destination directory to copy to */
			} dbBackup;
			struct {
				DBS_DELETE_TYPE deleteType;
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
			struct {
				int				recNo;
				int				isEof;
				int				isBof;
				int				isDeleted;
				int				isFound;
				int				isDirty;
			} dbStatus;
			struct {
				int				oldOrder;
			} dbOrder;
			struct {
				int				fieldCount;
				int				recSize;
				__int64			tableSize;
				__int64			numRecords;
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
		} rsp;
	} dat;
};

struct DB_CONNECT_MSG {
	char			userId[64];
	char			passHash[SHA_DIGEST_LENGTH];
};

// structure used for BUILDING new databases... this is the basic field description, NOT what is ultimately stored in the database
struct TBFIELD							
{
   char				name[FIELD_LARGE_SIZE];
   unsigned char	type;
   unsigned int		length;
   unsigned int		decimals;
};
#pragma pack ( pop, 4 )

#pragma pack ( push, 1 )
struct DBFFIELDS {
   char					name[241];		// asciiZ terminated
   int					nameLen;
   int					type;
   unsigned	long		offset;
   union
   {
	  unsigned short	fieldlen;
	  struct
	  {
		 char			len;
		 char			dec;
	  } num;
   } leninfo;
   unsigned long		getTriggerLength;
   unsigned long		putTriggerLength;
};
#pragma pack ( pop )
