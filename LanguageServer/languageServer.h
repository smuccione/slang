#pragma once

#include "Utility/settings.h"

#include <thread>
#include <tuple>
#include <map>
#include <vector>
#include <set>
#include <list>
#include <cstdio>
#include <variant>

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
	int64_t			version = 0;
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

	stringi				 activeFile;
	stringi				 activeUri;

	opFile				 file;			// need an opFile to hold everything.

	std::map<stringi, std::pair<int64_t, bool>>	fileVersions;

	stringi				 baseLibrary;

	bool				 needSendErrors = false;
	astLSInfo			 info;

	compExecutable		*compiler = nullptr;

	languageServerFile ()
	{	}

	~languageServerFile ()
	{
		if ( compiler ) delete compiler;
	}

	void updateSource ();
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
#define LS_FORMATTING_FLAGS \
	DEF ( indentMulilineExpressions, true, "indent multi-line expressions" ) \
	DEF ( indentCaseContents, true, "indent case contents" ) \
	DEF ( indentCaseLabels, true, "indent case labels" ) \
	DEF ( indentNamespaceContents, true, "indent namespace contents" ) \
	DEF ( indentClassContents, true, "indent class contents" ) \
	DEF ( indentBracesFollowingCase, true, "indent open brace follwing a case statement" ) \
	DEF ( indentBracesOfLambdaBody, true, "indent braces of lambda body" ) \
	DEF ( labelIndented, false, "indent label statement" ) \
	DEF ( indentBracesHalf, false, "indent case & case body one half tab-width" ) \
	DEF ( indentAccessSpecifiers, true, "indent access specifieers" ) \
	DEF ( indentAccessSpecifiersHalf, true, "indent access specfifiers one-half tab width" ) \
	DEF ( preserveCommentIndentation, false, "preserve comment indentation" ) \
	DEF ( newLineNamespaceBrace, true, "open brace for a namespace should be on a new line" ) \
	DEF ( newLineControlBrace, true, "open brace for a control statement should be on a new line" ) \
	DEF ( newLineClassBrace, true, "open brace for a class should be on a new line" ) \
	DEF ( newLineFunctionBrace, false, "open brace for a function should be on a new line" ) \
	DEF ( newLineBlockBrace, true, "open brace for a block should be on a new line" ) \
	DEF ( newLineLambdaBrace, true, "open brace for a lambda should be on a new line" ) \
	DEF ( newLineAfterScope, true, "closing brace should be on a new line" ) \
	DEF ( newLineCloseBrace, true, "open brace for a namespace should be on a new line" ) \
	DEF ( newLineEmptyBodyBrace, false, "closing brace of an empty block should be on a new line" ) \
	DEF ( newLineCatch, false, "catch statement should be on a new line" ) \
	DEF ( newLineFinally, false, "finally statment should be on a new line" ) \
	DEF ( newLineElse, false, "else statement should be on a new line" ) \
	DEF ( newLineWhile, false, "while statement should be on a new line" ) \
	DEF ( alignPerenthesis, true, "align perentheses" ) \
	DEF ( alignCurlyBraces, true, "align curly braces" ) \
	DEF ( alignMessageSend, false, "align message send operators" ) \
	DEF ( spaceBetweenFunctionNamePerenthesis, true, "insert space between function name and open perentheses" ) \
	DEF ( spaceWithinArgumentList, true, "insert space between elements in an argument list" ) \
	DEF ( spaceWithingEmptyArgumentList, false, "insert space within an empty argument list" ) \
	DEF ( spaceBetweenKeywordAndCondition, true, "insert space between keyword and open brace of a condition" ) \
	DEF ( spaceWithinCondition, true, "insert space within condtion" ) \
	DEF ( spaceWithinPerenthesizedExpression, false, "insert space wthin a perenthesized expression" ) \
	DEF ( spaceBeforeOpeningBlock, true, "insert space before an opening block" ) \
	DEF ( spaceWithinEmptyBlock, false, "insert space within an empty block" ) \
	DEF ( spacebeforeArrayContentsAccess, false, "insert space before an array dereference" ) \
	DEF ( spaceWithinArrayAccess, false, "insert space within an array dereference" ) \
	DEF ( spaceWithinArray, true, "insert space within an array definition" ) \
	DEF ( spaceWithinJSONObject, true, "insert space within a json definition" ) \
	DEF ( groupMultiDimensionalArraysTogether, true, "group multi-dimensional arrays together" ) \
	DEF ( spaceBeforeCommas, false, "insert space before commas" ) \
	DEF ( spaceAftereCommas, true, "insert space after commas" ) \
	DEF ( spaceBeforeMemberOperator, false, "insert space before member operator" ) \
	DEF ( spaceAfterMemberOperator, false, "insert space after member operator" ) \
	DEF ( spaceAfterSemicolons, false, "insert space after semicolons" ) \
	DEF ( spaceBetweenUnaryOperatorAndOperand, false, "insert space between unary operator and operand" ) \
	DEF ( spaceAroundOperators, true, "insert space around binary operators" ) \
	DEF ( spaceAroundRangeOperator, false, "insert space around range operator" ) \
	DEF ( spaceAroundAssignmentOperators, true, "insert space around assignment operators" ) \
	DEF ( spaceAroundConditionalOperators, true, "insert space around conditional operators" ) \
	DEF ( spaceBetweenAdjoiningCommas, false, "insert space between adjoining commas" ) \
	DEF ( spaceAroundPairOperator, true, "insert space around pair operator" ) \
	DEF ( capitalizeDeclarators, true, "capitilize declaration statements" ) \

