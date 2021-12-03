#include "odbf.h"

size_t _tbFreeListCheck ( TABLE *dbf, TABLE_CONNECTION *tblConn )
{	
	DBFHEADER			*dbfHeader;
	DBF_REC_HEADER		*recHeader;
	uint64_t			 recNo;
	size_t				 numRecs;

	SRRWLocker l1 ( dbf->headerLock, false );

	dbfHeader = dbf->getHeader ( );

	numRecs = 0;
	recNo = dbfHeader->freeList;
    while ( recNo )
	{
		recHeader = dbf->getRecord ( recNo );
		if ( !recHeader->deleted )
		{
			throw errorNum::scDATABASE_INTEGRITY;
		}
		recNo = recHeader->nextFree;
		numRecs++;
	}

	return numRecs;
}
