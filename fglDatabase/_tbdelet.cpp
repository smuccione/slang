#include "odbf.h"

void _tbDelete( TABLE *dbf, TABLE_CONNECTION *tblConn )
{
	DBFHEADER				*dbfHeader;
	DBF_REC_HEADER			*recHeader;
	char					*recPtr;
	int						 loop;
	char					 buff[32]{};
	uint64_t				 location;

	if( !tblConn->phantom && tblConn->recNo )
	{
		/* lock the header */
		SRRWLocker l1 ( dbf->headerLock, true );

		dbfHeader = dbf->getHeader ( );
		recHeader = dbf->getRecord ( tblConn->recNo );
		recPtr = recHeader->getData ( );

		if ( !recHeader->deleted )
		{
			// not already deleted so OK to do so now

			// delete any blob's
			if ( dbf->getHeader()->capabilities & DBF_CAPABILITY_BLOB )		// is there a blob?
			{
				for ( loop = 0; loop < dbf->fcount; loop++ )
				{
					switch(dbf->fields[loop].type)
					{
						case 'B' :
							buff[TB_BLOBSIZE / 2] = 0;
							if ( *(recPtr + dbf->fields[loop].offset) == 'x' )
							{
								location = *((uint64_t *)(recPtr + dbf->fields[loop].offset + 1));
							} else
							{
								strncpy_s (buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[loop].offset, _TRUNCATE );
								location = _atoi64 ( _firstchar ( buff ) );
							}

							if ( !blobDelete ( dbf->blob, tblConn, static_cast<int64_t>(location), static_cast<int64_t>(tblConn->recNo) ) )
							{
								throw errorNum::scDATABASE_INTEGRITY;
							}
							break;
					}
				}
			}

			recHeader->deleted  = 1;
			recHeader->nextFree = dbfHeader->freeList;
			dbfHeader->freeList = tblConn->recNo;

			dbfHeader->nAllocatedRecords--;
		}
	}
}
