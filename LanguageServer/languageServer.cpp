#include "Utility/settings.h"

#ifdef _WIN32
#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif


#include <sstream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <set>
#include <variant>
#include <tuple>

#include "Utility\jsonParser.h"
#include "rpcServer.h"
#include "Utility\socketServer.h"
#include "consoleServer.h"
#include "Target/fileCache.h"
#include "compilerParser/fileParser.h"
#include "compilerParser/funcParser.h"
#include "compilerParser/classParser.h"
#include "compilerPreprocessor\compilerPreprocessor.h"
#include "languageServer.h"
#include "Utility/stringCache.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmInstance.h"
#include "Target/vmTask.h"
#include "Utility/funcky.h"
#include "compilerAST/astNodeWalk.h"


//static	lsJsonRPCServerBase *activeLanguageServer = nullptr;

static stringi getElementSignature ( compClassElementSearch *elem )
{
	stringi signature;
	stringi type = elem->elem->symType;

	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_method:
			return elem->methodAccess.func->getSimpleFunctionSignature ();
		case fgxClassElementType::fgxClassType_iVar:
			return stringi ( "(ivar) " ) + type;
		case fgxClassElementType::fgxClassType_static:
			return stringi ( "(static) \n" ) + type;
		case fgxClassElementType::fgxClassType_const:
			return stringi ( "(const) \n" ) + type;
		case fgxClassElementType::fgxClassType_prop:
			return type + "(property)  " + elem->elem->name.c_str () + " ()";
		default:
			break;
	}
	return signature;
}

static void getDependencies ( languageServer *ls, stringi const &rootPath )
{
	int			retCode = 0;

	auto close = [&retCode] ( FILE *file )
	{
		retCode = _pclose ( file );
	};

	std::filesystem::path	maker ( rootPath.operator const std::string & () );

	stringi cmdToRun = "smake.exe -C ";
	cmdToRun += maker.generic_string ();
	cmdToRun += " new gendep -s -j8";
	cmdToRun += " 2>&1";// redirect stderr to stdout

	BUFFER buff;
	buff.write ( "{" );
	{
		std::unique_ptr<FILE, decltype(close)> pipe ( _popen ( cmdToRun, "r" ), close );
		if ( !pipe )
		{
			throw std::runtime_error ( "popen() failed!" );
		}

		buff.makeRoom ( 8192 );
		while ( fgets ( bufferBuff ( &buff ) + buff.size (), (int) bufferFree ( &buff ), pipe.get () ) != nullptr )
		{
			buff.assume ( strlen ( bufferBuff ( &buff ) + buff.size () ) );
			buff.makeRoom ( 8192 );
		}
	}

	buff.write ( "}" );
	buff.put ( 0 );
	//	printf ( "%s\r\n", buff.data<char *> () );

	char const *depStr = buff.data<char const *> ();

	try
	{
		jsonElement dep ( &depStr );
		for ( auto libs = dep.cbeginObject (); libs != dep.cendObject (); libs++ )
		{
			auto &lib = libs->first;
			ls->targetFiles[lib].push_back ( {lib, languageServer::targetType::library} );
			for ( auto files = libs->second.cbeginArray (); files != libs->second.cendArray (); files++ )
			{
				ls->fileToTargetMap[(*files).operator const stringi &()] = (stringi)lib;
				ls->targetFiles[lib].push_back ( {(*files).operator const stringi & (), languageServer::targetType::file} );
			}
		}
	} catch ( ... )
	{
	}
}

static stringi fileNameToUri ( stringi const &uri )
{
	stringi result = "file:///";
	result += uri;
	return result;
}

static stringi uriToFileName ( stringi const &uri )
{
	stringi result;
	char const *uriPtr = uri.c_str ();
	if ( !memcmpi ( uriPtr, "file:///", 8 ) )
	{
		uriPtr += 8;
	}
	while ( *uriPtr )
	{
		if ( *uriPtr == '\\' )
		{
			result += '/';
			uriPtr++;
		} else
		{
			result += *(uriPtr++);
		}
	}
	return result;
}

static stringi unescapeJsonString ( char const *str )
{
	stringi v;

	while ( *str && str[0] != '"' )
	{
		if ( str[0] == '%' && str[1] )
		{
			if ( _ishex ( &str[1] ) && _ishex ( &str[2] ) )
			{
				v += static_cast<char>((hexDecodingTable[(size_t) (uint8_t) str[1]] << 4) + hexDecodingTable[(size_t) (uint8_t) str[2]]);
				str += 3;
			} else
			{
				v += *(str++);
			}
		} else
		{
			v += *(str++);
		}
	}
	return v;
}

// support functions
static jsonElement range ( srcLocation const &loc )
{
	assert ( loc.lineNumberStart && loc.lineNumberEnd && loc.columnNumber && loc.columnNumberEnd );
	jsonElement ret;
	// apparently the send column number is exclusive while start is inclusive
	ret["start"]["line"] = loc.lineNumberStart - 1;
	ret["start"]["character"] = loc.columnNumber - 1;
	ret["end"]["line"] = loc.lineNumberEnd - 1;
	ret["end"]["character"] = loc.columnNumberEnd - 1;

	return ret;
}

static stringi getType ( astNode *node )
{
	switch ( node->getOp () )
	{
		case astOp::doubleValue:
			return stringi ( "(double) " ) + node->doubleValue ();
		case astOp::intValue:
			return stringi ( "(int) " ) + node->intValue ();
		case astOp::nullValue:
			return stringi ( "(variant) null" );
		case astOp::nilValue:
			return stringi ( "(nil)" );
		case astOp::codeBlockValue:
		case astOp::compValue:
			return stringi ( "(codeblock)" );
		case astOp::stringValue:
			return stringi ( "(string) " ) + node->stringValue ();
		default:
			return"";
	}
}

static stringi getDetail ( opFile *file, symbolSource const &source, bool isAccess )
{
	if ( std::holds_alternative<class opFunction *> ( source ) )
	{
		auto func = std::get<class opFunction *> ( source );
		return func->getSimpleFunctionSignature ();
	} else if ( std::holds_alternative<opClass *> ( source ) )
	{
		auto cls = std::get<opClass *> ( source );
		return stringi ( "class " ) + cls->name.operator const stringi & ();
	} else if ( std::holds_alternative<astNode *> ( source ) )
	{
		auto node = std::get<astNode *> ( source );
		auto type = getType ( node );
		if ( type.size () )
		{
			return stringi ( "" ) + type;
		} else return "";
	} else if ( std::holds_alternative<opClassElement *> ( source ) )
	{
		auto elem = std::get<opClassElement *> ( source );
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				return getDetail ( file, file->functionList.find ( elem->data.method.func )->second, isAccess );
			case fgxClassElementType::fgxClassType_iVar:
				{
					stringi type = elem->symType;
					return stringi ( "(ivar) " ) + type;
				}
			case fgxClassElementType::fgxClassType_static:
				{
					stringi type = elem->symType;
					return stringi ( "(static) " ) + type;
				}
			case fgxClassElementType::fgxClassType_const:
				{
					stringi type = elem->symType;
					return stringi ( "(const) " ) + type;
				}
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					return getDetail ( file, file->functionList.find ( elem->data.prop.access )->second, isAccess );
				} else
				{
					return getDetail ( file, file->functionList.find ( elem->data.prop.assign )->second, isAccess );
				}
			case fgxClassElementType::fgxClassType_inherit:
				{
					stringi type = elem->symType;
					return stringi ( "(inherited class) " ) + type;
				}
			default:
				break;
		}
	} else if ( std::holds_alternative<symbol *> ( source ) )
	{
		auto sym = std::get<symbol *> ( source );
		switch ( sym->symType )
		{
			case symbolSpaceType::symTypeLocal:
				return stringi ( "local " ) + sym->getType ( sym->getSymbolName (), isAccess ).operator stringi();
			case symbolSpaceType::symTypeStatic:
				return stringi ( "static " ) + sym->getType ( sym->getSymbolName (), isAccess ).operator stringi();
			case symbolSpaceType::symTypeField:
				return stringi ( "field " ) + sym->getType ( sym->getSymbolName (), isAccess ).operator stringi();
			default:
				break;
		}
	} else if ( std::holds_alternative<symbolParamDef *> ( source ) )
	{
		auto sym = std::get<symbolParamDef *> ( source );
		return stringi ( "param " ) + sym->type.operator stringi();
	}

	return "(unknown)";
}

static stringi getDocumentation ( opFile *file, symbolSource const &source, bool isAccess )
{
	if ( std::holds_alternative<class opFunction *> ( source ) )
	{
		auto func = std::get<class opFunction *> ( source );
		stringi documentation;
		documentation += stringi ( "<h4>" ) + func->documentation + "<h4><br><br>";
		for ( int index = 0; index < func->params.size (); index++ )
		{
			if ( func->classDef && !index )
			{
				// skip self
				continue;
			}
			auto const &param = func->params[index];
			if ( param->documentation.size () )
			{
				documentation += "<br>@param ";
				documentation += param->getName ();
				documentation += " ";
				documentation += param->getDocumentation();
			}
		}
		if ( func->returnDocumentation.size() )
		{
			documentation += "<br>@return ";
			documentation += func->returnDocumentation;
		}
		return documentation;
	} else if ( std::holds_alternative<class opSymbol *> ( source ) )
	{
		auto sym = std::get<class opSymbol *> ( source );
		return sym->documentation;
	} else if ( std::holds_alternative<class opClass *> ( source ) )
	{
		auto cls = std::get<class opClass *> ( source );
		return cls->documentation;
	} else if ( std::holds_alternative<class opClassElement *> ( source ) )
	{
		auto elem = std::get<opClassElement *> ( source );
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				return getDetail ( file, file->functionList.find ( elem->data.method.func )->second, isAccess );
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					return getDetail ( file, file->functionList.find ( elem->data.prop.access )->second, isAccess );
				} else
				{
					return getDetail ( file, file->functionList.find ( elem->data.prop.assign )->second, isAccess );
				}
			default:
				return elem->documentation;
		}
	} else if ( std::holds_alternative<symbol *> ( source ) )
	{
		auto sym = std::get<symbol *> ( source );
		switch ( sym->symType )
		{
			case symbolSpaceType::symTypeLocal:
			case symbolSpaceType::symTypeStatic:
			case symbolSpaceType::symTypeField:
				return sym->getDocumentation ( sym->getSymbolName (), isAccess );
			default:
				break;
		}
	}
	return stringi ();
}

static srcLocation getSymbolSourceLocation ( symbolSource const &source, cacheString const &name )
{
	if ( std::holds_alternative<opFunction *> ( source ) )
	{
		return std::get<opFunction *> ( source )->location;
	} else if ( std::holds_alternative<symbolParamDef *> ( source ) )
	{
		return std::get<symbolParamDef *> ( source )->location;
	} else if ( std::holds_alternative<symbol *> ( source ) )
	{
		return std::get<symbol *> ( source )->getDefinitionLocation ( name, true );
	} else if ( std::holds_alternative<opClassElement *> ( source ) )
	{
		return std::get<opClassElement *> ( source )->location;
	} else if ( std::holds_alternative<opSymbol *> ( source ) )
	{
		return std::get<opSymbol *> ( source )->location;
	} else if ( std::holds_alternative<opClass *> ( source ) )
	{
		auto cls = std::get<opClass *> ( source );
		for ( auto &elem : cls->elems )
		{
			if ( elem->name == "new" )
			{
				if ( elem->location.sourceIndex )
				{
					return elem->location;
				}
			}
		}

		return cls->nameLocation;
	}
	return srcLocation ();
}

