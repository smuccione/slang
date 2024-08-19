// unary variant operations

#include "stdlib.h"

#include "bcInterpreter.h"
#include "op_variant.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"
#include "bcVM/bcVM.h"
#include "bcVM/vmNativeInterface.h"

void op_pushValue ( class vmInstance* instance, fglOp* ops, char const *className, char const *message )
{
	bcClass				*callerClass;
	bcClassSearchEntry	*cEntry;

	callerClass = instance->atomTable->atomGetClass ( className );

	cEntry = findClassEntry ( callerClass, message, fgxClassElemSearchType::fgxClassElemPublicAccess);
	if ( !cEntry ) throw errorNum::scINVALID_ASSIGN;

	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			*(instance->stack) = *callerClass->globalsBase[cEntry->methodAccess.globalEntry];
			break;
		default:
			throw errorNum::scINVALID_ACCESS;
	}
}

void op_pushValueRef ( class vmInstance* instance, fglOp* ops, char const* className, char const* message )
{
	bcClass* callerClass;
	bcClassSearchEntry* cEntry;

	callerClass = instance->atomTable->atomGetClass ( className );

	cEntry = findClassEntry ( callerClass, message, fgxClassElemSearchType::fgxClassElemPublicAccess );
	if ( !cEntry ) throw errorNum::scINVALID_ASSIGN;

	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			(instance->stack) = VAR_REF ( callerClass->globalsBase[cEntry->methodAccess.globalEntry], callerClass->globalsBase[cEntry->methodAccess.globalEntry] );
			instance->stack++;
			break;
		default:
			throw errorNum::scINVALID_ACCESS;
	}
}

void op_storeValue ( class vmInstance* instance, fglOp* ops, char const* className, char const* message )
{
	bcClass				*callerClass;
	bcClassSearchEntry	*cEntry;
	VAR					*varTmp;

	callerClass = instance->atomTable->atomGetClass ( className );

	cEntry = findClassEntry ( callerClass, message, fgxClassElemSearchType::fgxClassElemPublicAssign );
	if ( !cEntry ) throw errorNum::scINVALID_ASSIGN;

	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_static:
			varTmp = callerClass->globalsBase[cEntry->methodAccess.globalEntry];
			while ( varTmp->type == slangType::eREFERENCE )
			{
				varTmp = varTmp->dat.ref.v;
			}
			if ( varTmp->type == slangType::eOBJECT_ROOT && varTmp->dat.ref.v->type != slangType::eNULL )
			{
				if ( varTmp->dat.ref.v->dat.obj.classDef->ops[int(fgxOvOp::ovAssign)] )
				{
					op_assign ( instance, ops, varTmp, instance->stack - 1 );
				} else
				{
					*callerClass->globalsBase[cEntry->methodAccess.globalEntry] = *(instance->stack - 1);
				}
			} else
			{
				*callerClass->globalsBase[cEntry->methodAccess.globalEntry] = *(instance->stack - 1);
			}
			break;
		default:
			throw errorNum::scINVALID_ASSIGN;
	}
}

