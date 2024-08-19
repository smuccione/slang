#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "webServer.h"
#include "webSocket.h"
#include "webProtoSSL.h"
#include "webProtoH1.h"
#include "webProtoH2.h"

#include "Target/fileCache.h"
#include "Utility/funcky.h"

#include "zlib.h"

#define DEFAULT_COMPRESSION (Z_BEST_COMPRESSION)
#define DEFAULT_WINDOWSIZE (-15)
#define DEFAULT_MEMLEVEL (9)
#define DEFAULT_BUFFERSIZE (8096)

static void webGZIPCompress ( char const *fSrc, char const *fDst )
{
	uint8_t		 inBuff[DEFAULT_BUFFERSIZE];
	uint8_t		 outBuff[DEFAULT_BUFFERSIZE] = {};
	z_stream	 stream;
	FILE		*fHandleSrc;
	FILE		*fHandleDst;
    int			 ret;
	int			 flush;
    size_t		 have;

	if ( fopen_s ( &fHandleSrc, fSrc, "rb" ) )
	{
		return;
	}

	if ( fopen_s ( &fHandleDst, fDst, "w+b" ) )
	{
		fclose ( fHandleSrc );
		return;
	}

	memset ( &stream, 0, sizeof ( stream ) );

	deflateInit2 ( &stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                       15 + 16, DEFAULT_MEMLEVEL,
                       Z_DEFAULT_STRATEGY);

	/* compress until end of file */
    do {
        stream.avail_in = (uint32_t)fread(inBuff, sizeof ( inBuff[0] ), (uint32_t)sizeof ( inBuff ), fHandleSrc);
        if (ferror(fHandleSrc)) 
		{
            deflateEnd(&stream);
            return;
        }
        flush = feof(fHandleSrc) ? Z_FINISH : Z_NO_FLUSH;
        stream.next_in = inBuff;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            stream.avail_out = sizeof ( outBuff );
            stream.next_out = outBuff;
            ret = deflate(&stream, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = sizeof ( outBuff ) - stream.avail_out;
            if (fwrite(outBuff, sizeof ( outBuff[0] ), have, fHandleDst) != have || ferror(fHandleDst)) 
			{
                deflateEnd(&stream);
                return;
            }
        } while (stream.avail_out == 0);
        assert(stream.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    deflateEnd(&stream);

	fclose ( fHandleSrc );
	fclose ( fHandleDst );
}

void webStaticPageHandler ( class vmInstance *instance, webProtocolH1 *conn )
{
	char				 fName[MAX_PATH+1];
	fileCacheEntry		*entry;

	char				 fNameComp[MAX_PATH+1];
	struct _stat		 fNameCompStat;
	

	// default to everything being OK
	conn->rsp.rspCode = 200;

	// default to non compressed
	conn->rsp.isCompressed = false;

	conn->rsp.cachePolicy.policyType = webCachePolicy_None;

	// see if we can compress and the browser want's it compressed
	if ( conn->req.mimeType->compressable && (conn->req.compress & compGzip) )
	{
		// build compressable file name
		memcpy_s ( fNameComp, sizeof ( fNameComp ), conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
		memcpy_s ( fNameComp + conn->proc.fileNameBaseLen, sizeof ( fNameComp ) - conn->proc.fileNameBaseLen, conn->req.mimeType->compExt.c_str(), conn->req.mimeType->compExt.size() + 1 );

		if ( (conn->rsp.body.entry = globalFileCache.get ( fNameComp ) ) )
		{
			// compressed version is already in the cache... it MUST therefore be valid so we will continue to use it
			// the file cache MUST by definition maintain only valid entries.   It is incumbent on users of the cache
			// to ensure that only valid entries be stored (read) into the cache.
			conn->rsp.body.bodyType = webServerResponse::body::btFileCacheEntry;
			conn->rsp.modifiedTime = conn->rsp.body.entry->getModifiedTime();
			conn->rsp.isCompressed = true;
		} else
		{
			// build base file name
			memcpy_s ( fName, sizeof ( fName ), conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
			memcpy_s ( fName + conn->proc.fileNameBaseLen, sizeof ( fName ) - conn->proc.fileNameBaseLen, conn->req.mimeType->ext.c_str(), conn->req.mimeType->ext.size() + 1);

			// let's see if we have a non-compressed version (at a minimum).. we don't read here as we're not sure yet that the compressed version is newer, we only query the cache.
			if ( (entry = globalFileCache.read ( fName ) ) )
			{
				// get modified time of the compressed version (non-compressed will have already been in the cache)
				if ( !_stat ( fNameComp, &fNameCompStat ) )
				{
					if ( entry->getModifiedTime() > fNameCompStat.st_mtime )
					{
						// recreate the compressed version, non-compressed is newer
						webGZIPCompress ( fName, fNameComp );
					}
				} else
				{
					// compressed version not available... compress it
					webGZIPCompress ( fName, fNameComp );
				}

				// we have AT LEAST the non-compressed version... now see if we can upgrade
				// NOW we can read it into the cache... we're sure that the compressed version is newer than the non-compressed version
				if ( (conn->rsp.body.entry = globalFileCache.read ( fNameComp ) ) )
				{
					// free the non-compressed version
					globalFileCache.free ( entry );

					// replace it with our version
					conn->rsp.isCompressed = true;
				} else
				{
					// use the non-compressed version
					conn->rsp.body.entry = entry;
					conn->rsp.isCompressed	= false;
				}
				conn->rsp.body.bodyType = webServerResponse::body::btFileCacheEntry;
				conn->rsp.modifiedTime = conn->rsp.body.entry->getModifiedTime ( );
			} else
			{
				// can't get compressed or non-compressed file... return not found 

				if ( conn->req.virtualServer->defaultRedir )
				{

				}
				conn->rsp.body.bodyType = webServerResponse::body::btNone;
				conn->rsp.rspCode = 404;
			}
		}
	} else
	{
		// build base file name
		memcpy_s ( fName, sizeof ( fName ), conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
		memcpy_s ( fName + conn->proc.fileNameBaseLen, sizeof ( fName ) - conn->proc.fileNameBaseLen, conn->req.mimeType->ext.c_str(), conn->req.mimeType->ext.size() + 1 );

		if ( (entry = globalFileCache.read ( fName ) ) )
		{
			conn->rsp.body.entry = entry;
			conn->rsp.body.bodyType = webServerResponse::body::btFileCacheEntry;
			conn->rsp.modifiedTime = entry->getModifiedTime ( );
		} else
		{
			conn->rsp.body.bodyType = webServerResponse::body::btNone;
			conn->rsp.rspCode = 404;
		}
	}
}
