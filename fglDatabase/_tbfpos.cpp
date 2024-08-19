#include "odbf.h"

size_t _tbFieldPos( TABLE *dbf, TABLE_CONNECTION *tblConn, char const *name )
{
	auto it = dbf->fieldMap.find ( stringi ( name ) );
	if ( it != dbf->fieldMap.end ( ) )
	{
		return std::get<0> ( it->second ) + 1;
	}
	return 0;
}
