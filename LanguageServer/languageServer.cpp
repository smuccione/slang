#include "Utility/settings.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif


#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <set>
#include <mutex>
#include <variant>
#include <memory.h>
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


static	jsonRPCServerBase *activeLanguageServer = nullptr;

stringi getElementSignature ( compClassElementSearch *elem )
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
			return type + "(property)  " + elem->elem->name.c_str() + " ()";
		default:
			break;
	}
	return signature;
}

void getDependencies ( languageServer *ls, stringi const &rootPath )
{
	int			retCode = 0;

	auto close = [&retCode]( FILE *file )
	{
		retCode = _pclose ( file );
	};

	std::filesystem::path	maker ( (std::string) rootPath );
		
	stringi cmdToRun = "smake.exe -C ";
	cmdToRun += maker.generic_string ();
	cmdToRun += " new gendep -s -j8";
	
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

	char const *depStr = buff.data<char const*> ();

	jsonElement dep ( &depStr );
	for ( auto libs = dep.cbeginObject (); libs != dep.cendObject (); libs++ )
	{
		auto lib = libs->first;
		for ( auto files = libs->second.cbeginArray(); files != libs->second.cendArray (); files++ )
		{
			ls->fileToTargetMap[*files] = lib;
			ls->targetFiles[lib].push_back ( *files );
		}
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

stringi unescapeJsonString( char const *str )
{
	stringi v;

	while ( *str && str[0] != '"' )
	{
		if ( str[0] == '%' && str[1] )
		{
			if ( _ishex( &str[1] ) && _ishex( &str[2] ) )
			{
				v += static_cast<char>((hexDecodingTable[(size_t)(uint8_t)str[1]] << 4) + hexDecodingTable[(size_t)(uint8_t)str[2]]);
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
	jsonElement ret;
	// apparently the send column number is exclusive while start is inclusive
	ret["start"]["line"] = loc.lineNumberStart - 1;
	ret["start"]["character"] = loc.columnNumber - 1;
	ret["end"]["line"] = loc.lineNumberEnd - 1;
	ret["end"]["character"] =  loc.columnNumberEnd - 1;

	return ret;
}

static jsonElement foldingRange ( srcLocation &loc )
{
	jsonElement ret;

	ret["range"]["start"]["line"] = loc.lineNumberStart - 1;
	ret["range"]["end"]["line"] = loc.lineNumberEnd - 1;
	ret["kind"] = 3;

	return ret;
}

static srcLocation getRange ( jsonElement const &elem )
{
	srcLocation loc;

	loc.lineNumberStart = (int32_t)*(*elem["start"])["line"] + 1;
	loc.lineNumberEnd = (int32_t)*(*elem["end"])["line"] + 1;
	loc.columnNumber = (int32_t)*(*elem["start"])["character"] + 1;
	loc.columnNumberEnd = (int32_t)*(*elem["end"])["character"] + 1;

	return loc;
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

static stringi getDetail( opFile *file, symbolSource const &source, bool isAccess )
{
	if ( std::holds_alternative<class opFunction *>( source ) )
	{
		auto func = std::get<class opFunction *>( source );
		return func->getSimpleFunctionSignature();
	} else if ( std::holds_alternative<symbolTypeClass>( source ) )
	{
		auto type = std::get<symbolTypeClass>( source );
		return stringi ( "" ) + type.operator stringi();
	} else if ( std::holds_alternative<opClass *> ( source ) )
	{
		auto cls = std::get<opClass *> ( source );
		return stringi ( "class " ) + cls->name.operator const stringi &();
	} else if ( std::holds_alternative<astNode *>( source ) )
	{
		auto node = std::get<astNode *>( source );
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
		return func->documentation;
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
				return sym->getDocumentation ( sym->getSymbolName(), isAccess );
			default:
				break;
		}
	}
	return stringi ();
}

static languageServerFile *getFile ( jsonRPCServerBase &server, languageServer *ls, stringi const &uri, int64_t id, stringi const &code = stringi(), int64_t version = 1, languageServerFileData::languageType languageId = languageServerFileData::languageType::slang )
{	
	auto uriFixed = unescapeJsonString( uri.c_str() );
	auto fileName = uriToFileName ( uriFixed );

	auto it = ls->fileToTargetMap.find ( fileName );
	if ( it == ls->fileToTargetMap.end () )
	{
		ls->fileToTargetMap[fileName] = fileName;
		ls->targetFiles[fileName].push_back ( fileName );
	}

	auto target = ls->fileToTargetMap[fileName];

	languageServerFile *lsFile = ls->targets[target].get();

	if ( !lsFile && code.size() )
	{
		ls->targets[target] = std::make_unique<languageServerFile> ();
		lsFile = ls->targets[target].get ();

		lsFile->fileVersions[target] = 0;

		lsFile->name	= fileName;
		lsFile->uri		= uri;

		// add in default libraries
		auto builtIn = ls->builtIn;
		lsFile->file.addStream ( (const char **) &builtIn, false, false );

		// add in all the associated sources from other files if we're part of a library to handle cross dependencies
		for ( auto &it : ls->targetFiles[target] )
		{
			if ( !lsFileCache.has ( it ) )
			{
//				printf ( "\topening: %s\r\n", it.c_str () );

				languageServerFileData::languageType langId = languageServerFileData::languageType::unknown;

				std::filesystem::path p = (std::string)it;
				if ( !strccmp ( p.extension ().generic_string ().c_str (), ".fgl" ) )
				{
					langId = languageServerFileData::languageType::fgl;
				} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".sl" ) )
				{
					langId = languageServerFileData::languageType::slang;
				} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".ap" ) )
				{
					langId = languageServerFileData::languageType::ap_fgl;
				} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".apf" ) )
				{
					langId = languageServerFileData::languageType::ap_fgl;
				} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".aps" ) )
				{
					langId = languageServerFileData::languageType::ap_slang;
				}

				if ( langId != languageServerFileData::languageType::unknown )
				{
					if ( it != fileName )
					{
						std::ifstream inFile ( it.c_str () );
						if ( inFile.is_open () )
						{
							lsFileCache.update ( it, code, version );

							// this makes that file a dependency of us
							lsFile->fileVersions[it] = 0;
						}
					}
				}
			} else
			{
				lsFile->fileVersions[it] = 0;
			}
		}

		languageServerFileData::languageType langId = languageServerFileData::languageType::unknown;

		std::filesystem::path p = (std::string) fileName;
		if ( !strccmp ( p.extension ().generic_string ().c_str (), ".fgl" ) )
		{
			langId = languageServerFileData::languageType::fgl;
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".sl" ) )
		{
			langId = languageServerFileData::languageType::slang;
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".ap" ) )
		{
			langId = languageServerFileData::languageType::ap_fgl;
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".apf" ) )
		{
			langId = languageServerFileData::languageType::ap_fgl;
		} else if ( !strccmp ( p.extension ().generic_string ().c_str (), ".aps" ) )
		{
			langId = languageServerFileData::languageType::ap_slang;
		}

		lsFile->fileVersions[fileName] = 0;
		// update the source but don't compile... we'll compile ONLY on demand
		lsFileCache.update ( fileName, code, version, langId );
	} else if ( lsFile )
	{
		if ( code.size () )
		{
			// update the file but don't compile
			lsFileCache.update ( fileName, code, version );
		} else
		{
			// we now need to actually start processing so parse the file
			lsFile->updateSource ();
			server.sendNotification ( (size_t)notifications::publishDiagnostics );
		}
	}

	return lsFile;
}

