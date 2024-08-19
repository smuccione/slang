#pragma once

#ifndef NOMINMAX
# define NOMINMAX
#endif


#include <Windows.h>

#include <stdint.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "Target/fileCache.h"

#include "zlib.h"

#include "webProtoH1.h"

#pragma pack ( push, 1 )

struct zipLocalFileHeader {
	uint32_t	signature = 0x04034b50;
	uint16_t	version = 45;			// version needed to extract
	uint16_t	flags = 0x08;			// data descriptor present
	uint16_t	compression = 0x08;		
	uint16_t	modTime = 0;
	uint16_t	modDate = 0;
	uint32_t	nullCRC32 = 0xFFFFFFFF;
	uint32_t	compressedSize = 0xFFFFFFFF;
	uint32_t	uncompressedSize = 0xFFFFFFFF;
	uint16_t	fileNameLength = 0;
	uint16_t	extraLength = 0;
	char		buff[2048];

	size_t		cFileSize = 0;			// never written... working values, not sent in the file header
	size_t		fileSize = 0;
	size_t		streamOffset = 0;
	uint32_t	crc32 = 0;

	struct zip64Extended {
		uint16_t	id;
		uint16_t	len = sizeof ( zip64Extended ) - offsetof ( zip64Extended, origionalSize );
		size_t		origionalSize;
		size_t		compressedSize = 0;

		zip64Extended ( size_t origionalSize ) : id ( 1 ), origionalSize ( origionalSize )
		{
		}
	};
	zipLocalFileHeader ( )
	{

	}
	zipLocalFileHeader ( std::string const &fName, std::string const &zipName, BY_HANDLE_FILE_INFORMATION const &fi )
	{
		SYSTEMTIME	st;
		FileTimeToSystemTime ( &fi.ftCreationTime, &st );

		modTime = st.wSecond / 2 + (st.wMinute << 5) + (st.wHour << 11);
		modDate = st.wDay + (st.wMonth << 5) + ((st.wYear - 1980) << 9);

		memcpy ( buff, zipName.c_str ( ), zipName.size ( ) );
		fileNameLength = (uint16_t) zipName.size ( );

		new (buff + zipName.size ( )) zip64Extended ( ((size_t) fi.nFileSizeHigh << 32) + (size_t) fi.nFileSizeLow );
		extraLength += sizeof ( zip64Extended );
	}

	operator uint32_t ( )
	{
		return (uint32_t) offsetof ( struct zipLocalFileHeader, buff ) + fileNameLength + extraLength;
	}
	operator char * ()
	{
		return (char *)this;
	}
};

struct zipDataDescriptor {
	uint32_t	signature = 0x08074b50;
	uint32_t	crc32;
	size_t	compressedSize;
	size_t	uncompressedSize;

	zipDataDescriptor ( )
	{
	}
	zipDataDescriptor ( uint32_t crc32, size_t compressedSize, size_t uncompressedSize ) : crc32 ( crc32 ), compressedSize ( compressedSize ), uncompressedSize ( uncompressedSize )
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

struct zipCentralDirectory {
	uint32_t	signature = 0x02014b50;
	uint16_t	versionMadeBy = 0x1E;
	uint16_t	versionNeedToExtract;
	uint16_t	flags;
	uint16_t	compression;
	uint16_t	modTime;
	uint16_t	modDate;
	uint32_t	crc32;
	uint32_t	compressedSize;
	uint32_t	uncompressedSize;
	uint16_t	fileNameLength;
	uint16_t	extraLength = 0;
	uint16_t	fileCommentLength = 0;
	uint16_t	diskNumberStart = 0;
	uint16_t	internalAttributes = 0;
	uint32_t	externalAttributes = 0;
	uint32_t	offsetOfLocalFileHeader = 0;
	char		buff[sizeof ( zipLocalFileHeader::buff )];

	zipCentralDirectory ( )
	{
	}

