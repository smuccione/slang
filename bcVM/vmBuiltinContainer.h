/*
	vm Container implmentation header file
*/

#pragma once

#include "bcInterpreter/bcInterpreter.h"

#define _MAKE_NAME( name ) #name
#define MAKE_NAME( name ) _MAKE_NAME(name)
#define base_type std::CONTAINER_NAME

template <bool isOrdered>
struct vmContainerCompare
{
	bool		 doUser;
	vmInstance	*instance;
	VAR			 cbFunc;

	vmContainerCompare ( vmInstance *instance, VAR *cbFunc ) : instance ( instance )
	{
		if ( cbFunc )
		{
			doUser = true;
			this->cbFunc = *cbFunc;
		} else
		{
			doUser = false;
			this->cbFunc = nullptr;
		}
	}

	vmContainerCompare ( vmInstance *instance ) : doUser ( false ), instance ( instance ), cbFunc ( VAR_NULL {} )
	{}

	vmContainerCompare ( const vmContainerCompare &old ) : doUser ( old.doUser ), instance ( old.instance ), cbFunc ( old.cbFunc )
	{
	}

	bool operator() ( VAR *const &left, VAR *const &right ) const
	{
		if ( doUser )
		{
			bcFuncDef *func;
			VAR *stackSave;
			VAR *stack;

			stackSave = stack = instance->stack;

			*(instance->stack++) = *right;
			*(instance->stack++) = *left;

			switch ( cbFunc.type )
			{
				case slangType::eATOM:
					func = instance->atomTable->atomGetFunc ( cbFunc.dat.atom );
					if ( func->nParams != 2 )
					{
						throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
					}
					break;
				case slangType::eSTRING:
					func = instance->atomTable->atomGetFunc ( cbFunc.dat.str.c );
					if ( func->nParams != 2 )
					{
						throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
					}
					break;
				case slangType::eOBJECT_ROOT:
					{
						auto cbFuncClass = cbFunc.dat.ref.v()->dat.obj.classDef;

						if ( !cbFuncClass->ops[int(fgxOvOp::ovFuncCall)] )
						{
							throw errorNum::scINVALID_OPERATOR;
						}
						auto cEntry = &cbFuncClass->elements[cbFuncClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

						if ( cEntry->isVirtual )
						{
							func = instance->atomTable->atomGetFunc ( cbFunc.dat.ref.v()->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
						} else
						{
							func = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
						}

						if ( func->nParams != 3 )
						{
							throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
						}
						if ( cEntry->isVirtual )
						{
							// fixup object to point to the virtual context
							*(instance->stack++) = std::move ( VAR_OBJ ( const_cast<VAR *>(&cbFunc), &cbFunc.dat.ref.v[cbFunc.dat.ref.v()->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] ) );
						} else
						{
							*(instance->stack++) = std::move ( VAR_OBJ ( const_cast<VAR *>(&cbFunc), &cbFunc.dat.ref.v[cEntry->methodAccess.objectStart] ) );
						}
					}
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}

			switch ( func->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( func, 0, 3 );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( func->atom, 3 );
					break;
				default:
					// this should never happen, but just in case
					__assume (false);
					throw errorNum::scILLEGAL_OPERAND;
			}

			instance->stack = stackSave;
			VAR *tmpVar = &instance->result;
			while ( TYPE ( tmpVar ) == slangType::eREFERENCE )
			{
				tmpVar = tmpVar->dat.ref.v;
			}
			if ( TYPE ( tmpVar ) != slangType::eLONG )
			{
				throw errorNum::scINTERNAL;
			}
			return tmpVar->dat.l ? true : false;
		} else
		{
			VAR *lName = const_cast<VAR *>(left);
			VAR *rName = const_cast<VAR *>(right);
			int64_t	r;
			double	dTmp;

			while ( TYPE ( lName ) == slangType::eREFERENCE )
			{
				lName = lName->dat.ref.v;
			}

			while ( TYPE ( rName ) == slangType::eREFERENCE )
			{
				rName = rName->dat.ref.v;
			}

			switch ( TYPE ( lName ) )
			{
				case slangType::eLONG:
					switch ( TYPE ( rName ) )
					{
						case slangType::eLONG:
							r = lName->dat.l - rName->dat.l;
							break;
						case slangType::eDOUBLE:
							dTmp = lName->dat.l - rName->dat.d;
							r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
							break;
						case slangType::eSTRING:
							r = -1;
							break;
						default:
							throw errorNum::scINVALID_PARAMETER;
					}
					break;
				case slangType::eDOUBLE:
					switch ( TYPE ( rName ) )
					{
						case slangType::eLONG:
							dTmp = lName->dat.d - rName->dat.l;
							r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
							break;
						case slangType::eDOUBLE:
							dTmp = lName->dat.d - rName->dat.d;
							r = dTmp < 0 ? -1 : (dTmp > 0 ? 1 : 0);
							break;
						case slangType::eSTRING:
							r = -1;
							break;
						default:
							throw errorNum::scINVALID_PARAMETER;
					}
					break;
				case slangType::eSTRING:
					switch ( TYPE ( rName ) )
					{
						case slangType::eLONG:
						case slangType::eDOUBLE:
							r = 1;
							break;
						case slangType::eSTRING:
							r = strccmp ( lName->dat.str.c, rName->dat.str.c );
							break;
						default:
							throw errorNum::scINVALID_PARAMETER;
					}
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			if ( isOrdered )
			{
				return r < 0;
			} else
			{
				return r == 0;
			}
		}
	}
};

class hashClass
{
public:
	size_t operator () ( VAR *var ) const
	{
		while ( TYPE ( var ) == slangType::eREFERENCE ) var = var->dat.ref.v;

		switch ( TYPE ( var ) )
		{
			case slangType::eLONG:
				return var->dat.l;
			case slangType::eATOM:
				return var->dat.atom;
			case slangType::eDOUBLE:
				return *(size_t *) &var->dat.d;
			case slangType::eSTRING:
				{
					size_t hash = 14695981039346656037ull;
					for ( auto loop = 0; loop < var->dat.str.len; loop++ )
					{
						hash ^= var->dat.str.c[loop];
						hash *= 1099511628211;
					}
					return hash;
				}
				break;
			case slangType::eARRAY_ROOT:
			case slangType::eOBJECT_ROOT:
			case slangType::eCODEBLOCK_ROOT:
			default:
				throw errorNum::scINTERNAL;
		}
	}
};

#if !IS_ORDERED
static hashClass hash;
#endif

#if IS_ORDERED
#define params comp, alloc
#if IS_SET
typedef vmAllocator<VAR *>	allocType;
typedef base_type<VAR *, vmContainerCompare<IS_ORDERED>, allocType> containerType;
#else
typedef vmAllocator<std::pair<VAR *const, VAR *>> allocType;
typedef base_type<VAR *, VAR *, vmContainerCompare<IS_ORDERED>, allocType> containerType;
#endif
#else
#define params 0, hash, comp, alloc
#if IS_SET
typedef vmAllocator<VAR *>	allocType;
typedef base_type<VAR *, hashClass, vmContainerCompare<IS_ORDERED>, allocType> containerType;
#else
typedef vmAllocator<std::pair<VAR *const, VAR *>> allocType;
typedef base_type<VAR *, VAR *, hashClass, vmContainerCompare<IS_ORDERED>, allocType> containerType;
#endif
#endif

template<class T>
struct vmContainer
{
	vmContainerCompare<IS_ORDERED>	comp;
	allocType						alloc;
	T								m_container;

	vmContainer ( vmInstance *instance, size_t age ) : comp ( instance ), alloc ( instance, age ), m_container ( params )
	{}

	vmContainer ( vmInstance *instance, VAR *cbFunc, size_t age ) : comp ( instance, cbFunc ), alloc ( instance, age ), m_container ( params )
	{}

	vmContainer ( vmContainer const &old, vmCopyCB copy, objectMemory::copyVariables *copyVar ) : comp ( old.comp ), alloc ( old.alloc ), m_container ( params )
	{
		for ( auto &it : old.m_container )
		{
#if IS_SET
			m_container.insert ( copy ( comp.instance, it, true, copyVar ) );
#else
			m_container.insert ( { copy ( comp.instance, it.first, true, copyVar ), copy ( comp.instance, it.second, true, copyVar ) } );
#endif
		}
	}
};

#if IS_ORDERED
#define gcParams table->comp, table->alloc
#else
#define gcParams 0, hash, table->comp, table->alloc
#endif

template<typename T>
struct vmContainerEnumerator
{
	vmInstance				*instance;
	VAR_OBJ					 obj;
	VAR						 startIndex = 0;
	bool					 hasStartIndex = false;
	typename T::iterator	 it;
	int64_t					 generation = 0;
	int64_t					 pos = 0;

	vmContainerEnumerator ( vmInstance *instance, VAR_OBJ *obj ) : instance ( instance ), obj ( obj )
	{
		auto table = obj->getObj<vmContainer<containerType>> ( );

		it = table->m_container.begin ( );

		generation = (*obj)["$generation"]->dat.l;
		it = table->m_container.begin ( );
	}
	vmContainerEnumerator ( vmContainerEnumerator const &left ) : obj ( left.obj )
	{
		instance = left.instance;
		startIndex = left.startIndex;
		hasStartIndex = left.hasStartIndex;
		it = left.it;
		generation = left.generation;
		pos = left.pos;
	}
};

// TODO: replace all the #if crap with templates/concepts and costexpr
static void cContainerGcCb ( class vmInstance *instance, VAR_OBJ *val, objectMemory::collectionVariables *col )
{
	auto oldTable = val->getObj<vmContainer<containerType>> ( );
	if ( !oldTable ) return;

	if ( col->doCopy ( oldTable ) )
	{
		oldTable->alloc.set ( col->getAge ( ) );
		instance->om->move ( instance, nullptr, &oldTable->comp.cbFunc, true, col );
		auto table = val->makeObj< vmContainer<containerType>> ( col->getAge (), instance, instance, &oldTable->comp.cbFunc, col->getAge () );

		for ( auto &it : oldTable->m_container )
		{
#if IS_SET
#if IS_MULTI
			auto newIt = table->m_container.insert ( it );
#else
			auto [newIt, ins] = table->m_container.insert ( it );
#endif
			*const_cast<VAR **>(&*newIt) = instance->om->move ( instance, const_cast<VAR **>(&*newIt), *newIt, true, col );
#else
#if IS_MULTI
			auto newIt = table->m_container.insert ( { it.first, it.second } );
#else
			auto [newIt, ins] = table->m_container.insert ( { it.first, it.second } );
#endif
			*const_cast<VAR **>(&newIt->first) = instance->om->move ( instance, const_cast<VAR **>(&newIt->first), newIt->first, false, col );
			*const_cast<VAR **>(&newIt->second) = instance->om->move ( instance, const_cast<VAR **>(&newIt->second), newIt->second , false, col );
#endif
		}
	} else
	{
		for ( auto &it : oldTable->m_container )
		{
#if IS_SET
			*const_cast<VAR **>(&it) = instance->om->move ( instance, const_cast<VAR **>(&it), it , true, col );
#else
			*const_cast<VAR **>(&it.first) = instance->om->move ( instance, const_cast<VAR **>(&it.first), it.first , false, col );
			*const_cast<VAR **>(&it.second) = instance->om->move ( instance, const_cast<VAR **>(&it.second), it.second, false, col );
#endif
		}
	}
}

static void cContainerCopyCb ( class vmInstance *instance, VAR *val, VAR *(*copy)(vmInstance *instance, VAR *val, bool copy, objectMemory::copyVariables *copyVar), objectMemory::copyVariables *copyVar )
{
	auto oldTable = VAR_OBJ ( val ).getObj<vmContainer<containerType>> ( );
	if ( !oldTable ) return;

	auto table = VAR_OBJ ( val ).makeObj<vmContainer<containerType>> ( instance, *oldTable, copy, copyVar );

	return;
}

//******************  pack routines

static void cContainerPackCb ( class vmInstance *instance, VAR *val, BUFFER *buff, void *param, void (*packCB) (VAR *var, void *param) )
{
	auto container = (vmContainer<containerType> *) classGetCargo ( val );

	bufferPut64 ( buff, (uint32_t) container->m_container.size ( ) );

	for ( auto &it : container->m_container )
	{
#if IS_SET
		packCB ( it, param );
#else
		packCB ( it.first, param );
		packCB ( it.second, param );
#endif
	}
}

static void cContainerUnPackCB ( class vmInstance *instance, struct VAR *val, unsigned char **buff, uint64_t *len, void *param, struct VAR *(*unPack)(unsigned char **buff, uint64_t *len, void *param) )
{
	VAR *index;
	VAR *data;
	uint64_t		 count;

	auto table = (vmContainer<containerType> *) classGetCargo ( val );

	index = 0;
	data = 0;

	*buff += sizeof ( unsigned long );
	*len -= sizeof ( unsigned long );

	count = *((uint64_t *) *buff);
	*buff += sizeof ( uint64_t );
	*len -= sizeof ( uint64_t );

	for ( ; count; count-- )
	{
		data = 0;
		index = 0;

#if IS_SET
		try
		{
			data = unPack ( buff, len, param );
		} catch ( ... )
		{
			throw errorNum::scINTERNAL;
		}

		table->m_container.insert ( data );
#else
		try
		{
			index = unPack ( buff, len, param );
			data = unPack ( buff, len, param );
		} catch ( ... )
		{
			throw errorNum::scINTERNAL;
		}

		table->m_container.insert ( std::make_pair ( index, data ) );
#endif
	}

	classIVarAccess ( val, "$generation" )->dat.l++;
}

static void cContainerNew ( class vmInstance *instance, VAR_OBJ *obj, VAR *p1 )
{
	vmContainer<containerType> *table;

	while ( TYPE ( p1 ) == slangType::eREFERENCE ) p1 = p1->dat.ref.v;

	switch ( TYPE ( p1 ) )
	{
		case slangType::eATOM:
		case slangType::eSTRING:
			table = obj->makeObj< vmContainer<containerType>> ( instance, instance, p1, 0 );
			break;
		case slangType::eOBJECT_ROOT:
			if ( p1->dat.ref.v )
			{
				table = obj->makeObj< vmContainer<containerType>> ( instance, instance, p1, 0 );
			} else
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eNULL:
			table = obj->makeObj< vmContainer<containerType>> ( instance, instance, 0 );
			break;
		default:
			// TODO: support this
			throw errorNum::scUNSUPPORTED;
	}

	*((*obj)["$generation"]) = VAR ( 0 );
}

#if !IS_MULTI
static VAR_REF cContainerAccess ( class vmInstance *instance, VAR_OBJ *obj, VAR *index )
{
	auto table = obj->getObj<vmContainer<containerType>> ( );
	auto it = table->m_container.find ( index );

	if ( it == table->m_container.end ( ) )
	{
		throw errorNum::scNOT_FOUND;
	}

#if IS_SET
	return VAR_REF ( obj, *it );
#else
	return VAR_REF ( obj, it->second );
#endif
}
#endif

static VAR_OBJ_TYPE< MAKE_NAME ( CONTAINER_NAME ) "Enumerator">  cContainerFind ( vmInstance *instance, VAR_OBJ *object, VAR *index )
{
	PUSH ( *object );
	auto res = *classNew ( instance, MAKE_NAME ( CONTAINER_NAME ) "Enumerator", 2 );
	POP ( 1 );

	auto e = reinterpret_cast<VAR_OBJ *>(&res)->getObj<vmContainerEnumerator<containerType>> ( );
	auto table = e->obj.getObj<vmContainer<containerType>> ( );

#if IS_MULTI
	e->startIndex = *index;
	e->hasStartIndex = true;
#endif
	e->it = table->m_container.find ( index );

	return VAR_OBJ( res );
}

#if IS_SET
static VAR cContainerInsert ( class vmInstance *instance, VAR_OBJ *obj, VAR *value )
{
	auto table = obj->getObj<vmContainer<containerType>> ( );

	classIVarAccess ( obj, "$generation" )->dat.l++;

#if IS_MULTI
	auto newValue = instance->om->allocVar ( 1 );
	*newValue = *value;

	table->m_container.insert ( newValue );
#else
	auto it = table->m_container.find ( value );
	if ( it == table->m_container.end ( ) )
	{
		auto newValue = instance->om->allocVar ( 1 );
		*newValue = *value;

		table->m_container.insert ( newValue );
	} else
	{
		auto newValue = instance->om->allocVar ( 1 );
		*newValue = *value;

		**it = *newValue;
	}
#endif

	return *value;
}
#else
static VAR cContainerInsert ( class vmInstance *instance, VAR_OBJ *obj, VAR *index, VAR *value )
{
	auto table = obj->getObj<vmContainer<containerType>> ( );

	classIVarAccess ( obj, "$generation" )->dat.l++;

#if IS_MULTI
	auto newValue = instance->om->allocVar ( 1 );
	*newValue = *value;
	auto newIndex = instance->om->allocVar ( 1 );
	*newIndex = *index;

	table->m_container.insert ( std::make_pair ( newIndex, newValue ) );
#else
	auto it = table->m_container.find ( index );
	if ( it == table->m_container.end ( ) )
	{
		auto newValue = instance->om->allocVar ( 1 );
		*newValue = *value;
		auto newIndex = instance->om->allocVar ( 1 );
		*newIndex = *index;

		table->m_container.insert ( std::make_pair ( newIndex, newValue ) );
	} else
	{
		auto newValue = instance->om->allocVar ( 1 );
		*newValue = *value;
		*it->second = *newValue;
	}
#endif
	return *value;
}
#endif

static int64_t cContainerIsPresent ( class vmInstance *instance, VAR_OBJ *obj, VAR *index )
{
	auto table = obj->getObj<vmContainer<containerType>> ( );
	auto it = table->m_container.find ( index );

	if ( it == table->m_container.end ( ) )
	{
		return 0;
	}

	return 1;
}

static int64_t cContainerDelete ( class vmInstance *instance, VAR_OBJ *obj, VAR *index )
{
	auto table = obj->getObj<vmContainer<containerType>> ( );
	auto it = table->m_container.find ( index );

	if ( it == table->m_container.end ( ) )
	{
		return 0;
	}

	table->m_container.erase ( it );

	classIVarAccess ( obj, "$generation" )->dat.l++;
	return 1;
}

static size_t cContainerLen ( class vmInstance *instance, VAR_OBJ *obj )
{
	return obj->getObj<vmContainer<containerType>> ( )->m_container.size ( );
}

static vmInspectList *cContainerInspector ( class vmInstance *instance, bcFuncDef *func, VAR *obj, uint64_t  start, uint64_t  end )
{
	auto table = static_cast<VAR_OBJ *>(obj)->getObj<vmContainer<containerType>> ( );
	auto vars = new vmInspectList ( );

	for ( auto it : table->m_container )
	{
#if IS_SET
#if IS_MULTI
		auto var = it;
#else
		auto var = it;
#endif
		while ( TYPE ( var ) == slangType::eREFERENCE ) var = var->dat.ref.v;

		switch ( TYPE ( var ) )
		{
			case slangType::eSTRING:
				vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, var->dat.str.c, it )) );
				break;
			case slangType::eLONG:
				vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, var->dat.l, it )) );
				break;
			case slangType::eDOUBLE:
				vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, var->dat.d, it )) );
				break;
			default:
				break;
		}
