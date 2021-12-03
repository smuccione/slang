#include "odbf.h"

unsigned char _tbFieldType( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t position )
{
	if( position > 0 && position <= dbf->fcount)
	{
		return( dbf->fields[position - 1].type );
	}
	
	return 0;
}

