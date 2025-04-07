

#include "webServer.h"
#include "webSocket.h"
#include "webProtoSSL.h"
#include "webProtoH1.h"
#include "webProtoH2.h"

#include "Target/fileCache.h"
#include "Target/vmTask.h"

#include "Utility/funcky.h"
#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/bcVMObject.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmDebug.h"
#include "bcInterpreter/bcInterpreter.h"
#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "compilerPreprocessor\compilerPreprocessor.h"
#include "Target/vmConf.h"
#include "Target/vmTask.h"
#include "Target/runAndCapture.h"

#include "webZipper.h"

#include "zlib.h"

#define PRINT_TIMES

static LARGE_INTEGER perfFreq;
static BOOL configured = (QueryPerformanceFrequency ( &perfFreq ), perfFreq.QuadPart = perfFreq.QuadPart / 1000, true);

void webServer::compileAp ( char const *inName, char const *outName, bool isSlang )
{
//	printf ("---------------------------------------------------");
//	Sleep (5000);
	BUFFER				 compBuff ( 1024ULL * 1024 * 4 );
	opFile				 oFile;
	char *code;
	LARGE_INTEGER		 startTimer;
	LARGE_INTEGER		 stopTimer;

	QueryPerformanceCounter ( &startTimer );

	// preproceses the file
	if ( !(code = compAPPreprocessor ( inName, isSlang, true )) )
	{
		oFile.errHandler.throwFatalError ( false, GetLastError ( ) );
		return;
	}

	try
	{
		// load all the preloaded interfaces (this code will be loaded into the instance for execution, so we need to link against the interfaces as well)
		for ( auto &it : instance->preLoadedSource )
		{
			auto obj = (char const *) std::get<1> ( it ).get ();

			oFile.addStream ( &obj, false, false );
		}

		// parse the file
		oFile.parseFile ( inName, code, isSlang, true, false );

		free ( code );

		if ( oFile.errHandler.isFatal ( ) )
		{
			return;
		}

		oFile.loadLibraries ( true, false );
		oFile.loadModules ( false, false );
	} catch ( errorNum err )
	{
		oFile.errHandler.throwFatalError ( false, err );
		throw;
	} catch ( unsigned long err )
	{
		oFile.errHandler.throwFatalError ( false, err );
		throw;
	}

	if ( oFile.errHandler.isFatal ( ) )
	{
		throw 500;
	}

	compExecutable comp ( &oFile );

	if ( vmConf.genDebug )
	{
		comp.enableDebugging ( );
	}
	if ( vmConf.genProfiler )
	{
		comp.enableProfiling ( );
	}

	if ( vmConf.genListing )
	{
		comp.listing.level = compLister::listLevel::LIGHT;
	}

	try
	{
		comp.genCode ( "main" );
	} catch ( errorNum err )
	{
		oFile.errHandler.throwFatalError ( false, err );
		throw;
	} catch ( unsigned long err )
	{
		oFile.errHandler.throwFatalError ( false, err );
		throw;
	}

	if ( oFile.errHandler.isFatal ( ) )
	{
		throw 500;
	}

	QueryPerformanceCounter ( &stopTimer );

	if ( vmConf.genListing )
	{
		stringi listName;
		listName = outName;
		listName += ".lst";
		FILE *lst;
		if ( fopen_s ( &lst, listName.c_str ( ), "wb" ) )
		{
			printf ( "unable to open list file \"%s\"for writing with error %lu\r\n", listName.c_str ( ), GetLastError ( ) );
		} else
		{
			comp.listing.printOutput ( lst );
			fclose ( lst );
		}
	} else
	{
		printf ( "rebuilt: %s in %I64ums\r\n", inName, ((stopTimer.QuadPart - startTimer.QuadPart) / perfFreq.QuadPart) );
	}

	// geneate the object file
	comp.serialize ( &compBuff, false );

	FILE *outHandle;

	if ( (outHandle = fopen ( outName, "w+b" )) )
	{
		fwrite ( bufferBuff ( &compBuff ), 1, bufferSize ( &compBuff ), outHandle );
		fclose ( outHandle );
	}
}

