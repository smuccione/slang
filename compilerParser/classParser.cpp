/*

	block processor


*/


#include "fileParser.h"
#include "Utility/counter.h"

opClass::opClass()
{
	opOverload.resize ( size_t(fgxOvOp::ovMaxOverload) + 1, (opClassElement *)0 );
	cClass = 0;
	isInterface = false;
}

opClass::~opClass()
{
	for ( auto it = elems.begin(); it != elems.end(); it++ )
	{
		delete (*it);
	}
	for ( auto it = opOverload.begin(); it != opOverload.end(); it++ )
	{
		if ( *it) delete (*it);
	}
}

opClass::opClass ( class opFile *file, sourceFile *srcFile, char const ** expr )
{
	uint32_t	 loop;

	cClass = 0;

	isInterface = (bool)**expr;
	(*expr)++;

	isFinal = (bool) **expr;
	(*expr)++;

	name = file->sCache.get ( expr );
	documentation = stringi ( expr );

	location = srcLocation ( srcFile, expr );
	nameLocation = srcLocation ( srcFile, expr );

	loop = *((uint32_t*) *expr);
	(*expr) += sizeof ( uint32_t );
	for (; loop; loop-- )
	{
		requiredClasses.push_back( file->sCache.get ( expr ) );
	}

	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );
	for (; loop; loop-- )
	{
		if ( *((*expr))++ ) elems.push_back ( new opClassElement ( file, expr, this, srcFile ) );
	}

	loop = *((uint32_t *)*expr);
	(*expr) += sizeof ( uint32_t );
	for (; loop; loop-- )
	{
		if ( *((*expr))++ ) 
		{
			opOverload.push_back ( new opClassElement ( file, expr, this, srcFile ) );
		} else
		{
			opOverload.push_back ( 0 );
		}
	}

	usingList = opUsingList ( file, expr );
}

void opClass::serialize ( BUFFER *buff )
{
	bufferPut8 ( buff, (char)isInterface );
	bufferPut8 ( buff, (char)isFinal );

	name.serialize ( buff );
	documentation.serialize ( buff );

	location.serialize ( buff );
	nameLocation.serialize ( buff );

	buff->put( (uint32_t) requiredClasses.size() );
	for ( auto& it : requiredClasses )
	{
		it.serialize ( buff );
	}

	bufferPut32 ( buff, (uint32_t)elems.size() );
	for ( auto it = elems.begin(); it != elems.end(); it++ )
	{
		(*it)->serialize ( buff );
	}

	bufferPut32 ( buff, (uint32_t)opOverload.size() );
	for ( auto it = opOverload.begin(); it != opOverload.end(); it++ )
	{
		(*it)->serialize ( buff );
	}

	usingList.serialize ( buff );
}

opClassElement::opClassElement ( class opFile *file, char const ** expr, opClass *classDef, class sourceFile *src )
{
	type  = (fgxClassElementType)  *((*expr)++);
	scope = (fgxClassElementScope) *((*expr)++);

	name = file->sCache.get ( expr );
	documentation = stringi ( expr );

	location = srcLocation ( src, expr );

	isVirtual = **expr & 1;
	(*expr)++;

	symType = symbolTypeClass ( file, expr );

	data.iVar.initializer = 0;

	switch ( type )
	{
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			data.iVar.index = *((uint32_t *)*expr);
			(*expr) += sizeof ( uint32_t );

			data.iVar.initializer = OP_NODE_DESERIALIZE ( file, src, expr );
			break;
		case fgxClassElementType::fgxClassType_method:
			data.method.func = file->sCache.get ( expr );
			{
				auto funcIt = file->functionList.find ( data.method.func );
				if ( funcIt == file->functionList.end ( ) ) throw errorNum::scINTERNAL;
				(*funcIt).second->classDef = classDef;
			}
			break;
		case fgxClassElementType::fgxClassType_prop:
			data.prop.assign = file->sCache.get ( expr );
			if ( data.prop.assign.size() )
			{
				auto funcIt = file->functionList.find ( data.prop.assign );
				if ( funcIt == file->functionList.end ( ) ) throw errorNum::scINTERNAL;
				(*funcIt).second->classDef = classDef;
			}

			data.prop.access = file->sCache.get ( expr );
			if ( data.prop.access.size ( ) )
			{
				auto funcIt = file->functionList.find ( data.prop.access );
				if ( funcIt == file->functionList.end ( ) ) throw errorNum::scINTERNAL;
				(*funcIt).second->classDef = classDef;
			}
			break;
		case fgxClassElementType::fgxClassType_message:
			data.message.oldName = file->sCache.get ( expr );
			break;
		default:
			break;
	}
}

void opClassElement::serialize( BUFFER *buff )
{
	if ( (ptrdiff_t)this )
	{
		bufferPut8 ( buff, 1 );
	} else
	{
		bufferPut8 ( buff, 0 );
		return;
	}

	bufferPut8 ( buff, uint8_t(type) );
	bufferPut8 ( buff, uint8_t ( scope) );

	name.serialize ( buff );
	documentation.serialize ( buff );

	location.serialize ( buff );

	bufferPut8  ( buff, isVirtual );

	symType.serialize ( buff );

	switch ( type )
	{
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			bufferPut32 ( buff, data.iVar.index );
			data.iVar.initializer->serialize ( buff );
			break;
		case fgxClassElementType::fgxClassType_method:
			data.method.func.serialize ( buff );
			break;
		case fgxClassElementType::fgxClassType_prop:
			data.prop.assign.serialize ( buff );
			data.prop.access.serialize ( buff );
			break;
		case fgxClassElementType::fgxClassType_message:
			data.message.oldName.serialize ( buff );
			break;
		default:
			break;
	}
}

