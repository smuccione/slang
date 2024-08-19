/*

	block processor


*/

#include "fileParser.h"
#include "Target/fileCache.h"
#include "slib/slangLib.h"
#include "compilerAST/astNodeWalk.h"

moduleLibraryEntry::moduleLibraryEntry ( class opFile *file, char const **expr, bool isLibrary )
{
	fName = file->sCache.get ( stringi ( expr ) );

	if ( isLibrary )
	{
		std::filesystem::path p ( fName.c_str () );

		if ( !strccmp ( p.extension ().string ().c_str (), ".flb" ) || !strccmp ( p.extension ().generic_string ().c_str (), ".slb" ) )
		{
			p.replace_extension ( ".sll" );
		}
		fName = file->sCache.get ( p.generic_string ().c_str() );
	}

	uint32_t cnt = *((uint32_t *)*expr);
	*expr += sizeof ( uint32_t );
	for ( ; cnt; cnt-- )
	{
		locations.insert ( srcLocation ( &file->srcFiles, expr ) );
	}
}

void moduleLibraryEntry::serialize ( BUFFER *buff )
{
	fName.serialize ( buff );
	bufferPut32 ( buff, (uint32_t)locations.size () );
	for ( srcLocation it : locations )
	{
		it.serialize ( buff );
	}
}

opFile::opFile () : errHandler ( this )
{
	ns.file			= this;
}

opFile::opFile ( char const ** expr ) : errHandler ( this )
{
	ns.file			= this;

	addStream ( expr, false, false );
}

void opFile::serialize ( BUFFER *buff, bool isLibrary )
{
	uint32_t count;
	srcFiles.serialize ( buff );

	bufferPut32 ( buff, (uint32_t) libraries.size () );
	for ( auto &it : libraries )
	{
		it.serialize ( buff );
	}

	bufferPut32 ( buff, (uint32_t) modules.size () );
	for ( auto &it : modules )
	{
		it.serialize ( buff );
	}

	bufferPut32 ( buff, (uint32_t) buildSources.size () );
	for ( auto &it : buildSources)
	{
		it.serialize ( buff );
	}

	if ( !isLibrary )
	{
		bufferPut32 ( buff, (uint32_t) functionList.size () );
		for ( auto it = functionList.begin (); it != functionList.end (); it++ )
		{
			(*it).second->serialize ( buff, true );
		}

		// no external dependencies... we're not generating a library
		bufferPut32 ( buff, 0);

		bufferPut32 ( buff, (uint32_t) classList.size () );
		for ( auto it = classList.begin (); it != classList.end (); it++ )
		{
			(*it).second->serialize ( buff );
		}
		bufferPut32 ( buff, (uint32_t) symbols.size () );
		for ( auto it = symbols.begin (); it != symbols.end (); it++ )
		{
			it->first.serialize ( buff );
			(*it).second.serialize ( buff );
		}
	} else
	{
		// for libraries we NEVER emit defines that are interfaces...
		count = 0;
		for ( auto &it : functionList )
		{
			if ( !it.second->isInterface ) count++;
		}
		bufferPut32 ( buff, count );
		for ( auto &it : functionList )
		{
			if ( !it.second->isInterface ) it.second->serialize ( buff, false );
		}

		// build list of dependencies
		count = 0;
		for ( auto &it : functionList )
		{
			if ( it.second->isInterface && it.second->inUse ) count++;
		}
		bufferPut32 ( buff, count );
		for ( auto &it : functionList )
		{
			if ( it.second->isInterface && it.second->inUse )
			{
				it.second->name.serialize ( buff );
			}
		}
	
		count = 0;
		for ( auto it = classList.begin (); it != classList.end (); it++ )
		{
			if ( !(*it).second->isInterface ) count++;
		}
		bufferPut32 ( buff, count );
		for ( auto it = classList.begin (); it != classList.end (); it++ )
		{
			if ( !(*it).second->isInterface ) (*it).second->serialize ( buff );
		}

		count = 0;
		for ( auto it = symbols.begin (); it != symbols.end (); it++ )
		{
			if ( !it->second.isInterface ) count++;
		}
		bufferPut32 ( buff, count );
		for ( auto it = symbols.begin (); it != symbols.end (); it++ )
		{
			if ( !it->second.isInterface )
			{
				it->first.serialize ( buff );
				(*it).second.serialize ( buff );
			}
		}
	}

	ns.serialize ( buff );
}

