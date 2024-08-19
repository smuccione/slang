
#include "webServer.h"
#include "webSocket.h"
#include "webProtoH1.h"
#include "webServerGlobalTables.h"
#include "Utility/funcky.h"


struct webServerFlags { 
	char const			*name;
	size_t				 nameLen;
	size_t				 value;
};


static const webServerFlags encodingType[] =	{	{ "gzip",			4, compGzip },
													{ "deflate",		7, compDeflate },
													{ "compress",		8, compCompress },
													{ 0, 0, 0 }
												};

static const webServerFlags connectionType[] =	{	{ "Keep-Alive",		10, 1},
													{ "close",			5,  0},
													{ 0, 0, 0 }
												};

static const webServerFlags teType[] =	{	{ "chunked",		7, 1},
											{ 0, 0, 0 }
										};



static __forceinline size_t webServerParseFlags ( uint8_t *ptr, uint8_t *end, const webServerFlags *flagValues )
{
	size_t			 flagValue;
	uint32_t		 loop;

	flagValue = 0;

	while ( (*ptr == ' ') && (ptr < end) ) 
	{
		ptr++;
	}

	while ( (ptr < end) )
	{
		for ( loop = 0; flagValues[loop].name; loop++ )
		{
			if ( !memcmpi ( ptr, flagValues[loop].name, flagValues[loop].nameLen) )
			{
				/* found a keyword */
				ptr += flagValues[loop].nameLen;

				/* or in the flag bit */
				flagValue |= flagValues[loop].value;

				break;
			}
		}
		if ( !flagValues[loop].name )
		{
			while ( (ptr < end) && (ptr[0] != ',') )
			{
				ptr++;
			}
		}
		/* skip to end of line or end of flag */
		while ( (ptr < end) && ((ptr[0] ==' ') || (ptr[0] == ',')) )
		{
			ptr++;
		}
	}
	return flagValue;
}

