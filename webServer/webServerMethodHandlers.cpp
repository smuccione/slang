
#include "webServer.h"
#include "webSocket.h"
#include "webProtoSSL.h"
#include "webProtoH1.h"
#include "webProtoH2.h"

#include "Utility/encodingTables.h"
#include "Target/fileCache.h"
#include "Target/vmTask.h"

#include "Utility/funcky.h"
#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcInterpreter/bcInterpreter.h"
#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "compilerPreprocessor\compilerPreprocessor.h"

#include "zlib.h"

// combines root + url file name, strips off extension, and guards against .. attacks	- dest MUST be MAX_PATH or bigger
// also does the url decoding (hex, +, etc.)
size_t serverBuildName ( std::string const &root, char const *name, char *dest )
{
	int 	 ctr;
	size_t	 destLen;
	char	*tmpP;
	char	*extStart;

	/* fix to use _fullpath */
	ctr = 0;

	memcpy_s ( dest, MAX_PATH - 1, root.c_str(), root.size() );

	tmpP = dest;
	dest	+= root.size();
	destLen  = root.size();

	if ( (name[0] == '\\') || (name[0] == '/') ) name++;

	extStart = 0;
	while ( name[0] )
	{
		if ( (name[0] == '/') || (name[0] == '\\') )
		{
			if ( destLen < MAX_PATH - 1 )
			{
				*(dest++) = '\\';			// ensure dos style path seperator
				name++;
				destLen++;
			}
			extStart = 0;
			while ( (name[0] == '/') || (name[0] == '\\') )
			{
				name++;
			}
			if ( (name[0] == '.') )
			{
				extStart = dest;
				if ( (name[0] == '.') )
				{
					if ( destLen < MAX_PATH - 1 )
					{
						*(dest++) = *(name++);
						destLen++;
					}
					extStart = 0;
					if ( ctr-- == 0 )
					{
						*tmpP = 0;
						return 0;
					}
				} 
			} else
			{
				ctr++;
			}
		} else if ( (name[0] == '%') && name[1] && name[2] )
		{
			// convert the ascii hex to a single character
			*(dest++) = static_cast<char>((hexEncodingTable[(size_t)(uint8_t)name[1]] << 4) + hexEncodingTable[(size_t)(uint8_t)name[2]]);
			name += 3;
		} else if ( name[0] == '+' )
		{
			name++;
			*(dest++) = ' ';
		} else
		{
			if ( (name[0] == '.') )
			{
				extStart = dest;
			}
			if ( destLen < MAX_PATH -1 )
			{
				*(dest++) = name[0];
				destLen++;
			} else
			{
				break;
			}
			name++;
		}
	}
	if ( extStart ) dest = extStart;
	*(dest++) = 0;

	return dest - tmpP - 1;
}

static void parseMimeExt ( char const *fname, char *ext )
{
	auto index = (int)strlen ( fname );
	auto extStart = -1;

	while ( index >= 0 )
	{
		if ( fname[index] == '.' )
		{
			extStart = index;
			break;
		}
		if ( fname[index] == '\\' || fname[index] == '/' )
		{
			break;
		}
		index--;
	}

	if ( extStart < 0 )
	{
		*ext = 0;
		return;
	}

	index = extStart;

	auto ctr = 0;
	while ( fname[index] )
	{
		if ( ctr >= (MAX_PATH - 1) )
		{
			break;
		}
		*(ext++) = fname[index];
		index++;
		ctr++;
	}
	*ext = 0;
}

bool webServerAuthenticate ( vmInstance *instance, webProtocolH1 *conn, webServerAuth *auth )
{
	switch ( auth->type )
	{
		case webServerAuth::authIndividual:
			if ( *auth == conn->req.headers[conn->req.authorization] )
			{
				return true;
			}
			break;
		case webServerAuth::authGroup:
			for ( auto grp : auth->group.groupList )
			{
				if ( webServerAuthenticate ( instance, conn, grp ) )
				{
					return true;
				}
			}
			break;
		case webServerAuth::authCallback:
			try {
				// call the function specified when registering the authorization i
				instance->run (  (char *)auth->callback.funcName.c_str() );

				switch ( TYPE ( &instance->result ) )
				{
					case slangType::eLONG:
						if ( instance->result.dat.l )
						{
							instance->om->processDestructors();
							instance->reset();
							return true;
						}
						break;
					default:
						break;
				}
				instance->om->processDestructors();
				instance->reset();
			} catch ( ... )
			{
				// ignore the error... by default we have to GIVE acceses, so if the slang function crash's we simply deny acceses by ignoring the fault
			}
			break;
	}
	return false;
}

