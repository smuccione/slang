#include "odbf.h"

size_t _tbFieldSize( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count )
{	
	size_t	 r = 0;

	if( count > 0 && count <= dbf->fcount)
	{
		--count;
		switch ( dbf->fields[count].type )
		{
			case 'B':
				{
					auto recPtr = dbf->getRecord ( tblConn->recNo )->getData ();
					char	 buff[TB_BLOBSIZE / 2 + 1];

					buff[TB_BLOBSIZE / 2] = 0;
					// this is LEN... it is NOT the address which can be a uint64_t
					strncpy_s ( buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[count].offset + TB_BLOBSIZE / 2, TB_BLOBSIZE / 2 );

					r = atol ( _firstchar ( buff ) );
				}
				break;
			case 'N':
				r = (long) dbf->fields[count].leninfo.num.len;
				break;
			default:
				r = dbf->fields[count].leninfo.fieldlen;
				break;
		}
	}
	
	return r;
}
 
