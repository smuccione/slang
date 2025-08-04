/*

	Symbol space

*/

#include "fileParser.h"
#include "symbolSpace.h"
#include "symbolSpaceClass.h"

symbolSpaceScope symbolClass::getScope ( cacheString const &name, bool isAccess ) const
{
	compClassElementSearch	*elem;

	elem = (*this)[name];

	if ( elem )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_method:
				return symbolSpaceScope::symClassMethod;
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func )
					{
						return symbolSpaceScope::symClassAccess;
					}
				} else
				{
					if ( elem->assign.func )
					{
						return symbolSpaceScope::symClassAssign;
					}
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
	return symbolSpaceScope::symScopeUnknown;
};

symbolTypeClass const symbolClass::getType ( cacheString const &name, bool isAccess )  const
{
	auto elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				return elem->methodAccess.func->getReturnType();;
			} else
			{
				return elem->assign.func->getReturnType();;
			}
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			return elem->elem->symType;
		case fgxClassElementType::fgxClassType_method:
			return elem->methodAccess.func->getReturnType();;
		case fgxClassElementType::fgxClassType_inherit:
			elem->elem->symType = symbolTypeClass ( symbolType::symWeakObject, elem->name );
			return elem->elem->symType;
		default:
			return symUnknownType;
	}
}

bool symbolClass::setType ( cacheString const &name, bool isAccess, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	auto elem = (*this)[name];
	return elem->elem->setType ( isAccess, type, acc, scanQueue );
}


int32_t symbolClass::getIndex	( cacheString const &name, bool isAccess ) const
{
	if ( !isDefined ( name, isAccess ) ) return -1;

	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			return (int32_t)file->symbols.find ( elem->symbolName )->second.globalIndex;
		default:
			throw errorNum::scINTERNAL;
	}
}

int16_t symbolClass::getObjectOffset ( cacheString const &name, bool isAccess ) const
{
	if ( !isDefined ( name, isAccess ) ) return -1;

	auto elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_method:
			if ( cacheString ( name ).find ( "::" ) == cacheString::npos )
			{
				if ( elem->isVirtual )
				{
					return (int16_t)elem->methodAccess.data.vTabEntry;
				} else
				{
					return (int16_t)elem->methodAccess.objectStart;
				}
			} else
			{
				return (int16_t)elem->methodAccess.objectStart;
			}
		case fgxClassElementType::fgxClassType_prop:
			if ( cacheString ( name ).find ( "::" ) == cacheString::npos )
			{
				if ( isAccess )
				{
					if ( elem->isVirtual )
					{
						return (int16_t)elem->methodAccess.data.vTabEntry;
					} else
					{
						return (int16_t)elem->methodAccess.objectStart;
					}
				} else
				{
					if ( elem->isVirtual )
					{
						return (int16_t)elem->assign.data.vTabEntry;
					} else
					{
						return (int16_t)elem->assign.objectStart;
					}
				}
			} else
			{
				if ( isAccess )
				{
					return (int16_t)elem->methodAccess.objectStart;
				} else
				{
					return (int16_t)elem->assign.objectStart;
				}
			}
			break;
		case fgxClassElementType::fgxClassType_inherit:
		case fgxClassElementType::fgxClassType_iVar:
			if ( elem->isVirtual )
			{
				return (int16_t)elem->methodAccess.data.vTabEntry;
			} else
			{
				return (int16_t)elem->methodAccess.data.varIndex;
			}
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			return (int16_t)file->symbols.find ( elem->symbolName )->second.globalIndex;
		default:
			throw errorNum::scINTERNAL;
	}
}

int16_t symbolClass::getVtabEntry ( cacheString const &name, bool isAccess ) const
{
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				return (int16_t)elem->methodAccess.data.vTabEntry;
			} else
			{
				return (int16_t)elem->assign.data.vTabEntry;
			}
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
		case fgxClassElementType::fgxClassType_method:
		case fgxClassElementType::fgxClassType_inherit:
			return (int16_t)(*this)[name]->methodAccess.data.vTabEntry;
		default:
			throw errorNum::scINTERNAL;
	}
}

void symbolClass::setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc )
{
	auto const elem = (*this)[name];
	std::lock_guard g1 ( elem->elem->accessorsAccess );

	switch ( elem->type )
	{
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
			break;
		case fgxClassElementType::fgxClassType_method:
			return elem->methodAccess.func->setAccessed ( acc, scanQueue, loc );
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
			if ( isAccess )
			{
				if ( elem->elem->accessors.empty () )
				{
					if ( scanQueue ) scanQueue->push ( elem->elem );
				}
				elem->elem->accessors.insert ( {acc, loc} );
			}
			break;
		default:
			break;
	}
}

