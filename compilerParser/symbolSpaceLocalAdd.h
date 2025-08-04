/* 

	Locals container

	This is ONLY used for makeSymbols to give a spot to add new locals into a symbol space.

	because symbols are added to the symbol stack as they are encountered we need a container to add new ones as they are encountered.  The language definitionr allows for implied definitions of local variables
	as such a local may need to be added to function scope.   however the symbol stack may also have other defined locals which would require us to somehow insert in the middle of the symbol stack.
	rather than do surgery on the symbol stack and all that messiness we do two things.

	the first is that we simply add the symbols into the start of the upper-most basic block.   this creates the definition of the symbol in the AST.
	we then add that symbol to this container which was added to the symbol stack.   this, in effect gives us the needed inserter into the middle of the symbol stack.
*/

#pragma once

#include <vector>
#include <string>

#include "Utility/sourceFile.h"

/*

Symbol space

*/

#pragma once

#include <vector>
#include <string>
#include <cassert>
#include "Utility/sourceFile.h"

class symbolSpaceLocalAdd : public symbol
{
	public:
	std::vector<symbolLocal> locals;
	public:
	symbolSpaceLocalAdd () : symbol ( symbolSpaceType::symTypeLocal )
	{
	}
	virtual ~symbolSpaceLocalAdd ( )
	{
	}

	bool setType ( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		if ( (*this)[name] )
		{
			return (*this)[name]->setType ( name, isAccess, type, acc, scanQueue );
		} else
		{
			throw errorNum::scINTERNAL;
		}
	}

	void setType ( cacheString const &name, bool isAccess, cacheString const &className ) override
	{
		if ( (*this)[name] )
		{
			(*this)[name]->type.setClass ( className );
		} else
		{
			throw errorNum::scINTERNAL;
		}
	}

	void add ( symbolLocal *sym )
	{
		if ( (*this)[sym->name] ) throw errorNum::scDUPLICATE_SYMBOL;

		locals.push_back ( symbolLocal ( *sym ) );
	}

	void addFront ( symbolLocal *sym )
	{
		if ( (*this)[sym->name] ) throw errorNum::scDUPLICATE_SYMBOL;

		locals.insert ( locals.begin(), symbolLocal ( *sym ) );
	}

	// symbol query and setting
	symbolLocal const *operator [] ( cacheString const & name ) const
	{
		for ( auto it = locals.begin ( ); it != locals.end ( ); it++ )
		{
			if ( it->name == name )
			{
				return &(*it);
			}
		}
		return 0;
	}

	symbolLocal *operator [] ( cacheString const &name )
	{
		for ( auto it = locals.begin ( ); it != locals.end ( ); it++ )
		{
			if ( it->name == name )
			{
				return &(*it);
			}
		}
		return 0;
	}

	symbolLocal *operator [] ( uint32_t index )
	{
		return &locals[index];
	}

	bool					 isDefined ( cacheString const &name, bool isAccess ) const override
	{
		if ( (*this)[name] ) return true;
		return false;
	}
	bool					 isDefinedNS ( cacheString const &name, bool isAccess ) const override
	{
		if ( (*this)[name] ) return true;
		return false;
	}
	void					 setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) override
	{
		return (*this)[name]->setAccessed ( file, name, isAccess, acc, scanQueue, loc );
	}
	void					 setClosedOver ( cacheString const &name ) override
	{
		return (*this)[name]->setClosedOver ( name );
	}
	bool					 isAccessed ( cacheString const &name, bool isAccess ) const override
	{
		return (*this)[name]->isAccessed ( name, isAccess );
	}
	bool					 isClosedOver ( cacheString const &name ) const override
	{
		return (*this)[name]->isClosedOver ( name );
	}
	bool					 isAssigned ( cacheString const &name, bool isAccess ) const override
	{
		return (*this)[name]->isAssigned ( name, isAccess );
	}
	// TODO: make getType a reference for speed.... silly to copy
	symbolTypeClass	const	 getType ( cacheString const &name, bool isAccess )  const override
	{
		return (*this)[name]->getType ( name, isAccess );
	}
	int32_t					 getRollup ( void ) const  override { return (int32_t) locals.size ( ); };
	bool					 isStackBased ( void ) const  override { return true; };

	cacheString const		 getSymbolName ( symbolSpaceScope scope, uint32_t index ) const override
	{
		if ( scope != symbolSpaceScope::symLocal ) return cacheString();
		if ( index >= locals.size ( ) ) return cacheString();
		return locals[index].name;
	}

	symbolSpaceScope		 getScope ( cacheString const &name, bool isAccess ) const  override { return symbolSpaceScope::symLocal; };
	int32_t					 getIndex ( cacheString const &name, bool isAccess ) const override
	{
		int32_t		index = 0;
		for ( auto it = locals.begin ( ); it != locals.end ( ); it++, index++ )
		{
			if ( (*it).name == name )
			{
				return index;
			}
		}
		throw errorNum::scINTERNAL;
	}

	size_t size ( void ) const override
	{
		return 	locals.size ( );
	}
};