void opClassElement::setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	switch ( type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				for ( auto &it : data.prop.accessVirtOverrides )
				{
					it->setAccessed ( acc, scanQueue );
				}
			} else
			{
				for ( auto &it : data.prop.assignVirtOverrides )
				{
					it->setAccessed ( acc, scanQueue );
				}
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			for ( auto &it : data.method.virtOverrides )
			{
				it->setAccessed ( acc, scanQueue );
			}
			break;
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
			if ( isAccess )
			{
				if ( accessors.empty () )
				{
					if ( scanQueue ) scanQueue->push ( accessorType ( this ) );
				}
				accessors.insert ( acc );
			}
			break;
		default:
			break;
	}
}

// iVar
errorNum opClass::add ( cacheString const &name, fgxClassElementType type, fgxClassElementScope scope, astNode *initializer, stringi const &documentation )
{
	opClassElement	*elem;
	for ( auto it = elems.begin(); it != elems.end(); it++ )
	{
		if ( name == (*it)->name )
		{
			return errorNum::scDUPLICATE_IDENTIFIER;
		}
	}

	elem = new opClassElement;

	elem->type					= type;
	elem->scope					= scope;
	elem->name					= name;
	elem->location				= initializer->location;
	elem->isVirtual				= false;
	elem->data.iVar.initializer = initializer;
	elem->documentation			= documentation;
	elems.push_back ( elem );
	return errorNum::ERROR_OK;
}

errorNum opClass::add ( cacheString const &name, fgxClassElementType type, fgxClassElementScope scope, srcLocation const &location, bool virt, stringi const &documentation )
{
	opClassElement	*elem;

	for ( auto it = elems.begin(); it != elems.end(); it++ )
	{
		if ( name == (*it)->name && (type != (*it)->type) )
		{
			return errorNum::scDUPLICATE_METHOD;
		}
	}

	elem = new opClassElement;

	elem->type					= type;
	elem->scope					= scope;
	elem->name					= name;
	elem->location				= location;
	elem->isVirtual				= virt;
	elem->data.iVar.initializer	= 0;
	elem->documentation			= documentation;
	elems.push_back ( elem );
	return errorNum::ERROR_OK;
}

errorNum opClass::addMethod ( cacheString const &name, fgxClassElementType type, fgxClassElementScope scope, bool isVirtual, bool isStatic, bool isExtension, opFunction *func, opFile *file, symbolTypeClass const &symType, bool addFunc, stringi const &documentation )
{
	opClassElement	*elem;
	auto ret = errorNum::ERROR_OK;

	for ( auto &it : elems )
	{
		if ( (name == it->name) && (type == it->type) )
		{
			if ( addFunc )
			{
				func->name = file->sCache.get ( MakeUniqueLabel ( "error" ) );
			}
			ret = errorNum::scDUPLICATE_IDENTIFIER;
			break;
		}
	}

	elem = new opClassElement;

	elem->location			= func->location;
	elem->type				= type;
	elem->scope				= scope;
	elem->name				= name;
	elem->data.method.func	= func->name;
	elem->isVirtual			= isVirtual;
	elem->documentation			= documentation;
	elems.push_back ( elem );

	if ( addFunc )
	{
		func->classDef			= this;
		func->isInterface		= this->isInterface;
		func->isExtension		= isExtension;
		func->isStatic			= isStatic;

		func->retType			= symType;			// NOTE: can NOT call setReturnType()... this does an &= which will convert strong to a weak type

		if ( ret == errorNum::ERROR_OK )
		{
			ret = file->ns.add ( func, true );
		} else
		{
			file->ns.add ( func, true );
		}
	}

	return ret;
}

errorNum opClass::addProperty ( cacheString const &name, bool isAccess, fgxClassElementScope scope, bool isVirtual, opFunction *func, opFile *file, symbolTypeClass const &symType, stringi const &documentation )
{
	func->classDef = this;
	func->isInterface = this->isInterface;

	for ( auto &it : elems )
	{
		if ( (it->name == name) && (it->type == fgxClassElementType::fgxClassType_prop) )
		{
			if ( isAccess )
			{
				if ( it->data.prop.access.size ( ) )
				{
					func->name = file->sCache.get ( MakeUniqueLabel ( "error" ) );
					it->data.prop.access = func->name;
					func->retType = symType;
					file->ns.add ( func, true );
					return errorNum::scDUPLICATE_IDENTIFIER;
				}
				it->data.prop.access = func->name;
				func->retType		 = symType;
				return file->ns.add ( func, true );
			} else
			{
				if ( it->data.prop.assign.size ( ) )
				{
					func->name = file->sCache.get ( MakeUniqueLabel ( "error" ) );
					it->data.prop.assign = func->name;
					func->retType = symType;
					file->ns.add ( func, true );
					return errorNum::scDUPLICATE_IDENTIFIER;
				}
				it->data.prop.assign = func->name;
				func->retType		 = symType;
				return file->ns.add ( func, true );
			}
		}
	}

	opClassElement *elem;

	elem = new opClassElement;
	elem->location = func->location;
	elem->type = fgxClassElementType::fgxClassType_prop;
	elem->scope = scope;
	elem->name = name;
	elem->isVirtual = isVirtual;
	elem->documentation = documentation;
	elems.push_back ( elem );

	if ( isAccess )
	{
		elem->data.prop.access = func->name;
	} else
	{
		elem->data.prop.assign = func->name;
	}

	func->retType = symType;
	return file->ns.add ( func, true );
}