	zipCentralDirectory ( zipLocalFileHeader &header )
	{
		versionNeedToExtract = header.version;
		flags = header.flags;
		compression = header.compression;
		modTime = header.modTime;
		modDate = header.modDate;
		crc32 = header.crc32;
		compressedSize = header.cFileSize > 0xFFFFFFFF ? 0xFFFFFFFF : (uint32_t)header.cFileSize;
		uncompressedSize = header.fileSize > 0xFFFFFFFF ? 0xFFFFFFFF : (uint32_t) header.fileSize;
		offsetOfLocalFileHeader = header.streamOffset > 0xFFFFFFFF ? 0xFFFFFFFF : (uint32_t) header.streamOffset;
		fileNameLength = header.fileNameLength;

		memcpy ( buff, header.buff, header.fileNameLength );

		char *extra = buff + fileNameLength;
		*(uint16_t *) extra = 0x0001;
		extra += sizeof ( uint16_t );

		char *size = extra;
		*(uint16_t *)extra = 0;
		extra += sizeof ( uint16_t );

		if ( uncompressedSize == 0xFFFFFFFF )
		{
			*(size_t *) extra = header.fileSize;
			extra += sizeof ( size_t );
		}
		if ( compressedSize == 0xFFFFFFFF )
		{
			*(size_t *) extra = header.cFileSize;
			extra += sizeof ( size_t );
		}
		if ( offsetOfLocalFileHeader == 0xFFFFFFFF )
		{
			*(size_t *) extra = header.streamOffset;
			extra += sizeof ( size_t );
		}
		extraLength = (uint16_t)(extra - (buff + fileNameLength));
		*(uint16_t *)size = extraLength - (sizeof ( uint16_t ) * 2);
	}


	operator uint32_t ( )
	{
		return (uint32_t) offsetof ( struct zipCentralDirectory, buff ) + fileNameLength + extraLength;
	}
	operator char * ()
	{
		return (char *)this;
	}
};

struct zip64EndOfCentralDirectoryRecord {
	uint32_t	signature = 0x06064b50;
	size_t		size = sizeof ( zip64EndOfCentralDirectoryRecord ) - offsetof ( zip64EndOfCentralDirectoryRecord, versionMadeBy );

	uint16_t	versionMadeBy = 0x1D;
	uint16_t	versionNeedToExtract = 45;
	uint32_t	diskNumber = 0;
	uint32_t	diskNumberStart = 0;

	size_t		nCentralDirectoryEntriesOnDisk;
	size_t		nCentralDirectoryEntries;
	size_t		sizeOfCentralDirectory;
	size_t		offsetOfStartOfCentralDirectory;

	zip64EndOfCentralDirectoryRecord ( )
	{

	}
	zip64EndOfCentralDirectoryRecord ( size_t nCentralDirectoryEntries, size_t sizeOfCentralDirectory, size_t offsetOfStartOfCentralDirectory ) :
				nCentralDirectoryEntriesOnDisk ( nCentralDirectoryEntries ),
				nCentralDirectoryEntries ( nCentralDirectoryEntries ), 
				sizeOfCentralDirectory ( sizeOfCentralDirectory ),
				offsetOfStartOfCentralDirectory ( offsetOfStartOfCentralDirectory )

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

struct zip64EndOfCentralDirectoryLocator {
	uint32_t	signature = 0x07064b50;
	uint32_t	diskNumber = 0;
	size_t		offsetOfzip64EndOfCentralDirectoryRecord;
	uint32_t	numDisks = 1;

	zip64EndOfCentralDirectoryLocator ( )
	{

	}
	zip64EndOfCentralDirectoryLocator ( size_t offsetOfzip64EndOfCentralDirectoryRecord ) : offsetOfzip64EndOfCentralDirectoryRecord ( offsetOfzip64EndOfCentralDirectoryRecord )
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

struct EndOfCentralDirectory {
	uint32_t	signature = 0x06054b50;
	uint16_t	numberofThisDisk = 0;
	uint16_t	numberOfDiskWithCentralDirectory = 0;
	uint16_t	totalNumberOfCentralDirectoryEntriesThisDisk = 0xFFFF;
	uint16_t	totalNumberOfCentralDirectoryEntries = 0xFFFF;
	uint32_t	sizeOfCentralDirectory = 0xFFFFFFFF;
	uint32_t	offsetOfStartOfCentralDirectory = 0xFFFFFFFF;
	uint16_t	zipFileCommentLength = 0;

	EndOfCentralDirectory ( )
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

#pragma pack ( pop )

//char const *stateNames[] = { "SendingNextFile","SendingHeader","SendingBody","SendingBodyEnd","SendingDataDescriptor","SendingCentralDirectory","SendingZip64EndCentralDirectoryRecord","SendingZip64EndCentralDirrectoryLocator","SendingEndOfCentralDirectory","SendingDone" };

class webZipper : public webServerResponse::body::chunked {
	struct fileDef {
		char const			*data = 0;
		size_t				 len = 0;
		std::string			 fileName;
		std::string			 zipName;
		bool				 compress;

		fileDef ( std::string fileName, std::string zipFileName, bool compress ) : fileName ( fileName ), zipName ( zipFileName ), compress ( compress )
		{
		}
	};
	std::vector<fileDef>	 fList;

