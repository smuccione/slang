
#include "Utility/settings.h"

#include "StdAfx.h"
#include "io.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "time.h"

#include "dbIoServer.h"
#include "Db.h"
#include "odbf.h"

#define MAX_BLOB_SIZE	(1024 * 1024)

bool dbRepair ( vmInstance *instance, char const *filename )
{
	char					 blbName[MAX_PATH];
	char					 newBackupDbName[MAX_PATH];
	char					 newBackupBlbName[MAX_PATH];
	time_t					 fTime;
	BUFFER					 newBuffer;
	TABLE					*tblOld;
	TABLE					*tblNew;

	if ( !_isfile ( filename ) )
	{
		return false;
	}

	fTime = time ( 0 );

	_fmerge ( "*.backup", filename, newBackupDbName, sizeof ( newBackupDbName ) );
	_snprintf_s ( newBackupDbName + strlen ( newBackupDbName ), sizeof ( newBackupDbName ) - strlen ( newBackupDbName ) - 1, _TRUNCATE, ".%I64i.db", fTime );

	_fmerge ( "*.backup", filename, newBackupBlbName, sizeof ( newBackupBlbName ) );
	_snprintf_s ( newBackupBlbName + strlen ( newBackupBlbName ), sizeof ( newBackupBlbName ) - strlen ( newBackupBlbName ) - 1, _TRUNCATE, ".%I64i.blb", fTime );

	_fmerge ( "*.blb", filename, blbName, sizeof ( blbName ) );

	MoveFile ( filename, newBackupDbName );
	MoveFile ( blbName, newBackupBlbName );

	printf ( "Repairing Database: %s  ", filename );

	// open OLD version
	if ( !(tblOld = _tbOpenRepair ( newBackupDbName, FILE_FLAG_WRITE_THROUGH )) )
	{
		printf ( "Unable to repair database %s\r\n", filename );
		return false;
	} else
	{
		try
		{
			_tbBuildFromTable ( filename, tblOld );
		} catch ( ... )
		{
			printf ( "Unable to create new database %s\r\n", filename );
			return false;
		}
	}

	if ( !(tblNew = _tbOpen ( filename, 0 )) )
	{
		return ( 0 );
	}

	TABLE_CONNECTION tblNewConn;
	TABLE_CONNECTION tblOldConn;

	_tbGoto ( tblOld, &tblOldConn, 1 );
	double step = 20.0 / (double)(_tbLastrec ( tblOld, &tblOldConn ) + 1);
	double cnt = 0;

	_tbSetSizeRaw ( tblNew, &tblNewConn, _tbLastrec ( tblOld, &tblOldConn ) );
	_tbGoto ( tblNew, &tblNewConn, 0 );
	while ( !_tbEof ( tblOld, &tblOldConn ) )
	{
		if ( !_tbIsDeleted ( tblOld, &tblOldConn ) )
		{
			_tbAppendRaw ( tblNew, &tblNewConn );
			bufferReset ( &newBuffer );
			_tbRecordGet ( tblOld, &tblOldConn, &newBuffer );												// does full recovery if needed
			_tbRecordPut ( tblNew, &tblNewConn, bufferBuff ( &newBuffer ), bufferSize ( &newBuffer ) );

			if ( tblOld->getHeader()->capabilities & DBF_CAPABILITY_BLOB )
			{
				for ( size_t loop = 0; loop < tblOld->fcount; loop++ )
				{
					if ( _tbFieldType ( tblNew, &tblNewConn, loop + 1 ) == 'B' )
					{
						bufferReset ( &newBuffer );
						_tbFieldGetRecover ( tblOld, &tblOldConn, loop + 1, &newBuffer );							// does full recovery if needed
						_tbFieldPut ( tblNew, &tblNewConn, loop + 1, bufferBuff ( &newBuffer ), bufferSize ( &newBuffer ) );
					}
				}
			}
		} else
		{
			_tbAppendRaw ( tblNew, &tblNewConn );
			_tbDelete ( tblNew, &tblNewConn );
		}
		_tbSkip ( tblOld, &tblOldConn, 1 );
		cnt += step;
		if ( cnt > 1 )
		{
			printf ( "*" );
			cnt = 0;
		}
	}
	printf ( "\r\n" );
	delete tblOld;
	delete tblNew;

	return true;
}
