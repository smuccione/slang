#include "odbf.h"

#include <filesystem>

bool _tbCreateDuplicate ( TABLE *dbf, char const *newPath )
{
	
	char				 buff[16]{};
	char				 fullName[MAX_PATH];
	DWORD				 nWritten;
	HANDLE				 fHandle;
	LARGE_INTEGER		 filePointer{};
	BLOB_FILE			*blob;
	DBFHEADER			 header{};				/* Header for the dbf in use						*/

	_fmerge ( newPath, dbf->alias, fullName, sizeof ( fullName ) );

	/* now. try to open the file */
	if	( (fHandle = CreateFile (	fullName, 
									GENERIC_READ | GENERIC_WRITE,
									FILE_SHARE_READ,
									0,
									CREATE_NEW,
									FILE_ATTRIBUTE_ARCHIVE  /*| FILE_FLAG_WRITE_THROUGH */ | FILE_FLAG_RANDOM_ACCESS,
									0
								)) == INVALID_HANDLE_VALUE	// NOLINT (performance-no-int-to-ptr)
		)
	{	
		/* damn. no file. oh well. .*/
		return false;
	}
	// write out the table header
	WriteFile ( fHandle, &header, sizeof ( DBFHEADER ), &nWritten, 0 );

	// TODO: expand on this to enable get/put triggers
	// like the song, the fields remain the same... just use the origional ones.
	WriteFile ( fHandle,  dbf->fields, dbf->fcount * sizeof ( DBFFIELDS ), &nWritten, 0 );
	WriteFile ( fHandle, buff, sizeof ( char ) * 2, &nWritten, 0 );

	// we're now at the start of the data portion... let's find it
	filePointer.QuadPart = 0;
	SetFilePointerEx ( fHandle, filePointer, &filePointer, FILE_END );
	header.dataStartOffset = filePointer.QuadPart;

	// rewrite the file header with the updated data start entry
	filePointer.QuadPart = 0;
	SetFilePointerEx ( fHandle, filePointer, &filePointer, FILE_BEGIN );
	WriteFile ( fHandle, &header, sizeof ( DBFHEADER ), &nWritten, 0 );

	if( header.capabilities & DBF_CAPABILITY_BLOB )
	{
		std::filesystem::path p ( dbf->alias );
		p.replace_extension ( __BLOBExtension );
		if ( !(blob = blobCreate ( p.string ().c_str (), dbf->getUpdateCount() )) )
		{
			CloseHandle ( fHandle );
			return false;
		}
		delete blob;
	}
	CloseHandle ( fHandle );
	
	return true;
}
