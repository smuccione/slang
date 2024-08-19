
#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>
#include <stdint.h>
#include <filesystem>

#include "Db.h"
#include "Utility/funcky.h"


void _tbCreate ( TABLE *dbf )
{
	char				 buff[16]{};
	DWORD				 nWritten;
	HANDLE				 fHandle;
	LARGE_INTEGER		 filePointer{};


	/* now. try to open the file */
	if	( (fHandle = CreateFile (	dbf->alias, 
									GENERIC_READ | GENERIC_WRITE,
									FILE_SHARE_READ,
									0,
									CREATE_NEW,
									FILE_ATTRIBUTE_ARCHIVE  /*| FILE_FLAG_WRITE_THROUGH */ | FILE_FLAG_RANDOM_ACCESS,
									0
								)) == INVALID_HANDLE_VALUE	// NOLINT (performance-no-int-to-ptr)
		)
	{	
		throw (errorNum) GetLastError ( );
	}

	// write out the table header
	WriteFile ( fHandle, &dbf->header, sizeof ( DBFHEADER ), &nWritten, 0 );

	WriteFile ( fHandle,  dbf->fields, dbf->fcount * sizeof ( DBFFIELDS ), &nWritten, 0 );
	WriteFile ( fHandle, buff, sizeof ( char ) * 2, &nWritten, 0 );

	// we're now at the start of the data portion... let's find it
	filePointer.QuadPart = 0;
	SetFilePointerEx ( fHandle, filePointer, &filePointer, FILE_END );
	dbf->header.dataStartOffset = filePointer.QuadPart;

	// rewrite the file header with the updated data start entry
	filePointer.QuadPart = 0;
	SetFilePointerEx ( fHandle, filePointer, &filePointer, FILE_BEGIN );
	WriteFile ( fHandle, &dbf->header, sizeof ( DBFHEADER ), &nWritten, 0 );

	if( dbf->header.capabilities & DBF_CAPABILITY_BLOB )
	{
		std::filesystem::path p ( dbf->alias );
		p.replace_extension ( __BLOBExtension );
		if( !(dbf->blob = blobCreate( p.generic_string ().c_str(), dbf->getUpdateCount() )))
		{
			CloseHandle ( fHandle );
			throw (errorNum) GetLastError();
		}
	}

	CloseHandle ( fHandle );
}
