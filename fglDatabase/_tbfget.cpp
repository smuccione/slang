#include "odbf.h"

size_t _tbFieldGet( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count, char *buffer )
{	
	char			buff[32]{};
	size_t			len;
	uint64_t		size;
	char			*recPtr;
	uint64_t		location;

	if( !tblConn->phantom && (count > 0) && (count <= dbf->fcount) && tblConn->recNo )
	{
		SRRWLocker l1( dbf->headerLock, false );
	
		auto recHeader = dbf->getRecord ( tblConn->recNo );

		if ( recHeader->deleted )
		{
			memset( buffer, ' ', count);
			return count;
		}
		recPtr = recHeader->getData ( );
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
					strncpy_s ( buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[count].offset, _TRUNCATE );
					location = atol( _firstchar ( buff ) );
				}

				strncpy_s (buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[count].offset + TB_BLOBSIZE / 2, _TRUNCATE );
				len = atol( _firstchar ( buff ) );

				size = blobReadBuff ( dbf->blob, tblConn, static_cast<int64_t>(location), buffer, (int64_t)len, static_cast<int64_t>(tblConn->recNo) );
				if ( size == -1 )
				{
					throw errorNum::scDATABASE_INTEGRITY;
				}
				if ( size != len )
				{
					throw errorNum::scDATABASE_INTEGRITY;
				}
				return len;
			case 'C' :
				len = dbf->fields[count].leninfo.fieldlen;
				break;
			case 'N' :
				len = (size_t)(uint8_t)dbf->fields[count].leninfo.num.len;
				break;
			case 'D' :
			case 'L' :
				len = dbf->fields[count].leninfo.fieldlen;
				break;
			case 'S':
				if( ! tblConn->phantom )
				{
					len = strlen ( (recPtr + dbf->fields[count].offset) ) + 1;
					
					memcpy ( buffer, (recPtr + dbf->fields[count].offset), len );
				} else
				{
					len = 0;
					*((char *) buffer + len) = 0;
				}
				return len;
			default:
				if( ! tblConn->phantom )
				{
					memcpy( buffer, (recPtr + dbf->fields[count].offset), dbf->fields[count].leninfo.fieldlen );
				} else
				{
					memset( buffer, ' ', dbf->fields[count].leninfo.fieldlen);
				}
				return dbf->fields[count].leninfo.fieldlen;
		}
		
		memcpy ( buffer, (recPtr + dbf->fields[count].offset), len );
		return len;
	} else
	{
		--count;
		switch(dbf->fields[count].type)
		{
			case 'B' :
				return( 0 );
			case 'C' :
				len = dbf->fields[count].leninfo.fieldlen;
				break;
			case 'N' :
				len = (size_t)(uint8_t) dbf->fields[count].leninfo.num.len;
				break;
			case 'D' :
			case 'L' :
				len = dbf->fields[count].leninfo.fieldlen;
				break;
			case 'S':
				len = 0;
				return ( len );
			default:
				len = dbf->fields[count].leninfo.fieldlen;
		}
		memset( buffer, ' ', len);
		return len;
	}   
	return 0;
}