#else
		auto index = it.first;
		while ( TYPE ( index ) == slangType::eREFERENCE ) index = index->dat.ref.v;

		switch ( TYPE ( index ) )
		{
			case slangType::eSTRING:
				vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, index->dat.str.c, it.second )) );
				break;
			case slangType::eLONG:
				vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, index->dat.l, it.second )) );
				break;
			case slangType::eDOUBLE:
				vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, index->dat.d, it.second )) );
				break;
			default:
				break;
		}
#endif
	}

	return vars;
}

static void cContainerAdd ( class vmInstance *instance, VAR_OBJ *obj, VAR *val )
{
	VAR *value;
#if !IS_SET
	VAR *index;
#endif

	instance->result = *obj;

	while ( TYPE ( val ) == slangType::eREFERENCE )
	{
		val = val->dat.ref.v;
	}

	if ( TYPE ( val ) != slangType::eARRAY_ROOT ) throw errorNum::scINVALID_PARAMETER;

	val = val->dat.ref.v;

	(*obj)["$generation"]->dat.l++;

	switch ( TYPE ( val ) )
	{
		case slangType::eARRAY_SPARSE:
			if ( val->dat.aSparse.v )
			{
				val = val->dat.aSparse.v;

				auto table = obj->getObj<vmContainer<containerType>> ( );
				while ( val )
				{
#if IS_SET
					auto newValue = instance->om->allocVar ( 1 );
					*newValue = *val->dat.aElem.var;

					table->m_container.insert ( newValue );
#else
					if ( !(index = (*val)[1]) )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					if ( !(value = (*val)[2]) )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					auto newValue = instance->om->allocVar ( 1 );
					*newValue = *value;
					auto newIndex = instance->om->allocVar ( 1 );
					*newIndex = *index;

					table->m_container.insert ( std::make_pair ( newIndex, newValue ) );
#endif

					val = val->dat.aElem.next;
				}
			}
			break;
		case slangType::eARRAY_FIXED:
			{
				auto table = obj->getObj<vmContainer<containerType>> ( );
				for ( auto indicie = val->dat.arrayFixed.startIndex; indicie <= val->dat.arrayFixed.endIndex; indicie++ )
				{
#if IS_SET
					if ( !(value = (*val)[indicie]) )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					auto newValue = instance->om->allocVar ( 1 );
					*newValue = *value;

					table->m_container.insert ( value );
#else
					if ( !(index = (*(*val)[indicie])[1]) )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					if ( !(value = (*(*val)[indicie])[2]) )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					auto newValue = instance->om->allocVar ( 1 );
					*newValue = *value;
					auto newIndex = instance->om->allocVar ( 1 );
					*newIndex = *index;

					table->m_container.insert ( std::make_pair ( newIndex, newValue ) );
#endif
				}
			}
			break;
	}
}