#define LS_CONFIGURATION_SETTINGS \
	DEF ( inlayHints, true, "enable inlayHints" ) \
	DEF ( inlayHintsParameterNames, true, "hint parameter names" ) \
	DEF ( inlayHintsVariableTypes, true, "hint variable types" ) \
	DEF ( inlayHintsFunctionReturnType, true, "hint function return types" ) \
	DEF ( inlayHintsTrailingTypes, true, "use trailing types" ) \

#define LS_GENERAL_SETTINGS \
	DEF ( apAreSLANG, true, "process .ap files as .aps" ) \

	struct formatingFlags
	{
#undef DEF
#define DEF( name, state, description ) std::variant<bool, stringi>  name = state;
		LS_FORMATTING_FLAGS
#undef DEF
	} formatFlags;

	struct configurationFlags
	{
#undef DEF
#define DEF( name, state, description ) std::variant<bool, stringi> name = state;
		LS_CONFIGURATION_SETTINGS
		LS_GENERAL_SETTINGS
#undef DEF
	} configFlags;


#if _DEBUG
	static void makeJsonFormatterSettings ( BUFFER &buff, stringi const &name, bool state, stringi const &description )
	{
		// generate the below
		/**
			"slang.format.indentCaseContents": {
				"type": "boolean",
					"default" : "true",
					"description" : "Indents a case statement on format."
			},
		*/

		buff.printf ( "\"slang.format.%s\": {\r\n", name.c_str () );
		buff.printf ( "	\"type\": \"boolean\",\r\n" );
		buff.printf ( "		\"default\": %s,\r\n", state ? "true" : "false" );
		buff.printf ( "		\"description\": %s\r\n", description.c_str() );
		buff.printf ( "},\r\n" );
	}
	static void makeJsonInlaySettings ( BUFFER &buff, stringi const &name, bool state, stringi const &description )
	{
		// generate the below
		/**
			"slang.format.indentCaseContents": {
				"type": "boolean",
					"default" : "true",
					"description" : "Indents a case statement on format."
			},
		*/

		buff.printf ( "\"slang.inlay.%s\": {\r\n", name.c_str () );
		buff.printf ( "	\"type\": \"boolean\",\r\n" );
		buff.printf ( "		\"default\": %s,\r\n", state ? "true" : "false" );
		buff.printf ( "		\"description\": %s\r\n", description.c_str () );
		buff.printf ( "},\r\n" );
	}
	static void makeJGeneralSettings ( BUFFER &buff, stringi const &name, bool state, stringi const &description )
	{
		// generate the below
		/**
			"slang.format.indentCaseContents": {
				"type": "boolean",
					"default" : "true",
					"description" : "Indents a case statement on format."
			},
		*/

		buff.printf ( "\"slang.general.%s\": {\r\n", name.c_str () );
		buff.printf ( "	\"type\": \"boolean\",\r\n" );
		buff.printf ( "		\"default\": %s,\r\n", state ? "true" : "false" );
		buff.printf ( "		\"description\": %s\r\n", description.c_str () );
		buff.printf ( "},\r\n" );
	}

	static void makeJsonSettings ()
	{
		BUFFER buff;
		buff.printf ( R"(		"configuration": [)" );

		// formatting settings
		buff.printf ( R"(
      {
        "title": "formatter",
        "properties": {
)" );
#undef DEF
#define DEF( name, state, description ) makeJsonFormatterSettings ( buff, #name, state, #description );
		LS_FORMATTING_FLAGS;
#undef DEF
		buff.printf ( R"(
			}
		}
)" );
		// Inlay Hints
		buff.printf ( R"(
      {
        "title": "Inlay Hints",
        "properties": {
)" );
#undef DEF
#define DEF( name, state, description ) makeJsonInlaySettings ( buff, #name, state, #description );
		LS_CONFIGURATION_SETTINGS;
#undef DEF
		buff.printf ( R"(
			}
		}
)" );
		// General Settingss
		buff.printf ( R"(
      {
        "title": "General Settings",
        "properties": {
)" );
#undef DEF
#define DEF( name, state, description ) makeJGeneralSettings ( buff, #name, state, #description );
		LS_GENERAL_SETTINGS;
#undef DEF
		buff.printf ( R"(
			}
		}
)" );

		//  done
		buff.printf ( R"(
		],
)" );

		buff.put ( 0 );
		printf ( "%s\r\n", buff.data<char *>() );
	}
#endif
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

	enum class targetType
	{
		file,
		library
	};

	std::map<stringi, stringi>											fileToTargetMap;
	std::map<stringi, std::vector<std::pair<stringi, targetType>>>		targetFiles;

	std::map<stringi, std::unique_ptr<languageServerFile>>				targets;

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

	std::map<stringi, std::variant<bool, stringi> &> configValue = {
#undef DEF
#define DEF( name, state, description ) {#name, formatFlags.name},
	LS_FORMATTING_FLAGS
#undef DEF
#undef DEF
#define DEF( name, state, description )  {#name, configFlags.name},
	LS_CONFIGURATION_SETTINGS
#undef DEF
	};


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

extern std::tuple<stringi, int64_t, srcLocation> formatCode ( opFile *file, astLSInfo const &info, char const *src, srcLocation const &loc, int64_t defaultIndent, languageRegion::languageId const languageId, int64_t tabWidth, bool preferSpaces, languageServer::formatingFlags const &flags );
extern class taskControl *startLanguageServer ( uint16_t port );
extern class taskControl *startLanguageServer ();