errorNum opClass::addIVar ( astNode *node, fgxClassElementType type, fgxClassElementScope scope, stringi const &documentation )
{
	if ( !node ) return errorNum::scBAD_VARIABLE_DEF;
	switch ( node->getOp() )
	{
		case astOp::symbolValue:
			add ( node->symbolValue (), type, scope, node, documentation );
			break;
		case astOp::assign:
			if ( node->left->getOp() != astOp::symbolValue )
			{
				delete node;
				return errorNum::scBAD_VARIABLE_DEF;
			}
			add ( node->left->symbolValue(), type, scope, node, documentation );
			break;
		case astOp::errorValue:
			return errorNum::scBAD_VARIABLE_DEF;
		default:
			delete node;
			return errorNum::scBAD_VARIABLE_DEF;
	}
	return errorNum::ERROR_OK;
}

errorNum opClass::addConst ( astNode *node, fgxClassElementType type, fgxClassElementScope scope, stringi const &documentation )
{
	if ( !node ) return errorNum::scBAD_VARIABLE_DEF;
	switch ( node->getOp() )
	{
		case astOp::assign:
			if ( node->left->getOp() != astOp::symbolValue )
			{
				delete node;
				return errorNum::scBAD_VARIABLE_DEF;
			}
			add ( node->left->symbolValue(), type, scope, node, documentation );
			break;
		default:
			delete node;
			return errorNum::scBAD_VARIABLE_DEF;
	}
	return errorNum::ERROR_OK;
}

errorNum opClass::addMessage ( cacheString const &name, cacheString const &oldName, srcLocation const &locatio, stringi const &documentation )
{
	opClassElement *elem;

	for ( auto it = elems.begin (); it != elems.end (); it++ )
	{
		if ( !strccmp ( name.c_str (), (*it)->name.c_str () ) )
		{
			return errorNum::scDUPLICATE_IDENTIFIER;
		}
	}

	elem = new opClassElement;

	elem->location				= location;
	elem->type					= fgxClassElementType::fgxClassType_message;
	elem->scope					= fgxClassElementScope::fgxClassScope_public;	// taken from origional identifiere... we're only renaming
	elem->name					= name;
	elem->data.message.oldName	= oldName;
	elem->isVirtual				= false;		// taken from origional identifier... we're only renaming
	elem->documentation			= documentation;
	elems.push_back ( elem );
	return errorNum::ERROR_OK;
}

errorNum opClass::addOp ( cacheString const &name, fgxOvOp op, fgxClassElementType type, fgxClassElementScope scope, opFunction *func, opFile *file, bool isVirtual, symbolTypeClass const &symType, stringi const &documentation )
{
	opClassElement *elem = new opClassElement;

	if ( opOverload[int(op)] )
	{
		delete func;
		delete elem;
		return errorNum::scDUPLICATE_IDENTIFIER;
	}

	elem->type				= type;
	elem->scope				= scope;
	elem->name				= name;
	elem->isVirtual			= isVirtual;
	elem->data.method.func	= func->name;
	elem->location			= func->location;
	elem->documentation		= documentation;
	func->classDef			= this;

	opOverload[int(op)] = elem;

	return file->addElem ( func, symType, true );
}

