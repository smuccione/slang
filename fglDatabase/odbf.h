#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>
#include <stdint.h>
#include <map>
#include <utility>
#include "Utility/buffer.h"
#include "Utility/stringi.h"
#include "Utility/util.h"
#include "blob.h"


#ifndef DEFAULT
#define DEFAULT -2
#endif

#define DBFTYPE 5

#define FIELD_SMALL_SIZE	 11
#define FIELD_LARGE_SIZE	241

#define TB_NAMESIZE 	(FIELD_LARGE_SIZE - 1)
#define TB_DATESIZE 	8
#define TB_LOGSIZE		1
#define TB_MEMOSIZE 	10
#define TB_BLOBSIZE 	20
#define TB_VIRTUALSIZE	0
#define TB_CRC32SIZE	4

#define TRIG_MOVEON 	1
#define TRIG_MOVEOFF	2
#define TRIG_RECORDPUT	4
#define TRIG_RECORDGET	8
#define TRIG_FIELDPUT	16
#define TRIG_FIELDGET	32

#define	DBF_ID_STRING				"Fifth Gen Database File\r\n"
#define DBF_VERSION					0x00010000

#define DBF_DEFAULT_APPEND_COUNT	256

constexpr char const *__tbfExtension = "DB";
constexpr char const *__BLOBExtension = "BLB";

enum dbfCapabilities {
	DBF_CAPABILITY_NONE = 0,
	DBF_CAPABILITY_BLOB	= 1,
};

#pragma pack ( push, DBF, 1 )
struct DBFHEADER {
	char			id[sizeof ( DBF_ID_STRING) ];
	unsigned long	version;
	dbfCapabilities	capabilities;

	unsigned long	updateCount;			// 0 while open... non-zero when closed indicating, incremented every close
	unsigned long	fieldCount;
	unsigned long	recDataSize;
	uint64_t		freeList;				// record number NOT offset
	uint64_t		lastRecord;				// record number NOT offset

	uint64_t		dataStartOffset;		// offset to start of record data
	uint64_t		nAllocatedRecords;

	unsigned long	pad[126];
};

struct DBF_REC_HEADER {
	unsigned long	deleted	: 1;
	union {
		uint64_t		nextFree;				// record number of next free block
		time_t			updateTime;				// time last record was updated at for non-deleted blocks
	};
	char *getData ( )
	{
		return (char *)&this[1];
	}
};

struct DBFFIELDS {
   char					name[FIELD_LARGE_SIZE];		// asciiZ terminated
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
#pragma pack ( pop, DBF )


#pragma pack ( push, FIELDS, 4 )
// strucutre used for BUILDING new databases... this is the basic field description, NOT what is ultimately stored in the database
struct TBFIELD							
{
   char				name[FIELD_LARGE_SIZE];
   unsigned char	type;
   unsigned int		length;
   unsigned int		decimals;
};
#pragma pack ( pop, FIELDS)

enum BLOB_BLOCK_REOAIR_STATUS {
	BLOB_BLOCK_STATUS_UNKNOWN	= 0,
	BLOB_BLOCK_STATUS_IN_USE	= 1,
	BLOB_BLOCK_STATUS_FREE		= 2,
};

struct TABLE_REPAIR_DATA {
	char *blobBlockStatus = nullptr;
};

struct TABLE_CONNECTION {
	struct	TABLE_CONNECTION		*prev = nullptr;
	struct	TABLE_CONNECTION		*next = nullptr;
			int						 isHot = false;
			bool					 isFound = false;
			bool					 lastField = 0;					/* last field accessed							*/
			uint64_t				 recNo = 0;						/* current record number						*/
			bool					 bof = true;					/* at bof indicator								*/
			bool					 eof = false;					/* at eof indicator								*/
			bool					 phantom = true;				/* phantom record indicator						*/

			BUFFER					 compressBuff;					/* buffer for decompressing data				*/

