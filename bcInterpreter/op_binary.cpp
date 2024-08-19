	// binary variant operations

#include "stdlib.h"
#include "math.h"

#include "bcInterpreter.h"
#include "op_variant.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"
#include "bcVM/bcVM.h"
#include "bcVM/vmNativeInterface.h"

void  op_addv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR			*leftOperand;
	VAR			*rightOperand;
	char		 tmpBuff[_CVTBUFSIZE];
	int			 decOff;
	int			 signOff;
	char const	*tmpC;
	size_t		 len;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
				case slangType::eSTRING:
					*(instance->stack - 2) = *rightOperand;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					*(instance->stack - 2) = *leftOperand;
					break;
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l + rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->type = slangType::eDOUBLE;
					(instance->stack - 2)->dat.d = (double) leftOperand->dat.l + rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l + _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					*(instance->stack - 2) = *leftOperand;
					break;
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = leftOperand->dat.d += (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = leftOperand->dat.d += rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = leftOperand->dat.d += atof ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					*(instance->stack - 2) = *leftOperand;
					return;
				case slangType::eLONG:
					_i64toa_s ( rightOperand->dat.l, tmpBuff, sizeof ( tmpBuff ), 10 );
					tmpC = tmpBuff;
					len = strlen ( tmpC );
					break;
				case slangType::eDOUBLE:
					_ecvt_s ( tmpBuff, sizeof ( tmpBuff ), rightOperand->dat.d, _CVTBUFSIZE - 3, &decOff, &signOff );
					tmpC = tmpBuff;
					len = strlen ( tmpC );
					break;
				case slangType::eSTRING:
					tmpC = rightOperand->dat.str.c;
					len = rightOperand->dat.str.len;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}

			char *newString;
			newString = (char *) instance->om->alloc ( sizeof ( char ) * (leftOperand->dat.str.len + len + 1) );
			memcpy ( newString, leftOperand->dat.str.c, leftOperand->dat.str.len );
			memcpy ( newString + leftOperand->dat.str.len, tmpC, len );
			(instance->stack - 2)->dat.str.len = leftOperand->dat.str.len + len;
			newString[(instance->stack - 2)->dat.str.len] = 0;
			(instance->stack - 2)->dat.str.c = newString;
			(instance->stack - 2)->type = slangType::eSTRING;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovAdd)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int ( fgxOvOp::ovAdd)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void  op_minv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char		 tmpBuff[_CVTBUFSIZE];
	int			 decOff;
	int			 signOff;
	char const *tmpC;
	size_t		 len;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
				case slangType::eSTRING:
					*(instance->stack - 2) = *rightOperand;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l > rightOperand->dat.l ? leftOperand->dat.l : rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->type = slangType::eDOUBLE;
					(instance->stack - 2)->dat.d = (double) leftOperand->dat.l > rightOperand->dat.d ? (double) leftOperand->dat.l : rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l > _atoi64 ( rightOperand->dat.str.c ) ? leftOperand->dat.l : _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = leftOperand->dat.d > (double) rightOperand->dat.l ? leftOperand->dat.d : (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = leftOperand->dat.d > rightOperand->dat.d ? leftOperand->dat.d : rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = leftOperand->dat.d > atof ( rightOperand->dat.str.c ) ? leftOperand->dat.d : atof ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					_i64toa_s ( rightOperand->dat.l, tmpBuff, sizeof ( tmpBuff ), 10 );
					tmpC = tmpBuff;
					len = strlen ( tmpC );
					break;
				case slangType::eDOUBLE:
					_ecvt_s ( tmpBuff, sizeof ( tmpBuff ), rightOperand->dat.d, _CVTBUFSIZE - 3, &decOff, &signOff );
					tmpC = tmpBuff;
					len = strlen ( tmpC );
					break;
				case slangType::eSTRING:
					tmpC = rightOperand->dat.str.c;
					len = rightOperand->dat.str.len;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}

			{
				auto r = strcmp ( leftOperand->dat.str.c, tmpC );
				if ( r >= 0 )
				{
					// left operand is a good choice
					break;
				} else
				{
					// right operand is best
					switch ( TYPE ( rightOperand ) )
					{
						case slangType::eLONG:
						case slangType::eDOUBLE:
							(instance->stack - 2)->dat.str.c = instance->om->strDup ( tmpC );
							(instance->stack - 2)->dat.str.len = len;
							break;
						case slangType::eSTRING:
							(instance->stack - 2)->dat.str.c = tmpC;
							(instance->stack - 2)->dat.str.len = len;
							break;
						default:
							throw errorNum::scINTERNAL;
					}
				}
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovMax)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovMax)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void  op_maxv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char		 tmpBuff[_CVTBUFSIZE];
	int			 decOff;
	int			 signOff;
	char const *tmpC;
	size_t		 len;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
				case slangType::eSTRING:
					*(instance->stack - 2) = *rightOperand;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l > rightOperand->dat.l ? leftOperand->dat.l : rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->type = slangType::eDOUBLE;
					(instance->stack - 2)->dat.d = (double) leftOperand->dat.l > rightOperand->dat.d ? (double) leftOperand->dat.l : rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l > _atoi64 ( rightOperand->dat.str.c ) ? leftOperand->dat.l : _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = leftOperand->dat.d > (double) rightOperand->dat.l ? leftOperand->dat.d : (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = leftOperand->dat.d > rightOperand->dat.d ? leftOperand->dat.d : rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = leftOperand->dat.d > atof ( rightOperand->dat.str.c ) ? leftOperand->dat.d : atof ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					_i64toa_s ( rightOperand->dat.l, tmpBuff, sizeof ( tmpBuff ), 10 );
					tmpC = tmpBuff;
					len = strlen ( tmpC );
					break;
				case slangType::eDOUBLE:
					_ecvt_s ( tmpBuff, sizeof ( tmpBuff ), rightOperand->dat.d, _CVTBUFSIZE - 3, &decOff, &signOff );
					tmpC = tmpBuff;
					len = strlen ( tmpC );
					break;
				case slangType::eSTRING:
					tmpC = rightOperand->dat.str.c;
					len = rightOperand->dat.str.len;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			{
				auto r = strcmp ( leftOperand->dat.str.c, tmpC );
				if ( r <= 0 )
				{
					// left operand is a good choice
					break;
				} else
				{
					// right operand is best
					switch ( TYPE ( rightOperand ) )
					{
						case slangType::eLONG:
						case slangType::eDOUBLE:
							(instance->stack - 2)->dat.str.c = instance->om->strDup ( tmpC );
							(instance->stack - 2)->dat.str.len = len;
							break;
						case slangType::eSTRING:
							(instance->stack - 2)->dat.str.c = tmpC;
							(instance->stack - 2)->dat.str.len = len;
							break;
						default:
							throw errorNum::scINTERNAL;
					}
				}
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovMin)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovMin)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_subv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l - rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l - (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l - _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = leftOperand->dat.d - ((double) rightOperand->dat.l);
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = leftOperand->dat.d - ((double) rightOperand->dat.d);
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = leftOperand->dat.d - (atof ( rightOperand->dat.str.c ));
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eSTRING:
					(instance->stack - 2)->type = slangType::eLONG;
					(instance->stack - 2)->dat.l = strcmp ( leftOperand->dat.str.c, rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovSubtract)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovSubtract)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;

	}
}