void opFile::parseProperty (source &src, opClass *classDef, bool isStatic, bool isVirtual, fgxClassElementScope scope, bool isLS, bool isAP,  stringi const &topDocumentation, srcLocation const &formatLocation )
{
	opFunction		*func = nullptr;
	srcLocation		 nameLocation = src;
	stringi			 documentation;

	auto name = sCache.get ( getSymbol ( src ) );
	nameLocation.setEnd ( src );

	if ( !name.size () )
	{
		if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
		errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
	}

	if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::property, nameLocation ) );

	try
	{
		BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

		if ( *src != '{' )
		{
			if ( !isLS ) throw errorNum::scMISSING_OPEN_BRACE;
			errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, src ) );
		} else
		{
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeOpenBlock, src ) );
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, src ) );
			src++;
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterOpenBlock, src ) );
		}
		BS_ADVANCE_EOL ( this, isLS, src );

		while ( *src )
		{
			while ( *src == ';' )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
				src++;
				BS_ADVANCE_EOL ( this, isLS, src );
			}
			errorLocality e ( &errHandler, src );

			srcLocation statementLocation = src;

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

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

				try
				{
					switch ( it->second.type )
					{
						case statementType::stVirtual:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isVirtual )
							{
								if ( !isLS ) throw errorNum::scDEFINITION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, statementLocation ) );
							}
							isVirtual = true;
							break;
						case statementType::stStatic:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							isStatic = true;
							break;
						case statementType::stSet:
							if ( isStatic && isVirtual )
							{
								if ( !isLS ) throw errorNum::scSTATIC_VIRTUAL;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scSTATIC_VIRTUAL, statementLocation ) );
							}
							if ( isStatic )
							{
								func = parseFunc ( src, buildString ( (char *)classDef->name.c_str (), name.c_str (), "assign" ).c_str (), true, isLS, isAP, statementLocation);
								func->isStatic = true;
							} else
							{
								func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), name.c_str (), "assign" ).c_str (), true, isLS, isAP, statementLocation);
							}
							func->nameLocation = statementLocation;
							func->location = statementLocation;
							func->extendedSelection = statementLocation;
							if ( func->codeBlock )
							{
								func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
								func->location.setEnd ( func->codeBlock->location );
							}
							if ( documentation.size () ) func->documentation = documentation.size () ? documentation : topDocumentation;

							if ( !strccmp ( name.c_str (), "default" ) )
							{
								if ( func->params.size () != 3 )
								{
									if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, func->nameLocation ) );
								}
							} else
							{
								if ( func->params.size () != 2 )
								{
									if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, func->nameLocation ) );
								}
							}
							classDef->addProperty ( name, false, scope, isVirtual, func, this, symUnknownType, documentation );
							isVirtual = false;
							isStatic = false;
							break;
						case statementType::stGet:
							if ( isStatic )
							{
								func = parseFunc ( src, buildString ( (char *)classDef->name.c_str (), name.c_str (), "access" ).c_str (), true, isLS, isAP, statementLocation);
								func->isStatic = true;
							} else
							{
								func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), name.c_str (), "access" ).c_str (), true, isLS, isAP, statementLocation);
							}
							func->nameLocation = statementLocation;
							func->location = statementLocation;
							func->extendedSelection = statementLocation;
							if ( func->codeBlock )
							{
								func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
								func->location.setEnd ( func->codeBlock->location );
							}
							if ( documentation.size () ) func->documentation = documentation.size () ? documentation : topDocumentation;

							if ( !strccmp ( name.c_str (), "default" ) )
							{
								if ( func->params.size () != 2 )
								{
									if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, func->nameLocation ) );
								}
							} else
							{
								if ( func->params.size () != 1 )
								{
									if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, func->nameLocation ) );
								}
							}
							classDef->addProperty ( name, true, scope, isVirtual, func, this, symUnknownType, documentation );
							isVirtual = false;
							isStatic = false;
							break;
						case statementType::stProtected:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeScope, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterScope, src ) );
							scope = fgxClassElementScope::fgxClassScope_protected;
							break;

						case statementType::stPublic:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeScope, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterScope, src ) );
							scope = fgxClassElementScope::fgxClassScope_public;
							break;
						case statementType::stPrivate:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeScope, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterScope, src ) );
							scope = fgxClassElementScope::fgxClassScope_private;
							break;
						case statementType::stMessage:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							break;
						case statementType::stCloseBrace:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlock, statementLocation ) );
							statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, statementLocation ) );
							if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatLocation ) );
							// if either a put or get occured (or both) than func will have a value and we'll be well formed
							if ( !func )
							{
								errors.push_back ( std::make_unique<astNode> ( errorNum::scPROPERTY_NO_METHOD, statementLocation ) );
							}
							return;
						default:
							if ( !isLS ) throw errorNum::scNOT_LEGAL_INSIDE_PROPERTY;
							statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_PROPERTY, statementLocation ) );
							src.advanceGarbage();
							errors.back().get()->setEnd( src );
							errHandler.throwFatalError ( isLS, errorNum::scNOT_LEGAL_INSIDE_PROPERTY );
							break;
					}
				} catch ( errorNum err )
				{
					errHandler.throwFatalError ( isLS, err );
					ADVANCE_EOL ( src );
				}
			} else 
			{
				if ( !isLS ) throw errorNum::scNOT_LEGAL_INSIDE_PROPERTY;
				statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
				errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_PROPERTY, statementLocation ) );
				src.advanceGarbage();
				errors.back().get()->setEnd( src );
				errHandler.throwFatalError ( isLS, errorNum::scNOT_LEGAL_INSIDE_PROPERTY );
			}
			documentation = "";
			BS_ADVANCE_EOL ( this, isLS, src );
		}
		if ( !isLS ) throw errorNum::scNOT_LEGAL_INSIDE_PROPERTY;
		statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src ) );
		errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_PROPERTY, src ) );
		src.advanceGarbage();
		errors.back().get()->setEnd( src );
	} catch ( errorNum err )
	{
		errHandler.throwFatalError ( isLS, err );
	}
}