void languageServerFile::updateSource ()
{
	LARGE_INTEGER		 start = {};
	LARGE_INTEGER		 parse = {};
	LARGE_INTEGER		 query = {};
	LARGE_INTEGER		 end = {};
	LARGE_INTEGER		 frequency = {};

	QueryPerformanceCounter ( &start );

	file.loadLibraries ( true, true );

	bool doBuild = false;

	for ( auto &[fileName, data] : fileVersions )
	{
		if ( lsFileCache.getVersion ( fileName ) != std::get<0> ( data ) )
		{
			fileVersions.find ( fileName )->second = {0, std::get<1> ( data )};						// if any child has changed make sure we rebuild our primary file even if the version hasn't changed as dependencies may well have
			info.clear ();
		}
	}

	for ( auto &[fileName, data] : fileVersions )
	{
		if ( lsFileCache.getVersion ( fileName ) != std::get<0> ( data ) )
		{
			doBuild = true;

			auto sourceIndex = file.srcFiles.getStaticIndex ( fileName );
			file.clearSourceIndex ( sourceIndex );

			char const *codeTmp = lsFileCache.getCode ( fileName ).c_str ();
			char *code;

			bool isSlang = lsFileCache.getLanguage ( fileName ) == languageServerFileData::languageType::ap_slang || lsFileCache.getLanguage ( fileName ) == languageServerFileData::languageType::slang;
			bool isAP = lsFileCache.getLanguage ( fileName ) == languageServerFileData::languageType::ap_fgl || lsFileCache.getLanguage ( fileName ) == languageServerFileData::languageType::ap_slang;

			if ( isAP )
			{
				code = compAPPreprocessBuffer ( fileName, codeTmp, isSlang, true );
			} else
			{
				code = compPreprocessor ( fileName, codeTmp, isSlang );
			}

			file.setSourceListing ( fileName, code );
			file.parseFile ( fileName, code, isSlang, true, isAP, sourceFile::sourceFileType::external );
			QueryPerformanceCounter ( &parse );
			free ( code );

			data = {lsFileCache.getVersion ( fileName ), std::get<1> ( data )};
		}
	}

	if ( doBuild )
	{
		try
		{
			file.loadLibraries ( true, true );
		} catch ( ... )
		{
			// TODO: genenerate LS diagnostic
//					printf ( "****** error loading libraries\r\n" );
		}

		if ( compiler )
		{
			delete compiler;
		}

		compiler = new compExecutable ( &file );

		needSendErrors = true;

		char const *entryPoint = nullptr;
		if ( file.functionList.find ( file.sCache.get ( "main" ) ) != file.functionList.end () )
		{
			entryPoint = "main";
		}

		compiler->genLS ( entryPoint, 0 );

		QueryPerformanceCounter ( &query );
		compiler->getInfo ( info, file.srcFiles.getStaticIndex ( activeFile ) );

		QueryPerformanceCounter ( &end );

		QueryPerformanceFrequency ( &frequency );
		frequency.QuadPart = frequency.QuadPart / 1000;
#if 0
		printf ( "parse time = %I64u ms\r\n", (parse.QuadPart - start.QuadPart) / frequency.QuadPart );
		printf ( "compile time = %I64u ms\r\n", (query.QuadPart - parse.QuadPart) / frequency.QuadPart );
		printf ( "getInfo time = %I64u ms\r\n", (end.QuadPart - query.QuadPart) / frequency.QuadPart );
#endif
	}
}


static languageServerFile *getFile ( lsJsonRPCServerBase &server, languageServer *ls, stringi const &uri, int64_t id, stringi const &code = stringi (), int64_t version = 1, languageServerFileData::languageType languageId = languageServerFileData::languageType::slang )
{
	auto uriFixed = unescapeJsonString ( uri.c_str () );
	auto fileName = uriToFileName ( uriFixed );

	auto it = ls->fileToTargetMap.find ( fileName );
	if ( it == ls->fileToTargetMap.end () )
	{
		ls->fileToTargetMap[fileName] = fileName;
		ls->targetFiles[fileName].push_back ( {fileName, languageServer::targetType::file} );
	}

	auto &target = ls->fileToTargetMap[fileName];

	languageServerFile *lsFile = ls->targets[target].get ();

	if ( !lsFile && code.size () )
	{
		ls->targets[target] = std::make_unique<languageServerFile> ();
		lsFile = ls->targets[target].get ();

		lsFile->fileVersions[fileName] = {0, std::get<1> ( lsFile->fileVersions[fileName] )};

		lsFile->baseLibrary = target;

		// add in default libraries
		auto builtIn = ls->builtIn;
		lsFile->file.addStream ( (const char **) &builtIn, false, false );

		std::filesystem::path	targetFName = target.c_str ();
		stringi targetExtension = targetFName.extension ().string ().c_str ();
		if ( targetExtension == "sll" || targetExtension == "flb" )
		{
			lsFile->file.libraries.push_back ( moduleLibraryEntry{lsFile->file.sCache.get ( target ), srcLocation ()} );
		}

		// add in all the associated sources from other files if we're part of a library to handle cross dependencies
		for ( auto &it : ls->targetFiles[target] )
		{
			switch ( it.second )
			{
				case languageServer::targetType::file:
					if ( !lsFileCache.has ( it.first ) )
					{
						//				printf ( "\topening: %s\r\n", it.first.c_str () );

						languageServerFileData::languageType langId = languageServerFileData::languageType::unknown;

						std::filesystem::path p = it.first.operator std::string & ();
						if ( !strccmp ( p.extension ().generic_string ().c_str (), ".fgl" ) )
						{
							langId = languageServerFileData::languageType::fgl;
						} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".sl" ) )
						{
							langId = languageServerFileData::languageType::slang;
						} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".ap" ) )
						{
							if ( std::get<bool> ( ls->configFlags.apAreSLANG ) )
							{
								langId = languageServerFileData::languageType::ap_slang;
							} else
							{
								langId = languageServerFileData::languageType::ap_fgl;
							}
						} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".apf" ) )
						{
							langId = languageServerFileData::languageType::ap_fgl;
						} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".aps" ) )
						{
							langId = languageServerFileData::languageType::ap_slang;
						}

						if ( langId != languageServerFileData::languageType::unknown )
						{
							if ( it.first != fileName )
							{
								std::ifstream inFile ( it.first.c_str () );
								if ( inFile.is_open () )
								{
									lsFileCache.update ( it.first, code, 0 );

									// this makes that file a dependency of us
									lsFile->fileVersions[it.first] = {0, false};
								}
							}
						}
					} else
					{
						lsFile->fileVersions[it.first] = std::pair{0, false};
					}
					break;
				case languageServer::targetType::library:
					{
						bool found = false;
						for ( auto &lib : lsFile->file.libraries )
						{
							if ( it.first == lib.fName.operator const stringi & () )
							{
								found = true;
							}
						}
						if ( !found )
						{
							lsFile->file.libraries.push_back ( moduleLibraryEntry{lsFile->file.sCache.get ( it.first ), srcLocation ()} );
						}
					}
					break;
			}
		}
		languageServerFileData::languageType langId = languageServerFileData::languageType::unknown;

		std::filesystem::path p = fileName.operator std::string & ();
		if ( !strccmp ( p.extension ().generic_string ().c_str (), ".fgl" ) )
		{
			langId = languageServerFileData::languageType::fgl;
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".sl" ) )
		{
			langId = languageServerFileData::languageType::slang;
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".ap" ) )
		{
			if ( std::get<bool> ( ls->configFlags.apAreSLANG ) )
			{
				langId = languageServerFileData::languageType::ap_slang;
			} else
			{
				langId = languageServerFileData::languageType::ap_fgl;
			}
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".apf" ) )
		{
			langId = languageServerFileData::languageType::ap_fgl;
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".aps" ) )
		{
			langId = languageServerFileData::languageType::ap_slang;
		}

		lsFile->fileVersions[fileName] = std::pair{0, true};
		// update the source but don't compile... we'll compile ONLY on demand
		lsFile->activeFile = fileName;
		lsFile->activeUri = uri;
		lsFileCache.update ( fileName, code, version, langId );
	} else if ( lsFile )
	{
		lsFile->activeFile = fileName;
		lsFile->activeUri = uri;
		lsFile->fileVersions[fileName] = {std::get<0> ( lsFile->fileVersions[fileName] ), true};
		if ( code.size () )
		{
			// update the file but don't compile
			lsFileCache.update ( fileName, code, version );
		} else
		{
			// we now need to actually start processing so parse the file
			lsFile->updateSource ();
			if ( lsFile->needSendErrors )
			{
				server.sendNotification ( (size_t) notifications::publishDiagnostics );
			}
		}
	}

	return lsFile;
}

static void workspaceConfigCB ( int64_t id, jsonElement const &rsp, lsJsonRPCServerBase &server, languageServer *ls )
{
	if ( rsp.isArray () )
	{
		for ( auto objectIt = rsp.cbeginArray (); objectIt != rsp.cendArray (); objectIt++ )
		{
			for ( auto sectionIt = (*objectIt).cbeginObject (); sectionIt != (*objectIt).cendObject (); sectionIt++ )
			{
				for ( auto elem = sectionIt->second.cbeginObject (); elem != sectionIt->second.cendObject (); elem++ )
				{
					auto &name = elem->first;
					if ( ls->configValue.find ( name ) != ls->configValue.end () )
					{
						if ( elem->second.isBool () )
						{
							ls->configValue.find ( name )->second = elem->second.operator const bool ();
						} else if ( elem->second.isString () )
						{
							ls->configValue.find ( name )->second = (elem->second).operator const stringi & ();
						} else
						{
							// invalid type;
						}
					}
				}
			}
		}
	} else if ( rsp.isObject () )
	{
		for ( auto objectIt = rsp.cbeginObject (); objectIt != rsp.cendObject (); objectIt++ )
		{
			auto &object = objectIt->second;
			for ( auto elem = object.cbeginObject (); elem != object.cendObject (); elem++ )
			{
				auto &name = elem->first;
				if ( ls->configValue.find ( name ) != ls->configValue.end () )
				{
					if ( elem->second.isBool () )
					{
						ls->configValue.find ( name )->second = elem->second.operator const bool ();
					} else if ( elem->second.isString () )
					{
						ls->configValue.find ( name )->second = (elem->second).operator const stringi & ();
					} else
					{
						// invalid type;
					}
				}
			}
		}
	}
}

static jsonElement initialized ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{

	jsonElement confReq;
	confReq["items"].makeArray ();
	confReq["items"][0]["section"] = "slang";
	server.sendRequest ( "workspace/configuration", confReq, [ls, &server] ( int64_t id, jsonElement const &rsp ) { workspaceConfigCB ( id, rsp, server, ls ); } );

	return jsonElement ();
}

static jsonElement workspaceDidChangeWatchedFile ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement workspaceDidChangeConfiguration ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	BUFFER tmp;
	req.serialize ( tmp, true );

	workspaceConfigCB ( id, req["settings"]["slang"], server, ls );
	return jsonElement ();
}