void  op_modv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l % rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l % (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l % _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = (int64_t) leftOperand->dat.d % rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = (int64_t) leftOperand->dat.d % (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = (int64_t) leftOperand->dat.d % _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovModulus)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovModulus)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void  op_powv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = (int64_t) pow ( (double) leftOperand->dat.l, (double) rightOperand->dat.l );
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = (int64_t) pow ( (double) leftOperand->dat.l, rightOperand->dat.d );
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = (int64_t) pow ( (double) leftOperand->dat.l, atof ( rightOperand->dat.str.c ) );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = pow ( leftOperand->dat.d, (double) rightOperand->dat.l );
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = pow ( leftOperand->dat.d, rightOperand->dat.d );
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = pow ( leftOperand->dat.d, atof ( rightOperand->dat.str.c ) );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovPower)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovPower)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void  op_divv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = (double) leftOperand->dat.l / (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = (double) leftOperand->dat.l / rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = (double) leftOperand->dat.l / atof ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = leftOperand->dat.d / (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = leftOperand->dat.d / rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = leftOperand->dat.d / atof ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovDivide)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovDivide)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void  op_eqv1 ( class vmInstance *instance, fglOp *ops )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 1)->dat.l = 1;
					(instance->stack - 1)->type = slangType::eLONG;
					break;
				default:
					(instance->stack - 1)->dat.l = 0;
					(instance->stack - 1)->type = slangType::eLONG;
					break;
			}
			break;
		case slangType::eLONG:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 1)->dat.l = leftOperand->dat.l == rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 1)->dat.l = leftOperand->dat.l == (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 1)->dat.l = leftOperand->dat.l == _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 1)->dat.l = 0;
					break;
			}
			break;
		case slangType::eDOUBLE:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 1)->dat.l = leftOperand->dat.d == (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 1)->dat.l = leftOperand->dat.d == rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 1)->dat.l = leftOperand->dat.d == atof ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 1)->dat.l = 0;
					break;
			}
			(instance->stack - 1)->type = slangType::eLONG;
			break;
		case slangType::eSTRING:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 1)->dat.l = strncmp ( tmpString, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 0 : 1;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 1)->dat.l = strncmp ( tmpString, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 0 : 1;
					break;
				case slangType::eSTRING:
					(instance->stack - 1)->dat.l = strncmp ( rightOperand->dat.str.c, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 0 : 1;
					break;
				default:
					(instance->stack - 1)->dat.l = 0;
					break;
			}
			(instance->stack - 1)->type = slangType::eLONG;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovEqual)] )
			{
				switch ( TYPE ( rightOperand ) )
				{
					case slangType::eOBJECT_ROOT:
						if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
						{
							(instance->stack - 1)->dat.l = 1;
						} else
						{
							(instance->stack - 1)->dat.l = 0;
						}
						break;
					default:
						(instance->stack - 1)->dat.l = 0;
						break;
				}
				(instance->stack - 1)->type = slangType::eLONG;
				break;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovEqual)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}
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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 1) = instance->result;
			break;
		case slangType::eARRAY_ROOT:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eARRAY_ROOT:
					if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
					{
						(instance->stack - 1)->dat.l = 1;
					} else
					{
						(instance->stack - 1)->dat.l = 0;
					}
					break;
				default:
					(instance->stack - 1)->dat.l = 0;
					break;
			}
			(instance->stack - 1)->type = slangType::eLONG;
			break;
		default:
			if ( rightOperand == leftOperand )
			{
				(instance->stack - 1)->dat.l = 1;
			} else
			{
				(instance->stack - 1)->dat.l = 0;
			}
			(instance->stack - 1)->type = slangType::eLONG;
			break;
	}
}