void opFile::addStream( char const ** expr, bool isInterface, bool isLS )
{
	uint32_t				 loop;

	srcFiles.addMapping ( expr );

	// libraries
	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );
	for (; loop; loop-- )
	{
		moduleLibraryEntry lib ( this, expr, true );

		bool found = false;
		for ( auto &it : libraries )
		{
			if ( isSameFile ( it.fName, lib.fName ) )
			{
				for ( auto &loc : lib.locations )
				{
					it.locations.insert ( loc );
				}
				found = true;
				break;
			}
		}
		if ( !found )
		{
			libraries.push_back ( std::move ( lib ) );
		}
	}

	// modules
	loop = *((uint32_t *) *expr);
	(*expr) += sizeof ( uint32_t );
	for ( ; loop; loop-- )
	{
		moduleLibraryEntry mod ( this, expr, true );

		bool found = false;
		for ( auto &it : modules )
		{
			if ( isSameFile ( it.fName, mod.fName ) )
			{
				for ( auto &loc : mod.locations )
				{
					it.locations.insert ( loc );
				}
				found = true;
				break;
			}
		}
		if ( !found )
		{
			modules.push_back ( std::move ( mod ) );
		}
	}

	// buildSource
	loop = *((uint32_t *) *expr);
	(*expr) += sizeof ( uint32_t );
	for ( ; loop; loop-- )
	{
		addBuildSource ( stringi ( expr ) );
	}

	// functions
	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );
	for ( ; loop; loop-- )
	{
		auto oFunc = new opFunction ( this, &srcFiles, expr );

		oFunc->isInterface = isInterface;

		auto it = functionList.find ( oFunc->name );
		if ( it != functionList.end () )
		{
			if ( oFunc->location == it->second->location )
			{
				delete oFunc;
			} else
			{
				{
					errorLocality e ( &errHandler, oFunc->location );
					errHandler.throwFatalError ( isLS, errorNum::scDUPLICATE_FUNCTION );
				}
				{
					errorLocality e ( &errHandler, (*it).second->location );
					errHandler.throwFatalError ( isLS, errorNum::scALSO_SEEN );
				}
			}
		} else
		{
			functionList.insert ( { oFunc->name, oFunc } );
		}
	}

	// external file dependencies
	loop = *((uint32_t *) *expr);
	(*expr) += sizeof ( uint32_t );
	for ( ; loop; loop-- )
	{
		dependenciesFromExternalSources.insert ( sCache.get ( expr ) );
	}

	// class's
	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );
	for (; loop; loop-- )
	{
		auto oClass = new opClass ( this, &srcFiles, expr );
		
		oClass->isInterface = isInterface;

		errorLocality e ( &errHandler, oClass->location );

		auto it = classList.find ( oClass->name );
		if ( it != classList.end() )
		{
			if ( oClass->location == it->second->location )
			{
				delete oClass;
			} else
			{
				{
					errorLocality e ( &errHandler, oClass->location );
					errHandler.throwFatalError ( isLS, errorNum::scDUPLICATE_CLASS );
				}
				{
					errorLocality e ( &errHandler, (*it).second->location );
					errHandler.throwFatalError ( isLS, errorNum::scALSO_SEEN );
				}
			}
		} else
		{
			classList.insert ( { oClass->name, oClass } );
		}
	}

	// symbols
	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );
	for (; loop; loop-- )
	{
		auto name = sCache.get ( expr );

		opSymbol oSymbol ( this, &srcFiles, expr, isInterface );

		auto it = symbols.find ( name );
		if ( it != symbols.end() )
		{
			if ( oSymbol.location == (*it).second.location )
			{
				// do nothing, just don't insert it
			} else
			{
				if ( (*it).second.initializer && oSymbol.initializer )
				{
					{
						errorLocality e ( &errHandler, oSymbol.location );
						errHandler.throwFatalError ( isLS, errorNum::scDUPLICATE_INITIALIZER );
					}
					{
						errorLocality e ( &errHandler, (*it).second.location );
						errHandler.throwFatalError ( isLS, errorNum::scALSO_SEEN );
					}
				} else if ( oSymbol.initializer )
				{
					// just get the initializer
					(*it).second.initializer = oSymbol.initializer;
					oSymbol.initializer = 0;
				}
			}
		} else
		{
			symbols.insert( { name, std::move( oSymbol ) } );
		}
	}

	ns = opNamespace ( this, expr );

	srcFiles.freeMapping();
}

void opFile::loadLibraries ( bool isInterface, bool isLS )
{
	for ( size_t index = 0; index < libraries.size(); index++ )		// this can NOT be a for-each... we will notload libraries appropraitely as a load may add ADDITIONAL libraries to be loaded!
	{
		if ( !libraries[index].isLoaded )
		{
			libraries[index].isLoaded = true;

			auto entry = globalFileCache.read ( libraries[index].fName.operator const stringi &() );

			if ( !entry )
			{
				errorLocality e ( &errHandler, *libraries[index].locations.begin () );
				if ( !isLS ) throw (errorNum)GetLastError ();
				errors.push_back ( std::make_unique<astNode> ( errorNum(GetLastError ()), *libraries[index].locations.begin() ) );
				continue;
			}

			addBuildSource ( libraries[index].fName );

			fgxFileHeader *header = (fgxFileHeader *)entry->getData ();
			if ( header->offsetInterface )
			{
				char const *expr = (char *)header + header->offsetInterface;
				addStream ( &expr, isInterface, isLS );
			}

			entry->free ();
		}
	}
}

void opFile::loadLibraryDirect ( uint8_t const *comp, bool isInterface, bool isLS )
{
	fgxFileHeader const *header = (fgxFileHeader const *)comp;
	if ( header->offsetInterface )
	{
		char const *expr = (char const *)comp + header->offsetInterface;
		addStream ( &expr, isInterface, isLS );
	}
}

void opFile::loadModules ( bool isInterface, bool isLS )
{
	for ( size_t index = 0; index < modules.size (); index++ )
	{
		auto &mod = modules[index];
		if ( !mod.isLoaded )
		{
			libStore lib ( mod.fName );

			for ( auto &it : lib )
			{
				char const *data = (char *)(uint8_t *)*it;

				addBuildSource ( it->fName );
				addStream ( &data, isInterface, isLS );
			}
			modules[index].isLoaded = true;
		}
	}
}

void opFile::loadLibrary ( char const *libName, bool isInterface, bool isLS )
{
	libStore lib ( libName );

	for ( auto &it : lib )
	{
		char const *data = (char *)(uint8_t *)*it;

		addBuildSource(it->fName);

		addStream ( &data, isInterface, isLS );
	}
}

void opFile::validate ( opFunction* func )
{
	if ( func->isIterator ) return;
	symbolStack	sym ( this );
	errorLocality e1 ( &errHandler, func->location );

	if ( func->params.symbols.size ( ) )
	{
		// need to do this... the default parameters are not built under the context of the function
		for ( auto it = func->params.symbols.begin ( ); it != func->params.symbols.end ( ); it++ )
		{
			if ( it->initializer ) it->initializer->validate ( &sym, &errHandler );
		}
	}

	if ( func->codeBlock )
	{
		func->codeBlock->validate ( &sym, &errHandler );
	}
}

