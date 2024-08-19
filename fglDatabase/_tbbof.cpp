#include "odbf.h"

bool _tbBof( TABLE *dbf, TABLE_CONNECTION *tblConn )
{
	return tblConn->bof ? true : false;
}
