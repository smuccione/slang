/*
		nammespace support


*/

#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <stack>
#include <unordered_map>
#include <stack>
#include <unordered_set> 
#include "Utility/stringCache.h"
#include "compilerParser/opSymbol.h"
#include "util.h"

class opUsingList {
	std::vector<cacheString>	useList;

public:
	opUsingList()
	{
	};
	opUsingList ( const opUsingList &old )
	{
		useList = old.useList;
	}
	opUsingList ( opUsingList &&old ) noexcept
	{
		useList.swap ( old.useList );
	}
	opUsingList & operator = ( const opUsingList &old )
	{
		useList = old.useList;
		return *this;
	}
	opUsingList & operator = ( opUsingList &&old ) noexcept
	{
		useList.swap ( old.useList );
		return *this;
	}
	opUsingList ( class opFile *file, char const **expr );

	void serialize ( BUFFER *buff )
	{
		bufferPut32 ( buff, (uint32_t)useList.size() );
		for ( auto it = useList.begin(); it != useList.end(); it++ )
		{
			it->serialize ( buff );
		}
	}
	void add ( std::vector<cacheString> *inUse )
	{
		for ( auto it = inUse->begin(); it != inUse->end(); it++ )
		{
			if ( *it != "") 
			{
				if ( (useList.size () != 1) || (useList.back () != "") )
				{
					useList.emplace_back ( *it );
				} else
				{
					useList[0] = *it;
				}
			}
		}
	}

	friend class opNamespace;
};

class opNamespace {
	std::vector<cacheString>	 nameSpaces;
	std::stack<cacheString>		 nsStack;		// namespace currently being defined
	std::vector<cacheString>	 inUse;			// using namespaces

	class opFile				*file = nullptr;

	class opFunction			*lastFuncCache = nullptr;
public:
	opNamespace ();

	// serialization/deserialization code 
	opNamespace ( class opFile *file, char const ** expr );
	opNamespace ( opNamespace &&old ) noexcept;						// move constructor for deserialization

	opNamespace & operator = ( opNamespace &&old ) noexcept;

	void serialize		( BUFFER *buff );

	bool isDefined		( cacheString const &simpleName, bool isAccess );
	bool isDefinedNS	( cacheString const &simpleName, bool isAccess, opUsingList &useList );

	cacheString const	getResolvedName ( cacheString const &name, bool isAccess, opUsingList &useList ) const;


	errorNum add ( class opClass *cl, bool isLS );
	errorNum add ( class opFunction *func, bool isLS );
	errorNum add ( opSymbol::symbolClass cls, cacheString const &name, astNode *node, srcLocation const &location, bool isExportable, bool isInterface, bool isLS, stringi const &documentation );
	errorNum add ( opSymbol::symbolClass cls, astNode *node, bool isExportable, bool isInterface, bool isLS, stringi const &documentation );
	bool isDefined ( cacheString const &name ) const;
	void pushNamespace ( char const *name );
	void pushNamespace ( cacheString const &name );
	void popNamespace ( void );
	void use ( char const *name );
	void use ( cacheString const &name );
	void useClear ( char const *name );
	cacheString makeName ( cacheString const &name );
	cacheString makeName ( const char *name );

	class opClass					*getClass			( cacheString const &simpleName );
	class opClass					*findClass			( cacheString const &simpleName );
	class compClassElementSearch	*findClassElement	( cacheString const &simpleName, bool isAccess );
	class opFunction				*findFunction		( cacheString const &simpleName );
	class opSymbol					*findSymbol			( cacheString const &simpleName );

	opUsingList getUseList ( void )
	{
		opUsingList list;
		list.useList = inUse;
		return list;
	}
private:

	friend class opFile;
	friend class opClass;
};