static jsonElement textDocumentDidClose ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi uri = req["textDocument"]["uri"];

	auto uriFixed = unescapeJsonString ( uri.c_str () );
	auto fileName = uriToFileName ( uriFixed );

	auto it = ls->fileToTargetMap.find ( fileName );
	if ( it != ls->fileToTargetMap.end () )
	{
		auto lsFile = ls->targets[it->second].get ();

		lsFile->fileVersions[fileName] = std::pair{std::get<0> ( lsFile->fileVersions[fileName] ), false};
		bool inUse = false;
		for ( auto &data : lsFile->fileVersions )
		{
			inUse |= std::get<1> ( data.second );
		}
		if ( !inUse )
		{
			ls->targets.erase ( it->second );
			ls->fileToTargetMap.erase ( it );
			if ( ls->activeFile == lsFile )
			{
				ls->activeFile = nullptr;
			}
		}
	}
	return jsonElement ();
}

static jsonElement textDocumentDidOpen ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi uri = req["textDocument"]["uri"];
	stringi languageId = req["textDocument"]["languageId"];
	int64_t version = req["textDocument"]["version"];
	stringi text = req["textDocument"]["text"];

	languageServerFileData::languageType langId;

	if ( languageId == "fgl" )
	{
		langId = languageServerFileData::languageType::fgl;
	} else
	{
		langId = languageServerFileData::languageType::slang;
	}

	getFile ( server, ls, uri, id, text, version, langId );
	ls->activeFile = getFile ( server, ls, uri, id );

	return jsonElement ();
}

static jsonElement textDocumentDidChange ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t	version = req["textDocument"]["version"];

	auto uriFixed = unescapeJsonString ( uri.c_str () );
	auto fileName = uriToFileName ( uriFixed );

	auto &code = lsFileCache.getCode ( fileName );

	std::vector<std::pair<srcLocation, stringi const &>> changes;

	size_t nChanges = req["contentChanges"].size ();
	for ( size_t nChange = 0; nChange < nChanges; nChange++ )
	{
		jsonElement const &change = req["contentChanges"][nChange];

		if ( change.has ( "range" ) )
		{
			jsonElement const &start = change["range"]["start"];
			jsonElement const &end = change["range"]["end"];

			int64_t const startLine = start["line"];
			int64_t const startCharacter = start["character"];
			int64_t const endLine = end["line"];
			int64_t const endCharacter = end["character"];

			changes.push_back ( {srcLocation ( 0, (uint32_t) startCharacter, (uint32_t) startLine, (uint32_t) endCharacter, (uint32_t) endLine ), change["text"]} );
		} else
		{
			changes.push_back ( {srcLocation ( UINT32_MAX, (uint32_t) 0, (uint32_t) 0, (uint32_t) 0, (uint32_t) 0 ), change["text"]} );
		}
	}

	size_t				lastLine = 0;
	std::vector<size_t>	linePtr;
	linePtr.push_back ( 0 );

	// now apply changes from state to state
	for ( auto &[location, newCode] : changes )
	{
		if ( location.sourceIndex == UINT32_MAX )
		{
			// full update
			code = newCode;

			// reset parse points as we just did a full change
			lastLine = 1;
		} else
		{
			linePtr.reserve ( static_cast<size_t>(location.lineNumberEnd) + 1 );

			// parse line information starting from where we last left off (or the beginning if first time though), up to the last line we need.  Don't parse more than we need to
			auto ptr = code.c_str () + linePtr[lastLine];
			auto maxLine = location.lineNumberEnd;
			while ( *ptr && lastLine < maxLine )
			{
				if ( *ptr == '\r' )
				{
					ptr++;
					if ( !*ptr ) break;
					if ( *ptr == '\n' )
					{
						ptr++;
					}
					if ( linePtr.size () <= lastLine + 1 )
					{
						linePtr.push_back ( ptr - code.c_str () );
					} else
					{
						linePtr[lastLine + 1] = ptr - code.c_str ();
					}
					lastLine++;
				} else if ( *ptr == '\n' )
				{
					ptr++;
					if ( linePtr.size () <= lastLine + 1 )
					{
						linePtr.push_back ( ptr - code.c_str () );
					} else
					{
						linePtr[lastLine + 1] = ptr - code.c_str ();
					}
					lastLine++;
				} else
				{
					ptr++;
				}
			}

			auto startOffset = linePtr[location.lineNumberStart] + location.columnNumber;
			auto endOffset = linePtr[location.lineNumberEnd] + location.columnNumberEnd;

			auto amountToDelete = endOffset - startOffset;

			code.erase ( startOffset, amountToDelete );
			code.insert ( startOffset, newCode.operator std::string const &() );

			lastLine = location.lineNumberStart;
		}
	}
	//	printf ( "new fileVersion=%I64i\r\n", version );

	ls->activeFile = getFile ( server, ls, uri, id, code, version );
	return jsonElement ();
}

