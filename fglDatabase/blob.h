/*
	new blob header file

*/

#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>
#include <stdint.h>
#include "Utility/buffer.h"
#include "Utility/util.h"

#define BLOB_ID						"FGL_BLOB"

#define BLOB_VERSION				3				/* minimum compatible blob version number		*/ 
#define BLOB_DEFAULT_BLOCK_SIZE		4096			/* default block size							*/
#define BLOB_ALLOCATION_QUANTA		512				/* number of free blocks to allocate at a time	*/

#pragma pack ( push, BLOB, 4 )

struct BLOB_HEADER {
	char			ID[sizeof ( BLOB_ID )];
	char			version;

	uint64_t	    freeStart;
	uint32_t		blockSize;				/* block size of blob file */
//	uint32_t		updateCount;			// 0 while open... non-zero when closed indicating, incremented every close
};

struct BLOB_FIELD {  
	uint32_t		used		: 1;
	uint32_t		free		: 1;

	union {
		struct {
			uint32_t		start		: 1;		/* the first block?			*/
			uint32_t		stop		: 1;		/* the last block?			*/
			uint32_t		compressed	: 1;
			uint32_t		reserved	: 4;		/* reserved for future expansion		*/
			uint32_t		sequenceNo	: 25;		/* the sequence number of this block	*/

			uint32_t		relation;				/* the dbf record this blob relates to	*/
			uint32_t		size;					/* size of data in this block */

			FILETIME		timeStamp;				/* update date stamp, valid for start block only	*/

			uint64_t		next;
		} used;
		struct {
			uint64_t		next;
		} free;
	} dat;
	uint32_t				blockCheck;				/* makes the field header 0 */

	char *getData ( )
	{
		return (char *) (&this[1]);
	}
};

#pragma pack ( pop, BLOB )

struct BLOB_FILE
{
	// header lock
	SRRWLOCK			 lock;					/* lock												*/
	HANDLE				 fileHandle = 0;		/* file handle										*/
	HANDLE				 fileMap = 0;			/* memory mapped handle								*/
	uint8_t				*data = 0;				/* pointer to mapped view of memory					*/

	size_t				 fileSize = 0;

	char 				 name[MAX_PATH];		/* sBLOB file name */

	size_t				 blockSize = 0;			/* size of each block				 */
	size_t				 blockDataSize = 0;		/* amount of data in each blob block */

	BLOB_FILE ( )
	{
	}
	virtual ~BLOB_FILE ( );

	BLOB_HEADER *getHeader ( )
	{
		return (BLOB_HEADER *) data;
	}
	BLOB_FIELD *getField ( uint64_t addr )
	{
		return (BLOB_FIELD *) (data + addr);
	}
	void setUpdateCount ( int32_t updateCount )
	{
//		getHeader ()->updateCount = updateCount;
	}
	uint32_t getUpdateCount ()
	{
		return 0;
//		return getHeader ()->updateCount;
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
		unMapFile ( );

		fileMap = CreateFileMapping ( fileHandle, NULL, PAGE_READWRITE, (uint32_t) (fileSize >> 32), (uint32_t) (fileSize & 0xFFFFFFFF), NULL );
		data = (uint8_t *) MapViewOfFile ( fileMap, FILE_MAP_ALL_ACCESS, 0, 0, fileSize );
	}
};

extern BLOB_FILE		*blobInit			( char const *fName, uint32_t updateCount );
extern BLOB_FILE		*blobInitRead		( char const *fName, uint32_t updateCount );		// opens the blob regardless of error
extern BLOB_FILE		*blobCreate			( char const *fName, uint32_t updateCount );
extern bool				 blobCheck			( BLOB_FILE *blob );
extern void				 blobCopy			( BLOB_FILE *blob, char const *destDir );
extern bool				 blobDelete			( BLOB_FILE *blob, struct TABLE_CONNECTION *tblConn, int64_t addr, int64_t relation  );
extern int64_t			 blobWrite ( BLOB_FILE *blob, struct TABLE_CONNECTION *tblConn, int64_t addr, char const *data, int64_t size, int64_t relation );
extern int64_t			 blobReadBuff ( BLOB_FILE *blob, struct TABLE_CONNECTION *tblConn, int64_t addr, char *cPtr, int64_t buffLen, int64_t relation );
extern int64_t			 blobRecover ( BLOB_FILE *blob, struct TABLE_CONNECTION *tblConn, int64_t *pAddr, BUFFER *buffer, int64_t relation );
extern int64_t			 blobReadCheck ( BLOB_FILE *blob, struct TABLE_CONNECTION *tblConn, int64_t addr, int64_t relation );
