/*

	Byte Code Generator

*/

#pragma once

#include "Utility/settings.h"

#include <stack>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <string>
#include <variant>

#include "Utility/stringCache.h"
#include "Utility/stringi.h"
#include "Utility/sourceFile.h"
#include "symbolType.h"

class opForceScan
{
	class opFunction *func;
	public:
	opForceScan ( class opFunction *func ) : func ( func )
	{	}
	operator class opFunction *() const
	{
		return func;
	}
	inline size_t hash () const noexcept
	{
		return reinterpret_cast<size_t>(func);
	}
};

namespace std
{
	template <>
	class hash<opForceScan>
	{
		public:
		size_t operator()( const opForceScan &obj ) const noexcept
		{
			return obj.hash ();
		}
	};
}

using accessorType = std::variant<std::monostate, class opFunction *, class opClassElement *, class opSymbol *, opForceScan, struct makeIteratorType *>;
using symbolSource = std::variant<std::monostate, class opFunction *, class opClassElement *, class symbolParamDef *, class symbol *, class opSymbol *, class opClass *, class astNode *>;

struct makeIteratorType
{
	class opFunction	*function;
	accessorType		 acc;
	bool				 generic;

	~makeIteratorType()
	{
		if ( std::holds_alternative<struct makeIteratorType *>( acc ) )
		{
			delete std::get<struct makeIteratorType *>( acc );
		}
	}
};

#define SEMANTIC_TOKEN_TYPES \
DEF ( none, none ) \
DEF ( macro, macro ) \
DEF ( variable, variable ) \
DEF ( parameter, parameter ) \
DEF ( type, type ) \
DEF ( iterator, iterator ) \
DEF ( function, function ) \
DEF ( name, name ) \
DEF ( method, method ) \
DEF ( methodIterator, methodIterator ) \
DEF ( property, property ) \
DEF ( cls, cls ) \
DEF ( inherit, inherit ) \
DEF ( ivar, ivar ) \
DEF ( ns, ns ) \
DEF ( label, label ) \
DEF ( customLiteral, customLiteral ) \
DEF ( numberLiteral, numberLiteral ) \
DEF ( stringLiteral, stringLiteral ) \
DEF ( operatorOverload, operatorOverload ) \
DEF ( newOperator, newOperator ) \
DEF ( field, field ) \
DEF ( self, self ) \
DEF ( super, super ) \
DEF ( punctuation, punctuation ) \
DEF ( keywordFlow, keywordFlow ) \
DEF ( keywordOperator, keywordOperator ) \
DEF ( keywordOther, keywordOther ) \
DEF ( comment, comment ) \
DEF ( commaSeparator, punctuation ) \
DEF ( openPeren, keywordOperator ) \
DEF ( closePeren, keywordOperator ) \
DEF ( startCodeblock, keywordOperator ) \
DEF ( endCodeblock, keywordOperator ) \
DEF ( startAnonymousClass, none ) \
DEF ( endAnonymousClass, none ) \
DEF ( startArrayAccess, keywordOperator ) \
DEF ( endArrayAccess, keywordOperator ) \
DEF ( startFixedArray, punctuation ) \
DEF ( endFixedArray, punctuation ) \
DEF ( startVariableArray, punctuation ) \
DEF ( endVariableArray, punctuation ) \
DEF ( startJson, punctuation ) \
DEF ( endJson, punctuation ) \
DEF ( pairSeparator, commaSeparator ) \
DEF ( cbParamSeparator, commaSeparator ) \
DEF ( semicolon, punctuation ) \
DEF ( colon, keywordOperator ) \
DEF ( conditional, keywordOperator ) \
DEF ( expressionFunctionDelimiter, keywordOther ) \
DEF ( funcCallStart, keywordOperator ) \
DEF ( funcCallEnd, keywordOperator ) \
DEF ( info, none ) \
DEF ( last, last) /* MUST be the LAST value!!! */ 

class astLSInfo {
	public:

	enum class semanticSymbolType {
#define DEF(value, mappedValue ) value,
		SEMANTIC_TOKEN_TYPES
#undef DEF
	};

	inline static std::map<stringi, astLSInfo::semanticSymbolType > nameToSemanticTokenType = {
#define DEF(value, mappedValue ) { stringi ( #mappedValue ), astLSInfo::semanticSymbolType::mappedValue },
		SEMANTIC_TOKEN_TYPES
#undef DEF
	};