static languageServerFile *initFile( languageServer *ls, stringi const &uri, languageServerFileData::languageType languageId = languageServerFileData::languageType::slang )
{
	auto uriFixed = unescapeJsonString( uri.c_str() );
	auto fileName = uriToFileName( uriFixed );
	auto lsFile = ls->targets[fileName].get ();

#if 0
	if ( !lsFile )
	{
		printf( "\tinitializing: %s\r\n", fileName.c_str() );
		lsFile = new languageServerFile();
		ls->targets[fileName] = lsFile;
		lsFile->name = fileName;
		lsFile->uri = uri;
		lsFile->type = languageId;
		lsFile->sourceIndex = lsFile->file.srcFiles.getIndex( fileName );

		auto builtIn = ls->builtIn;
		lsFile->file.addStream( (const char **) &builtIn, false );
	}
#endif
	return lsFile;
}

jsonElement initialized ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement workspaceDidChangeWatchedFile ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{	
	return jsonElement ();
}

static jsonElement textDocumentDidClose ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{	
	stringi uri = *(*req["textDocument"])["uri"];

	auto uriFixed = unescapeJsonString ( uri.c_str () );
	auto fileName = uriToFileName ( uriFixed );

	auto it = ls->fileToTargetMap.find ( fileName );
	if ( it != ls->fileToTargetMap.end () )
	{
		auto &target = ls->fileToTargetMap[fileName];
		ls->targets.erase ( target );
		ls->fileToTargetMap.erase ( it );
	}
	return jsonElement ();
}

