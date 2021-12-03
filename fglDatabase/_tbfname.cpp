#include "odbf.h"

char *_tbFieldName( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count, char *buffer )
{
	buffer[0] = 0;
	if( (count > 0) && (count <= dbf->fcount) )
	{
		strncpy_s ( buffer, TB_NAMESIZE, dbf->fields[count - 1].name, _TRUNCATE );
		buffer[TB_NAMESIZE-1] = 0;
	}

	return buffer;
}
