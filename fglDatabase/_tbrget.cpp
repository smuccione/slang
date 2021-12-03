#include "odbf.h"

size_t _tbRecordGet ( TABLE  *dbf, TABLE_CONNECTION *tblConn, BUFFER *buff )
{
	SRRWLocker l1 ( dbf->headerLock, false );

	if( !tblConn->phantom && tblConn->recNo )
	{
		auto recHeader = dbf->getRecord ( tblConn->recNo );
		auto recPtr = recHeader->getData ( );

		if ( recHeader->deleted )
		{
			buff->makeRoom ( dbf->getHeader ( )->recDataSize );
			memset( buff->data<char *>(), ' ', dbf->getHeader()->recDataSize );
			buff->assume ( dbf->getHeader()->recDataSize );
		} else
		{
			buff->write ( recPtr, dbf->getHeader()->recDataSize );
		}
	} else
	{
		buff->makeRoom ( dbf->getHeader ( )->recDataSize );
		memset( buff->data<char *>(), ' ', dbf->getHeader()->recDataSize );
		buff->assume ( dbf->getHeader()->recDataSize );
	}
	
	return dbf->getHeader ( )->recDataSize;
}