static jsonElement textDocumentHover ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t line = req["position"]["line"];
	int64_t character = req["position"]["character"];

	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		srcLocation src{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

		if ( !lsFile->info.semanticTokens.empty () )
		{
			astLSInfo::symbolRange sym{ accessorType (), astLSInfo::semanticSymbolType::type, src, std::monostate () };

			auto it = lsFile->info.semanticTokens.lower_bound ( sym );

			if ( !(it != lsFile->info.semanticTokens.end () && it->location.encloses ( character + 1, line + 1 )) )
			{
				if ( it != lsFile->info.semanticTokens.begin () ) it--;
			}

			if ( it != lsFile->info.semanticTokens.end () && it->location.encloses ( character + 1, line + 1 ) )
			{
				auto detail = getDetail ( &lsFile->file, it->data, it->isAccess );
				if ( detail.size () )
				{
					switch ( lsFile->file.getRegion ( src ) )
					{
						case languageRegion::languageId::fgl:
							rsp["contents"][0] = stringi ( "```fgl\n" ) + detail + "\n```";
							break;
						case languageRegion::languageId::slang:
						default:
							rsp["contents"][0] = stringi ( "```slang\n" ) + detail + "\n```";
							break;
					}
				} else
				{
					rsp["contents"][0] = "";
				}
				auto doc = getDocumentation ( &lsFile->file, it->data, it->isAccess );
				if ( doc.size () )
				{
					rsp["contents"].push_back ( doc );
				}
				rsp["range"] = range ( it->location );
			}
		}

		if ( !lsFile->info.signatureHelp.empty () )
		{
			astLSInfo::signatureHelpDef sym{ src, nullptr, nullptr };
			auto it = lsFile->info.signatureHelp.lower_bound ( sym );

			size_t paramNum = 0;

			if ( !(it != lsFile->info.signatureHelp.end () && it->location.encloses ( character + 1, line + 1 )) )
			{
				if ( it != lsFile->info.signatureHelp.begin () ) it--;
			}

			if ( it != lsFile->info.signatureHelp.end () && it->location.encloses ( character + 1, line + 1 ) )
			{
				for ( auto &pIt : it->node->pList ().paramRegion )
				{
					if ( pIt.encloses ( character + 1, line + 1 ) )
					{
						break;
					}
					paramNum++;
				}
				auto pSigs = it->func->getParameterHelp ();
				if ( paramNum < pSigs.size () )
				{
					rsp["contents"].push_back ( std::get<1> ( pSigs[paramNum] ) );
				}
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentInlayHint ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];

	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

//	rsp["inlayHint"].makeArray ();

	if ( lsFile && std::get<bool> ( ls->configFlags.inlayHints ) )
	{
		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );

		jsonElement const &start = req["range"]["start"];
		jsonElement const &end = req["range"]["end"];

		int64_t startLine = start["line"];
		int64_t startCharacter = start["character"];
		int64_t endLine = end["line"];
		int64_t endCharacter = end["character"];

		srcLocation loc ( sourceIndex, (uint32_t) startCharacter + 1, (uint32_t) startLine + 1, (uint32_t) endCharacter + 1, (uint32_t) endLine + 1 );

		// parameter inlay hints
		if ( std::get<bool>(ls->configFlags.inlayHintsParameterNames) )
		{
			if ( !lsFile->info.signatureHelp.empty () )
			{
				astLSInfo::signatureHelpDef sym{ loc, nullptr, nullptr };

				auto it = lsFile->info.signatureHelp.lower_bound ( sym );

				if ( !lsFile->info.signatureHelp.empty () )
				{
					while ( (it != lsFile->info.signatureHelp.end ()) && loc.encloses ( it->location ) )
					{
						size_t paramNum = 0;
						auto pSigs = it->func->getSlimParameterHelp ();

						// we start at 0.  There are no hidden parameters as this is removed by the getSlipParameterHelp call
						for ( auto index = 0; index < it->node->pList().paramRegion.size(); index++ )
						{
							auto const &pIt = it->node->pList ().paramRegion[index];

							if ( loc.encloses ( pIt ) )
							{
								if ( paramNum < pSigs.size () )
								{
									auto const &[label, documentation, location] = pSigs[paramNum];

									jsonElement inlayHint;

									inlayHint["position"]["line"] = pIt.lineNumberStart - 1;
									inlayHint["position"]["character"] = pIt.columnNumber - 1;
									if ( location.sourceIndex )
									{
										jsonElement tmp;
										tmp["value"] = label + ":";
										tmp["tooltip"] = documentation;
										tmp["location"]["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( location.sourceIndex ) );
										tmp["location"]["range"] = range ( location );
										inlayHint["label"][0] = std::move ( tmp );
									} else
									{
										inlayHint["label"] = label + ":";
									}
									inlayHint["kind"] = 2;
									rsp.push_back ( std::move ( inlayHint ) );
								}
							}
							paramNum++;
						}
						it++;
					}
				}
			}
		}

		// type inlay hints
		if ( !lsFile->info.semanticTokens.empty () )
		{
			astLSInfo::symbolRange sym{ accessorType (), astLSInfo::semanticSymbolType::type, loc, std::monostate () };

			auto it = lsFile->info.semanticTokens.lower_bound ( sym );

			if ( !(it != lsFile->info.semanticTokens.end () && loc.encloses ( it->location )) )
			{
				if ( it != lsFile->info.semanticTokens.begin () ) it--;
			}

			while ( it != lsFile->info.semanticTokens.end () && loc.encloses ( it->location ) )
			{
				jsonElement inlayHint;

				if ( !it->isInFunctionCall )
				{
					auto doc = getDocumentation ( &lsFile->file, it->data, it->isAccess );

					switch ( it->type )
					{
						case astLSInfo::semanticSymbolType::variable:
						case astLSInfo::semanticSymbolType::field:
							if ( std::get<bool> ( ls->configFlags.inlayHintsVariableTypes) )
							{
								if ( !it->location.empty () )
								{
									if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes) )
									{
										inlayHint["position"]["line"] = it->location.lineNumberEnd - 1;
										inlayHint["position"]["character"] = it->location.columnNumberEnd - 1;
									} else
									{
										inlayHint["position"]["line"] = it->location.lineNumberStart - 1;
										inlayHint["position"]["character"] = it->location.columnNumber - 1;
									}
									inlayHint["kind"] = 1;

									if ( std::holds_alternative<symbol *> ( it->data ) )
									{
										if ( getSymbolSourceLocation ( it->data, std::get<symbol *> ( it->data )->getSymbolName () ).sourceIndex )
										{
											jsonElement tmp;
											if ( doc.size () )
											{
												tmp["tooltip"] = doc;
											}
											tmp["location"]["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( getSymbolSourceLocation ( it->data, std::get<symbol *> ( it->data )->getSymbolName () ).sourceIndex ) );
											tmp["location"]["range"] = range ( getSymbolSourceLocation ( it->data, std::get<symbol *> ( it->data )->getSymbolName () ) );
											if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes ) )
											{
												tmp["value"] = stringi ( ":" ) + it->compType.operator stringi();
											} else
											{
												tmp["value"] = (stringi) it->compType.operator stringi() + ":";
											}
											inlayHint["label"][0] = std::move ( tmp );
										} else
										{
											if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes ) )
											{
												inlayHint["label"] = stringi ( ":" ) + it->compType.operator stringi();
											} else
											{
												inlayHint["label"] = (stringi) it->compType.operator stringi() + ":";
											}
										}
									} else if ( std::holds_alternative<opSymbol *> ( it->data ) )
									{
										if ( getSymbolSourceLocation ( it->data, std::get<opSymbol *> ( it->data )->name ).sourceIndex )
										{
											jsonElement tmp;
											if ( doc.size () )
											{
												tmp["tooltip"] = doc;
											}
											tmp["location"]["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( getSymbolSourceLocation ( it->data, std::get<opSymbol *> ( it->data )->name ).sourceIndex ) );
											tmp["location"]["range"] = range ( getSymbolSourceLocation ( it->data, std::get<opSymbol *> ( it->data )->name ) );
											if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes ) )
											{
												tmp["value"] = stringi ( ":" ) + it->compType.operator stringi();
											} else
											{
												tmp["value"] = (stringi) it->compType.operator stringi() + ":";
											}
											inlayHint["label"][0] = std::move ( tmp );
										} else
										{
											if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes ) )
											{
												inlayHint["label"] = stringi ( ":" ) + it->compType.operator stringi();
											} else
											{
												inlayHint["label"] = (stringi) it->compType.operator stringi() + ":";
											}
										}
									}
									rsp.push_back ( std::move ( inlayHint ) );
								}
							}
							break;
						case astLSInfo::semanticSymbolType::ivar:
							if ( std::get<bool> ( ls->configFlags.inlayHintsVariableTypes ) )
							{
								if ( !it->location.empty () )
								{
									if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes ) )
									{
										inlayHint["position"]["line"] = it->location.lineNumberEnd - 1;
										inlayHint["position"]["character"] = it->location.columnNumberEnd - 1;
									} else
									{
										inlayHint["position"]["line"] = it->location.lineNumberStart - 1;
										inlayHint["position"]["character"] = it->location.columnNumber - 1;
									}
									inlayHint["kind"] = 1;

									if ( std::holds_alternative<opClassElement *>( it->data ) )
									{
										if ( getSymbolSourceLocation ( it->data, std::get<opClassElement *> ( it->data )->getSymbolName () ).sourceIndex )
										{
											jsonElement tmp;
											if ( doc.size () )
											{
												tmp["tooltip"] = doc;
											}
											tmp["location"]["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( getSymbolSourceLocation ( it->data, std::get<opClassElement *> ( it->data )->getSymbolName () ).sourceIndex ) );
											tmp["location"]["range"] = range ( getSymbolSourceLocation ( it->data, std::get<opClassElement *> ( it->data )->getSymbolName () ) );
											if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes ) )
											{
												tmp["value"] = stringi ( ":" ) + it->compType.operator stringi();
											} else
											{
												tmp["value"] = (stringi) it->compType.operator stringi() + ":";
											}
											inlayHint["label"][0] = std::move ( tmp );
										} else
										{
											if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes ) )
											{
												inlayHint["label"] = stringi ( ":" ) + it->compType.operator stringi();
											} else
											{
												inlayHint["label"] = (stringi) it->compType.operator stringi() + ":";
											}
										}
										rsp.push_back ( std::move ( inlayHint ) );
									}
								}
							}
							break;
						case astLSInfo::semanticSymbolType::parameter:
							if ( std::get<bool> ( ls->configFlags.inlayHintsVariableTypes) )
							{
								if ( !it->location.empty () )
								{
									if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes) )
									{
										inlayHint["position"]["line"] = it->location.lineNumberEnd - 1;
										inlayHint["position"]["character"] = it->location.columnNumberEnd - 1;
									} else
									{
										inlayHint["position"]["line"] = it->location.lineNumberStart - 1;
										inlayHint["position"]["character"] = it->location.columnNumber - 1;
									}
									inlayHint["kind"] = 1;

									if ( getSymbolSourceLocation ( it->data, std::get<symbolParamDef *> ( it->data )->name ).sourceIndex )
									{
										jsonElement tmp;
										if ( doc.size () )
										{
											tmp["tooltip"] = doc;
										}
										tmp["location"]["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( getSymbolSourceLocation ( it->data, std::get<symbolParamDef *> ( it->data )->name ).sourceIndex ) );
										tmp["location"]["range"] = range ( getSymbolSourceLocation ( it->data, std::get<symbolParamDef *> ( it->data )->name ) );
										if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes) )
										{
											tmp["value"] = stringi ( ":" ) + it->compType.operator stringi();
										} else
										{
											tmp["value"] = (stringi) it->compType.operator stringi() + ":";
										}
										inlayHint["label"][0] = std::move ( tmp );
									} else
									{
										if ( std::get<bool> ( ls->configFlags.inlayHintsTrailingTypes) )
										{
											inlayHint["label"] = stringi ( ":" ) + it->compType.operator stringi();
										} else
										{
											inlayHint["label"] = (stringi) it->compType.operator stringi() + ":";
										}
									}
									rsp.push_back ( std::move ( inlayHint ) );
								}
							}
							break;
						case astLSInfo::semanticSymbolType::function:
						case astLSInfo::semanticSymbolType::iterator:
						case astLSInfo::semanticSymbolType::method:
						case astLSInfo::semanticSymbolType::methodIterator:
							if ( std::get<bool> ( ls->configFlags.inlayHintsFunctionReturnType) && !it->noDisplayReturnType )
							{
								if ( !it->location.empty () && std::holds_alternative<opFunction *> ( it->data ) && !std::get<opFunction *> ( it->data )->isAutoMain )
								{
									inlayHint["position"]["line"] = it->location.lineNumberStart - 1;
									inlayHint["position"]["character"] = it->location.columnNumber - 1;
									inlayHint["kind"] = 1;

									if ( getSymbolSourceLocation ( it->data, std::get<opFunction *> ( it->data )->name ).sourceIndex == 0 )
									{
										if ( it->isInFunctionCall )
										{
											inlayHint["label"] = it->compType.operator stringi() + ":";
										} else
										{
											inlayHint["label"] = stringi ( ":" ) + it->compType.operator stringi();
										}
										inlayHint["label"] = (stringi)it->compType.operator stringi() + ":";
									} else
									{
										jsonElement tmp;
										if ( doc.size () )
										{
											tmp["tooltip"] = doc;
										}
										tmp["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( getSymbolSourceLocation ( it->data, std::get<opFunction *> ( it->data )->name ).sourceIndex ) );
										tmp["range"] = range ( getSymbolSourceLocation ( it->data, std::get<opFunction *> ( it->data )->name ) );
										if ( it->isInFunctionCall )
										{
											tmp["value"] = (stringi)it->compType.operator stringi() + ":";
										} else
										{
											tmp["value"] = stringi ( ":" ) + it->compType.operator stringi();
										}
										inlayHint["label"][0] = std::move ( tmp );
									}
									rsp.push_back ( std::move ( inlayHint ) );
								}
							}
							break;
						default:
							break;
					}
				}
				it++;
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentSignatureHelp ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t line = req["position"]["line"];
	int64_t character = req["position"]["character"];

	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		srcLocation src{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

		astLSInfo::signatureHelpDef sym{ src, nullptr, nullptr };

		auto it = lsFile->info.signatureHelp.lower_bound ( sym );

		size_t paramNum = 0;

		if ( !lsFile->info.signatureHelp.empty () )
		{
			if ( !(it != lsFile->info.signatureHelp.end () && it->location.encloses ( character + 1, line + 1 )) )
			{
				if ( it != lsFile->info.signatureHelp.begin () ) it--;
			}

			if ( it != lsFile->info.signatureHelp.end () && it->location.encloses ( character + 1, line + 1 ) )
			{
				if ( !it->node->pList ().param.size () )
				{
					rsp["activeParameter"] = 0;
				}
				for ( auto &pIt : it->node->pList ().paramRegion )
				{
					if ( pIt.encloses ( character + 1, line + 1 ) )
					{
						rsp["activeParameter"] = paramNum;
						break;
					}
					paramNum++;
				}
				size_t index = 0;
				auto pSigs = it->func->getParameterHelp ();
				rsp["signatures"][0]["label"] = it->func->getSimpleFunctionSignature ();
				rsp["signatures"][0]["documentation"] = it->func->getDocumentation ();
				for ( auto &[label, documentation, location] : pSigs )
				{
					rsp["signatures"][0]["parameters"][index]["label"] = label;
					rsp["signatures"][0]["parameters"][index++]["documentation"] = documentation;
				}
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentCompletion ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t line = req["position"]["line"];
	int64_t character = req["position"]["character"];
	jsonElement rsp;

	auto lsFile = getFile ( server, ls, uri, id );

	if ( lsFile )
	{
		size_t index = 0;
		rsp["isIncomplete"] = false;
		rsp["items"] = jsonElement ().makeArray ();

		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		srcLocation src{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

		astLSInfo::objectCompletions sym{ src, symVariantType };

		auto it = lsFile->info.objCompletions.lower_bound ( sym );

		if ( !(it != lsFile->info.objCompletions.end () && it->location.encloses ( character, line + 1 )) && it != lsFile->info.objCompletions.begin () )
		{
			it--;
		}

		if ( it != lsFile->info.objCompletions.end () && it->location.encloses ( character, line + 1 ) )
		{
			if ( it->type.isAnObject () && it->type.hasClass () )
			{
				auto cls = lsFile->file.classList.find ( it->type.getClass () )->second;
				std::set<stringi>	completions;
				for ( auto &elemIt : cls->cClass->elements )
				{
					switch ( elemIt.type )
					{
						case fgxClassElementType::fgxClassType_prop:
							rsp["items"][index]["kind"] = languageServerFile::CompletionItemKind::Property;	// property
							rsp["items"][index]["label"] = elemIt.name;
							rsp["items"][index]["detail"] = getElementSignature ( &elemIt );
							index++;
							break;
						case fgxClassElementType::fgxClassType_method:
							rsp["items"][index]["kind"] = languageServerFile::CompletionItemKind::Method;	// method
							rsp["items"][index]["label"] = elemIt.name;
							rsp["items"][index]["detail"] = getElementSignature ( &elemIt );
							index++;
							break;
						case fgxClassElementType::fgxClassType_iVar:
						case fgxClassElementType::fgxClassType_static:
							rsp["items"][index]["kind"] = languageServerFile::CompletionItemKind::Field;	// field
							rsp["items"][index]["label"] = elemIt.name;
							rsp["items"][index]["detail"] = getElementSignature ( &elemIt );
							index++;
							break;
						case fgxClassElementType::fgxClassType_const:
							rsp["items"][index]["kind"] = languageServerFile::CompletionItemKind::Constant;	// constant
							rsp["items"][index]["label"] = elemIt.name;
							rsp["items"][index]["detail"] = getElementSignature ( &elemIt );
							index++;
							break;
						default:
							break;
					}
				}
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentDocumentSymbol ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	auto lsFile = getFile ( server, ls, uri, id );

	if ( !lsFile )
	{
		return jsonElement ();
	}
	size_t index = 0;
	jsonElement rsp;

	auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );

	std::map<cacheString, std::pair<opFunction *, jsonElement>>	funcSymbolMap;

	for ( auto const &it : lsFile->info.symbolDefinitions )
	{
		if ( it.location.sourceIndex == sourceIndex )
		{
			auto func = it.func;
			if ( func )
			{
				auto &funcSyms = funcSymbolMap[func->name];

				if ( !std::get<0> ( funcSyms ) )
				{
					funcSyms = {func, jsonElement ().makeArray ()};
				}
				auto &location = it.location;

				jsonElement tmp = {{ "name", it.name },
									{ "kind", languageServerFile::SymbolKind::Variable }, /* variable */
									{ "range", range ( location ) },
									{ "selectionRange", range ( location ) },
									{ "detail", it.type.operator stringi() }};

				std::get<1> ( funcSyms ).push_back ( std::move ( tmp ) );
			} else
			{
				jsonElement tmp = {{ "name", it.name },
									{ "kind", languageServerFile::SymbolKind::Class }, /* variable */
									{ "range", range ( it.location ) },
									{ "selectionRange", range ( it.location ) },
									{ "detail", it.type.operator stringi() }};

				rsp.push_back ( std::move ( tmp ) );
			}
		}
	}

	for ( auto &it : lsFile->file.classList )
	{
		if ( it.second->nameLocation.sourceIndex == sourceIndex )
		{
			rsp[index]["name"] = it.second->name;
			rsp[index]["kind"] = languageServerFile::SymbolKind::Class; /* class */
			rsp[index]["range"] = range ( it.second->location );
			rsp[index]["selectionRange"] = range ( it.second->nameLocation );

			jsonElement children;
			children.makeArray ();
			for ( auto &elem : it.second->elems )
			{
				if ( elem->location.sourceIndex == sourceIndex )
				{
					jsonElement tmp;
					switch ( elem->type )
					{
						case fgxClassElementType::fgxClassType_method:
							{
								auto func = lsFile->file.functionList.find ( elem->data.method.func )->second;
								if ( func->nameLocation.sourceIndex == sourceIndex )
								{
									tmp["name"] = elem->name;
									if ( elem->name == lsFile->file.newValue )
									{
										tmp["kind"] = languageServerFile::SymbolKind::Constructor; /* constructor */
									} else
									{
										tmp["kind"] = languageServerFile::SymbolKind::Method; /* method */
									}
									tmp["range"] = range ( func->location );
									tmp["selectionRange"] = range ( func->nameLocation );
									tmp["detail"] = func->getSimpleFunctionSignature ();
									if ( funcSymbolMap.find ( elem->data.method.func ) != funcSymbolMap.end () )
									{
										tmp["children"] = std::get<1> ( funcSymbolMap.find ( elem->data.method.func )->second );
										funcSymbolMap.erase ( funcSymbolMap.find ( elem->data.method.func ) );
									}
									children[children.size ()] = std::move ( tmp );
								}
							}
							break;
						case fgxClassElementType::fgxClassType_iVar:
						case fgxClassElementType::fgxClassType_static:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Field; /* Field  */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = std::move ( tmp );
							break;
						case fgxClassElementType::fgxClassType_prop:
							if ( elem->data.prop.access )
							{
								auto func = lsFile->file.functionList.find ( elem->data.prop.access )->second;

								tmp["name"] = elem->name;
								tmp["kind"] = languageServerFile::SymbolKind::Property; /* Property   */
								tmp["range"] = range ( func->location );
								tmp["selectionRange"] = range ( func->nameLocation );
								tmp["detail"] = func->getSimpleFunctionSignature ();

								if ( funcSymbolMap.find ( elem->data.prop.access ) != funcSymbolMap.end () )
								{
									tmp["children"] = std::get<1> ( funcSymbolMap.find ( elem->data.prop.access )->second );
									funcSymbolMap.erase ( funcSymbolMap.find ( elem->data.prop.access ) );
								}
								children[children.size ()] = std::move ( tmp );
							}
							if ( elem->data.prop.assign )
							{
								auto func = lsFile->file.functionList.find ( elem->data.prop.assign )->second;

								tmp["name"] = elem->name;
								tmp["kind"] = languageServerFile::SymbolKind::Property; /* Property   */
								tmp["range"] = range ( func->location );
								tmp["selectionRange"] = range ( func->nameLocation );
								tmp["detail"] = func->getSimpleFunctionSignature ();
								if ( funcSymbolMap.find ( elem->data.prop.assign ) != funcSymbolMap.end () )
								{
									tmp["children"] = std::get<1> ( funcSymbolMap.find ( elem->data.prop.assign )->second );
									funcSymbolMap.erase ( funcSymbolMap.find ( elem->data.prop.assign ) );
								}
								children[children.size ()] = std::move ( tmp );
							}
							break;
						case fgxClassElementType::fgxClassType_const:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Constant; /* Constant    */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = std::move ( tmp );
							break;
						case fgxClassElementType::fgxClassType_inherit:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Class; /* class    */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = std::move ( tmp );
							break;
						case fgxClassElementType::fgxClassType_message:
							break;
					}
				}
			}
			rsp[index]["children"] = std::move ( children );
			index++;
		}
	}

	for ( auto &it : funcSymbolMap )
	{
		if ( std::get<0> ( it.second )->nameLocation.sourceIndex == sourceIndex )
		{
			rsp[index]["name"] = it.first;
			rsp[index]["kind"] = languageServerFile::SymbolKind::Function; /* function */
			rsp[index]["selectionRange"] = range ( std::get<0> ( it.second )->nameLocation );
			rsp[index]["range"] = range ( std::get<0> ( it.second )->location );
			rsp[index]["detail"] = std::get<0> ( it.second )->getSimpleFunctionSignature ();
			rsp[index++]["children"] = std::get<1> ( it.second );
		}
	}

	return rsp;
}

static void addSymbolToRsp ( languageServerFile *lsFile, jsonElement &rsp, const stringi &name, languageServerFile::SymbolKind kind, const srcLocation &location, const stringi &containerName )
{
	jsonElement tmp;

	if ( containerName.size() )
	{
		tmp = {{ "name", name },
				{ "kind", kind },
				{ "containerName", containerName },
				{ "location", { { "uri", fileNameToUri ( lsFile->file.srcFiles.getName ( location.sourceIndex ) ) },
								{ "range", range ( location ) } }
				}};
	} else
	{
		tmp = {{ "name", name },
				{ "kind", kind },
				{ "location", { { "uri", fileNameToUri ( lsFile->file.srcFiles.getName ( location.sourceIndex ) ) },
								{ "range", range ( location ) } }
				}};
	}
	rsp.push_back ( std::move ( tmp ) );
}

static jsonElement workspaceSymbol ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const query = req["query"];

	size_t index = 0;
	jsonElement rsp;

	for ( auto &[name, lsFilePtr] : ls->targets )
	{
		auto lsFile = lsFilePtr.get ();

		std::map<cacheString, std::pair<opFunction *, jsonElement>>	funcSymbolMap;

		for ( auto const &it : lsFile->info.symbolDefinitions )
		{
			if ( lsFile->file.srcFiles.getFileType ( it.location.sourceIndex ) == sourceFile::sourceFileType::external )
			{
				if ( !memcmpi ( query.c_str (), it.name.c_str (), query.size () ) )
				{
					auto func = it.func;
					if ( func )
					{
						auto &funcSyms = funcSymbolMap[func->name];

						if ( !std::get<0> ( funcSyms ) )
						{
							funcSyms = {func, jsonElement ().makeArray ()};
						}
						auto &location = it.location;

						addSymbolToRsp ( lsFile, rsp, it.name, languageServerFile::SymbolKind::Function, location, func->name );
					} else
					{
						addSymbolToRsp ( lsFile, rsp, it.name, languageServerFile::SymbolKind::Variable, it.location, "" );
					}
				}
			}
		}

		for ( auto &it : lsFile->file.classList )
		{
			if ( lsFile->file.srcFiles.getFileType ( it.second->location.sourceIndex ) == sourceFile::sourceFileType::external )
			{
				if ( !memcmpi ( query.c_str (), it.second->name.c_str (), query.size () ) )
				{
					addSymbolToRsp ( lsFile, rsp, it.second->name, languageServerFile::SymbolKind::Class, it.second->location, "" );
				}

				jsonElement children;
				children.makeArray ();
				for ( auto &elem : it.second->elems )
				{
					if ( lsFile->file.srcFiles.getFileType ( elem->location.sourceIndex ) == sourceFile::sourceFileType::external )
					{
						if ( !memcmpi ( query.c_str (), elem->name.c_str (), query.size () ) )
						{
							switch ( elem->type )
							{
								case fgxClassElementType::fgxClassType_method:
									{
										auto func = lsFile->file.functionList.find ( elem->data.method.func )->second;

										if ( elem->name == lsFile->file.newValue )
										{
											addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Constructor, func->location, it.second->name );
										} else
										{
											addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Method, func->location, it.second->name );
										}
									}
									break;
								case fgxClassElementType::fgxClassType_iVar:
								case fgxClassElementType::fgxClassType_static:
									addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Field, elem->location, it.second->name );
									break;
								case fgxClassElementType::fgxClassType_prop:
									if ( elem->data.prop.access )
									{
										auto func = lsFile->file.functionList.find ( elem->data.prop.access )->second;

										addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Property, func->location, it.second->name );
									}
									if ( elem->data.prop.assign )
									{
										auto func = lsFile->file.functionList.find ( elem->data.prop.assign )->second;

										addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Property, func->location, it.second->name );
									}
									break;
								case fgxClassElementType::fgxClassType_const:
									addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Constant, elem->location, it.second->name );
									break;
								case fgxClassElementType::fgxClassType_inherit:
									addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Class, elem->location, it.second->name );
									addSymbolToRsp ( lsFile, children, elem->name, languageServerFile::SymbolKind::Class, elem->location, it.second->name );
									break;
								case fgxClassElementType::fgxClassType_message:
									addSymbolToRsp ( lsFile, rsp, elem->name, languageServerFile::SymbolKind::Interface, elem->location, it.second->name );
									addSymbolToRsp ( lsFile, children, elem->name, languageServerFile::SymbolKind::Interface, elem->location, it.second->name );
									break;
							}
						}
					}
				}
			}
		}
		for ( auto &it : lsFile->file.symbols )
		{
			if ( it.second.isExportable )
			{
				if ( lsFile->file.srcFiles.getFileType ( it.second.location.sourceIndex ) == sourceFile::sourceFileType::external )
				{
					addSymbolToRsp ( lsFile, rsp, it.second.name, languageServerFile::SymbolKind::Variable, it.second.location, it.second.enclosingName ? it.second.enclosingName : "" );
				}
			}
		}
	}
	return rsp;
}

