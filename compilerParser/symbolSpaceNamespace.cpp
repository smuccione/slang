/*

	Symbol space

*/

#include "fileParser.h"
#include "symbolSpace.h"
#include "symbolSpace.h"
#include "symbolSpaceNamespace.h"

class compClass	*symbolSpaceNamespace::findClass ( cacheString const &name ) const
{
	opClass *cls;

	if ( (cls = ns->findClass ( name )) )
	{
		return cls->cClass;
	}
	return 0;
}

cacheString const symbolSpaceNamespace::getFuncName ( cacheString const &name, bool isAccess ) const
{
	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					break;
			}
		}
	}

	if ( func ) return func->name;
	return cacheString();
}

void symbolSpaceNamespace::setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc )
{
	auto *func = ns->findFunction( name );
	if ( func )
	{
		func->setAccessed ( acc, scanQueue, loc );
		return;
	}

	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement( name, isAccess )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				for ( auto &it : elem->elem->data.method.virtOverrides )
				{
					it->setAccessed ( acc, scanQueue, loc );
				}
				return;
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
					{
						it->setAccessed ( acc, scanQueue, loc );
					}
				} else
				{
					for ( auto &it : elem->elem->data.prop.assignVirtOverrides )
					{
						it->setAccessed ( acc, scanQueue, loc );
					}
				}
				return;
			default:
				break;
		}
	}

	auto sym = ns->findSymbol ( name );
	if ( ns && sym )
	{
		sym->setAccessed ( name, isAccess, acc, scanQueue, loc );
	}
}

uint32_t symbolSpaceNamespace::getFuncNumParams ( cacheString const &name, bool isAccess ) const
{
	size_t numParams = 0;
	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					for ( auto &it : elem->elem->data.method.virtOverrides )
					{
						if ( it->params.size () > numParams )
						{
							numParams = it->params.size ();
						}
					}
					return (uint32_t) numParams;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
						{
							if ( it->params.size () > numParams )
							{
								numParams = it->params.size ();
							}
						}
						return (uint32_t)numParams;
					} else
					{
						for ( auto &it : elem->elem->data.prop.assignVirtOverrides )
						{
							if ( it->params.size () > numParams )
							{
								numParams = it->params.size ();
							}
						}
						return (uint32_t)numParams;
					}
					break;
				default:
					break;
			}
		}
		return 0;
	} else
	{
		return (uint32_t) func->params.size ();
	}
}

symbolTypeClass const symbolSpaceNamespace::getFuncReturnType ( class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) const
{
	auto retType = symUnknownType;

	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					for ( auto &it : elem->elem->data.method.virtOverrides )
					{
						it->setAccessed ( acc, scanQueue, loc );
						retType &= it->getReturnType();
					}
					return retType;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
						{
							it->setAccessed ( acc, scanQueue, loc );
							retType &= it->getReturnType();
						}
					} else
					{
						for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
						{
							it->setAccessed ( acc, scanQueue, loc );
							retType &= it->getReturnType();
						}
					}
					return retType;
				default:
					break;
			}
		}
		return symVariantType;
	} else
	{
		func->setAccessed ( acc, scanQueue, loc );
		return func->getReturnType();
	}
}

symbolTypeClass const symbolSpaceNamespace::getMarkFuncReturnType ( class compExecutable *comp, class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS, srcLocation const &loc ) const
{
	auto retType = symUnknownType;

	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					for ( auto &it : elem->elem->data.method.virtOverrides )
					{
						it->setAccessed ( acc, scanQueue, loc );
						retType &= it->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc );
					}
					return retType;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
						{
							it->setAccessed ( acc, scanQueue, loc );
							retType &= it->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc );
						}
					} else
					{
						for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
						{
							it->setAccessed ( acc, scanQueue, loc );
							retType &= it->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc );
						}
					}
					return retType;
				default:
					break;
			}
		}
		return symVariantType;
	} else
	{
		func->setAccessed ( acc, scanQueue, loc );
		return func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc );
	}
}

size_t symbolSpaceNamespace::getFuncSourceIndex ( cacheString const &name, bool isAccess ) const
{
	auto *func = ns->findFunction ( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					break;
			}
		}
	}

	if ( func ) return func->location.sourceIndex;

	return 0;
}

astNode *symbolSpaceNamespace::getFuncDefaultParam ( cacheString const &name, bool isAccess, int32_t nParam ) const
{
	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
		}
	}

	if ( func )
	{
		if ( nParam < (int32_t)func->params.size() )
		{
			return func->params[nParam]->initializer;
		}
	}
	return 0;
}
symbolTypeClass const symbolSpaceNamespace::getFuncParamType ( cacheString const &name, bool isAccess, int32_t nParam, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const
{
	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					break;
			}
		}
	}

	if ( func )
	{
		if ( nParam < (int32_t)func->params.size() )
		{
			func->params[nParam]->setAccessed ( acc, scanQueue, srcLocation {} );
			return func->params[nParam]->getType ();
		}
	}
	return symVariantType;
}
bool symbolSpaceNamespace::isFunc ( cacheString const &name, bool isAccess ) const
{
	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					break;
			}
		}
	}
	if ( func ) return true;
	return false;
}