static void webServerAuthenticate ( vmInstance *instance,  webProtocolH1 *conn, char const *fName, size_t fNameLen )
{
	while ( (fName[fNameLen - 1] != '\\') && fNameLen ) fNameLen--;
	if ( !fNameLen )
	{
		return;
	}

	pathString		path ( fName, fNameLen );
	
	auto it = conn->server->realmPathMap.lower_bound  ( path );

	if ( it == conn->server->realmPathMap.end() )
	{
		return;
	}

	// we have a directory mandating authorization... initially set as denied until we determine we have access
	conn->rsp.rspCode = 401;

	if ( !conn->req.authorization )
	{
		conn->rsp.body.release();
		conn->rsp.body.bodyType = webServerResponse::body::btStaticPointer;
		conn->rsp.body.cPtr = (uint8_t *)(*it).second->name.c_str();
		conn->rsp.body.cLen = (*it).second->name.length();
		return;
	}

	for ( auto &authIt : (*it).second->accessList )
	{
		if ( webServerAuthenticate (  instance, conn, authIt ) ) 
		{
			// we have authorization
			conn->rsp.rspCode = 200;
			return;
		}
	}
	conn->rsp.body.release();
	conn->rsp.body.bodyType = webServerResponse::body::btStaticPointer;
	conn->rsp.body.cPtr = (uint8_t *)(*it).second->name.c_str();
	conn->rsp.body.cLen = (*it).second->name.length();
}

void webTraceMethodHandler ( class vmInstance *instance, class webProtocolH1 *conn )
{
	tm			 utcTime;
	char		 date[32];
	time_t		 currentTime = time ( 0 );

	gmtime_s ( &utcTime, &currentTime );
		
	sprintf_s ( date, sizeof ( date ), 
					"%s, %02i %s %04i %02i:%02i:%02i GMT",
					aShortDays[utcTime.tm_wday],
					utcTime.tm_mday,
					aShortMonths[utcTime.tm_mon],
					utcTime.tm_year + 1900,
					utcTime.tm_hour,
					utcTime.tm_min,
					utcTime.tm_sec
				);

	conn->rsp.headers.write  ( "HTTP/1.1 304 Unmodified\r\n", 25 );
	conn->rsp.headers.write  ( "Server: Slang Server\r\n", 22 );
	conn->rsp.headers.printf ( "Content-Length: %d\r\n", conn->req.headerBufferLen );
	conn->rsp.headers.printf ( "Date: %s\r\n\r\n", date );
	conn->rsp.headers.write  ( conn->req.headerBuffer, conn->req.headerBufferLen );
}


void webHeadMethodHandler ( class vmInstance *instance, class webProtocolH1 *conn )
{
	webGetPostMethodHandler ( instance, conn );
	conn->rsp.body.release();
}

/*
	1.	find destination file
	2.	find mime type
	3.	call handler
*/