static jsonElement textDocumentDidOpen ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi uri = *(*req["textDocument"])["uri"];
	stringi languageId = *(*req["textDocument"])["languageId"];
	int64_t version = *(*req["textDocument"])["version"];
	stringi text = *(*req["textDocument"])["text"];

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

static jsonElement textDocumentDidChange ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{	
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t	version = *(*req["textDocument"])["version"];
	stringi const code = *(*(*req["contentChanges"])[(size_t)0])["text"];

//	printf ( "new fileVersion=%I64i\r\n", version );

	ls->activeFile = getFile ( server, ls, uri, id, code, version );
	return jsonElement ();
}

static jsonElement textDocumentHover( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t line = *(*req["position"])["line"];
	int64_t character = *(*req["position"])["character"];

	auto lsFile = getFile( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );
		srcLocation src{ sourceIndex, (uint32_t)character + 1, (uint32_t)line + 1, (uint32_t)line + 1 };

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
				size_t index = 0;
				auto pSigs = it->func->getParameterHelp ();
				if ( paramNum < pSigs.size () )
				{
					rsp["contents"].push_back ( std::get<1>(pSigs[paramNum]) );
				}
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentSignatureHelp ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t line = *(*req["position"])["line"];
	int64_t character = *(*req["position"])["character"];

	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );
		srcLocation src{ sourceIndex, (uint32_t)character + 1, (uint32_t)line + 1, (uint32_t)line + 1 };

		astLSInfo::signatureHelpDef sym{ src, nullptr, nullptr };

		auto it = lsFile->info.signatureHelp.lower_bound ( sym );

		size_t paramNum = 0;

		if ( !lsFile->info.signatureHelp.empty () )
		{
			if ( !(it != lsFile->info.signatureHelp.end () && it->location.encloses ( character + 1, line + 1 )) )
			{
				if ( it != lsFile->info.signatureHelp.begin() ) it--;
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
				for ( auto &[label, documentation]: pSigs )
				{
					rsp["signatures"][0]["parameters"][index]["label"] = label;
					rsp["signatures"][0]["parameters"][index++]["documentation"] = documentation;
				}
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentCompletion ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t line = *(*req["position"])["line"];
	int64_t character = *(*req["position"])["character"];
//	int64_t trigger = *(*req["context"])["triggerKind"];
	jsonElement rsp;

	auto lsFile = getFile( server, ls, uri, id );

	if ( lsFile )
	{
		size_t index = 0;
		rsp["isIncomplete"] = false;
		rsp["items"] = jsonElement ().makeArray ();

		auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );
		srcLocation src{ sourceIndex, (uint32_t)character + 1, (uint32_t)line + 1, (uint32_t)line + 1 };

		astLSInfo::objectCompletions sym{ src, symVariantType };

		auto it = lsFile->info.objCompletions.lower_bound ( sym );

		if ( !(it != lsFile->info.objCompletions.end () && it->location.encloses ( character, line + 1 ) ) && it != lsFile->info.objCompletions.begin () )
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

static jsonElement textDocumentDocumentSymbol ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{	
	stringi const uri = *(*req["textDocument"])["uri"];
	auto lsFile = getFile ( server, ls, uri, id );

	if ( !lsFile )
	{
		return jsonElement();
	}
	size_t index = 0;
	jsonElement rsp;

	auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );

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
					funcSyms = { func, jsonElement ().makeArray () };
				}
				auto &location = it.location;

				jsonElement tmp;

				tmp["name"] = it.name;
				tmp["kind"] = languageServerFile::SymbolKind::Variable; /* variable */
				tmp["range"] = range ( location );
				tmp["selectionRange"] = range ( location );
				tmp["detail"] = it.type.operator stringi();

				std::get<1> ( funcSyms ).push_back ( tmp );
			} else
			{
				// global symbol... not within a function namespace so emit it here
				rsp[index]["name"] = it.name;
				rsp[index]["kind"] = languageServerFile::SymbolKind::Class; /* class    */
				rsp[index]["range"] = range ( it.location );
				rsp[index]["selectionRange"] = range ( it.location );
				rsp[index]["detail"] = it.type.operator stringi();
				index++;
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
									children[children.size ()] = tmp;
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
							children[children.size ()] = tmp;
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
								children[children.size ()] = tmp;
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
								children[children.size ()] = tmp;
							}
							break;
						case fgxClassElementType::fgxClassType_const:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Constant; /* Constant    */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = tmp;
							break;
						case fgxClassElementType::fgxClassType_inherit:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Class; /* class    */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = tmp;
							break;
						case fgxClassElementType::fgxClassType_message:
							break;
					}
				}
			}
			rsp[index]["children"] = children;
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

