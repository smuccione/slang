#include "odbf.h"

size_t  _tbFieldGetRecover( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count, BUFFER *buffer )
{	
	char			 buff[32] = "";
	size_t			 len;
	char			*recPtr;
	int64_t			 location;

	if( !tblConn->phantom && (count > 0) && (count <= dbf->fcount) && tblConn->recNo )
	{
		SRRWLocker l1 ( dbf->headerLock, false );
	
		buffer->reset ( );
		recPtr = dbf->getRecord ( tblConn->recNo )->getData ( );
		--count;
		switch(dbf->fields[count].type)
		{
			case 'B' :
				buff[TB_BLOBSIZE / 2] = 0;
				if ( *(recPtr + dbf->fields[count].offset) == 'x' )
				{
					location = *((int64_t *)(recPtr + dbf->fields[count].offset + 1));
				} else
				{
					strncpy_s ( buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[count].offset, _TRUNCATE );
					location = _atoi64 ( _firstchar ( buff ) );
				}

				strncpy_s ( buff, TB_BLOBSIZE / 2, recPtr + dbf->fields[count].offset + TB_BLOBSIZE / 2, _TRUNCATE );
				len = _atoi64 ( _firstchar ( buff ) );

				buffer->reset ( );
				buffer->makeRoom ( len );
				len = blobReadBuff ( dbf->blob, tblConn, location, buffer->data<char *>(), (int64_t)len, static_cast<int64_t>(tblConn->recNo) );
				buffer->assume ( len );

				if ( len == -1 )
				{
					// deep recover
					len = blobRecover ( dbf->blob, tblConn, &location, buffer, static_cast<int64_t>(tblConn->recNo) );

					char			 buff[65];

					(recPtr + dbf->fields[count].offset)[0]='x';
					*((int64_t *)(((char *)(recPtr + dbf->fields[count].offset)) + 1)) = location;

					/* size goes into second half of field */
					_i64toa_s ( (int64_t)len, buff, sizeof ( buff ), 10);
					strncpy_s ( recPtr + dbf->fields[count].offset + TB_BLOBSIZE / 2, TB_BLOBSIZE / 2, buff, _TRUNCATE );

					if ( len == -1 )
					{
						len = 0;
					}
				} else if ( len == -2 )
				{
					// crap... can't recover
					len = 0;
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
					buffer->write ( recPtr + dbf->fields[count].offset );
				} else
				{
					buffer->put ( 0 );
				}
				return buffer->size ( ) - 1;
			default:
				if( !tblConn->phantom )
				{
					buffer->write ( recPtr + dbf->fields[count].offset, dbf->fields[count].leninfo.fieldlen );
				} else
				{
					buffer->makeRoom ( dbf->fields[count].leninfo.fieldlen );
					memset ( buffer->data<char *>(), ' ', dbf->fields[count].leninfo.fieldlen);
					buffer->assume ( dbf->fields[count].leninfo.fieldlen );
				}
				return buffer->size ( );
		}
		
		if( ! tblConn->phantom )
		{
			buffer->write ( recPtr + dbf->fields[count].offset, len );
		} else
		{
			buffer->makeRoom ( len );
			memset ( buffer->data<char *>(), ' ', len );
			buffer->assume ( len );
			buffer->put ( 0 );
		}
		return len;
   }
   
	throw errorNum::scDATABASE_INTEGRITY;
}
