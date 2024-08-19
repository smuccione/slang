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

void  op_incv ( class vmInstance *instance, fglOp *ops, VAR *leftOperand, fgxOvOp ovOp )
{
	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			leftOperand->dat.l += 1;
			break;
		case slangType::eDOUBLE:
			leftOperand->dat.d += 1;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(ovOp)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(ovOp)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}
			switch (  callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*(instance->stack - 2) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void  op_decv ( class vmInstance *instance, fglOp *ops, VAR *leftOperand, fgxOvOp ovOp )
{
	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			leftOperand->dat.l -= 1;
			break;
		case slangType::eDOUBLE:
			leftOperand->dat.d -= 1;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(ovOp)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(ovOp)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}
			switch (  callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*(instance->stack - 2) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_negv ( class vmInstance *instance, fglOp *ops, VAR *leftOperand, bool safeCall )
{
	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			leftOperand->dat.l = -leftOperand->dat.l;
			break;
		case slangType::eDOUBLE:
			leftOperand->dat.d = (double)-leftOperand->dat.l;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovNegate)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovNegate)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}
			if ( safeCall && !callerFDef->isConst ) throw errorNum::scSAFE_CALL;
			switch (  callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*(instance->stack - 2) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void  op_notv ( class vmInstance *instance, fglOp *ops, VAR *leftOperand, bool safeCall )
{
	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			leftOperand->type = slangType::eLONG;
			leftOperand->dat.l = 1;
			break;
		case slangType::eLONG:
			leftOperand->dat.l = !leftOperand->dat.l;
			break;
		case slangType::eDOUBLE:
			leftOperand->type = slangType::eLONG;
			leftOperand->dat.l = (int64_t) !(bool)leftOperand->dat.d;
			break;
		case slangType::eSTRING:
			leftOperand->type = slangType::eLONG;
			leftOperand->dat.l = leftOperand->dat.str.len?0:1;
			break;
		case slangType::eCODEBLOCK_ROOT:
		case slangType::eATOM:
			leftOperand->type = slangType::eLONG;
			leftOperand->dat.l = 0;
			break;
		case slangType::eARRAY_ROOT:
			leftOperand->type = slangType::eLONG;
			switch ( leftOperand->dat.ref.v->type )
			{
				case slangType::eARRAY_FIXED:
					leftOperand->dat.l = leftOperand->dat.ref.v->dat.arrayFixed.endIndex >= leftOperand->dat.ref.v->dat.arrayFixed.startIndex?1:0;
					break;
				case slangType::eARRAY_SPARSE:
					leftOperand->dat.l = leftOperand->dat.ref.v->dat.aSparse.v?1:0;
					break;
				default:
					break;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				// !<object>   for a valid object is true
				leftOperand->type = slangType::eLONG;
				leftOperand->dat.l = 1;
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovNot)] )
			{
				leftOperand->type = slangType::eLONG;
				leftOperand->dat.l = 0;
				break;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovNot)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}
			if ( safeCall && !callerFDef->isConst ) throw errorNum::scSAFE_CALL;
			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*(instance->stack - 2) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_twocv ( class vmInstance *instance, fglOp *ops, VAR *leftOperand, bool safeCall )
{
	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			leftOperand->dat.l = ~leftOperand->dat.l;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovTwosComplement)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovTwosComplement)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}
			if ( safeCall && !callerFDef->isConst ) throw errorNum::scSAFE_CALL;
			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			*(instance->stack - 2) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