static jsonElement workspaceSymbol ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi const query = *req["query"];
#if 0

	auto lsFile = getFile ( server, ls, uri, id );

	if ( !lsFile )
	{
		return jsonElement ();
	}
	size_t index = 0;
	jsonElement rsp;

	auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );

	std::map<cacheString, std::pair<opFunction *, jsonElement>>	funcSymbolMap;

	for ( auto const &it : lsFile->info.symbolDefinitions )
	{
		if ( it.location.sourceIndex == sourceIndex )
		{
			auto func = it.func;
			auto &funcSyms = funcSymbolMap[func->name];

			if ( !std::get<0> ( funcSyms ) )
			{
				funcSyms = { func, jsonElement ().makeArray () };
			}
			auto location = it.location;

			jsonElement tmp;

			tmp["name"] = it.name;
			tmp["kind"] = languageServerFile::SymbolKind::Variable; /* variable */
			tmp["range"] = range ( location );
			tmp["selectionRange"] = range ( location );
			tmp["detail"] = it.type.operator stringi();

			std::get<1> ( funcSyms ).push_back ( tmp );
		}
	}

	for ( auto &it : lsFile->file.classList )
	{
		if ( it.second->location.sourceIndex == sourceIndex )
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
								children[children.size ()] = tmp;
							}
							break;
						case fgxClassElementType::fgxClassType_iVar:
						case fgxClassElementType::fgxClassType_static:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Field; /* Field  */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = tmp;
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
								children[children.size ()] = tmp;
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
								children[children.size ()] = tmp;
							}
							break;
						case fgxClassElementType::fgxClassType_const:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Constant; /* Constant    */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = tmp;
							break;
						case fgxClassElementType::fgxClassType_inherit:
							tmp["name"] = elem->name;
							tmp["kind"] = languageServerFile::SymbolKind::Class; /* class    */
							tmp["range"] = range ( elem->location );
							tmp["selectionRange"] = range ( elem->location );
							tmp["detail"] = elem->getType ( true ).operator stringi();
							children[children.size ()] = tmp;
							break;
						case fgxClassType_message:
							break;
					}
				}
			}

			rsp[index]["children"] = children;
			index++;
		}
	}

	for ( auto &it : funcSymbolMap )
	{
		if ( std::get<0> ( it.second )->location.sourceIndex == sourceIndex )
		{
			rsp[index]["name"] = it.first;
			rsp[index]["kind"] = languageServerFile::SymbolKind::Function; /* function */
			rsp[index]["selectionRange"] = range ( std::get<0> ( it.second )->nameLocation );
			rsp[index]["range"] = range ( std::get<0> ( it.second )->location );
			rsp[index]["detail"] = std::get<0> ( it.second )->getSimpleFunctionSignature ();
			rsp[index++]["children"] = std::get<1> ( it.second );
		}
	}

	for ( auto &it : lsFile->file.symbols )
	{
		if ( it.second.isExportable )
		{
			if ( it.second.location.sourceIndex == sourceIndex )
			{
				rsp[index]["name"] = it.second.name;
				rsp[index]["kind"] = languageServerFile::SymbolKind::Variable; /* variable */
				rsp[index]["range"] = range ( it.second.location );
				rsp[index]["selectionRange"] = range ( it.second.location );
				rsp[index]["detail"] = it.second.getType ();
				index++;
			}
		}
	}
	return rsp; 

#else
	return jsonElement ().makeArray();
#endif
}

static jsonElement shutdown ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement exit ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	return jsonElement ();
}

static jsonElement cancel_request ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	int64_t cid = *req["id"];
//	printf( "cancel: %I64i\r\n", cid );
	return jsonElement ();
}

static jsonElement textDocumentFoldingRange ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;
	stringi uri = *(*req["textDocument"])["uri"];
	auto lsFile = getFile( server, ls, uri, id );

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
		rsp = jsonElement().makeArray();
	}
	return rsp;
}

