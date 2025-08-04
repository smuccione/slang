/*

	Symbol space

*/

#pragma once

#include <vector>
#include <unordered_map>

/*

Symbol space

*/

#pragma once

#include <vector>
#include <string>
#include <cassert>
#include "Utility/stringCache.h"
#include "symbolSpace.h"
#include "Utility/sourceFile.h"

class symbolStatic : public symbol {
public:
	opFile				*file;
	cacheString			 name;
	cacheString			 qualName;
	cacheString			 enclosingName;
	bool				 closedOver = false;
	bool				 initialized = false;

public:
	symbolStatic ( opFile *file, cacheString const &qualPrefix, cacheString const &enclosingName, class astNode *initializer, symbolTypeClass const &type, stringi const &documentation );

	symbolStatic ( const symbolStatic&old )  noexcept : symbol ( symbolSpaceType::symTypeStatic )
	{
		file = old.file;
		name = old.name;
		qualName = old.qualName;
		enclosingName = old.enclosingName;
		closedOver = old.closedOver;
	}
	symbolStatic ( symbolStatic&&old ) noexcept : symbol ( symbolSpaceType::symTypeStatic )
	{
		*this = std::move ( old );
	}
	symbolStatic &operator = ( const symbolStatic&old ) noexcept
	{
		file = old.file;
		name = old.name;
		qualName = old.qualName;
		enclosingName = old.enclosingName;
		closedOver = old.closedOver;
		initialized = old.initialized;
		return *this;
	}
	symbolStatic&operator = ( symbolStatic&&old ) noexcept
	{
		std::swap ( name, old.name );
		std::swap ( file, old.file );
		std::swap ( closedOver, old.closedOver );
		std::swap ( qualName, old.qualName );
		std::swap ( enclosingName, old.enclosingName );
		std::swap ( initialized, old.initialized );
		return *this;
	}

	symbolStatic ( class opFile *file, class sourceFile *srcFile, char const **expr ) noexcept;

	void serialize ( BUFFER *buff ) override
	{
		symbol::serialize ( buff );
		name.serialize ( buff );
		qualName.serialize ( buff );
		enclosingName.serialize ( buff );
	}
	astNode *getInitializer ( cacheString const &name ) const noexcept override;
	void setInitializer ( cacheString const &name, astNode *initializer ) override;

	virtual ~symbolStatic ( )
	{
	}

	bool getFgxSymbol ( visibleSymbols &fgxSym, size_t index = 0 ) const override
	{
		assert ( index == 0 );
		fgxSym.isStackBased = false;
		fgxSym.noRef = false;
		fgxSym.name = name;

		return true;
	}

	bool isDefined ( cacheString const &name, bool isAccess ) const override
	{
		return this->name == name;
	}
	bool isDefinedNS ( cacheString const &name, bool isAccess ) const override
	{
		return this->name == name;
	}
	stringi const &getDocumentation ( cacheString const &name, bool isAccess ) override;
	void setInitialized ( cacheString const &name, bool isAccess ) override
	{
		initialized = true;
	}
	void setDocumentation ( cacheString const &name, stringi const &documentation ) override;
	void setClosedOver ( cacheString const &name ) override;
	bool isAccessed ( cacheString const &name, bool isAccess ) const override;
	bool isInitialized ( cacheString const &name, bool isAccess ) const override
	{
		return initialized;
	}
	bool isClosedOver ( cacheString const &name ) const override
	{
		assert ( this->name == name );
		return closedOver;
	}
	bool isAssigned ( cacheString const &name, bool isAccess ) const override;
	symbolTypeClass	const getType ( cacheString const &name, bool isAccess ) const override;
	bool isStackBased ( void ) const  override { return false; };

	cacheString const getSymbolName ( void ) const override
	{
		return name;
	}
	symbolSpaceScope getScope ( cacheString const &name, bool isAssign = true ) const  override { return symbolSpaceScope::symLocalStatic; };
	int32_t getIndex ( cacheString const &name, bool isAccess ) const override;
	cacheString const getSymbolName (symbolSpaceScope scope, uint32_t index) const override;

	void rename ( cacheString const &oldName, cacheString const &newName ) override
	{
		assert ( name == oldName );
		name = newName;
	}

	void forceVariant ( uint32_t start = 0 );
	void forceAllVariant ( uint32_t start = 0 );
	bool makeKnown ( );
	void pruneRefs ( void );
	bool hasCodeblock ( void ) const;
	bool hasFunctionReference ( class symbolStack *sym ) const;
	void resetType ( class opFile * ) override ;
	size_t size ( ) const override { return 1; };

	void setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) override;
	void setAllLocalAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) override;

	bool setType			( cacheString const &name, bool isAccess, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	void setInitType		( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	void setType			( cacheString const &name, bool isAccess, cacheString const &className ) override;

	symbolSource getSymbolSource ( cacheString const &name ) override
	{
		assert ( this->name == name );
		return this;
	}
	srcLocation const &getDefinitionLocation ( cacheString const &name, bool isAccess ) const override;
 };
