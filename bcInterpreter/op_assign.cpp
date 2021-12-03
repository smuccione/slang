// variant assignment operations

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

void op_assign ( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value )
{
	while ( TYPE ( value ) == slangType::eREFERENCE ) value = value->dat.ref.v;
	while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;

	switch ( TYPE ( dest ) )
	{
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			if ( dest->dat.ref.v->type == slangType::eNULL )
			{
				// this is a root, but NOT a valid object (was destructed)
				*dest = *value;
				break;
			}

			callerClass = dest->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovAssign)] )
			{
				// not overridden, so we just do a normal assignment to our variable
				*dest = *value;
				break;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovAssign)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[cEntry->methodAccess.objectStart] );
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
			break;
		default:
			*dest = *value;
			break;
	}
}

void op_assignAdd ( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value )
{
	char		 tmpBuff[_CVTBUFSIZE];
	int			 decOff;
	int			 signOff;
	size_t		 len;
	char const	*tmpC;

	while ( TYPE ( value ) == slangType::eREFERENCE ) value = value->dat.ref.v;
	while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;

	switch ( TYPE ( dest ) )
	{
		case slangType::eNULL:
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
				case slangType::eDOUBLE:
				case slangType::eSTRING:
					*dest = *value;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eLONG:
			switch ( TYPE( value ) )
			{
				case slangType::eLONG:
					dest->dat.l += value->dat.l;
					break;
				case slangType::eDOUBLE:
					dest->dat.l += (long)value->dat.d;
					break;
				case slangType::eSTRING:
					dest->dat.l += atol ( value->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			switch ( TYPE( value ) )
			{
				case slangType::eLONG:
					dest->dat.d += (double)value->dat.l;
					break;
				case slangType::eDOUBLE:
					dest->dat.d += value->dat.d;
					break;
				case slangType::eSTRING:
					dest->dat.d += atof ( value->dat.str.c );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
					_i64toa_s ( value->dat.l, tmpBuff, sizeof ( tmpBuff ), 10 );
					tmpC = tmpBuff;
					len = (uint32_t)strlen ( tmpC );
					break;
				case slangType::eDOUBLE:
					_ecvt_s ( tmpBuff, sizeof ( tmpBuff ), value->dat.d, _CVTBUFSIZE-3, &decOff, &signOff );
					tmpC = tmpBuff;
					len = (uint32_t)strlen ( tmpC );
					break;
				case slangType::eSTRING:
					tmpC = value->dat.str.c;
					len  = value->dat.str.len;
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}

			char	*newString;
			newString = (char *)instance->om->alloc ( sizeof ( char ) * (dest->dat.str.len + len + 1 ) );
			memcpy ( newString, dest->dat.str.c, dest->dat.str.len );
			memcpy ( newString + dest->dat.str.len, tmpC, len );
			dest->dat.str.len += len;
			newString[dest->dat.str.len] = 0;
			dest->dat.str.c = newString;
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcClassSearchEntry	*cEntry;
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;

			if ( dest->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = dest->dat.ref.v->dat.obj.classDef;
			if ( !callerClass->ops[int ( fgxOvOp::ovAddAssign)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int (fgxOvOp::ovAddAssign)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[cEntry->methodAccess.objectStart] );
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
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_assignPow ( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value )
{
	while ( TYPE ( value ) == slangType::eREFERENCE ) value = value->dat.ref.v;
	while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;

	switch ( TYPE ( dest ) )
	{
		case slangType::eLONG:
			dest->type = slangType::eDOUBLE;
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
					dest->dat.d = pow ( dest->dat.l, value->dat.l );
					break;
				case slangType::eDOUBLE:
					dest->dat.d = pow ( dest->dat.l, value->dat.d );
					break;
				case slangType::eSTRING:
					dest->dat.d = pow ( dest->dat.l, atof ( value->dat.str.c ) );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eDOUBLE:
			switch ( TYPE ( value ) )
			{
				case slangType::eLONG:
					dest->dat.d = pow ( dest->dat.d, value->dat.l );
					break;
				case slangType::eDOUBLE:
					dest->dat.d = pow ( dest->dat.d, value->dat.d );
					break;
				case slangType::eSTRING:
					dest->dat.d = pow ( dest->dat.d, atof ( value->dat.str.c ) );
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case slangType::eSTRING:
			throw errorNum::scILLEGAL_OPERAND;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcClassSearchEntry *cEntry;
			bcFuncDef *callerFDef;
			bcClass *callerClass;

			if ( dest->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = dest->dat.ref.v->dat.obj.classDef;
			if ( !callerClass->ops[int ( fgxOvOp::ovAddAssign)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int (fgxOvOp::ovPowAssign)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[cEntry->methodAccess.objectStart] );
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
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

#define MAKE_ASSIGN_OP(name,ovOp,op)  \
void  name ( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value ) \
{ \
	while ( TYPE ( value ) == slangType::eREFERENCE ) value = value->dat.ref.v; \
	while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v; \
 \
	switch ( TYPE ( dest ) ) \
	{ \
		case slangType::eLONG: \
			switch ( TYPE( value ) ) \
			{ \
				case slangType::eLONG: \
					dest->dat.l op value->dat.l; \
					break; \
				case slangType::eDOUBLE: \
					dest->dat.l op (long)value->dat.d; \
					break; \
				case slangType::eSTRING: \
					dest->dat.l op atol ( value->dat.str.c ); \
					break; \
				default: \
					throw errorNum::scILLEGAL_OPERAND; \
			} \
			break; \
		case slangType::eDOUBLE: \
			switch ( TYPE( value ) ) \
			{ \
				case slangType::eLONG: \
					dest->dat.d op value->dat.l; \
					break; \
				case slangType::eDOUBLE: \
					dest->dat.d op value->dat.d; \
					break; \
				case slangType::eSTRING: \
					dest->dat.d op atof ( value->dat.str.c ); \
					break; \
				default: \
					throw errorNum::scILLEGAL_OPERAND; \
			} \
			break; \
		case slangType::eOBJECT_ROOT: \
			bcClassSearchEntry		 *cEntry; \
			bcFuncDef				 *callerFDef; \
			bcClass					 *callerClass; \
\
			if ( dest->dat.ref.v->type == slangType::eNULL )	\
			{	\
				throw errorNum::scSEND_TO_NONOBJECT;	\
			}	\
			\
			callerClass = dest->dat.ref.v->dat.obj.classDef; \
 \
			if ( !callerClass->ops[int(fgxOvOp::ovOp)] ) \
			{ \
				throw errorNum::scINVALID_OPERATOR; \
			} \
 \
			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovOp)] - 1]; \
\
			if ( cEntry->isVirtual )	\
			{	\
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );	\
				callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );	\
			} else \
			{	\
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[cEntry->methodAccess.objectStart] );	\
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );	\
			}	\
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
			break; \
		default: \
			throw errorNum::scILLEGAL_OPERAND; \
	} \
} \

#define MAKE_ASSIGN_OP_INT(name,ovOp,op)  \
void  name ( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value ) \
{ \
	while ( TYPE ( value ) == slangType::eREFERENCE ) value = value->dat.ref.v; \
	while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v; \
 \
	switch ( TYPE ( dest ) ) \
	{ \
		case slangType::eLONG: \
			switch ( TYPE( value ) ) \
			{ \
				case slangType::eLONG: \
					dest->dat.l op value->dat.l; \
					break; \
				case slangType::eDOUBLE: \
					dest->dat.l op (long)value->dat.d; \
					break; \
				case slangType::eSTRING: \
					dest->dat.l op atol ( value->dat.str.c ); \
					break; \
				default: \
					throw errorNum::scILLEGAL_OPERAND; \
			} \
			break; \
		case slangType::eOBJECT_ROOT: \
			bcClassSearchEntry		 *cEntry; \
			bcFuncDef				 *callerFDef; \
			bcClass					 *callerClass; \
\
			if ( dest->dat.ref.v->type == slangType::eNULL )	\
			{	\
				throw errorNum::scSEND_TO_NONOBJECT;	\
			}	\
\
			callerClass = dest->dat.ref.v->dat.obj.classDef; \
\
			dest = &dest[ops->imm.objOp.objectIndex];	\
			if ( !callerClass->ops[int(fgxOvOp::ovOp)] ) \
			{ \
				throw errorNum::scINVALID_OPERATOR; \
			} \
\
			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovOp)] - 1]; \
\
			if ( cEntry->isVirtual )	\
			{	\
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );	\
				callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );	\
			} else \
			{	\
				*(instance->stack++) = VAR_OBJ ( dest, &dest->dat.ref.v[cEntry->methodAccess.objectStart] );	\
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );	\
			}	\
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
			break; \
		default: \
			throw errorNum::scILLEGAL_OPERAND; \
	} \
} \

MAKE_ASSIGN_OP		( op_assignSub,		ovSubAssign,			-=  );
MAKE_ASSIGN_OP		( op_assignMul,		ovMulAssign,			*=  );
MAKE_ASSIGN_OP		( op_assignDiv,		ovDivAssign,			/=  );
MAKE_ASSIGN_OP_INT	( op_assignMod,		ovModAssign,			%=  );
MAKE_ASSIGN_OP_INT	( op_assignBwAnd,	ovBwAndAssign,			|=  );
MAKE_ASSIGN_OP_INT	( op_assignBwOr,	ovBwOrAssign,			&=  );
MAKE_ASSIGN_OP_INT	( op_assignBwXor,	ovBwXorAssign,			^=  );
MAKE_ASSIGN_OP_INT	( op_assignShLeft,	ovShLeftAssign,			<<= );
MAKE_ASSIGN_OP_INT	( op_assignShRight,	ovShRightAssign,		>>= );
