#include "odbf.h"

size_t _tbFieldLen( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t count )
{
	if( count > 0 && count <= dbf->fcount)
	{
		--count;
		if(dbf->fields[count].type == 'N')
		{
			return( dbf->fields[count].leninfo.num.len );
		} else
		{
			return( dbf->fields[count].leninfo.fieldlen );
		}
	}
	
	return( 0 );
}
