#include "odbf.h"

uint64_t _tbGoto(TABLE *dbf, TABLE_CONNECTION *tblConn, uint64_t record)
{
	if (record < 1L)
	{
		tblConn->phantom	= true;
		tblConn->recNo		= 0;
		tblConn->bof		= true;
		tblConn->eof		= false;
	} else
	{
		SRRWLocker l1 ( dbf->headerLock, true );

		if ( record > dbf->getHeader ( )->lastRecord )
		{
			tblConn->phantom = true;
			tblConn->recNo = dbf->getHeader ( )->lastRecord + 1;
			tblConn->bof = false;
			tblConn->eof = true;
		} else
		{
			tblConn->bof = false;
			tblConn->eof = false;
			tblConn->phantom = false;
			tblConn->recNo = record;
		}
	}
	return tblConn->recNo;
}

