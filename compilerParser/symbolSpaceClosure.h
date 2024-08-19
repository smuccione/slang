/* 

	Closure container

	This is ONLY used for makeSymbols to give a spot to add new closures into a symbol space.   we can't add them directly to the closure vector as those symbols are pushed onto the symbol stack by pointer
	and adding them to the vector can invalidate those pointers.   this is just a container to allow indirect access to that vector

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

class symbolSpaceClosures : public symbol
{
	public:
	std::vector<symbolLocal> *closures;

	public:
	symbolSpaceClosures ( std::vector<symbolLocal> *closures ) : symbol ( symbolSpaceType::symTypeLocal ), closures ( closures )
	{
	}
	~symbolSpaceClosures ( )
	{
	}

	bool setType ( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		throw errorNum::scINTERNAL;
	}

	void setType ( cacheString const &name, bool isAccess, cacheString const &className ) override
	{
		throw errorNum::scINTERNAL;
	}

	void add ( cacheString const &name, symbolTypeClass const &type )
	{
		if ( (*this)[name] ) throw errorNum::scDUPLICATE_SYMBOL;

		(*closures).push_back ( symbolLocal ( name, type, srcLocation(), "closure" ) );
	}

	// symbol query and setting
	symbolLocal *operator [] ( cacheString const & name ) const
	{
		for ( auto it = (*closures).begin ( ); it != (*closures).end ( ); it++ )
		{
			if ( it->name == name )
			{
				return &*it;
			}
		}
		return 0;
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
	void					 setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		throw errorNum::scINTERNAL;
	}
	void					 setClosedOver ( cacheString const &name ) override
	{
		throw errorNum::scINTERNAL;
	}
	bool					 isAccessed ( cacheString const &name, bool isAccess ) const override
	{
		throw errorNum::scINTERNAL;
	}
	bool					 isClosedOver ( cacheString const &name ) const override
	{
		throw errorNum::scINTERNAL;
	}
	bool					 isAssigned ( cacheString const &name, bool isAccess ) const override
	{
		// assume it's assigned as call is detached from initialization
		return true;
	}
	// TODO: make getType a reference for speed.... silly to copy
	symbolTypeClass	const	 getType ( cacheString const &name, bool isAccess )  const override
	{
		throw errorNum::scINTERNAL;
	}
	int32_t					 getRollup ( void ) const  override { return (int32_t) (*closures).size ( ); };
	bool					 isStackBased ( void ) const  override { return true; };

	cacheString const		 getSymbolName ( symbolSpaceScope scope, uint32_t index ) const override
	{
		if ( scope != symbolSpaceScope::symLocal ) return cacheString();
		if ( index >= (*closures).size ( ) ) return cacheString();
		return (*closures)[index].name;
	}

	symbolSpaceScope		 getScope ( cacheString const &name, bool isAccess ) const  override { return symbolSpaceScope::symLocal; };
	int32_t					 getIndex ( cacheString const &name, bool isAccess ) const override
	{
		throw errorNum::scINTERNAL;
	}

	size_t size ( void ) const override
	{
		return 	(uint32_t) (*closures).size ( );
	}
	srcLocation const &getDefinitionLocation ( cacheString const &name, bool isAccess ) const override
	{
		throw errorNum::scINTERNAL;
	}

};