bool opFile::parseInnerClass ( source &src, bool doBraces, opClass *classDef, bool isLS, bool isAP, srcLocation const &formatLocation )
{
	cacheString				 name;
	cacheString				 className;
	cacheString				 oldName;
	fgxClassElementScope	 scope = fgxClassElementScope::fgxClassScope_public;
	bool					 isVirtual = false;
	bool					 isStatic = false;
	bool					 isExtension = false;
	opFunction				*func = nullptr;
	opListDef				*ol = nullptr;
	stringi					 documentation;

	try
	{
		BS_ADVANCE_EOL ( this, isLS, src );
		while ( *src )
		{
			while ( *src == ';' )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
				src++;
				BS_ADVANCE_EOL ( this, isLS, src );
			}
			errorLocality e ( &errHandler, src );
			srcLocation statementLocation = src;

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
				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

				try
				{
					switch ( it->second.type )
					{
						case statementType::stVirtual:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( !doBraces )
							{
								if ( !isLS ) throw errorNum::scNOT_ALLOWED_IN_FGL;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_ALLOWED_IN_FGL, statementLocation ) );
							}
							if ( isVirtual )
							{
								if ( !isLS ) throw errorNum::scDEFINITION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, statementLocation ) );
							}
							isVirtual = true;
							break;
						case statementType::stExtension:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isExtension )
							{
								if ( !isLS ) throw errorNum::scDEFINITION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, statementLocation ) );
							}
							isExtension = true;
							break;
						case statementType::stStatic:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( doBraces )
							{
								if ( isStatic )
								{
									if ( !isLS ) throw errorNum::scDEFINITION;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, statementLocation ) );
								}
								isStatic = true;
							} else
							{
								goto process_local;
							}
							break;
						case statementType::stIterator:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							if ( isStatic && isVirtual ) throw;
							{
								srcLocation nameLocation = src;
								auto tmpName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );

								if ( !tmpName.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
								}
								if ( isStatic || isExtension )
								{
									func = parseFunc ( src, buildString ( (char *)classDef->name.c_str (), tmpName, "iterator" ).c_str (), doBraces, isLS, isAP, statementLocation);
								} else
								{
									func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), tmpName, "iterator" ).c_str (), doBraces, isLS, isAP, statementLocation);
								}
								func->nameLocation = nameLocation;
								func->location = statementLocation;
								func->extendedSelection = statementLocation;
								if ( func->codeBlock )
								{
									func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
									func->location.setEnd ( func->codeBlock->location );
								}
								func->isStatic = isStatic;
								func->isIterator = true;
								if ( documentation.size () ) func->documentation = documentation;
								auto error = classDef->addMethod ( tmpName, fgxClassElementType::fgxClassType_method, scope, isVirtual, isStatic, isExtension, func, this, symUnknownType, true, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( isLS )
									{
										if ( !isLS ) throw error;
										errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
									}
								}
								isVirtual = false;
								isStatic = false;
								isExtension = false;
							}
							break;
						case statementType::stMethod:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							if ( isStatic && isVirtual )
							{
								if ( !isLS ) throw errorNum::scSTATIC_VIRTUAL;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scSTATIC_VIRTUAL, statementLocation ) );
								isStatic = false;
							}
							if ( isStatic && isExtension )
							{
								if ( !isLS ) throw errorNum::scILLEGAL_EXTENSION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXTENSION, statementLocation ) );
								isStatic = false;
							} else
							{
								stringi tmpName;
								srcLocation nameLocation = src;
								do
								{
									nameLocation = src;
									tmpName = (stringi)sCache.get ( getSymbol ( src ) );
									nameLocation.setEnd ( src );

									if ( !tmpName.size () )
									{
										if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
										break;
									}
									if ( tmpName == "VIRTUAL" )
									{
										if ( doBraces || isVirtual )
										{
											if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
											errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, nameLocation ) );
											break;
										}
										isVirtual = true;
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, nameLocation ) );
									} else if ( tmpName == "ASSOCIATIVE" )
									{
										if ( !isLS ) throw errorNum::scUNSUPPORTED;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scUNSUPPORTED, nameLocation ) );
									}
									BS_ADVANCE_COMMENT ( this, isLS, src );
									if ( !doBraces && _iseol ( src ) )
									{
										break;
									}
									BS_ADVANCE_COMMENT ( this, isLS, src );
								} while ( *src != '(' );

								if ( isStatic || isExtension )
								{
									// for language server in FGL mode, insert implied blocks for formatting
									// the difference is that parse method adds a self parameter (and handling for new)... we don't want to do that here
									func = parseFunc ( src, buildString ( (char *)classDef->name.c_str (), sCache.get ( tmpName ), "method" ).c_str (), doBraces, isLS, isAP, statementLocation);
								} else
								{
									func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), sCache.get ( tmpName ), "method" ).c_str (), doBraces, isLS, isAP, statementLocation);
								}
								func->isStatic = isStatic;
								func->nameLocation = nameLocation;
								func->location = statementLocation;
								func->extendedSelection = statementLocation;
								if ( func->codeBlock )
								{
									func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
									func->location.setEnd ( func->codeBlock->location );
								}
								if ( documentation.size() ) func->documentation = documentation;
								if ( !strccmp ( tmpName, "default" ) )
								{
									if ( func->params.size () < 2 )
									{
										delete func;
										if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, nameLocation ) );
										break;
									}
								}
								if ( !strccmp ( tmpName, "release" ) )
								{
									isVirtual = true;
								}
								auto error = classDef->addMethod ( sCache.get ( tmpName ), fgxClassElementType::fgxClassType_method, scope, isVirtual, isStatic, isExtension, func, this, symUnknownType, true, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
								}
								isVirtual = false;
								isStatic = false;
								isExtension = false;
							}
							break;
						case statementType::stInteger:
						case statementType::stDouble:
						case statementType::stString:
						case statementType::stArray:
						case statementType::stCodeblock:
						case statementType::stVariant:
						case statementType::stObject:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isStatic && isVirtual )
							{
								if ( !isLS ) throw errorNum::scSTATIC_VIRTUAL;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scSTATIC_VIRTUAL, statementLocation ) );
								isStatic = false;
							} else
							{
								if ( !cmpTerminated ( src, "METHOD", 6 ) )
								{
									if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, statementLocation ) );
								}
								src += 6;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

								srcLocation nameLocation = src;
								auto tmpName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( !tmpName.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
								}
								if ( !strccmp ( tmpName.c_str (), "new" ) )
								{
									if ( !isLS ) throw errorNum::scNEW_RETURN_TYPE_NOT_ALLOWED;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scNEW_RETURN_TYPE_NOT_ALLOWED, nameLocation ) );
								}

								if ( isStatic )
								{
									func = parseFunc ( src, buildString ( (char *)classDef->name.c_str (), tmpName, "method" ).c_str (), doBraces, isLS, isAP, statementLocation);
								} else
								{
									func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), tmpName, "method" ).c_str (), doBraces, isLS, isAP, statementLocation);
								}

								func->isStatic = isStatic;
								func->nameLocation = nameLocation;
								func->location = statementLocation;
								func->extendedSelection = statementLocation;
								if ( func->codeBlock )
								{
									func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
									func->location.setEnd ( func->codeBlock->location );
								}
								if ( documentation.size () ) func->documentation = documentation;

								if ( !strccmp ( tmpName, "default" ) )
								{
									if ( func->params.size () < 2 )
									{
										delete func;
										if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, nameLocation ) );
										break;
									}
								}

								symbolTypeClass type = symUnknownType;
								switch ( it->second.type )
								{
									case statementType::stString:
										type = symStringType;
										break;
									case statementType::stArray:
										type = symArrayType;
										break;
									case statementType::stInteger:
										type = symIntType;
										break;
									case statementType::stDouble:
										type = symDoubleType;
										break;
									case statementType::stObject:
										type = symObjectType;
										break;
									case statementType::stLocal:
									case statementType::stVariant:
										break;
									default:
										delete func;
										throw errorNum::scINTERNAL;
								}
								auto error = classDef->addMethod ( tmpName, fgxClassElementType::fgxClassType_method, scope, isVirtual, isStatic, isExtension, func, this, type, true, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
								}
								isVirtual = false;
								isStatic = false;
								isExtension = false;
							}
							break;
						case statementType::stProperty:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							if ( !doBraces )
							{
								if ( !isLS ) throw errorNum::scNOT_ALLOWED_IN_FGL;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scSTATIC_VIRTUAL, statementLocation ) );
							}
							if ( isExtension )
							{
								if ( !isLS ) throw errorNum::scILLEGAL_EXTENSION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXTENSION, statementLocation ) );
								isExtension = false;
							}
							if ( isStatic )
							{
								if ( !isLS ) throw errorNum::scILLEGAL_STATIC_PROP;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_STATIC_PROP, statementLocation ) );
								isStatic = false;
							}
							parseProperty ( src, classDef, isStatic, isVirtual, scope, isLS, isAP, documentation, statementLocation );
							BS_ADVANCE_EOL ( this, isLS, src );
							break;
						case statementType::stAssign:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							if ( isExtension )
							{
								if ( !isLS ) throw errorNum::scILLEGAL_EXTENSION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXTENSION, statementLocation ) );
								isExtension = false;
							}
							if ( isStatic )
							{
								if ( !isLS ) throw errorNum::scILLEGAL_STATIC_PROP;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_STATIC_PROP, statementLocation ) );
								isStatic = false;
							} else
							{
								srcLocation nameLocation = src;
								auto tmpName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );

								if ( !tmpName.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
								}
								if ( isStatic )
								{
									func = parseFunc ( src, buildString ( (char *)classDef->name.c_str (), tmpName, "assign" ).c_str (), doBraces, isLS, isAP, statementLocation);
									func->isStatic = true;
								} else
								{
									func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), tmpName, "assign" ).c_str (), doBraces, isLS, isAP, statementLocation);
								}

								func->nameLocation = nameLocation;
								func->location = statementLocation;
								func->extendedSelection = statementLocation;
								if ( func->codeBlock )
								{
									func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
									func->location.setEnd ( func->codeBlock->location );
								}
								if ( documentation.size () ) func->documentation = documentation;
								if ( !strccmp ( tmpName, "default" ) )
								{
									if ( func->params.size () != 3 )
									{
										delete func;
										if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, nameLocation ) );
										break;
									}
								} else
								{
									if ( func->params.size () != 2 )
									{
										delete func;
										if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, nameLocation ) );
										break;
									}
								}
								auto error = classDef->addProperty ( tmpName, false, scope, isVirtual, func, this, symUnknownType, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
								}
								isVirtual = false;
								isStatic = false;
							}
							break;
						case statementType::stAccess:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							if ( isExtension )
							{
								if ( !isLS ) throw errorNum::scILLEGAL_EXTENSION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXTENSION, statementLocation ) );
								isExtension = false;
							}
							if ( isStatic )
							{
								if ( !isLS ) throw errorNum::scILLEGAL_STATIC_PROP;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_STATIC_PROP, statementLocation ) );
								isStatic = false;
							} else

							{
								srcLocation nameLocation = src;
								auto tmpName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );

								if ( !tmpName.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
								}
								if ( isStatic )
								{
									func = parseFunc ( src, buildString ( (char *)classDef->name.c_str (), tmpName, "access" ).c_str (), doBraces, isLS, isAP, statementLocation);
									func->isStatic = true;
								} else
								{
									func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), tmpName, "access" ).c_str (), doBraces, isLS, isAP, statementLocation);
								}

								func->nameLocation = nameLocation;
								func->location = statementLocation;
								func->extendedSelection = statementLocation;
								if ( func->codeBlock )
								{
									func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
									func->location.setEnd ( func->codeBlock->location );
								}
								if ( !strccmp ( tmpName, "default" ) )
								{
									if ( func->params.size () != 2 )
									{
										delete func;
										if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, nameLocation ) );
										break;
									}
								} else
								{
									if ( func->params.size () != 1 )
									{
										delete func;
										if ( !isLS ) throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scINCORRECT_NUMBER_OF_PARAMETERS, nameLocation ) );
										break;
									}
								}
								auto error = classDef->addProperty ( tmpName, true, scope, isVirtual, func, this, symUnknownType, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
								}
								isVirtual = false;
								isStatic = false;
							}
							break;
						case statementType::stLocal:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