void opFile::validate ( void )
{
	symbolStack sym ( this );
	opUsingList opUse;
	symbolSpaceNamespace ns ( this, &this->ns, &opUse );
	sym.push ( &ns );

	for ( auto& it : symbols )
	{
		if ( it.second.initializer ) it.second.initializer->validate ( &sym, &errHandler );
	}

	for ( auto& it : classList )
	{
		if ( it.second )
		{
			for ( auto &it2 : it.second->elems )
			{
				switch ( it2->type )
				{
					case fgxClassElementType::fgxClassType_static:
						if ( it2->data.iVar.initializer->getOp () == astOp::assign ) 
						{
							it2->data.iVar.initializer->right->validate ( &sym, &errHandler );
						}
						break;
					default:
						break;
				}
			}
		}
	}

	for ( auto& funcIt : functionList )
	{
		if ( funcIt.second ) validate ( funcIt.second );
	}
}

opFile::~opFile()
{
	for ( auto &it : functionList )
	{
		delete it.second;
	}
	for ( auto &it : classList )
	{
		delete it.second;
	}
}

errorNum opFile::addElem ( opFunction *func, symbolTypeClass const &type, bool isLS )
{
	func->retType = type;

	// TODO: refactor this... don't use exceptions for flow control.  this is an artificat of not initially being designed for language sever support (didn't exist at the time)
	return ns.add ( func, isLS );
}

errorNum opFile::addElem ( opClass *classDef, bool isLS )
{
	// TODO: refactor this... don't use exceptions for flow control.  this is an artificat of not initially being designed for language sever support (didn't exist at the time)
	return ns.add ( classDef, isLS );
}

errorNum opFile::addElem ( char const *lib, srcLocation &location, bool isModule, bool isLS )
{
	std::filesystem::path p ( lib );

	if ( isModule )
	{
		if ( !strccmp ( p.extension ().generic_string ().c_str (), ".flb" ) )
		{
			p.replace_extension ( ".slb" );

		}
		p = p.generic_string ();
		for ( auto it = modules.begin ( ); it != modules.end ( ); it++ )
		{
			if ( (*it).fName == p.generic_string ().c_str() )
			{
				for ( auto const &it : it->locations )
				{
					if ( it == location )
					{
						if ( !isLS ) throw errorNum::scMODULE_ALREADY_DEFINED;
						return errorNum::scMODULE_ALREADY_DEFINED;
					}
				}
				it->locations.insert ( location );
				return errorNum::ERROR_OK;
			}
		}

		modules.push_back ( moduleLibraryEntry ( sCache.get ( stringi ( p.string () ) ), location ) );
	} else
	{
		if ( !strccmp ( p.extension ().generic_string ().c_str (), ".flb" ) || !strccmp ( p.extension ().generic_string ().c_str (), ".slb" ) )
		{
			p.replace_extension ( ".sll" );
		}

		p = p.generic_string ();
		for ( auto it = libraries.begin (); it != libraries.end (); it++ )
		{
			if ( (*it).fName == p.generic_string ().c_str () )
			{
				for ( auto const &it : it->locations )
				{
					if ( it == location )
					{
						if ( !isLS ) throw errorNum::scMODULE_ALREADY_DEFINED;
						return errorNum::scMODULE_ALREADY_DEFINED;
					}
				}
				it->locations.insert ( location );
				return errorNum::ERROR_OK;
			}
		}

		libraries.push_back ( moduleLibraryEntry ( sCache.get ( stringi ( p.generic_string () ) ), location ) );
	}
	return errorNum::ERROR_OK;
}

std::pair<stringi, astNode *>	opFile::getDocumentation ( source &src )
{
	srcLocation start = src;
	stringi documentation;

	BS_ADVANCE ( src );

	if ( src[0] == '/' && src[1] == '/' )
	{
		source nextLine = src;
		srcLocation end = src;
		do {
			src += 2;	// comments are comments... don't parse out metadata, including position information
			end = src;
			while ( _isspace ( src ) )
			{
				src++;
			}
			while ( *src && !_iseol ( src ) )
			{
				end = src;
				documentation += *src;
				src++;
			}
			if ( src[0] == '\n' )
			{
				src++;
			} if ( src[0] == '\r' && src[1] == '\n' )
			{
				src += 2;
			} else
			{
				break;
			}
			nextLine = src;
			BS_ADVANCE ( nextLine );
			if ( !(nextLine[0] == '/' && nextLine[1] == '/') )
			{
				break;
			}
			src = nextLine;
		} while ( *src );

		return { documentation, (new astNode ( astLSInfo::semanticSymbolType::comment, start ))->setEnd ( end ) };
	}
	if ( src[0] == '/' && src[1] == '*' )
	{
		src += 2;
		srcLocation end = src;
		BS_ADVANCE_EOL ( this, false, src );
		uint32_t columnConsume = src.getColumn () - 1;
		while ( *src && !(src[0] == '*' && src[1] == '/') )
		{
			if ( _iseol ( src ) )
			{
				while ( _iseol ( src ) )
				{
					documentation += *src;
					src++;
				}
				for ( auto loop = columnConsume; loop; loop-- )
				{
					if ( !_isspace ( src ) )
					{
						break;
					}
					src++;
				}
			}
			if ( *src && !(src[0] == '*' && src[1] == '/') )
			{
				if ( *src != '*' )
				{
					documentation += *src;
				}
				src++;
			}
		}
		if ( src[0] && src[1] )
		{
			src++;
			end = src;
			src++;
		}
		return { documentation, (new astNode ( astLSInfo::semanticSymbolType::comment, start ))->setEnd ( end ) };
	}
	return { documentation, nullptr };
}

astNode *findFirstInScopeCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, uint32_t sourceIndex, srcLocation &loc )
{
	if ( node->location.sourceIndex == sourceIndex )
	{
		if ( loc.sourceIndex == sourceIndex )
		{
			if ( node->location < loc )
			{
				loc = node->location;
			}
		} else
		{
			loc = node->location;
		}
	} 
	return node;
};