void op_objStore ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, VAR *basePtr, uint8_t const *dsBase )
{
	VAR					*varTmp;
	bcClass				*callerClass;
	bcClassSearchEntry	*cEntry;
	bcFuncDef			*callerFDef;
	int					 callerNParam;

	// value
	// object

	varTmp = instance->stack - 1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE )
	{
		varTmp = varTmp->dat.ref.v;
		*(instance->stack - 1) = *varTmp;
	}

	if ( varTmp->type != slangType::eOBJECT_ROOT || varTmp->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}

	assert ( varTmp->dat.ref.v->type == slangType::eOBJECT );

	callerClass = varTmp->dat.ref.v->dat.obj.classDef;
	callerNParam = 2;

	if ( ((bcObjQuick *)&(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->classDef == callerClass )
	{
		// cEntry CAN be null... it indicates that we need to use default assign
		cEntry = ((bcObjQuick *)& (funcDef->loadImage->bssBase)[ops->imm.dual.val2])->cEntry;
	} else
	{
		cEntry = findClassEntry ( callerClass, (char *) (dsBase + ops->imm.index + sizeof ( uint32_t )), fgxClassElemSearchType::fgxClassElemPublicAssign );
		((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->cEntry = cEntry;
		((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->classDef = callerClass;
	}
	if ( !cEntry )
	{
		// need to make the stack
		// value
		// name
		// object
		*(instance->stack++) = *varTmp;
		varTmp = instance->stack - 1;
		(instance->stack - 2)->type = slangType::eSTRING;
		(instance->stack - 2)->dat.str.c = (char *) (dsBase + ops->imm.index + sizeof ( uint32_t ));
		(instance->stack - 2)->dat.str.len = *(uint32_t *) (dsBase + ops->imm.index);
		callerNParam++;
		cEntry = callerClass->defaultAssignEntry;
		if ( !cEntry ) throw errorNum::scINVALID_ASSIGN;
	}
	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_iVar:
			if ( cEntry->isVirtual )
			{
				varTmp->dat.ref.v[varTmp->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] = *(instance->stack - 2);
			} else
			{
				varTmp->dat.ref.v[cEntry->methodAccess.iVarIndex] = *(instance->stack - 2);
			}
			break;
		case fgxClassElementType::fgxClassType_prop:
			// modify object for correct contexxt
			if ( cEntry->assign.atom == -1 ) throw errorNum::scINVALID_ASSIGN;
			if ( cEntry->isVirtual )
			{
				*(instance->stack - 1) = VAR_OBJ ( varTmp, &varTmp->dat.ref.v[varTmp->dat.ref.v->dat.obj.vPtr[cEntry->assign.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( varTmp->dat.ref.v->dat.obj.vPtr[cEntry->assign.vTableEntry].atom );
			} else
			{
				*(instance->stack - 1) = VAR_OBJ ( varTmp, &varTmp->dat.ref.v[cEntry->assign.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->assign.atom );
			}			
			switch (  callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*(instance->stack - callerNParam) = instance->result;
			break;
		case fgxClassElementType::fgxClassType_static:
			*(callerClass->loadImage->globals)[cEntry->methodAccess.globalEntry] = *(instance->stack - 2);
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

void op_objStoreInd ( class vmInstance *instance, fglOp *ops, VAR *basePtr )
{
	VAR					*varTmp;
	VAR					*varTmp2;
	bcClass				*callerClass;
	bcClassSearchEntry	*cEntry;
	bcFuncDef			*callerFDef;
	int					 callerNParam;

	// value
	// message
	// object

	varTmp = instance->stack - 1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE )
	{
		varTmp = varTmp->dat.ref.v;
		*(instance->stack - 1) = *varTmp;
	}

	if ( varTmp->type != slangType::eOBJECT_ROOT || varTmp->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}

	varTmp2 = instance->stack - 2;
	while ( TYPE ( varTmp2 ) == slangType::eREFERENCE ) varTmp2 = varTmp2->dat.ref.v;

	if ( TYPE ( varTmp2 ) != slangType::eSTRING )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}

	callerClass		= varTmp->dat.ref.v->dat.obj.classDef;
	callerNParam	= 2;

	cEntry = findClassEntry ( callerClass, varTmp2->dat.str.c, fgxClassElemSearchType::fgxClassElemPublicAssign );
	if ( !cEntry )
	{
		callerNParam++;
		cEntry = callerClass->defaultAssignEntry;
	} else
	{
		// get rid of the message and move object where the message was
		*(instance->stack - 2) = *(instance->stack - 1);
		instance->stack--;
		varTmp = instance->stack - 1;
	}
	if ( !cEntry ) throw errorNum::scINVALID_ASSIGN;
	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_iVar:
			if ( cEntry->isVirtual )
			{
				varTmp->dat.ref.v[varTmp->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] = *(instance->stack - 2);
			} else
			{
				varTmp->dat.ref.v[cEntry->methodAccess.iVarIndex] = *(instance->stack - 2);
			}
			break;
		case fgxClassElementType::fgxClassType_prop:
			if ( cEntry->assign.atom == -1 ) throw errorNum::scINVALID_ASSIGN;
			// modify object for correct contexxt
			if ( cEntry->isVirtual )
			{
				*(instance->stack - 1) = VAR_OBJ ( varTmp, &varTmp->dat.ref.v[varTmp->dat.ref.v->dat.obj.vPtr[cEntry->assign.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( varTmp->dat.ref.v->dat.obj.vPtr[cEntry->assign.vTableEntry].atom );
			} else
			{
				*(instance->stack - 1) = VAR_OBJ ( varTmp, &varTmp->dat.ref.v[cEntry->assign.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->assign.atom );
			}			
			switch (  callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*(instance->stack - callerNParam) = instance->result;
			break;
		case fgxClassElementType::fgxClassType_static:
			*(callerClass->loadImage->globals)[cEntry->methodAccess.globalEntry] = *(instance->stack - 1);
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

static void __inline op_accessObj ( class vmInstance *instance, bcClass *callerClass, bcClassSearchEntry *cEntry, fglOp *retAddr, VAR *obj, VAR *basePtr, VAR *destAddr, bool safeCalls )
{
	bcFuncDef			*callerFDef;

	// message (if default)
	// object
	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_inherit:
			if ( cEntry->isVirtual )
			{
				*(instance->stack - 1) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
			} else
			{
				*(instance->stack - 1) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->methodAccess.iVarIndex] );
			}
			break;
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			*(instance->stack-1) = *(callerClass->loadImage->globals)[cEntry->methodAccess.globalEntry];
			break;
		case fgxClassElementType::fgxClassType_iVar:
			if ( cEntry->isVirtual )
			{
				*(instance->stack - 1) = obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta];
			} else
			{
				*(instance->stack - 1) = obj->dat.ref.v[cEntry->methodAccess.iVarIndex];
			}
			break;
		case fgxClassElementType::fgxClassType_prop:
			if ( cEntry->methodAccess.atom == -1 ) throw errorNum::scINVALID_ACCESS;
			if ( cEntry->isVirtual )
			{
				*(instance->stack - 1) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->assign.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack - 1) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->assign.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}
			if ( safeCalls && !callerFDef->isConst )
			{
				throw errorNum::scSAFE_CALL;
			}
			switch (  callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, retAddr + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*destAddr = instance->result;
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

void op_accessObj ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, VAR *basePtr, uint8_t const *dsBase )
{
	auto destAddr = instance->stack - 1;

	// object
	auto varTmp = instance->stack - 1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE )
	{
		varTmp = varTmp->dat.ref.v;
		*(instance->stack - 1) = *varTmp;
	}

	if ( varTmp->type != slangType::eOBJECT_ROOT || varTmp->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}

	assert ( varTmp->dat.ref.v->type == slangType::eOBJECT );

	auto callerClass = varTmp->dat.ref.v->dat.obj.classDef;

	bcClassSearchEntry	*cEntry;
	if ( ((bcObjQuick *)&(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->classDef == callerClass )
	{
		// cEntry CAN be null... indicates that we need to use default accessor
		cEntry = ((bcObjQuick *)& (funcDef->loadImage->bssBase)[ops->imm.dual.val2])->cEntry;
	} else
	{
		cEntry = findClassEntry ( callerClass, (char *) (dsBase + ops->imm.index + sizeof ( uint32_t )), fgxClassElemSearchType::fgxClassElemPublicAccess );
		((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->cEntry = cEntry;
		((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->classDef = callerClass;
	}
	if ( !cEntry )
	{
		cEntry = callerClass->defaultAccessEntry;
		if ( !cEntry ) throw errorNum::scINVALID_ACCESS;

		// duplicate the object
		*(instance->stack++) = *varTmp;
		varTmp = instance->stack - 1;
		// save the message name in the right spot on the instance->stack
		(instance->stack - 2)->type = slangType::eSTRING;
		(instance->stack - 2)->dat.str.c = (char *) (dsBase + ops->imm.index + sizeof ( uint32_t ));
		(instance->stack - 2)->dat.str.len = *(uint32_t *) (dsBase + ops->imm.index);
	}

	op_accessObj ( instance, callerClass, cEntry, ops, varTmp, basePtr, destAddr, false );

	while ( destAddr->type == slangType::eREFERENCE ) *destAddr = *(destAddr->dat.ref.v);

	instance->stack = destAddr + 1;
}

void op_accessObjInd ( class vmInstance *instance, fglOp *ops, VAR *basePtr )
{
	auto destAddr = instance->stack - 2;

	// message
	// object

	auto varTmp = instance->stack - 1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE )
	{
		varTmp = varTmp->dat.ref.v;
		*(instance->stack - 1) = *varTmp;		// making sure we're dereferenced on the instance->stack since the acutall call requires an OBJECT not a reference to it
	}

	if ( varTmp->type != slangType::eOBJECT_ROOT || varTmp->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}

	assert ( varTmp->dat.ref.v->type == slangType::eOBJECT );

	auto varTmp2 = instance->stack - 2;
	while ( TYPE ( varTmp2 ) == slangType::eREFERENCE ) varTmp2 = varTmp2->dat.ref.v;

	if ( TYPE ( varTmp2 ) != slangType::eSTRING )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}

	auto callerClass = varTmp->dat.ref.v->dat.obj.classDef;

	auto cEntry = findClassEntry ( callerClass, varTmp2->dat.str.c, fgxClassElemSearchType::fgxClassElemPublicAccess );
	if ( !cEntry ) 
	{
		// leave message where it is, we'll need it
		cEntry = callerClass->defaultAccessEntry;
	} else
	{
		// get rid of the message and move object where the message was
		*(instance->stack - 2) = *(instance->stack - 1);
		instance->stack--;
		varTmp = instance->stack - 1;
	}
	if ( !cEntry ) throw errorNum::scINVALID_ACCESS;

	op_accessObj ( instance, callerClass, cEntry, ops, varTmp, basePtr, destAddr, false );

	while ( destAddr->type == slangType::eREFERENCE ) *destAddr = *(destAddr->dat.ref.v);

	instance->stack = destAddr + 1;
}

void op_accessObjIndSafe ( class vmInstance *instance, fglOp *ops, VAR *basePtr )
{
	auto destAddr = instance->stack - 2;

	// message
	// object

	auto varTmp = instance->stack - 1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE )
	{
		varTmp = varTmp->dat.ref.v;
		*(instance->stack - 1) = *varTmp;		// making sure we're dereferenced on the instance->stack since the acutall call requires an OBJECT not a reference to it
	}

	if ( varTmp->type != slangType::eOBJECT_ROOT || varTmp->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}

	assert ( varTmp->dat.ref.v->type == slangType::eOBJECT );

	auto varTmp2 = instance->stack - 2;
	while ( TYPE ( varTmp2 ) == slangType::eREFERENCE ) varTmp2 = varTmp2->dat.ref.v;

	if ( TYPE ( varTmp2 ) != slangType::eSTRING )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}

	auto callerClass = varTmp->dat.ref.v->dat.obj.classDef;

	auto cEntry = findClassEntry ( callerClass, varTmp2->dat.str.c, fgxClassElemSearchType::fgxClassElemPublicAccess );
	if ( !cEntry )
	{
		// leave message where it is, we'll need it
		cEntry = callerClass->defaultAccessEntry;
	} else
	{
		// get rid of the message and move object where the message was
		*(instance->stack - 2) = *(instance->stack - 1);
		instance->stack--;
		varTmp = instance->stack - 1;
	}
	if ( !cEntry ) throw errorNum::scINVALID_ACCESS;

	op_accessObj ( instance, callerClass, cEntry, ops, varTmp, basePtr, destAddr, true );

	while ( destAddr->type == slangType::eREFERENCE ) *destAddr = *(destAddr->dat.ref.v);

	instance->stack = destAddr + 1;
}

void op_accessObjRef ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, VAR *basePtr, uint8_t const *dsBase )
{
	auto destAddr = instance->stack - 1;

	auto varTmp = instance->stack - 1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE )
	{
		varTmp = varTmp->dat.ref.v;
		*(instance->stack - 1) = *varTmp;
	}

	if ( varTmp->type != slangType::eOBJECT_ROOT || varTmp->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}

	auto callerClass = varTmp->dat.ref.v->dat.obj.classDef;

	bcClassSearchEntry	*cEntry;
	if ( ((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->classDef == callerClass )
	{
		// cEntry CAN be null... this is OK... it indicates that we need to use default accessor
		cEntry = ((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->cEntry;
	} else
	{
		cEntry = findClassEntry ( callerClass, (char *) (dsBase + ops->imm.index + sizeof ( uint32_t )), fgxClassElemSearchType::fgxClassElemPublicAccess );
		((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->cEntry = cEntry;
		((bcObjQuick *) &(funcDef->loadImage->bssBase)[ops->imm.dual.val2])->classDef = callerClass;
	}
	if ( !cEntry )
	{
		cEntry = callerClass->defaultAccessEntry;
		if ( !cEntry ) throw errorNum::scINVALID_ACCESS;

		// duplicate the object
		*(instance->stack++) = *varTmp;
		varTmp = instance->stack - 1;
		// save the message name in the right spot on the instance->stack
		(instance->stack - 2)->type = slangType::eSTRING;
		(instance->stack - 2)->dat.str.c = (char *) (dsBase + ops->imm.index + sizeof ( uint32_t ));
		(instance->stack - 2)->dat.str.len = *(uint32_t *) (dsBase + ops->imm.index);
	}
	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_static:
			*destAddr = VAR_REF ( (callerClass->loadImage->globals)[cEntry->methodAccess.globalEntry], (callerClass->loadImage->globals)[cEntry->methodAccess.globalEntry] );
			return;
		case fgxClassElementType::fgxClassType_iVar:
			{
				GRIP g1 ( instance );
				if ( cEntry->isVirtual )
				{
					*destAddr = VAR_REF ( varTmp->dat.ref.v, &varTmp->dat.ref.v[varTmp->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				} else
				{
					*destAddr = VAR_REF ( varTmp->dat.ref.v, &varTmp->dat.ref.v[cEntry->methodAccess.iVarIndex] );
				}
			}
			return;
		case fgxClassElementType::fgxClassType_prop:
			op_accessObj ( instance, callerClass, cEntry, ops, varTmp, basePtr, destAddr, false );
			if ( destAddr->type != slangType::eREFERENCE ) throw errorNum::scINVALID_ASSIGN;
			return;
		default:
			throw errorNum::scINTERNAL;
	}
	instance->stack = destAddr + 1;
}

void op_accessObjRefInd ( class vmInstance *instance, fglOp *ops, VAR *basePtr )
{
	auto destAddr = instance->stack - 2;

	// message
	// object
	auto varTmp = instance->stack - 1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE )
	{
		varTmp = varTmp->dat.ref.v;
		*(instance->stack - 1) = *varTmp;
	}

	if ( varTmp->type != slangType::eOBJECT_ROOT || varTmp->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}

	auto varTmp2 = instance->stack - 2;
	while ( TYPE ( varTmp2 ) == slangType::eREFERENCE ) varTmp2 = varTmp2->dat.ref.v;

	if ( TYPE ( varTmp2 ) != slangType::eSTRING )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}

	auto callerClass = varTmp->dat.ref.v->dat.obj.classDef;

	auto cEntry = findClassEntry ( callerClass, varTmp2->dat.str.c, fgxClassElemSearchType::fgxClassElemPublicAccess );
	if ( !cEntry )
	{
		cEntry = callerClass->defaultAccessEntry;
		// leave the message be... we need it!
	} else
	{
		// get rid of the message and move object where the message was
		*(instance->stack - 2) = *(instance->stack - 1);
		instance->stack--;
		varTmp = instance->stack - 1;
	}
	if ( !cEntry ) throw errorNum::scINVALID_ASSIGN;
	switch ( cEntry->type )
	{
		case fgxClassElementType::fgxClassType_static:
			*destAddr = VAR_REF ( (callerClass->loadImage->globals)[cEntry->methodAccess.globalEntry], (callerClass->loadImage->globals)[cEntry->methodAccess.globalEntry] );
			break;
		case fgxClassElementType::fgxClassType_iVar:
			{
				GRIP g1 ( instance );
				if ( cEntry->isVirtual )
				{
					*destAddr = VAR_REF ( varTmp->dat.ref.v, &varTmp->dat.ref.v[varTmp->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				} else
				{
					*destAddr = VAR_REF ( varTmp->dat.ref.v, &varTmp->dat.ref.v[cEntry->methodAccess.iVarIndex] );
				}
			}
			break;
		case fgxClassElementType::fgxClassType_prop:
			op_accessObj ( instance, callerClass, cEntry, ops, varTmp, basePtr, destAddr, false );
			if ( destAddr->type != slangType::eREFERENCE ) throw errorNum::scINVALID_ASSIGN;
			break;
		default:
			throw errorNum::scINTERNAL;
	}
	instance->stack = destAddr + 1;
}

fglOp *op_objCall ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	auto destAddr = instance->stack - 2;
	auto prefix = &ops[-1];

	auto methodName = instance->stack - 2;	
	while ( TYPE ( methodName ) == slangType::eREFERENCE ) methodName = methodName->dat.ref.v;
	if ( TYPE ( methodName ) != slangType::eSTRING )
	{
		throw errorNum::scINVALID_MESSAGE;
	}

	auto caller = instance->stack - 1;
	auto callerNParam	= ops->imm.dual.val2;
	while ( TYPE ( caller ) == slangType::eREFERENCE ) caller = caller->dat.ref.v;

	if ( caller->type != slangType::eOBJECT_ROOT )
	{
		if ( instance->atomTable->atomGetType ( methodName->dat.str.c) == atom::atomType::aFUNCDEF )
		{
			// pseudo object call <param>.function()
			auto callerAtom = instance->atomTable->atomFind ( methodName->dat.str.c );
			auto callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

			// any NULL (not passed parameters... build that now
			for ( auto pIndex = callerFDef->nParams; pIndex > callerNParam + 1; pIndex-- )
			{
				if ( callerFDef->defaultParams && callerFDef->defaultParams[pIndex - 1] )
				{
					instance->stack = instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex - 1], 0, instance->stack, basePtr );
				} else
				{
					(instance->stack++)->type = slangType::eNULL;
				}
			}

			if ( callerNParam )
			{
				// build the passed parameters
				instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );
			}

			// note: neeed to push the REFERENCE since we may want to do (@a).xxx   we want the same as if we did xxx ( @a ) which means keep the caller intact
			*(instance->stack++) = *(destAddr + 1);

			callerNParam =  callerFDef->nParams > callerNParam + 1 ? callerFDef->nParams : callerNParam + 1;		// first param + default + callerNParam

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerAtom, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack = destAddr;
			return ops;
		}
		throw errorNum::scSEND_TO_NONOBJECT;
	}
	if ( caller->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}
	auto callerClass = caller->dat.ref.v->dat.obj.classDef;

	bcClassSearchEntry	*cEntry;
	if (((bcObjQuick *)&funcDef->loadImage->bssBase[prefix->imm.index])->classDef == callerClass)
	{
		cEntry = ((bcObjQuick *)&funcDef->loadImage->bssBase[prefix->imm.index])->cEntry;
	} else
	{
		cEntry = findClassEntry ( callerClass, methodName->dat.str.c, fgxClassElemSearchType::fgxClassElemAllPublic );
		((bcObjQuick *)&funcDef->loadImage->bssBase[prefix->imm.index])->cEntry = cEntry;
		((bcObjQuick *)&funcDef->loadImage->bssBase[prefix->imm.index])->classDef = callerClass;
	}
	if ( cEntry )
	{
		// check to see if we need to call throught the CONTENTS of the method.  e.g.   a.b = lambda; a.b()
		switch ( cEntry->type )
		{
			case fgxClassElementType::fgxClassType_method:
				break;
			case fgxClassElementType::fgxClassType_iVar:
			case fgxClassElementType::fgxClassType_static:
			case fgxClassElementType::fgxClassType_const:
			case fgxClassElementType::fgxClassType_prop:
				op_accessObj ( instance, callerClass, cEntry, ops, caller, basePtr, destAddr, false );
				ops = op_call ( instance, ops, funcDef, nParams, basePtr );
				instance->stack = destAddr + 1;	// op_call gets rid of the caller, but we need to also get rid of the message;
				return ops;
			default:
				throw errorNum::scUNKNOWN_METHOD;
		}
	}
	bcFuncDef *callerFDef;
	uint32_t   callerAtom;
	if ( !cEntry ) 
	{
		cEntry = callerClass->defaultMethodEntry;
		if ( !cEntry )
		{
			// check to see if we need to call throught the CONTENTS of the method.  e.g.   a.b = lambda; a.b()
			cEntry = callerClass->defaultAccessEntry;
			if ( !cEntry )
			{
				throw errorNum::scUNKNOWN_METHOD;
			}
			op_accessObj ( instance,  callerClass, cEntry, ops, caller, basePtr, destAddr, false );
			ops = op_call ( instance, ops, funcDef, nParams, basePtr );
			instance->stack = destAddr + 1;	// op_call gets rid of the caller, but we need to also get rid of the message;
			return ops;
		}

		if ( cEntry->isVirtual )
		{
			callerAtom = caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
		} else
		{
			callerAtom = cEntry->methodAccess.atom;
		}
		callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

		// build the parameter list

		// any NULL (not passed parameters... build that now)   NOTE we do NOT include self or messageName in this (hence the +2)... these are generated parameters, never passed
		for ( auto pIndex = callerFDef->nParams; pIndex > callerNParam + 2; pIndex-- )
		{
			// pindex is 1  based (e.g. number of parameters so subtract 1 to get to array offset of our default param)
			if ( callerFDef->defaultParams && callerFDef->defaultParams[pIndex - 1] )
			{
				instance->stack = instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex - 1], 0, instance->stack, basePtr );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}
		if ( callerNParam )
		{
			// build the passed parameters
			instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );
		}

		// message name
		*(instance->stack++) = *methodName;

		// compute the number of passed parameteers.  we use callerFDef but subtract two during the comparison because we dont "pass" self and messageName
		callerNParam = ( callerFDef->nParams - 2 > callerNParam) ? callerFDef->nParams - 2 : callerNParam;
		// we then add self and messageName back in since it'll be on the instance->stack (we push it ourself below)
		callerNParam += 2;
	} else
	{
		if ( cEntry->isVirtual )
		{
			callerAtom = caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
		} else
		{
			callerAtom = cEntry->methodAccess.atom;
		}
		callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

		// any NULL (not passed parameters... build that now)   NOTE we do NOT include self or messageName in this (hence the +2)... these are generated parameters, never passed
		for ( auto pIndex = callerFDef->nParams; pIndex > callerNParam + 1; pIndex-- )
		{
			// pindex is 1  based (e.g. number of parameters so subtract 1 to get to array offset of our default param)
			if ( callerFDef->defaultParams && callerFDef->defaultParams[pIndex - 1] )
			{
				instance->stack = instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex - 1], 0, instance->stack, basePtr );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}
		if ( callerNParam )
		{
			// build the passed parameters
			instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );
		}

		// compute the number of passed parameteers.  we use callerFDef but subtract one during the comparison because we dont "pass" self.
		callerNParam = ( callerFDef->nParams - 1 > callerNParam) ? callerFDef->nParams - 1 : callerNParam;
	}

	if ( callerFDef->isExtension )
	{
		*(instance->stack++) = *caller;
		// we then add self back in since it'll be on the instance->stack (we push it ourself below)
		callerNParam += 1;
	} else if ( !cEntry->isStatic )
	{
		// push hard context
		if ( cEntry->isVirtual )
		{
			*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
		} else
		{
			*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[cEntry->methodAccess.objectStart] );
		}
		// we then add self back in since it'll be on the instance->stack (we push it ourself below)
		callerNParam += 1;
	}

	// the address of our next op is right after our code/ret sequence for paramaeter building
	ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

	// now call the new function
	switch (  callerFDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( callerFDef, ops, callerNParam );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( callerAtom, callerNParam );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
	instance->stack = destAddr;
	return ops;
}

fglOp *op_objCallPPack ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	bool doDefault = false;
	auto destAddr = instance->stack - 2;

	auto methodName = instance->stack - 2;
	while ( TYPE( methodName ) == slangType::eREFERENCE ) methodName = methodName->dat.ref.v;
	if ( TYPE( methodName ) != slangType::eSTRING )
	{
		throw errorNum::scINVALID_MESSAGE;
	}

	auto caller = instance->stack - 1;
	auto callerNParam = ops->imm.dual.val2;
	while ( TYPE( caller ) == slangType::eREFERENCE ) caller = caller->dat.ref.v;

	if ( caller->type != slangType::eOBJECT_ROOT )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}
	if ( caller->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}
	auto callerClass = caller->dat.ref.v->dat.obj.classDef;

	auto cEntry = findClassEntry ( callerClass, methodName->dat.str.c, fgxClassElemSearchType::fgxClassElemAllPublic );
	if ( cEntry )
	{
		switch ( cEntry->type )
		{
			case fgxClassElementType::fgxClassType_method:
				break;
			case fgxClassElementType::fgxClassType_iVar:
			case fgxClassElementType::fgxClassType_const:
			case fgxClassElementType::fgxClassType_prop:
				op_accessObj( instance, callerClass, cEntry, ops, caller, basePtr, destAddr, false );
				ops = op_call( instance, ops, funcDef, nParams, basePtr );
				instance->stack = destAddr + 1;	// op_call gets rid of the caller, but we need to also get rid of the message;
				return ops;
			default:
				throw errorNum::scUNKNOWN_METHOD;
		}
	}

	// resolve our atom 
	if ( !cEntry )
	{
		cEntry = callerClass->defaultMethodEntry;
		if ( !cEntry )
		{
			cEntry = callerClass->defaultAccessEntry;
			if ( !cEntry )
			{
				throw errorNum::scUNKNOWN_METHOD;
			}
			op_accessObj ( instance, callerClass, cEntry, ops, caller, basePtr, destAddr, false );
			ops = op_call ( instance, ops, funcDef, nParams, basePtr );
			instance->stack = destAddr + 1;	// op_call gets rid of the caller, but we need to also get rid of the message;
			return ops;
		}
		doDefault = true;
	}

	uint32_t callerAtom;
	if ( cEntry->isVirtual )
	{
		callerAtom = caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
	} else
	{
		callerAtom = cEntry->methodAccess.atom;
	}

	// convert the atom into the function 
	auto callerFDef = instance->atomTable->atomGetFunc( callerAtom );

	// we have no idea how many actual parameters will be pushed... for safety we need to push sufficient NULL's to make up the difference
	for ( auto pIndex = callerFDef->nParams - 1; pIndex > callerNParam; pIndex-- )
	{
		(instance->stack++)->type = slangType::eNULL;
	}

	// save our current instance->stack pointer so we know how much we actually pushed
	destAddr = instance->stack;

	// build the passed parameters
	instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );

	if ( doDefault )
	{
		// message name for default methods
		*(instance->stack++) = *methodName;
	}

	// push the object (with correct instance, etc.
	if ( callerFDef->isExtension )
	{
		*(instance->stack++) = *caller;
	} else if ( !cEntry->isStatic )
	{
		// push hard context
		if ( cEntry->isVirtual )
		{
			*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
		} else
		{
			*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[cEntry->methodAccess.objectStart] );
		}
	}

	if ( (uint32_t)(instance->stack - destAddr) > callerFDef->nParams )
	{
		// we've passed more parameters than necessary
		// TODO: throw an error if the function isn't var paramable (need to add mark)
		callerNParam = (uint32_t)(instance->stack - destAddr);
	} else
	{
		callerNParam = callerFDef->nParams;
	}

	// the address of our next op is right after our code/ret sequence for paramaeter building
	ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

	// now call the new function
	switch ( callerFDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC( callerFDef, ops, callerNParam );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall( callerAtom, callerNParam );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
	instance->stack = destAddr;
	return ops;
}

void op_objRelease ( class vmInstance *instance, fglOp *ops, VAR *basePtr )
{
	VAR					*varTmp;
	bcClass				*callerClass;

	varTmp = instance->stack-1;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;
	if ( TYPE ( varTmp ) != slangType::eOBJECT_ROOT )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}

	// replace our object with the root object
	(instance->stack - 1)->dat.ref.v = varTmp->dat.ref.v[1].dat.ref.v;
	callerClass = (instance->stack - 1)->dat.ref.v->dat.obj.classDef;

	if ( callerClass->releaseEntry )
	{
		instance->interpretBC ( instance->atomTable->atomGetFunc ( callerClass->releaseEntry->methodAccess.atom ), ops + 1, 0 );
	}
}

VAR_OBJ *init_obj( vmInstance *instance, bcClass *callerClass )
{
	VAR_OBJ	*obj;

	auto newVar = instance->om->allocVar( callerClass->numVars );

	obj = (VAR_OBJ *)newVar++;

	obj->type = slangType::eOBJECT;
	obj->dat.obj.classDef = callerClass;
	obj->dat.obj.vPtr  = callerClass->vTable;
	obj->dat.obj.cargo = 0;

	newVar->type = slangType::eOBJECT_ROOT;
	newVar->dat.ref.v = obj;
	newVar->dat.ref.obj = obj;
	newVar++;
	
	for ( size_t loop = 2; loop < callerClass->numVars; loop++ )
	{
		obj[loop].type = slangType::eNULL;
	}

	for ( size_t loop = 0; loop < callerClass->nInherit; loop++ )
	{
		obj[callerClass->inherit[loop].objOffset].type = slangType::eOBJECT;
		obj[callerClass->inherit[loop].objOffset].dat.obj.cargo = 0;
		obj[callerClass->inherit[loop].objOffset].dat.obj.classDef = instance->atomTable->atomGetClass ( callerClass->inherit[loop].atom );
		obj[callerClass->inherit[loop].objOffset].dat.obj.vPtr = callerClass->inherit[loop].vTablePtr;

		obj[callerClass->inherit[loop].objOffset + 1].type = slangType::eOBJECT_ROOT;
		obj[callerClass->inherit[loop].objOffset + 1].dat.ref.v		= obj;
		obj[callerClass->inherit[loop].objOffset + 1].dat.ref.obj	= obj;
	}

	return obj;
}

void op_objBuild ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	bcClass				*callerClass;

	callerClass = instance->atomTable->atomGetClass ( ops->imm.dual.val1 );

	instance->stack->type = slangType::eOBJECT_ROOT;
	instance->stack->dat.ref.v = init_obj ( instance, callerClass );
	instance->stack->dat.ref.obj = instance->stack->dat.ref.v;
	instance->stack++;

	// register the destructor since we've succeded in creating the object
	if ( callerClass->releaseEntry )
	{
		instance->om->registerDestructor( (instance->stack - 1)->dat.ref.v );
	}

	GRIP g1 ( instance );
}

void op_objNew ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	VAR					*varTmp;
	bcClass				*callerClass;
	bcFuncDef			*callerFDef;
	uint32_t			 callerAtom;
	uint32_t			 callerNParam;
	VAR					*stackSave;

	stackSave = varTmp = instance->stack;

	callerClass = instance->atomTable->atomGetClass ( ops->imm.dual.val1 );

	instance->stack->type = slangType::eOBJECT_ROOT;
	instance->stack->dat.ref.v = init_obj ( instance, callerClass );
	instance->stack->dat.ref.obj = instance->stack->dat.ref.v;
	instance->stack++;

	if ( callerClass->newEntry )
	{
		callerAtom = callerClass->newEntry->methodAccess.atom;
		callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

		callerNParam = ops->imm.dual.val2 + 1;	// 1 for object we created

		// now call the new function
		switch (  callerFDef->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				instance->interpretBC ( callerFDef, ops, callerNParam );			// ops is the return address
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				instance->funcCall ( callerAtom, callerNParam );
				break;
			default:
				throw errorNum::scINTERNAL;
		}

		instance->stack = stackSave - (callerNParam - 1);
		*(instance->stack++) = *varTmp;
	}

	// register the destructor since we've succeded in creating the object
	if ( callerClass->releaseEntry )
	{
		instance->om->registerDestructor( (instance->stack - 1)->dat.ref.v );
	}

	GRIP g1 ( instance );
}

fglOp *op_objNewV ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr, bool safeCall )
{
	VAR			*varTmp;
	VAR			*stackSave;
	bcClass		*callerClass;
	bcFuncDef	*callerFDef;
	uint32_t	 callerAtom;
	uint32_t	 callerNParam;
	uint32_t	 pIndex;
	fglOp		*prefix = &ops[-1];

	stackSave = instance->stack;

	varTmp = --instance->stack;
	while ( TYPE ( varTmp ) == slangType::eREFERENCE ) varTmp = varTmp->dat.ref.v;

	if ( TYPE ( varTmp ) != slangType::eSTRING )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	if ( ((bcObjQuick *) &funcDef->loadImage->bssBase[prefix->imm.index])->name == varTmp->dat.str.c )
	{
		callerClass = ((bcObjQuick *) &funcDef->loadImage->bssBase[prefix->imm.index])->cClass;
	} else
	{
		callerClass = instance->atomTable->atomGetClass ( varTmp->dat.str.c );
		((bcObjQuick *) &funcDef->loadImage->bssBase[prefix->imm.index])->name = varTmp->dat.str.c;
		((bcObjQuick *) &funcDef->loadImage->bssBase[prefix->imm.index])->cClass = callerClass;
	}

	varTmp = instance->stack;
	instance->stack->type = slangType::eOBJECT_ROOT;
	instance->stack->dat.ref.v = init_obj ( instance, callerClass );
	instance->stack->dat.ref.obj = instance->stack->dat.ref.v;
	instance->stack++;

	if ( callerClass->newEntry )
	{
		callerAtom = callerClass->newEntry->methodAccess.atom;

		callerNParam = ops->imm.dual.val2;

		callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

		if ( safeCall && !callerFDef->isConst )
		{
			throw errorNum::scSAFE_CALL;
		}

		// build the parameter list

		// any NULL (not passed parameters... build that now
		for ( pIndex = callerFDef->nParams; pIndex > callerNParam + 1; pIndex-- )
		{
			if ( callerFDef->defaultParams && callerFDef->defaultParams[pIndex - 1] )
			{
				instance->stack = instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex - 1], 0, instance->stack, basePtr );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}
		callerNParam = callerFDef->nParams > callerNParam + 1 ? callerFDef->nParams : callerNParam + 1;		// self + default + callerNParam

		if ( ops->imm.dual.val2 )
		{
			// build the passed parameters
			instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );
		}

		// push self
		*(instance->stack++) = *varTmp;

		// the address of our next op is right after our code/ret sequence for paramaeter building
		ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

		// now call the new function
		switch ( callerFDef->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				instance->interpretBC ( callerFDef, ops, callerNParam );			// ops is the return address
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				instance->funcCall ( callerAtom, callerNParam );
				break;
			default:
				throw errorNum::scINTERNAL;
		}

		instance->stack = stackSave - (callerNParam - 1);;
	} else
	{
		// the address of our next op is right after our code/ret sequence for parameter building
		if ( nParams )
		{
			throw errorNum::scNO_CONSTRUCTOR;
		}
		ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];
	}

	// register the destructor since we've succeded in creating the object
	if ( callerClass->releaseEntry )
	{
		instance->om->registerDestructor ( (instance->stack - 1)->dat.ref.v );
	}

	GRIP g1 ( instance );

	return ops;
}