size_t webServerRequest::parseHeaders ( uint8_t **basePtr, uint8_t *ptr, uint8_t *buffEnd )
{
	size_t									 loop;
	uint8_t									*parseStart;
	uint8_t									*valueStart;

	parseStart = ptr;

	if ( ptr + 4 > buffEnd )
	{
		return 0;
	}

	while ( ptr < buffEnd )
	{
		if ( ptr[0] == ':' )
		{
			ptr++;
			break;
		}
		if ( (ptr[0] == '\r') || (ptr[0] == '\n') ) throw 400;
		ptr++;
	}
	if ( ptr == buffEnd )
	{
		needMoreInput = true;
		return 0;
	}

	auto mapIt = httpHeaderMap.find ( statString ( (char *)parseStart, ptr - parseStart ) );

	if ( mapIt != httpHeaderMap.end() )
	{
		loop = mapIt->second;
		while ( ptr < buffEnd && (ptr[0] == ' ') ) ptr++;

		// start of our value
		valueStart = ptr;
		while ( ((ptr + 1) < buffEnd) && (ptr[0] != '\r') && (ptr[1] != '\n') )
		{
			ptr++;
		}

		if ( ptr >= buffEnd )
		{
			// didn't find a \r\n
			return 0;
		}

		headers.push_back ( webServerHeader ( basePtr,  httpHeaderTable[loop].name, httpHeaderTable[loop].nameLen - 1, valueStart,(uint32_t)(ptr - valueStart) ) );
		switch ( loop )
		{
			case HTTP_contentLength:
				contentLength = _atoi64 ( (char *)valueStart );
				contentLengthValid = true;
				break;
			case HTTP_acceptEncoding:
				compress = webServerParseFlags ( valueStart, ptr, encodingType );
				break;
			case HTTP_keepAlive:
			case HTTP_connection:
				keepAlive = webServerParseFlags ( valueStart, ptr, connectionType ) == 1;
				break;
			case HTTP_cookie:
				uint8_t *tmpPtr;
				uint8_t *cookieValue;
				tmpPtr = valueStart;
				cookieValue = 0;
				while ( tmpPtr < ptr )
				{
					if ( !cookieValue && (tmpPtr[0] == '=') )
					{
						tmpPtr++;
						cookieValue = tmpPtr;
					}
					if ( tmpPtr[0] == ';' )
					{
						// NOTE: we differ any decoding until we actaully need the cookie... for static pages we'll never decode so it just saves us a lot of work
						//headers.push_back( webServerHeader( basePtr, "CookieValue:", 12, valueStart, (uint32_t)(tmpPtr - valueStart) ) );
						if ( cookieValue )
						{
							cookies.push_back( webServerRequestCookie( basePtr, valueStart, cookieValue - valueStart - 1, cookieValue, tmpPtr - cookieValue ) );
						} else
						{
							cookies.push_back( webServerRequestCookie( basePtr, valueStart, cookieValue - 1 - valueStart, 0, 0 ) );
						}
						cookieValue = 0;
						tmpPtr++;
						while ( tmpPtr[0] == ' ' ) tmpPtr++;
						valueStart = tmpPtr;
					} else
					{
						tmpPtr++;
					}
				}
				// NOTE: we differ any decoding until we actaully need the cookie... for static pages we'll never decode so it just saves us a lot of work
				//headers.push_back( webServerHeader( basePtr, "CookieValue:", 12, valueStart, (uint32_t)(tmpPtr - valueStart) ) );
				if ( cookieValue )
				{
					cookies.push_back ( webServerRequestCookie ( basePtr, valueStart, cookieValue - valueStart - 1, cookieValue, tmpPtr - cookieValue ) );
				} else
				{
					cookies.push_back ( webServerRequestCookie ( basePtr, valueStart, cookieValue - 1 - valueStart,  0, 0 ) );
				}
				break;
			case HTTP_contentType:
				contentType = headers.size() - 1;

				if ( !memcmpi( valueStart, "multipart/", 10 ) )
				{
					while ( valueStart < buffEnd )
					{
						while ( valueStart < buffEnd )
						{
							if ( valueStart[0] == ';' ) 
							{
								valueStart++;
								break;
							}
							valueStart++;
						}
						while ( valueStart < buffEnd && (valueStart[0] == ' ') ) valueStart++;
						if ( !memcmpi ( valueStart, "boundary=", 9 ) )
						{
							uint8_t *boundary;

							valueStart += 9;
							boundary = valueStart;
							while ( (boundary < buffEnd) && (boundary[0] != ';') && (boundary[0] != '\r') )
							{
								boundary++;
							}
							headers.push_back ( webServerHeader ( basePtr, "multi-part-boundary", 19, valueStart, (uint32_t)(boundary - valueStart) ) );
							multiPartBoundary = headers.size() - 1;
						}
					}
					multiPartState		= wsParseBodyMultipartMemoryStart;
					multiPartStart		= 0;
					multiPartScanOffset = 0;
				}
				break;
			case HTTP_transferEncoding:
				chunked = webServerParseFlags ( valueStart, ptr, teType ) == 1;

				if ( chunked )
				{
					chunkBuff = server->getChunkBuff();
					chunkBuff->teState = webChunkedRxBuff::wsParseBodyChunkedMemoryLen;
					chunkBuff->chunkRcvdLength = 0;
				} else throw 400;
				break;
			case HTTP_range:
				range = headers.size() -1;
				size_t	rangeStart;
				size_t	rangeEnd;
				if ( !memcmpi ( (char *)valueStart, "bytes=", 6 ) )
				{
					valueStart += 6;
				} else throw 400;
				for ( ;; )
				{
					if ( valueStart >= ptr ) throw 400;
					rangeStart = _atoi64 ( (char *)valueStart );
					while ( (valueStart < ptr) && (_isdigit ( valueStart )) )
					{
						valueStart++;
					}
					if ( valueStart[0] != '-' ) throw 400;
					valueStart++;
					if ( _isdigit ( valueStart ) )
					{
						rangeEnd = _atoi64 ( (char *)valueStart );
						while ( (valueStart < ptr) && (_isdigit ( valueStart )) )
						{
							valueStart++;
						}
					} else rangeEnd = ULLONG_MAX;
					ranges.push_back ( webServerRange ( rangeStart, rangeEnd ) );
					if ( valueStart[0] != ',' ) break;
				}
				if ( valueStart != ptr ) throw 400;
				break;
			case HTTP_expect:
				expect = headers.size() - 1;
				break;
			case HTTP_ifModifiedSince:
				ifModifiedSince = headers.size() -1;
				break;
			case HTTP_ifMatch:
				ifMatch= headers.size() -1;
				break;
			case HTTP_ifNoneMatch:
				ifNoneMatch= headers.size() -1;
				break;
			case HTTP_ifRange:
				ifRange = headers.size() -1;
				break;
			case HTTP_ifUnmodifiedSince:
				ifUnmodifiedSince = headers.size() -1;
				break;
			case HTTP_authorization:
				authorization = headers.size() -1;
				break;
			case HTTP_from:
				from = headers.size() - 1;
				break;
			case HTTP_xForward_for:
			case HTTP_xForwarded_for:
				this->xForwardFor = headers.size() - 1;
				break;
			case HTTP_host:
				host = headers.size() -1;
				for ( uint32_t index = 0; index < headers[host].valueLen; index++ )
				{
					if ( ((char const *) (headers[host]))[index] == ':' )
					{
						headers[host].valueLen = index;
						break;
					}
				}
				virtualServer = server->getVirtualServer ( (uint8_t *)(char const *)(headers[host]), headers[host].valueLen );
				break;
		}
		ptr += 2;	// \r\n
	} else
	{
		uint8_t	*nameEnd = ptr;

		while ( (ptr + 1 < buffEnd) && (ptr[0] == ' ') ) ptr++;
		valueStart = ptr;

		while ( ptr + 1 < buffEnd )
		{
			if ( (ptr[0] == '\r') && (ptr[1] == '\n') )
			{
				if ( nameEnd )
				{
					headers.push_back ( webServerHeader ( basePtr, parseStart - *basePtr, (uint32_t)(nameEnd - parseStart - 1), valueStart, (uint32_t)(ptr - valueStart) ) );
				}
				return (ptr + 2) - parseStart;
			}
			ptr++;
		}
		needMoreInput = true;
		return 0;
	}
	// we were able to consume the header
	return ptr - parseStart;
}