	// ORDER is important... when sorted, the scalar enumerator is compared and sorted based up this, so breakTypes that are order important (afterJsonContens, afterJson) must occur in that order in the enum
	enum class semanticLineBreakType
	{
		none = 0,
		beforeEnd,
		afterEnd,
		afterBlockEnd,
		afterCondition,			/*must come before closePeren */
		beforeNamespaceName,
		afterNamespaceName,
		beforeNamespace,
		afterNamespace,
		beforeFor,
		beforeIf,
		beforeElse,
		beforeRepeat,
		afterRepeat,
		beforeWhile,
		beforeSwitch,
		beforeSwitchBody,
		beforeCase,
		afterCase,
		beforeCaseContents,
		afterCaseContents,
		beforeDefault,
		beforeFunction,
		afterFunction,
		beforeClass,
		afterClass,
		beforeElement,
		afterElement,
		beforeArrayContents,
		afterArrayContents,
		afterArray,
		beforePerenContents,
		afterPerenContents,
		afterPeren,
		beforeJsonContents,
		afterJsonContents,
		afterJson,
		beforeOpenBlock,
		afterOpenBlock,
		afterBlock,
		beforeImpliedBlock,
		afterImpliedBlock,
		beforeScope,
		afterScope,
		beforeCodeblock,
		afterCodeblock,
		beforeStatement,
		afterStatement,
		beforeLabel,
		afterLabel,
		beforeTry,
		afterTry,
		beforeCatch,
		afterCatch,
		beforeFinally,
		afterFinally,
		beforeLinq,
		afterLinq,
		beforeSendMsg,
		afterSendMsg,
		fglBreak,
		withinCondition,				/* must come AFTER open peren */
		autoFormatPoint,
	};

	static inline semanticSymbolType mappedValues[(size_t)semanticSymbolType::last + 1] = {
#define DEF(value, mappedValue ) semanticSymbolType::mappedValue,
		SEMANTIC_TOKEN_TYPES
#undef DEF
	};

	class symbolRange {
		public:
		accessorType			 acc;
		semanticSymbolType		 type;
		srcLocation				 location;
		symbolSource			 data;
		bool					 isAccess;
		symbolTypeClass			 compType;
		astNode					*node = nullptr;			// used for selection provider
		bool					 isInFunctionCall = false;

		bool operator == ( symbolRange const &right ) const noexcept
		{
			return location == right.location;
		}

		bool operator < ( symbolRange const &right ) const noexcept
		{
			if ( type == semanticSymbolType::info )
			{
				if ( right.type == semanticSymbolType::info )
				{
					return int( type ) < int ( right.type );
				}
				return true;
			} else if ( right.type == semanticSymbolType::info )
			{
				return false;
			}
			return location < right.location;
		}
	};

	class symbolReference
	{
		public:
		srcLocation		 location;
		cacheString		 name;
		astNode			*node;
		symbolSource	 source;

		bool operator == ( symbolReference const &right ) const noexcept
		{
			return location == right.location;
		}

		bool operator < ( symbolReference const &right ) const noexcept
		{
			return location < right.location;
		}
	};

	class signatureHelpDef {
		public:
		srcLocation		 location;
		opFunction		*func;
		astNode			*node;

		bool operator == ( signatureHelpDef const &right ) const noexcept
		{
			return location == right.location;
		}

		bool operator < ( signatureHelpDef const &right ) const noexcept
		{
			return location < right.location;
		}
	};

	class objectCompletions {
		public:
		srcLocation		location;
		symbolTypeClass	type;

		bool operator == ( objectCompletions const &right ) const noexcept
		{
			return location == right.location;
		}

		bool operator < ( objectCompletions const &right ) const noexcept
		{
			return location < right.location;
		}
	};


	class symbolDefinition
	{
		public:
		srcLocation		 location;
		cacheString		 name;
		opFunction		*func;
		symbolTypeClass	 type;

		bool operator == ( symbolDefinition const &right ) const noexcept
		{
			return location == right.location;
		}

		bool operator < ( symbolDefinition const &right ) const noexcept
		{
			return location < right.location;
		}
	};

	template<typename T>
	class dummy : public std::set<T>
	{
		public:
		auto insert ( T const value )
		{
			if ( std::set<T>::size () == 24 )
			{
//				printf ( "" );
			}
			return std::set<T>::insert ( value );
		}
	};

	//dummy<symbolRange>			semanticTokens;
	std::set<symbolRange>			semanticTokens;

	std::set<signatureHelpDef>		signatureHelp;
	std::set<symbolDefinition>		symbolDefinitions;
	std::set<symbolReference>		symbolReferences;
	std::vector<astNode *>			statementNodes;
	std::set<srcLocation>			foldingRanges;
	std::vector<astNode *>			errors;
	std::vector<astNode *>			warnings;
	std::set<objectCompletions>		objCompletions;

	void clear ()
	{
		signatureHelp.clear ();
		semanticTokens.clear ();
		symbolReferences.clear ();
		foldingRanges.clear ();
		statementNodes.clear ();
		errors.clear ();
		warnings.clear ();
		objCompletions.clear ();
		symbolDefinitions.clear ();
	}
};