void  op_eqv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l == rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l == (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l == _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.d == (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.d == rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.d == atof ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 2)->dat.l = strncmp ( tmpString, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 0 : 1;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 2)->dat.l = strncmp ( tmpString, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 0 : 1;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = strncmp ( rightOperand->dat.str.c, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 0 : 1;
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovEqual)] )
			{
				switch ( TYPE ( rightOperand ) )
				{
					case slangType::eOBJECT_ROOT:
						if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
						{
							(instance->stack - 2)->dat.l = 1;
						} else
						{
							(instance->stack - 2)->dat.l = 0;
						}
						break;
					default:
						(instance->stack - 2)->dat.l = 0;
						break;
				}
				(instance->stack - 2)->type = slangType::eLONG;
				break;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovEqual)] - 1];

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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 3) = instance->result;
			break;
		case slangType::eARRAY_ROOT:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eARRAY_ROOT:
					if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
					{
						(instance->stack - 2)->dat.l = 1;
					} else
					{
						(instance->stack - 2)->dat.l = 0;
					}
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
		default:
			if ( rightOperand == leftOperand )
			{
				(instance->stack - 2)->dat.l = 1;
			} else
			{
				(instance->stack - 2)->dat.l = 0;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
	}
}

void  op_aeqv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l == rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l == (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l == _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.d == (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.d == rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.d == atof ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 2)->dat.l = strccmp ( tmpString, leftOperand->dat.str.c ) ? 0 : 1;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 2)->dat.l = strccmp ( tmpString, leftOperand->dat.str.c ) ? 0 : 1;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = strccmp ( rightOperand->dat.str.c, leftOperand->dat.str.c ) ? 0 : 1;
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovEqualA)] )
			{
				switch ( TYPE ( rightOperand ) )
				{
					case slangType::eOBJECT_ROOT:
						if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
						{
							(instance->stack - 2)->dat.l = 1;
						} else
						{
							(instance->stack - 2)->dat.l = 0;
						}
						break;
					default:
						(instance->stack - 2)->dat.l = 0;
						break;
				}
				(instance->stack - 2)->type = slangType::eLONG;
				break;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovEqualA)] - 1];

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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 3) = instance->result;
			break;
		case slangType::eARRAY_ROOT:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eARRAY_ROOT:
					if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
					{
						(instance->stack - 2)->dat.l = 1;
					} else
					{
						(instance->stack - 2)->dat.l = 0;
					}
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
		default:
			if ( rightOperand == leftOperand )
			{
				(instance->stack - 2)->dat.l = 1;
			} else
			{
				(instance->stack - 2)->dat.l = 0;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
	}
}

