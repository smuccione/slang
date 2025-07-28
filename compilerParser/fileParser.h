/*

	block parser definitions

*/

#pragma once

#include "Utility/settings.h"

#include <algorithm>
#include <string>
#include <stdint.h>
#include <cassert>
#include <vector>
#include <stack>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "symbolSpace.h"
#include "errorHandler.h"
#include "nameSpace.h"
#include "funcParser.h"
#include "funcParser.h"
#include "classParser.h"
#include "opSymbol.h"
#include "Utility/buffer.h"
#include "Utility/sourceFile.h"
#include "Utility/stringCache.h"
#include "compilerPreprocessor/compilerPreprocessor.h"

#define LAMBDA_ID "lambda@"

class compExecutable;

class linqProjectionVars
{
public:
	astNode *name = nullptr;
	astNode *expr = nullptr;

	linqProjectionVars ( )
	{
	}
	linqProjectionVars ( astNode *name ) : name ( name )
	{
	}
	linqProjectionVars ( astNode *name, astNode *expr ) : name ( name ), expr ( expr )
	{}
};

struct opListerSource
{
	stringi						 fName;
	std::vector<char const *>	 linePtr;
	std::vector<uint32_t>		 lineLength;
	uint32_t					 numLines = 0;
	uint32_t					 lastLine = 0;
	std::string					 text;
	std::stack<uint32_t>		 lineStack;

	opListerSource ( )
	{
	}

	virtual ~opListerSource ( )
	{}

	opListerSource ( opListerSource const &old )
	{
		fName = old.fName;

		linePtr.clear ( );
		lineLength = old.lineLength;
		lastLine = old.lastLine;
		text = old.text;

		// need to rebuild as we have a different c_str() from the new text
		build ( );
	}

	opListerSource ( opListerSource &&old ) noexcept
	{
		*this = std::move ( old );
	}

	opListerSource &operator = ( opListerSource const &old )
	{
		return *this = opListerSource ( old );
	}

	opListerSource &operator = ( opListerSource &&old ) noexcept
	{
		std::swap ( fName, old.fName );
		std::swap ( linePtr, old.linePtr );
		std::swap ( lineLength, old.lineLength );
		std::swap ( text, old.text );
		std::swap ( numLines, old.numLines );
		std::swap ( lastLine, old.lastLine );
		return *this;
	}

	opListerSource ( char const *fName )
	{
		lastLine = 0;
		numLines = 0;
		this->fName = fName;

		std::ifstream t ( fName );

		if ( t.bad ( ) )
		{
			return;
		}

		std::stringstream buffer;
		buffer << t.rdbuf ( );
		text = buffer.str ( );
		text += '\0';

		build ( );
	}

	opListerSource ( char const *fName, char const *code )
	{
		lastLine = 0;
		numLines = 0;
		this->fName = fName;
		text = code;
		text += '\0';

		build ();
	}

	char const *getLine ( uint32_t lineNum )
	{
		if ( lineNum && (lineNum < numLines) )
		{
			return (linePtr[lineNum]);
		}
		return 0;
	}
	uint32_t getLineLen ( uint32_t lineNum )
	{
		if ( lineNum && (lineNum < numLines) )
		{
			return (lineLength[lineNum]);
		}
		return 0;
	}
	void setLastLine ( uint32_t lineNum )
	{
		lastLine = lineNum;
	}
	void pushLine ( uint32_t lineNum )
	{
		lineStack.push ( lastLine );
		lastLine = lineNum;
	}
	void popLine ( )
	{
		lastLine = lineStack.top ( );
		lineStack.pop ( );
	}

private:

	void build ( )
	{
		uint32_t	 len;
		char const *ptr;

		numLines = 0;

		ptr = text.c_str ( );
		len = 0;
		linePtr.push_back ( text.c_str ( ) );			// start at 1 so push a 0th element
		lineLength.push_back ( 0 );

		linePtr.push_back ( text.c_str ( ) );
		while ( *ptr )
		{
			if ( *ptr == '\r' )
			{
				ptr++;
				lineLength.push_back ( len );
				if ( *ptr == '\n' )
				{
					ptr++;
				}
				linePtr.push_back ( ptr );
				len = 0;
			} else if ( *ptr == '\n' )
			{
				ptr++;
				lineLength.push_back ( len );
				linePtr.push_back ( ptr );
				len = 0;
			} else
			{
				len++;
				ptr++;
			}
		}
		lineLength.push_back ( len );
		numLines = (uint32_t) linePtr.size ( );
	}
};


using symbolSearchElementType = std::variant<class opFunction *, class opClass *, opSymbol *, class compClassElementSearch *>;

#if 0 || (1 && defined (NDEBUG))
//#if 1
// for speed
using functionListType = std::unordered_multimap<cacheString, class opFunction *>;
using classListType = std::unordered_multimap<cacheString, class opClass *>;
using symbolListType = std::unordered_multimap<cacheString, opSymbol>;
using staticElemListType = std::unordered_multimap<cacheString, class compClassElementSearch *>;
using dependencyListType = std::unordered_multiset<cacheString>;
using srcListType = std::unordered_map<stringi, opListerSource>;
using symbolSearchType = std::unordered_multimap<cacheString, symbolSearchElementType>;
#else
// for ease of debugging so we can find what we're looking for 
using functionListType = std::multimap<cacheString, class opFunction *>;
using classListType = std::multimap<cacheString, class opClass *>;
using symbolListType = std::multimap<cacheString, opSymbol>;
using staticElemListType = std::multimap<cacheString, class compClassElementSearch *>;
using dependencyListType = std::set<cacheString>;
using srcListType = std::map<stringi, opListerSource>;
using symbolSearchType = std::multimap<cacheString, symbolSearchElementType>;
#endif

struct macroDef
{
	std::vector<cacheString>	 params;
	astNode						*block;
};

struct tokenDef
{
	cacheString					 params;
	stringi						 dest;
};

struct moduleLibraryEntry
{
	cacheString					fName;
	bool						isLoaded = false;
	std::set<srcLocation>		locations;	

	moduleLibraryEntry ( cacheString const &fName, srcLocation const &location ) : fName ( fName )
	{
		locations.insert ( location );
	}
	moduleLibraryEntry ( class opFile *file, char const **expr, bool isLibrary );
	void serialize ( BUFFER *buff );
};

class opFile
{
public:
	// unordered is faster but it makes it a bitch to find what you're looking for in the debugger
	classListType								 classList;
	functionListType							 functionList;
	symbolListType						 		 symbols;				// all .data symbols regardless of scope... these have fully qualified names
	staticElemListType							 staticElemMap;			// all static class elements
	std::vector<moduleLibraryEntry>				 libraries;				// all libraries included with the LIBRARY statement
	std::vector<moduleLibraryEntry>				 modules;				// all modules included with the MODULE statement
	std::set<stringi>							 buildSources;			// all build sources - not disassembled libraries, but just the library, but including any .sl or .fgl files
	errorHandler								 errHandler;
	sourceFile									 srcFiles;
	opNamespace									 ns;

	symbolSearchType							 symSearch;				// unified symbol repository

	std::vector<std::unique_ptr<astNode>>		 autoFormatPoints;
	std::vector<std::unique_ptr<astNode>>		 statements;
	std::vector<std::unique_ptr<astNode>>		 warnings;
	std::vector<std::unique_ptr<astNode>>		 errors;

	dependencyListType							 dependenciesFromExternalSources;	// list of dependencies from external sources
	stringCache									 sCache;				// cache for all symbol names. by making all equivalent names the same string it makes comparison O(1) with a K of 1

	srcListType									 src;					// filled in on demand, used to cache source files for error and disassembly listings

	std::vector<macroDef>						 macros;		// #define macros
	std::vector<tokenDef>						 tokens;		// #define defines

