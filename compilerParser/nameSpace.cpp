/*
		nammespace support


*/

#include "fileParser.h"
#include "classParser.h"
#include "opSymbol.h"

errorNum opNamespace::add ( opClass *cl, bool isLS )
{
	cl->name = makeName(cl->name);

	if ( file->classList.find ( cl->name ) != file->classList.end () )
	{
		file->classList.insert( { cl->name, cl } );
		if ( !isLS ) throw errorNum::scDUPLICATE_CLASS;
		return errorNum::scDUPLICATE_CLASS;
	}
	file->classList.insert( { cl->name, cl } );
	return errorNum::ERROR_OK;
}

errorNum opNamespace::add ( opFunction *func, bool isLS )
{
	func->name = makeName(func->name);

	if ( file->functionList.find ( func->name ) != file->functionList.end () )
	{
		file->functionList.insert( { func->name, func } );

		if ( !isLS ) throw errorNum::scDUPLICATE_FUNCTION;
		return errorNum::scDUPLICATE_FUNCTION;
	}
	file->functionList.insert( { func->name, func } );
	return errorNum::ERROR_OK;
}

errorNum opNamespace::add ( opSymbol::symbolClass cls, cacheString const &name, astNode *node, srcLocation const &location, bool isExportable, bool isInterface, bool isLS, stringi const &documentation )
{
	assert ( name.size ( ) );

	auto it = file->symbols.find ( name );
	if ( it != file->symbols.end ( ) )
	{
		if ( it->second.initializer->getOp() == astOp::assign && node )
		{
			opSymbol sym( cls, makeName( name ), location, symVariantType, node, true, isExportable, isInterface, documentation );

			file->symbols.insert( { sym.name, std::move( sym ) } );

			if ( !isLS ) throw errorNum::scMULTIPLE_INITIALZERS;
			return errorNum::scMULTIPLE_INITIALZERS;
		}
		// isInterface being false is the most important state... that means we keep the definition around
		it->second.isInterface = isInterface;
		it->second.initializer = it->second.initializer->getOp () == astOp::assign ? it->second.initializer : node;
		if ( !it->second.documentation.size() ) it->second.documentation = documentation;
		return errorNum::ERROR_OK;
	}
	opSymbol sym ( cls, makeName ( name ), location, symVariantType, node, true, isExportable, isInterface, documentation );

	file->symbols.insert( { sym.name, std::move( sym ) } );
	return errorNum::ERROR_OK;
}

errorNum opNamespace::add ( opSymbol::symbolClass cls, astNode *node, bool isExportable, bool isInterface, bool isLS, stringi const &documentation )
{
	switch ( node->getOp() )
	{
		case astOp::assign:
			add ( cls, node->left->symbolValue(), node, node->left->location, isExportable, isInterface, isLS, documentation );
			break;
		case astOp::symbolValue:
			add ( cls, node->symbolValue(), node, node->location, isExportable, isInterface, isLS, documentation );
			break;
		default:
			break;
	}
	return errorNum::ERROR_OK;
}

bool opNamespace::isDefined ( cacheString const &simpleName, bool isAccess )
{
	auto cIt = file->symSearch.find ( simpleName );
	if ( cIt != file->symSearch.end () )
	{
		if ( std::holds_alternative<class opFunction *> ( cIt->second ) )
		{
			lastFuncCache = std::get<class opFunction *> ( cIt->second );
		}

		return true;
	}

	if ( simpleName[0] == ':' )
	{
		return false;
	}
	return false;
}

bool opNamespace::isDefinedNS ( cacheString const &simpleName, bool isAccess, opUsingList &useList )
{
	auto cIt = file->symSearch.find ( simpleName );
	if ( cIt != file->symSearch.end () )
	{
		return true;
	}

	if ( simpleName[0] == ':' )
	{
		return false;
	}

	for ( auto &it : useList.useList )
	{
		cacheString	name = file->sCache.get ( (stringi)it + (stringi)simpleName );

		auto cIt = file->symSearch.find ( name );
		if ( cIt != file->symSearch.end () )
		{
			return true;
		}
	}
	return false;
}

cacheString const opNamespace::getResolvedName ( cacheString const &simpleName, bool isAccess, opUsingList &useList ) const
{
	auto cIt = file->symSearch.find ( simpleName );
	if ( cIt != file->symSearch.end () )
	{
		return simpleName;
	}

	for ( auto &it : useList.useList )
	{
		cacheString	name = file->sCache.get ( (stringi)it + (stringi)simpleName );

		auto cIt = file->symSearch.find ( name );
		if ( cIt != file->symSearch.end () )
		{
			return name;
		}
	}
	throw errorNum::scINTERNAL;
}


opClass *opNamespace::getClass ( cacheString const &simpleName )
{
	auto cIt = file->classList.find ( simpleName );
	if ( cIt != file->classList.end () )
	{
		return (*cIt).second;
	}
	return nullptr;
}

opClass *opNamespace::findClass ( cacheString const &simpleName )
{
	auto cIt = file->classList.find ( simpleName );
	if ( cIt != file->classList.end () )
	{
		return (*cIt).second;
	}
	return nullptr;
}