static jsonElement shutdown ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement exit ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement setTrace ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement logTrace ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement textDocumentFoldingRange ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;
	stringi uri = req["textDocument"]["uri"];
	auto lsFile = getFile ( server, ls, uri, id );

	if ( lsFile )
	{
		size_t index = 0;
		for ( auto const &it : lsFile->info.foldingRanges )
		{
			rsp[index]["startLine"] = it.lineNumberStart - 1;
			rsp[index]["endLine"] = it.lineNumberEnd - 1;
			rsp[index]["kind"] = "region";
			index++;
		}
	} else
	{
		rsp = jsonElement ().makeArray ();
	}
	return rsp;
}

static jsonElement textDocumentSemanticTokensRange ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		int64_t lastLine = 1;
		int64_t lastColumn = 1;
		auto currentSourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );

		jsonElement const &start = req["range"]["start"];
		jsonElement const &end = req["range"]["end"];

		int64_t startLine = start["line"];
		int64_t endLine = end["line"];

		jsonElement arrValues;

		arrValues.makeArray ();
		arrValues.reserve ( lsFile->info.semanticTokens.size () * 5 );

		for ( auto const &it : lsFile->info.semanticTokens )
		{
			if ( it.location.sourceIndex == currentSourceIndex )
			{
				if ( it.location.lineNumberStart >= startLine && it.location.lineNumberStart <= endLine )
				{
					if ( it.location.lineNumberStart == it.location.lineNumberEnd )
					{
						// line
						arrValues.emplace_back ( it.location.lineNumberStart - lastLine );
						// character
						if ( lastLine == it.location.lineNumberStart )
						{
							arrValues.emplace_back ( it.location.columnNumber - lastColumn );
						} else
						{
							arrValues.emplace_back ( it.location.columnNumber - 1 );
						}
						// length
						arrValues.emplace_back ( it.location.columnNumberEnd - it.location.columnNumber );
						// type
						arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
						// modifier
						arrValues.emplace_back ( (int64_t) 0 );

						lastLine = it.location.lineNumberStart;
						lastColumn = it.location.columnNumber;
					} else
					{
						/***************** start of multi-line token *****************/
						// line
						arrValues.emplace_back ( it.location.lineNumberStart - lastLine );
						// character
						if ( lastLine == it.location.lineNumberStart )
						{
							arrValues.emplace_back ( it.location.columnNumber - lastColumn );
						} else
						{
							arrValues.emplace_back ( it.location.columnNumber - 1 );
						}
						// length
						arrValues.emplace_back ( 65536 );		// indicator for multi-line
						// type
						arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
						// modifier
						arrValues.emplace_back ( (int64_t) 0 );

						for ( uint32_t loop = it.location.lineNumberStart; loop < it.location.lineNumberEnd - 1; loop++ )
						{
							// line
							arrValues.emplace_back ( 1 );
							// column
							arrValues.emplace_back ( 0 );
							// length
							arrValues.emplace_back ( 65536 );	// indicator for multi-line
							// type
							arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
							// modifier
							arrValues.emplace_back ( (int64_t) 0 );
						}

						/***************** end of multi-line token *****************/
						// line
						arrValues.emplace_back ( 1 );
						// character
						arrValues.emplace_back ( 0 );
						// length
						arrValues.emplace_back ( it.location.columnNumberEnd );
						// type
						arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
						// modifier
						arrValues.emplace_back ( (int64_t) 0 );

						lastLine = it.location.lineNumberEnd;
						lastColumn = it.location.columnNumber;
					}
				}
			}
		}
		rsp["data"] = std::move ( arrValues );
	} else
	{
		rsp.makeArray ();
	}
	return rsp;
}

