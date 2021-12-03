#pragma once

#include "Utility/settings.h"

#include <thread>
#include <tuple>
#include <map>
#include <vector>
#include <set>
#include <list>
#include <cstdio>

#include <io.h>
#include <fcntl.h>

#include "compilerParser/fileParser.h"
#include "Utility/stringi.h"
#include "Utility/jsonParser.h"

struct nodeLookup
{
	cacheString		 name;
	astNode *node;
};

struct languageServerFileData
{
	enum class languageType
	{
		unknown,
		slang,
		fgl,
		ap_slang,
		ap_fgl
	}				langage = languageType::unknown;
	stringi			source = "";
	int64_t			version = 0;;
};


struct languageServerFileCache
{
	std::map<stringi, languageServerFileData>	cache;

	auto getVersion ( stringi const &fName )
	{
		return cache[fName].version;
	}
	auto &getCode ( stringi const &fName )
	{
		return cache[fName].source;
	}
	auto getLanguage ( stringi const &fName )
	{
		return cache[fName].langage;
	}
	void update ( stringi const &fName, stringi const &code, int64_t version )
	{
		cache[fName].source = code;
		cache[fName].version = version;
	}
	void update ( stringi const &fName, stringi const &code, int64_t version, languageServerFileData::languageType languageId )
	{
		cache[fName].source = code;
		cache[fName].version = version;
		cache[fName].langage = languageId;
	}
	bool has ( stringi const &fName )
	{
		return cache.find ( fName ) != cache.end ();
	}
	void clear ()
	{
		cache.clear ();
	}
};

inline languageServerFileCache lsFileCache;

struct languageServerFile
{
	enum class CompletionItemKind : int64_t
	{
		Text = 1,
		Method = 2,
		Function = 3,
		Constructor = 4,
		Field = 5,
		Variable = 6,
		Class = 7,
		Interface = 8,
		Module = 9,
		Property = 10,
		Unit = 11,
		Value = 12,
		Enum = 13,
		Keyword = 14,
		Snippet = 15,
		Color = 16,
		File = 17,
		Reference = 18,
		Folder = 19,
		EnumMember = 20,
		Constant = 21,
		Struct = 22,
		Event = 23,
		Operator = 24,
		TypeParameter = 25
	};

	enum class SymbolKind : int64_t
	{
		File = 1,
		Module = 2,
		Namespace = 3,
		Package = 4,
		Class = 5,
		Method = 6,
		Property = 7,
		Field = 8,
		Constructor = 9,
		Enum = 10,
		Interface = 11,
		Function = 12,
		Variable = 13,
		Constant = 14,
		String = 15,
		Number = 16,
		Boolean = 17,
		Array = 18,
		Object = 19,
		Key = 20,
		Null = 21,
		EnumMember = 22,
		Struct = 23,
		Event = 24,
		Operator = 25,
		TypeParameter = 26
	};

	stringi				 name;
	stringi				 uri;
	opFile				 file;			// need an opFile to hold everything.

	std::map<stringi, int64_t>	fileVersions;

	bool				 needSendErrors = false;
	astLSInfo			 info;

	compExecutable *compiler = nullptr;

	languageServerFile ()
	{	}

	~languageServerFile ()
	{
		if ( compiler ) delete compiler;
	}

