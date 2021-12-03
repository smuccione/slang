#include "odbf.h"

size_t _tbFieldDec( TABLE *dbf, TABLE_CONNECTION *tblConn, size_t position )
{
	if( position > 0 && position <= dbf->fcount )
	{
		--position;
		return( (int) dbf->fields[position].leninfo.num.dec );
	}
	
	return( 0 );
}


