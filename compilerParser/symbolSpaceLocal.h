/* 

	Symbol space

*/

#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <cassert>
#include "compilerAST/astNode.h"
#include "Utility/sourceFile.h"

class symbolLocal : public symbol {
public:
	cacheString							 name;
	symbolTypeClass						 type;
	class astNode						*initializer = 0;
	srcLocation							 location;
	stringi								 documentation;
	bool								 assigned = false;
	bool								 initialized = false;
	bool								 closedOver = false;
	bool								 accessed = false;
	std::mutex							 accCtrl;

public:
	symbolLocal ( cacheString const &name, symbolTypeClass const &type, srcLocation const &location, stringi const &documentation, class astNode *initializer = 0) : symbol ( symbolSpaceType::symTypeLocal ), name ( name ), type ( type ), initializer ( initializer ), location ( location ), documentation ( documentation )
	{
		if ( !initializer )
		{
			this->initializer = new astNode ( astOp::symbolValue, name );
		}
	}

	symbolLocal ( class astNode *initializer, symbolTypeClass const &type, srcLocation const &location, stringi const &documentation ) : symbol ( symbolSpaceType::symTypeLocal ), type ( type ), initializer ( initializer ), location ( location ), documentation ( documentation )
	{
		if ( !initializer )
		{
			throw errorNum::scILLEGAL_IDENTIFIER;
		}
		if ( initializer->getOp() == astOp::assign )
		{
			if ( initializer->left->getOp() != astOp::symbolValue )
			{
				throw errorNum::scILLEGAL_IDENTIFIER;
			}
			name = initializer->left->symbolValue();
		} else
		{
			if ( initializer->getOp() != astOp::symbolValue )
			{
				throw errorNum::scILLEGAL_IDENTIFIER;
			}
			name = initializer->symbolValue();
		}

	}

	symbolLocal ( const symbolLocal &old ) : symbol ( symbolSpaceType::symTypeLocal )
	{
		if ( old.initializer )
		{
			initializer = new astNode ( *old.initializer );
		}
		name = old.name;
		location = old.location;
		documentation = old.documentation;
		assigned = old.assigned;
		accessed = old.accessed;
		closedOver = old.closedOver;
		type = old.type;
		initialized = old.initialized;
	}
	symbolLocal ( symbolLocal &&old ) noexcept : symbol ( symbolSpaceType::symTypeLocal )
	{
		*this = std::move ( old );
	}
	symbolLocal &operator = ( const symbolLocal &old )
	{
		return *this = symbolLocal ( old );
	}
	symbolLocal &operator = ( symbolLocal &&old ) noexcept
	{
		std::swap ( initializer, old.initializer );
		std::swap ( accessed, old.accessed);
		std::swap ( assigned, old.assigned );
		std::swap ( closedOver, old.closedOver );
		std::swap ( name, old.name );
		std::swap ( location, old.location );
		std::swap ( documentation, old.documentation );
		std::swap ( type, old.type );
		std::swap ( initialized, old.initialized );
		return *this;
	}

	symbolLocal ( class opFile *file, class sourceFile *srcFile, char const **expr );

	void serialize ( BUFFER *buff ) override
	{
		symbol::serialize ( buff );
		name.serialize ( buff );
		location.serialize ( buff );
		type.serialize ( buff );
		initializer->serialize ( buff );
		documentation.serialize ( buff );
	}

public:
	~symbolLocal ()
	{
		if ( initializer )
		{
			delete initializer;
		}
	}

	bool getFgxSymbol ( visibleSymbols &fgxSym, size_t index ) const override
	{
		assert ( index == 0 );
		fgxSym.isStackBased = true;
		fgxSym.noRef = false;
		fgxSym.name = name;

		return true;
	}

