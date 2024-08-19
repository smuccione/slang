#include "odbf.h"

size_t _tbFreeListBuild ( TABLE *dbf, TABLE_CONNECTION *tblConn )
{	
	DBFHEADER			 *dbfHeader;
	DBF_REC_HEADER		 *recHeader;
	uint64_t			 recNo;
	size_t				 numRecs;

	numRecs = 0;

	SRRWLocker l1 ( dbf->headerLock, true );

	dbfHeader = dbf->getHeader ( );

	// null out the free list... we're going to rebuild it
	dbfHeader->freeList = 0;
	recNo = 1;
	_tbGoto ( dbf, tblConn, recNo );

	while ( !tblConn->eof )
	{
		if ( _tbIsDeleted ( dbf, tblConn ) )
		{
			// map the record and get it's ehader
			recHeader = dbf->getRecord ( tblConn->recNo );

			// point the next free to what ever is in the header (we'll become the top)
			recHeader->nextFree = dbfHeader->freeList;

			dbfHeader->freeList = tblConn->recNo;
			numRecs++;
		}
		recNo++;
	}
	return numRecs;
}
