#include "odbf.h"

bool _tbIsDeleted( TABLE *dbf, TABLE_CONNECTION *tblConn )
{
	DBF_REC_HEADER			 *recHeader;

	if( ! tblConn->phantom && tblConn->recNo )
	{
		SRRWLocker l1 ( dbf->headerLock, false );

		recHeader = dbf->getRecord ( tblConn->recNo );
		return recHeader->deleted ? true : false;
	}
		
	return false;
}