	void updateSource ()
	{
		LARGE_INTEGER		 start;
		LARGE_INTEGER		 parse;
		LARGE_INTEGER		 query;
		LARGE_INTEGER		 end;
		LARGE_INTEGER		 frequency;

		QueryPerformanceCounter ( &start );

		bool doBuild = false;

		for ( auto &[fileName, version] : fileVersions )
		{
			if ( lsFileCache.getVersion ( fileName ) != version )
			{
				fileVersions.find ( name )->second = 0;						// if any child has changed make sure we rebuild our primary file even if the version hasn't changed as dependencies may well have
				info.clear ();
			}
		}

		for ( auto &[fileName, version] : fileVersions )
		{
			if ( lsFileCache.getVersion ( fileName ) != version )
			{
				doBuild = true;

				auto sourceIndex = file.srcFiles.getIndex ( fileName );
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
				file.parseFile ( fileName, code, isSlang, true );
				QueryPerformanceCounter ( &parse );
				free ( code );

				version = lsFileCache.getVersion ( fileName );
			}
		}

		if ( doBuild )
		{
			try
			{
				file.loadLibraries ( false, true );
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
			compiler->getInfo ( info, file.srcFiles.getIndex ( name ) );
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
};

#define NOTIFICATIONS \
	DEF ( publishDiagnostics ) \
	DEF ( logOutput ) \

enum class notifications : size_t
{
#define DEF(a) a,
	NOTIFICATIONS
#undef DEF
};


struct languageServer
{
	struct formatingFlags
	{
		bool	indentCaseContents = true;					// verified
		bool	indentCaseLabels = true;					// verified
		bool	indentNamespaceContents = true;				// implemented
		bool	indentClassContents = true;					// implemented
		bool	indentBracesFollowingCase = true;
		bool	indentBracesOfLambdaBody = true;
		bool	labelIndented = false;						// implemented
		bool	indentAccessSpecifiers = true;
		bool	preserveCommentIndentation = false;
		bool	newLineNamespaceBrace = true;
		bool	newLineControlBrace = true;
		bool	newLineClassBrace = true;
		bool	newLineFunctionBrace = false;
		bool	newLineBlockBrace = true;
		bool	newLineLambdaBrace = true;
		bool	newLineScopeBrace = true;
		bool	newLineCloseBrace = true;
		bool	newLineEmptyBodyBrace = false;
		bool	newLineCatch = false;
		bool	newLineFinally = false;
		bool	newLineElse = false;
		bool	newLineWhile = false;
		bool	alignPerenthesis = true;
		bool	spaceBetweenFunctionNamePerenthesis = true;		// verified
		bool	spaceWithinArgumentList = true;
		bool	spaceWithingEmptyArgumentList = false;
		bool	spaceBetweenKeywordAndCondition = true;
		bool	spaceWithinCondition = true;
		bool	spaceWithinPerenthesizedExpression = false;
		bool	spaceBeforeOpeningBlock = true;
		bool	spaceWithinEmptyBlock = false;
		bool	spacebeforeArrayContentsAccess = false;
		bool	spaceWithinArrayAccess = false;					// verified
		bool	spaceWithinArray = true;						// verified
		bool	spaceWithinJSONObject = true;					// verified
		bool	groupMultiDimensionalArraysTogether = true;
		bool	spaceBeforeCommas = false;						// verified
		bool	spaceAftereCommas = true;						// verified
		bool	spaceBeforeMemberOperator = false;				// verified
		bool	spaceAfterMemberOperator = false;				// verified
		bool	spaceAfterSemicolons = false;;					// verified
		bool	spaceBetweenUnaryOperatorAndOperand = false;	// verified
		bool	spaceAroundOperators = true;					// verfiied
		bool	spaceAroundRangeOperator = false;				// verified
		bool	spaceAroundAssignmentOperators = true;			// verified
		bool	spaceAroundConditionalOperators = true;			// verified
		bool	spaceBetweenAdjoiningCommas = false;			// verified
		bool	spaceAroundPairOperator = true;					// verified
		bool	capitalizeDeclarators= true;					// verified
	} formatFlags;

	struct
	{
		struct
		{
			bool applyEdit = false;
			struct
			{
				bool documentChanges = false;
				bool create = false;
				bool rename = false;
				bool del = false;
			} edit;
		} workspace;
		bool didChangeConfiguration = false;
		bool didChangeWatchedFiles = false;
	} capabilities;

	std::vector<std::pair<stringi, stringi>> workspaceFolders;

	stringi rootPath;
	stringi rootURI;

	std::map<stringi, stringi>					fileToTargetMap;
	std::map<stringi, std::vector<stringi>>		targetFiles;

	std::map<stringi, std::unique_ptr<languageServerFile>>		targets;

	languageServerFile *activeFile = nullptr;

	uint8_t	const		*builtIn = nullptr;
	size_t				 builtInSize{};

	/* support for notifications (async callbacks from the server to the client) */
	std::mutex			 sendNotificationLock;
	HANDLE				 readHandle;
	HANDLE				 writeHandle;
	int					 writePipeDesc;
	int					 readPipeDesc;
	BUFFER				 logBuffer;

	void reset ()
	{
		targets.clear ();
		fileToTargetMap.clear ();
		targetFiles.clear ();
		rootPath.clear ();
		rootURI.clear ();
		workspaceFolders.clear ();
		activeFile = nullptr;
	}

	languageServer ()
	{
		CreatePipe ( &readHandle, &writeHandle, 0, 0 );

		writePipeDesc = _open_osfhandle ( (intptr_t)writeHandle, _O_APPEND | _O_WRONLY );
		readPipeDesc = _open_osfhandle ( (intptr_t)readHandle, _O_RDONLY );
	}
	~languageServer ()
	{
		CloseHandle ( readHandle );
		CloseHandle ( writeHandle );
	}
};


extern class taskControl *startLanguageServer ( uint16_t port );
extern class taskControl *startLanguageServer ();