void  op_neqv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l != rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l != (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l != _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.d != (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.d != rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.d != atof ( rightOperand->dat.str.c );
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eSTRING:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 2)->dat.l = strncmp ( tmpString, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 1 : 0;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 2)->dat.l = strncmp ( tmpString, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 1 : 0;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = strncmp ( rightOperand->dat.str.c, leftOperand->dat.str.c, leftOperand->dat.str.len + 1 ) ? 1 : 0;
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovNotEqual)] )
			{
				switch ( TYPE ( rightOperand ) )
				{
					case slangType::eOBJECT_ROOT:
						if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
						{
							(instance->stack - 2)->dat.l = 1;
						} else
						{
							(instance->stack - 2)->dat.l = 0;
						}
						break;
					default:
						(instance->stack - 2)->dat.l = 1;
						break;
				}
				(instance->stack - 2)->type = slangType::eLONG;
				break;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovNotEqual)] - 1];

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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 3) = instance->result;
			break;
		case slangType::eARRAY_ROOT:
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eARRAY_ROOT:
					if ( rightOperand->dat.ref.v == leftOperand->dat.ref.v )
					{
						(instance->stack - 2)->dat.l = 0;
					} else
					{
						(instance->stack - 2)->dat.l = 1;
					}
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
		default:
			if ( rightOperand == leftOperand )
			{
				(instance->stack - 2)->dat.l = 0;
			} else
			{
				(instance->stack - 2)->dat.l = 1;
			}
			(instance->stack - 2)->type = slangType::eLONG;
			break;
	}
}

void op_mulv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l * rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->type = slangType::eDOUBLE;
					(instance->stack - 2)->dat.d = (double) leftOperand->dat.l * rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l * _atoi64 ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eDOUBLE;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.d = leftOperand->dat.d * (double) rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.d = leftOperand->dat.d * rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.d = leftOperand->dat.d * atof ( rightOperand->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eOBJECT_ROOT:
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovMultiply)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovMultiply)] - 1];

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
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