	std::shared_mutex							 makeIteratorLock;		// controls threaded access to class/function/symbols, etc. lists

	opFile();
	opFile ( char const **expr );

	~opFile ( );

	// language region support for embedded code
	std::map<srcLocation, languageRegion::languageId>				languageRegions;
	std::vector<srcLocation>										getRegions ( srcLocation const &loc, languageRegion::languageId ) const;
	std::vector<std::pair<srcLocation, languageRegion::languageId>> getRegionsExcept ( srcLocation const &loc, languageRegion::languageId ) const;
	bool															isInRegion ( srcLocation const &location ) const;
	languageRegion::languageId										getRegion ( srcLocation const &loc ) const;

	void buildSymbolSearch ( bool reset )
	{
		if (reset)
		{
			symSearch.clear();
		}
		for ( auto &[name, func] : functionList )
		{
			symSearch.insert( { name, func } );
		}
		for ( auto &[name, cls] : classList )
		{
			symSearch.insert( { name, cls } );
		}
		for ( auto &[name, sym] : symbols )
		{
			symSearch.insert( { name, &sym } );
		}
		for ( auto &[name, elem] : staticElemMap )
		{
			symSearch.insert( { name, elem } );
		}
	}

	void clearSourceIndex ( size_t sourceIndex );

	void addBuildSource( stringi const& file)
	{
		std::filesystem::path p ( std::filesystem::absolute ( file.c_str() ));
		p.make_preferred();
		p.remove_filename();
		buildSources.insert(p.string());
	}

	void			 serialize ( BUFFER *buff, bool isLibrary );
	void			 loadLibrary		( char const *libName, bool isInterface, bool isLS );
	void			 loadLibraryDirect	( uint8_t const *comp, bool isInterface, bool isLS );
	void			 loadLibraries		( bool isInterface, bool isLS );
	void			 loadModules		( bool isInterface, bool isLS );
	void			 addStream			( char const **stream, bool isInterface, bool isLS );

	errorNum		 addElem ( class opFunction *func, symbolTypeClass const &type, bool isLS );
	errorNum		 addElem ( class opClass *classDef, bool isLS );
	errorNum		 addElem ( char const *lib, srcLocation &location, bool isModule, bool isLS );

	opClass			*findClass ( char const *qualName );

	void			 validate ( void );
	void			 validate ( class opFunction *func );

private:
	enum class lastOpType {
		symbol = 0,
		binary,
		unary,
		send
	};


	astNode			*_parseExpr				( source &src, bool sValid, bool onlySimpleExpressions, bool linqExpr, bool eCondValid, bool pairValid, opFunction *func, bool doBraces, bool isLS, bool isAP );
	astNode			*parseCaseExpr			( source &src, bool sValid, bool onlySimpleExpressions, opFunction *func, bool doBraces, bool isLS );
	astNode			*parseLinqExpr			( source &src, bool sValid, bool onlySimpleExpressions, opFunction *func, bool doBraces, bool isLS );
	astNode			*getLinq				( source &src, opFunction *func, bool isLS );
	astNode			*getNode				( source &src, lastOpType lastOp, opFunction *func, bool doBraces, bool isLS, bool isAP );
	astArray		*arrayDef				( source &src, char onlySimpleExpressions, opFunction *func, bool doBraces, bool isLS, bool isAP );
	astArray		*arrayJsonDef			( source &src, char onlySimpleExpressions, opFunction *func, bool doBraces, bool isLS, bool isAP );
	opListDef		*getOperator			( char const *name, uint32_t nParams );
	bool			 isContinuationSymbol	( source &src, opFile::lastOpType lastOp );
	opFunction		*_parseFunc				( source &src, char const *funcName, bool parseInitializers, bool autoMain, bool doBraces, bool cbParam, bool isLS, bool isAP, srcLocation const &formatStart, class opClass *classDef = 0 );
	opFunction		*parseFunc				( source &src, char const *funcName, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart );
	opFunction		*parseCBFunc			( source &src, char const *funcName, bool doBraces, bool isLS, bool isAP );
	opFunction		*parseAnonymousFunc		( source &src, char const *funcName, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart );
	opFunction		*parseFuncAutoMain		( source &src, char const *funcName, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart );

	opClass			*parseClass				( source &src, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart );
	bool			 parseInnerClass		( source &src, bool doBraces, opClass *classDef, bool isLS, bool isAP, srcLocation const &formatStart );
	void			 parseProperty			( source &src, opClass *oClass, bool isStatic, bool isVirtual, fgxClassElementScope scope, bool isLS, bool isAP, stringi const &documentation, srcLocation const &formatStart );
	class astNode	*parseBlockFGL			( source &src, opFunction *func, bool noImplied, bool isLS, bool isAP, srcLocation const &formatStart );
	class astNode	*parseBlockSlang		( source &src, opFunction *func, bool virtualBrace, bool isLS, bool isAP, srcLocation const &formatStart );
	class astNode	*parseBlock				( source &src, opFunction *func, bool doBrace, bool virtualBrace, bool isLS, bool isAP, srcLocation const &formatStart );

	void			 _parseFile				( source &src, bool doBraces, bool isLS, bool isAP );

	astNode			*makeLambda				( srcLocation &src, opFunction *func, std::vector<linqProjectionVars> &projVars, astNode *expr, astNode *secondParam = 0 );

public:
	void			 parseFile				( char const *fileName, char const *expr, bool doBraces, bool isLS, bool isAP, sourceFile::sourceFileType type );
	void			 parseFile				( source &src, bool doBraces, bool isLS, bool isAP );
	class astNode	*parseExpr				( source &src, bool sValid, bool onlySimpleExpressions, opFunction *func, bool doBraces, bool isLS, bool isAP );
	opFunction		*parseMethod			( source &src, class opClass *classDef, char const *name, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart );
	opFunction		*findFunc				( char const *name, char const *nSpace );
	opFunction		*findFunc				( char const *name )
	{
		return findFunc ( name, 0 );
	}

	std::pair<stringi, astNode *>	getDocumentation ( source &src );


	opListerSource &getSourceListing ( stringi const &fileName )
	{
		auto it = src.find ( fileName );
		if ( it == src.end ( ) )
		{
			src[fileName] = opListerSource ( fileName );
		}
		return src[fileName];
	}

	opListerSource &setSourceListing ( stringi const &fileName, stringi const &code )
	{
		src[fileName] = opListerSource ( fileName, code );
		return src[fileName];
	}
	friend class vmInstance;
	friend class vmNativeInterface;
	friend class compExecutable;