void opFile::_parseFile ( source &src, bool doBraces, bool isLS )
{
	opFunction		*func;
	cacheString		 nsName;
	cacheString		 name;
	stringi			 documentation;
	char			*line;

	source primarySource = src;

	auto sourceIndex = src.getSourceIndex ();

	BS_ADVANCE_EOL ( this, isLS, src );

	while ( *src )
	{
		if ( *src == ';' )
		{
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
			src++;
			BS_ADVANCE_EOL ( this, isLS, src );
			continue;
		}
		errorLocality e ( &errHandler, src );
		srcLocation statementLocation = src;

		if ( src.isLineStart () )
		{
			auto it = ppMap.find ( statementString ( src ) );

			if ( it != ppMap.end () )
			{
				// TODO:  implement PP handling
				switch ( it->second.type )
				{
					case statementType::stPPdefine:
					case statementType::stPPinclude:
					case statementType::stPPif:
					case statementType::stPPifDef:
					case statementType::stPPifNdef:
					case statementType::stPPelse:
					case statementType::stPPelIf:
					case statementType::stPPendIf:
					case statementType::stPPerror:
					case statementType::stPPwarn:
					default:
						break;
				}
			}
		}

		auto [doc, commentStatement] = getDocumentation ( src );

		if ( commentStatement )
		{
			statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
			documentation = doc;
			BS_ADVANCE_EOL ( this, isLS, src );
			continue;
		}

		auto it = statementMap.find ( statementString ( src ) );

		if ( it != statementMap.end () )
		{
			src += it->second.len;
			statementLocation.setEnd ( src );

			try
			{
				switch ( it->second.type )
				{
					case statementType::stInteger:
					case statementType::stDouble:
					case statementType::stString:
					case statementType::stArray:
					case statementType::stCodeblock:
					case statementType::stVariant:
					case statementType::stObject:
						{
							auto fIt = functionList.find( mainValue );
							if ( fIt != functionList.end() )
							{
								it = statementMap.end();
								break;
							}

							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );

							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							if ( cmpTerminated ( src, "function", 8 ) )
							{
								src += 8;
							} else
							{
								if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
							}
							srcLocation nameLocation = src;
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src ); 
							try
							{
								name = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::function, nameLocation ) );
							} catch ( errorNum e )
							{
								if ( !isLS ) throw;
								errors.push_back ( std::make_unique<astNode> ( e, nameLocation ) );
								name = sCache.get ( stringi ( "errorName" ) + nameLocation.columnNumber + ":" + nameLocation.lineNumberStart );
							}
							func = parseFunc ( src, name, doBraces, isLS, statementLocation );
							func->location = statementLocation;
							if ( func->codeBlock ) func->location.setEnd ( func->codeBlock->location );
							func->nameLocation = nameLocation;
							func->documentation = documentation;
							if ( func )
							{
								switch ( it->second.type )
								{
									case statementType::stInteger:
										func->retType = symIntType;
										break;
									case statementType::stDouble:
										func->retType = symDoubleType;
										break;
									case statementType::stString:
										func->retType = symStringType;
										break;
									case statementType::stArray:
										func->retType = symArrayType;
										break;
									case statementType::stCodeblock:
										func->retType = symCodeblockType;
										break;
									case statementType::stObject:
										func->retType = symObjectType;
										break;
									case statementType::stVariant:
										func->retType = symVariantType;
										break;
									default:
										throw errorNum::scINTERNAL;
								}
								auto errorNum = ns.add ( func, isLS );
								if ( errorNum != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw errorNum;
									errors.push_back ( std::make_unique<astNode> ( errorNum, func->nameLocation ) );
								}
							}
						}
						break;
					case statementType::stIterator:
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							srcLocation nameLocation = src;
							try
							{
								name = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::iterator, nameLocation ) );
							} catch ( errorNum e )
							{
								if ( !isLS ) throw;
								errors.push_back ( std::make_unique<astNode> ( e, nameLocation ) );
								name = sCache.get ( stringi ( "errorName" ) + nameLocation.columnNumber + ":" + nameLocation.lineNumberStart );
							}
							nameLocation.setEnd ( src );
							func = parseFunc ( src, name, doBraces, isLS, statementLocation );
							func->isIterator = true;
							func->nameLocation = nameLocation;
							func->retType = symUnknownType;
							func->location = statementLocation;
							if ( func->codeBlock ) func->location.setEnd ( func->codeBlock->location );
							func->documentation = documentation;
							auto errorNum = ns.add ( func, isLS );

							if ( !strccmp ( func->name.c_str (), "main" ) )
							{
								if ( it->second.type == statementType::stIterator )
								{
									if ( !isLS ) throw errorNum::scITERATOR_MAIN;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scITERATOR_MAIN, func->nameLocation ) );
								}
							}
							if ( errorNum != errorNum::ERROR_OK )
							{
								if ( !isLS ) throw errorNum;
								errors.push_back ( std::make_unique<astNode> ( errorNum, func->nameLocation ) );
							}
						}
						break;
					case statementType::stFunction:
					case statementType::stProcedure:
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

							BS_ADVANCE_EOL ( this, isLS, src );
							srcLocation nameLocation = src;
							try
							{
								name = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::function, nameLocation ) );
							} catch ( errorNum e )
							{
								if ( !isLS ) throw;
								errors.push_back ( std::make_unique<astNode> ( e, nameLocation ) );
								name = sCache.get ( stringi ( "errorName" ) + nameLocation.columnNumber + ":" + nameLocation.lineNumberStart );
							}
							nameLocation.setEnd ( src );
							func = parseFunc ( src, name, doBraces, isLS, statementLocation );
							func->nameLocation = nameLocation;
							func->retType = it->second.type == statementType::stIterator ? symVariantType : symUnknownType;
							func->location = statementLocation;
							func->extendedSelection = statementLocation;
							if ( func->codeBlock )
							{
								func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
								func->location.setEnd ( func->codeBlock->location );
							}
							if ( documentation.size() ) func->documentation = documentation;
							auto errorNum = ns.add ( func, isLS );
							if ( !strccmp ( func->name.c_str (), "main" ) )
							{
								if ( it->second.type == statementType::stIterator )
								{
									if ( !isLS ) throw errorNum::scITERATOR_MAIN;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scITERATOR_MAIN, src ) );
								}
							}
							if ( errorNum != errorNum::ERROR_OK )
							{
								if ( !isLS ) throw errorNum;
								errors.push_back ( std::make_unique<astNode> ( errorNum, func->nameLocation ) );
							}
						}
						break;
					case statementType::stFinal:
					case statementType::stClass:
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeClass, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

							bool final = false;
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							if ( it->second.type == statementType::stFinal )
							{
								if ( !cmpTerminated ( src, "CLASS", 5 ) )
								{
									if ( !isLS ) throw errorNum::scILLEGAL_FINAL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_FINAL, src ) );
									break;
								} else
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 5 ) );
									src += 5;
								}
								final = true;
							}
							auto cls = parseClass ( src, doBraces, isLS, statementLocation );
							srcLocation newLoc = statementLocation;
							newLoc.setEnd ( cls->location );
							cls->location = newLoc;
							cls->isFinal = final;
							cls->documentation = documentation;
							auto errorNum = ns.add ( cls, isLS );
							if ( errorNum != errorNum::ERROR_OK )
							{
								if ( !isLS ) throw errorNum;
								errors.push_back ( std::make_unique<astNode> ( errorNum, cls->location ) );
							}
						}
						break;
					case statementType::stNamespace:
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeNamespaceName, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, astLSInfo::semanticLineBreakType::beforeNamespaceName, statementLocation ) );

							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							srcLocation nameLocation = src;
							try
							{
								nsName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::name, nameLocation ) );
							} catch ( errorNum e )
							{
								if ( !isLS ) throw;
								errors.push_back ( std::make_unique<astNode> ( e, nameLocation ) );
								nsName = sCache.get ( stringi ( "errorName" ) + nameLocation.columnNumber + ":" + nameLocation.lineNumberStart );
							}
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterNamespaceName, src ) );
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							ns.pushNamespace ( nsName );
							if ( doBraces )
							{
								if ( *src != '{' )
								{
									if ( !isLS ) throw errorNum::scMISSING_OPEN_BRACE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_OPEN_BRACE, src ) );
								} else
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, src ) );
									src++;
								}
							}
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeNamespace, src ) );
							_parseFile ( src, doBraces, isLS );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterNamespace, statements.back()->location ) );
							ns.popNamespace ();
						}
						break;
					case statementType::stUsing:
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							srcLocation nameLocation = src;
							try
							{
								nsName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::name, nameLocation ) );
							} catch ( errorNum e )
							{
								if ( !isLS ) throw;
								errors.push_back ( std::make_unique<astNode> ( e, nameLocation ) );
								nsName = sCache.get ( stringi ( "errorName" ) + nameLocation.columnNumber + ":" + nameLocation.lineNumberStart );
							}
							ns.use ( nsName );
						}
						break;
					case statementType::stGlobal:
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

						// TODO: add post-comment documentation
						while ( *src )
						{
							auto expr = parseExpr ( src, false, false, 0, doBraces, isLS );
							if ( !expr )
							{
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXPRESSION, src ) );
							} else
							{
								auto errorNum = ns.add ( opSymbol::symbolClass::symbolClass_global, expr, true, false, isLS, documentation );
								if ( errorNum != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw errorNum;
									errors.push_back ( std::make_unique<astNode> ( errorNum, expr->location ) );
								}
							}
							BS_ADVANCE ( src );

							if ( *src == ',' )
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
								src++;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							} else
							{
								if ( *src == ';' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
									src++;
									break;
								} else if ( _iseol ( src ) )
								{
								} else
								{
									if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
								}
								break;
							}
						}
						break;
					case statementType::stStatic:
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

						while ( 1 )
						{
							auto expr = parseExpr ( src, false, false, 0, doBraces, isLS );
							if ( !expr )
							{
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXPRESSION, src ) );
							} else
							{
								auto errorNum = ns.add ( opSymbol::symbolClass::symbolClass_global, expr, false, false, isLS, documentation );
								if ( errorNum != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw errorNum;
									errors.push_back ( std::make_unique<astNode> ( errorNum, expr->location ) );
								}
							}

							BS_ADVANCE ( src );

							if ( *src == ',' )
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
								src++;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							} else
							{
								if ( *src == ';' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
									src++;
								} else if ( _iseol ( src ) )
								{
								} else
								{
									if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
								}
								break;
							}
						}
						break;
					case statementType::stLibrary:
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							srcLocation lineLoc = src;
							line = getLine ( src );
							lineLoc.setEnd ( src );
							errorNum err = addElem ( line, lineLoc, false, isLS );
							free ( line );
							if ( err != errorNum::ERROR_OK )
							{
								if ( !isLS ) throw err;
								errors.push_back ( std::make_unique<astNode> ( err, lineLoc ) );
							}
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::stringLiteral, lineLoc ) );
						}
						break;
					case statementType::stModule:
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							srcLocation lineLoc = src;
							line = getLine ( src );
							lineLoc.setEnd ( src );
							errorNum err = addElem ( line, lineLoc, true, isLS );
							free ( line );
							if ( err != errorNum::ERROR_OK )
							{
								if ( !isLS ) throw err;
								errors.push_back ( std::make_unique<astNode> ( err, lineLoc ) );
							}
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::stringLiteral, lineLoc ) );
						}
						break;
					case statementType::stEnd:
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeEnd, statementLocation ) );
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
						if ( doBraces )
						{
							if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
							errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, statementLocation ) );
						}
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterEnd, src ) );
						return;
					case statementType::stCloseBrace:
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, statementLocation ) );
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlock, statementLocation ) );
						if ( !doBraces )
						{
							if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
							errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, statementLocation ) );
						}
						return;
					case statementType::stPragma:
						if ( isLS )
						{
							statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							BS_ADVANCE_COMMENT ( this, isLS, src );
							srcLocation nameLocation = src;
							srcLocation pragmaLoc = src;
							name = sCache.get ( getSymbol ( src ) );
							nameLocation.setEnd ( src );
							statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::macro, nameLocation ) );
							if ( name == "region" )
							{
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								auto language = getSymbol ( src );

								languageRegion region;
								if ( language == "html" )
								{
									region.language = languageRegion::languageId::html;
								} else if ( language == "slang" )
								{
									region.language = languageRegion::languageId::slang;
								} else if ( language == "fgl" )
								{
									region.language = languageRegion::languageId::fgl;
								}

								region.location.sourceIndex = src.getSourceIndex ();
								sscanf_s ( src, "%u %u %u %u", &region.location.columnNumber, &region.location.lineNumberStart, &region.location.columnNumberEnd, &region.location.lineNumberEnd );

								languageRegions[region.location] = region.language;
							} else if ( name == "token" )
							{
								char tokenName[256]{};
								srcLocation location;
								sscanf_s ( src, "%s %u %u %u %u", tokenName, (unsigned int)sizeof ( tokenName ), &location.columnNumber, &location.lineNumberStart, &location.columnNumberEnd, &location.lineNumberEnd );
								location.sourceIndex = src.getSourceIndex ();
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::nameToSemanticTokenType[stringi ( tokenName )], location ) );
							} else
							{
								warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwUNKNOWN_PRAGMA, errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_PRAGMA, name ) ) );
								warnings.back ().get ()->setLocation ( nameLocation );
							}
							pragmaLoc.setEnd ( src );
							statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::stringLiteral, pragmaLoc ) );
						}
						src.advanceEOL ();
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
						break;
					case statementType::stMethod:
					case statementType::stAssign:
					case statementType::stAccess:
					case statementType::stOperator:
					case statementType::stExtension:
						// these are all error cases, can only exist within a class, however let's parse them so that we can do better error identification within the language server
						if ( !isLS )
						{
							throw errorNum::scNESTING;
						} else
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							errors.push_back( std::make_unique<astNode>( errorNum::scNESTING, statementLocation ) );

							if ( isLS ) statements.push_back( std::make_unique<astNode>( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

							BS_ADVANCE_EOL( this, isLS, src );
							srcLocation nameLocation = src;
							try
							{
								name = sCache.get( getSymbol( src ) );
								nameLocation.setEnd( src );
								if ( isLS ) statements.push_back( std::make_unique<astNode>( astLSInfo::semanticSymbolType::function, nameLocation ) );
							} catch ( errorNum e )
							{
								if ( !isLS ) throw;
								errors.push_back( std::make_unique<astNode>( e, nameLocation ) );
								name = sCache.get( stringi( "errorName" ) + nameLocation.columnNumber + ":" + nameLocation.lineNumberStart );
							}
							nameLocation.setEnd( src );
							func = parseFunc( src, name, doBraces, isLS, statementLocation );
							func->nameLocation = nameLocation;
							func->retType = it->second.type == statementType::stIterator ? symVariantType : symUnknownType;
							func->location = statementLocation;
							if ( func->codeBlock ) func->location.setEnd( func->codeBlock->location );
							if ( documentation.size() ) func->documentation = documentation;
							auto errorNum = ns.add( func, isLS );
							if ( !strccmp( func->name.c_str(), "main" ) )
							{
								if ( it->second.type == statementType::stIterator )
								{
									if ( !isLS ) throw errorNum::scITERATOR_MAIN;
									errors.push_back( std::make_unique<astNode>( errorNum::scITERATOR_MAIN, src ) );
								}
							}
							if ( errorNum != errorNum::ERROR_OK )
							{
								if ( !isLS ) throw errorNum;
								errors.push_back( std::make_unique<astNode>( errorNum, func->nameLocation ) );
							}
						}
						break;
					case statementType::stPublic:
					case statementType::stPrivate:
					case statementType::stProtected:
					case statementType::stGet:
					case statementType::stSet:
					case statementType::stProperty:
					case statementType::stInherit:
						if ( !isLS )
						{
							throw errorNum::scNESTING;
						} else
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeScope, statementLocation ) );
							errors.push_back( std::make_unique<astNode>( errorNum::scNESTING, statementLocation ) );

							if ( isLS ) statements.push_back( std::make_unique<astNode>( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterScope, src ) );
						}
						break;
					case statementType::stMessage:
						if ( !isLS )
						{
							throw errorNum::scNESTING;
						} else
						{
							errors.push_back( std::make_unique<astNode>( errorNum::scNESTING, statementLocation ) );

							if ( isLS ) statements.push_back( std::make_unique<astNode>( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
						}
						break;
					default:
						src -= it->second.len;
						goto autoMain;
				}
			} catch ( errorNum err )
			{
				assert ( !isLS );
				errHandler.throwFatalError ( isLS, err );
			}
		}
		documentation = stringi();

		if ( it == statementMap.end() )
		{
autoMain:
			auto it = functionList.find( mainValue );
			if ( it != functionList.end() )
			{
				if ( !doBraces )
				{
					if ( !isLS ) throw errorNum::scDUPLICATE_FUNCTION;
					errors.push_back( std::make_unique<astNode>( errorNum::scDUPLICATE_FUNCTION, src ) );
				}
			}
			// auto-main?

			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, src ) );
			func = parseFuncAutoMain ( src, "main", doBraces, isLS, statementLocation );
			func->location = statementLocation;
			astNodeWalk ( func->codeBlock, nullptr, findFirstInScopeCB, sourceIndex, func->location );
			func->nameLocation = func->location;
			if ( func->codeBlock ) func->location.setEnd ( func->codeBlock->location );
			func->isAutoMain = true;
			auto err = ns.add ( func, isLS );
			if ( err != errorNum::ERROR_OK )
			{
				if ( !isLS ) throw err;
				errors.push_back ( std::make_unique<astNode> ( err, func->nameLocation ) );
			}
			if ( *src )
			{
				if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
				srcLocation extraLocation = src;
				src.advanceGarbage ();
				extraLocation.setEnd ( src );
				statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::comment, extraLocation ) );
			}
		}
		BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
	}
}

