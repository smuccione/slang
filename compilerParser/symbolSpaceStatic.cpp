/*

	Symbol space

*/

#include <vector>
#include <unordered_map>

#include "compilerAST/astNode.h"
#include "fileParser.h"
#include "symbolSpace.h"

// the only direct CALLERS of these are the parsers which are handling the compilation of the code... so these are always non-interface only.   interface's only matter at load time
symbolStatic::symbolStatic ( opFile *file, cacheString const &qualPrefix, cacheString const &enclosingName, class astNode *initializer, symbolTypeClass const &type, stringi const &documentation ) : symbol ( symbolSpaceType::symTypeStatic ), file ( file )
{
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
	qualName = qualName = file->sCache.get ( (stringi)qualPrefix + (stringi)name);
	this->enclosingName = enclosingName;
	file->symbols.insert( { qualName, opSymbol( opSymbol::symbolClass::symbolClass_functionStatic, name, enclosingName, initializer->location, type, initializer, false, false, false, documentation ) } );
}

astNode *symbolStatic::getInitializer (cacheString const &name )  const noexcept
{
	assert (name == this->name);
	auto it = file->symbols.find ( qualName );
	if ( it != file->symbols.end () )
	{
		return it->second.initializer;
	}
	return nullptr;
}

srcLocation const &symbolStatic::getDefinitionLocation ( cacheString const &name, bool isAccess ) const
{
	assert ( name == this->name );
	return file->symbols.find ( qualName )->second.initializer->location;
}

void symbolStatic::setInitializer ( cacheString const &name, astNode *initializer )
{
	assert ( this->name == name );
	file->symbols.find ( qualName )->second.initializer = initializer;
}

void symbolStatic::setAllLocalAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc )
{
//	file->symbols.find ( qualName )->second.setAccessed ( qualName, true, acc, scanQueue );
}

void symbolStatic::setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc )
{
	assert ( this->name == name );
//	file->symbols.find ( qualName )->second.setAccessed ( name, isAccess, acc, scanQueue );
}

void symbolStatic::setClosedOver ( cacheString const &name )
{
	assert ( this->name == name );
	closedOver = true;
}
stringi const &symbolStatic::getDocumentation ( cacheString const &name, bool isAccess )
{
	return file->symbols.find ( qualName )->second.documentation;
}
void symbolStatic::setDocumentation ( cacheString const &name, stringi const &documentation )
{
	assert ( this->name == name );
	file->symbols.find ( qualName )->second.documentation = documentation;
}

bool symbolStatic::isAccessed ( cacheString const &name, bool isAccess ) const
{
	assert ( this->name == name );
	return true;
//	return !file->symbols.find ( qualName )->second.accessors.empty();
}
bool symbolStatic::isAssigned ( cacheString const &name, bool isAccess ) const
{
	assert ( this->name == name );
	return file->symbols.find( qualName )->second.assigned;
}

symbolTypeClass	const symbolStatic::getType ( cacheString const &name, bool isAccess )  const
{
	assert ( this->name == name );
	return file->symbols.find( qualName )->second.type;
}

int32_t symbolStatic::getIndex ( cacheString const &name, bool isAccess ) const
{
	assert ( this->name == name );
	auto it = file->symbols.find ( qualName );
	assert ( it != file->symbols.end () );
	assert ( (int32_t)(*it).second.globalIndex != -1 );
	return (int32_t)(*it).second.globalIndex;
}

cacheString const symbolStatic::getSymbolName (symbolSpaceScope scope, uint32_t index) const
{
	// we'll let the namespace handler do this...
	return cacheString();
}

void symbolStatic::forceVariant ( uint32_t start )
{
	switch ( file->symbols.find( qualName )->second.type )
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
			file->symbols.find( qualName )->second.type = symbolType::symVariant;
			break;
		default:
			break;
	}
}

void symbolStatic::forceAllVariant ( uint32_t start )
{
	file->symbols.find( qualName )->second.type = symbolType::symVariant;
}

bool symbolStatic::makeKnown ( )
{
	if ( file->symbols.find( qualName )->second.type == symbolType::symUnknown )
	{
		file->symbols.find( qualName )->second.type = symbolType::symWeakVariant;
		return true;
	}
	return false;
}
void symbolStatic::pruneRefs ( void )
{
	file->symbols.find( qualName )->second.initializer->pruneRefs ( true );
}
bool symbolStatic::hasCodeblock ( void ) const
{
	if ( file->symbols.find( qualName)->second.initializer ) return file->symbols.find( qualName )->second.initializer->hasCodeblock ( );
	return false;
}
bool symbolStatic::hasFunctionReference ( class symbolStack *sym ) const
{
	if ( file->symbols.find( qualName )->second.initializer ) return file->symbols.find( qualName )->second.initializer->hasFunctionReference ( sym );
	return false;
}
void symbolStatic::resetType ( class opFile *file )
{
	if ( file->symbols.find( qualName )->second.accessors.size() && !file->symbols.find( qualName )->second.assigned )
	{
		file->symbols.find( qualName )->second.type = symbolType::symVariant;
	}
	switch ( file->symbols.find( qualName )->second.type )
	{
		case symbolType::symWeakAtom:
		case symbolType::symWeakInt:
		case symbolType::symWeakArray:
		case symbolType::symWeakDouble:
		case symbolType::symWeakString:
		case symbolType::symWeakObject:
		case symbolType::symWeakVariant:
		case symbolType::symWeakCodeblock:
		case symbolType::symWeakFunc:
			file->symbols.find( qualName )->second.type = symbolType::symUnknown;
			break;
		default:
			break;
	}
	if ( file->symbols.find( qualName )->second.initializer ) file->symbols.find( qualName )->second.initializer->resetType ( file );
}

bool symbolStatic::setType ( cacheString const &name, bool isAccess, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	assert ( this->name == name );
	return file->symbols.find( qualName )->second.setType ( type, nullptr ); // scanQueue );
};

void symbolStatic::setInitType ( cacheString const &name, bool isAccess, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	assert ( this->name == name );
	// TODO: find out why assigned is necessary here... it causes braces_basic to fail amongst others.
	initialized = true;

	file->symbols.find( qualName )->second.setType ( type, scanQueue );
};

void symbolStatic::setType ( cacheString const &name, bool isAccess, cacheString const &className )
{
	throw errorNum::scINTERNAL;
};
