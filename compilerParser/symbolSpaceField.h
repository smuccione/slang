/*

	Symbol space

*/

#pragma once

#include <vector>
#include <string>

#include "Utility/sourceFile.h"

class symbolField : public symbol {
private:
	cacheString				 name;
	srcLocation				 location;
	stringi					 documentation;

public:
	symbolField ( cacheString const &name, stringi const &documentation ) : symbol ( symbolSpaceType::symTypeField ), name  ( name )
	{
	}

	symbolField ( astNode *node, stringi const &documentation ) : symbol ( symbolSpaceType::symTypeField )
	{
		if ( node->getOp() != astOp::symbolValue )
		{
			throw errorNum::scILLEGAL_IDENTIFIER;
		}
		name = node->symbolValue();
		location = node->location;
		delete node;
	}

	symbolField ( const symbolField &old ) : symbol ( symbolSpaceType::symTypeField )
	{
		name = old.name;
		documentation = old.documentation;
	}
	symbolField ( symbolField &&old )  noexcept : symbol ( symbolSpaceType::symTypeField )
	{
		*this = std::move ( old );
	}
	symbolField &operator = ( const symbolField &old )
	{
		name = old.name;
		return *this;
	}
	symbolField &operator = ( symbolField &&old ) noexcept
	{
		std::swap ( name, old.name );
		std::swap ( documentation, old.documentation );
		return *this;
	}

	symbolField ( class opFile *file, class sourceFile *srcFile, char const **expr );

	void serialize ( BUFFER *buff ) override
	{
		symbol::serialize ( buff );
		name.serialize ( buff );
		documentation.serialize ( buff );
	}
	cacheString const &getName ()
	{
		return name;
	}

	virtual ~symbolField ()
	{
	}

	bool isDefined ( cacheString const &name, bool isAccess ) const override
	{
		return this->name == name;
	}
	bool isDefinedNS ( cacheString const &name, bool isAccess ) const override
	{
		return this->name == name;
	}
	void setDocumentation ( cacheString const &name, stringi const &documentation ) override
	{
		this->documentation = documentation;
	}
	stringi const &getDocumentation ( cacheString const &name, bool isAccess ) override
	{
		assert ( this->name == name );
		return documentation;
	}
	void setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) override
	{
		// NOTE: we don't care about accessors... our type will never change... it's ALWAYS variant
		return;
	}
	symbolTypeClass	const getType ( cacheString const &name, bool isAccess ) const override
	{
		return symWeakVariantType;
	}

	cacheString const getSymbolName ( void ) const override
	{
		return name;
	}
	symbolSpaceScope		 getScope ( cacheString const &name, bool isAccess ) const  override { return symbolSpaceScope::symLocalField; };
	int32_t					 getIndex ( cacheString const &name, bool isAccess ) const override
	{
		if ( this->name == name ) return 1;
		throw errorNum::scINTERNAL;
	}

	size_t					 size ( )				const override { return 0; };

	void rename ( cacheString const &oldName, cacheString const &newName ) override
	{
		if ( this->name == oldName )
		{
			name = newName;
		}
		throw errorNum::scINTERNAL;
	}

	void forceVariant ( uint32_t start = 0 )
	{
	}

	bool makeKnown ( )
	{
		return false;
	}

	void resetType ( class opFile * ) override
	{
	}

	srcLocation const &getDefinitionLocation ( cacheString const &name, bool isAccess ) const override
	{
		return location;
	}

};
