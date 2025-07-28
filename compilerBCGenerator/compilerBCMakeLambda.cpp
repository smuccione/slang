/*

	converts a function into a lambda object... 

	the object is of the form


	class functor
	{
		local test

		method new ( test ) : test ( test )
		{
		}
		operator () ( ... )
		{
		}
	}

	were the intitializer is passed references to all the closures.  the class subsequently captures the closures

	self, if necessary, becomes an instance variable _self and all access to class members are converted to _self.<member>


*/

#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "Utility/counter.h"

static astNode *compMakeLambdaCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, opFunction *func )
{
	switch ( node->getOp () )
	{
		case astOp::funcValue:
			assert ( false );
			throw errorNum::scINTERNAL;

		case astOp::symbolValue:
			if ( memcmp ( node->symbolValue(), "::", 2 ) != 0 )
			{
				if ( sym->isDefined ( node->symbolValue(), true ) )
				{
					switch ( sym->getScope ( node->symbolValue(), isAccess ) )
					{
						case symbolSpaceScope::symLocal:
							break;
						case symbolSpaceScope::symClassConst:
						case symbolSpaceScope::symClassStatic:
							assert ( func->classDef );
							node->symbolValue () = sym->file->sCache.get ( (stringi)func->classDef->name + "::" + (stringi) node->symbolValue () );
							node->symbolValue() = sym->getResolvedName ( node->symbolValue (), true );
							break;
						case symbolSpaceScope::symClassAssign:
						case symbolSpaceScope::symClassAccess:
						case symbolSpaceScope::symClassIVar:
						case symbolSpaceScope::symClassInherit:
							assert ( func->classDef );
							node->left = (new astNode ( sym->file->sCache, astOp::symbolValue, sym->file->_selfValue ))->setLocation ( node );
							node->right = (new astNode ( sym->file->sCache, astOp::nameValue, node->symbolValue() ))->setLocation ( node );
							node->setOp ( sym->file->sCache, astOp::sendMsg );
							break;
						case symbolSpaceScope::symClassMethod:
							if ( sym->isStatic ( node->symbolValue(), true ) || sym->isExtension ( node->symbolValue(), true ) )
							{
								assert ( func->classDef );
								node->symbolValue () = sym->file->sCache.get ( (stringi)func->classDef->name + "::" + (stringi)node->symbolValue () );
								node->symbolValue () = sym->getResolvedName ( node->symbolValue (), true );
							} else
							{
								node->left = (new astNode ( sym->file->sCache, astOp::symbolValue, sym->file->_selfValue ) )->setLocation ( node );
								node->right = (new astNode ( sym->file->sCache, astOp::nameValue, node->symbolValue() ))->setLocation ( node );
								node->setOp ( sym->file->sCache, astOp::sendMsg );
							}
							break;
						default:
							break;
					}
				}
			}
			break;
		case astOp::funcCall:
			if ( node->left->getOp () == astOp::atomValue )
			{
				if ( sym->isDefined ( node->left->symbolValue(), true ) )
				{
					switch ( sym->getScope ( node->left->symbolValue(), isAccess ) )
					{
						case symbolSpaceScope::symClassMethod:
							{
								node->left->left = ((new astNode ( sym->file->sCache, astOp::nameValue, sym->file->_selfValue )))->setLocation ( node );
								node->left->right = ((new astNode ( sym->file->sCache, astOp::symbolValue, node->left->symbolValue() )))->setLocation ( node );
								node->left->setOp ( sym->file->sCache, astOp::sendMsg );
							}
							break;
						default:
							break;
					}
				}
			}
			break;
		default:
			break;
	}
	return node;
}