void op_objCallFuncOv ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	VAR				*caller;
	bcClass			*callerClass;
	bcFuncDef		*callerFDef;
	uint32_t		 callerNParam;
	uint32_t		 callerAtom;
	VAR				*stackSave;

	stackSave = instance->stack;
	caller = --instance->stack;					// pop off our object... we'll recreate it with the proper context

	callerNParam	= ops->imm.dual.val2;
	while ( TYPE ( caller ) == slangType::eREFERENCE ) caller = caller->dat.ref.v;

	if ( caller->type != slangType::eOBJECT_ROOT )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}
	if ( caller->dat.ref.v->type == slangType::eNULL )
	{
		throw errorNum::scSEND_TO_NONOBJECT;
	}
	callerClass = caller->dat.ref.v->dat.obj.classDef;

	auto cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

	if ( cEntry->isVirtual )
	{
		callerAtom = caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom;
	} else
	{
		callerAtom = cEntry->methodAccess.atom;
	}

	if ( cEntry->isVirtual )
	{
		*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
	} else
	{
		*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[cEntry->methodAccess.objectStart] );
	}
	callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

	// now call the new function
	switch (  callerFDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( callerFDef, ops + 1, callerNParam );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( ops->imm.dual.val1, callerNParam );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
	instance->stack = stackSave - callerNParam;
	return;
}
