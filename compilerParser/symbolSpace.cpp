/*
	symbol space implementation

*/

#include "fileParser.h"
#include "symbolSpace.h"
#include "symbolSpaceClass.h"
#include "symbolSpaceNamespace.h"
#include "nameSpace.h"

bool symbolStack::isDefined ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ((*it)->isDefined (name, isAccess)) return true;
	}
	return false;
}

bool symbolStack::isDefinedNS ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin (); it != syms.crend (); it++ )
	{
		if ( (*it)->isDefinedNS ( name, isAccess ) ) return true;
	}
	return false;
}

bool symbolStack::isAccessed ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isAccessed ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isAssigned ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isAssigned ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isInitialized ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isInitialized ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isClosedOver ( cacheString const &name ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, true ) ) return (*it)->isClosedOver ( name );
	}
	return false;
}

bool symbolStack::isMethod ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ;it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isMethod ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isMethod ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isIterator ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ;it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isIterator ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isIterator ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isExtension ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ;it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isExtension ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isExtension ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isStatic ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isStatic ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isStatic ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isVariantParam ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isVariantParam ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isVariantParam ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isPcountParam ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin (); it != syms.crend (); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend (); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isPcountParam ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isPcountParam ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isVirtual ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isVirtual ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isVirtual ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isIVar ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) ) return (*it)->isIVar ( name );
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) ) return (*it)->isIVar ( name );
	}
	return false;
}

bool symbolStack::isConst ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased ( ) )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isConst ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isConst ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isPure ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased ( ) )
				{
					if ( (*it)->isDefined ( name, isAccess ) ) return (*it)->isPure ( name, isAccess );
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) ) return (*it)->isPure ( name, isAccess );
	}
	return false;
}

bool symbolStack::isInterface ( cacheString const& name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased ( ) )
				{
					if ( (*it)->isDefined ( name, isAccess ) ) return (*it)->isInterface ( name, isAccess );
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) ) return (*it)->isInterface ( name, isAccess );
	}
	return false;
}
void symbolStack::setClosedOver ( cacheString const &name )
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, true ) )
		{
			(*it)->setClosedOver ( name );
			break;
		}
	}
	return;
}

void symbolStack::setAccessed ( cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			(*it)->setAccessed ( file, name, isAccess, acc, scanQueue );
			break;
		}
	}
	return;
}

symbolTypeClass const symbolStack::getType ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->getType ( name, isAccess );
		}
	}
	return symUnknownType;
}

bool symbolStack::isFuncBC ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (*it)->getFuncCallingConv ( name, isAccess ) == fgxFuncCallConvention::opFuncType_Bytecode;
						}
						return false;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (*it)->getFuncCallingConv ( name, isAccess ) == fgxFuncCallConvention::opFuncType_Bytecode;
			}
			return false;
		}
	}
	throw errorNum::scINTERNAL;
}

bool symbolStack::isFuncC ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							if ( (*it)->getFuncCallingConv ( name, isAccess ) == fgxFuncCallConvention::opFuncType_cDecl )
							{
								return true;
							}
							return false;
						}
						return false;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				if ( (*it)->getFuncCallingConv ( name, isAccess ) == fgxFuncCallConvention::opFuncType_cDecl )
				{
					return true;
				}
				return false;
			}
			return false;
		}
	}
	throw errorNum::scINTERNAL;
}

bool symbolStack::isFunc ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isFunc ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isFunc ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isClass ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return ( *it )->isClass ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isClass ( name, isAccess );
		}
	}
	return false;
}

bool symbolStack::isProperty ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						return (*it)->isProperty ( name, isAccess );
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->isProperty ( name, isAccess );
		}
	}
	return false;
}

