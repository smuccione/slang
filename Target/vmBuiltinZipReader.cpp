/*
SLANG support functions

*/

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <windows.h>

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"
#include "bcVM/vmAllocator.h"

#include <stdlib.h>
#include <stdlib.h>
#include <string>

#include "zlib.h"


struct zipLocalFileHeader {
	uint32_t	 signature = 0;
	uint16_t	 version = 0;
	uint16_t	 flags = 0;
	uint16_t	 compression = 0;
	uint16_t	 modTime = 0;
	uint16_t	 modDate = 0;
	uint32_t	 CRC32 = 0;
	uint32_t	 compressedSize = 0;
	uint32_t	 uncompressedSize = 0;
	uint16_t	 fileNameLength = 0;
	uint16_t	 extraLength = 0;
	char		 buff[2048];

	struct extendedHeader {
		uint16_t	id;
		uint16_t	len;
	};
	struct zip64Extended : public extendedHeader {
		uint64_t	origionalSize;
		uint64_t	compressedSize = 0;
	};

	zipLocalFileHeader ()
	{
	}

	std::string fileName ( )
	{
		return std::string ( (char *)&this[1], fileNameLength );
	}

	uint64_t getCompressedSize ( )
	{
		if ( uncompressedSize == 0xFFFFFFFF )
		{
			uint64_t offset = 0;
			while ( zip64Extended *extended = getExtended ( offset ) )
			{
				if ( extended->id == 1 )
				{
					return extended->compressedSize;
				}
			}
		}
		return 0;
	}

	uint64_t getUncompressedSize ( )
	{
		if ( uncompressedSize == 0xFFFFFFFF )
		{
			uint64_t offset = 0;
			while ( zip64Extended *extended = getExtended ( offset ) )
			{
				if ( extended->id == 1 )
				{
					return extended->origionalSize;
				}
			}
		}
		return 0;
	}

	uint32_t getCRC32 ( )
	{
		return 0xFFFFFFFF;
	}

	operator uint32_t ( )
	{
		return (uint32_t) offsetof ( struct zipLocalFileHeader, buff ) + fileNameLength + extraLength;
	}
	operator char * ()
	{
		return (char *)this;
	}

	private:
	zip64Extended *getExtended ( uint64_t &offset )
	{
		zip64Extended	*next;

		if ( offset > extraLength )
		{
			return 0;
		}
		next = (zip64Extended *) ((buff + fileNameLength) + offset);
		offset += sizeof ( extendedHeader ) + next->len;
		return next;
	}
};

struct zipDataDescriptor {
	uint32_t	crc32;
	uint64_t	compressedSize;
	uint64_t	uncompressedSize;

	zipDataDescriptor ( )
	{
	}
	~zipDataDescriptor ()
	{
	}
	operator uint32_t ( )
	{
		return sizeof ( *this );
	}
	operator char * ()
	{
		return (char *)this;
	}
};

struct zipEncryptionHeader {
	char x;
};

struct zipFileDef {
	zipLocalFileHeader		*fileHeader = nullptr;
	zipDataDescriptor		*dataDescriptor = 0;
	zipEncryptionHeader		*encryptionheader = 0;

	uint64_t				 compressedSize = 0;
	uint64_t				 uncompressedSize = 0;
	uint32_t				 crc32 = 0;

	uint8_t					*basePtr;
	uint64_t				 streamOffset;

	zipFileDef ( uint8_t *basePtr, uint64_t streamOffset )	: basePtr ( basePtr ), streamOffset ( streamOffset )
	{
		fileHeader = new (basePtr + streamOffset) zipLocalFileHeader ();
	}

};

struct EndOfCentralDirectory {
	uint32_t	signature;
	uint16_t	numberofThisDisk;
	uint16_t	numberOfDiskWithCentralDirectory;
	uint16_t	totalNumberOfCentralDirectoryEntriesThisDisk;
	uint16_t	totalNumberOfCentralDirectoryEntries;
	uint32_t	sizeOfCentralDirectory;
	uint32_t	offsetOfStartOfCentralDirectory;
	uint16_t	zipFileCommentLength;
};

struct zipFile {
	std::string		 fileName;

	HANDLE			 fileHandle;
	HANDLE			 memHandle;
	uint64_t		 fileLength;
	uint8_t			*data;

	EndOfCentralDirectory	*EODRecord;

	bool	setEODRecord ( )
	{
		for ( uint64_t index = fileLength - sizeof ( EndOfCentralDirectory ) - sizeof ( EndOfCentralDirectory::signature ); index; index-- )
		{
			if ( ((EndOfCentralDirectory *) (data + index))->signature == 0x06054b50 )
			{
				EODRecord = ((EndOfCentralDirectory *) (data + index));
				return true;
			}
		}
		return false;
	}

	zipFile ( char const *str ) : fileName ( str )
	{
		BY_HANDLE_FILE_INFORMATION	fi;

		if ( !(fileHandle = CreateFile ( str, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL )) )
		{
			throw GetLastError ( );
		}
		if ( !(memHandle = CreateFileMapping ( fileHandle, NULL, PAGE_READONLY, 0, 0, NULL )) )
		{
			CloseHandle ( fileHandle );
			fileHandle = 0;
			throw GetLastError ( );
		}
		data = (uint8_t *) MapViewOfFile ( memHandle, FILE_MAP_READ, 0, 0, 0 );

		GetFileInformationByHandle ( fileHandle, &fi );
		fileLength = ((uint64_t)fi.nFileSizeHigh << 32) + fi.nFileSizeLow;

		if ( !setEODRecord() )
		{
			throw errorNum::scINVALID_PARAMETER;
		}

	}
};

static int cZipReaderNew ( class vmInstance *instance, VAR *obj, VAR_STR *fName )
{
	// allocate and instantiate the 
	new ((zipFile *) classAllocateCargo ( instance, obj, sizeof ( zipFile ) )) zipFile ( fName->dat.str.c );

	return 1;
}

void builtinCompressionInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		CLASS ( "zipReader" );
			METHOD( "new", cZipReaderNew );
#if 0
			ACCESS( "str", cBufferStr );
			ACCESS( "value", cBufferStr );
			IVAR ( "buff" );
			IVAR ( "size" );
			OP ( "=", cBufferAssign );
			OP ( "+=", cBufferAddAssign );
			INSPECTORCB ( cBufferInspector );
#endif

		END;
	END;
}
