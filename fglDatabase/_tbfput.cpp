#include "odbf.h"

void _tbFieldPut( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count, char const *buffer, size_t size )
{	
	char			 buff[32] = "";
	size_t			 len;
	uint64_t		 location;
	DBF_REC_HEADER	*recHeader;

	if( !tblConn->phantom && (count > 0) && (count <= dbf->fcount) && tblConn->recNo )
	{
		SRRWLocker l1 ( dbf->headerLock, false );
		recHeader = dbf->getRecord ( tblConn->recNo );

		if ( !recHeader->deleted )
		{
			--count;

			switch(dbf->fields[count].type)
			{
				case 'C' :
				case 'D' :
					memcpyset( recHeader->getData() + dbf->fields[count].offset, buffer, dbf->fields[count].leninfo.fieldlen, size, ' ');
					break;
					
				case 'N' :
					len = strlen( (buffer = _firstchar ( buffer )));

					if( len > (unsigned int)dbf->fields[count].leninfo.num.len )
					{
						memset ( recHeader->getData() + dbf->fields[count].offset, '*', dbf->fields[count].leninfo.num.len);
						break;
					}
					
					memset( recHeader->getData() + dbf->fields[count].offset, ' ', dbf->fields[count].leninfo.num.len);
					memcpy( recHeader->getData() + dbf->fields[count].offset + dbf->fields[count].leninfo.num.len - len, buffer, len);
					break;
					
				case 'L' :
					*(recHeader->getData() + dbf->fields[count].offset) = ((*(unsigned char *) buffer) | 0x20) == 't' ? (unsigned char)'T' : (unsigned char)'F';
					break;
					
				case 'B' :
					if ( *(recHeader->getData() + dbf->fields[count].offset) == 'x' )
					{
						location = *((uint64_t *)(recHeader->getData() + dbf->fields[count].offset + 1));
					} else
					{
						buff[TB_BLOBSIZE / 2] = 0;
						strncpy_s ( buff, TB_BLOBSIZE / 2, recHeader->getData() + dbf->fields[count].offset, _TRUNCATE );
						location = atol( _firstchar ( buff ) );
					}
							
					location = blobWrite ( dbf->blob, tblConn, static_cast<int64_t>(location), buffer, (int64_t)size, static_cast<int64_t>(tblConn->recNo) );

					((char *)(recHeader->getData() + dbf->fields[count].offset))[0]='x';
					*((uint64_t *)(((char *)(recHeader->getData() + dbf->fields[count].offset)) + 1)) = location;

					/* size goes into second half of field */
					_i64toa_s ( (int64_t)size, buff, sizeof ( buff ), 10);
					strncpy_s ( recHeader->getData() + dbf->fields[count].offset + TB_BLOBSIZE / 2, TB_BLOBSIZE / 2, buff, _TRUNCATE );
					break;
					
				case 'V' :
					return;
					
				default :
					if( size > dbf->fields[count].leninfo.fieldlen )
					{
						size = dbf->fields[count].leninfo.fieldlen;
					}
					memcpy( recHeader->getData() + dbf->fields[count].offset, buffer, size );
					break;
					
			}
		}
	} else
	{
		throw errorNum::scDATABASE_INTEGRITY;
	};
}