cacheString const symbolStack::getSymbolName ( symbolSpaceScope scope, uint32_t index )  const
{
	switch ( scope )
	{
		case symbolSpaceScope::symGlobal:
		case symbolSpaceScope::symLocalStatic:
		case symbolSpaceScope::symLocalConst:
		case symbolSpaceScope::symClass:
		case symbolSpaceScope::symFunction:
		case symbolSpaceScope::symClassConst:
		case symbolSpaceScope::symClassAssign:
		case symbolSpaceScope::symClassAccess:
		case symbolSpaceScope::symClassMethod:
		case symbolSpaceScope::symClassStatic:
		case symbolSpaceScope::symClassIVar:
		case symbolSpaceScope::symClassInherit:
			for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
			{
				if ( !(*it)->isStackBased() )
				{
					auto &name = (*it)->getSymbolName ( scope, index );
					if ( name.size () )
					{
						return name;
					}
				}
			}
			break;
		case symbolSpaceScope::symLocalField:
			break;
		case symbolSpaceScope::symLocal:
		case symbolSpaceScope::symLocalParam:
			for ( auto it = syms.cbegin ( ); it != syms.cend ( ); it++ )
			{
				if ( (*it)->isStackBased ( ) )
				{
					auto &name = (*it)->getSymbolName ( scope, index );
					if ( name.size () && (int32_t)index < (*it)->getRollup ( ) )
					{
						return name;
					}
					index = (uint32_t)((int32_t)index - (*it)->getRollup ());
				}
			}
			break;
		default:
			break;
	}
	throw errorNum::scINTERNAL;
}

symbolSpaceScope symbolStack::getScope ( cacheString const &name, bool isAccess ) const
{
	static cacheString newName = file->sCache.get ( "new" );
	if ( name == newName ) return symbolSpaceScope::symFunction;				// special case for new... always maps to ::new
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->getScope ( name, isAccess );
		}
	}
	return symbolSpaceScope::symScopeUnknown;
}

int32_t symbolStack::getIndex ( cacheString const &name, bool isAccess ) const
{
	int32_t index = 0;

	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			// we must rollup everything going up the stack so we get the correct index into the base
			if ( (*it)->isStackBased () )
			{
				index = (*it)->getIndex ( name, isAccess );
				it++;
				while ( it != syms.crend() )
				{
					index += (*it)->getRollup();
					it++;
				}
				assert ( index != -1 );
				return index;
			}
			index = (*it)->getIndex ( name, isAccess );
			assert ( index != -1 );
			return index;
		}
	}
	throw errorNum::scINTERNAL;
}

compClass *symbolStack::findClass ( cacheString const &name ) const
{
	compClass *cls;
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		cls = (*it)->findClass ( name );
		if ( cls )
		{
			return cls;
		}
		
	}
	return nullptr;
}

int16_t symbolStack::getVtabEntry ( cacheString const &name, bool isAccess ) const
{
	int16_t vTab;
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		vTab = (*it)->getVtabEntry ( name, isAccess );
		if ( vTab != -1 ) return vTab;
	}
	throw errorNum::scINTERNAL;
}

int16_t symbolStack::getObjectOffset ( cacheString const &name, bool isAccess ) const
{
	int16_t ctxt;

	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		ctxt = (*it)->getObjectOffset ( name, isAccess );
		if ( ctxt != -1 )
		{
			return ctxt;
		}
	}
	throw errorNum::scINTERNAL;
}

compClass *symbolStack::getClass ( void ) const
{
	for ( auto it = syms.crbegin (); it != syms.crend (); it++ )
	{
		auto c = (*it)->getClass ();
		if ( c ) return c;
	}
	throw errorNum::scINTERNAL;
}

void symbolStack::setParameterType ( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	for ( auto it = syms.crbegin (); it != syms.rend (); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							(*it)->setParameterType ( comp, sym, name, isAccess, pNum, type, acc, scanQueue );
							return;
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				(*it)->setParameterType ( comp, sym, name, isAccess, pNum, type, acc, scanQueue );
				return;
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
}

void symbolStack::setParameterTypeNoThrow ( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	for ( auto it = syms.crbegin (); it != syms.rend (); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend (); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							(*it)->setParameterTypeNoThrow ( comp, sym, name, isAccess, pNum, type, acc, scanQueue );
							return;
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				(*it)->setParameterTypeNoThrow ( comp, sym, name, isAccess, pNum, type, acc, scanQueue );
				return;
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
}

cacheString const symbolStack::getFuncName ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (*it)->getFuncName ( name, isAccess );
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (*it)->getFuncName ( name, isAccess );
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
	throw errorNum::scINTERNAL;
}

astNode	*symbolStack::getFuncDefaultParam ( cacheString const &name, bool isAccess, int32_t nParam ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (*it)->getFuncDefaultParam ( name, isAccess, nParam );
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (*it)->getFuncDefaultParam ( name, isAccess, nParam );
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
	throw errorNum::scINTERNAL;
}

int32_t symbolStack::getFuncNumParams ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (int32_t)(*it)->getFuncNumParams ( name, isAccess );
						}
						return -1;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (int32_t)(*it)->getFuncNumParams ( name, isAccess );
			}
			return -1;
		}
	}
	return -1;
}

