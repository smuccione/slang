/*

   Object/Class support - SLANG compiler

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "bcInterpreter/op_variant.h"

VAR_OBJ *classNew ( class vmInstance *instance, char const *name, int nParams )
{
	VAR					 *stack;
	VAR					 *stackSave;
	uint32_t			  callerAtom;
	uint32_t			  callerNParam;
	bcFuncDef			 *callerFDef;
	bcClass				 *callerClass;
	VAR					 *varTmp;
	VAR_OBJ				 *obj;

	nParams++;		// for the object we're creating;
	stack = stackSave = instance->stack;

	callerClass = instance->atomTable->atomGetClass ( name );

	obj = init_obj( instance, callerClass );

	varTmp			= stack;
	stack->type		= slangType::eOBJECT_ROOT;
	stack->dat.ref.v	= obj;
	stack->dat.ref.obj  = obj;
	stack++;

	if ( callerClass->newEntry )
	{
		callerAtom = callerClass->newEntry->methodAccess.atom;

		callerNParam = nParams;

		callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

		// build the parameter list

		// any NULL (not passed parameters... build that now
		for ( size_t pIndex = callerFDef->nParams; pIndex > nParams; pIndex-- )
		{

			if ( callerFDef->defaultParams && callerFDef->defaultParams[pIndex - 1] )
			{
				stack = instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex - 1], 0, stack, stack );
			} else
			{
				(stack++)->type = slangType::eNULL;
			}
		}
		memcpy ( stack, stackSave - (nParams - 1), sizeof ( VAR ) * (nParams -1) );
		stack += nParams - 1;

		// push self
		*(stack++) = *varTmp;

		callerNParam = callerFDef->nParams > callerNParam + 1 ? callerFDef->nParams : callerNParam + 1;		// self + default + callerNParam

		// now call the new function
		instance->stack = stack;
		switch ( callerFDef->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				instance->interpretBC ( callerFDef, 0, callerNParam );			// ops is the return address
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				instance->funcCall ( callerAtom, callerNParam );
				break;
			default:
				throw errorNum::scINTERNAL;
		}

		instance->stack = stackSave;
	}

	// register the destructor since we've succeded in creating the object
	if ( callerClass->releaseEntry )
	{
		instance->om->registerDestructor ( (instance->stack - 1)->dat.ref.v );
	}

	instance->result = *varTmp;
	return (VAR_OBJ *)&instance->result;
}

VAR	*classIVarAccess  ( VAR *obj, char const *name )
{
	bcClassSearchEntry	*cEntry;

	while ( TYPE ( obj ) == slangType::eREFERENCE ) obj = obj->dat.ref.v;

	if ( TYPE ( obj ) != slangType::eOBJECT_ROOT )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	cEntry = findClassEntry ( obj->dat.ref.v->dat.obj.classDef, const_cast<char *>(name), fgxClassElemSearchType::fgxClassElemPrivateAccess );
	if ( cEntry )
	{
		switch ( cEntry->type )
		{
			case fgxClassElementType::fgxClassType_iVar:
				return &obj->dat.ref.v[cEntry->methodAccess.iVarIndex];
			case fgxClassElementType::fgxClassType_static:
				return obj->dat.ref.v->dat.obj.classDef->globalsBase[cEntry->methodAccess.globalEntry];
			default:
				return 0;
		}
	}
	return 0;
}

bool classCallMethod ( class vmInstance *instance, VAR *obj, char const *name, int nParams )
{
	VAR					*stack;
	uint32_t			 callerAtom;
	bcFuncDef			*callerFDef;
	bcClassSearchEntry	*cEntry;

	stack = instance->stack;

	while ( TYPE ( obj ) == slangType::eREFERENCE ) obj = obj->dat.ref.v;

	if ( obj->type != slangType::eOBJECT_ROOT )
	{
		return false;
	}

	cEntry = findClassEntry ( obj->dat.ref.v->dat.obj.classDef, const_cast<char *>(name), fgxClassElemSearchType::fgxClassElemPublicMethod );
	if ( cEntry )
	{
		if ( cEntry->type == fgxClassElementType::fgxClassType_method )
		{
			if ( cEntry->isVirtual )
			{
				callerAtom = obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
			} else
			{
				callerAtom = cEntry->methodAccess.atom;
			}

			callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

			if ( nParams != callerFDef->nParams - 1 )
			{
				return false;
			}
			// build the parameter list

			// push object
			if ( cEntry->isVirtual )
			{
				*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
			} else
			{
				*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->methodAccess.objectStart] );
			}

			// now call the new function
			instance->stack = stack;
			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, 0, nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerAtom, nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			return true;
		}
	}
	return false;
}

bool classCallRelease ( class vmInstance *instance, VAR *obj )
{
	VAR					*stack;
	uint32_t			 callerAtom;
	bcFuncDef			*callerFDef;
	bcClass				*callerClass;
	bcClassSearchEntry	*cEntry;

	stack = instance->stack;

	if ( obj->type != slangType::eOBJECT )
	{
		// already been released
		return false;
	}

	callerClass = obj->dat.obj.classDef;

	cEntry = callerClass->releaseEntry;

	if ( !cEntry )
	{
		return false;
	}

	if ( cEntry->isVirtual )
	{
		callerAtom = obj->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
	} else
	{
		callerAtom = cEntry->methodAccess.atom;
	}

	callerFDef	= instance->atomTable->atomGetFunc ( callerAtom );

	// push object
	if ( cEntry->isVirtual )
	{
		*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
	} else
	{
		*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->methodAccess.objectStart] );
	}

	// now call the destructor
	instance->stack = stack;
	switch (  callerFDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( callerFDef, 0, callerFDef->nParams );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( callerAtom, 0 );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
	return true;
}

bool classCallPack ( class vmInstance *instance, VAR *obj )
{
	VAR					*stack;
	uint32_t				 callerAtom;
	bcFuncDef			*callerFDef;
	bcClass				*callerClass;
	bcClassSearchEntry	*cEntry;

	stack = instance->stack;

	while ( TYPE ( obj ) == slangType::eREFERENCE ) obj = obj->dat.ref.v;

	if ( obj->type == slangType::eOBJECT_ROOT )
	{
		obj = obj->dat.ref.v;
	}
	if ( obj->type != slangType::eOBJECT )
	{
		return false;
	}

	callerClass = obj->dat.obj.classDef;

	cEntry = callerClass->packEntry;

	if ( !cEntry )
	{
		return false;
	}

	if ( cEntry->isVirtual )
	{
		callerAtom = obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
	} else
	{
		callerAtom = cEntry->methodAccess.atom;
	}

	callerFDef	= instance->atomTable->atomGetFunc ( callerAtom );

	// push object
	if ( cEntry->isVirtual )
	{
		*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
	} else
	{
		*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->methodAccess.objectStart] );
	}

	// now call the new function
	instance->stack = stack;
	switch (  callerFDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( callerFDef, 0, callerFDef->nParams );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( callerAtom, 0 );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
	return true;
}

bool classCallUnpack ( class vmInstance *instance, VAR *obj )
{
	VAR					*stack;
	uint32_t			 callerAtom;
	bcFuncDef			*callerFDef;
	bcClass				*callerClass;
	bcClassSearchEntry	*cEntry;

	stack = instance->stack;

	while ( TYPE ( obj ) == slangType::eREFERENCE ) obj = obj->dat.ref.v;

	if ( obj->type == slangType::eOBJECT_ROOT )
	{
		obj = obj->dat.ref.v;
	}
	if ( obj->type != slangType::eOBJECT )
	{
		return false;
	}

	callerClass = obj->dat.obj.classDef;

	cEntry = callerClass->unpackEntry;

	if ( !cEntry )
	{
		return false;
	}

	if ( cEntry->isVirtual )
	{
		callerAtom = obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
	} else
	{
		callerAtom = cEntry->methodAccess.atom;
	}
	callerFDef	= instance->atomTable->atomGetFunc ( callerAtom );

	// push object
	if ( cEntry->isVirtual )
	{
		*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
	} else
	{
		*(stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->methodAccess.objectStart] );
	}
	// now call the new function
	instance->stack = stack;
	switch (  callerFDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( callerFDef, 0, 0 );		
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( callerAtom, 0 );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
	return true;
}

void *classAllocateCargo ( class vmInstance *instance, VAR *var, size_t len )
{
	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}
	if ( TYPE ( var ) == slangType::eOBJECT_ROOT )
	{
		var = var->dat.ref.v;
	}
	assert ( TYPE ( var ) == slangType::eOBJECT );
	var->dat.obj.cargo = instance->om->allocGen ( sizeof ( uint8_t ) * (len + 16), instance->om->getAge ( var ) );
	*(size_t *) var->dat.obj.cargo  = sizeof ( uint8_t ) * len;

	return (uint8_t *) var->dat.obj.cargo + 16;
}

void *classGetCargo ( VAR *var )
{
	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}
	if ( TYPE ( var ) == slangType::eOBJECT_ROOT )
	{
		var = var->dat.ref.v;
	}
	assert ( var );
	assert ( TYPE ( var ) == slangType::eOBJECT );
	if ( var->dat.obj.cargo )
	{
		return (uint8_t *)var->dat.obj.cargo + 16;
	} else
	{
		return 0;
	}
}

size_t classGetCargoSize ( VAR *var )
{
	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}
	if ( TYPE ( var ) == slangType::eOBJECT_ROOT )
	{
		var = var->dat.ref.v;
	}
	assert ( var );
	assert ( TYPE ( var ) == slangType::eOBJECT );
	if ( var->dat.obj.cargo )
	{
		return *(size_t *) var->dat.obj.cargo;
	} else
	{
		return 0;
	}
}

