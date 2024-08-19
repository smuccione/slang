#include "odbf.h"

bool _tbFieldCheck( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count )
{	
	char					 buff[32] = "";
	uint64_t				 len;
	uint64_t				 size;
	uint64_t				 location;
	DBF_REC_HEADER			*rec;
	char					*recPtr;

	if( !tblConn->phantom && (count > 0) && (count <= dbf->fcount) && tblConn->recNo )
	{
		SRRWLocker l1( dbf->headerLock, false );

		rec = dbf->getRecord ( tblConn->recNo );
		recPtr = rec->getData ( );
	
		--count;
		switch(dbf->fields[count].type)
		{
			case 'B' :
				buff[TB_BLOBSIZE / 2] = 0;
				if ( *(recPtr + dbf->fields[count].offset) == 'x' )
				{
					location = *((uint64_t *)(recPtr + dbf->fields[count].offset + 1));
				} else
				{
					strncpy_s (buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[count].offset, _TRUNCATE );
					location = atol( _firstchar ( buff ) );
				}

				strncpy_s (buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[count].offset + TB_BLOBSIZE / 2, _TRUNCATE );
				len = atol( _firstchar ( buff ) );

				size = blobReadCheck ( dbf->blob, tblConn, static_cast<int64_t>(location), static_cast<int64_t>(tblConn->recNo) );

				if ( size == -1 )
				{
					return false;
				}
				if ( size != len )
				{
					return false;
				}
				return true;
			case 'C' :
			case 'N' :
			case 'D' :
			case 'L' :
			case 'S':
			default:
				break;
		}
		return true;
   }   
	return false;
}
