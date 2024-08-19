/*

	New style blob

*/

#include "odbf.h"
#include <filesystem>

#include "Utility/buffer.h"
#include "zlib.h"


BLOB_FILE *blobInit ( char const *fName, uint32_t updateCount )
{
	BLOB_FILE				*blob;
	BLOB_HEADER				*header;

	blob = new BLOB_FILE ( );

	if ( (blob->fileHandle = CreateFile ( fName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		delete blob;
		throw (errorNum) GetLastError();
	}

	BY_HANDLE_FILE_INFORMATION	 fileInfo;
	GetFileInformationByHandle ( blob->fileHandle, &fileInfo );
	blob->fileSize = ((uint64_t) fileInfo.nFileSizeHigh << 32) + fileInfo.nFileSizeLow;

	blob->mapFile ( );

	header = blob->getHeader ( );

	strncpy_s ( blob->name, sizeof ( blob->name ), fName, _TRUNCATE );

	if ( memcmp ( header->ID, BLOB_ID, sizeof ( BLOB_ID ) ) != 0 )
	{
		delete blob;
		return nullptr;
	}

	if ( header->version != 4 )
	{
		delete blob;
		return nullptr;
	}
#if 0
	if ( header->updateCount != updateCount )
	{
		delete blob;
		return nullptr;
	}
#endif

	blob->blockSize		= header->blockSize;
	blob->blockDataSize = header->blockSize - sizeof ( BLOB_FIELD );

	return blob;
}

BLOB_FILE *blobInitRead ( char const *fName, uint32_t updateCount )
{
	BLOB_FILE				*blob;
	BLOB_HEADER				*header;

	blob = new BLOB_FILE ( );

	if ( (blob->fileHandle = CreateFile ( fName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		delete blob;
		throw (errorNum) GetLastError();
	}

	BY_HANDLE_FILE_INFORMATION	 fileInfo;
	GetFileInformationByHandle ( blob->fileHandle, &fileInfo );
	blob->fileSize = ((int64_t) fileInfo.nFileSizeHigh << 32) + fileInfo.nFileSizeLow;

	blob->mapFile ( );

	header = blob->getHeader ( );

	strncpy_s ( blob->name, sizeof ( blob->name ), fName, _TRUNCATE );

	blob->blockSize = BLOB_DEFAULT_BLOCK_SIZE;
	blob->blockDataSize = header->blockSize - sizeof ( BLOB_FIELD );

	return blob;
}

BLOB_FILE *blobCreate ( char const *fName, uint32_t updateCount )
{
	HANDLE				 fHandle;
	BLOB_HEADER			 header{};
	LARGE_INTEGER		 fileSize{};
	DWORD				 nWritten;
	
	if	( (fHandle = CreateFile (	fName, 
									GENERIC_READ | GENERIC_WRITE,
									FILE_SHARE_READ,
									0,
									CREATE_NEW,
									FILE_ATTRIBUTE_ARCHIVE /*| FILE_FLAG_WRITE_THROUGH */ | FILE_FLAG_RANDOM_ACCESS,
									0
								)) == INVALID_HANDLE_VALUE	// NOLINT (performance-no-int-to-ptr)
		)
	{	
		/* damn. no file. oh well. .*/
		return ( 0 );
	}

	memcpy ( header.ID, BLOB_ID, sizeof ( BLOB_ID ) );
	header.version		= 4;
	header.freeStart	= 0;
	header.blockSize	= BLOB_DEFAULT_BLOCK_SIZE;
//	header.updateCount	= updateCount;

	/* write the header out... we can't map yet as we are groing the file here */
	WriteFile ( fHandle, &header, sizeof ( BLOB_HEADER ), &nWritten, 0 );

	/* round it out to the full block */
	fileSize.QuadPart = header.blockSize;
	SetFilePointer ( fHandle, static_cast<long>(fileSize.LowPart), &fileSize.HighPart, FILE_BEGIN );
	SetEndOfFile ( fHandle );
	CloseHandle ( fHandle );

	return blobInit ( fName, updateCount );
}

BLOB_FILE::~BLOB_FILE ( )
{
	unMapFile ( );
	CloseHandle ( fileHandle );
}

static void blobMakeBCC ( BLOB_FIELD *blobField )
{
	unsigned long	bcc;
	unsigned int	loop;

	blobField->blockCheck = 0;

	bcc = 0;
	for ( loop = 0; loop < sizeof ( BLOB_FIELD ); loop += sizeof ( unsigned long ) )
	{
		bcc += *(unsigned long *)((char *)(blobField) + loop);
	}
	blobField->blockCheck = ~bcc + 1;	// so sum including is 0 
}

static bool blobCheckBlock ( BLOB_FILE *blob, BLOB_FIELD *blobField, int64_t relation, bool free )
{
	unsigned long	bcc;
	unsigned int	loop;

	bcc = 0;
	for ( loop = 0; loop < sizeof ( BLOB_FIELD ); loop += sizeof ( unsigned long ) )
	{
		bcc += *(unsigned long *)(((char *)blobField) + loop);
	}

	if ( free )
	{
		if ( !blobField->free )
		{
			bcc = 1;
		}
	} else
	{
		if ( !blobField->used )
		{
			bcc = 1;
		}

		if ( relation && (blobField->dat.used.relation != relation) )
		{
			// will show as error
			bcc = 1;
		}
		if ( blobField->dat.used.size > blob->blockDataSize )
		{
			// bad data size 
			bcc = 1;
		}
	}

	if ( (uint32_t)blobField->free == (uint32_t)blobField->used )
	{
		// can't ever happen
		bcc = 1;
	}

	return bcc == 0;
}

// alloacte BLOB_ALLOCATION_QUANTA new free blocks and add them to the free chain
static void blobAllocateFreeBlocks ( BLOB_FILE *blob )
{
	size_t					 loop;
	int64_t					 addr;
	BLOB_FIELD				*blobField;
	BLOB_HEADER				*header;					/* blob header						 */

	addr = static_cast<int64_t>(blob->fileSize);

	// get our file size
	blob->fileSize += static_cast<size_t>(blob->getHeader ( )->blockSize) * BLOB_ALLOCATION_QUANTA;
	blob->mapFile ( );

	header = blob->getHeader ( );

	for ( loop = 0; loop < BLOB_ALLOCATION_QUANTA; loop++ )
	{
		blobField = blob->getField ( addr );

		memset ( blobField, 0, sizeof ( BLOB_FIELD ) );		// just clear out the header 

		blobField->free = 1;
		blobField->dat.free.next	= header->freeStart;

		blobMakeBCC ( blobField );
		
		header->freeStart	= addr;

		addr += (int64_t)blob->blockSize;
	}
}

static int64_t	blobGetFreeBlock ( BLOB_FILE *blob )
{
	BLOB_FIELD				*blobField;
	BLOB_HEADER				*header;					/* blob header						 */
	int64_t				 addr;

	header = blob->getHeader ( );

	if ( !header->freeStart )
	{
		blobAllocateFreeBlocks ( blob );
	}

	// it may have moved so need to get it again... need to do this whenever we may grow a file
	header = blob->getHeader ( );

	addr = (int64_t)header->freeStart;

	blobField = blob->getField ( addr );

	if ( !blobCheckBlock ( blob, blobField, 0, 1 ) )
	{
		// crap... bad block...

		header->freeStart = 0;

		blobAllocateFreeBlocks ( blob );

		header = blob->getHeader ( );
		addr = (int64_t)header->freeStart;

		if ( !blobCheckBlock ( blob, blobField, 0, 1 ) )
		{
			throw errorNum::scDATABASE_INTEGRITY;
		}
	}

	header->freeStart = blobField->dat.free.next;
	
	memset ( blobField, 0, sizeof ( BLOB_FIELD ) );
	blobField->used	= 1;
	return addr;
}

#define DEFAULT_COMPRESSION (Z_DEFAULT_COMPRESSION)
#define DEFAULT_WINDOWSIZE (-15)	// no wrapper
#define DEFAULT_MEMLEVEL (9)
#define DEFAULT_BUFFERSIZE (8096)

void nullFunc ( void )
{}

struct blobScratchPad {
	unsigned long	ptr = 0;
	char			data[524288];
};

voidpf blobZlibAlloc ( voidpf opaque, uInt items, uInt size )

{
	blobScratchPad	*pad;
	voidpf			 ret;

	pad = (blobScratchPad *)opaque;

	ret = (voidpf)( pad->data + pad->ptr );
	pad->ptr += items * size;

	memset ( ret, 0, static_cast<size_t>(items) * size );
	return ( ret );
}

int64_t blobWrite ( BLOB_FILE *blob, TABLE_CONNECTION *tblConn, int64_t addr, char const *data, int64_t size, int64_t relation )

{
	BLOB_FIELD				*blobField;
	BLOB_HEADER				*header;					/* blob header						 */

	int64_t					 prevAddr;
	int64_t					 nextAddr;
	int64_t					 rootAddr;					/* what will be the start of the blob entry	*/

	char					 tmpBuff[DEFAULT_BUFFERSIZE] = "";			// Optimize out in the future!
	z_stream				 stream;
	size_t					 bufferSize;
	size_t					 nBlocks;
	size_t					 nBlocksCompressed;
	size_t					 dataSize;
	bool					 compressed = false;
	bool					 last;

	blobScratchPad			 scratchPad;

	nBlocks				= (size + blob->blockDataSize - 1) / blob->blockDataSize;

	if  ( nBlocks > 2 )
	{
		bufferReset ( &tblConn->compressBuff );

		memset ( &stream, 0, sizeof ( stream ) );

		stream.zalloc	= blobZlibAlloc;
		stream.zfree	= (free_func)nullFunc;
		stream.opaque	= &scratchPad;

		deflateInit2 ( &stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
						   15, DEFAULT_MEMLEVEL,
						   Z_DEFAULT_STRATEGY);

		stream.next_in		= (Bytef *)data;
		stream.avail_in		= (uInt)size;
		stream.next_out		= (Bytef *)tmpBuff;
		stream.avail_out	= sizeof ( tmpBuff );

		stream.data_type	= Z_BINARY;

		if ( deflate( &stream, Z_NO_FLUSH) != Z_OK )
		{
			bufferSize   = size;
			deflateEnd ( &stream );
			goto BLOB_STORE;
		}
		bufferWrite ( &tblConn->compressBuff, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );

		stream.next_out		= (Bytef *)tmpBuff;
		stream.avail_out	= sizeof ( tmpBuff );
		while ( deflate ( &stream, Z_FINISH ) == Z_OK ) // != Z_STREAM_END )
		{
			bufferWrite ( &tblConn->compressBuff, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );
			stream.next_out		= (Bytef *) tmpBuff;
			stream.avail_out	= sizeof ( tmpBuff );
		}
		bufferWrite ( &tblConn->compressBuff, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );

		deflateEnd ( &stream );

		nBlocks				= (size + blob->blockDataSize - 1) / blob->blockDataSize;
		nBlocksCompressed	= (bufferSize ( &tblConn->compressBuff ) + blob->blockDataSize - 1) / blob->blockDataSize;

		if ( nBlocksCompressed  + 2 < nBlocks )
		{
			bufferSize	= bufferSize ( &tblConn->compressBuff );
			data		= bufferBuff ( &tblConn->compressBuff );
			compressed	= true;
		} else
		{
			bufferSize	= size;
			compressed	= false;
		}
	} else
	{
		bufferSize = size;
		compressed = false;
	}
BLOB_STORE:

	/* ok... here's the trick... if we write BACKWARDS... then we only need to have a single entry 
		mapped at any time since we know the address of the NEXT block in the chain as we're writing backwards.
	*/

	nBlocks	 = (bufferSize + blob->blockDataSize - 1) / blob->blockDataSize;

	if ( !nBlocks )
	{
		nBlocks = 1;	// fix for 0 block issue
	}

	dataSize = bufferSize - (blob->blockDataSize * (nBlocks - 1));		// compute REMAINDER for last block... this will bet set to blob->blockDataSize at bottom of write loop

	data += blob->blockDataSize * (nBlocks - 1);

	prevAddr = 0;
	last = true;

	SRRWLocker l1 ( blob->lock, true );

	header = blob->getHeader ( );

	while ( nBlocks )
	{
		if ( addr )
		{
			blobField = blob->getField ( addr );

			if ( !blobCheckBlock ( blob, blobField, relation, 0 ) )
			{
				addr = 0;
			} else
			{
				nextAddr = (int64_t)blobField->dat.used.next;
			}
		}
		if ( !addr )
		{
			addr = blobGetFreeBlock ( blob );

			blobField = blob->getField ( addr );
		    nextAddr = 0;
		}
		memcpy ( blobField->getData(), data, dataSize );

		blobField->dat.used.compressed	= compressed;
		blobField->dat.used.relation	= (unsigned long)relation;
		blobField->dat.used.next		= prevAddr;
		blobField->dat.used.sequenceNo	= nBlocks;
		blobField->dat.used.size		= (unsigned long)dataSize;

		blobField->dat.used.stop = last;
		last = false;

		if ( nBlocks == 1 )
		{
			blobField->dat.used.start = 1;
			GetSystemTimeAsFileTime ( &(blobField->dat.used.timeStamp) );
		} else
		{
			blobField->dat.used.start = 0;
		}

		blobMakeBCC ( blobField );

		data    -= blob->blockDataSize;

		dataSize = blob->blockDataSize;	// first one (last) may be smaller

		rootAddr  = addr;
		prevAddr  = addr;
		addr	  = nextAddr;
		nBlocks--;
	}

	// return any remaining (if the blob shrunk) to the free pool)
	while ( addr )
	{
		blobField = blob->getField ( addr );

		if ( !blobCheckBlock ( blob, blobField, relation, 0 ) )
		{
			break;
		}

		nextAddr = (int64_t)blobField->dat.used.next;

		blobField->free = 1;
		blobField->used = 0;
		blobField->dat.free.next = header->freeStart;

		blobMakeBCC ( blobField );

		header->freeStart = addr;

		addr = nextAddr;
	}
	return (int64_t)rootAddr;
}

bool blobDelete ( BLOB_FILE *blob, TABLE_CONNECTION *tblConn, int64_t addr, int64_t relation  )
{
	BLOB_FIELD		*blobField;
	BLOB_HEADER		 *header;					/* blob header						 */
	int64_t		 nextAddr;

	SRRWLocker l1 ( blob->lock, true );

	header = blob->getHeader ( );

	while ( addr )
	{
		blobField = blob->getField ( addr );

		if ( !blobCheckBlock ( blob, blobField, relation, 0 ) )
		{
			break;
		}

		nextAddr = (int64_t)blobField->dat.used.next;

		blobField->free = 1;
		blobField->used = 0;
		blobField->dat.free.next = header->freeStart;

		blobMakeBCC ( blobField );

		header->freeStart = addr;

		addr = nextAddr;
	}
	return addr ? false : true;
}

int64_t blobReadBuff ( BLOB_FILE *blob, TABLE_CONNECTION *tblConn, int64_t addr, char *cPtr, int64_t buffLen, int64_t relation )
{
	BLOB_FIELD				*blobField;
	z_stream				 stream;
	char					 tmpBuff[DEFAULT_BUFFERSIZE] = "";
	bool					 compressed;
	bool					 deflateInitialized;
	int32_t					 sequence;
	blobScratchPad			 scratchPad;
	int64_t					 buffLenSave;

	if ( !addr )
	{
		return (0);
	}

	SRRWLocker l1 ( blob->lock, false );

	buffLenSave = buffLen;
	sequence = 1;
	deflateInitialized = 0;

	blobField = blob->getField ( addr );

	if ( !blobCheckBlock ( blob, blobField, relation, 0 ) )
	{
		throw errorNum::scDATABASE_INTEGRITY;
	}
	compressed = blobField->dat.used.compressed;	// add + 1 so 0 can be non-initialized state

	do	{
		if ( sequence != (int64_t)blobField->dat.used.sequenceNo )
		{
			throw errorNum::scDATABASE_INTEGRITY;
		}

		if ( sequence == 1 )
		{
			if ( !blobField->dat.used.start )
			{
				throw errorNum::scDATABASE_INTEGRITY;
			}
		}

		if ( compressed )
		{
			if ( !deflateInitialized )
			{
				memset ( &stream, 0, sizeof ( stream ) );

				stream.zalloc	= blobZlibAlloc;
				stream.zfree	= (free_func)nullFunc;
				stream.opaque	= &scratchPad;

				stream.data_type	= Z_UNKNOWN;

				inflateInit (  &stream );

				deflateInitialized = 1;
			}

			stream.next_out		= (Bytef *) tmpBuff;
			stream.avail_out	= sizeof ( tmpBuff );

			stream.avail_in = blobField->dat.used.size;
			stream.next_in	= (Bytef *)&(blobField[1]);

			for ( ;; )
			{
				stream.next_out		= (Bytef *)tmpBuff;
				stream.avail_out	= (uInt)sizeof ( tmpBuff );

				switch ( inflate ( &stream, Z_NO_FLUSH ) )
				{
					case Z_OK:
						if ( (int64_t)sizeof ( tmpBuff ) - stream.avail_out > buffLen )
						{
							throw errorNum::scDATABASE_INTEGRITY;
						}
						memcpy ( cPtr, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );
						cPtr += sizeof ( tmpBuff ) - stream.avail_out;
						buffLen -= (int64_t)(sizeof ( tmpBuff ) - stream.avail_out);
						break;
					case Z_STREAM_END:
						if ( (int64_t)sizeof ( tmpBuff ) - stream.avail_out > buffLen )
						{
							throw errorNum::scDATABASE_INTEGRITY;
						}
						buffLen -= (int64_t)(sizeof ( tmpBuff ) - stream.avail_out);

						memcpy ( cPtr, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );

						inflateEnd ( &stream );

						// OPTIMIZE we should be able to use the dest buffer directly and not have to worry about size
						//   set the size in the avail_in and we should get an end on first shot...

						if ( buffLen || !blobField->dat.used.stop )
						{
							throw errorNum::scDATABASE_INTEGRITY;
						}
						return buffLenSave ;
					case Z_BUF_ERROR:
						break;
					default:
						throw errorNum::scDATABASE_INTEGRITY;
				}

				if ( stream.avail_out )
				{
					if ( !stream.avail_in )
					{
						// break out so we can get more data to process
						break;
					}
				}
			}
		} else
		{
			if ( blobField->dat.used.size > buffLen )
			{
				throw errorNum::scDATABASE_INTEGRITY;
			}
			memcpy ( cPtr, (char *)&(blobField[1]), blobField->dat.used.size );
			cPtr += blobField->dat.used.size;
			buffLen -= blobField->dat.used.size;
		}

		if ( blobField->dat.used.stop )
		{
			if ( blobField->dat.used.next )
			{
				// potentially bad... we have a stop bit but a next pointer also... these are mutually exculsive
			}
			addr = 0;
		} else
		{
			addr = static_cast<int64_t>(blobField->dat.used.next);
		}
		sequence++;

		if ( addr )
		{
			blobField = blob->getField ( addr );

			if ( !blobCheckBlock ( blob, blobField, relation, 0 ) )
			{
				throw errorNum::scDATABASE_INTEGRITY;
			}

			if ( compressed != static_cast<bool>(blobField->dat.used.compressed) )
			{
				throw errorNum::scDATABASE_INTEGRITY;
			}
		}
	} while ( addr );

	if ( compressed )
	{
		throw errorNum::scDATABASE_INTEGRITY;
	} else
	{
		if ( buffLen )
		{
			throw errorNum::scDATABASE_INTEGRITY;
		}
		return buffLenSave;
	}
}

int64_t blobReadCheck ( BLOB_FILE *blob, TABLE_CONNECTION *tblConn, int64_t addr, int64_t relation )  
{
	z_stream				 stream;
	char					 tmpBuff[DEFAULT_BUFFERSIZE] = "";
	bool					 compressed = 0;
	unsigned long			 sequence;
	bool					 deflateInitialized = 0;;
	blobScratchPad			 scratchPad;

	if ( !addr )
	{
		return ( 0 );
	}

	sequence = 1;
	SRRWLocker l1 ( blob->lock, true );

	size_t size = 0;

	try
	{
		auto blobField = blob->getField ( addr );
		compressed = blobField->dat.used.compressed ? true : false;

		while ( addr )
		{
			if ( addr + blob->blockSize > blob->fileSize )
			{
				throw errorNum::scDATABASE_INTEGRITY;
			}

			blobField = blob->getField ( addr );

			if ( !blobCheckBlock ( blob, blobField, relation, 0 ) )
			{
				throw errorNum::scDATABASE_INTEGRITY;
			}

			if ( compressed )
			{
				if ( compressed != (bool)blobField->dat.used.compressed )
				{
					throw errorNum::scDATABASE_INTEGRITY;
				}
			}
			compressed = blobField->dat.used.compressed;	// add + 1 so 0 can be non-initialized state

			if ( sequence != blobField->dat.used.sequenceNo )
			{
				throw errorNum::scDATABASE_INTEGRITY;
			}

			if ( sequence == 1 )
			{
				if ( !blobField->dat.used.start )
				{
					throw errorNum::scDATABASE_INTEGRITY;
				}
			}

			if ( compressed )
			{
				if ( !deflateInitialized )
				{
					memset ( &stream, 0, sizeof ( stream ) );

					stream.zalloc = blobZlibAlloc;
					stream.zfree = (free_func) nullFunc;
					stream.opaque = &scratchPad;

					stream.data_type = Z_UNKNOWN;

					inflateInit ( &stream );

					deflateInitialized = 1;
				}

				stream.next_out = (Bytef *) tmpBuff;
				stream.avail_out = sizeof ( tmpBuff );

				stream.avail_in = blobField->dat.used.size;
				stream.next_in = (Bytef *) &(blobField[1]);

				for ( ;; )
				{
					stream.next_out = (Bytef *) tmpBuff;
					stream.avail_out = (uInt) sizeof ( tmpBuff );

					switch ( inflate ( &stream, Z_NO_FLUSH ) )
					{
						case Z_OK:
							size += sizeof ( tmpBuff ) - stream.avail_out;
							break;
						case Z_STREAM_END:
							size += sizeof ( tmpBuff ) - stream.avail_out;
							inflateEnd ( &stream );

							// OPTIMIZE we should be able to use the dest buffer directly and not have to worry about size
							//   set the size in the avail_in and we should get an end on first shot...

							if ( !blobField->dat.used.stop )
							{
								throw errorNum::scDATABASE_INTEGRITY;
							}
							return (int64_t)size;
						case Z_BUF_ERROR:
							break;
						default:
							throw errorNum::scDATABASE_INTEGRITY;
					}

					if ( stream.avail_out )
					{
						if ( !stream.avail_in )
						{
							// break out so we can get more data to process
							break;
						}
					}
				}
			} else
			{
				size += blobField->dat.used.size;
			}

			if ( blobField->dat.used.stop )
			{
				if ( blobField->dat.used.next )
				{
					// potentially bad... we have a stop bit but a next pointer also... these are mutually exculsive
				}
				addr = 0;
			} else
			{
				addr = (int64_t)blobField->dat.used.next;
			}
			sequence++;
		}
	} catch ( ... )
	{
		if ( deflateInitialized )
		{
			inflateEnd ( &stream );
		}
		throw;
	}
	if ( deflateInitialized )
	{
		inflateEnd ( &stream );
		throw errorNum::scDATABASE_INTEGRITY;
	}
	return (int64_t)size;
}

void blobCopy ( BLOB_FILE *blob, char const *destDir )
{
	std::filesystem::path src ( blob->name );
	std::filesystem::path dest ( destDir );

	dest.replace_filename ( src.filename () );
	dest.replace_extension ( src.extension () );

	CopyFile ( blob->name, dest.generic_string ().c_str(), 0 );
}

bool blobCheck ( BLOB_FILE *blob )
{
	BLOB_HEADER				*header;					/* blob header						 */
	BLOB_FIELD				*blobField;
	int64_t				 addr;
	unsigned long			 nFree1;
	unsigned long			 nFree2;
	
	SRRWLocker l1 ( blob->lock, true );

	nFree1 = nFree2 = 0;

	addr = (int64_t)blob->blockSize;	// start at the first block

	while ( addr + blob->blockSize <= blob->fileSize )
	{
		blobField = blob->getField ( addr );

		if ( !blobCheckBlock ( blob, blobField, 0, blobField->free ) )
		{
			throw errorNum::scDATABASE_INTEGRITY;
		}

		if ( blobField->free )
		{
			nFree1++;
		}

		addr += (int64_t)blob->blockSize;
	}

	header = blob->getHeader ( );
	addr = (int64_t)header->freeStart;

	while ( addr )
	{
		if ( addr + blob->blockSize > blob->fileSize )
		{
			throw errorNum::scDATABASE_INTEGRITY;
		}

		blobField = blob->getField ( addr );

		if ( !blobField->free )
		{
			throw errorNum::scDATABASE_INTEGRITY;
		}
		
		nFree2++;

		addr = (int64_t)blobField->dat.free.next;
	}

	if ( nFree1 != nFree2 )
	{
		throw errorNum::scDATABASE_INTEGRITY;
	}

	return true;
}

int64_t blobRecover ( BLOB_FILE *blob, TABLE_CONNECTION *tblConn, int64_t *pAddr, BUFFER *buffer, int64_t relation )  
{
	BLOB_FIELD				*blobField;
	int64_t					 addr;
	int64_t					 size;
	FILETIME				 bestTime{};
	int64_t					 bestAddr;
	int64_t					 bestSize;

	bestAddr	= 0;
	addr		= (int64_t)blob->blockSize;	// start at the first block

	while ( addr < (int64_t)blob->fileSize )
	{
		blobField = blob->getField ( addr );

		if ( blobCheckBlock ( blob, blobField, relation, blobField->free ) )  // good field?
		{
			if ( blobField->used )									
			{
				if ( blobField->dat.used.relation == relation )			// is it ours?
				{
					if ( (uint32_t)blobField->dat.used.sequenceNo == 1 )			// is it the start?
					{
						if ( bestAddr )
						{
							// is this one later then the first?
							if ( CompareFileTime ( &blobField->dat.used.timeStamp, &bestTime ) > 0 )
							{
								// can we read it?
								if ( (size = blobReadCheck ( blob, tblConn, addr, relation )) >=0 )
								{
									// use this one...
									bestSize = size;
									bestAddr = addr;
									bestTime = blobField->dat.used.timeStamp;
								}
							}
						} else
						{
							// first one
							if ( (size = blobReadCheck ( blob, tblConn, addr, relation )) >=0 )
							{
								// we can read it so use it for now
								bestSize = size;
								bestAddr = addr;
								bestTime = blobField->dat.used.timeStamp;
							}
						}
					}
				}
			}
		}
		addr += (int64_t)blob->blockSize;
	}

	// did we find any?
	if ( bestAddr )
	{
		// use the latest one that we found
		*pAddr = bestAddr;
		bufferMakeRoom ( buffer, bestSize );
		return blobReadBuff ( blob, tblConn, *pAddr, bufferBuff ( buffer ), bestSize, relation );
	}
	throw errorNum::scDATABASE_INTEGRITY;
}

