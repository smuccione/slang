#include "odbf.h"

void tbAppendAddRecs ( TABLE *dbf, TABLE_CONNECTION *tblConn, int numRecs )

{
	DBFHEADER				*dbfHeader;
	DBF_REC_HEADER			*recHeader;
	uint64_t				 recNo;

	dbfHeader = dbf->getHeader ( );

	// increase total number of records
	dbfHeader->lastRecord += numRecs;

	dbf->mapFile ( );

	dbfHeader = dbf->getHeader ( );

	recNo = dbfHeader->lastRecord;
	while ( numRecs-- )
	{
		// read in the record, map it as dirty and get a pointer to the record header
		recHeader = dbf->getRecord ( recNo );

		// initialize the new record's header
		recHeader->nextFree = dbfHeader->freeList;
		recHeader->deleted = 1;						// mark it as free

		// attatch it to the head of the free list;
		dbfHeader->freeList = recNo;

		recNo--;
	}
}

void _tbAppend( TABLE *dbf, TABLE_CONNECTION *tblConn )
{
	uint64_t				 recNum;
	size_t					 loop;
	char					*recPtr;
	DBFHEADER				*dbfHeader;
	DBF_REC_HEADER			*recHeader;

	/* lock the header */
	SRRWLocker l1 ( dbf->headerLock, true );

	// read and map the dbf header 
	dbfHeader = dbf->getHeader ( );

	// fill free list if it's empty;
	if ( !dbfHeader->freeList )
	{
		tbAppendAddRecs ( dbf, tblConn, DBF_DEFAULT_APPEND_COUNT );
	}

	// get it again incase it remapped
	dbfHeader = dbf->getHeader ( );

	// get new rec from top of free list
	recNum = dbfHeader->freeList;

	// get record
	recHeader = dbf->getRecord ( recNum );

	// cleave it from the top
	dbfHeader->freeList = recHeader->nextFree;
	dbfHeader->nAllocatedRecords++;

	if ( !recHeader->deleted )
	{
		// shit!!!!  why is a non-deleted record in the deleted record list???
			
		// bad free list.... break the list and get a whole new record at the end of the file
		dbfHeader->freeList = 0;

		tbAppendAddRecs ( dbf, tblConn, DBF_DEFAULT_APPEND_COUNT );

		// get it again incase it remapped
		dbfHeader = dbf->getHeader ( );

		// get new rec from top of free list
		recNum = dbfHeader->freeList;

		recHeader = dbf->getRecord ( recNum );

		// cleave it from the top
		dbfHeader->freeList = recHeader->nextFree;
	}

	// mark it as no longer deleted;
	recHeader->deleted = 0;

	recPtr = recHeader->getData ( );

	/* init deleted flag to not-deleted */
	memset ( recPtr, ' ', dbf->getHeader()->recDataSize );

	for ( loop = 0; loop < dbf->fcount; loop++ )
	{
		switch ( dbf->fields[loop].type )
		{
			case 1:
			case 2:
			case 4:
			case 'S':
				*(recPtr + dbf->fields[loop].offset) = 0;
				break;
		}
	}
		
	/* position on the new record */
	_tbGoto (dbf, tblConn, recNum );
}

void _tbSetSizeRaw ( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t nRecs )
{	
	dbf->setFileSize ( nRecs );
}

void _tbAppendRaw ( TABLE *dbf, TABLE_CONNECTION *tblConn )
{
	size_t					 loop;
	char					*recPtr;

	/* lock the header */
	SRRWLocker l1 ( dbf->headerLock, true );

	tblConn->phantom	= false;
	tblConn->bof		= false;
	tblConn->eof		= false;

	/* skip to next record */
	tblConn->recNo++;

	// map the header
	auto recHeader = dbf->getRecord ( tblConn->recNo );

	// mark it as no longer deleted;
	recHeader->deleted = 0;

	recPtr = recHeader->getData ( );
	for ( loop = 0; loop < dbf->fcount; loop++ )
	{
		switch ( dbf->fields[loop].type )
		{
			case 1:
			case 2:
			case 4:
			case 'S':
				*(recPtr + dbf->fields[loop].offset) = 0;
				break;
		}
	}

}
