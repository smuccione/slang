#include "odbf.h"

uint64_t _tbSkip(TABLE *dbf, TABLE_CONNECTION *tblConn, uint64_t skipCount)
{
	return _tbGoto (dbf, tblConn, tblConn->recNo + skipCount );
}
