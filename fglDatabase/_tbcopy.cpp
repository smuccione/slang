/*
	_tbcopy - table copy functions

    this function performs a bitewise copy of a file to a new directory
*/

#include "odbf.h"
#include <filesystem>

#include "shlobj.h"

void _tbCopy ( TABLE *dbf, char const *destDir )
{
	SRRWLocker l1 ( dbf->headerLock, false );

	std::filesystem::path src ( dbf->alias );
	std::filesystem::path dest ( destDir );

	dest.replace_filename ( src.filename () );
	dest.replace_extension ( src.extension () );

	SHCreateDirectoryEx ( NULL, std::filesystem::path ( dest ).remove_filename ().generic_string ().c_str(), 0 );

	CopyFile ( dbf->alias, dest.generic_string ().c_str(), 0 );

	if ( dbf->blob )
	{
		blobCopy ( dbf->blob, destDir );
	}
}