void opFile::parseFile ( char const *fileName, char const *expr, bool doBraces, bool isLS )
{
	source src ( &srcFiles, sCache, fileName, expr );
	addBuildSource ( fileName );
	srcLocation entireRegion ( src );
	parseFile ( src, doBraces, isLS );
	entireRegion.setEnd ( src );
	if ( languageRegions.empty () )
	{
		if ( doBraces )
		{
			languageRegions.insert ( { entireRegion, languageRegion::languageId::slang } );
		} else
		{
			languageRegions.insert ( { entireRegion, languageRegion::languageId::fgl } );
		}
	}
}

void opFile::parseFile ( source &src, bool doBraces, bool isLS )
{
	srcLocation entireRegion ( src );
	try
	{
		errorLocality e ( &errHandler, src );

		_parseFile ( src, doBraces, isLS );
		if ( *src )
		{
			if ( !isLS ) throw errorNum::scEXTRA_END;
			errors.push_back ( std::make_unique<astNode> ( errorNum::scEXTRA_END, src ) );
		}
	} catch ( errorNum num )
	{
		errHandler.throwFatalError ( isLS, num );
	}
	entireRegion.setEnd ( src );
	if ( languageRegions.empty () )
	{
		if ( doBraces )
		{
			languageRegions.insert ( { entireRegion, languageRegion::languageId::slang } );
		} else
		{
			languageRegions.insert ( { entireRegion, languageRegion::languageId::fgl } );
		}
	}

}