static jsonElement textDocumentSemanticTokens ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = *(*req["textDocument"])["uri"];
//	int64_t semID = *req["id"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		size_t index = 0;
		int64_t lastLine = 1;
		int64_t lastColumn = 1;
		auto currentSourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );

		jsonElement arrValues;
		
		arrValues.makeArray ();
		arrValues.reserve ( lsFile->info.semanticTokens.size() * 5 );

		for ( auto const &it : lsFile->info.semanticTokens )
		{
			if ( it.location.sourceIndex == currentSourceIndex )
			{
				if ( it.location.lineNumberStart == it.location.lineNumberEnd )
				{
					// line
					arrValues.emplace_back( it.location.lineNumberStart - lastLine );
					// character
					if ( lastLine == it.location.lineNumberStart )
					{
						arrValues.emplace_back( it.location.columnNumber - lastColumn );
					} else
					{
						arrValues.emplace_back( it.location.columnNumber - 1 );
					}
					// length
					arrValues.emplace_back( it.location.columnNumberEnd - it.location.columnNumber );
					// type
					arrValues.emplace_back( astLSInfo::mappedValues[(int64_t)it.type] ); 
					// modifier
					arrValues.emplace_back( (int64_t)0 ); 

					lastLine = it.location.lineNumberStart;
					lastColumn = it.location.columnNumber;
				} else
				{
					/***************** start of multi-line token *****************/
					// line
					arrValues.emplace_back( it.location.lineNumberStart - lastLine );
					// character
					if ( lastLine == it.location.lineNumberStart )
					{
						arrValues.emplace_back( it.location.columnNumber - lastColumn );
					} else
					{
						arrValues.emplace_back( it.location.columnNumber - 1 );
					}
					// length
					arrValues.emplace_back( 65536 );		// indicator for multi-line
					// type
					arrValues.emplace_back( astLSInfo::mappedValues[(int64_t) it.type] );
					// modifier
					arrValues.emplace_back( (int64_t)0 ); 

					for ( uint32_t loop = it.location.lineNumberStart; loop < it.location.lineNumberEnd - 1; loop++ )
					{
						// line
						arrValues.emplace_back( 1 );
						// column
						arrValues.emplace_back( 0 );
						// length
						arrValues.emplace_back( 65536 );	// indicator for multi-line
						// type
						arrValues.emplace_back( astLSInfo::mappedValues[(int64_t) it.type] );
						// modifier
						arrValues.emplace_back( (int64_t)0 ); 
					}

					/***************** end of multi-line token *****************/
					// line
					arrValues.emplace_back( 1 );
					// character
					arrValues.emplace_back( 0 );
					// length
					arrValues.emplace_back( it.location.columnNumberEnd );
					// type
					arrValues.emplace_back( astLSInfo::mappedValues[(int64_t) it.type] );
					// modifier
					arrValues.emplace_back( (int64_t)0 ); 

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
	} else if ( std::holds_alternative<symbolParamDef *> ( source ) )
	{
		return std::get<symbolParamDef *> ( source )->location;
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

static jsonElement textDocumentRangeFormatting ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t const tabWidth = *(*req["options"])["tabSize"];
	bool const preferSpaces = *(*req["options"])["insertSpaces"];

	rsp.makeArray ();
	auto lsFile = getFile ( server, ls, uri, id );

	if ( lsFile )
	{
		// we MUST do this again... normal update runs type-inference and other compile-time processing to generate diagnostics and types.   This modifies the AST
		// which will interferere with formatting
//		updateFormatSource ( lsFile, lsFile->name );

		auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );
		char const *src = lsFileCache.getCode ( lsFile->name ).c_str ();

		jsonElement start = *(*req["range"])["start"];
		jsonElement end = *(*req["range"])["end"];

		int64_t startLine = start["line"];
		int64_t startCharacter = start["character"];
		int64_t endLine = end["line"];
		int64_t endCharacter = end["character"];

		srcLocation loc ( sourceIndex, (uint32_t)startCharacter + 1, (uint32_t)startLine + 1, (uint32_t)endCharacter + 1, (uint32_t)endLine + 1 );

		auto regions = lsFile->file.getRegionsExcept ( loc, languageRegion::languageId::html );

		int64_t indent = -1;
		if ( lsFile->file.languageRegions.begin () != lsFile->file.languageRegions.end () )
		{
			if ( lsFile->file.languageRegions.begin ()->first.encloses ( loc ) )
			{
				if ( lsFile->file.languageRegions.begin ()->second == languageRegion::languageId::html )
				{
					indent = 1 + 4;
				}
			}
		}
		for ( auto const &[reg, languageId] : regions )
		{
			jsonElement	formattedSection;

			auto [newText, nextIndent, formatLoc] = lsFile->file.formateCode ( lsFile->info, src, reg, indent, languageId, tabWidth, preferSpaces );
			formattedSection["newText"] = newText;
			formattedSection["range"] = range ( { sourceIndex, formatLoc.columnNumber, formatLoc.lineNumberStart, formatLoc.columnNumberEnd, formatLoc.lineNumberEnd } );
			indent = nextIndent;

			rsp.push_back ( formattedSection );
		}
	}
	return rsp;
}

static jsonElement textDocumentFormatting ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t const tabWidth = *(*req["options"])["tabSize"];
	bool const preferSpaces = *(*req["options"])["insertSpaces"];

	rsp.makeArray ();
	auto lsFile = getFile ( server, ls, uri, id );

	if ( lsFile )
	{
		// we MUST do this again... normal update runs type-inference and other compile-time processing to generate diagnostics and types.   This modifies the AST
		// which will interferere with formatting
//		updateFormatSource ( lsFile, lsFile->name );

		auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );
		char const *src = lsFileCache.getCode ( lsFile->name ).c_str ();

		srcLocation loc ( sourceIndex, (uint32_t)1, (uint32_t)1, (uint32_t)lsFile->file.languageRegions.rbegin()->first.columnNumberEnd, (uint32_t)lsFile->file.languageRegions.rbegin ()->first.lineNumberEnd );

		auto regions = lsFile->file.getRegionsExcept ( loc, languageRegion::languageId::html );

		int64_t indent = -1;
		if ( lsFile->file.languageRegions.begin () != lsFile->file.languageRegions.end () )
		{
			if ( lsFile->file.languageRegions.begin ()->first.encloses ( loc ) )
			{
				if ( lsFile->file.languageRegions.begin ()->second == languageRegion::languageId::html )
				{
					indent = 1 + 4;
				}
			}
		}
		for ( auto const &[reg, languageId] : regions )
		{
			jsonElement	formattedSection;

			auto [newText, nextIndent, formatLoc] = lsFile->file.formateCode ( lsFile->info, src, reg, indent, languageId, tabWidth, preferSpaces );
			formattedSection["newText"] = newText;
			formattedSection["range"] = range ( { sourceIndex, formatLoc.columnNumber, formatLoc.lineNumberStart, formatLoc.columnNumberEnd, formatLoc.lineNumberEnd } );

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

static jsonElement textDocumentDefinition ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t line = *(*req["position"])["line"];
	int64_t character = *(*req["position"])["character"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	if ( lsFile )
	{
		jsonElement result;

		auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );
		srcLocation src{ sourceIndex, (uint32_t)character + 1, (uint32_t)line + 1, (uint32_t)line + 1 };

		astLSInfo::symbolReference sym{ src };

		auto it = lsFile->info.symbolReferences.lower_bound ( sym );

		if ( !(it != lsFile->info.symbolReferences.end () && it->location.sourceIndex == sourceIndex && it->location.encloses ( character + 1, line + 1 ) ) )
		{
			it--;
		}

		if ( it != lsFile->info.symbolReferences.end () && it->location.sourceIndex == sourceIndex && it->location.encloses ( character + 1, line + 1 ) )
		{
			auto location = getSymbolSourceLocation ( it->source, it->name );
			if ( location.sourceIndex )
			{
				rsp["uri"] = fileNameToUri ( lsFile->file.srcFiles.getName ( location.sourceIndex ) );
				rsp["range"] = range ( location );
			}
		}
	}
	return rsp;
}

static jsonElement textDocumentReferences ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	stringi const uri = *(*req["textDocument"])["uri"];
	int64_t line = *(*req["position"])["line"];
	int64_t character = *(*req["position"])["character"];
	auto lsFile = getFile ( server, ls, uri, id );

	jsonElement rsp;

	rsp.makeArray ();

	if ( lsFile )
	{
		jsonElement result;
		size_t index = 0;

		auto sourceIndex = lsFile->file.srcFiles.getIndex ( lsFile->name );
		srcLocation src{ sourceIndex, (uint32_t)character + 1, (uint32_t)line + 1, (uint32_t)line + 1 };

		astLSInfo::symbolReference sym{ src };

		auto it = lsFile->info.symbolReferences.lower_bound ( sym );

		if ( !(it != lsFile->info.symbolReferences.end () && it->location.encloses ( character + 1, line + 1 ) ) )
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

// language server methods
static jsonElement initialize ( jsonElement const &req, int64_t id, jsonRPCServerBase &server, languageServer *ls )
{
	ls->rootPath = *req["rootPath"];

	ls->reset ();
	lsFileCache.clear ();

	if ( ls->rootPath.size () )
	{
		getDependencies ( ls, ls->rootPath );
	}

	jsonElement serverCapabilities;

	serverCapabilities["capabilities"]["textDocumentSync"]["openClose"] = true;
	serverCapabilities["capabilities"]["textDocumentSync"]["change"] = 1;
//	serverCapabilities["capabilities"]["textDocumentSync"]["save"] = jsonElement::objectType();
	serverCapabilities["capabilities"]["hoverProvider"]["contents"] = stringi ( "markdown" );
	serverCapabilities["capabilities"]["completionProvider"]["triggerCharacters"][0] = stringi ( "." );
	serverCapabilities["capabilities"]["completionProvider"]["triggerCharacters"][1] = stringi ( ":" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][0] = stringi ( ";" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][1] = stringi ( " " );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][2] = stringi ( "[" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][3] = stringi ( "]" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][4] = stringi ( "(" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][5] = stringi ( ")" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][6] = stringi ( "{" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][7] = stringi ( "}" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][8] = stringi ( "+" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][9] = stringi ( "-" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][10] = stringi ( "/" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][11] = stringi ( "*" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][12] = stringi ( "=" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][13] = stringi ( "%" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][14] = stringi ( "^" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][15] = stringi ( "<" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][16] = stringi ( ">" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][17] = stringi ( "!" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][18] = stringi ( "," );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][19] = stringi ( "." );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][20] = stringi ( "?" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][21] = stringi ( ":" );
	serverCapabilities["capabilities"]["completionProvider"]["allCommitCharacters"][22] = stringi ( "~" );
	serverCapabilities["capabilities"]["signatureHelpProvider"]["triggerCharacters"][0] = "(";
	serverCapabilities["capabilities"]["signatureHelpProvider"]["triggerCharacters"][1] = ",";
	serverCapabilities["capabilities"]["definitionProvider"] = true;
	serverCapabilities["capabilities"]["typeDefinitionProvider"] = false;
	serverCapabilities["capabilities"]["implementationProvider"] = false;
	serverCapabilities["capabilities"]["referencesProvider"] = true;
	serverCapabilities["capabilities"]["documentHighlightProvider"] = false;
	serverCapabilities["capabilities"]["documentSymbolProvider"] = true;
