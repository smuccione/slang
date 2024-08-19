// these are definitions for the global level symbol table.   In short these are all symbols that need to be in the root list of the garbage collector but are not always referenced by an element
// on the stack.
//
// They compose:
//		global scoped symbols
//		file static
//		class constants
//		class statics
//		function statics
//
// Of the above, all should be initialized prior to execution of the MAIN except for function statics which are initialized upon first use (at the point of declaration)
//
// Only globally scoped variables need to be placed in the lookup binary tree.   
//		They are also the only ones that need to have initialization time linkage done with .sll's (e.g. references to another image's global).
//
//

#pragma once

#include <stdint.h>
#include "Utility/sourceFile.h"
#include "Utility/stringCache.h"

class opSymbol
{
public:
	enum class symbolClass
	{
		symbolClass_undefined = 0,
		symbolClass_global,
		symbolClass_globalConst,
		symbolClass_functionStatic,
		symbolClass_classConst,
		symbolClass_classStatic,
	}									 symClass;
	cacheString							 name;
	cacheString							 enclosingName;
	stringi								 documentation;
	srcLocation							 location;
	symbolTypeClass						 type;
	uint32_t							 globalIndex = 0;		// index into globals array
	class astNode						*initializer = 0;
	bool								 loadTimeInitializable = true;
	bool								 isExportable = false;
	bool								 assigned = false;
	bool								 isInterface = false;					// these generate no code.  They are necessary to allow for constant folding for symbols that do need to be emitted
	bool								 inUse = false;
	bool								 needScan = false;

	std::mutex							 accessorsAccess;
	std::unordered_set<accessorType>	 accessors;						// for type inferencing in case our type changes

	opSymbol ( opSymbol::symbolClass cls, cacheString const &name, srcLocation const &location, symbolTypeClass const &type, class astNode *init, bool loadTimeInitializable, bool isExportable, bool isInterface, stringi const &documentation ) : symClass ( cls ), name ( name ), documentation ( documentation ), location ( location ), type ( type ), initializer ( init ), loadTimeInitializable ( loadTimeInitializable ), isExportable ( isExportable ), isInterface ( isInterface )
	{
		assert ( name );
	}
	opSymbol ( opSymbol::symbolClass cls, cacheString const &name, cacheString const &enclosingName, srcLocation const &location, symbolTypeClass const &type, class astNode *init, bool loadTimeInitializable, bool isExportable, bool isInterface, stringi const &documentation ) : symClass ( cls ), name ( name ), enclosingName ( enclosingName ), documentation ( documentation ), location ( location ), type ( type ), initializer ( init ), loadTimeInitializable ( loadTimeInitializable ), isExportable ( isExportable ), isInterface ( isInterface )
	{
		assert ( name );
	}
	opSymbol ( )
	{
		type = symUnknownType;
	};
	opSymbol ( const opSymbol &old )
	{
		symClass = old.symClass;
		name = old.name;
		enclosingName = old.enclosingName;
		documentation = old.documentation;
		if ( old.initializer )
		{
			initializer = new astNode ( *old.initializer );
		} else
		{
			initializer = 0;
		}
		type = old.type;
		location = old.location;
		accessors = old.accessors;
		assigned = old.assigned;
		globalIndex = old.globalIndex;
		loadTimeInitializable = old.loadTimeInitializable;
		isInterface = old.isInterface;
		isExportable = old.isExportable;
	}
	opSymbol ( opSymbol &&old ) noexcept
	{
		*this = std::move ( old );
	}
	opSymbol ( class opFile *file, class sourceFile *src, char const **expr, bool isInterface );

	opSymbol &operator = ( opSymbol &&old ) noexcept
	{
		std::swap ( symClass, old.symClass );
		std::swap ( name, old.name );
		std::swap ( enclosingName, old.enclosingName);
		std::swap ( documentation, old.documentation );
		std::swap ( initializer, old.initializer );
		std::swap ( type, old.type );
		std::swap ( location, old.location );
		std::swap ( accessors, old.accessors );
		std::swap ( assigned, old.assigned );
		std::swap ( globalIndex, old.globalIndex );
		std::swap ( loadTimeInitializable, old.loadTimeInitializable );
		std::swap ( isInterface, old.isInterface );
		std::swap ( isExportable, old.isExportable );
		return *this;
	}

	opSymbol &operator = ( opSymbol const &old )
	{
		*this = opSymbol ( old );
		return *this;
	}

	~opSymbol ( )
	{
		if ( initializer ) delete initializer;
	}

	void serialize ( BUFFER *buff );

	char const *getName ( void ) const
	{
		return name.c_str ( );
	}

	char const *getEnclosingName ( void ) const
	{
		return enclosingName.c_str ();
	}

	symbolTypeClass const &getType ( void ) const
	{
		return type;
	}

	bool setType ( symbolTypeClass const &newType, unique_queue<accessorType> *scanQueue )
	{
		std::lock_guard g1 ( accessorsAccess );
		assigned = true;
		if ( type &= newType )
		{
			if ( scanQueue ) scanQueue->push ( this );

			for ( auto &it : accessors )
			{
				if ( scanQueue ) scanQueue->push ( it );
			}
			return true;
		}
		return false;
	}

	void setAccessed ( cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue )
	{
		if ( isAccess )
		{
			std::lock_guard g1 ( accessorsAccess );
			accessors.insert ( acc );
		}
	}
};