			TABLE_REPAIR_DATA		*tblRepairData = nullptr;		/* table repair data							*/

			int						 inUse = false;

	TABLE_CONNECTION () : compressBuff  ( 32*1024*1024 )
	{}
};

struct TABLE
{
	// header lock
	SRRWLOCK			 headerLock;			/* header lock										*/
	HANDLE				 fileHandle = 0;		/* file handle										*/		
	HANDLE				 fileMap = 0;			/* memory mapped handle								*/		
	uint8_t				*data = 0;				/* pointer to mapped view of memory					*/		

	char				 alias[MAX_PATH];		/* Fully qualified path name						*/

	DBFHEADER			 header;				// for table creation ONLY!!!						*/

	// new dbf format
	DBFFIELDS			*fields = 0;			/* Pointer to new database engine field descriptor	*/
	std::map<stringi, std::pair<size_t,DBFFIELDS *>>	fieldMap;

	int 				 fcount = 0;			/* Number of fields in a record						*/
	BLOB_FILE			*blob = 0;				/* Pointer to the sBLOB structure					*/
	uint32_t			 updateCount = 0;		/* update count to be written back to the header when closed */

	TABLE ( )
	{
	}
	~TABLE ( );

	DBFHEADER			*getHeader ( )
	{
		return (DBFHEADER *) data;
	}
	DBF_REC_HEADER		*getRecord ( uint64_t recno)
	{
		return (DBF_REC_HEADER *) (data + getHeader ( )->dataStartOffset + ((uint64_t) getHeader ( )->recDataSize + sizeof ( DBF_REC_HEADER )) * ((uint64_t) (recno) -1));
	}
	DBFFIELDS			*getField ( uint64_t num )
	{
		return &((DBFFIELDS *) (&getHeader()[1]))[num];
	}

	void setUpdateCount ( uint32_t updateCount )
	{
		this->updateCount = updateCount;
		getHeader ()->updateCount = updateCount;
	}

	uint32_t getUpdateCount ()
	{
		return updateCount;
	}

	void unMapFile ( )
	{
		if ( fileMap )
		{
			UnmapViewOfFile ( data );
			CloseHandle ( fileMap );
			fileMap = 0;
		}
	}
	void mapFile ( )
	{
		if ( !fileMap )
		{
			// at a minimum, we need the dbf header mapped in so that we can get read the header to boot-strap the mapping
			fileMap = CreateFileMapping ( fileHandle, NULL, PAGE_READWRITE, 0, sizeof ( DBFHEADER ) , NULL );
			data = (uint8_t *) MapViewOfFile ( fileMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof ( DBFHEADER ) );
		}
		// need to compute this BEFORE we map the file!!!
		uint64_t newSize = getHeader ( )->dataStartOffset + (sizeof ( DBF_REC_HEADER ) + getHeader ( )->recDataSize) * getHeader ( )->lastRecord;

		unMapFile ( );

		fileMap = CreateFileMapping ( fileHandle, NULL, PAGE_READWRITE, (uint32_t) (newSize >> 32), (uint32_t) (newSize & 0xFFFFFFFF), NULL );
		data = (uint8_t *) MapViewOfFile ( fileMap, FILE_MAP_ALL_ACCESS, 0, 0, newSize );
	}