static jsonElement textDocumentSemanticTokens ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		int64_t lastLine = 1;
		int64_t lastColumn = 1;
		auto currentSourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );

		jsonElement arrValues;

		arrValues.makeArray ();
		arrValues.reserve ( lsFile->info.semanticTokens.size () * 5 );

		for ( auto const &it : lsFile->info.semanticTokens )
		{
			if ( it.location.sourceIndex == currentSourceIndex )
			{
				if ( it.location.lineNumberStart == it.location.lineNumberEnd )
				{
					// line
					arrValues.emplace_back ( it.location.lineNumberStart - lastLine );
					// character
					if ( lastLine == it.location.lineNumberStart )
					{
						arrValues.emplace_back ( it.location.columnNumber - lastColumn );
					} else
					{
						arrValues.emplace_back ( it.location.columnNumber - 1 );
					}
					// length
					arrValues.emplace_back ( it.location.columnNumberEnd - it.location.columnNumber );
					// type
					arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
					// modifier
					arrValues.emplace_back ( (int64_t) 0 );

					lastLine = it.location.lineNumberStart;
					lastColumn = it.location.columnNumber;
				} else
				{
					/***************** start of multi-line token *****************/
					// line
					arrValues.emplace_back ( it.location.lineNumberStart - lastLine );
					// character
					if ( lastLine == it.location.lineNumberStart )
					{
						arrValues.emplace_back ( it.location.columnNumber - lastColumn );
					} else
					{
						arrValues.emplace_back ( it.location.columnNumber - 1 );
					}
					// length
					arrValues.emplace_back ( 65536 );		// indicator for multi-line
					// type
					arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
					// modifier
					arrValues.emplace_back ( (int64_t) 0 );

					for ( uint32_t loop = it.location.lineNumberStart; loop < it.location.lineNumberEnd - 1; loop++ )
					{
						// line
						arrValues.emplace_back ( 1 );
						// column
						arrValues.emplace_back ( 0 );
						// length
						arrValues.emplace_back ( 65536 );	// indicator for multi-line
						// type
						arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
						// modifier
						arrValues.emplace_back ( (int64_t) 0 );
					}

					/***************** end of multi-line token *****************/
					// line
					arrValues.emplace_back ( 1 );
					// character
					arrValues.emplace_back ( 0 );
					// length
					arrValues.emplace_back ( it.location.columnNumberEnd );
					// type
					arrValues.emplace_back ( astLSInfo::mappedValues[(int64_t) it.type] );
					// modifier
					arrValues.emplace_back ( (int64_t) 0 );

					lastLine = it.location.lineNumberEnd;
					lastColumn = it.location.columnNumber;
				}
			}
		}
		rsp["data"] = std::move ( arrValues );
	} else
	{
		rsp.makeArray ();
	}
	return rsp;
}

static void extendLocation ( srcLocation &origLocation, srcLocation const &childLocation )
{
	if ( childLocation.lineNumberStart < origLocation.lineNumberStart )
	{
		origLocation.lineNumberStart = childLocation.lineNumberStart;
		origLocation.columnNumber = childLocation.columnNumber;
	} else if ( childLocation.lineNumberStart == origLocation.lineNumberStart && childLocation.columnNumber < origLocation.columnNumber )
	{
		origLocation.columnNumber = childLocation.columnNumber;
	}

	if ( childLocation.lineNumberEnd > origLocation.lineNumberEnd )
	{
		origLocation.lineNumberEnd = childLocation.lineNumberEnd;
		origLocation.columnNumberEnd = childLocation.columnNumberEnd;
	} else if ( childLocation.lineNumberEnd == origLocation.lineNumberEnd && childLocation.columnNumber > origLocation.columnNumber )
	{
		origLocation.columnNumberEnd = childLocation.columnNumberEnd;
	}
}

static astNode *computeRangedNodesCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, accessorType const &acc )
{
	if ( parent )
	{
		if ( parent->getExtendedSelection ().sourceIndex == node->location.sourceIndex && node->location < parent->getExtendedSelection () )
		{
			extendLocation ( parent->getExtendedSelection (), node->location );
		}
	}
	return node;
}

static astNode *getRangedNodesCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, accessorType const &acc, std::vector<srcLocation> &locations, srcLocation const &target )
{
	if ( node->getExtendedSelection ().encloses ( target ) )
	{
		locations.push_back ( node->getExtendedSelection () );
	}
	return node;
}

static jsonElement textDocumentSelectionRange ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile && !lsFile->info.semanticTokens.empty () )
	{
		auto &positions = req["positions"];

		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );

		for ( auto pos = positions.cbeginArray (); pos != positions.cendArray (); pos++ )
		{
			int64_t line = (*pos)["line"];
			int64_t character = (*pos)["character"];

			srcLocation src{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

			astLSInfo::symbolRange sym{ accessorType (), astLSInfo::semanticSymbolType::type, src, std::monostate () };

			rsp.makeArray ();

			jsonElement selectionRange;

			for ( auto &it : lsFile->file.functionList )
			{
				auto func = it.second;

				if ( func->location.sourceIndex == sourceIndex && func->location.encloses ( src ) )
				{
					std::vector<srcLocation>	locations;

					// whole file
					srcLocation fileSrc = lsFile->file.languageRegions.begin ()->first;
					fileSrc.setEnd ( std::prev ( lsFile->file.languageRegions.end () )->first );
//					locations.push_back ( fileSrc );

					// class definition
					if ( func->classDef )
					{
//						locations.push_back ( func->classDef->location );
					}

					astNodeWalkTrailing ( func->codeBlock, nullptr, computeRangedNodesCB, accessorType{} );
					astNodeWalk ( func->codeBlock, nullptr, getRangedNodesCB, accessorType{}, locations, src );

					// whole function
					extendLocation ( func->extendedSelection, *locations.begin () );
					locations.insert ( locations.begin (), (func->extendedSelection) );


					for ( auto it = locations.begin (); it != locations.end (); it++ )
					{
						if ( it != locations.begin () )
						{
							selectionRange["parent"] = jsonElement ( selectionRange );
						}
						selectionRange["range"] = range ( *it );
					}
					break;
				}
			}
			rsp.push_back ( selectionRange );
		}
	}
	return rsp;
}

static jsonElement textDocumentRangeFormatting ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;
	stringi const uri = req["textDocument"]["uri"];
	int64_t const tabWidth = req["options"]["tabSize"];
	bool const preferSpaces = req["options"]["insertSpaces"];

	rsp.makeArray ();
	auto lsFile = getFile ( server, ls, uri, id );

	if ( lsFile )
	{
		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		char const *src = lsFileCache.getCode ( lsFile->activeFile ).c_str ();

		jsonElement const &start = req["range"]["start"];
		jsonElement const &end = req["range"]["end"];

		int64_t startLine = start["line"];
		int64_t startCharacter = start["character"];
		int64_t endLine = end["line"];
		int64_t endCharacter = end["character"];

		srcLocation loc ( sourceIndex, (uint32_t) startCharacter + 1, (uint32_t) startLine + 1, (uint32_t) endCharacter + 1, (uint32_t) endLine + 1 );

		auto regions = lsFile->file.getRegionsExcept ( loc, languageRegion::languageId::html );

		int64_t indent = -1;
		if ( lsFile->file.languageRegions.begin () != lsFile->file.languageRegions.end () )
		{
			if ( lsFile->file.languageRegions.begin ()->first.encloses ( loc ) )
			{
				if ( lsFile->file.languageRegions.begin ()->second == languageRegion::languageId::html )
				{
					indent = 1ull + 4;
				}
			}
		}
		for ( auto const &[reg, languageId] : regions )
		{
			jsonElement	formattedSection;

			auto [newText, nextIndent, formatLoc] = formatCode ( &lsFile->file, lsFile->info, src, reg, indent, languageId, tabWidth, preferSpaces, ls->formatFlags );
			formattedSection["newText"] = newText;
			formattedSection["range"] = range ( {sourceIndex, formatLoc.columnNumber, formatLoc.lineNumberStart, formatLoc.columnNumberEnd, formatLoc.lineNumberEnd} );
			indent = nextIndent;

			rsp.push_back ( formattedSection );
		}
	}
	return rsp;
}

