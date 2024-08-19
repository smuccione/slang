#include "odbf.h"

uint64_t _tbRecCount ( TABLE *dbf, TABLE_CONNECTION *tblConn )
{
	SRRWLocker l1 ( dbf->headerLock, false );

	return dbf->getHeader()->nAllocatedRecords;
}