void symbolClass::setParameterType ( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				if ( !elem->elem->data.prop.accessVirtOverrides.size () ) throw errorNum::scNOT_A_FUNCTION;
				for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
				{
					it->setParamType ( comp, sym, pNum, type, scanQueue );
				}
			} else
			{
				if ( !elem->elem->data.prop.assignVirtOverrides.size () ) throw errorNum::scNOT_A_FUNCTION;
				for ( auto &it : elem->elem->data.prop.assignVirtOverrides )
				{
					it->setParamType ( comp, sym, pNum, type, scanQueue );
				}
			}
			return;
		case fgxClassElementType::fgxClassType_method:
			elem->methodAccess.func->setParamType ( comp, sym, pNum, type, scanQueue );
			return;
		default:
			break;
	}
	throw errorNum::scNOT_A_FUNCTION;
}

void symbolClass::setParameterTypeNoThrow ( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				if ( !elem->elem->data.prop.accessVirtOverrides.size () ) throw errorNum::scNOT_A_FUNCTION;
				for ( auto &it : elem->elem->data.prop.accessVirtOverrides )
				{
					it->setParamTypeNoThrow ( comp, sym, pNum, type, scanQueue );
				}
			} else
			{
				if ( !elem->elem->data.prop.assignVirtOverrides.size () ) throw errorNum::scNOT_A_FUNCTION;
				for ( auto &it : elem->elem->data.prop.assignVirtOverrides )
				{
					it->setParamTypeNoThrow ( comp, sym, pNum, type, scanQueue );
				}
			}
			return;
		case fgxClassElementType::fgxClassType_method:
			elem->methodAccess.func->setParamTypeNoThrow ( comp, sym, pNum, type, scanQueue );
			return;
		default:
			break;
	}
	throw errorNum::scNOT_A_FUNCTION;
}

bool symbolClass::isFunc ( cacheString const &name, bool isAccess ) const
{
	auto elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_method:
			return true;
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				if ( elem->methodAccess.func ) return true;
			} else
			{
				if ( elem->assign.func ) return true;
			}
			return false;
		default:
			return false;
	}
}

bool symbolClass::isProperty ( cacheString const &name, bool isAccess ) const
{
	switch ( (*this)[name]->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			return true;
		default:
			return false;
	}
}

fgxFuncCallConvention symbolClass::getFuncCallingConv ( cacheString const &name, bool isAccess ) const
{
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				return elem->methodAccess.func->conv;
			} else
			{
				return elem->assign.func->conv;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			return elem->methodAccess.func->conv;
		default:
			return fgxFuncCallConvention::opFuncType_Bytecode;
	}
}


cacheString const symbolClass::getFuncName ( cacheString const &name, bool isAccess ) const
{
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				return elem->methodAccess.func->name;
			} else
			{
				return elem->assign.func->name;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			return elem->methodAccess.func->name;
		default:
			return cacheString();
	}
}

uint32_t symbolClass::getFuncNumParams ( cacheString const &name, bool isAccess ) const
{
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				return (uint32_t)elem->methodAccess.func->params.size ( );
			} else
			{
				return ( uint32_t) elem->assign.func->params.size ( );
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			return ( uint32_t) elem->methodAccess.func->params.size ( );
		default:
			return 0;
	}
}
symbolTypeClass	const symbolClass::getFuncReturnType ( class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) const
{
	opFunction *func;
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				func = elem->methodAccess.func;
			} else
			{
				func = elem->assign.func;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			func = elem->methodAccess.func;
			break;
		default:
			return symWeakVariantType;
	}
	func->accessors.insert ( {acc, loc} );
	return func->getReturnType ();
}

symbolTypeClass	const symbolClass::getMarkFuncReturnType ( class compExecutable *compDef, class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS, srcLocation const &loc ) const
{
	opFunction *func;
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				func = elem->methodAccess.func;
			} else
			{
				func = elem->assign.func;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			func = elem->methodAccess.func;
			break;
		default:
			return symWeakVariantType;
	}
	func->accessors.insert ( {acc, loc} );
	return func->getMarkReturnType ( compDef, sym, acc, scanQueue, isLS, loc );
}

symbolTypeClass const symbolClass::getFuncParamType ( cacheString const &name, bool isAccess, int32_t nParam, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const
{
	opFunction *func;
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				func = elem->methodAccess.func;
			} else
			{
				func = elem->assign.func;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			func = elem->methodAccess.func;
			break;
		default:
			return symWeakVariantType;
	}

	if ( nParam < (int32_t)func->params.size () )
	{
		return func->getParamType ( nParam, acc, scanQueue );
	}
	return symWeakVariantType;
}

astNode	*symbolClass::getFuncDefaultParam ( cacheString const &name, bool isAccess, int32_t nParam ) const
{
	opFunction *func;
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				func = elem->methodAccess.func;
			} else
			{
				func = elem->assign.func;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			func = elem->methodAccess.func;
			break;
		default:
			return 0;
	}
	return func->params[nParam]->initializer;
}