std::vector<srcLocation> opFile::getRegions ( srcLocation const &loc, languageRegion::languageId lang ) const
{
	std::vector<srcLocation> ret;

	srcLocation startPt ( loc.sourceIndex, loc.columnNumber, loc.lineNumberStart, loc.columnNumber, loc.lineNumberStart );
	srcLocation endPt ( loc.sourceIndex, loc.columnNumberEnd, loc.lineNumberEnd, loc.columnNumberEnd, loc.lineNumberEnd );

//	for ( auto const &[location, language] : languageRegions )
	for ( auto it = languageRegions.begin(); it != languageRegions.end(); it++ )
	{
		srcLocation const location = it->first;
		languageRegion::languageId const language = it->second;

		if ( location.encloses ( startPt ) )
		{
			// we've found the start of our region.
			if ( language == lang )
			{
				// our start is within the language we want.
				if ( location.encloses ( endPt ) )
				{
					// everything contained in one location, so emit start up to end
					srcLocation pt ( loc.sourceIndex, startPt.columnNumber, startPt.lineNumberStart, endPt.columnNumber, endPt.lineNumberEnd );
					ret.push_back ( pt );
					break;
				} else
				{
					// this is partial, emit start up to the end of the current region
					srcLocation pt ( loc.sourceIndex, startPt.columnNumber, startPt.lineNumberStart, endPt.columnNumber, endPt.lineNumberEnd );
					ret.push_back ( pt );
				}
			}
			// our start is either in the wrong region or we've emitted a partial.   In any case, we just need to eithr e
			for ( ;  it != languageRegions.end (); it++ )
			{
				srcLocation const partialLocation = it->first;
				languageRegion::languageId const partialLanguage = it->second;
				if ( partialLanguage == lang )
				{
					if ( partialLocation.encloses ( endPt ) )
					{
						// we'ver found our end... emit the location information and break;
						srcLocation pt ( loc.sourceIndex, partialLocation.columnNumber, partialLocation.lineNumberStart, endPt.columnNumber, endPt.lineNumberEnd );
						ret.push_back ( pt );
						break;
					} else
					{
						// we're the proper language but our selection encompases the entire region so emit it all
						srcLocation pt ( loc.sourceIndex, partialLocation.columnNumber, partialLocation.lineNumberStart, partialLocation.columnNumberEnd, partialLocation.lineNumberEnd );
						ret.push_back ( pt );
					}
				}
			}
			break;
		}
	}

	return ret;
}