compClassElementSearch *opNamespace::findClassElement ( cacheString const &simpleName, bool isAccess )
{
	auto start = simpleName.c_str ();

	if ( (start[0] == ':' && start[1] == ':') )
	{
		cacheString	name = file->sCache.get ( stringi ( simpleName.c_str ( ) + 2 ) );

		auto elem = file->staticElemMap.find ( name );
		if ( elem != file->staticElemMap.end () )
		{
			return elem->second;
		}
	}

	auto nsOp = simpleName.find ( "::" );

	if ( nsOp != cacheString::npos )
	{
		auto elem = file->staticElemMap.find ( simpleName );
		if ( elem != file->staticElemMap.end () )
		{
			return elem->second;
		}
	}
	return nullptr;
}

opFunction *opNamespace::findFunction ( cacheString const &simpleName  )
{
	if ( lastFuncCache && lastFuncCache->name == simpleName )
	{
	//	return lastFuncCache;
	}
	if ( (simpleName[0] == ':' && simpleName[1] == ':') || !memcmpi ( simpleName.c_str ( ), "root::", 6 ) )
	{
		if ( !memcmpi ( simpleName.c_str ( ), "root::", 6 ) )
		{
			cacheString	name = file->sCache.get ( stringi ( simpleName.c_str ( ) + 6 ) );
			auto it = file->functionList.find ( name );
			if ( it != file->functionList.end ( ) )
			{
				return it->second;
			}
		} else
		{
			cacheString	name = file->sCache.get ( stringi ( simpleName.c_str ( ) + 2 ) );
			auto it = file->functionList.find ( name );
			if ( it != file->functionList.end ( ) )
			{
				return it->second;
			}
		}
		return nullptr;
	}

	auto fIt = file->functionList.find ( simpleName );
	if ( fIt != file->functionList.end () )
	{
		lastFuncCache = (*fIt).second;
		return (*fIt).second;
	}

	auto elem = findClassElement ( simpleName, true );
	if ( elem )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				lastFuncCache = elem->methodAccess.func;
				return elem->methodAccess.func;
			default:
				break;
		}
	}
	return nullptr;
}

opSymbol *opNamespace::findSymbol ( cacheString const &simpleName )
{
	auto it = file->symbols.find ( simpleName );
	if ( it != file->symbols.end() )
	{
		return &it->second;
	}
	return nullptr;
}


void opNamespace::pushNamespace ( char const *name )
{
	cacheString qualName;

	nsStack.push ( makeName ( name ) );

	if ( !isDefined ( file->sCache.get ( name ) ) )
	{
		nameSpaces.push_back ( file->sCache.get ( name ) );
	}
}

void opNamespace::pushNamespace ( cacheString const &name )
{
	cacheString qualName;

	nsStack.push ( makeName ( name.c_str() ) );

	if ( !isDefined ( name ) )
	{
		nameSpaces.push_back ( name );
	}
}

void opNamespace::popNamespace ( void )
{
	nsStack.pop();
}

bool opNamespace::isDefined ( cacheString const &name ) const
{
	for ( auto &it : nameSpaces )
	{
		if ( it == name )
		{
			return true;
		}
	}
	return false;
}

void opNamespace::use ( char const *name )
{
	auto ns = file->sCache.get ( stringi ( name ) + "::" );

	if ( !isDefined ( ns ) )
	{
		nameSpaces.push_back ( ns );
	}
	inUse.push_back ( ns );
}

void opNamespace::use ( cacheString const &name )
{
	cacheString ns = file->sCache.get ( (stringi )name + "::" );
	if ( !isDefined ( ns ) )
	{
		nameSpaces.push_back ( ns );
	}
	inUse.push_back ( ns );
}

void opNamespace::useClear ( char const *name )
{
	inUse.clear();
}

cacheString opNamespace::makeName ( cacheString const &name )
{
	if ( name.length() && name.c_str()[0] == ':' )
	{
		return name;
	}
	return nsStack.top() ? file->sCache.get ( (stringi) nsStack.top ( )+ "::" + (stringi)name ): name;
}

cacheString opNamespace::makeName ( const char *name )
{
	if ( name[0] == ':' )
	{
		return file->sCache.get ( stringi ( name ) );
	}

	return nsStack.top ( ) ? file->sCache.get ( (stringi) nsStack.top ( )+ "::" + name ): file->sCache.get ( name );
}

void opNamespace::serialize ( BUFFER *buff )
{
	bufferPut32 ( buff, (uint32_t)nameSpaces.size() );
	for ( auto it = nameSpaces.begin(); it!= nameSpaces.end(); it++ )
	{
		it->serialize ( buff );
	}
}

opNamespace::opNamespace ( opNamespace &&old ) noexcept
{
	*this = std::move ( old );
}

opNamespace &opNamespace::operator = ( opNamespace &&old ) noexcept
{
	std::swap ( file, old.file );
	std::swap ( nameSpaces, old.nameSpaces );
	return *this;
}

opNamespace::opNamespace ( class opFile *file, char const ** expr )
{
	uint32_t loop;

	this->file = file;

	nameSpaces.clear();

	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );
	for (; loop; loop-- )
	{
		nameSpaces.push_back ( file->sCache.get ( expr ) );
	}

	nsStack.push ( cacheString() );
}

opNamespace::opNamespace()
{
	nameSpaces.push_back ( cacheString() );
	nsStack.push ( cacheString() );
}

opUsingList::opUsingList ( class opFile *file, char const ** expr )
{
	uint32_t loop;

	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );

	useList.clear();

	for (; loop; loop-- )
	{
		auto ns = file->sCache.get ( expr );
		if ( ns.size() )
		{
			if ( (useList.size () != 1) || (!strccmp ( useList.back (), "" ) ) )
			{
				useList.emplace_back ( ns );
			} else
			{
				useList[0] = ns;
			}
		}
	}
}