size_t webServerRequest::parseHeaders ( std::vector<webServerHeader> *headers, uint8_t **basePtr, uint8_t *ptr, uint8_t *buffEnd )
{
	uint8_t									*parseStart;
	uint8_t									*valueStart;

	parseStart = ptr;

	if ( ptr + 4 > buffEnd )
	{
		return 0;
	}

	while ( ptr < buffEnd )
	{
		if ( ptr[0] == ':' )
		{
			ptr++;
			break;
		}
		if ( (ptr[0] == '\r') || (ptr[0] == '\n') ) 
			throw 400;
		ptr++;
	}
	if ( ptr == buffEnd )
	{
		needMoreInput = true;
		return 0;
	}

	uint8_t	*nameEnd = ptr;

	while ( (ptr + 1 < buffEnd) && (ptr[0] == ' ') ) ptr++;
	valueStart = ptr;

	while ( ptr + 1 < buffEnd )
	{
		if ( (ptr[0] == '\r') && (ptr[1] == '\n') )
		{
			if ( nameEnd )
			{
				headers->push_back ( webServerHeader ( basePtr, (char *)parseStart, (uint32_t)(nameEnd - parseStart), valueStart, (uint32_t)(ptr - valueStart) ) );
			}
			return (ptr + 2) - parseStart;
		}
		ptr++;
	}
	needMoreInput = true;
	return 0;
}

