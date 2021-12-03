#include "odbf.h"

size_t _tbFieldCount( TABLE *dbf, TABLE_CONNECTION *tblConn )
{
	return( dbf->fcount );
}