#define MAKE_BINARY_LONG_OP(name,ovOp, op)  \
void  name ( class vmInstance *instance, fglOp *ops, bool safeCall ) \
{ \
	VAR		*leftOperand; \
	VAR		*rightOperand; \
 \
	leftOperand = instance->stack - 2; \
	rightOperand = instance->stack - 1; \
 \
	while ( TYPE ( leftOperand ) == slangType::eREFERENCE ) \
	{ \
		leftOperand = leftOperand->dat.ref.v; \
	} \
 \
	while ( TYPE ( rightOperand ) == slangType::eREFERENCE ) \
	{ \
		rightOperand = rightOperand->dat.ref.v; \
	} \
 \
	switch ( TYPE ( leftOperand ) ) \
	{ \
		case slangType::eLONG: \
			(instance->stack-2)->type = slangType::eLONG; \
			switch ( TYPE ( rightOperand ) ) \
			{ \
				case slangType::eLONG: \
					(instance->stack-2)->dat.l = leftOperand->dat.l op rightOperand->dat.l; \
					break; \
				case slangType::eDOUBLE: \
					(instance->stack-2)->dat.l = leftOperand->dat.l op (int64_t)rightOperand->dat.d; \
					break; \
				case slangType::eSTRING: \
					(instance->stack-2)->dat.l = leftOperand->dat.l op _atoi64 ( rightOperand->dat.str.c ); \
					break; \
				default: \
					throw errorNum::scILLEGAL_OPERAND; \
			} \
			break; \
		case slangType::eDOUBLE: \
			(instance->stack-2)->type = slangType::eLONG; \
			switch ( TYPE ( rightOperand ) ) \
			{ \
				case slangType::eLONG: \
					(instance->stack-2)->dat.l = ((int64_t)leftOperand->dat.d) op ((int64_t)rightOperand->dat.l); \
					break; \
				case slangType::eDOUBLE: \
					(instance->stack-2)->dat.l = ((int64_t)leftOperand->dat.d) op ((int64_t)rightOperand->dat.d); \
					break; \
				case slangType::eSTRING: \
					(instance->stack-2)->dat.l = ((int64_t)leftOperand->dat.d) op (_atoi64( rightOperand->dat.str.c )); \
					break; \
				default: \
					throw errorNum::scILLEGAL_OPERAND; \
			} \
			break; \
		case slangType::eOBJECT_ROOT: \
			bcFuncDef				 *callerFDef;\
			bcClass					 *callerClass;\
			bcClassSearchEntry		 *cEntry;\
\
			if ( leftOperand->dat.ref.v->type == slangType::eNULL )	\
			{	\
				throw errorNum::scSEND_TO_NONOBJECT;	\
			}	\
\
			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;\
\
			if ( !callerClass->ops[int(ovOp)] )\
			{\
				throw errorNum::scINVALID_OPERATOR;\
			}\
\
			cEntry = &callerClass->elements[callerClass->ops[int(ovOp)] - 1];\
\
			if ( cEntry->isVirtual )	\
			{	\
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );	\
				callerFDef = instance->atomTable->atomGetFunc ( leftOperand->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );	\
			} else	\
			{	\
				*(instance->stack++) = VAR_OBJ ( leftOperand, &leftOperand->dat.ref.v[cEntry->methodAccess.objectStart] );	\
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );	\
			}	\
			if ( safeCall && !callerFDef->isConst ) throw errorNum::scSAFE_CALL;	\
			switch (  callerFDef->conv ) \
			{ \
				case fgxFuncCallConvention::opFuncType_Bytecode: \
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams ); \
					break; \
				case fgxFuncCallConvention::opFuncType_cDecl: \
					instance->funcCall ( callerFDef, callerFDef->nParams ); \
					break; \
				default: \
					throw errorNum::scINTERNAL; \
			} \
			*(instance->stack - 3) = instance->result; \
			break; \
		default: \
			throw errorNum::scILLEGAL_OPERAND; \
	} \
}