process_local:
							if ( isVirtual )
							{
								if ( !isLS ) throw errorNum::scDEFINITION;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, statementLocation ) );
								isExtension = false;
							}

							while ( 1 )
							{
								errorNum error;

								srcLocation exprLocation = src;

								auto expr = parseExpr ( src, false, false, 0, doBraces, isLS, isAP );

								exprLocation.setEnd ( src );

								BS_ADVANCE ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									documentation = doc2;
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement2 ) );
									BS_ADVANCE ( src );
								}

								if ( expr )
								{
									if ( isStatic )
									{
										error = classDef->addIVar ( expr, fgxClassElementType::fgxClassType_static, scope, documentation );
									} else
									{
										error = classDef->addIVar ( expr, fgxClassElementType::fgxClassType_iVar, scope, documentation );
									}
								} else
								{
									error = errorNum::scBAD_VARIABLE_DEF;
								}
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, exprLocation ) );
								}

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
								} else
								{
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
										break;
									}
								}
							}
							isVirtual = false;
							isStatic = false;
							break;
						case statementType::stOperator:
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFunction, statementLocation ) );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
								if ( isStatic ) throw errorNum::scDEFINITION;

								srcLocation nameLocation = src;
								auto tmpName = sCache.get ( parseOperator ( src ) );
								nameLocation.setEnd ( src );

								func = parseMethod ( src, classDef, buildString ( (char *)classDef->name.c_str (), tmpName, "operator" ).c_str (), doBraces, isLS, isAP, statementLocation);

								func->nameLocation = nameLocation;
								func->location = statementLocation;
								func->extendedSelection = statementLocation;
								if ( func->codeBlock )
								{
									func->extendedSelection.setEnd ( func->codeBlock->extendedSelection );
									func->location.setEnd ( func->codeBlock->location );
								}

								if ( !(ol = getOperator ( tmpName, (uint32_t)func->params.size () - 1 )) )
								{
									delete func;
									if ( !isLS ) throw errorNum::scNO_SUCH_OPERATOR_SIGNATURE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scNO_SUCH_OPERATOR_SIGNATURE, nameLocation ) );
									break;
								}
								func->name = sCache.get ( buildString ( (char *)classDef->name.c_str (), tmpName, ol->opCat == astOpCat::opBinary ? "binary" : "unary" ) );

								auto error = classDef->addOp ( sCache.get ( buildString ( "", tmpName, ol->opCat == astOpCat::opBinary ? "binary" : "unary" ) ), ol->overloadOp, fgxClassElementType::fgxClassType_method, scope, func, this, isVirtual, symVariantType, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
								}
								isVirtual = false;
								isStatic = false;
							}
							break;
						case statementType::stInherit:
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
								if ( isStatic )
								{
									if ( !isLS ) throw errorNum::scNOT_ALLOWED_IN_FGL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, statementLocation ) );
								}

								while ( 1 )
								{
									srcLocation symStart = src;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

									if ( cmpTerminated ( src, "public", 6 ) )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 6 ) );
										src += 6;
										scope = fgxClassElementScope::fgxClassScope_public;
										if ( doBraces )
										{
											if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
											errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, symStart ) );
											errors.back ().get ()->setEnd ( src );
										}

									} else if ( cmpTerminated ( src, "protected", 9 ) )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 9 ) );
										src += 9;
										scope = fgxClassElementScope::fgxClassScope_public;
										if ( doBraces )
										{
											if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
											errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, symStart ) );
											errors.back ().get ()->setEnd ( src );
										}
									} else if ( cmpTerminated ( src, "private", 7 ) )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 7 ) );
										src += 7;
										scope = fgxClassElementScope::fgxClassScope_public;
										if ( doBraces )
										{
											if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
											errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, symStart ) );
											errors.back ().get ()->setEnd ( src );
										}
									} else if ( cmpTerminated ( src, "virtual", 7 ) )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 7 ) );
										src += 7;
										isVirtual = true;
										if ( doBraces )
										{
											if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
											errors.push_back ( std::make_unique<astNode> ( errorNum::scDEFINITION, symStart ) );
											errors.back ().get ()->setEnd ( src );
										}
									} else
									{
										break;
									}
								}
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								srcLocation nameLocation = src;
								auto tmpName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::cls, nameLocation ) );

								if ( !tmpName.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
								}
								auto error = classDef->add ( tmpName, fgxClassElementType::fgxClassType_inherit, scope, nameLocation, isVirtual, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
								}
								isVirtual = false;
								isStatic = false;
							}
							break;
						case statementType::stConst:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );

							if ( isStatic || isVirtual ) throw errorNum::scDEFINITION;

							while ( 1 )
							{
								srcLocation exprLocation = src;

								auto expr = parseExpr ( src, false, false, 0, doBraces, isLS, isAP );
								exprLocation.setEnd ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									documentation = doc2;
									delete commentStatement2;
								}

								auto error = classDef->addConst ( expr , fgxClassElementType::fgxClassType_const, scope, documentation );

								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, exprLocation ) );
								}

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
								} else
								{
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
									}
								}
							}
							break;
						case statementType::stProtected:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeScope, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterScope, src ) );
							scope = fgxClassElementScope::fgxClassScope_protected;
							break;
						case statementType::stPublic:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeScope, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterScope, src ) );
							scope = fgxClassElementScope::fgxClassScope_public;
							break;
						case statementType::stPrivate:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeScope, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterScope, src ) );
							scope = fgxClassElementScope::fgxClassScope_private;
							break;
						case statementType::stMessage:
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
								if ( isStatic && isVirtual )
								{
									if ( !isLS ) throw errorNum::scSTATIC_VIRTUAL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scSTATIC_VIRTUAL, statementLocation ) );
								}
								srcLocation nameLocation = src;
								auto tmpName = sCache.get ( getSymbol ( src ) );
								nameLocation.setEnd ( src );
								if ( !tmpName.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, nameLocation ) );
								}
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::ivar, nameLocation ) );
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								if ( !strccmp ( src, "is" ) )
								{
									if ( !isLS ) throw errorNum::scMISSING_IS_IN_MESSAGE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_IS_IN_MESSAGE, nameLocation ) );
								}
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 2 ) );
								src += 2;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								srcLocation oldLocation = src; 
								auto oldName = sCache.get ( getSymbol ( src ) );
								oldLocation.setEnd ( src );
								if ( !oldName.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, oldLocation ) );
								}
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::ivar, oldLocation ) );
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								if ( !strccmp ( src, "in" ) )
								{
									if ( !isLS ) throw errorNum::scMISSING_IN_IN_MESSAGE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_IN_IN_MESSAGE, nameLocation ) );
								}
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 2 ) );
								src += 2;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src ) );
								auto className = sCache.get ( getSymbol ( src ) );
								if ( !className.size () )
								{
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_SYMBOL, src ) );
								}
								if ( isLS ) statements.back ().get ()->setEnd ( src );
								BS_ADVANCE ( src );
								if ( *src == ';' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
									src++;
									break;
								} else if ( _iseol ( src ) )
								{
									// good, we terminated at eol
								} else
								{
									if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
								}
								if ( tmpName == "new" || tmpName == "$new" || tmpName == "_pack" || tmpName == "_unpack" || tmpName == "default" )
								{
									if ( !isLS ) throw errorNum::scMESSAGE_REANAME_ILLEGAL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMESSAGE_REANAME_ILLEGAL, nameLocation ) );
								}
								if ( oldName == "new" || oldName == "$new" || oldName == "_pack" || oldName == "_unpack" || oldName == "default" )
								{
									if ( !isLS ) throw errorNum::scMESSAGE_REANAME_ILLEGAL;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scMESSAGE_REANAME_ILLEGAL, nameLocation ) );
								}
								auto error = classDef->addMessage ( tmpName, sCache.get ( (stringi)className + "::" + (stringi)oldName ), nameLocation, documentation );
								if ( error != errorNum::ERROR_OK )
								{
									if ( !isLS ) throw error;
									errors.push_back ( std::make_unique<astNode> ( error, nameLocation ) );
								}
							}
							break;
						case statementType::stEnd:
							// we don't have to worry about ensuring that the class has a constructor here... it will be generated later in an additional step
							// that also ensures that all C classes have constructors (which makes things easier on everyone)
							if ( doBraces )
							{
								if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_ALLOWED_IN_FGL, statementLocation ) );
							}
							if ( isLS )
							{
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeEnd, statementLocation ) );
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
								src--;
								autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatLocation) );
								src++;
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterEnd, src ) );
							}
							classDef->location.setEnd ( src );
							return true;
						case statementType::stCloseBrace:
							if ( !doBraces )
							{
								if ( !isLS ) throw errorNum::scNOT_ALLOWED_IN_FGL;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_ALLOWED_IN_FGL, statementLocation ) );
							}
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlock, statementLocation ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, statementLocation ) );
							// we don't have to worry about ensuring that the class has a constructor here... it will be generated later in an additional step
							// that also ensures that all C classes have constructors (which makes things easier on everyone)
							classDef->location.setEnd ( statementLocation );
							return true;
						case statementType::stSet:
						case statementType::stGet:
							if ( !isLS ) throw errorNum::scNOT_LEGAL_OUTSIDE_PROPERTY;
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_OUTSIDE_PROPERTY, statementLocation ) );
							break;
						default:
							if ( !isLS ) throw errorNum::scNOT_LEGAL_INSIDE_CLASS;
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, statementLocation ) );
							errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_CLASS, statementLocation ) );
							break;
					}
				} catch ( errorNum err )
				{
					assert ( !isLS );
					errHandler.throwFatalError ( isLS, err );
					errors.push_back ( std::make_unique<astNode> ( err, statementLocation ) );
					ADVANCE_EOL ( src );
				}
			}
			if ( it == statementMap.end () )
			{
				if ( !isLS ) throw errorNum::scNOT_LEGAL_INSIDE_CLASS;
				errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_CLASS, src ) );
				src.advanceGarbage ();
				errors.back ().get ()->setEnd ( src );
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, *errors.back().get() ) );
			}
			documentation = "";
			BS_ADVANCE_EOL ( this, isLS, src );
		}
	} catch ( errorNum err )
	{
		assert ( !isLS );
		errHandler.throwFatalError ( isLS, err );
	}
	return false;
}

