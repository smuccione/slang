#include "odbf.h"

uint64_t _tbRecno( TABLE  *dbf, TABLE_CONNECTION *tblConn )
{
	return tblConn->recNo;
}