languageRegion::languageId opFile::getRegion ( srcLocation const &loc ) const
{
	std::vector<srcLocation> ret;

	srcLocation startPt ( loc.sourceIndex, loc.columnNumber, loc.lineNumberStart, loc.columnNumber, loc.lineNumberStart );
	srcLocation endPt ( loc.sourceIndex, loc.columnNumberEnd, loc.lineNumberEnd, loc.columnNumberEnd, loc.lineNumberEnd );

//	for ( auto const &[location, language] : languageRegions )
	for ( auto it = languageRegions.begin (); it != languageRegions.end (); it++ )
	{
		srcLocation const location = it->first;
		languageRegion::languageId const language = it->second;

		if ( location.encloses ( startPt ) )
		{
			return language;
		}
	}
	return languageRegion::languageId::fgl;
}

std::vector<std::pair<srcLocation, languageRegion::languageId>> opFile::getRegionsExcept ( srcLocation const &loc, languageRegion::languageId lang ) const
{
	std::vector<std::pair<srcLocation, languageRegion::languageId>> ret;

	srcLocation startPt ( loc.sourceIndex, loc.columnNumber, loc.lineNumberStart, loc.columnNumber, loc.lineNumberStart );
	srcLocation endPt ( loc.sourceIndex, loc.columnNumberEnd, loc.lineNumberEnd, loc.columnNumberEnd, loc.lineNumberEnd );

//	for ( auto const &[location, language] : languageRegions )
	for ( auto it = languageRegions.begin (); it != languageRegions.end (); it++ )
	{
		srcLocation const location = it->first;
		languageRegion::languageId const language = it->second;

		if ( location.encloses ( startPt ) )
		{
			// we've found the start of our region.
			if ( language != lang )
			{
				// our start is within the language we want.
				if ( location.encloses ( endPt ) )
				{
					// everything contained in one location, so emit start up to end
					srcLocation pt ( loc.sourceIndex, startPt.columnNumber, startPt.lineNumberStart, endPt.columnNumber, endPt.lineNumberEnd );
					ret.push_back ( { pt, language } );
					break;
				} else
				{
					// this is partial, emit start up to the end of the current region
					srcLocation pt ( loc.sourceIndex, startPt.columnNumber, startPt.lineNumberStart, endPt.columnNumber, endPt.lineNumberEnd );
					ret.push_back ( { pt, language } );
				}
			}
			// our start is either in the wrong region or we've emitted a partial.   In any case, we just need to eithr e
			for ( ; it != languageRegions.end (); it++ )
			{
				auto const &partialLocation = it->first;
				auto const &partialLanguage = it->second;
				if ( partialLanguage != lang )
				{
					if ( partialLocation.encloses ( endPt ) )
					{
						// we'ver found our end... emit the location information and break;
						srcLocation pt ( loc.sourceIndex, partialLocation.columnNumber, partialLocation.lineNumberStart, endPt.columnNumber, endPt.lineNumberEnd );
						ret.push_back ( { pt, language } );
						break;
					} else
					{
						// we're the proper language but our selection encompases the entire region so emit it all
						srcLocation pt ( loc.sourceIndex, partialLocation.columnNumber, partialLocation.lineNumberStart, partialLocation.columnNumberEnd, partialLocation.lineNumberEnd );
						ret.push_back ( { pt, language } );
					}
				}
			}
			break;
		}
	}

	return ret;
}

