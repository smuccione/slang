/*
		IO Support functions

*/

#include "Utility/settings.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"
#include "Target/vmTask.h"
#include "Target/fileCache.h"

#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>
#include <filesystem>
#include <numeric>



static char *vmCurdir( class vmInstance *instance, nParamType num )
{
	VAR  *var;
	char *res;
	int	 drive;

	res = ( char * )instance->om->alloc ( MAX_NAME_SZ );

	if ( num )
	{
		if ( num == 1 )
		{
			var = num[0];
			switch ( TYPE( var ) )
			{
				case slangType::eLONG:
					drive = (int)var->dat.l;
					break;
				case slangType::eDOUBLE:
					drive = (int)var->dat.d;
					break;
				case slangType::eSTRING:
					if ( var->dat.str.len != 1 )
					{
						throw errorNum::scINVALID_PARAMETER;
					}
					drive = var->dat.str.c[0] - 'a';
					if ( drive < 0 )
					{
						drive = drive + 'a' - 'A';
					}
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		} else
		{
			throw errorNum::scINVALID_PARAMETER;
		}
		if ( !_getdcwd( drive, res, MAX_NAME_SZ ) ) throw GetLastError();
	} else
	{
		if ( !_getdcwd( 0, res, MAX_NAME_SZ ) ) throw GetLastError();
	}

	return (res);
}

static char *vmGetvolume( vmInstance *instance, nParamType num )
{
	VAR			*var;
	char		*res;
	char		 dStr[3] = {};
	char const	*param;
	DWORD		 serialNum;
	DWORD		 maxCompLen;
	DWORD		 fsFlags;

	res = (char *)instance->om->alloc ( MAX_PATH );

	param = dStr;

	dStr[1] = ':';
	dStr[2] = 0;

	if ( num )
	{
		if ( num == 1 )
		{
			var = num[0];
			switch ( TYPE( var ) )
			{
				case slangType::eLONG:
					dStr[0] = static_cast<char>(var->dat.l + 'A');
					break;
				case slangType::eDOUBLE:
					dStr[0] = static_cast<char>(static_cast<char>(var->dat.d) + 'A');
					break;
				case slangType::eSTRING:
					param = var->dat.str.c;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		} else
		{
			throw errorNum::scINVALID_PARAMETER;
		}

		GetVolumeInformation( param, res, 256, &serialNum, &maxCompLen, &fsFlags, 0, 0 );
	} else
	{
		GetVolumeInformation( 0, res, 256, &serialNum, &maxCompLen, &fsFlags, 0, 0 );
	}

	return (res);
}

static double vmDisksize( char const *name )
{
	ULARGE_INTEGER	freeBytes;
	ULARGE_INTEGER	totalBytes;
	ULARGE_INTEGER	totalFreeBytes;

	GetDiskFreeSpaceEx( name, &freeBytes, &totalBytes, &totalFreeBytes );

	return ((double)((signed __int64)totalBytes.QuadPart));
}

static double vmDiskspace( char const *name )
{
	ULARGE_INTEGER	freeBytes;
	ULARGE_INTEGER	totalBytes;
	ULARGE_INTEGER	totalFreeBytes;

	GetDiskFreeSpaceEx( name, &freeBytes, &totalBytes, &totalFreeBytes );

	return ((double)((signed __int64)freeBytes.QuadPart));
}

static int vmClose ( class vmInstance *instance, int handle )
{
	if ( handle == -1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	instance->resourceFree ( (void *) (ptrdiff_t) handle, [] ( void *resource ){_close ( (int) (ptrdiff_t) resource );} );	// NOLINT(performance-no-int-to-ptr)
	return (_close( handle ));
}

static int vmFcreate( class vmInstance *instance, char const *name )
{
	int handle;

	if ( (handle = _open( name, _O_CREAT | _O_BINARY | _O_TRUNC | O_RDWR, _S_IREAD | _S_IWRITE )) != -1 )
	{
		instance->resourceRegister( (void *)(ptrdiff_t)handle,  [] ( void *resource ){_close ( (int) (ptrdiff_t) resource );} , RES_MODE::RES_REINIT );	// NOLINT(performance-no-int-to-ptr)
	}
	return (handle);
}

static int vmOpen( class vmInstance *instance, char const *name, nParamType num )
{
	int  mode;
	int  shareMode;
	int  handle;
	VAR *var;

	mode = _O_RDWR | _O_BINARY;
	shareMode = _SH_DENYNO;

	if ( num )
	{
		switch ( num )
		{
			case 2:
				var = num[1];

				switch ( TYPE( var ) )
				{
					case slangType::eLONG:
						shareMode = (bool)var->dat.l ? _SH_DENYRW : _SH_DENYNO;
						break;
					case slangType::eDOUBLE:
						shareMode = (bool)var->dat.d ? _SH_DENYRW : _SH_DENYNO;
						break;
					case slangType::eSTRING:
						shareMode = (bool)atoi( var->dat.str.c ) ? _SH_DENYRW : _SH_DENYNO;
						break;
					default:
						throw errorNum::scINVALID_PARAMETER;
				}
				[[fallthrough]];
			case 1:
				var = num[0];

				switch ( TYPE( var ) )
				{
					case slangType::eLONG:
						mode = (int)var->dat.l;
						break;
					case slangType::eDOUBLE:
						mode = (int)var->dat.d;
						break;
					case slangType::eSTRING:
						mode = atoi( var->dat.str.c );
						break;
					default:
						throw errorNum::scINVALID_PARAMETER;
				}
				break;
		}
	}

	if ( !mode )
	{
		mode = _O_RDWR | _O_BINARY;
	}

	if ( (handle = _sopen( name, mode, shareMode, _S_IREAD | _S_IWRITE )) != -1 )
	{
		instance->resourceRegister( (void *)(ptrdiff_t)handle,  [] ( void *resource ){_close ( (int) (ptrdiff_t) resource );} , RES_MODE::RES_REINIT );	// NOLINT(performance-no-int-to-ptr)
	}

	return (handle);
}

static int vmWrite( class vmInstance *instance, int handle, VAR *var )
{
	int  r;

	if ( handle == -1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	switch ( TYPE( var ) )
	{
		case slangType::eSTRING:
			r = _write( handle, var->dat.str.c, (int)var->dat.str.len );
			break;
		case slangType::eLONG:
			r = _write( handle, &(var->dat.l), sizeof( long ) );
			break;
		case slangType::eDOUBLE:
			r = _write( handle, &(var->dat.d), sizeof( double ) );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	return r;
}

static int vmWriteline( class vmInstance *instance, int handle, VAR *var )
{
	int  r;
	if ( handle == -1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	switch ( TYPE( var ) )
	{
		case slangType::eSTRING:
			r = _write( handle, var->dat.str.c, (int)var->dat.str.len );
			break;
		case slangType::eLONG:
			r = _write( handle, &(var->dat.l), sizeof( long ) );
			break;
		case slangType::eDOUBLE:
			r = _write( handle, &(var->dat.d), sizeof( double ) );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	r += _write( handle, "\r\n", 2 );

	return (r);
}

static VAR_STR vmRead( class vmInstance *instance, int handle, int len )
{
	if ( handle == -1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	auto ret = (char *)instance->om->alloc ( (size_t) len + 1 );

	if ( (len = _read( handle, ret, len )) == -1 )
	{
		instance->result.type = slangType::eNULL;
		throw errorNum::scINVALID_PARAMETER;
	}
	ret[len] = 0;
	return VAR_STR ( ret, len );
}

static uint64_t vmReadLong( int handle )
{
	uint64_t x;

	if ( handle == -1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	if ( _read( handle, &x, sizeof( uint64_t ) ) != sizeof( uint64_t ) ) throw GetLastError();

	return x;
}

static double vmReadDouble( int handle )
{
	double x;

	if ( handle == -1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	if ( _read( handle, &x, sizeof( double ) ) == sizeof( double ) ) throw GetLastError();

	return x;
}


static VAR_STR vmReadline( class vmInstance *instance, int handle )
{
	char	  chr;
	BUFFER	  buffer;

	if ( handle == -1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	if ( _read( handle, &chr, 1 ) == 1 )
	{
		while ( chr != '\n' )
		{
			if ( chr != '\r' )
			{
				bufferPut8 ( &buffer, chr );
			} else
			{
				if ( _read( handle, &chr, 1 ) != 1 )
				{
					break;
				}
				if ( chr != '\n' )
				{
					_lseek( handle, -1, SEEK_CUR );
				}
				break;
			}
			if ( _read( handle, &chr, 1 ) != 1 )
			{
				break;
			}
		}
	}
	bufferPut8 ( &buffer, 0 );

	return VAR_STR ( instance, buffer );

}

static VAR vmGetdir( class vmInstance *instance, char const *fspec, int64_t attrib )
{
	VAR					  result;
	int64_t			      loop;
	char				  tmpC[MAX_PATH];
	HANDLE				  handle;
	WIN32_FIND_DATA		  findFileData;
	FILETIME			  fileTime;
	SYSTEMTIME			  systemTime;

	result = VAR_ARRAY ( instance );

	if ( (handle = FindFirstFile( fspec, &findFileData )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		/* no matching files */
		return result;
	}

	loop = 0;
	while ( 1 )
	{
		if ( ((findFileData.dwFileAttributes & attrib) == attrib) || !attrib )
		{
			loop++;

			FileTimeToLocalFileTime( &(findFileData.ftLastWriteTime), &fileTime );
			FileTimeToSystemTime( &fileTime, &systemTime );

			///////////////////////// File Name /////////////////////////////
			arrayGet ( instance, &result, loop, 1 ) = VAR_STR ( instance, findFileData.cFileName );

			//////////////////////// File Size /////////////////////////////
			arrayGet ( instance, &result, loop, 2 ) = VAR ( ((int64_t)findFileData.nFileSizeLow + ((int64_t)findFileData.nFileSizeHigh << 32)) );

			/////////////////////// File Date //////////////////////////////
			_snprintf_s( tmpC, 11, _TRUNCATE, "%02u/%02u/%04u", systemTime.wMonth, systemTime.wDay, systemTime.wYear );
			arrayGet ( instance, &result, loop, 3 ) = VAR_STR ( instance, tmpC, 10 );

			/////////////////////// File Time //////////////////////////////
			_snprintf_s( tmpC, 9, _TRUNCATE, "%02u:%02u:%02u", systemTime.wHour, systemTime.wMinute, systemTime.wSecond );
			arrayGet ( instance, &result, loop, 4 ) = VAR_STR ( instance, tmpC, 9 );

			/////////////////////// File Attributes ////////////////////////
			arrayGet ( instance, &result, loop, 5 ) = VAR ( (int64_t) findFileData.dwFileAttributes );

			/////////////////////// Sortable Date/Time Time //////////////////////////////
			_snprintf_s( tmpC, 20, _TRUNCATE, "%04u/%02u/%02u %02u:%02u:%02u", systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond );
			arrayGet ( instance, &result, loop, 3 ) = VAR_STR ( instance, tmpC, 19 );
		}
		if ( !FindNextFile( handle, &findFileData ) )
		{
			break;
		}
	}
	FindClose( handle );

	return result;
}

static long vmCreateDirectory( char const *dName )
{
	return CreateDirectory( dName, 0 );
}

static size_t vmCopyFile( LPCTSTR src, LPCTSTR dst, BOOL failIfExists )

{
	return (CopyFile( src, dst, failIfExists ));
}

static VAR_STR vmFparsefx( vmInstance *instance, char const *file )
{
	std::filesystem::path in ( file );
	auto ext = in.extension ().string ();
	if ( ext.size () )
	{
		return VAR_STR ( instance, ext.c_str() + 1, ext.size() - 1 );
	} else
	{
		return VAR_STR ( "", 0 );
	}
}

static VAR_STR vmFparsefp ( vmInstance *instance, char const *file )
{
	std::filesystem::path in ( file );
	return VAR_STR ( instance, in.root_directory ().string () );
}

static VAR_STR vmFparsedr( vmInstance *instance, char const *file )
{
	std::filesystem::path in ( file );
	return VAR_STR ( instance, in.root_name().string () );
}

static VAR_STR vmFparsefn( class vmInstance *instance, char const *file )
{
	std::filesystem::path in ( file );
	return VAR_STR ( instance, in.stem ().string () );
}

static VAR_STR vmFmerge( class vmInstance *instance, char const *temp, char const *inName )
{
	std::filesystem::path pat ( temp );
	std::filesystem::path in ( inName );
	std::filesystem::path d;

	if ( pat.root_path ().string ().c_str ()[0] == '*' )
	{
		d = in.root_path ();
	} else
	{
		d = pat.root_path ();
	}

	if ( std::filesystem::path ( pat ).remove_filename ().string ().c_str ()[0] == '*' )
	{
		d /= std::filesystem::path ( in ).remove_filename ();
	} else
	{
		d /= std::filesystem::path ( pat ).remove_filename ();
	}

	if ( pat.stem ().generic_string ().c_str ()[0] == '*' )
	{
		d /= in.stem ();
	} else
	{
		d /= pat.stem ();
	}

	if ( pat.extension ().generic_string ().c_str ()[0] == '*' )
	{
		d += in.extension ();
	} else
	{
		d += pat.extension ();
	}

	return VAR_STR( instance, d.generic_string () );
}

static VAR_STR vmFFullName ( class vmInstance *instance, char const *file )
{
	std::filesystem::path in ( file );
	return VAR_STR ( instance, in.filename().generic_string () );
}

static VAR_STR vmFFullPath ( class vmInstance *instance, char const *file )
{
	std::filesystem::path in ( file );
	return VAR_STR ( instance, (in.remove_filename()).generic_string () );
}

static VAR_STR vmFmake( class vmInstance *instance, char const *drive, char const *path, char const *name, char const *ext )
{
	std::vector<char const *> components = { drive, path, name, ext };
	
	return VAR_STR ( instance, std::accumulate ( std::next ( components.begin () ), components.end (), std::filesystem::path {}, std::divides {} ).generic_string () );
}

static VAR_STR vmFileGetContentsCache ( vmInstance *instance, char const *fName )
{
	auto entry = globalFileCache.read ( fName );

	auto var = VAR_STR ( instance, (char const *)entry->getData(), entry->getDataLen() );

	entry->free ( );

	return var;
}

static VAR_STR vmFileGetContents ( vmInstance *instance, char const *fName )
{
	HANDLE  hFile;

	if ( (hFile = CreateFile ( fName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, 0 )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		throw GetLastError ( );
	}

	auto len = GetFileSize ( hFile, 0 );
	auto ret = (char *) instance->om->alloc ( (size_t) len + 1 );

	DWORD nBytesRead;
	if ( !ReadFile ( hFile, ret, len, &nBytesRead, 0 ) )
	{
		CloseHandle ( hFile );
		throw GetLastError ( );
	}
	ret[nBytesRead] = 0;
	CloseHandle ( hFile );

	return VAR_STR ( ret, nBytesRead );
}

static uint64_t vmFileWriteContents ( vmInstance *instance, char const *fName, VAR_STR *data )
{
	HANDLE  hFile;

	if ( (hFile = CreateFile ( fName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, 0 )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		throw GetLastError ( );
	}

	DWORD nBytesWritten;
	if ( !WriteFile ( hFile, data->dat.str.c, (DWORD)data->dat.str.len, &nBytesWritten, 0 ) )
	{
		CloseHandle ( hFile );
		throw GetLastError ( );
	}
	CloseHandle ( hFile );
	return nBytesWritten;
}

static uint64_t vmFLen ( size_t handle )
{
	BY_HANDLE_FILE_INFORMATION	fi;
	if ( !GetFileInformationByHandle ( (HANDLE) handle, &fi ) )	// NOLINT(performance-no-int-to-ptr)
	{
		throw GetLastError ( );
	}

	return ((uint64_t) fi.nFileSizeHigh << 32) + (uint64_t) fi.nFileSizeLow;
}

static bool vmDeleteFile ( char *fName )
{
	return (bool) (DeleteFile ( fName ) ? true : false);
}

static VAR_STR vmFileUniqueName ( vmInstance *instance, char *fName )
{
	BYTE *str;
	GUID guid;
	CoCreateGuid ( &guid );
	UuidToString ( (UUID *)&guid, &str );
	std::string uniqueId ( reinterpret_cast<char *>(str) );
	RpcStringFree ( &str );


	stringi newStem;

	std::filesystem::path in ( fName );
	std::filesystem::path d;

	auto stem = std::filesystem::path ( in ).stem ().string ();

	for ( auto &it : stem )
	{
		switch ( it )
		{
			case '?':
				newStem += (char)(_rand () % 10 + '0');
				break;
			case '*':
				newStem += uniqueId;
				break;
			default:
				newStem += it;
				break;
		}
	}

	d = in.root_path ();

	d /= std::filesystem::path ( in ).remove_filename ();

	d /= std::filesystem::path ( newStem.operator const std::string &() );

	d += in.extension ();

	return VAR_STR ( instance, d.string () );
}

static void findFileResourceFree ( void *ptr )
{
	FindClose ( (HANDLE)ptr );
}

static VAR_STR vmFileFindFirst ( vmInstance *instance, char const *fName, int attrib )
{
	auto task = static_cast<vmTaskInstance *>(instance);

	HANDLE findFileHandle = task->getCargo<HANDLE> ( 1 );

	WIN32_FIND_DATA findFileData;

	if ( findFileHandle )
	{
		FindClose ( findFileHandle );
		task->setCargo ( nullptr, vmTaskInstance::cargoType::findFileCargo );
		task->resourceFree ( findFileHandle, findFileResourceFree );
	}

	if ( (findFileHandle = FindFirstFile ( _strdup ( fName ), &(findFileData) )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		return VAR_STR ( "", 0 );
	} else
	{
		task->setCargo ( findFileHandle, vmTaskInstance::cargoType::findFileCargo );	// NOLINT ( clang-analyzer-unix.Malloc )
		task->resourceRegister ( findFileHandle, findFileResourceFree, RES_MODE::RES_REINIT );	// registering as a resouce with reinit cleanup so we don't lose memory
		return VAR_STR ( instance, findFileData.cFileName );
	}
}

static VAR_STR vmFileFindNext ( vmInstance *instance )
{
	auto task = static_cast<vmTaskInstance *>(instance);

	HANDLE findFileHandle = task->getCargo<HANDLE> ( 1 );
	WIN32_FIND_DATA findFileData;

	if ( findFileHandle )
	{
		if ( FindNextFile ( findFileHandle, &(findFileData) ) )
		{
			return VAR_STR ( instance, findFileData.cFileName );
		} else
		{
			FindClose ( findFileHandle );
			task->setCargo ( nullptr, vmTaskInstance::cargoType::findFileCargo );
			task->resourceFree ( findFileHandle, findFileResourceFree );
			return VAR_STR ( "", 0 );
		}
	} else
	{
		throw errorNum::scFIND_FILE_OUT_OF_ORDER;
	}
}

static uint64_t vmFileSize ( char *fName )
{
	int 	handle;
	uint64_t len;

	if ( (handle = _sopen ( fName, _O_RDONLY, _SH_DENYNO )) == -1 )
	{
		return(0L);
	}
	_lseeki64 ( handle, 0, SEEK_END );
	len = _telli64 ( handle );
	_close ( handle );

	return len;
}

static VAR_STR  vmFileDate ( vmInstance *instance, char const *fName )
{
	struct _stat64	 fileStat;
	struct tm		 sTime;	
	char			 date[11];

	if ( _stat64 ( fName, &fileStat ) == -1 )
	{
		localtime_s ( &sTime, &fileStat.st_ctime );

		_snprintf_s ( date, 11, _TRUNCATE, "%02u/%02u/%04u", sTime.tm_mon + 1900, sTime.tm_mday + 1, sTime.tm_year);

		return VAR_STR ( instance, date );
	} else
	{
		return VAR_STR ( "", 0 );
	}
}

static VAR_STR vmFileTime ( vmInstance *instance, char const *fName )
{
	struct _stat64	 fileStat;
	struct tm		 sTime;
	char			 date[9];

	if ( _stat64 ( fName, &fileStat ) == -1 )
	{
		localtime_s ( &sTime, &fileStat.st_ctime );

		_snprintf_s ( date, 11, _TRUNCATE, "%02u:%02u:%02u", sTime.tm_hour, sTime.tm_min, sTime.tm_sec );

		return VAR_STR ( instance, date );
	} else
	{
		return VAR_STR ( "", 0 );
	}
}

void builtinIOInit( class vmInstance *instance, opFile *file )
{
	//--------------------------------------------------------------------------
	//                     General System Support Functions
	//--------------------------------------------------------------------------
	REGISTER( instance, file );
		FUNC ( "fOpen", vmOpen );
		FUNC ( "fCLose", vmClose );
		FUNC ( "fWrite", vmWrite );
		FUNC ( "fWriteLine", vmWriteline );
		FUNC ( "fRead", vmRead );
		FUNC ( "fReadLong", vmReadLong );
		FUNC ( "fReadDouble", vmReadDouble );
		FUNC ( "fReadLine", vmReadline );
		FUNC ( "fCreate", vmFcreate );
		FUNC ( "fErase", remove );
		FUNC ( "fSeek", _lseeki64 );
		FUNC ( "fEOF", _eof );
		FUNC ( "fTell", _telli64 );
		FUNC ( "fLen", vmFLen );
		FUNC ( "fRename", rename );
		FUNC ( "chDir", _chdir );
		FUNC ( "rmDir", _rmdir );
		FUNC ( "curDir", vmCurdir );
		FUNC ( "renDir", rename );
		FUNC ( "diskSize", vmDisksize );
		FUNC ( "diskSpace", vmDiskspace );

		FUNC ( "fileOpen", vmOpen );
		FUNC ( "fileClose", vmClose );
		FUNC ( "fileWrite", vmWrite );
		FUNC ( "fileWriteLine", vmWriteline );
		FUNC ( "fileRead", vmRead );
		FUNC ( "fileReadLong", vmReadLong );
		FUNC ( "fileReadDouble", vmReadDouble );
		FUNC ( "fileReadLine", vmReadline );
		FUNC ( "fileCreate", vmFcreate );
		FUNC ( "fileErase", remove );
		FUNC ( "fileSeek", _lseeki64 );
		FUNC ( "fileEOF", _eof );
		FUNC ( "fileTell", _telli64 );
		FUNC ( "fileLen", vmFLen );
		FUNC ( "fileRename", rename );
		FUNC ( "fileSize", vmFileSize );
		FUNC ( "dirChange", _chdir );
		FUNC ( "dirRemove", _rmdir );
		FUNC ( "dirGetCurrent", vmCurdir );
		FUNC ( "dir", vmGetdir, DEF ( 2, "0" ) );
		FUNC ( "dirMake", _mkdir );
		FUNC ( "dirRename", rename );
		FUNC ( "mkDir", vmCreateDirectory );

		FUNC ( "fileReadAscii", vmFileGetContents );
		FUNC ( "fileGetContents", vmFileGetContents );

		FUNC ( "fileWriteAscii", vmFileWriteContents );
		FUNC ( "fileWriteContents", vmFileWriteContents );

		FUNC ( "fileUniqueName", vmFileUniqueName );
		FUNC ( "fileTempName", vmFileUniqueName );

		FUNC ( "isFile", _isfile ) CONST PURE;
		FUNC ( "fileExists", _isfile );

		FUNC ( "fileDelete", vmDeleteFile );

		FUNC ( "copyFile", vmCopyFile, DEF ( 3, "true" ) );
		FUNC ( "fileCopy", vmCopyFile, DEF ( 3, "false" ) );

		FUNC ( "fParseFN", vmFparsefn ) CONST PURE;
		FUNC ( "fileName", vmFparsefn ) CONST PURE;

		FUNC ( "fparseFP", vmFparsefp ) CONST PURE;
		FUNC ( "filePath", vmFparsefp ) CONST PURE;

		FUNC ( "fparseFX", vmFparsefx ) CONST PURE;
		FUNC ( "fileExtension", vmFparsefx ) CONST PURE;

		FUNC ( "fparseDr", vmFparsedr ) CONST PURE;
		FUNC ( "fileDrive", vmFparsedr ) CONST PURE;

		FUNC ( "fileFullName", vmFFullName) CONST PURE;
		FUNC ( "fileFullPath", vmFFullPath ) CONST PURE;
		FUNC ( "filePathName", vmFparsefp ) CONST PURE;

		FUNC ( "fMerge", vmFmerge ) CONST PURE;
		FUNC ( "fMake", vmFmake ) CONST PURE;

		FUNC ( "fileFindFirst", vmFileFindFirst );
		FUNC ( "fileFindNext", vmFileFindNext );

		FUNC ( "fileDate", vmFileDate );
		FUNC ( "fileTime", vmFileTime );

		FUNC ( "getVolume", vmGetvolume );

		FUNC ( "cacheRead", vmFileGetContentsCache );
	END;
}