bool symbolClass::isMethod ( cacheString const &name, bool isAccess ) const
{
	if ( !isDefined ( name, isAccess ) ) return false;

	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				if ( elem->methodAccess.func )
				{
					return true;
				}
			} else
			{
				if ( elem->assign.func )
				{
					return true;
				}
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			if ( elem->methodAccess.func && !elem->methodAccess.func->isStatic && !elem->methodAccess.func->isExtension )
			{
				return true;
			}
			break;
		default:
			break;
	}
	return false;
}

bool symbolClass::isExtension ( cacheString const &name, bool isAccess ) const
{
	if ( !isDefined ( name, isAccess ) ) return false;
	opFunction *func;
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				func = elem->methodAccess.func;
			} else
			{
				func = elem->assign.func;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			func = elem->methodAccess.func;
			break;
		default:
			return 0;
	}
	return func->isExtension;
}

bool symbolClass::isVirtual ( cacheString const &name, bool isAccess ) const
{
	if ( !isDefined ( name, isAccess ) ) return false;
	if ( name.find ( "::" ) == cacheString::npos )
	{
		return (*this)[name]->isVirtual;
	} else
	{
		return false;
	}
}

bool symbolClass::isStatic ( cacheString const &name, bool isAccess ) const
{
	if ( !isDefined ( name, isAccess ) ) return false;
	return (*this)[name]->isStatic;
}

bool symbolClass::isVariantParam ( cacheString const &name, bool isAccess ) const
{
	if ( !isDefined ( name, isAccess ) ) return false;
	opFunction *func;
	auto const elem = (*this)[name];
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_prop:
			if ( isAccess )
			{
				func = elem->methodAccess.func;
			} else
			{
				func = elem->assign.func;
			}
			break;
		case fgxClassElementType::fgxClassType_method:
			func = elem->methodAccess.func;
			break;
		default:
			return false;
	}

	if ( func ) return func->isVariantParam;
	return false;
}

bool symbolClass::isIVar ( cacheString const &name ) const
{
	if ( !isDefined ( name, true ) ) return false;

	switch ( (*this)[name]->type )
	{
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
			return true;
		default:
			return false;
	}
}

compClassElementSearch *symbolClass::operator [] ( cacheString const &name ) const
{
	return getClass()->getElement ( fgxClassElemSearchType::fgxClassElemAllPrivate, name );
}


bool symbolClass::isDefined ( cacheString const &name, bool isAccess ) const
{
	// TODO: do full namespace resolution!!!!   do everyting by elems
	if ( !getClass ( ) ) return false;

	auto const elem = (*this)[name];
	if ( elem )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func ) return true;
				} else
				{
					if ( elem->assign.func ) return true;
				}
				break;
			default:
				return true;
		}
	}
	return false;
}

bool symbolClass::isDefinedNS ( cacheString const &name, bool isAccess ) const
{
	// TODO: do full namespace resolution!!!!   do everyting by elems
	if ( !getClass () ) return false;

	auto const elem = (*this)[name];
	if ( elem )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func ) return true;
				} else
				{
					if ( elem->assign.func ) return true;
				}
				break;
			default:
				return true;
		}
	}
	return false;
}

cacheString const symbolClass::getSymbolName ( enum symbolSpaceScope scope, uint32_t index ) const
{
	auto cl = getClass ( );
	switch ( scope )
	{
		case symbolSpaceScope::symClassConst:
		case symbolSpaceScope::symClassAccess:
		case symbolSpaceScope::symClassMethod:
		case symbolSpaceScope::symClassStatic:
		case symbolSpaceScope::symClassIVar:
		case symbolSpaceScope::symClassAssign:
			return cl->elements[index].name;
		case symbolSpaceScope::symClassInherit:
			for ( size_t loop = 0; loop < cl->elements.size(); loop++ )
			{
				if ( cl->elements[loop].methodAccess.objectStart == index )
				{
					return cl->elements[loop].name;
				}
			}
			break;
		default:
			break;
	}
	return cacheString();
}

symbolSource symbolClass::getSymbolSource ( cacheString const &name )
{
	// TODO: do full namespace resolution!!!!   do everyting by elems
	if ( !getClass () ) return std::monostate ();

	auto const elem = (*this)[name];
	if ( elem )
	{
		return elem->elem;
	}
	return std::monostate();
}

srcLocation	const &symbolClass::getDefinitionLocation ( cacheString const &name, bool isAccess ) const
{
	// TODO: do full namespace resolution!!!!   do everyting by elems
	if ( !getClass () ) throw errorNum::scINTERNAL;;

	auto const elem = (*this)[name];
	if ( elem )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_prop:
				if ( isAccess )
				{
					if ( elem->methodAccess.func ) return elem->methodAccess.func->location;
				} else
				{
					if ( elem->assign.func ) return elem->assign.func->location;;
				}
				break;
			default:
				return elem->elem->location;
		}
	}
	throw errorNum::scINTERNAL;
}
compClass *symbolClass::getClass ( void ) const
{
	if ( file->classList.find ( className ) == file->classList.end ( ) )
	{
		return nullptr;
	}
	return file->classList.find( className )->second->cClass;
}