//	serverCapabilities["capabilities"]["selectionRangeProvider"] = false;
	serverCapabilities["capabilities"]["workspaceSymbolProvider"] = false;			// do this!
	serverCapabilities["capabilities"]["codeActionProvider"] = false;
	serverCapabilities["capabilities"]["codeLensProvider"] = false;
	serverCapabilities["capabilities"]["documentFormattingProvider"] = true;
	serverCapabilities["capabilities"]["documentRangeFormattingProvider"] = true;
	serverCapabilities["capabilities"]["documentOnTypeFormattingProvider"]["firstTriggerCharacter"] = stringi ( ";" );
	serverCapabilities["capabilities"]["documentOnTypeFormattingProvider"]["moreTriggerCharacter"][0] = stringi ( "}" ); 
	serverCapabilities["capabilities"]["documentOnTypeFormattingProvider"]["moreTriggerCharacter"][1] = stringi ( "\r" );
	serverCapabilities["capabilities"]["documentOnTypeFormattingProvider"]["moreTriggerCharacter"][2] = stringi ( "\n" );
//	serverCapabilities["capabilities"]["documentLinkProvider"] = true;
	serverCapabilities["capabilities"]["colorProvider"] = false;
	serverCapabilities["capabilities"]["foldingRangeProvider"] = true;
	serverCapabilities["capabilities"]["declarationProvider"] = false;