static VAR_OBJ_TYPE< MAKE_NAME ( CONTAINER_NAME ) "Enumerator"> cContainerGetEnumerator ( vmInstance *instance, VAR_OBJ *object )
{
	PUSH ( *object );
	classNew ( instance, MAKE_NAME ( CONTAINER_NAME ) "Enumerator", 1 );
	POP ( 1 );
	return VAR_OBJ(instance->result);
}

static void containerEnumeratorNew ( vmInstance *instance, VAR_OBJ *obj, VAR_OBJ *arr )
{
	auto vmEnum = obj->makeObj<vmContainerEnumerator<containerType>> ( instance, instance, arr );
}

static void containerEnumeratorGcCb ( class vmInstance *instance, VAR_OBJ *val, objectMemory::collectionVariables *col )
{
	auto enumOld = val->getObj<vmContainerEnumerator<containerType>> ( );
	auto table = enumOld->obj.getObj<vmContainer<containerType>> ();

	if ( col->doCopy ( enumOld ) || col->doCopy ( table ) )
	{
		instance->om->move ( instance, nullptr, &enumOld->obj, false, col );

		auto enumNew = val->makeObj<vmContainerEnumerator<containerType>> ( col->getAge ( ), instance, *enumOld );
		auto table = enumNew->obj.getObj<vmContainer<containerType>> ( );

#if IS_MULTI
		// best we can do for multi entries as we can't just find...  bummer.
		if ( enumNew->hasStartIndex )
		{
			instance->om->move ( instance, nullptr, &enumNew->startIndex, false, col );
			enumNew->it = table->m_container.find ( &enumNew->startIndex );
		} else
		{
			enumNew->it = table->m_container.begin ( );
		}
		auto pos = enumNew->pos;
		if ( pos ) pos--;
		for ( ; pos; pos-- )
		{
			enumNew->it++;
		}
#else
#if IS_SET
		enumNew->it = table->m_container.find ( *enumOld->it );
#else
		enumNew->it = table->m_container.find ( enumOld->it->first );
#endif
#endif
	}
}