webServerPageState webServerRequest::parseRequest ( webServerPageState state )
{
	uint8_t										*startUrl;
	uint8_t										*startName;
	uint8_t										*startValue;
	uint8_t										*headerEnd;
	uint8_t										*startQuery;

	switch ( state )
	{
		case wsParseURL:
			headerEnd = headerBuffer + headerBufferLen;

			// first stage, pointer is set to head,
			headerBufferPointer = headerBuffer;
			shortUrl	= 0;
			method		= nullptr;

			startName = headerBufferPointer;
			while ( (headerBufferPointer[0] != ' ') && (headerBufferPointer < headerEnd)  ) headerBufferPointer++;
			headers.push_back ( methodHeader =  webServerHeader ( &headerBuffer, "Method", 6, startName, (uint32_t)(headerBufferPointer - startName + 1) ) );		// include trailing space
			while ( (headerBufferPointer[0] == ' ') && (headerBufferPointer < headerEnd)  ) headerBufferPointer++;

			if ( headerBufferLen < 10 )
			{
				needMoreInput = true;
				return state;
			}

			startValue = 0;
			startName = 0;
			startQuery = 0;
			startUrl = headerBufferPointer;

			while ( headerBufferPointer < headerEnd )
			{
				if ( (*headerBufferPointer == '?') && !shortUrl )
				{
					headers.push_back ( webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, (uint32_t)(headerBufferPointer - startUrl)) );
					shortUrl = headers.size() - 1;
					headerBufferPointer++;
					startName = headerBufferPointer;
					startQuery = headerBufferPointer;
					multiParts.push_back ( webServerMultipart () );
					multiParts.back().dataLen	 = 0;
					multiParts.back().dataOffset = 0;
				}
				if ( *headerBufferPointer == '=' )
				{
					headerBufferPointer++;
					startValue = headerBufferPointer;
				}
				if ( *headerBufferPointer == '&' )
				{
					if ( !startName || !shortUrl )
					{
						headers.push_back ( webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, (uint32_t) (headerBufferPointer - startUrl) ) );
						shortUrl = headers.size() - 1;
						headers.push_back ( webServerHeader ( &headerBuffer, "URL", 3, startUrl, (uint32_t) (headerBufferPointer - startUrl) ) );
						url = headers.size() - 1;
						headers.push_back ( webServerHeader ( &headerBuffer, "FIRST_LINE", 10, startUrl, (uint32_t) (headerBufferPointer - startUrl) ) );
						firstLine = headers.size() - 1;
						throw 400;
					}
					if ( startValue )
					{
						multiParts.back().vars.push_back ( webServerRequestVar ( &headerBuffer, startName, startValue - 1 - startName, startValue, headerBufferPointer - startValue ) );
					} else
					{
						multiParts.back().vars.push_back ( webServerRequestVar ( &headerBuffer, startName, startValue - 1 - startName, startName, 0 ) );
					}
					startValue = 0;
					headerBufferPointer++;
					startName = headerBufferPointer;
				}
				if ( *headerBufferPointer == ' ' )
				{
					// done with URL... if shortURL is present then we've already stored the short URL and extracted out the params... otherwise we need to store it now
					if ( !shortUrl )
					{
						headers.push_back ( webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, (uint32_t)(headerBufferPointer - startUrl)) );
						shortUrl = headers.size() - 1;
					}	
					break;
				}
				headerBufferPointer++;
			}
			if ( startName )
			{
				if ( !shortUrl )
				{
					headers.push_back ( webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, (uint32_t) (headerBufferPointer - startUrl) ) );
					shortUrl = headers.size() - 1;
					headers.push_back ( webServerHeader ( &headerBuffer, "URL", 3, startUrl, (uint32_t) (headerBufferPointer - startUrl) ) );
					url = headers.size() - 1;
					headers.push_back ( webServerHeader ( &headerBuffer, "FIRST_LINE", 10, startUrl, (uint32_t) (headerBufferPointer - startUrl) ) );
					firstLine = headers.size() - 1;
					throw 400;
				}
				if ( startValue && (startValue != headerEnd) )
				{
					multiParts.back().vars.push_back ( webServerRequestVar ( &headerBuffer, startName, (uint32_t)(startValue - 1 - startName), startValue, (uint32_t)(headerBufferPointer - startValue) ) );
				} else
				{
					multiParts.back().vars.push_back ( webServerRequestVar ( &headerBuffer, startName, (uint32_t)(startValue - 1 - startName), startName, 0 ) );
				}
			}
			if ( startQuery )
			{
				headers.push_back ( webServerHeader ( &headerBuffer, "QUERY", 3, startQuery, (uint32_t)(headerBufferPointer - startQuery) ) );
				query = headers.size() - 1;
			}
			headers.push_back ( webServerHeader ( &headerBuffer, "URL", 3, startUrl, (uint32_t)(headerBufferPointer - startUrl) ) );
			url = headers.size() - 1;

			headerBufferPointer++;	// skip over white space
			if ( memcmpi ( headerBufferPointer, "HTTP/", 5 ) != 0 )
			{
				headers.push_back ( webServerHeader ( &headerBuffer, "FIRST_LINE", 10, headerBuffer, (uint32_t) (headerBufferPointer - headerBuffer) ) );
				firstLine = headers.size() - 1;
				throw 400;
			}
			headerBufferPointer += 5;

			memcpy ( version, headerBufferPointer, 3 );
			headerBufferPointer += 3;

			if ( memcmp ( version, "1.1", 3 ) != 0)
			{
				keepAlive = false;
			}

			if ( memcmp ( headerBufferPointer, "\r\n", 2 ) != 0 ) throw 405;

			headers.push_back ( webServerHeader ( &headerBuffer, "FIRST_LINE", 10, headerBuffer, (uint32_t)(headerBufferPointer - headerBuffer) ) );
			firstLine = headers.size() - 1;

			headerBufferPointer+=2;

			state = wsParseHeaders;
			break;
		case wsParseHeaders:
			{
				while ( headerBufferPointer[0] != '\r' && headerBufferPointer[1] != '\n' )
				{
					size_t	consumed;

					try
					{
						consumed = parseHeaders ( &headerBuffer, headerBufferPointer, headerBuffer + headerBufferLen );
					} catch ( ... )
					{
						needMoreInput = false;
						state = wsReset;
						break;
					}
					headerBufferPointer += consumed;
					if ( !consumed )
					{
						needMoreInput = true;
						return state;
					}
				}
				headerBufferPointer += 2;

				if ( !virtualServer )
				{
					virtualServer = server->getVirtualServer ( (uint8_t *) "", 0 );
				}

				auto it = virtualServer->methodHandlers.find ( statString ( methodHeader, methodHeader ) );

				if ( it == virtualServer->methodHandlers.end ( ) )
				{
					throw 405;		// method not allowed ( no handler registered for method )
				} else
				{
					method = &((*it).second);
				}

#ifdef _DEBUG
				if ( server->printRequestHeaders )
				{
					printf ( "-------------------------------REQUEST---------------------------------------\r\n" );
					printf ( "%.*s", (int) (headerBufferPointer - headerBuffer), headerBuffer );
					printf ( "-------------------------------REQUEST---------------------------------------\r\n" );
				}
#endif
			// done parsing headers... now what do we do?
				state = wsReceiveBody;
			}
			break;
		default:
			throw 500;
	}
	return state;
}