//	serverCapabilities["capabilities"]["executeCommandProvider"] = true;
	serverCapabilities["capabilities"]["workspace"]["workspaceFolders"]["supported"] = true;
	serverCapabilities["capabilities"]["workspace"]["workspaceFolders"]["changeNotifications"] = true;

	size_t index = 0;
	serverCapabilities["capabilities"]["semanticTokensProvider"]["full"] = true;
	serverCapabilities["capabilities"]["semanticTokensProvider"]["range"] = false;
	serverCapabilities["capabilities"]["semanticTokensProvider"]["legend"]["tokenModifiers"] = jsonElement ().makeArray ();

	std::map<int64_t, stringi > mappedValues = {
#define DEF(value, mappedValue ) { (int64_t)astLSInfo::semanticSymbolType::mappedValue, stringi ( #mappedValue ) },
		SEMANTIC_TOKEN_TYPES
#undef DEF
	};

	for ( auto &it : mappedValues )
	{
		serverCapabilities["capabilities"]["semanticTokensProvider"]["legend"]["tokenTypes"][index++] = std::get<1>(it);
	}

	return serverCapabilities;
}

static std::pair<stringi, jsonElement> publishDiagnostics ( jsonRPCServerBase &server, languageServer *ls )
{
	jsonElement rsp;

	auto lsFile = ls->activeFile;

	if ( lsFile )
	{
		rsp["diagnostics"] = jsonElement ().makeArray ();
		rsp["uri"] = lsFile->uri;

		size_t index = 0;
		for ( auto const &it : lsFile->info.errors )
		{
			jsonElement tmp;
			char hex[64];
			sprintf_s ( hex, sizeof ( hex ), "%I32x", int(it->errorData ()) );

			tmp["range"] = range ( it->location );
			tmp["severity"] = 1;
			tmp["source"] = "slangd";
			tmp["code"] = hex;
			tmp["message"] = scCompErrorAsText ( int(it->errorData ()) );

			rsp["diagnostics"][index++] = tmp;
		}
		for ( auto const &it : lsFile->info.warnings )
		{
			jsonElement tmp;
			char hex[64];
			sprintf_s ( hex, sizeof ( hex ), "%I32x", int(it->warnData ().num) );

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
					sprintf_s ( hex, sizeof ( hex ), "%I32x", int ( err.first) );

					tmp["range"] = range ( err.second->location );
					tmp["severity"] = 1;
					tmp["source"] = "slangd";
					tmp["code"] = hex;
					tmp["message"] = scCompErrorAsText ( int(err.first) );

					rsp["diagnostics"][index++] = tmp;
				}
			}
		}
	}

	return { "textDocument/publishDiagnostics", rsp };
}