	enum {
		SendingNextFile,
		SendingHeader,
		SendingBody,
		SendingBodyEnd,
		SendingDataDescriptor,
		SendingCentralDirectory,
		SendingZip64EndCentralDirectoryRecord,
		SendingZip64EndCentralDirrectoryLocator,
		SendingEndOfCentralDirectory,
		SendingDone,
	}						 state = SendingNextFile;

	size_t					 index = 0;

	fileCacheEntry			*entry = 0;
	const uint8_t			*data;				// pointer to memory mapped file data
	size_t					 dataOffset;		// offset into data of next location to compress

	z_stream							stream;
	zipLocalFileHeader					header;
	zipDataDescriptor					dataDescriptor;
	zipCentralDirectory					centralDirectory;
	zip64EndOfCentralDirectoryRecord	zip64EOCDRecord;
	zip64EndOfCentralDirectoryLocator	zip64EOCDLocator;
	EndOfCentralDirectory				EOCD;

	char					 txBuff[65536];
	size_t					 txBytes = 0;			// number of bytes available to send (either txBuff or header)
	size_t					 txOffset = 0;			// offset into (txBuff or header) of bytes to send

	size_t				 streamOffset = 0;
	size_t				 streamCDOffset = 0;
	size_t				 streamZip64CDOffset = 0;

	std::vector<zipLocalFileHeader>	fHeaders;

	public:
	webZipper ()
	{
	}
	~webZipper ( )
	{
		if ( entry )
		{
			globalFileCache.free ( entry );
		}
	}

	void addFile ( char const *fName, char const *zipName, bool compress = true )
	{
		fList.push_back ( fileDef ( std::string ( fName ), std::string ( zipName ),  compress ) );
	}

	operator size_t ( ) override
	{
		switch ( state )
		{
			case SendingHeader:
			case SendingBody:
			case SendingBodyEnd:
			case SendingDataDescriptor:
			case SendingCentralDirectory:
			case SendingZip64EndCentralDirectoryRecord:
			case SendingZip64EndCentralDirrectoryLocator:
			case SendingEndOfCentralDirectory:
				return txBytes;
			default:
				return 0;
		}
	}
	operator uint8_t *() override
	{
		switch ( state )
		{
			case SendingHeader:
				return (uint8_t *) (char *) header + txOffset;
			case SendingBody:
			case SendingBodyEnd:
				return (uint8_t *) txBuff + txOffset;
			case SendingDataDescriptor:
				return (uint8_t *) (char *)&dataDescriptor + txOffset;
			case SendingCentralDirectory:
				return (uint8_t *) (char *) centralDirectory + txOffset;
			case SendingZip64EndCentralDirectoryRecord:
				return (uint8_t *) (char *) zip64EOCDRecord + txOffset;
			case SendingZip64EndCentralDirrectoryLocator:
				return (uint8_t *) (char *) zip64EOCDLocator + txOffset;
			case SendingEndOfCentralDirectory:
				return (uint8_t *) (char *) EOCD + txOffset;
			default:
				return 0;
		}
	}

