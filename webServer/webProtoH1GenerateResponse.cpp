
#include "webServer.h"
#include "webSocket.h"
#include "webProtoH1.h"
#include "webServerGlobalTables.h"

#if 0
static char const *stateName[] =	{
		"webServerResponseStart",
		"webServerResponseMemory",
		"webServerResponseMemoryRangeSingle",
		"webServerResponseMemoryRangeHeaders",
		"webserverResponseMemoryRangeMimeHeaders",
		"webserverResponseMemoryRangeBody",
		"webserverResponseMemoryRangeEnd",
		"webServerResponseStreamHeaders",
		"webServerResponseStreamReadFile",
		"webServerResponseStreamSend",
		"webServerResponseStreamRangeMimeHeaders",
		"webServerResponseStreamRangeReadFile",
		"webServerResponseStreamRangeSend"
	};
#endif

webServerPageState webServerResponse::processBuffer ( size_t len )
{
	if ( len == -1 )
	{
		body.release ( );
		return wsReset;
	}

	for ( ;; )
	{
//		printf ( "state: %s\t\t%i\r\n", stateName[state], len );
		switch ( state )
		{
			case webServerResponseStart:
				printf ( "ERROR: webServerResponse::processBuffer - invalid state - %i\r\n", state );
				throw 500;

			////    Combined Headers and Body in one scatter-gather transmission from memory
			case webServerResponseMemory:
			case webserverResponseMemoryRangeSingle:
				bytesSent += len;
				if ( txBuffList.consume ( len ) ) 
				{
					body.release();
					if ( keepAlive )
					{
						return wsParseURL;
					} else return wsReset;
				}
				return wsSendResponse;

				////   chunked response
			case webServerResponseChunkedHeaders:
				bytesSent += len;
				if ( txBuffList.consume ( len ) )
				{
					state = webServerResponseChunkedLength;
				}
				return wsSendResponse;

			case webServerResponseChunkedLength:
				bytesSent += len;
				if ( txBuffList.consume ( len ) )
				{
					if ( (size_t) body )
					{
						state = webServerResponseChunkedBody;
					} else
					{
						body.release ( );
						if ( keepAlive )
						{
							return wsParseURL;
						} else return wsReset;
					}
				}
				return wsSendResponse;

			case webServerResponseChunkedBody:
				bytesSent += len;
				body.chunker->processBuffer ( len );
				if ( txBuffList.consume ( len ) )
				{
					state = webServerResponseChunkedLength;
				}
				return wsSendResponse;

			////   Range response from memory
			case webServerResponseMemoryRangeHeaders:
				bytesSent += len;
				if ( txBuffList.consume ( len ) ) 
				{
					if ( conn->req.ranges.size() > 1 )
					{
						state = webServerResponseMemoryRangeMimeHeaders;
					} else
					{
						state = webServerResponseMemoryRangeBody;
					}
				}
				return wsSendResponse;
			case webServerResponseMemoryRangeMimeHeaders:
				bytesSent += len;
				if ( txBuffList.consume ( len ) ) 
				{
					headers.erase();
					headers.printf( "\r\n--%s\r\n", conn->req.multiPartBoundary );
					headers.printf( "Content-Type: %s\r\n", conn->req.mimeType->contentType.c_str() );
					headers.printf( "Content-Range: bytes %I64u-%I64u/%I64u\r\n\r\n", rangeIt->rangeLow, rangeIt->rangeHigh, body.entry->getDataLen() );

					state = webServerResponseMemoryRangeBody;
				}
				return wsSendResponse;
			case webServerResponseMemoryRangeBody:
				bytesSent += len;
				if ( txBuffList.consume ( len ) ) 
				{
					rangeIt++;
					if ( rangeIt == conn->req.ranges.end() )
					{
						headers.erase();
						headers.printf( "\r\n--%s--\r\n", conn->req.multiPartBoundary );
						state = webServerResponseMemoryRangeEnd;
					}
					state = webServerResponseMemoryRangeMimeHeaders;
				}
				return wsSendResponse;
			case webServerResponseMemoryRangeEnd:
				body.release();
				if ( keepAlive )
				{
					return wsParseURL;
				} else return wsReset;
				// streamed response non-range
			default:
				throw 500;
		}
	}
}