void op_ltv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l < rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l < (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l < _atoi64 ( rightOperand->dat.str.c );
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) < ((int64_t) rightOperand->dat.l);
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) < ((int64_t) rightOperand->dat.d);
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) < (_atoi64 ( rightOperand->dat.str.c ));
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) < 0;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) < 0;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, rightOperand->dat.str.c, leftOperand->dat.str.len + 1 ) < 0;
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eOBJECT_ROOT:
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovLess)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovLess)] - 1];

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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_lteqv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR *leftOperand;
	VAR *rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l <= rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l <= (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l <= _atoi64 ( rightOperand->dat.str.c );
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) <= ((int64_t) rightOperand->dat.l);
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) <= ((int64_t) rightOperand->dat.d);
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) <= (_atoi64 ( rightOperand->dat.str.c ));
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) <= 0;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) <= 0;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, rightOperand->dat.str.c, leftOperand->dat.str.len + 1 ) <= 0;
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eOBJECT_ROOT:
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovLessEqual)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovLessEqual)] - 1];

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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_gtv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR		*leftOperand;
	VAR		*rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			(instance->stack - 2)->dat.l = 0;
			(instance->stack - 2)->type = slangType::eLONG;
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l > rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l > (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l > _atoi64 ( rightOperand->dat.str.c );
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) > ((int64_t) rightOperand->dat.l);
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) > ((int64_t) rightOperand->dat.d);
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) > (_atoi64 ( rightOperand->dat.str.c ));
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) > 0;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) > 0;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, rightOperand->dat.str.c, leftOperand->dat.str.len + 1 ) > 0;
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eOBJECT_ROOT:
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovGreater)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovGreater)] - 1];

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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_gteqv ( class vmInstance *instance, fglOp *ops, bool safeCall )
{
	VAR		*leftOperand;
	VAR		*rightOperand;
	char	 tmpString[33];

	leftOperand = instance->stack - 2;
	rightOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	while ( TYPE ( rightOperand ) == slangType::eREFERENCE )
	{
		rightOperand = rightOperand->dat.ref.v;
	}

	switch ( TYPE ( leftOperand ) )
	{
		case slangType::eNULL:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					(instance->stack - 2)->dat.l = 0;
					break;
			}
			break;
		case slangType::eLONG:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = leftOperand->dat.l >= rightOperand->dat.l;
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = leftOperand->dat.l >= (int64_t) rightOperand->dat.d;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = leftOperand->dat.l >= _atoi64 ( rightOperand->dat.str.c );
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eLONG:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) >= ((int64_t) rightOperand->dat.l);
					break;
				case slangType::eDOUBLE:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) >= ((int64_t) rightOperand->dat.d);
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = ((int64_t) leftOperand->dat.d) >= (_atoi64 ( rightOperand->dat.str.c ));
					break;
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 1;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			(instance->stack - 2)->type = slangType::eLONG;
			switch ( TYPE ( rightOperand ) )
			{
				case slangType::eNULL:
					(instance->stack - 2)->dat.l = 0;
					break;
				case slangType::eLONG:
					_i64toa_s ( leftOperand->dat.l, tmpString, sizeof ( tmpString ), 10 );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) >= 0;
					break;
				case slangType::eDOUBLE:
					int sign;
					int dec;
					_fcvt_s ( tmpString, sizeof ( tmpString ), leftOperand->dat.d, 4, &dec, &sign );
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, tmpString, leftOperand->dat.str.len + 1 ) >= 0;
					break;
				case slangType::eSTRING:
					(instance->stack - 2)->dat.l = strncmp ( leftOperand->dat.str.c, rightOperand->dat.str.c, leftOperand->dat.str.len + 1 ) >= 0;
					break;
				default:
					(instance->stack - 2)->dat.l = 1;
					break;
			}
			break;
		case slangType::eOBJECT_ROOT:
			bcFuncDef *callerFDef;
			bcClass *callerClass;
			bcClassSearchEntry *cEntry;

			if ( leftOperand->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = leftOperand->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int ( fgxOvOp::ovGreaterEqual)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovGreaterEqual)] - 1];

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
			if ( instance->result.type != slangType::eLONG )
			{
				throw errorNum::scINVALID_COMPARATOR_RESULT;
			}
			*(instance->stack - 3) = instance->result;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

// add, pow, and mod div are special cased.

MAKE_BINARY_LONG_OP ( op_bwOrv, fgxOvOp::ovBitOr, | );
MAKE_BINARY_LONG_OP ( op_bwAndv, fgxOvOp::ovBitAnd, & );
MAKE_BINARY_LONG_OP ( op_bwXor, fgxOvOp::ovBitXor, ^ );
MAKE_BINARY_LONG_OP ( op_bwShLeft, fgxOvOp::ovShiftLeft, << );
MAKE_BINARY_LONG_OP ( op_bwShRight, fgxOvOp::ovShiftRight, >> );