static void containerEnumeratorCopyCb ( class vmInstance *instance, VAR *val, VAR *(*omCopy)(vmInstance *instance, VAR *val, bool copy, objectMemory::copyVariables *copyVar), objectMemory::copyVariables *copyVar )
{
	auto vmEnum = (vmContainerEnumerator<containerType> *) classAllocateCargo ( instance, val, sizeof ( vmContainerEnumerator<containerType> ) );

	auto e = (vmContainerEnumerator<containerType> *) classGetCargo ( val );
	*vmEnum = *e;
}

static VAR_OBJ_TYPE< MAKE_NAME ( CONTAINER_NAME ) "Enumerator"> containerEnumeratorGetEnumerator ( vmInstance *instance, VAR_OBJ *object )
{
	return *object;
}

static VAR containerEnumeratorCurrent ( vmInstance *instance, VAR_OBJ *object )
{
	auto e = object->getObj<vmContainerEnumerator<containerType>> ( );

#if IS_SET
	return **(e->it);
#else
	return *e->it->second;
#endif
}

#if !IS_SET
static VAR containerEnumeratorIndex ( vmInstance *instance, VAR_OBJ *object )
{
	auto e = object->getObj<vmContainerEnumerator<containerType>> ( );

	return *e->it->first;
}
#endif

static void containerEnumeratorReset ( vmInstance *instance, VAR_OBJ *object )
{
	auto e = object->getObj<vmContainerEnumerator<containerType>> ( );
	auto table = e->obj.getObj< vmContainer<containerType>> ( );

#if IS_MULTI
	if ( e->hasStartIndex )
	{
		e->it = table->m_container.find ( &e->startIndex );
	} else
	{
		e->it = table->m_container.begin ( );
	}
#else
		e->it = table->m_container.begin ( );
#endif
	e->pos = 0;
}

static bool  containerEnumeratorMoveNext ( vmInstance *instance, VAR_OBJ *object )
{
	auto e = object->getObj<vmContainerEnumerator<containerType>> ( );
	auto table = e->obj.getObj<vmContainer<containerType>> ( );

	if ( e->pos )
	{
		e->it++;
	}
	e->pos++;

	if ( e->it == table->m_container.end ( ) )
	{
		return false;
	}
	return true;
}