webServerPageState webServerRequest::processBuffer ( size_t len )
{
	uint8_t	*tmpC;

	bytesRcvd += len;
	needMoreInput = false;
	while ( !needMoreInput )
	{
		switch ( state )
		{
			case wsParseURL:
				QueryPerformanceCounter ( &reqStartTime );
				[[fallthrough]];
			case wsParseHeaders:
				headerBufferLen += (uint32_t)len;
				len = 0;

				state = parseRequest ( state );

				if ( state == wsReceiveBody )
				{
					if ( headerBuffer + headerBufferLen > headerBufferPointer )
					{
						// we have unused rx bytes in the header buffer... these need to placed into the body
						if ( chunked )
						{
							// put them into the chunked assembly buffer
							memcpy ( headerBufferPointer, chunkBuff->chunkedAssemblyBuffer, headerBufferLen - (headerBufferPointer - headerBuffer) );
							chunkBuff->chunkRcvdLength = headerBufferLen - (headerBufferPointer - headerBuffer);
						} else
						{
							// put them directly into the body buffer
							rxBuffer.writeFront ( headerBufferPointer, headerBufferLen - (headerBufferPointer - headerBuffer) );
						}
					}
				}
				break;
			case wsReceiveBody:
				// ok, we either receive via contentLength or chunked transfers
				// TODO: close-socket based reception

				if ( chunked )
				{
					chunkBuff->chunkRcvdLength += len;
					len = 0;
					switch ( chunkBuff->teState )
					{
						case webChunkedRxBuff::wsParseBodyChunkedMemoryLen:
							needMoreInput = true;
							tmpC = chunkBuff->chunkedAssemblyBuffer;
							while ( tmpC < chunkBuff->chunkedAssemblyBuffer + chunkBuff->chunkRcvdLength )
							{
								if ( (tmpC[0] == '\r') && (tmpC[1] == '\n') )
								{
									// next chunk length
									size_t	tmpLarge;
									tmpLarge = _strtoui64( (char *)chunkBuff->chunkedAssemblyBuffer, 0, 10 );

									if ( tmpLarge & 0xFFFFFFFF00000000 ) throw 413; // request to large
									chunkBuff->chunkRcvdLength = (uint32_t) tmpLarge;

									tmpC += 2;
									// remove the length from the assembly buffer
									memmove ( chunkBuff->chunkedAssemblyBuffer, tmpC, tmpC - (chunkBuff->chunkedAssemblyBuffer + chunkBuff->chunkRcvdLength ) );

									if ( !chunkBuff->chunkLength )
									{
										state = wsParseBody;
										needMoreInput = false;
									}
									break;
								}
							}
							break;
						case webChunkedRxBuff::wsParseBodyChunkedMemoryContent:
							if ( chunkBuff->chunkLength < chunkBuff->chunkRcvdLength )
							{
								rxBuffer.write ( chunkBuff->chunkedAssemblyBuffer, chunkBuff->chunkLength );
								memmove ( chunkBuff->chunkedAssemblyBuffer, chunkBuff->chunkedAssemblyBuffer + chunkBuff->chunkLength, chunkBuff->chunkRcvdLength - chunkBuff->chunkLength );
								chunkBuff->teState = webChunkedRxBuff::wsParseBodyChunkedMemoryLen;
							}
							break;
					}
				} else
				{
					if ( contentLengthValid )
					{
						rxBuffer.assume( len );
						if ( rxBuffer.getSize() < contentLength )
						{
							needMoreInput = true;
						} else
						{
							state = wsParseBody;
						}
					} else
					{
						return state = wsProcess;
					}
				}
				break;
			case wsParseBody:
				if ( multiPartBoundary )
				{
					uint8_t	*buffEnd;

					do {
						switch ( multiPartState )
						{
							case wsParseBodyMultipartMemoryStart:
								if ( rxBuffer.getSize() >= headers[multiPartBoundary].valueLen + 2 )
								{
									tmpC = rxBuffer.getBuffer();
									if ( (tmpC[0] == '-') && (tmpC[1] == '-') && (tmpC[headers[multiPartBoundary].valueLen + 2] == '\r') && (tmpC[headers[multiPartBoundary].valueLen + 3] == '\n')  && !memcmp( rxBuffer.getBuffer() + 2, headers[multiPartBoundary], headers[multiPartBoundary].valueLen ) )
									{
										multiPartScanOffset =  headers[multiPartBoundary].valueLen + 4;
										multiParts.push_back ( webServerMultipart () );
										multiPartState = wsParseBodyMultipartMemoryHeaders;
										break;
									} else
									{
										printf ( "ERROR: invalid multipart body - webServerRequest::processBuffer\r\n" );
										throw 405;
									}
								} else
								{
									printf ( "ERROR: invalid multipart body - webServerRequest::processBuffer\r\n" );
									throw 400;
								}
								break;
							case wsParseBodyMultipartMemoryHeaders:
								tmpC = rxBuffer.getBuffer() + multiPartScanOffset;				
								while ( tmpC[0] != '\r' && tmpC[1] != '\n' )
								{
									uint8_t *ptr;
									size_t	consumed;

									ptr = tmpC;
									try
									{
										consumed = parseHeaders ( &multiParts[multiParts.size ( ) - 1].headers, rxBuffer.getBufferDataPtr ( ), tmpC, rxBuffer.getBuffer ( ) + rxBuffer.getSize ( ) );
									} catch ( ... )
									{
										needMoreInput = false;
										state = wsReset;
										break;
									}
									tmpC += consumed;
									if ( !consumed )
									{
										printf ( "ERROR: invalid multipart body - webServerRequest::processBuffer\r\n" );
										throw 400;
									} else
									{
										if (	!memcmpi ( multiParts.back().headers[multiParts.back().headers.size() - 1].getName(), "Content-Disposition:", multiParts.back().headers[multiParts.back().headers.size() - 1].nameLen ) ||
												!memcmpi ( multiParts.back().headers[multiParts.back().headers.size() - 1].getName(), "X-Form-Var:", multiParts.back().headers[multiParts.back().headers.size() - 1].nameLen )		)
										{
											uint8_t *startName;
											uint8_t *endName;
											uint8_t *startValue;
											uint8_t *endValue;

											startName = 0;
											startValue = 0;
											ptr += multiParts.back().headers[multiParts.back().headers.size() - 1].nameLen;

											while ( (ptr[0] == ' ') && ptr < tmpC ) ptr++;

											startName = endName = ptr;
											endValue = 0;
											while ( ptr < tmpC )
											{
												if ( ptr[0] == '=' )
												{
													endName = ptr;
													ptr++;
													while ( ptr[0] == ' ' ) ptr++;
													startValue = ptr;

													if ( ptr[0] == '\"' )
													{
														ptr++;
														startValue = ptr;
														while ( (ptr < tmpC ) && ptr[0] != '\"' )
														{
															ptr++;
														}
														if ( ptr[0] == '\"' )
														{
															endValue = ptr;
															ptr++;
														} else
														{
															printf ( "ERROR: invalid multipart body - webServerRequest::processBuffer\r\n" );
															throw 400;
														}
													}
												}
												if ( ptr[0] == ';' )
												{
													if ( startValue && (ptr - startValue) )
													{
														multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, endName - startName, startValue, endValue ? endValue - startValue :  ptr - startValue) );
													} else
													{
														multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, ptr - startName, startName, 0 ) );
													}
													startValue = 0;
													ptr++;
													while ( (ptr[0] == ' ') && (ptr < tmpC)  ) ptr++;
													startName = ptr;
												} else
												{
													ptr++;
												}
											}
											// last var on line
											if ( startName < tmpC )
											{
												if ( startValue && (startValue != tmpC) )
												{
													multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, endName - startName, startValue, endValue ? endValue - startValue : ptr - 1 - startValue ) );
												} else
												{
													multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, ptr - 1 - startName, startName, 0 ) );
												}
											}
										}
									}
								}
								multiPartScanOffset = (tmpC + 2) - rxBuffer.getBuffer();
								multiPartStart = multiPartScanOffset;
								multiPartState = wsParseBodyMultipartMemoryContent;
								break;
							case wsParseBodyMultipartMemoryContent:
								buffEnd = rxBuffer.getBuffer() + rxBuffer.getSize();
								buffEnd -= 2 + headers[multiPartBoundary].valueLen + 2;				// make sure we always have enough for the boundary and \r\n

								tmpC = rxBuffer.getBuffer() + multiPartScanOffset;
								while ( tmpC < buffEnd)
								{
									if ( *(uint32_t *)tmpC == 0x2d2d0a0d )
									{
										if ( (tmpC[headers[multiPartBoundary].valueLen + 4] == '\r') && (tmpC[headers[multiPartBoundary].valueLen + 5] == '\n')  )
										{
											if ( ! memcmp( &tmpC[4], headers[multiPartBoundary], headers[multiPartBoundary].valueLen ) )
											{
												multiParts.back().dataLen		=(tmpC - rxBuffer.getBuffer()) - multiPartStart;
												multiParts.back().dataOffset	= multiPartStart;
												multiParts.push_back ( webServerMultipart () );
												tmpC += headers[multiPartBoundary].valueLen + 6;
												multiPartScanOffset = tmpC - rxBuffer.getBuffer();
												multiPartState = wsParseBodyMultipartMemoryHeaders;
												break;
											}
										} else if ( *(uint32_t *)&tmpC[headers[multiPartBoundary].valueLen + 4] == 0x0a0d2d2d  )
										{
											if ( ! memcmp( &tmpC[4], headers[multiPartBoundary], headers[multiPartBoundary].valueLen ) )
											{
												multiParts.back().dataLen		= (tmpC - rxBuffer.getBuffer()) - multiPartStart;
												multiParts.back().dataOffset	= multiPartStart;
												return state = wsProcess;
											}
										}
									} 
									tmpC++;
								}
								if ( tmpC >= buffEnd )
								{
									// if we readhed here it's because we didn't encounter the terminating multipart border
									throw 400;
								}
								break;
						}
					} while ( state == wsParseBody );
				} else 
				{
					if ( contentType && !headers[contentType].cmpValuei ( "application/x-www-form-urlencoded" ) )
					{
						multiPartState = wsParseBodyMultipartMemoryHeaders;
						multiParts.push_back ( webServerMultipart () );
						multiParts.back().dataLen		= rxBuffer.getSize();
						multiParts.back().dataOffset	= 0;

						uint8_t *startName;
						uint8_t *startValue;
						uint8_t	*buffEnd;
						uint8_t *ptr;

						buffEnd = rxBuffer.getBuffer() + rxBuffer.getSize();
						ptr = rxBuffer.getBuffer();
						startValue = 0;
						startName = ptr;
						while ( ptr < buffEnd )
						{
							if ( ptr[0] == '=' )
							{
								ptr++;
								while ( ptr[0] == ' ' ) ptr++;
								startValue = ptr;
							}
							if ( ptr[0] == '&' )
							{
								if ( startValue && (ptr - startValue) )
								{
									multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, startValue - 1 - startName, startValue, ptr - startValue) );
								} else
								{
									multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, ptr - 1 - startName, startName, 0 ) );
								}
								startValue = 0;
								ptr++;
								while ( (ptr[0] == ' ') && (ptr < buffEnd) ) ptr++;
								startName = ptr;
							} else
							{
								ptr++;
							}
						}
						// last var on line
						if ( startName < buffEnd )
						{
							if ( startValue && (startValue != buffEnd ))
							{
								multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, startValue - 1 - startName, startValue, ptr - startValue ) );
							} else
							{
								multiParts.back().vars.push_back ( webServerRequestVar ( rxBuffer.getBufferDataPtr(), startName, ptr - 1 - startName, startName, 0 ) );
							}
						}

					} else if ( rxBuffer.getSize() )
					{
						multiParts.push_back ( webServerMultipart ( ) );
						multiParts[multiParts.size ( ) - 1].dataLen = rxBuffer.getSize ( );
						multiParts[multiParts.size ( ) - 1].dataOffset = 0;
					}
					return state = wsProcess;
				}
 				break;
			default:
				throw 500;
		}
	}
	return state;
}