struct execDataEntry : public fileCacheUserData
{
	class atomTable *atomTable;
	stringi			 name;

	execDataEntry ( class atomTable *atomTable, stringi const &name ) : atomTable ( atomTable ), name ( name )
	{}
	virtual ~execDataEntry ( )
	{
		delete atomTable;
	}
};

void webServer::execAp ( class vmInstance *instance, webProtocolH1 *conn, char const *fName, fileCacheEntry *entry )
{
	VAR				*var;
	execDataEntry	*execData = nullptr;

	conn->rsp.activePage = true;
	QueryPerformanceCounter ( &conn->rsp.loadStartTime );
	conn->rsp.runStartTime = conn->rsp.loadEndTime;

	try
	{
		static_cast<vmTaskInstance *>(instance)->setCargo ( conn );

		conn->rsp.body.bodyType = webServerResponse::body::btObjectPage;

		auto vmTask = static_cast<vmTaskInstance *>(instance);
		vmTask->setName ( fName );

		QueryPerformanceCounter ( &conn->rsp.loadEndTime );

		if ( instance->debug->enabled )
		{
			if ( instance->atomTable )
			{
				delete instance->atomTable;
				instance->atomTable = nullptr;
			}
			instance->allocateAtomTable ( );
			builtinInit ( instance, builtinFlags::builtin_RegisterOnly );			// this is the INTERFACE

#if 0
			for ( auto &it : vmTask->preLoadedSource )
			{
				std::shared_ptr<uint8_t> dup ( (uint8_t *) instance->malloc ( std::get<0> ( it ) ), [=]( auto p ) { instance->free ( p ); } );
				memcpy ( &*dup, &*std::get<1> ( it ), std::get<0> ( it ) );
				instance->load ( dup, "builtIn", false );
			}
#endif
			if ( conn->req.virtualServer->apProjectNeedBuild )
			{
				std::lock_guard g1 ( conn->req.virtualServer->apProjectBuildGuard );

				if ( conn->req.virtualServer->apProjectNeedBuild )
				{
					BUFFER res;
					stringi cmd = stringi ( "cmd /c " ) + conn->req.virtualServer->apProjectBuildCommand;

					auto r = runAndCapture ( ".", cmd.c_str (), "", nullptr, res );

					printf ( "%.*s", (int) res.size (), res.data<char const *>() );

					if ( r )
					{
						printf ( "failure to make standard libraries\r\n" );
						throw (errorNum) r;
					}

					conn->req.virtualServer->apProjectNeedBuild = false;
				}
			}
			std::shared_ptr<uint8_t> dup ( (uint8_t *) instance->malloc ( entry->getDataLen () ), [=]( auto p ) { instance->free ( p ); } );
			memcpy ( &*dup, entry->getData (), entry->getDataLen () );
			if ( !instance->load ( dup, fName, false, false ) )
			{
				throw 500;
			}
			instance->getSourcePaths ( &*dup, [=]( auto dir ) 
									   { 
										   conn->req.virtualServer->monitor ( dir ); 
									   } 
			);
		} else
		{
			execData = static_cast <execDataEntry *>(entry->getUserData ( ));

			assert ( !execData || (execData && execData->name == fName) );

			if ( execData )
			{
				if ( instance->atomTable )
				{
					delete instance->atomTable;
				}
				instance->atomTable = execData->atomTable;
				instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
											   {
												   auto loadImage = instance->atomTable->atomGetLoadImage ( index );
												   memset ( loadImage->bssBase, 0, loadImage->bssSize );

												   auto globals = instance->om->allocVar ( loadImage->nGlobals );
												   for ( size_t loop = 0; loop < loadImage->nGlobals; loop++ )
												   {
													   loadImage->globals[loop] = &(globals[loop]);
													   loadImage->globals[loop]->type = slangType::eNULL;

													   loadImage->globals[loop] = loadImage->globalDefs[loop].image->globals[loadImage->globalDefs[loop].globalIndex];

												   }
												   return false;
											   }
				);
			} else
			{
				// all this copying around, etc is quite cpu intensive...
				// however, when we're NOT in debug mode (e.g. NOT putting breakpoints into the code segments)
				// we can get huge performance advantages by cacheing.   This is quite memory intensive.  For each
				// executing thread that is running the same image we need to have a separate cached user data segment
				// we can't just share one as individually running instances will have their BSS instances and may
				// individually load additional libraries at runtime.
				try
				{
					if ( instance->atomTable )
					{
						delete instance->atomTable;
						instance->atomTable = nullptr;
					}
					instance->allocateAtomTable ( );
					builtinInit ( instance,builtinFlags:: builtin_RegisterOnly );			// this is the INTERFACE
#if 0
					for ( auto &it : vmTask->preLoadedSource )
					{
						std::shared_ptr<uint8_t> dup ( (uint8_t *) instance->malloc ( std::get<0> ( it ) ), [=]( auto p ) { instance->free ( p ); } );
						memcpy ( &*dup, &*std::get<1> ( it ), std::get<0> ( it ) );
						instance->load ( dup, "builtIn", true );
					}
#endif
					std::shared_ptr<uint8_t> dup ( (uint8_t *) instance->malloc ( entry->getDataLen () ), [=]( auto p ) { instance->free ( p ); } );
					memcpy ( &*dup, entry->getData (), entry->getDataLen () );
					instance->load ( dup, fName, true, false );
				} catch ( ... )
				{
					// ensure sanity for our counters in the log
					QueryPerformanceCounter ( &conn->rsp.loadEndTime );
					conn->rsp.runEndTime = conn->rsp.runStartTime = { {0} };
					delete execData;
					execData = nullptr;
					instance->atomTable = nullptr;
					throw;
				}
				execData = new execDataEntry ( instance->atomTable, fName );
				entry->addUserData ( execData );
			}
		}
		QueryPerformanceCounter ( &conn->rsp.loadEndTime );

		// call main
		QueryPerformanceCounter ( &conn->rsp.runStartTime );
		instance->run ( "main" );
		instance->om->processDestructors ();					// MUST be done before calling detachPage!!!   destructors may live in those pages!
		QueryPerformanceCounter ( &conn->rsp.runEndTime );

		/* get the return string */
		var = instance->getGlobal ( fName, "__outputString" );

		if ( var )
		{
			while ( TYPE ( var ) == slangType::eREFERENCE )
			{
				var = var->dat.ref.v;
			}

			switch ( conn->rsp.body.bodyType )
			{
				case webServerResponse::body::btObjectPage:
				case webServerResponse::body::btObjectPageComplete:
					switch ( TYPE ( var ) )
					{
						case slangType::eOBJECT_ROOT:
							{
								VAR_OBJ out ( instance->getGlobal ( fName, "__outputString" ) );
								conn->rsp.body.cPtr = (uint8_t *)out["buff"]->dat.str.c;
								conn->rsp.body.cLen = out["size"]->dat.str.len;
								conn->rsp.body.cMemoryPage = instance->om->detachPage ( conn->rsp.body.cPtr );
								conn->rsp.body.cInstance = instance;
							}
							break;

						case slangType::eSTRING:
							conn->rsp.body.cPtr = (uint8_t *)var->dat.str.c;
							conn->rsp.body.cLen = var->dat.str.len;
							conn->rsp.body.cMemoryPage = instance->om->detachPage ( conn->rsp.body.cPtr );
							conn->rsp.body.cInstance = instance;
							break;

						default:
							conn->rsp.body.bodyType = webServerResponse::body::btNone;
							conn->rsp.rspCode = 500;
							break;
					}
					break;
				case webServerResponse::body::btFileCacheEntry:
					switch ( TYPE ( var ) )
					{
						case slangType::eOBJECT_ROOT:
							{
								conn->rsp.body.entry = globalFileCache.read ( classIVarAccess ( instance->getGlobal ( fName, "__outputString" ), "buff" )->dat.str.c );
								if ( !conn->rsp.body.entry )
								{
									printf ( "sendFile failed: %s  %lu\r\n", classIVarAccess ( instance->getGlobal ( fName, "__outputString" ), "buff" )->dat.str.c, (long unsigned)GetLastError () );
									conn->rsp.body.bodyType = webServerResponse::body::btNone;
									conn->rsp.rspCode = 404;
								}
							}
							break;
						case slangType::eSTRING:
							conn->rsp.body.entry = globalFileCache.read ( var->dat.str.c );
							if ( !conn->rsp.body.entry )
							{
								printf ( "sendFile failed: %s  %lu\r\n", classIVarAccess ( instance->getGlobal ( fName, "__outputString" ), "buff" )->dat.str.c, (long unsigned)GetLastError () );
								conn->rsp.body.bodyType = webServerResponse::body::btNone;
								conn->rsp.rspCode = 404;
							}
							break;
						case slangType::eARRAY_ROOT:
							conn->rsp.body.bodyType = webServerResponse::body::btChunked;
							conn->req.ranges.clear ();
							{
								int64_t indicie = 1;

								std::unique_ptr<webZipper> zip;
								conn->rsp.body.chunker = std::move ( zip );
								for ( ;; )
								{
									auto elem = (*var)[indicie];
									if ( !elem ) break;

									while ( TYPE ( elem ) == slangType::eREFERENCE ) elem = elem->dat.ref.v;

									switch ( TYPE ( elem ) )
									{
										case slangType::eARRAY_ROOT:
											{
												auto fName = (*elem)[1];
												auto zipFName = (*elem)[2];
												auto comp = (*elem)[3];
												if ( TYPE ( fName ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;
												if ( TYPE ( zipFName ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;
												if ( comp )
												{
													if ( TYPE ( comp ) != slangType::eLONG ) throw errorNum::scINVALID_PARAMETER;
													if ( comp->dat.l < 0 || comp->dat.l > 9 ) throw errorNum::scINVALID_PARAMETER;
												}
												if ( !_isfile ( fName->dat.str.c ) )
												{
													throw GetLastError ();
												}
												zip->addFile ( fName->dat.str.c, zipFName->dat.str.c, (!comp || comp->dat.l) ? true : false );
											}
											break;
										case slangType::eSTRING:
											if ( !_isfile ( elem->dat.str.c ) )
											{
												throw GetLastError ();
											}
											zip->addFile ( elem->dat.str.c, elem->dat.str.c, true );
											break;
										default:
											throw errorNum::scINVALID_PARAMETER;
									}
									indicie++;
								}
								zip->processBuffer ( 0 );		// prime the pump (get ready to send the first file
							}
							break;
						default:
							conn->rsp.body.bodyType = webServerResponse::body::btNone;
							conn->rsp.rspCode = 500;
							break;
					}
					break;
				default:
					throw 500;
			}
		} else
		{
			// odd case where we optimized away the output!
			conn->rsp.body.bodyType = webServerResponse::body::btNone;
		}

		if ( instance->debug->enabled )
		{
			if ( instance->atomTable ) delete instance->atomTable;
			instance->atomTable = 0;
			instance->reset ( );
		} else
		{
			instance->reset ( );
			if ( execData ) entry->freeUserData ( execData );
			instance->atomTable = 0;
		}
	} catch ( ... )
	{
		instance->stack = instance->eval;
		instance->om->processDestructors ();	// calling twice is OK... entries are removed as processed so the list will be empty when called again
		if ( instance->debug->enabled )
		{
			if ( instance->atomTable ) delete instance->atomTable;
			instance->atomTable = 0;
			instance->reset ( );
		} else
		{
			instance->reset ( );
			if  ( execData ) entry->freeUserData ( execData );
			instance->atomTable = 0;
		}
		QueryPerformanceCounter ( &conn->rsp.runEndTime );
		conn->rsp.body.bodyType = webServerResponse::body::btNone;
		conn->rsp.rspCode = 500;		// we threw some fault...
	}
}

#define DEFAULT_COMPRESSION (Z_BEST_SPEED)
#define DEFAULT_WINDOWSIZE (-15)
#define DEFAULT_MEMLEVEL (9)
#define DEFAULT_BUFFERSIZE (8096)

static void compressBuffer ( serverBuffer * destBuff, uint8_t * buff, size_t len )
{
	char		 tmpBuff[DEFAULT_BUFFERSIZE]; // NOLINT ( Int-uninitialized-local )
	z_stream	 stream;

	destBuff->reset ( );
	memset ( &stream, 0, sizeof ( stream ) );

	deflateInit2 ( &stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,15 + 16, DEFAULT_MEMLEVEL, Z_DEFAULT_STRATEGY );

	stream.next_in = ( Bytef *) buff;
	stream.avail_in = (uInt)len;
	stream.next_out = ( Bytef *) tmpBuff;
	stream.avail_out = sizeof ( tmpBuff );
	stream.data_type = Z_BINARY;

	if ( deflate ( &stream, Z_NO_FLUSH ) != Z_OK )
	{
		destBuff->reset ( );
	}
	destBuff->write ( tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );

	stream.next_out = ( Bytef *) tmpBuff;
	stream.avail_out = sizeof ( tmpBuff );
	while ( deflate ( &stream, Z_FINISH ) != Z_STREAM_END )
	{
		destBuff->write ( tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );
		stream.next_out = ( Bytef *) tmpBuff;
		stream.avail_out = sizeof ( tmpBuff );
	}
	destBuff->write ( tmpBuff, sizeof ( tmpBuff ) - stream.avail_out );

	deflateEnd ( &stream );
}

size_t addOrChangeHeader ( webProtocolH1 *conn, webServerHeader const &header )
{
	for ( auto &it : conn->req.headers )
	{
		if ( !strccmp ( it.name, header.name ) )
		{
			it = header;
			return std::distance ( &conn->req.headers.front(), &it );
		}
	}
	conn->req.headers.push_back ( header );
	return conn->req.headers.size ( ) - 1;
}

void webActivePageHandler ( vmInstance *instance, webProtocolH1 *conn )
{
	char				 fName[MAX_PATH + 1];
	fileCacheEntry		*entry;

	char				 fNameAPX[MAX_PATH + 1];

	struct _stat		 APStat;
	struct _stat		 APSStat;
	struct _stat		 APFStat;
	struct _stat		 APXStat;


	// default to everything being OK
	conn->rsp.rspCode = 200;

	if ( sizeof ( fNameAPX ) - conn->proc.fileNameBaseLen < 5 )
	{
		// worst case .apx\0  size
		conn->rsp.rspCode = 404;
		return;
	}

	// apx file name
	memcpy ( fNameAPX, conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
	memcpy ( fNameAPX + conn->proc.fileNameBaseLen, ".apx", 5 );
	if ( !(entry = globalFileCache.get ( fNameAPX, conn->proc.fileNameBaseLen + 4 ))
		 )
	{
		// no cache entry... see if the compiled code exists on disk
		if ( _stat ( fNameAPX, &APXStat ) )
		{
			// no apx, _stat failed... try to compile it

			// build base file name - we'll use ap here and build with the apf/aps default
			memcpy ( fName, conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
			memcpy ( fName + conn->proc.fileNameBaseLen, ".ap", 4 );
			if ( !_stat ( fName, &APStat ) )
			{
				try
				{
//for ( auto loop = 0; loop < 1000; loop++ )
					conn->req.server->compileAp ( fName, fNameAPX, conn->req.virtualServer->apDefaultAPS );
				} catch ( ... )
				{
					conn->rsp.rspCode = 500;
					return;
				}
			} else
			{
				// no .AP file, let's see if there's an APF file instead 
				// build base file name
				memcpy ( fName + conn->proc.fileNameBaseLen, ".apf", 5 );
				if ( !_stat ( fName, &APStat ) )
				{
					try
					{
						conn->req.server->compileAp ( fName, fNameAPX, false );
					} catch ( ... )
					{
						conn->rsp.rspCode = 500;
						return;
					}
				} else
				{
					// no .AP file, let's see if there's an APF file instead 
					// build base file name
					memcpy ( fName + conn->proc.fileNameBaseLen, ".aps", 5 );
					if ( !_stat ( fName, &APStat ) )
					{
						try
						{
							conn->req.server->compileAp ( fName, fNameAPX, true );
						} catch ( ... )
						{
							conn->rsp.rspCode = 500;
							return;
						}
					} else
					{
						// not found
						conn->rsp.rspCode = 404;
						return;
					}
				}
			}
		} else
		{
			// copy over the base part of the file name
			memcpy ( fName, conn->proc.fileNameBase, conn->proc.fileNameBaseLen );
			// get dates on the possible source files (.ap, .apf, .aps
			memcpy ( fName + conn->proc.fileNameBaseLen, ".ap", 4 );
			auto apStat = _stat ( fName, &APStat );
			memcpy ( fName + conn->proc.fileNameBaseLen, ".apf", 5 );
			auto apfStat = _stat ( fName, &APFStat );
			memcpy ( fName + conn->proc.fileNameBaseLen, ".aps", 5 );
			auto apsStat = _stat ( fName, &APSStat );

			if ( apStat && apfStat && apsStat )
			{
				// no source available, dang, just an apx so we'll use that 
			} else
			{
				// see if any of the sources are available and if so build them, we priortize .ap, .aps, .apf
				if ( !apStat && APStat.st_mtime > APXStat.st_mtime )
				{
					// ap file is newer so we must recompile the .apx
					try
					{
						memcpy ( fName + conn->proc.fileNameBaseLen, ".ap", 4 );
						conn->req.server->compileAp ( fName, fNameAPX, conn->req.virtualServer->apDefaultAPS );
					} catch ( ... )
					{
						conn->rsp.rspCode = 500;
						return;
					}
				} else if ( !apsStat && APSStat.st_mtime > APXStat.st_mtime )
				{
					// aps file is newer so we must recompile the .apx
					try
					{
						memcpy ( fName + conn->proc.fileNameBaseLen, ".aps", 5 );
						conn->req.server->compileAp ( fName, fNameAPX, true );
					} catch ( ... )
					{
						conn->rsp.rspCode = 500;
						return;
					}
				} else if ( !apfStat && APFStat.st_mtime > APXStat.st_mtime )
				{
					// apf file is newer so we must recompile the .apx
					try
					{
						memcpy ( fName + conn->proc.fileNameBaseLen, ".apf", 5 );
						conn->req.server->compileAp ( fName, fNameAPX, false );
					} catch ( ... )
					{
						conn->rsp.rspCode = 500;
						return;
					}
				} else
				{
					// all good... the source is current and we can go ahead and execute
				}
			}
		}
		entry = globalFileCache.read ( fNameAPX, true );
	}

	conn->rsp.cachePolicy.policyType = webCachePolicy_NoCache;

	if ( entry && entry->isDataCached ( ) )
	{
		conn->req.fName = fNameAPX;
		conn->req.server->execAp ( instance, conn, fNameAPX, entry );
		globalFileCache.free ( entry );

	} else
	{
		conn->rsp.rspCode = 404;
	}

	conn->rsp.modifiedTime = time ( 0 );

	if ( conn->rsp.rspCode == 200 )
	{
		conn->rsp.isCompressed = false;
		if ( conn->rsp.body.bodyType == webServerResponse::body::btObjectPage || conn->rsp.body.bodyType == webServerResponse::body::btObjectPageComplete )
		{
			if ( (conn->req.compress & compGzip) && (conn->rsp.body.cLen >= conn->req.virtualServer->compressAPMin) )
			{
				compressBuffer ( &conn->rsp.body.buff, conn->rsp.body.cPtr, conn->rsp.body.cLen );
				conn->rsp.body.release ( );
				conn->rsp.body.bodyType = webServerResponse::body::btServerBuffer;
				conn->rsp.isCompressed = true;
			}
		}
	} else if ( conn->rsp.rspCode == 308 )
	{
		// processing for chainPage()

		// in this case, we clear out all the multiparts,
		// we then reparse our header line and replace any header entries with the new reparsed values as well as redoing 
		uint8_t *startUrl;
		uint8_t *startName;
		uint8_t *startValue;
		uint8_t *headerEnd;
		uint8_t *startQuery;

		std::string headerString ( (char const *)( uint8_t *) conn->rsp.body, (size_t)conn->rsp.body );
		auto headerBufferLen = headerString.size ( );
		uint8_t *headerBuffer = const_cast<uint8_t *>((uint8_t const *)headerString.c_str ( ));

		conn->rsp.body.release ( );

		headerEnd = headerBuffer + headerBufferLen;

		// first stage, pointer is set to head,
		auto headerBufferPointer = headerBuffer;
		conn->req.shortUrl = 0;
		conn->req.method = 0;

		startValue = 0;
		startName = 0;
		startQuery = 0;
		startUrl = headerBufferPointer;
		conn->req.multiParts.clear ( );

		while ( headerBufferPointer < headerEnd )
		{
			if ( (*headerBufferPointer == '?') && !conn->req.shortUrl )
			{
				conn->req.shortUrl = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
				headerBufferPointer++;
				startName = headerBufferPointer;
				startQuery = headerBufferPointer;
				conn->req.multiParts.push_back ( webServerMultipart ( ) );
				conn->req.multiParts.back ( ).dataLen = 0;
				conn->req.multiParts.back ( ).dataOffset = 0;
			}
			if ( *headerBufferPointer == '=' )
			{
				headerBufferPointer++;
				startValue = headerBufferPointer;
			}
			if ( *headerBufferPointer == '&' )
			{
				if ( !startName || !conn->req.shortUrl )
				{
					conn->req.shortUrl = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
					conn->req.url = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "URL", 3, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
					conn->req.firstLine = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "FIRST_LINE", 10, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
					throw 400;
				}
				if ( startValue )
				{
					conn->req.multiParts.back ( ).vars.push_back ( webServerRequestVar ( &headerBuffer, startName, startValue - 1 - startName, startValue, headerBufferPointer - startValue ) );
				} else
				{
					conn->req.multiParts.back ( ).vars.push_back ( webServerRequestVar ( &headerBuffer, startName, startValue - 1 - startName, startName, 0 ) );
				}
				startValue = 0;
				headerBufferPointer++;
				startName = headerBufferPointer;
			}
			if ( *headerBufferPointer == ' ' )
			{
				// done with URL... if shortURL is present then we've already stored the short URL and extracted out the params... otherwise we need to store it now
				if ( !conn->req.shortUrl )
				{
					conn->req.shortUrl = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
				}
				break;
			}
			headerBufferPointer++;
		}
		if ( startName )
		{
			if ( !conn->req.shortUrl )
			{
				conn->req.shortUrl = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "shortURL", 8, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
				conn->req.url = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "URL", 3, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
				conn->req.firstLine = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "FIRST_LINE", 10, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );
				throw 400;
			}
			if ( startValue && (startValue != headerEnd) )
			{
				conn->req.multiParts.back ( ).vars.push_back ( webServerRequestVar ( &headerBuffer, startName, ( uint32_t) (startValue - 1 - startName), startValue, ( uint32_t) (headerBufferPointer - startValue) ) );
			} else
			{
				conn->req.multiParts.back ( ).vars.push_back ( webServerRequestVar ( &headerBuffer, startName, ( uint32_t) (startValue - 1 - startName), startName, 0 ) );
			}
		}
		if ( startQuery )
		{
			conn->req.query = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "QUERY", 3, startQuery, ( uint32_t) (headerBufferPointer - startQuery) ) );
		}
		conn->req.url = addOrChangeHeader ( conn, webServerHeader ( &headerBuffer, "URL", 3, startUrl, ( uint32_t) (headerBufferPointer - startUrl) ) );

		// recurse and reparse the new query
		conn->rsp.rspCode = 200;
		webGetPostMethodHandler ( instance, conn );
	}
}