bool opFile::isInRegion ( srcLocation const &location ) const
{
	if ( languageRegions.empty () ) return true;
	auto it = languageRegions.lower_bound ( location );

	if ( (it != languageRegions.end ()) && it->first.encloses ( location ) )
	{
		switch ( it->second )
		{
			case languageRegion::languageId::fgl:
			case languageRegion::languageId::slang:
				return true;
			default:
				return false;
		}
	}
	if ( it != languageRegions.begin () )
	{
		it--;
		if ( it->first.encloses ( location ) )
		{
			switch ( it->second )
			{
				case languageRegion::languageId::fgl:
				case languageRegion::languageId::slang:
					return true;
				default:
					return false;
			}
		}
	}
	return false;
}
void opFile::clearSourceIndex ( size_t sourceIndex )
{	
	// clear libraries 
	for ( auto &it : libraries )
	{
		for ( auto loc = it.locations.begin (); loc != it.locations.end (); )
		{
			if ( loc->sourceIndex == sourceIndex )
			{
				loc = it.locations.erase ( loc );
			} else
			{
				loc++;
			}
		}
	}
	
	// clear modules 
	for ( auto &it : modules )
	{
		for ( auto loc = it.locations.begin (); loc != it.locations.end (); )
		{
			if ( loc->sourceIndex == sourceIndex )
			{
				loc = it.locations.erase ( loc );
			} else
			{
				loc++;
			}
		}
	}

	// clear functions
	for ( auto it = functionList.begin (); it != functionList.end (); )
	{
		if ( !sourceIndex || it->second->location.sourceIndex == sourceIndex )
		{
			it = functionList.erase ( it );
		} else
		{
			it++;
		}
	}	
	// clear classes
	for ( auto it = classList.begin (); it != classList.end (); )
	{
		if ( !sourceIndex ||it->second->location.sourceIndex == sourceIndex )
		{
			it = classList.erase ( it );
		} else
		{
			for ( auto elemIt = it->second->elems.begin();  elemIt != it->second->elems.end(); )
			{
				if ( (*elemIt)->type == fgxClassElementType::fgxClassType_method )
				{
					if ( functionList.find ( (*elemIt)->data.method.func ) == functionList.end() )
					{
						elemIt = it->second->elems.erase ( elemIt );
					} else
					{
						elemIt++;
					}
				} else
				{
					elemIt++;
				}
			}
			it++;
		}
	}	
	// clear symbols
#if 0
	std::erase_if ( symbols, [sourceIndex]( auto sym ) {
		return !sourceIndex || sym.second.location.get ()->location.sourceIndex || sym.second.location.sourceIndex == sourceIndex;
					} );
#endif

	for ( auto it = symbols.begin (); it != symbols.end (); )
	{
		if ( !sourceIndex || it->second.location.sourceIndex == sourceIndex )
		{
			it = symbols.erase ( it );
		} else
		{
			it++;
		}
	}
	// clear errors
	std::erase_if ( errors, [sourceIndex]( auto &node ) { return !sourceIndex || node.get ()->location.sourceIndex == sourceIndex; } );

	// clear warnings
	std::erase_if ( warnings, [sourceIndex]( auto &node ) { return !sourceIndex || node.get ()->location.sourceIndex == sourceIndex; } );

	// clear statements
	std::erase_if ( statements, [sourceIndex]( auto &node ) { return !sourceIndex || node.get ()->location.sourceIndex == sourceIndex; } );

	// clear the file's language regions (ap files to differentiate between slang/fgl and html in the code)
	languageRegions.clear ();
	autoFormatPoints.clear ();
}

opClass *opFile::findClass ( char const *name )
{
	cacheString className = ns.makeName ( name );
	auto clsIt  = classList.find ( className );
	if ( clsIt == classList.end() )
	{
		throw errorNum::scINTERNAL;
	}
	return (*clsIt).second;
}