	friend astNode *compileCodeBlockCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool isInFunctionCall, class errorHandler *err );


	cacheString queryableValue = sCache.get ( stringi ( "queryable" ) );
	cacheString getEnumeratorValue = sCache.get ( stringi ( "getEnumerator" ) );
	cacheString selfValue = sCache.get ( stringi ( "self" ) );
	cacheString _selfValue = sCache.get ( stringi ( "_self" ) );
	cacheString varCopyValue = sCache.get ( stringi ( "varCopy" ) );
	cacheString unpackValue = sCache.get ( stringi ( "unpack" ) );
	cacheString exceptValue = sCache.get ( stringi ( "except" ) );
	cacheString aArrayValue = sCache.get ( stringi ( "aArray" ) );
	cacheString rangeValue = sCache.get ( stringi ( "range" ) );
	cacheString fixedArrayEnumeratorValue = sCache.get ( stringi ( "fixedArrayEnumerator") );
	cacheString stringEnumeratorValue = sCache.get ( stringi ( "stringEnumerator" ) );
	cacheString sparseArrayEnumeratorValue = sCache.get ( stringi ( "sparseArrayEnumerator") );
	cacheString fieldValue = sCache.get ( stringi ( "field" ) );
	cacheString barrierValue = sCache.get ( stringi ( "$barrier" ) ); 
	cacheString retValue = sCache.get ( stringi ( "$ret" ) ); 
	cacheString newValue = sCache.get ( stringi ( "new" ) );
	cacheString newBaseValue = sCache.get ( stringi ( "new_base" ) );
	cacheString releaseValue = sCache.get ( stringi ( "release") );
	cacheString releaseBaseValue = sCache.get ( stringi ( "release_base" ) );
	cacheString cNewValue = sCache.get ( stringi ( "$new" ) );
	cacheString cReleaseValue = sCache.get ( stringi ( "$release" ) );
	cacheString _packValue = sCache.get ( stringi ( "_pack" ) );
	cacheString _unpackValue = sCache.get ( stringi ( "_unpack" ) );
	cacheString debugBreakValue = sCache.get ( stringi ( "debugBreak" ) );
	cacheString defaultMethodValue = sCache.get ( stringi ( "defaultMethod" ) );
	cacheString defaultPropValue = sCache.get ( stringi ( "defaultProp" ) );
	cacheString defaultValue = sCache.get ( stringi ( "default" ) );
	cacheString pcountValue = sCache.get ( stringi ( "pcount" ) );
	cacheString paramValue = sCache.get ( stringi ( "param" ) );
	cacheString typeValue = sCache.get ( stringi ( "type" ) );
	cacheString typeXValue = sCache.get ( stringi ( "typeEX" ) );
	cacheString typeOfValue = sCache.get ( stringi ( "typeof" ) );
	cacheString outputStringValue = sCache.get ( stringi ( "__outputString" ) );
	cacheString mainValue= sCache.get( stringi( "main" ) );
};

void inline BS_ADVANCE_EOL_COMMENT ( opFile *file, bool isLS, source &src )
{
	do
	{
		src.bsAdvanceEol ( true );
		if ( *src == ';' )
		{
			if ( isLS ) file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
			src++;
		} else 	if ( (src[0] == '/' && src[1] == '/') || (src[0] == '/' && src[1] == '*') )
		{
			auto [_, commentStatement] = file->getDocumentation ( src );

			assert ( commentStatement );

			file->statements.push_back ( std::unique_ptr<astNode>( commentStatement ) );
		} else
		{
			break;
		}
	} while ( 1 );
}

void inline BS_ADVANCE_COMMENT ( opFile *file, bool isLS, source &src )
{
	do
	{
		src.bsAdvance ();
		if ( (src[0] == '/' && src[1] == '/') || (src[0] == '/' && src[1] == '*') )
		{
			auto [_, commentStatement] = file->getDocumentation ( src );

			assert ( commentStatement );

			file->statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
		} else
		{
			break;
		}
	} while ( 1 );
}

void inline BS_ADVANCE_EOL ( opFile *file, bool isLS, source &src )
{
	do
	{
		src.bsAdvanceEol ( true );
		if ( *src == ';' )
		{
			if ( isLS )	 file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
			src++;
		} else
		{
			break;
		}
	} while ( 1 );
}

void inline BS_ADVANCE_EOL_STATEMENT ( opFile *file, bool isLS, source &src, srcLocation &assocLocation )
{
	do
	{
		src.bsAdvanceEol ( true );
		if ( *src == ';' )
		{
			if ( isLS )	file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
			if ( isLS ) file->autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, assocLocation ) ); 	// unindent point
			src++;
		} else
		{
			break;
		}
	} while ( 1 );
}

void inline BS_ADVANCE_EOL_NO_SEMI ( opFile *file, bool isLS, source &src )
{
	do
	{
		src.bsAdvanceEol ( true );
		if ( (src[0] == '/' && src[1] == '/') || (src[0] == '/' && src[1] == '*') )
		{
			auto [_, commentStatement] = file->getDocumentation ( src );

			assert ( commentStatement );

			file->statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
		} else
		{
			break;
		}
	} while ( 1 );
}