webBuffs	*webServerResponse::getBuffer ( void )
{
	if ( txBuffList.totalIo )
	{
		return &txBuffList;
	}
	txBuffList.reset();

nextState:

	switch ( state )
	{
		case webServerResponseStart:
			if ( conn->rsp.body.bodyType == webServerResponse::body::btChunked )
			{
				state = webServerResponseChunkedHeaders;
			} else
			{
				if ( conn->req.ranges.size ( ) )
				{
					rangeIt = conn->req.ranges.begin ( );
					if ( conn->req.ranges.size ( ) == 1 )
					{
						state = webserverResponseMemoryRangeSingle;
					} else
					{
						state = webServerResponseMemoryRangeHeaders;
					}
				} else
				{
					state = webServerResponseMemory;
				}
			}
			goto nextState;

		case webServerResponseChunkedHeaders:
			txBuffList.addBuff ( headers.getBuffer ( ), headers.getSize ( ) - 2 );	// chop off trailing \r\n   we'll add it on when we do the first chunk length
			break;

		case webServerResponseChunkedLength:
			if ( (size_t) body )
			{
				auto len = sprintf_s ( (char *) body.chunkedLength, sizeof ( body.chunkedLength ), "\r\n%I64X\r\n", (size_t) body );
				txBuffList.addBuff ( body.chunkedLength, len );
			} else
			{
				// TODO: add support for chunk trailers
				txBuffList.addBuff ( (uint8_t *)"\r\n0\r\n\r\n", 7 );		// terminating 0-length chunk and no trailers
			}
			break;

		case webServerResponseChunkedBody:
			txBuffList.addBuff ( body, body );		// add the first chunk
			break;

		case webServerResponseMemory:
			// both headers and body at the same time...
			txBuffList.addBuff ( headers.getBuffer(), headers.getSize() );
			txBuffList.addBuff ( body, body );
			break;

		case webserverResponseMemoryRangeSingle:
			{
				auto endRange = (*rangeIt).rangeHigh;
				auto startRange = (*rangeIt).rangeLow;

				if ( endRange > (size_t)body - 1 )
				{
					endRange = (size_t)body - 1;
				}
				if ( startRange < 0 )
				{
					/* start from the end */

					startRange = (endRange - 1) - startRange;
					if ( startRange < 0 )
					{
						startRange = 0;
					}
				} else if ( startRange > ( size_t )body - 1 )
				{
					startRange = (size_t) body - 1;
				}

				// headers
				txBuffList.addBuff( headers.getBuffer(), headers.getSize() );
				// range body
				txBuffList.addBuff( (uint8_t*)body + startRange, endRange - startRange + 1 );
			}
			break;

		case webServerResponseMemoryRangeHeaders:
		case webServerResponseMemoryRangeMimeHeaders:
			// send the headers
			txBuffList.addBuff( headers.getBuffer(), headers.getSize() );
			break;

		case webServerResponseMemoryRangeBody:
			{
				auto endRange = (*rangeIt).rangeHigh;
				auto startRange = (*rangeIt).rangeLow;

				while ( rangeIt != conn->req.ranges.end() )
				{
					if ( endRange > (size_t) - 1 )
					{
						endRange = (size_t) body - 1;
					}
					if ( startRange < 0 )
					{
						/* start from the end */
						startRange = (endRange - 1) - startRange;
						if ( startRange < 0 )
						{
							startRange = 0;
						}
					}
					if ( startRange < (size_t) body - 1 )
					{
						txBuffList.addBuff( (uint8_t*)body + startRange, endRange - startRange + 1 );
						break;
					}
					rangeIt++;
				}
			}
			break;

		default:
			txBuffList.addBuff ( body, body );
			break;
	}
	return &txBuffList;
}

serverIOType	webServerResponse::getIoOp( void )
{
	return needSend;
}

webBuffs	*webServerResponse::getBufferLog ( void )
{
	if ( txBuffList.totalIo ) return &txBuffList;
	txBuffList.reset();
	txBuffList.addBuff ( body, body );
	return &txBuffList;
}