	void processBuffer ( size_t len ) override
	{
		int ret;

		assert ( len <= txBytes );

		txBytes -= (uint32_t) len;
		txOffset += len;

		streamOffset += len;
		if ( txBytes ) return;

	nextState:
//		printf ( "%s\r\n", stateNames[state] );
		switch ( state )
		{
			case SendingNextFile:
				if ( entry )
				{
					globalFileCache.free ( entry );
				}

				if ( index >= fList.size ( ) )
				{
					index = 0;
					state = SendingCentralDirectory;
					streamCDOffset = streamOffset;
					goto nextState;
				}

				entry = globalFileCache.read ( fList[index].fileName.c_str ( ) );

				if ( !entry )
				{
					index++;
					goto nextState;
				}

				new (header) zipLocalFileHeader ( fList[index].fileName, fList[index].zipName, entry->fileInfo );

				data = entry->getData ( );
				header.fileSize = entry->getDataLen ( );
				dataOffset = 0;

				memset ( &stream, 0, sizeof ( stream ) );

				if ( fList[index].compress )
				{
					deflateInit2 ( &stream, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8, Z_BINARY );
				} else
				{
					deflateInit2 ( &stream, Z_NO_COMPRESSION, Z_DEFLATED, -15, 8, Z_BINARY );
				}
				txBytes = (uint32_t) header;
				txOffset = 0;

				header.crc32 = crc32 ( 0L, Z_NULL, 0 );
				header.streamOffset = streamOffset;

				state = SendingHeader;
				return;
			case SendingHeader:
				stream.avail_in = (header.fileSize - dataOffset) > 65536 ? 65536 : (uint32_t) (header.fileSize - dataOffset);
				stream.avail_out = sizeof ( txBuff );
				stream.next_in = (z_const Bytef *)(data + dataOffset);
				stream.next_out = (Bytef *) txBuff;

				ret = deflate ( &stream, Z_NO_FLUSH );    /* no bad return value */
				assert ( ret != Z_STREAM_ERROR );  /* state not clobbered */
				txOffset = 0;
				txBytes = sizeof ( txBuff ) - stream.avail_out;
				header.crc32 = crc32 ( header.crc32, data + dataOffset, (uInt)(stream.next_in - (data + dataOffset)) );
				dataOffset += stream.next_in - (data + dataOffset);
				header.cFileSize += txBytes;
				state = SendingBody;

				if ( !txBytes )
				{
					// fully buffered so feed it more
					goto nextState;
				}
				break;
			case SendingBody:
				if ( dataOffset < header.fileSize )
				{
					stream.avail_in = (header.fileSize - dataOffset) > 65536 ? 65536 : (uint32_t) (header.fileSize - dataOffset);
					stream.avail_out = sizeof ( txBuff );
					stream.next_in = (z_const Bytef *)(data + dataOffset);
					stream.next_out = (Bytef *) txBuff;

					ret = deflate ( &stream, Z_NO_FLUSH );    /* no bad return value */
					assert ( ret != Z_STREAM_ERROR );  /* state not clobbered */

					txBytes = sizeof ( txBuff ) - stream.avail_out;
					header.crc32 = crc32 ( header.crc32, data + dataOffset, (uInt) (stream.next_in - (data + dataOffset)) );
					dataOffset += stream.next_in - (data + dataOffset);
					header.cFileSize += txBytes;
					txOffset = 0;

					if ( !txBytes )
					{
						// fully buffered inside deflate, so feed it more
						goto nextState;
					}
				} else
				{
					stream.avail_out = sizeof ( txBuff );
					stream.next_out = (Bytef *) txBuff;

					ret = deflate ( &stream, Z_FINISH );    /* no bad return value */
					assert ( ret != Z_STREAM_ERROR );  /* state not clobbered */

					txBytes = sizeof ( txBuff ) - stream.avail_out;
					header.cFileSize += txBytes;
					txOffset = 0;

					if ( ret == Z_STREAM_END )
					{
						deflateEnd ( &stream );
						state = SendingBodyEnd;

						if ( !txBytes )
						{
							goto nextState;
						}
					}
				}
				break;
			case SendingBodyEnd:
				if ( entry )
				{
					globalFileCache.free ( entry );
					entry = 0;
				}

				dataDescriptor = zipDataDescriptor ( header.crc32, header.cFileSize, header.fileSize );
				txBytes = (uint32_t) dataDescriptor;
				txOffset = 0;
				state = SendingDataDescriptor;
				break;
			case SendingDataDescriptor:
				fHeaders.push_back ( header );
				index++;
				state = SendingNextFile;
				goto nextState;
			case SendingCentralDirectory:
				if ( index >= fHeaders.size ( ) )
				{
					zip64EOCDRecord = zip64EndOfCentralDirectoryRecord ( fHeaders.size ( ), streamOffset - streamCDOffset, streamCDOffset );
					txBytes = (uint32_t) zip64EOCDRecord;
					txOffset = 0;
					streamZip64CDOffset = streamOffset;
					state = SendingZip64EndCentralDirectoryRecord;
					break;
				}
				centralDirectory = zipCentralDirectory ( fHeaders[index] );
				txBytes = (uint32_t) centralDirectory;
				txOffset = 0;
				index++;
				break;
			case SendingZip64EndCentralDirectoryRecord:
				zip64EOCDLocator = zip64EndOfCentralDirectoryLocator ( streamZip64CDOffset );
				txBytes = (uint32_t) zip64EOCDLocator;
				txOffset = 0;
				state = SendingZip64EndCentralDirrectoryLocator;
				break;
			case SendingZip64EndCentralDirrectoryLocator:
				EOCD = EndOfCentralDirectory ();
				txBytes = (uint32_t) EOCD;
				txOffset = 0;
				state = SendingEndOfCentralDirectory;
				break;
			case SendingEndOfCentralDirectory:
				break;
			default:
				throw 500;
		}
	}
	void reset ( ) override
	{
		fList.clear ( );
		fHeaders.clear ( );
		index = 0;

		if ( entry )
		{
			globalFileCache.free ( entry );
		}

		txBytes = 0;
		txOffset = 0;
		streamOffset = 0;
		streamCDOffset = 0;
		streamZip64CDOffset = 0;
	}
};