webBuffs *webServerRequest::getBuffer ( void )
{
	rxBufferList.reset();
	switch ( state )
	{
		case wsParseURL:
		case wsParseHeaders:
			rxBufferList.addBuff ( headerBuffer + headerBufferLen, headerBufferSize - headerBufferLen );
			return &rxBufferList;
		case wsReceiveBody:
			if ( chunked )
			{
				rxBufferList.addBuff ( chunkBuff->chunkedAssemblyBuffer, chunkBuff->chunkedAssemblyBufferSize - chunkBuff->chunkRcvdLength );
				return &rxBufferList;
			} else 
			{
				if ( contentLengthValid )
				{
					rxBuffer.makeRoom( contentLength - rxBuffer.getSize() );
				} else
				{
					throw 400;
				}
				rxBufferList.addBuff ( rxBuffer.getBuffer() + rxBuffer.getSize(), rxBuffer.getFree() );
				return &rxBufferList;
			}
			break;
		case wsProcess:
		default:
			throw 500;
	}
}

void webServerRequest::clear ( void )
{
	state = wsParseURL;
	needMoreInput = true;

	headerBufferLen = 0;
	headerBufferPointer = headerBuffer;

	headers.clear ();
	cookies.clear ();
	multiParts.clear ();
	ranges.clear ();

	bytesRcvd = 0;

	rxBuffer.reset();

	if ( virtualServer ) server->freeVirtualServer ( virtualServer );
	virtualServer = 0;

	if ( chunkBuff ) server->freeChunkBuff( chunkBuff );
	chunkBuff = 0;

	keepAlive = true;			// assume we're 1.1 unless otherwise detected
	contentLengthValid = false;
	compress = 0;
	streamHandle = 0;
	chunked = false;
	multiPartBoundary = 0;

	firstLine = 0;
	shortUrl = 0;
	url	= 0;
	host = 0;
	from = 0;
	accept = 0;
	ifModifiedSince = 0;
	ifMatch = 0;
	ifNoneMatch = 0;
	ifRange = 0;
	ifUnmodifiedSince = 0;
	origin = 0;
	authorization = 0;
	referer = 0;
	contentType = 0;
	expect = 0;
	range = 0;
	te = 0;
	query = 0;
	userAgent = 0;
	xForwardFor = 0;
}

webServerRequest::webServerRequest ( class webProtocolH1 *conn, size_t headerPages )
{
	headers.reserve( 32 );
	cookies.reserve( 16 );
	multiParts.reserve( 8 );
	headerBufferSize = headerPages * (size_t)vmPageSize;
	headerBuffer = (uint8_t *) VirtualAlloc ( 0, headerBufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );

	virtualServer = 0;
	chunkBuff = 0;

	reset();
	streamHandle		= 0;
}

webServerRequest::~webServerRequest()
{
	VirtualFree ( headerBuffer, 0, MEM_RELEASE );
	if ( chunkBuff )
	{
		delete chunkBuff;
	}
}