symbolTypeClass symbolStack::getFuncParamType ( cacheString const &name, bool isAccess, int32_t nParam, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (*it)->getFuncParamType ( name, isAccess, nParam, acc, scanQueue );
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (*it)->getFuncParamType ( name, isAccess, nParam, acc, scanQueue );
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
	throw errorNum::scINTERNAL;
}

symbolTypeClass symbolStack::getFuncReturnType ( cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (*it)->getFuncReturnType ( this, name, isAccess, acc, scanQueue );
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (*it)->getFuncReturnType ( this, name, isAccess, acc, scanQueue );
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
	return symUnknownType;
}

symbolTypeClass symbolStack::getMarkFuncReturnType ( class compExecutable *comp, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS ) const
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (*it)->getMarkFuncReturnType ( comp, this, name, isAccess, acc, scanQueue, isLS );
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (*it)->getMarkFuncReturnType ( comp, this, name, isAccess, acc, scanQueue, isLS );
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
	return symUnknownType;
}

size_t symbolStack::getFuncSourceIndex ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			for ( ; it != syms.crend ( ); it++ )
			{
				if ( !(*it)->isStackBased () )
				{
					if ( (*it)->isDefined ( name, isAccess ) )
					{
						if ( (*it)->isFunc ( name, isAccess ) )
						{
							return (*it)->getFuncSourceIndex ( name, isAccess );
						}
						throw errorNum::scNOT_A_FUNCTION;
					}
				}
			}
			break;
		}
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			if ( (*it)->isFunc ( name, isAccess ) )
			{
				return (*it)->getFuncSourceIndex ( name, isAccess );
			}
			throw errorNum::scNOT_A_FUNCTION;
		}
	}
	throw errorNum::scINTERNAL;
}

astNode *symbolStack::getInitializer ( cacheString const &name ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->isDefined ( name, true ) )
		{
			return (*it)->getInitializer ( name );
		}
	}
	throw errorNum::scINTERNAL;
}

symbolSource symbolStack::getSymbolSource ( cacheString const &name ) const
{
	for ( auto it = syms.crbegin (); it != syms.crend (); it++ )
	{
		if ( (*it)->isDefined ( name, true ) )
		{
			return (*it)->getSymbolSource ( name );
		}
	}
	throw errorNum::scINTERNAL;
}

cacheString const symbolStack::getResolvedName ( cacheString const &name, bool isAccess ) const
{
	for ( auto it = syms.crbegin (); it != syms.crend (); it++ )
	{
		if ( (*it)->isDefinedNS ( name, isAccess ) )
		{
			return (*it)->getResolvedName ( name, isAccess );
		}
	}
	throw errorNum::scINTERNAL;
}

char const *symbolStack::getInlineFunc (  ) const
{
	for ( auto it = syms.crbegin ( ); it != syms.crend ( ); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			return dynamic_cast<symbolSpaceInline *>((*it))->funcName.c_str ( );
		}
	}
	return nullptr;
}

uint32_t symbolStack::getInlineFuncRetIndex () const
{
	for ( auto it = syms.crbegin (); it != syms.crend (); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			return  dynamic_cast<symbolSpaceInline *>((*it))->retIndex;
		}
	}
	throw errorNum::scINTERNAL;
}

bool symbolStack::getInlineFuncNeedValue () const
{
	for ( auto it = syms.crbegin (); it != syms.crend (); it++ )
	{
		if ( (*it)->symType == symbolSpaceType::symTypeInline )
		{
			return  dynamic_cast<symbolSpaceInline *>((*it))->needValue;
		}
	}
	throw errorNum::scINTERNAL;
}