bool symbolSpaceNamespace::isClass ( cacheString const &name, bool isAccess ) const
{
	return ns->findClass ( name ) ? true : false;
}

fgxFuncCallConvention symbolSpaceNamespace::getFuncCallingConv	( cacheString const &name, bool isAccess ) const
{
	auto *func = ns->findFunction( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					break;
			}
		}
	}

	if ( func ) return func->conv;
	return fgxFuncCallConvention::opFuncType_Bytecode;
}

int32_t symbolSpaceNamespace::getIndex ( cacheString const &name, bool isAccess ) const
{
	if ( ns->findSymbol ( name ) )
	{
		assert ( !ns->findSymbol ( name )->accessors.empty () );
		return (int32_t)ns->findSymbol ( name )->globalIndex;
	}
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		return (int32_t)ns->findSymbol ( elem->symbolName )->globalIndex;
	}
	throw errorNum::scINTERNAL;
}

bool symbolSpaceNamespace::isDefined ( cacheString const &name, bool isAccess ) const
{
	if ( ns )
	{
		return ns->isDefined ( name, isAccess );
	}
	return false;
}

bool symbolSpaceNamespace::isDefinedNS ( cacheString const &name, bool isAccess ) const
{
	if ( ns )
	{
		return ns->isDefinedNS ( name, isAccess, *useList );
	}
	return false;
}

cacheString const symbolSpaceNamespace::getResolvedName ( cacheString const &simpleName, bool isAccess ) const
{
	return ns->getResolvedName ( simpleName, isAccess, *useList );
}

astNode *symbolSpaceNamespace::getInitializer ( cacheString const &name ) const noexcept
{
	if ( auto elem = ns->findClassElement ( name, true ) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_static:
			case fgxClassElementType::fgxClassType_const:
				{
					auto it = file->symbols.find ( elem->symbolName );
					if ( it != file->symbols.end () )
					{
						return it->second.initializer;
					}
					return nullptr;
				}
			default:
				break;
		}
	}
	if ( auto sym = ns->findSymbol ( name ) )
	{
		return sym->initializer;
	}
	return nullptr;
}
symbolSpaceScope symbolSpaceNamespace::getScope ( cacheString const &name, bool isAccess ) const
{
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				return symbolSpaceScope::symClassMethod;
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func ) return symbolSpaceScope::symClassAssign;
				} else
				{
					if ( elem->assign.func ) return symbolSpaceScope::symClassAccess;
				}
				break;

			case fgxClassElementType::fgxClassType_iVar:
				return symbolSpaceScope::symClassIVar;
			case fgxClassElementType::fgxClassType_static:
				return symbolSpaceScope::symClassStatic;
			case fgxClassElementType::fgxClassType_const:
				return symbolSpaceScope::symClassConst;
			case fgxClassElementType::fgxClassType_inherit:
				return symbolSpaceScope::symClassInherit;
			default:
				break;
		}
	}
	if ( ns->findClass ( name ) )	return symbolSpaceScope::symClass;
	if ( ns->findFunction ( name ) ) return symbolSpaceScope::symFunction;
	if ( auto sym = ns->findSymbol ( name ) )
	{
		switch ( sym->symClass )
		{
			case opSymbol::symbolClass::symbolClass_globalConst:
			case opSymbol::symbolClass::symbolClass_global:
				return symbolSpaceScope::symGlobal;
			case opSymbol::symbolClass::symbolClass_functionStatic:
				return symbolSpaceScope::symLocalStatic;
			case opSymbol::symbolClass::symbolClass_classConst:
				return symbolSpaceScope::symClassConst;
			case opSymbol::symbolClass::symbolClass_classStatic:
				return symbolSpaceScope::symClassStatic;
			default:
				throw errorNum::scINTERNAL;
		}
		return symbolSpaceScope::symGlobal;
	}
	throw errorNum::scINTERNAL;
}

bool symbolSpaceNamespace::isInterface ( cacheString const& name, bool isAccess ) const
{
	if ( auto *elem = ns->findClassElement ( name, isAccess ) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					return elem->methodAccess.func->isInterface;
				} else
				{
					return elem->assign.func->isInterface;
				}
				break;
			case fgxClassElementType::fgxClassType_method:
				return elem->methodAccess.func->isInterface;
				break;
			default:
				return false;
		}
	}
	if ( auto cls = ns->findClass ( name ) )
	{
		return cls->isInterface;
	}
	if ( auto *sym = ns->findSymbol ( name ) )
	{
		return sym->isInterface;
	}
	if ( auto * func = ns->findFunction ( name ) )
	{
		return func->isInterface;
	}
	throw errorNum::scINTERNAL;
}