	bool isDefined ( cacheString const &name, bool isAccess ) const override
	{
		if ( this->name == name )
		{
			return true;
		}
		return false;
	}
	bool isDefinedNS ( cacheString const &name, bool isAccess ) const override
	{
		if ( this->name == name )
		{
			return true;
		}
		return false;
	}
	void setDocumentation ( cacheString const &name, stringi const &documentation ) override
	{
		assert ( this->name == name );
		this->documentation = documentation;
	}
	stringi const &getDocumentation ( cacheString const &name, bool isAccess ) override
	{
		assert ( this->name == name );
		return documentation;
	}
	void setInitialized ( cacheString const &name, bool isAccess ) override
	{
		assert ( this->name == name );
		initialized = true;
	}
	void setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		assert ( this->name == name );
		accessed = true;
	}
	void setClosedOver ( cacheString const &name ) override
	{
		assert ( this->name == name );
		closedOver = true;
	}
	bool isAccessed ( cacheString const &name, bool isAccess ) const override
	{
		assert ( this->name == name );
		return accessed;
	}
	bool isClosedOver ( cacheString const &name ) const override
	{
		assert ( this->name == name );
		return closedOver;
	}
	bool isAssigned ( cacheString const &name, bool isAccess ) const override
	{
		assert ( this->name == name );
		return assigned;
	}
	bool isInitialized ( cacheString const &name, bool isAccess ) const override
	{
		assert ( this->name == name );
		return initialized;
	}
	// TODO: make getType a reference for speed.... silly to copy
	symbolTypeClass	const getType ( cacheString const &name, bool isAccess )  const override
	{
		assert ( this->name == name );
		return type;
	}
	int32_t getRollup ( void ) const  override { return 1; };
	bool isStackBased ( void ) const  override { return true; };

	cacheString const getSymbolName ( void ) const override
	{
		return name;
	}
	cacheString const getSymbolName ( symbolSpaceScope scope, uint32_t index ) const override
	{
		if ( !index && scope == symbolSpaceScope::symLocal )
		{
			return name;
		}
		return cacheString();
	};

	symbolSpaceScope getScope ( cacheString const &name, bool isAccess ) const  override { return symbolSpaceScope::symLocal; };
	int32_t getIndex ( cacheString const &name, bool isAccess ) const override
	{
		assert ( this->name == name );
		return 0;
	}

	astNode *getInitializer ( cacheString const &name ) const noexcept override
	{
		assert ( this->name == name );
		return initializer;
	}

	void setInitializer ( cacheString const &name, astNode *initializer ) override
	{
		assert ( this->name == name );
		this->initializer = initializer;
	}

	void rename ( cacheString const &oldName, cacheString const &newName ) override
	{
		if ( this->name == oldName)
		{
			name = newName;
		}
		throw errorNum::scINTERNAL;
	}

	void forceVariant ( uint32_t start = 0 )
	{
		switch ( type )
		{
			case symbolType::symUnknown:
			case symbolType::symWeakInt:
			case symbolType::symWeakArray:
			case symbolType::symWeakDouble:
			case symbolType::symWeakString:
			case symbolType::symWeakObject:
			case symbolType::symWeakVariant:
			case symbolType::symWeakCodeblock:
			case symbolType::symWeakFunc:
				type.forceVariant ();
				break;
			default:
				break;
		}
	}

	void forceAllVariant ( uint32_t start = 0 )
	{
		if ( name != "self" )
		{
			type.forceVariant ();
		}
	}

	bool makeKnown ( )
	{
		if ( type == symbolType::symUnknown )
		{
			type.makeKnown ();
			return true;
		}
		return false;
	}

	void pruneRefs ( void )
	{
		initializer->pruneRefs ( true );
	}
	bool hasCodeblock ( void )
	{
		if ( initializer ) return initializer->hasCodeblock ( );
		return false;
	}
	bool hasFunctionReference ( class symbolStack *sym )
	{
		if ( initializer ) return initializer->hasFunctionReference ( sym );
		return false;
	}
	void resetType ( class opFile *file ) override
	{
		if ( this->name == "self" ) return;
		if ( accessed && !assigned )
		{
			type.makeKnown ();
		} else
		{
			type.resetType ();
		}
	}

	bool setType ( cacheString const &name, bool isAccess, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		std::lock_guard g1 ( accCtrl );

		assert ( this->name == name );
		assigned = true;
		initialized = true;
		if ( this->type == symUnknownType && !accessed )
		{
			return this->type &= type;
		}
		if ( this->type &= type )
		{
			if ( scanQueue ) scanQueue->push ( acc );
			return true;
		}
		return false;
	};

	void setInitType ( cacheString const &name, bool isAccess, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		std::lock_guard g1 ( accCtrl );
		assert ( this->name == name );
// TODO: find out why assigned is necessary here... it causes braces_basic to fail amongst others.
		assigned = true;
		initialized = true;
		this->type &= type;
	};

	void setType ( cacheString const &name, bool isAccess, cacheString const &className ) override
	{
		std::lock_guard g1 ( accCtrl );
		assert ( this->name == name );
		type = symbolTypeClass ( symbolType::symWeakObject, className );
	};

	void setAllLocalAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue ) override
	{
		accessed = true;
	}

	symbolSource getSymbolSource ( cacheString const &className ) override
	{
		assert ( this->name == name );
		return this;
	}
	srcLocation	const &getDefinitionLocation ( cacheString const &name, bool isAccess ) const override
	{
		assert ( this->name == name );
		return location;
	}
	size_t size ( ) const override { return 1; };
};