bool symbolStack::setType ( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			return (*it)->setType ( name, isAccess, type, acc, scanQueue );
		}
	}
	throw errorNum::scINTERNAL;
}

void symbolStack::setType ( cacheString const &name, bool isAccess, cacheString const &className )
{
	for ( auto it = syms.crbegin(); it != syms.crend(); it++ )
	{
		if ( (*it)->isDefined ( name, isAccess ) )
		{
			(*it)->setType ( name, isAccess, className );
			return;
		}
	}
	throw errorNum::scINTERNAL;
}

void symbolStack::setAllLocalAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	for ( auto it = syms.begin(); it != syms.end(); it++ )
	{
		if ( (*it)->isStackBased () )
		{
			(*it)->setAllLocalAccessed ( acc, scanQueue );
		}
	}
}

symbolStack::symbolStack ( class opFile *file, class opFunction *func, class symbolSpaceLocalAdd *localAdd ) : file ( file )
{
	if ( func )
	{
		ns = symbolSpaceNamespace ( file, &file->ns, &func->usingList );
	} else
	{
		ns = symbolSpaceNamespace ( file, &file->ns, &usingList );
	}

	push ( &ns );
	if ( func )
	{
		if ( !func->isFGL )
		{
			if ( func->classDef && !func->isStatic && !func->isExtension )
			{
				sClass = symbolClass ( file, func->classDef->name, 0 );
				push ( &sClass );
			}
		}
		if ( func->params.size () )
		{
			push ( &func->params );
		}
	}
	if ( localAdd )
	{
		push ( localAdd );
	}
	push ( &sPCall );
}

symbol *symbol::symbolFactory ( class opFile *file, class sourceFile *srcFile, const char **expr )
{
	auto symType = *(symbolSpaceType *) (*expr);
	(*expr) += sizeof ( symbolSpaceType );
	switch ( symType )
	{
		case symbolSpaceType::symTypeLocal:
			return new symbolLocal ( file, srcFile, expr );
		case symbolSpaceType::symTypeClass:
			return new symbolClass ( file, expr );
		case symbolSpaceType::symTypeStatic:
			return new symbolStatic ( file, srcFile, expr );
		case symbolSpaceType::symTypeField:
			return new symbolField ( file, srcFile, expr );
		case symbolSpaceType::symTypeParam:
			return new symbolSpaceParams ( file, srcFile, expr );
		default:
			throw errorNum::scINTERNAL;
	}
}

symbolLocal::symbolLocal ( class opFile *file, class sourceFile *srcFile, char const ** expr ) : symbol ( symbolSpaceType::symTypeLocal )
{
	name = file->sCache.get ( expr );
	location = srcLocation ( srcFile, expr );

	type = symbolTypeClass ( file, expr );
	initializer = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	documentation = stringi ( expr );
}


symbolStatic::symbolStatic ( class opFile *file, class sourceFile *srcFile, char const ** expr ) noexcept : symbol ( symbolSpaceType::symTypeStatic ), file ( file )
{
	name = file->sCache.get ( expr );
	qualName = file->sCache.get ( expr );
	enclosingName = file->sCache.get ( expr );
}

symbolField::symbolField ( class opFile *file, class sourceFile *srcFile, char const **expr ) : symbol ( symbolSpaceType::symTypeField )
{
	name = file->sCache.get ( expr );
	documentation =  stringi ( expr );
}

symbolParamDef::symbolParamDef ( opFile *file, class sourceFile *srcFile, char const ** expr )
{
	name = file->sCache.get ( expr );
	displayName = file->sCache.get ( expr );
	documentation = stringi ( expr );
	location = srcLocation ( srcFile, expr );
	type = symbolTypeClass ( file, expr );
	localType = symbolTypeClass ( file, expr );
	initializer = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	assigned = false;
	closedOver = false;
}

symbolClass::symbolClass ( class opFile *file, const char **expr ) : symbol ( symbolSpaceType::symTypeClass ), file ( file )
{
	className = file->sCache.get ( expr );
	(*expr) += className.length ( ) + 1;

	insertPos += *(size_t *) *expr;
	(*expr) += sizeof ( size_t );
}