bool symbolSpaceNamespace::isMethod ( cacheString const &name, bool isAccess ) const
{
	if ( ns->findFunction ( name ) ) return false;

	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
			case fgxClassElementType::fgxClassType_prop:
				return true;
			default:
				return false;
		}
	}
	return false;
}

bool symbolSpaceNamespace::isIterator ( cacheString const &name, bool isAccess ) const
{
	if ( auto func = ns->findFunction ( name ) )
	{
		return func->isIterator;
	}

	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				{
					return elem->methodAccess.func->isIterator;
				}
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func )
					{
					return elem->methodAccess.func->isIterator;
					}
				} else
				{
					if ( elem->assign.func )
					{
						return elem->assign.func->isIterator;
					}
				}
				return false;
			default:
				return false;
		}
	}
	return  false;
}

bool symbolSpaceNamespace::isVirtual ( cacheString const &name, bool isAccess ) const
{
	if ( ns->findFunction ( name ) ) return false;
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		return elem->isVirtual;
	}
	return false;
}

bool symbolSpaceNamespace::isStatic ( cacheString const &name, bool isAccess ) const
{
	if ( ns->findFunction ( name ) ) return false;
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		return elem->isStatic;
	}
	return false;
}

bool symbolSpaceNamespace::isExtension ( cacheString const &name, bool isAccess ) const
{
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				if ( elem->methodAccess.func ) return elem->methodAccess.func->isExtension;
				break;
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func ) return elem->methodAccess.func->isExtension;
				} else
				{
					if ( elem->assign.func ) return elem->assign.func->isExtension;
				}
				break;
			default:
				break;
		}
	}
	return false;
}

bool symbolSpaceNamespace::isVariantParam ( cacheString const &name, bool isAccess ) const
{
	if ( ns->findFunction ( name ) )
	{
		return ns->findFunction ( name )->isVariantParam;
	}

	compClassElementSearch *elem;

	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		return elem->isStatic;
	}
	return false;
}

bool symbolSpaceNamespace::isPcountParam ( cacheString const &name, bool isAccess ) const
{
	if ( ns->findFunction ( name ) )
	{
		return ns->findFunction ( name )->isPcountParam;
	}
	compClassElementSearch *elem;

	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		return elem->isStatic;
	}
	return false;
}

int16_t symbolSpaceNamespace::getObjectOffset ( cacheString const &name, bool isAccess ) const
{
	if ( ns->findFunction ( name ) ) return -1;
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_prop:
				throw errorNum::scINTERNAL;
			default:
				return elem ->methodAccess.objectStart;
		}
	}
	return -1;
}

bool symbolSpaceNamespace::isIVar ( cacheString const &name ) const
{
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, true )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_iVar:
			case fgxClassElementType::fgxClassType_static:
				return true;
			default:
				break;
		}
	}
	return false;
}

symbolTypeClass	const symbolSpaceNamespace::getType ( cacheString const &name, bool isAccess ) const
{
	compClassElementSearch *elem;
	if ( (elem = ns->findClassElement ( name, isAccess )) )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				return elem->methodAccess.func->getReturnType();;
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func ) return elem->methodAccess.func->getReturnType();;
				} else
				{
					if ( elem->assign.func ) return elem->assign.func->getReturnType();;
				}
				return symWeakAtomType;
			case fgxClassElementType::fgxClassType_iVar:
			case fgxClassElementType::fgxClassType_static:
			case fgxClassElementType::fgxClassType_const:
				return elem->elem->symType;
			case fgxClassElementType::fgxClassType_inherit:
				elem->elem->symType = symbolTypeClass ( symbolType::symWeakObject, elem->name );
				return elem->elem->symType;
			default:
				break;
		}
		return symUnknownType;
	}
	if ( ns->findClass ( name ) ) return symWeakAtomType;
	if ( ns->findFunction ( name ) ) return symWeakAtomType;
	if ( auto s = ns->findSymbol ( name ) )
	{
		return s->getType ( );
	}
	return symUnknownType;
}