opClass *opFile::parseClass ( source &src, bool doBraces, bool isLS, bool isAP, srcLocation const &formatLocation )
{
	opClass		*classDef;

	classDef	= new opClass;
	classDef->location.set ( src );

	classDef->usingList		= ns.getUseList();

	BS_ADVANCE_EOL( this, isLS, src );

	classDef->nameLocation = src;
	classDef->name = sCache.get ( getSymbol ( src ) );
	classDef->nameLocation.setEnd ( src );

	if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::cls, classDef->nameLocation ) );

	if ( !classDef->name.length ( ) )
	{
		if ( !isLS ) throw errorNum::scUNKNOWN_FUNCTION;
		errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_CLASS, classDef->nameLocation ) );
	}

	if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterClass, src ) );

	BS_ADVANCE_EOL ( this, isLS, src );

	if ( doBraces )
	{
		if ( *src != '{' )
		{
			if ( !isLS ) throw errorNum::scMISSING_OPEN_BRACE;
			errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_CLASS, src ) );
		} else
		{
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeOpenBlock, src ) );
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, src ) );
			src++;
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterOpenBlock, src ) );
		}
		BS_ADVANCE_EOL ( this, isLS, src );
	} else
	{
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeImpliedBlock, src ) );
	}

	if ( !parseInnerClass ( src, doBraces, classDef, isLS, isAP, formatLocation ) )
	{
		if ( !isLS ) throw errorNum::scNOT_LEGAL_INSIDE_CLASS;
		errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_LEGAL_INSIDE_CLASS, src ) );
	} 
	return classDef;
}