static std::pair<stringi, jsonElement> logOutput ( jsonRPCServerBase &server, languageServer *ls )
{
	jsonElement	rsp;

	std::lock_guard g1 ( ls->sendNotificationLock );

	rsp["type"] = 4;				// 4, constant per spec... log level output
	rsp["message"] = stringi ( ls->logBuffer.data<char *> (), ls->logBuffer.size () );

	return { "window/logMessage", rsp };
}

static UINT languageServerOutputThread ( jsonRPCServerBase &server, languageServer *ls )
{
	char buffer[4096];
	size_t nRead;

	while ( 1 )
	{
		nRead = _read ( ls->readPipeDesc, buffer, sizeof ( buffer ) );
		if ( nRead )
		{
			{
				std::lock_guard g1 ( ls->sendNotificationLock );
				ls->logBuffer.write ( buffer, nRead );
			}
			activeLanguageServer->sendNotification ( (size_t)notifications::logOutput );
		}
	}
}

taskControl *startLanguageServer ( uint16_t port  )
{
	languageServer *ls = new languageServer ();

	vmTaskInstance instance ( "test" );

	auto [builtIn, builtInSize] = builtinInit ( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE
	ls->builtIn = builtIn;
	ls->builtInSize = builtInSize;

	auto svr = new socketServer<languageServer *> ( port, std::move ( ls ) );

	svr->setMethods	( 
						{
							{ "initialize", initialize },
							{ "initialized", initialized },
							{ "$/cancelRequest", cancel_request },
							{ "workspace/didChangeWatchedFiles",		workspaceDidChangeWatchedFile },
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
							{ "textDocument/references",				textDocumentReferences },
							{ "textDocument/rangeFormatting",			textDocumentRangeFormatting},
							{ "textDocument/formatting",				textDocumentFormatting},
							{ "textDocument/onTypeFormatting",			textDocumentFormatting},
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

	auto tc = new socketServerTaskControl<languageServer *>( HWND(0) );
	svr->startServer ( tc );

	return tc;
}

taskControl *startLanguageServer()
{
	languageServer *ls = new languageServer();

	vmTaskInstance instance( "test" );

	auto [builtIn, builtInSize] = builtinInit( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE
	ls->builtIn = builtIn;
	ls->builtInSize = builtInSize;

	auto svr = new lspConsoleServer ( std::move ( ls ) );

	svr->setMethods	(
						{
							{ "initialize", initialize },
							{ "initialized", initialized },
							{ "$/cancelRequest", cancel_request },
							{ "workspace/didChangeWatchedFiles",		workspaceDidChangeWatchedFile },
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
							{ "textDocument/references",				textDocumentReferences },
							{ "textDocument/rangeFormatting",			textDocumentRangeFormatting},
							{ "textDocument/formatting",				textDocumentFormatting},
							{ "textDocument/onTypeFormatting",			textDocumentFormatting},
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

	auto tc = new lspConsoleServerTaskControl<languageServer *> ( ls );
	svr->startServer( tc );

	return tc;
}
