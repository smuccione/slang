/*
SLANG support functions

*/


#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"

#undef Z_SOLO
#include "zlib.h"

#define DEFAULT_COMPRESSION Z_DEFAULT_COMPRESSION
#define DEFAULT_MEMLEVEL 9
#define DEFAULT_BUFFERSIZE 8096

static VAR_STR vmDeflate ( class vmInstance *instance, VAR *var )
{
	size_t		 len;
	char const	*buff;
	BUFFER		 buffer;
	char		 tmpBuff[DEFAULT_BUFFERSIZE];
	z_stream	 stream;

	if ( TYPE ( var ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;

	buff = var->dat.str.c;
	len  = var->dat.str.len + 1;

	memset ( &stream, 0, sizeof ( stream ) );

	deflateInit2 ( &stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, DEFAULT_MEMLEVEL, Z_DEFAULT_STRATEGY );

	stream.next_in		= (Bytef *)buff;
	stream.avail_in		= (uInt)len;
	stream.next_out		= (Bytef *)tmpBuff;
	stream.avail_out	= sizeof ( tmpBuff );
	stream.data_type	= Z_ASCII;

	if ( deflate( &stream, Z_NO_FLUSH) != Z_OK )
	{
		throw errorNum::scINTERNAL;
	}

	bufferWrite ( &buffer, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );

	stream.next_out		= (Bytef *)tmpBuff;
	stream.avail_out	= sizeof ( tmpBuff );
	while ( deflate ( &stream, Z_FINISH ) != Z_STREAM_END )
	{
		bufferWrite ( &buffer, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );
		stream.next_out		= (Bytef *)tmpBuff;
		stream.avail_out	= sizeof ( tmpBuff );
	}
	bufferWrite ( &buffer, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );

	deflateEnd ( &stream );

	return VAR_STR ( instance, buffer.buff, buffer.buffPos);
}

static VAR_STR vmInflate ( class vmInstance *instance, VAR *var )

{
	BUFFER		 buffer;
	char		 tmpBuff[DEFAULT_BUFFERSIZE];
	z_stream	 stream;
	size_t		 len;
	char const	*buff;
	int			 ret;

	if ( TYPE ( var ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;

	buff = var->dat.str.c;
	len  = var->dat.str.len;

	memset ( &stream, 0, sizeof ( stream ) );

	stream.next_in		= (Bytef *)buff;
	stream.avail_in		= (uInt)len;
	stream.data_type	= Z_UNKNOWN;

	inflateInit2 (  &stream, 15 + 16 );

	for (;; )
	{
		stream.next_out		= (Bytef *)tmpBuff;
		stream.avail_out	= (uInt)sizeof ( tmpBuff );

		if ( stream.total_in < (unsigned)len )
		{
			ret = inflate ( &stream, Z_NO_FLUSH );

			switch ( ret )
			{
				case Z_OK:
					bufferWrite ( &buffer, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );
					break;
				case Z_STREAM_END:
					bufferWrite ( &buffer, tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );
					inflateEnd ( &stream );

					return VAR_STR ( instance, bufferBuff ( &buffer ), bufferSize ( &buffer ) );
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		} else
		{
			inflateEnd ( &stream );

			return VAR_STR ( instance, bufferBuff ( &buffer ), bufferSize ( &buffer ) );
		}
	}
}

void builtinCompressionInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		FUNC ( "inflate", vmInflate ) CONST PURE;
		FUNC ( "deflate", vmDeflate ) CONST PURE;
	END;
}