cacheString const symbolSpaceNamespace::getSymbolName ( symbolSpaceScope scope, uint32_t index ) const
{
	switch ( scope )
	{
		case symbolSpaceScope::symGlobal:
			for ( auto it = file->symbols.begin(); it != file->symbols.end(); it++ )
			{
				if ( (*it).second.globalIndex == index ) return (*it).second.name;
			}
			break;
		case symbolSpaceScope::symClass:
			for ( auto it = file->classList.begin(); it != file->classList.end(); it++ )
			{
				if ( (*it).second->cClass->atom == index ) return (*it).second->name;
			}
			break;
		case symbolSpaceScope::symFunction:
			for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
			{
				if ( (*it).second->cFunc->atom == index ) return (*it).second->name;
			}
			break;
		default:
			return cacheString();
	}
	return cacheString();
}

bool symbolSpaceNamespace::setType ( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	auto sym = ns->findSymbol ( name );
	if ( sym )
	{
		switch ( sym->symClass )
		{
			case opSymbol::symbolClass::symbolClass_undefined:
			case opSymbol::symbolClass::symbolClass_global:
			case opSymbol::symbolClass::symbolClass_functionStatic:
				sym->assigned = true;
				return sym->setType ( type, scanQueue );
			case opSymbol::symbolClass::symbolClass_globalConst:
			case opSymbol::symbolClass::symbolClass_classConst:
			case opSymbol::symbolClass::symbolClass_classStatic:
				return false;
		}
	}
	return false;
}

bool symbolSpaceNamespace::isConst ( cacheString const &name, bool isAccess ) const
{
	bool isConst = true;
	opFunction *func = ns->findFunction ( name );
	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					for ( auto &it : elem->elem->data.method.virtOverrides )
					{
						isConst &= it->isConst;
					}
					return isConst;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
						{
							isConst &= it->isConst;
						}
						return isConst;
					} else
					{
						for ( auto &it : elem->elem->data.prop.assignVirtOverrides )
						{
							isConst &= it->isConst;
						}
						return isConst;
					}
				case fgxClassElementType::fgxClassType_const:
					return true;
				default:
					return false;
			}
		}
		return false;
	} else
	{
		return func->isConst;
	}
}

bool symbolSpaceNamespace::isPure ( cacheString const &name, bool isAccess ) const
{
	bool isPure = true;
	opFunction *func = ns->findFunction ( name );
	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					for ( auto &it : elem->elem->data.method.virtOverrides )
					{
						isPure &= it->isPure;
					}
					return isPure;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
						{
							isPure &= it->isPure;
						}
						return isPure;
					} else
					{
						for ( auto &it : elem->elem->data.prop.assignVirtOverrides )
						{
							isPure &= it->isPure;
						}
						return isPure;
					}
				default:
					return true;
			}
		}
		return false;
	} else
	{
		return func->isPure;
	}
}

void symbolSpaceNamespace::setParameterType	( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	opFunction *func =  ns->findFunction ( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
		}
	}

	if ( !func )
	{
		return;
	}
	if ( !func->inUse )
	{
		func->inUse = true;
		if ( scanQueue ) scanQueue->push ( func );
		func->accessors.insert ( {acc, srcLocation{}} );
	}

	func->setParamType ( comp, sym, pNum, type, scanQueue );
}

void symbolSpaceNamespace::setParameterTypeNoThrow ( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	opFunction *func = ns->findFunction ( name );

	if ( !func )
	{
		compClassElementSearch *elem;
		if ( (elem = ns->findClassElement ( name, isAccess )) )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					func = ns->findFunction ( elem->methodAccess.func->name );
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( isAccess )
					{
						func = ns->findFunction ( elem->methodAccess.func->name );
					} else
					{
						func = ns->findFunction ( elem->assign.func->name );
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
		}
	}

	if ( !func )
	{
		return;
	}
	if ( !func->inUse )
	{
		func->inUse = true;
		if ( scanQueue ) scanQueue->push ( func );
		func->accessors.insert ( {acc, srcLocation{}} );
	}

	func->setParamTypeNoThrow ( comp, sym, pNum, type, scanQueue );
}

symbolSource symbolSpaceNamespace::getSymbolSource ( cacheString const &name )
{
	auto *elem = ns->findClassElement ( name, true );
	if ( elem ) return elem->elem;

	auto *func = ns->findFunction ( name );
	if ( func ) return func;

	auto *sym = ns->findSymbol ( name );
	if ( sym ) return sym;

	auto *cls = ns->findClass ( name );
	if ( cls ) return cls;

	return std::monostate ();
}

srcLocation const &symbolSpaceNamespace::getDefinitionLocation ( cacheString const &name, bool isAccess ) const
{
	auto *elem = ns->findClassElement ( name, true );
	if ( elem ) return elem->elem->location;

	auto *func = ns->findFunction ( name );
	if ( func ) return func->location;

	auto *sym = ns->findSymbol ( name );
	if ( sym ) return sym->location;

	auto *cls = ns->findClass ( name );
	if ( cls ) return cls->location;

	throw errorNum::scINTERNAL;
}