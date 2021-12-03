#include "odbf.h"

void _tbRecordPut( TABLE *dbf, TABLE_CONNECTION *tblConn, char const *data, size_t dataLen )
{
	unsigned int			 fieldLen;
	DBF_REC_HEADER			*recHeader;
	char					*recPtr;
	int						 count;
	unsigned int			 offset;

	SRRWLocker l1 ( dbf->headerLock, false );

	if ( dataLen != dbf->getHeader()->recDataSize )
	{
		throw errorNum::scDATABASE_INTEGRITY;
	}

	recHeader = dbf->getRecord ( tblConn->recNo );

	if ( !recHeader->deleted )
	{
		recPtr = recHeader->getData ( );
		for ( count = 0; count < dbf->fcount; count++ )
		{
			switch(dbf->fields[count].type)
			{
				case 'R':
					*(recPtr + dbf->fields[count].offset) = *(data + dbf->fields[count].offset);
					break;
				case 'N':
					fieldLen = (size_t)(uint8_t)dbf->fields[count].leninfo.num.len;
					offset = dbf->fields[count].offset;
					memcpy ( recPtr + offset, data + offset, fieldLen );
					break;
				case 'B' :
					break;
				default:
					fieldLen = dbf->fields[count].leninfo.fieldlen;
					offset = dbf->fields[count].offset;
					memcpy ( recPtr + offset, data + offset, fieldLen );
					break;
			}
		}
	}
}