static jsonElement textDocumentFormatting ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;
	stringi const uri = req["textDocument"]["uri"];
	int64_t const tabWidth = req["options"]["tabSize"];
	bool const preferSpaces = req["options"]["insertSpaces"];

	rsp.makeArray ();
	auto lsFile = getFile ( server, ls, uri, id );

	if ( lsFile )
	{
		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		char const *src = lsFileCache.getCode ( lsFile->activeFile ).c_str ();

		srcLocation loc ( sourceIndex, (uint32_t) 1, (uint32_t) 1, (uint32_t) lsFile->file.languageRegions.rbegin ()->first.columnNumberEnd, (uint32_t) lsFile->file.languageRegions.rbegin ()->first.lineNumberEnd );

		auto regions = lsFile->file.getRegionsExcept ( loc, languageRegion::languageId::html );

		int64_t indent = -1;
		if ( lsFile->file.languageRegions.begin () != lsFile->file.languageRegions.end () )
		{
			if ( lsFile->file.languageRegions.begin ()->first.encloses ( loc ) )
			{
				if ( lsFile->file.languageRegions.begin ()->second == languageRegion::languageId::html )
				{
					indent = 1ull + 4;
				}
			}
		}
		for ( auto const &[reg, languageId] : regions )
		{
			jsonElement	formattedSection;

			auto [newText, nextIndent, formatLoc] = formatCode ( &lsFile->file, lsFile->info, src, reg, indent, languageId, tabWidth, preferSpaces, ls->formatFlags );
			formattedSection["newText"] = newText;
			formattedSection["range"] = range ( {sourceIndex, formatLoc.columnNumber, formatLoc.lineNumberStart, formatLoc.columnNumberEnd, formatLoc.lineNumberEnd} );

			if ( newText.size () )
			{
				formattedSection["newText"] = newText;

				rsp.push_back ( formattedSection );
			}
			indent = nextIndent;
		}
	}
	return rsp;
}