	/* set the file size to hold the specified number of records */
	void setFileSize ( size_t nRecs )
	{
		LARGE_INTEGER	newAddr;
		LARGE_INTEGER	addr;

		// need to compute this BEFORE we map the file!!!
		uint64_t newSize = getHeader ()->dataStartOffset + (sizeof ( DBF_REC_HEADER ) + getHeader ()->recDataSize) * nRecs;

		unMapFile ();

		addr.QuadPart = newSize;
		SetFilePointerEx ( fileHandle, addr, &newAddr, FILE_BEGIN );
		SetEndOfFile ( fileHandle );

		fileMap = CreateFileMapping ( fileHandle, NULL, PAGE_READWRITE, (uint32_t) (newSize >> 32), (uint32_t) (newSize & 0xFFFFFFFF), NULL );
		data = (uint8_t *) MapViewOfFile ( fileMap, FILE_MAP_ALL_ACCESS, 0, 0, newSize );

		getHeader ()->lastRecord = nRecs;
	}
};



extern TABLE			*_tbInit			( char const *tablename, size_t NumFields, size_t RecSize);
extern TABLE			*_tbOpen			( char const *tablename, int openmode );
extern TABLE			*_tbOpenRepair		( char const *filename, int openmode );
extern TABLE			*_tbBuild			( char const *tablename, TBFIELD * field, int numfields);
extern void				 _tbBuildFromTable	( char const *newFilename, TABLE *tblOld );
extern void				 _tbCreate			( TABLE  *dbf );
extern char 			*_tbFieldName		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t position, char *buffer );
extern size_t			 _tbFieldGet		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t position, char *buffer);
extern void				 _tbFieldPut		( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count, char const *buffer, size_t size );
extern bool				 _tbFieldCheck		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t count );
extern size_t			 _tbRecordGet		( TABLE  *dbf, TABLE_CONNECTION *tblConn, struct BUFFER *buff );
extern void				 _tbRecordPut		( TABLE  *dbf, TABLE_CONNECTION *tblConn, char const *data, size_t dataLen );
extern unsigned char	 _tbFieldType		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t position );
extern uint64_t			 _tbRecno 			( TABLE  *dbf, TABLE_CONNECTION *tblConn );
extern uint64_t			 _tbLastrec			( TABLE  *dbf, TABLE_CONNECTION *tblConn );
extern size_t			 _tbFieldSize		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t position );
extern size_t			 _tbFieldOff		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t position );
extern size_t			 _tbFieldCount		( TABLE  *dbf, TABLE_CONNECTION *tblConn );
extern uint64_t			 _tbRecCount		( TABLE  *dbf, TABLE_CONNECTION *tblConn );
extern uint64_t			 _tbGoto			( TABLE  *dbf, TABLE_CONNECTION *tblConn, uint64_t recno );
extern uint64_t			 _tbSkip			( TABLE  *dbf, TABLE_CONNECTION *tblConn, uint64_t recno );
extern bool				 _tbEof				( TABLE  *dbf, TABLE_CONNECTION *tblConn);
extern bool				 _tbBof				( TABLE  *dbf, TABLE_CONNECTION *tblConn);
extern void				 _tbDelete			( TABLE  *dbf, TABLE_CONNECTION *tblConn);
extern bool				 _tbIsDeleted		( TABLE  *dbf, TABLE_CONNECTION *tblConn);
extern size_t			 _tbFieldLen		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t position);
extern size_t			 _tbFieldDec		( TABLE  *dbf, TABLE_CONNECTION *tblConn, size_t position);
extern size_t			 _tbFieldPos		( TABLE  *dbf, TABLE_CONNECTION *tblConn, char const *name);
extern void 			 _tbAppend			( TABLE  *dbf, TABLE_CONNECTION *tblConn);
extern void				 _tbCopy			( TABLE  *dbf, char const *destDir );
extern bool				 _tbCreateDuplicate	( TABLE  *dbf, char const *destDir );

extern size_t			 _tbFieldGetRecover ( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count, BUFFER *buffer );
extern size_t			 _tbFreeListCheck	( TABLE *dbf, TABLE_CONNECTION *tblConn );

// these are for REPAIR ONLY.   set size hard truncates/grows the database to the specified number of records, append inits the next record, but does NOT grow the database
extern void				 _tbAppendRaw		( TABLE *dbf, TABLE_CONNECTION *tblConn );		// no free list use
extern void				 _tbSetSizeRaw		( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t nRecs );

extern size_t			 _tbFreeListBuild	( TABLE *dbf, TABLE_CONNECTION *tblConn );