void webGetPostMethodHandler ( class vmInstance *instance, webProtocolH1 *conn )
{
	char													 ext[MAX_PATH];
	char													 fName[MAX_PATH];
	fileCacheEntry											*entry;

	if ( conn->rsp.rspCode == 200 )
	{
		try
		{
			conn->rsp.rspCode = 200;
			conn->rsp.keepAlive = conn->req.keepAlive;			// can only be made false from here on out... if false to start with then we close

			// build a null-terminated string from the short-url (stripped of parameters)
			memcpy ( fName, (char const *) conn->req.headers[conn->req.shortUrl], (size_t) conn->req.headers[conn->req.shortUrl] );
			fName[(size_t) conn->req.headers[conn->req.shortUrl]] = 0;

			// extract the file extension
			parseMimeExt ( fName, ext );

			// build our file name (without extension)
			conn->proc.fileNameBaseLen = serverBuildName ( conn->req.virtualServer->rootName, fName, conn->proc.fileNameBase );

			if ( conn->req.virtualServer->passiveSecurity )
			{
				webServerAuthenticate ( instance, conn, conn->proc.fileNameBase, conn->proc.fileNameBaseLen );

				switch ( conn->rsp.rspCode )
				{
					case 200:
						// ok.. we can continue;
						break;
					default:
						// non-200 return (either authentication failure or other).. in all cases, build headers and return
						conn->rsp.buildRspHeaders ( conn );
						return;
				}
			}

			// get the mime type
			char fileNameSearch[MAX_PATH];
			memcpy_s ( fileNameSearch, sizeof ( fileNameSearch ), conn->proc.fileNameBase, conn->proc.fileNameBaseLen );

			if ( *ext )
			{
				auto it = conn->req.virtualServer->mimeTypes.find ( statString ( ext ) );

				if ( it != conn->req.virtualServer->mimeTypes.end ( ) )
				{
					memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, it->second.ext.c_str ( ), it->second.ext.size ( ) + 1 );
					if ( (entry = globalFileCache.read ( fileNameSearch )) )
					{
						globalFileCache.free ( entry );
						// found it so now dispatch
						conn->req.mimeType = &it->second;
						conn->rsp.mimeType = &it->second;
						(conn->req.mimeType->handler)(instance, conn);
						conn->rsp.buildRspHeaders ( conn );
						return;
					}
				}

				// the exact file is not found, now we need to see if it's an ap with an html extension
				if ( *fName && (!memcmpi ( ext, ".html", 6 ) || !memcmpi ( ext, ".htm", 5 )) )
				{
					// for htm and html files we look for a .ap file if they're we can't find the static file
					memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".ap", 4 );
					if ( (entry = globalFileCache.read ( fileNameSearch )) )
					{
						globalFileCache.free ( entry );
						conn->req.mimeType = apxMimeType;
						conn->rsp.mimeType = apxMimeType;
						(conn->req.mimeType->handler)(instance, conn);
						conn->rsp.buildRspHeaders ( conn );
						return;
					}
				}

				// build a default mime type
				webMimeType defMimeType ( webStaticPageHandler, ext, "", "application/octet-stream", "", "" );

				conn->req.mimeType = &defMimeType;
				conn->rsp.mimeType = &defMimeType;

				(conn->req.mimeType->handler)(instance, conn);

				if ( conn->rsp.rspCode != 404 )
				{
					// something other than 404 so this WAS resolved into something or was an invalid request, in either case we can halt processing
					// no mime type... build and send response or execute handler
					conn->rsp.buildRspHeaders ( conn );
					return;
				}
			} else
			{
				auto lenSave = conn->proc.fileNameBaseLen;
				// search for .ap, .html, .htm in that order for names without an extension (this is non-standarad!)

				if ( !fName[0] || (fName[0] == '/' && !fName[1]) )
				{
					for ( auto &it : conn->req.virtualServer->defaultPages )
					{
						conn->proc.fileNameBaseLen = serverBuildName ( conn->req.virtualServer->rootName, it, conn->proc.fileNameBase );

						memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".ap", 4 );
						if ( _isfile ( fileNameSearch ) )
						{
							conn->req.mimeType = apxMimeType;
							conn->rsp.mimeType = apxMimeType;
							(conn->req.mimeType->handler)(instance, conn);
							conn->rsp.buildRspHeaders ( conn );
							return;
						}
						memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".apf", 5 );
						if ( _isfile ( fileNameSearch ) )
						{
							conn->req.mimeType = apxMimeType;
							conn->rsp.mimeType = apxMimeType;
							(conn->req.mimeType->handler)(instance, conn);
							conn->rsp.buildRspHeaders ( conn );
							return;
						}
						memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".aps", 5 );
						if ( _isfile ( fileNameSearch ) )
						{
							conn->req.mimeType = apxMimeType;
							conn->rsp.mimeType = apxMimeType;
							(conn->req.mimeType->handler)(instance, conn);
							conn->rsp.buildRspHeaders ( conn );
							return;
						}
						memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".html", 6 );
						if ( _isfile ( fileNameSearch ) )
						{
							conn->req.mimeType = htmlMimeType;
							conn->rsp.mimeType = htmlMimeType;
							(conn->req.mimeType->handler)(instance, conn);
							conn->rsp.buildRspHeaders ( conn );
							return;
						}
						memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".htm", 5 );
						if ( _isfile ( fileNameSearch ) )
						{
							conn->req.mimeType = htmMimeType;
							conn->rsp.mimeType = htmMimeType;
							(conn->req.mimeType->handler)(instance, conn);
							conn->rsp.buildRspHeaders ( conn );
							return;
						}
						conn->proc.fileNameBaseLen = lenSave;
					}
				} else
				{
					memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".ap", 4 );
					if ( _isfile ( fileNameSearch ) )
					{
						conn->req.mimeType = apxMimeType;
						conn->rsp.mimeType = apxMimeType;
						(conn->req.mimeType->handler)(instance, conn);
						conn->rsp.buildRspHeaders ( conn );
						return;
					}
					memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".apf", 5 );
					if ( _isfile ( fileNameSearch ) )
					{
						conn->req.mimeType = apxMimeType;
						conn->rsp.mimeType = apxMimeType;
						(conn->req.mimeType->handler)(instance, conn);
						conn->rsp.buildRspHeaders ( conn );
						return;
					}
					memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".aps", 5 );
					if ( _isfile ( fileNameSearch ) )
					{
						conn->req.mimeType = apxMimeType;
						conn->rsp.mimeType = apxMimeType;
						(conn->req.mimeType->handler)(instance, conn);
						conn->rsp.buildRspHeaders ( conn );
						return;
					}
					memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".html", 6 );
					if ( _isfile ( fileNameSearch ) )
					{
						conn->req.mimeType = htmlMimeType;
						conn->rsp.mimeType = htmlMimeType;
						(conn->req.mimeType->handler)(instance, conn);
						conn->rsp.buildRspHeaders ( conn );
						return;
					}
					memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".htm", 5 );
					if ( _isfile ( fileNameSearch ) )
					{
						conn->req.mimeType = htmMimeType;
						conn->rsp.mimeType = htmMimeType;
						(conn->req.mimeType->handler)(instance, conn);
						conn->rsp.buildRspHeaders ( conn );
						return;
					}
					conn->proc.fileNameBaseLen = lenSave;
				}
			}

			fileNameSearch[conn->proc.fileNameBaseLen] = 0;
			if ( _isDirectory ( fileNameSearch ) )
			{
				auto lenSave = conn->proc.fileNameBaseLen;
				memcpy_s ( conn->proc.fileNameBase + conn->proc.fileNameBaseLen, sizeof ( conn->proc.fileNameBase ) - conn->proc.fileNameBaseLen, "/index", 6 );
				conn->proc.fileNameBaseLen += 6;

				memcpy_s ( fileNameSearch, sizeof ( fileNameSearch ), conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
				memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".ap", 4 );
				if ( _isfile ( fileNameSearch ) )
				{
					conn->req.mimeType = apxMimeType;
					conn->rsp.mimeType = apxMimeType;
					(conn->req.mimeType->handler)(instance, conn);
					conn->rsp.buildRspHeaders ( conn );
					return;
				}
				memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".apf", 5 );
				if ( _isfile ( fileNameSearch ) )
				{
					conn->req.mimeType = apxMimeType;
					conn->rsp.mimeType = apxMimeType;
					(conn->req.mimeType->handler)(instance, conn);
					conn->rsp.buildRspHeaders ( conn );
					return;
				}
				memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".aps", 5 );
				if ( _isfile ( fileNameSearch ) )
				{
					conn->req.mimeType = apxMimeType;
					conn->rsp.mimeType = apxMimeType;
					(conn->req.mimeType->handler)(instance, conn);
					conn->rsp.buildRspHeaders ( conn );
					return;
				}
				memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".html", 6 );
				if ( _isfile ( fileNameSearch ) )
				{
					conn->req.mimeType = htmlMimeType;
					conn->rsp.mimeType = htmlMimeType;
					(conn->req.mimeType->handler)(instance, conn);
					conn->rsp.buildRspHeaders ( conn );
					return;
				}
				memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".htm", 5 );
				if ( _isfile ( fileNameSearch ) )
				{
					conn->req.mimeType = htmMimeType;
					conn->rsp.mimeType = htmMimeType;
					(conn->req.mimeType->handler)(instance, conn);
					conn->rsp.buildRspHeaders ( conn );
					return;
				}
				// need to do this for continued processing
				conn->proc.fileNameBaseLen = lenSave;
			}

			auto fNameLen = conn->proc.fileNameBaseLen;
			while ( (conn->proc.fileNameBase[fNameLen - 1] != '\\') && fNameLen ) fNameLen--;
			if ( !fNameLen )
			{
				// no mime type... build and send response or execute handler
				conn->rsp.rspCode = 404;
			}

			pathString	path ( (char const *) conn->req.headers[conn->req.shortUrl], (size_t) conn->req.headers[conn->req.shortUrl] );

			auto pIt = conn->req.virtualServer->pathPageMap.lower_bound ( path );

			if ( pIt != conn->req.virtualServer->pathPageMap.end ( ) )
			{
				if ( pIt->first == path )
				{
					memcpy ( conn->proc.fileNameBase, conn->req.virtualServer->rootName.c_str ( ), conn->req.virtualServer->rootName.size ( ) );
					memcpy ( conn->proc.fileNameBase + conn->req.virtualServer->rootName.size ( ), pIt->second.c_str ( ), pIt->second.size ( ) );
					conn->proc.fileNameBase[conn->req.virtualServer->rootName.size ( ) + pIt->second.size ( )] = 0;
					conn->proc.fileNameBaseLen = conn->req.virtualServer->rootName.size ( ) + pIt->second.size ( );
					conn->req.mimeType = apxMimeType;
					conn->rsp.mimeType = apxMimeType;
					(conn->req.mimeType->handler)(instance, conn);
					conn->rsp.buildRspHeaders ( conn );
					return;
				}
			}

			conn->rsp.rspCode = 404;
		} catch ( int rCode )
		{
			conn->rsp.rspCode = rCode;
		}
	}

	char name[256];
	sprintf_s ( name, sizeof ( name ), "http%i", conn->rsp.rspCode );
	char fileNameSearch[MAX_PATH];

	conn->proc.fileNameBaseLen = serverBuildName ( conn->req.virtualServer->rootName, name, conn->proc.fileNameBase );
	memcpy_s ( fileNameSearch, sizeof ( fileNameSearch ), conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
	memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".ap", 4 );
	if ( _isfile ( fileNameSearch ) )
	{
		conn->req.mimeType = apxMimeType;
		conn->rsp.mimeType = apxMimeType;
		(conn->req.mimeType->handler)(instance, conn);
		conn->rsp.buildRspHeaders ( conn );
		return;
	}

	memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".apf", 5 );
	if ( _isfile ( fileNameSearch ) )
	{
		conn->req.mimeType = apxMimeType;
		conn->rsp.mimeType = apxMimeType;
		(conn->req.mimeType->handler)(instance, conn);
		conn->rsp.buildRspHeaders ( conn );
		return;
	}

	memcpy_s ( fileNameSearch + conn->proc.fileNameBaseLen, sizeof ( fileNameSearch ) - conn->proc.fileNameBaseLen, ".aps", 5 );
	if ( _isfile ( fileNameSearch ) )
	{
		conn->req.mimeType = apxMimeType;
		conn->rsp.mimeType = apxMimeType;
		(conn->req.mimeType->handler)(instance, conn);
		conn->rsp.buildRspHeaders ( conn );
		return;
	}

	// no mime type... build and send response or execute handler
	conn->rsp.buildRspHeaders ( conn );
}

void webCustomMethodHandler ( class vmInstance *instance, class webProtocolH1 *conn )
{
	memcpy ( conn->proc.fileNameBase, conn->req.virtualServer->rootName.c_str ( ), conn->req.virtualServer->rootName.size ( ) );
	memcpy ( conn->proc.fileNameBase + conn->req.virtualServer->rootName.size ( ), conn->req.method->funcName.c_str(), conn->req.method->funcName.size() );
	conn->proc.fileNameBase[conn->req.virtualServer->rootName.size ( ) + conn->req.method->funcName.size() ] = 0;
	conn->proc.fileNameBaseLen = conn->req.virtualServer->rootName.size ( ) + conn->req.method->funcName.size ( );
	conn->req.mimeType = apxMimeType;
	conn->rsp.mimeType = apxMimeType;
	(conn->req.mimeType->handler)(instance, conn);
	return;
}