static jsonElement textDocumentOnTypeFormatting ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;
	stringi const uri = req["textDocument"]["uri"];
	int64_t const tabWidth = req["options"]["tabSize"];
	bool const preferSpaces = req["options"]["insertSpaces"];

	rsp.makeArray ();
	auto lsFile = getFile ( server, ls, uri, id );

	if ( lsFile )
	{
		if ( !lsFile->file.autoFormatPoints.size () )
		{
			return rsp;
		}

		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		char const *src = lsFileCache.getCode ( lsFile->activeFile ).c_str ();

		int64_t line = req["position"]["line"];
		int64_t character = req["position"]["character"];
		srcLocation charLocation{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

		std::unique_ptr<astNode> cmpNode = std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, charLocation );

		auto it = std::upper_bound ( lsFile->file.autoFormatPoints.begin (), lsFile->file.autoFormatPoints.end (), cmpNode, [] ( std::unique_ptr<astNode> const &left, std::unique_ptr<astNode> const &right )
			{
				return left.get ()->location < right.get ()->location;
			}
		);

		if ( it != lsFile->file.autoFormatPoints.begin () )
		{
			// upper bound so need to go back one to our current auto format point
			it--;
			if ( it != lsFile->file.autoFormatPoints.begin () )
			{
				// now go one back from there to find where we really want to start from
				it--;
			}
		}

		if ( (*it)->location < charLocation ) //  (*it)->location.columnNumber == character && (*it)->location.lineNumberStart == line + 1 )
		{
			srcLocation loc ( sourceIndex, (*it)->getSemanticAssociatedLocation ().columnNumber, (*it)->getSemanticAssociatedLocation ().lineNumberStart, (uint32_t) character + 1, (uint32_t) line + 1 );
//			srcLocation loc( sourceIndex, 1, (*it)->getSemanticAssociatedLocation().lineNumberStart > 1 ? (*it)->getSemanticAssociatedLocation().lineNumberStart - 1 : (*it)->getSemanticAssociatedLocation().lineNumberStart, (uint32_t) character + 1, (uint32_t) line + 1 );

			auto regions = lsFile->file.getRegionsExcept ( loc, languageRegion::languageId::html );

			int64_t indent = -1;
			if ( lsFile->file.languageRegions.begin () != lsFile->file.languageRegions.end () )
			{
				if ( lsFile->file.languageRegions.begin ()->first.encloses ( loc ) )
				{
					if ( lsFile->file.languageRegions.begin ()->second == languageRegion::languageId::html )
					{
						indent = 1ull + 4;
					}
				}
			}
			for ( auto const &[reg, languageId] : regions )
			{
				jsonElement	formattedSection;

				auto [newText, nextIndent, formatLoc] = formatCode ( &lsFile->file, lsFile->info, src, reg, indent, languageId, tabWidth, preferSpaces, ls->formatFlags );
				formattedSection["newText"] = newText;
				formattedSection["range"] = range ( {sourceIndex, formatLoc.columnNumber, formatLoc.lineNumberStart, formatLoc.columnNumberEnd, formatLoc.lineNumberEnd} );
				indent = nextIndent;

				rsp.push_back ( formattedSection );
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentDefinition ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t line = req["position"]["line"];
	int64_t character = req["position"]["character"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		jsonElement result;

		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		srcLocation src{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

		astLSInfo::symbolReference sym{ src };

		auto it = lsFile->info.symbolReferences.lower_bound ( sym );

		if ( !(it != lsFile->info.symbolReferences.end () && it->location.sourceIndex == sourceIndex && it->location.encloses ( character + 1, line + 1 )) )
		{
			it--;
		}

		if ( it != lsFile->info.symbolReferences.end () && it->location.sourceIndex == sourceIndex && it->location.encloses ( character + 1, line + 1 ) )
		{
			auto location = getSymbolSourceLocation ( it->source, it->name );
			if ( location.sourceIndex )
			{
				rsp = {{ "uri", fileNameToUri ( lsFile->file.srcFiles.getName ( location.sourceIndex ) ) },
							{ "range", range ( location ) }
				};
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentReferences ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t line = req["position"]["line"];
	int64_t character = req["position"]["character"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	rsp.makeArray ();

	if ( lsFile )
	{
		jsonElement result;
		size_t index = 0;

		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		srcLocation src{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

		astLSInfo::symbolReference sym{ src };

		auto it = lsFile->info.symbolReferences.lower_bound ( sym );

		if ( !(it != lsFile->info.symbolReferences.end () && it->location.encloses ( character + 1, line + 1 )) )
		{
			it--;
		}

		if ( it != lsFile->info.symbolReferences.end () && it->location.encloses ( character + 1, line + 1 ) )
		{
			for ( auto const &ref : lsFile->info.symbolReferences )
			{
				if ( ref.source == it->source )
				{
					rsp[index]["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( ref.location.sourceIndex ) );
					rsp[index]["range"] = range ( ref.location );
					index++;
				}
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentPrepareCallHierarchy ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t line = req["position"]["line"];
	int64_t character = req["position"]["character"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	rsp.makeArray ();

	if ( lsFile )
	{
		jsonElement result;
		size_t index = 0;

		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		srcLocation src {sourceIndex, (uint32_t)character + 1, (uint32_t)line + 1, (uint32_t)line + 1};

		astLSInfo::symbolReference sym {src};

		auto it = lsFile->info.symbolReferences.lower_bound ( sym );

		if ( !(it != lsFile->info.symbolReferences.end () && it->location.encloses ( character + 1, line + 1 )) )
		{
			it--;
		}

		if ( it != lsFile->info.symbolReferences.end () && it->location.encloses ( character + 1, line + 1 ) )
		{
			if ( std::holds_alternative<opFunction *>(it->source) )
			{
				auto func = lsFile->file.functionList.find ( it->name );
				if ( func != lsFile->file.functionList.end () )
				{
					rsp[index]["name"] = it->name;
					rsp[index]["kind"] = 12; // Function
					rsp[index]["tags"] = jsonElement ().makeArray ();
					rsp[index]["detail"] = getDetail ( &lsFile->file, it->source, true );
					rsp[index]["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( it->location.sourceIndex ) );
					rsp[index]["range"] = range ( func->second->extendedSelection );
					rsp[index]["selectionRange"] = range ( func->second->nameLocation );
					rsp[index]["data"] = reinterpret_cast<int64_t>(func->second);
					index++;
				}
			}
		}
	}
	return rsp;
}

static jsonElement callHiearchyIncomingCalls ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement {};
}
static jsonElement callHeiarchyOutgoingCalls ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )

{
	return jsonElement {};
}
static jsonElement textDocumentPrepareTypeHierarchy ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = req["textDocument"]["uri"];
	int64_t line = req["position"]["line"];
	int64_t character = req["position"]["character"];

	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		auto sourceIndex = lsFile->file.srcFiles.getStaticIndex ( lsFile->activeFile );
		srcLocation src{sourceIndex, (uint32_t) character + 1, (uint32_t) line + 1, (uint32_t) line + 1};

		if ( !lsFile->info.semanticTokens.empty () )
		{
			astLSInfo::symbolRange sym{ accessorType (), astLSInfo::semanticSymbolType::type, src, std::monostate () };

			auto it = lsFile->info.semanticTokens.lower_bound ( sym );

			if ( !(it != lsFile->info.semanticTokens.end () && it->location.encloses ( character + 1, line + 1 )) )
			{
				if ( it != lsFile->info.semanticTokens.begin () ) it--;
			}

			if ( it != lsFile->info.semanticTokens.end () && it->location.encloses ( character + 1, line + 1 ) )
			{
				auto detail = getDetail ( &lsFile->file, it->data, it->isAccess );
				if ( detail.size () )
				{
					switch ( lsFile->file.getRegion ( src ) )
					{
						case languageRegion::languageId::fgl:
							rsp["contents"][0] = stringi ( "```fgl\n" ) + detail + "\n```";
							break;
						case languageRegion::languageId::slang:
						default:
							rsp["contents"][0] = stringi ( "```slang\n" ) + detail + "\n```";
							break;
					}
				} else
				{
					rsp["contents"][0] = "";
				}
				auto doc = getDocumentation ( &lsFile->file, it->data, it->isAccess );
				if ( doc.size () )
				{
					rsp["contents"].push_back ( doc );
				}
				rsp["range"] = range ( it->location );
			}
		}
	}
	return rsp;
}

static std::pair<stringi, jsonElement> publishDiagnostics ( lsJsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;

	auto lsFile = ls->activeFile;

	if ( lsFile )
	{
		rsp["diagnostics"] = jsonElement ().makeArray ();
		rsp["uri"] = lsFile->activeUri;

		size_t index = 0;
		for ( auto const &it : lsFile->info.errors )
		{
			jsonElement tmp;
			char hex[64];
			sprintf_s ( hex, sizeof ( hex ), "%I32x", int ( it->errorData () ) );

			tmp["range"] = range ( it->location );
			tmp["severity"] = 1;
			tmp["source"] = "slangd";
			tmp["code"] = hex;
			tmp["message"] = scCompErrorAsText ( int ( it->errorData () ) );

			rsp["diagnostics"][index++] = tmp;
		}
		for ( auto const &it : lsFile->info.warnings )
		{
			jsonElement tmp;
			char hex[64];
			sprintf_s ( hex, sizeof ( hex ), "%I32x", int ( it->warnData ().num ) );

			tmp["range"] = range ( it->location );
			tmp["severity"] = 2;
			tmp["source"] = "slangd";
			tmp["code"] = hex;
			tmp["message"] = it->warnData ().text;

			rsp["diagnostics"][index++] = tmp;
		}
		for ( auto const &it : lsFile->file.classList )
		{
			if ( it.second->cClass )
			{
				for ( auto const &err : it.second->cClass->lsErrors )
				{
					jsonElement tmp;
					char hex[64];
					sprintf_s ( hex, sizeof ( hex ), "%I32x", int ( err.first ) );

					tmp["range"] = range ( err.second->location );
					tmp["severity"] = 1;
					tmp["source"] = "slangd";
					tmp["code"] = hex;
					tmp["message"] = scCompErrorAsText ( int ( err.first ) );

					rsp["diagnostics"][index++] = tmp;
				}
			}
		}
		lsFile->needSendErrors = false;
	}

	return {"textDocument/publishDiagnostics", rsp};
}

static std::pair<stringi, jsonElement> logOutput ( lsJsonRPCServerBase &server, languageServer *ls )
{
	jsonElement	rsp;

	std::lock_guard g1 ( ls->sendNotificationLock );

	rsp["type"] = 4;				// 4, constant per spec... log level output
	rsp["message"] = stringi ( ls->logBuffer.data<char *> (), ls->logBuffer.size () );

	return {"window/logMessage", rsp};
}

// language server methods
static jsonElement initialize ( jsonElement const &req, int64_t id, lsJsonRPCServerBase &server, languageServer *ls )
{
	ls->reset ();
	lsFileCache.clear ();

	if ( req.has ( "rootPath" ) )
	{
		ls->rootPath = req["rootPath"];
		if ( ls->rootPath.size () )
		{
			getDependencies ( ls, ls->rootPath );
		}
	}

	jsonElement serverCapabilities =
	{
		{ "capabilities",	{
								{ "textDocumentSync",	{	{ "openClose", true},
															{ "change", 2 } } },
								{ "hoverProvider",	{ "contents", "markdown" } },
								{ "completionProvider",	{	{ "triggerCharacters", { jsonElement::array, ".", ":" } },
															{ "allCommitCharacters", { jsonElement::array, ".", ":", " ", "[", "]", "(", ")", "{", "}", "+", "-", "/", "*", "=", "%", "^", "<", ">", "!", ",", ".", "?", ":", "~" } } } },
								{ "signatureHelpProvider",	{ "triggerCharacters", { jsonElement::array, "(", "," } } },
								{ "declarationProvider", false },
								{ "definitionProvider", true },
								{ "typeDefinitionProvider", true },
								{ "implementationProvider", false },
								{ "referencesProvider", true },
								{ "documentHighlightProvider", false },
								{ "documentSymbolProvider", true },
								{ "codeActionProvider", false },
								{ "codeLensProvider", false },
								{ "documentLinkProvider", false },
								{ "colorProvider", false },
								{ "documentFormattingProvider", true },
								{ "documentRangeFormattingProvider", true },
								{ "documentOnTypeFormattingProvider",	{	{ "firstTriggerCharacter", "}" },
																			{ "moreTriggerCharacter", { jsonElement::array, ";", ")", "d", "D", "{" } } } }, // d/D is for END 
								{ "renameProvider", false },
								{ "foldingRangeProvider", true },
								{ "executeCommandProvider", false },
								{ "selectionRangeProvider", true },

								{ "linkedEditingRangeProvider", false },
								{ "callHierarchyProvider", true },
								{ "semanticTokensProvider",	{	{ "full", true },
																{ "range", true },
																{ "legend", { "tokenModifiers", { jsonElement::array } } } } },
								{ "typeHierarchyProvider", true },
								{ "inlineValueProvider", false },
								{ "inlayHintProvider", true },
								{ "diagnosticProvider", false },
								{ "workspaceSymbolProvider", true },
								{ "workspace",	{ "workspaceFolders",	{	{ "supported",true },
																			{ "changeNotifications",true } }, } },
							}
		}
	};

	std::map<int64_t, stringi > mappedValues = {
#define DEF(value, mappedValue ) { (int64_t)astLSInfo::semanticSymbolType::mappedValue, stringi ( #mappedValue ) },
		SEMANTIC_TOKEN_TYPES
#undef DEF
	};

	for ( auto &it : mappedValues )
	{
		serverCapabilities["capabilities"]["semanticTokensProvider"]["legend"]["tokenTypes"].push_back ( std::get<1> ( it ) );
	}

	return serverCapabilities;
}

taskControl *startLanguageServer ( uint16_t port )
{
	// this call writes to the console the configuration settings for inclusion into the package.json file for vscode
	// we need to rerun this and copy the output to the file anytime we add something new to the configuration file.
	// currently everything is a boolean, and there are default values as well as a description written out

//		languageServer::makeJsonSettings ();

	languageServer *ls = new languageServer ();
	vmTaskInstance instance ( "test" );

	auto [builtIn, builtInSize] = builtinInit ( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE
	ls->builtIn = builtIn;
	ls->builtInSize = builtInSize;

	auto svr = new socketServer<lsJsonRpcServer<socketServer, decltype(ls)>, languageServer *> ( port, std::move ( ls ) );			// NOLINT(performance-move-const-arg) 

	svr->setMethods (
		{
			{ "initialize", initialize },
			{ "initialized", initialized },
			{ "shutdown", shutdown },
			{ "exit", exit },
			{ "$/setTrace", setTrace},
			{ "$/logTrace", logTrace },
			{ "workspace/didChangeWatchedFiles",		workspaceDidChangeWatchedFile },
			{ "workspace/didChangeConfiguration",		workspaceDidChangeConfiguration },
			{ "workspace/symbol",						workspaceSymbol },
			{ "textDocument/didOpen",					textDocumentDidOpen },
			{ "textDocument/didClose",					textDocumentDidClose },
			{ "textDocument/didChange",					textDocumentDidChange },
			{ "textDocument/documentSymbol",			textDocumentDocumentSymbol },
			{ "textDocument/hover",						textDocumentHover},
			{ "textDocument/completion",				textDocumentCompletion},
			{ "textDocument/foldingRange",				textDocumentFoldingRange },
			{ "textDocument/definition",				textDocumentDefinition },
			{ "textDocument/signatureHelp",				textDocumentSignatureHelp },
			{ "textDocument/semanticTokens/full",		textDocumentSemanticTokens },
			{ "textDocument/semanticTokens/range",		textDocumentSemanticTokensRange },
			{ "textDocument/references",				textDocumentReferences },
			{ "textDocument/rangeFormatting",			textDocumentRangeFormatting},
			{ "textDocument/formatting",				textDocumentFormatting},
			{ "textDocument/onTypeFormatting",			textDocumentOnTypeFormatting},
			{ "textDocument/selectionRange",			textDocumentSelectionRange},
			{ "textDocument/inlayHint",					textDocumentInlayHint },
			{ "textDocument/prepareTypeHierarchy",		textDocumentPrepareTypeHierarchy },
			{ "textDocument/prepareCallHierarchy",		textDocumentPrepareCallHierarchy},
			{ "callHierarchy/incomingCalls",			callHiearchyIncomingCalls },
			{ "callHierarchy/outgoingCalls",			callHeiarchyOutgoingCalls },

			//							{ "textDocument/documentSymbol",			textDocumentDocumentSymbol},
			//							{ "textDocument/documentSymbol",			textDocumentDocumentSymbol},
		}
	);
	svr->setNotificationHandler (
		{
#define DEF(a) a,
				NOTIFICATIONS
#undef DEF
		}
	);
	auto tc = new socketServerTaskControl<languageServer *> ( HWND ( 0 ) );
	svr->startServer ( tc );
	return tc;
}

taskControl *startLanguageServer ()
{
	languageServer *ls = new languageServer ();
	vmTaskInstance instance ( "test" );

	auto [builtIn, builtInSize] = builtinInit ( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE

	ls->builtIn = builtIn;
	ls->builtInSize = builtInSize;
	auto tmp = ls;
	auto svr = new lspConsoleServer< lsJsonRpcServer<socketServer, decltype(ls)>, languageServer *> ( std::move ( tmp ) );			// NOLINT(performance-move-const-arg) 

	svr->setMethods (
		{
			{ "initialize", initialize },
			{ "initialized", initialized },
			{ "shutdown", shutdown },
			{ "exit", exit },
			{ "$/setTrace", setTrace},
			{ "$/logTrace", logTrace },
			{ "workspace/didChangeWatchedFiles",		workspaceDidChangeWatchedFile },
			{ "workspace/didChangeConfiguration",		workspaceDidChangeConfiguration },
			{ "workspace/symbol",						workspaceSymbol },
			{ "textDocument/didOpen",					textDocumentDidOpen },
			{ "textDocument/didClose",					textDocumentDidClose },
			{ "textDocument/didChange",					textDocumentDidChange },
			{ "textDocument/documentSymbol",			textDocumentDocumentSymbol },
			{ "textDocument/hover",						textDocumentHover},
			{ "textDocument/completion",				textDocumentCompletion},
			{ "textDocument/foldingRange",				textDocumentFoldingRange },
			{ "textDocument/definition",				textDocumentDefinition },
			{ "textDocument/signatureHelp",				textDocumentSignatureHelp },
			{ "textDocument/semanticTokens/full",		textDocumentSemanticTokens },
			{ "textDocument/semanticTokens/range",		textDocumentSemanticTokensRange },
			{ "textDocument/references",				textDocumentReferences },
			{ "textDocument/rangeFormatting",			textDocumentRangeFormatting },
			{ "textDocument/formatting",				textDocumentFormatting },
			{ "textDocument/onTypeFormatting",			textDocumentOnTypeFormatting },
			{ "textDocument/selectionRange",			textDocumentSelectionRange },
			{ "textDocument/inlayHint",					textDocumentInlayHint },
			{ "textDocument/prepareTypeHierarchy",		textDocumentPrepareTypeHierarchy },
			{ "textDocument/prepareCallHierarchy",		textDocumentPrepareCallHierarchy},
			{ "callHierarchy/incomingCalls",			callHiearchyIncomingCalls },
			{ "callHierarchy/outgoingCalls",			callHeiarchyOutgoingCalls },
			//							{ "textDocument/documentSymbol",			textDocumentDocumentSymbol},
		}
	);

	svr->setNotificationHandler (
		{
#define DEF(a) a,
				NOTIFICATIONS
#undef DEF
		}
	);

	auto tc = new lspConsoleServerTaskControl<languageServer *> ( ls );				// NOLINT(performance-move-const-arg) 
	svr->startServer ( tc );

	return tc;
}