cacheString compExecutable::compMakeLambda ( opFile *file, opFunction *func, std::vector<symbolLocal> &closures, bool isStatic, bool isExtension, bool isLS )
{
	char const		*expr;
	opFunction		*newFunc;
	opClassElement	*elem;
	bool			 needNew = (func->classDef && !isStatic && !isExtension) || closures.size ();
	
	opClass	*itClass = new opClass ();

	itClass->isInterface	= false;
	itClass->usingList		= file->ns.getUseList();
	itClass->name			= file->sCache.get ( (stringi)func->name + "." + GetUniqueCounter ( ) );
	itClass->location		= func->location;
	itClass->nameLocation	= func->nameLocation;

	symbolStack sym ( file, func );

#if 0
	for ( size_t loop = 0; loop < itClass->name.length(); loop++ )
	{
		if ( itClass->name[loop] == '.' )
		{
			itClass->name[loop] = '$';
		}
	}
#endif

	if ( func->classDef && !isStatic && !isExtension)
	{
		auto opElem = new opClassElement ();
		opElem->isVirtual				= false;
		opElem->isStatic				= false;
		opElem->type					= fgxClassElementType::fgxClassType_iVar;
		opElem->scope					= fgxClassElementScope::fgxClassScope_public;
		opElem->location				= isLS ? srcLocation () : func->location;
		opElem->symType					= symbolTypeClass ( symbolType::symObject, func->classDef->name );
		opElem->name					= file->_selfValue;
		opElem->data.iVar.initializer	= new astNode ( file->sCache, astOp::symbolValue, opElem->name );
		if ( !isLS )
		{
			opElem->data.iVar.initializer->setLocation ( func->location );
		}
		itClass->elems.push_back ( opElem );
	}

	for ( auto &it : closures )
	{
		auto opElem = new opClassElement ();
		opElem->isVirtual				= false;
		opElem->isStatic				= false;
		opElem->type					= fgxClassElementType::fgxClassType_iVar;
		opElem->scope					= fgxClassElementScope::fgxClassScope_public;
		opElem->location				= isLS ? srcLocation () : func->location;
		opElem->name					= it.getSymbolName ();
		opElem->data.iVar.initializer	= new astNode ( file->sCache, astOp::symbolValue, opElem->name );
		if ( !isLS )
		{
			opElem->data.iVar.initializer->setLocation ( func->location );
		}
		itClass->elems.push_back ( opElem );
	}

	std::vector<symbol *> cleanupSymbols;

	astNodeWalk ( func->codeBlock, &sym, compMakeLambdaCB, func );

	/**************************   new method   ***********************/
	// the only time we ever need to emit a new method is if we're doing a capture... including an automatic this
	if ( needNew )
	{
		elem = new opClassElement ();
		elem->isVirtual = false;
		elem->isStatic = false;
		elem->type = fgxClassElementType::fgxClassType_method;
		elem->scope = fgxClassElementScope::fgxClassScope_public;
		elem->location = srcLocation (); // func->location;
		elem->name = file->newValue;

		BUFFER buff;
		buff += "( ";
		bool first = true;
		if ( func->classDef && !isStatic && !isExtension )
		{
			buff += "this";
			first = false;
		}
		for ( auto &it : closures )
		{
			if ( !first )
			{
				buff += ", ";
			}
			first = false;
			buff += it.getSymbolName ().c_str ();
		}

		buff += ") : ";
		first = true;
		if ( func->classDef && !isStatic && !isExtension )
		{
			buff += "_self ( this )";
			first = false;
		}
		for ( auto &it : closures )
		{
			if ( !first )
			{
				buff += ", ";
			}
			first = false;
			buff += it.getSymbolName ().c_str ();
//			buff += " ( @";
			buff += " ( ";
			buff += it.getSymbolName ().c_str ();
			buff += " ) ";
		}

		buff += "{}\r\n";
		buff.put ( (char) 0 );
		expr = buff.data<char *> ();
		{
//			source src ( &file->srcFiles, file->sCache, file->srcFiles.getName ( func->location.sourceIndex ), expr, func->location.lineNumberStart );
			source src ( &file->srcFiles, file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none, 1 );
			newFunc = file->parseMethod ( src, itClass, buildString ( itClass->name.c_str (), elem->name, "method" ).c_str (), true, false, false, srcLocation() );
		}
		elem->data.method.func = newFunc->name;
		elem->data.method.virtOverrides.insert ( newFunc );
		newFunc->isLambda = true;
		newFunc->location = func->location;	// MUST be correct and have the proper sourceIndex or we won't remove it upon edit of other LS entities.
		file->functionList.insert( { newFunc->name, newFunc }  );
		newFunc->isInterface = false;
		newFunc->classDef = itClass;
		newFunc->lsIgnore = true;
		itClass->elems.push_back ( elem );
	}

	/**************************   operator () ***********************/
	elem = new opClassElement ();
	elem->isVirtual		= false;
	elem->isStatic		= false;
	elem->type			= fgxClassElementType::fgxClassType_method;
	elem->scope			= fgxClassElementScope::fgxClassScope_public;
	elem->location		= func->location;		// we really do want this location even in LS mode so we can jump to it properly
	elem->name			= file->sCache.get ( "()" );

	// in this case, the unnamed function simply becomes the lambda functor (it has been modified to remap captures to the new object)
	auto it = file->functionList.find ( func->name );
	file->functionList.erase ( it );

	func->params.add ( file->selfValue, symbolTypeClass ( symbolType::symObject, itClass->name ), func->location, (new astNode ( file->sCache, astOp::symbolValue, file->selfValue ))->setLocation ( func->nameLocation ), true, stringi() );
//	func->params.add ( file->selfValue, symbolTypeClass ( symbolType::symObject, itClass->name ), func->location, (new astNode ( file->sCache, astOp::symbolValue, file->selfValue )), true, stringi () );

	func->name = file->sCache.get ( buildString ( itClass->name.c_str (), elem->name, "operator" ) );
	elem->data.method.func = func->name;
	elem->data.method.virtOverrides.insert ( func );
	func->classDef = itClass;
	func->isLambda = true;
	file->functionList.insert( { func->name, func } );
	itClass->opOverload[int(fgxOvOp::ovFuncCall)] = elem;
	file->classList.insert( { itClass->name, itClass } );

	makeClass ( itClass, isLS );
	makeConstructorDestructors ( itClass, func->location.sourceIndex );

	// the below have already been run on all other functions.   we need to do it on the newly created ones.
	// we can't simply move this until after iterator creation on the other functions as we have to have done this already in order to 
	// properly make the iterators in the first place.   it has not choice but to be multipass.

	if ( needNew )
	{
		makeSymbols ( itClass->cClass->getElement ( file->newValue )->methodAccess.func,isLS );
		makeAtoms ( itClass->cClass->getElement ( file->newValue )->methodAccess.func, isLS );
		pruneRefs ( itClass->cClass->getElement ( file->newValue )->methodAccess.func );
	}

	return itClass->name;
}

