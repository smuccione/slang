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

fglOp *op_call ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	VAR *caller;
	bcFuncDef *callerFDef;
	uint32_t				 callerAtom;
	uint32_t				 pIndex;
	uint32_t				 callerNParam;
	bcClass *callerClass;
	bcClassSearchEntry *cEntry;

	auto stackSave = instance->stack;

	callerNParam = ops->imm.dual.val2;

	while ( TYPE ( instance->stack - 1 ) == slangType::eREFERENCE ) *(instance->stack - 1) = *(instance->stack - 1)->dat.ref.v;
	caller = instance->stack - 1;

	switch ( caller->type )
	{
		case slangType::eCODEBLOCK_ROOT:
			// build the passed parameters
			if ( callerNParam )
			{
				instance->stack = instance->interpretBCParam ( funcDef, 0, ops + 1, nParams, instance->stack, basePtr );
			}

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			op_cbCall ( instance, ops + 1, funcDef, basePtr, callerNParam );
			instance->stack = stackSave - 1;
			break;
		case slangType::eSTRING:
		case slangType::eATOM:
			if ( caller->type == slangType::eSTRING )
			{
				callerAtom = instance->atomTable->atomFind ( caller->dat.str.c );
			} else
			{
				callerAtom = caller->dat.atom;
			}
			if ( instance->atomTable->atomGetType ( callerAtom ) == atom::atomType::aFUNCDEF )
			{
				callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

				// any NULL (not passed parameters... build that now
				for ( pIndex = callerFDef->nParams; pIndex > callerNParam; pIndex-- )
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

				callerNParam = callerFDef->nParams > callerNParam ? callerFDef->nParams : callerNParam;		// self + default + callerNParam

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
				instance->stack = stackSave - 1;
			} else if ( instance->atomTable->atomGetType ( callerAtom ) == atom::atomType::aCLASSDEF )
			{
				ops = op_objNewV ( instance, ops, funcDef, nParams, basePtr, false );
				instance->result = *(instance->stack - 1);
				instance->stack = stackSave - 1;
			} else
			{
				throw errorNum::scUNKNOWN_FUNCTION;
			}
			break;
		case slangType::eOBJECT_ROOT:
			callerNParam = ops->imm.dual.val2;
			if ( caller->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}
			callerClass = caller->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovFuncCall)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

			if ( cEntry->isVirtual )
			{
				callerFDef = instance->atomTable->atomGetFunc ( caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
			} else
			{
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}

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

			if ( callerNParam )
			{
				// build the passed parameters
				instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );
			}

			// compute the number of passed parameteers.  we use callerFDef but subtract one during the comparison because we dont "pass" self.
			callerNParam = (callerFDef->nParams - 1 > callerNParam) ? callerFDef->nParams - 1 : callerNParam;
			// we then add self back in since it'll be on the stack (we push it ourself below)
			callerNParam += 1;

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			if ( instance->debug ) instance->debug->IP = ops;

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[cEntry->methodAccess.objectStart] );
			}
			// now call the new function
			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack = stackSave - 1;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
	return ops;
}

fglOp *op_callSafe ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	VAR *caller;
	bcFuncDef *callerFDef;
	uint32_t				 callerAtom;
	uint32_t				 pIndex;
	uint32_t				 callerNParam;
	bcClass *callerClass;
	bcClassSearchEntry *cEntry;

	auto stackSave = instance->stack;

	callerNParam = ops->imm.dual.val2;

	while ( TYPE ( instance->stack - 1 ) == slangType::eREFERENCE ) *(instance->stack - 1) = *(instance->stack - 1)->dat.ref.v;
	caller = instance->stack - 1;

	switch ( caller->type )
	{
		case slangType::eCODEBLOCK_ROOT:
			// build the passed parameters
			if ( callerNParam )
			{
				instance->stack = instance->interpretBCParam ( funcDef, 0, ops + 1, nParams, instance->stack, basePtr );
			}

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			op_cbCall ( instance, ops + 1, funcDef, basePtr, callerNParam );
			instance->stack = stackSave - 1;
			break;
		case slangType::eSTRING:
		case slangType::eATOM:
			if ( caller->type == slangType::eSTRING )
			{
				callerAtom = instance->atomTable->atomFind ( caller->dat.str.c );
			} else
			{
				callerAtom = caller->dat.atom;
			}
			if ( instance->atomTable->atomGetType ( callerAtom ) == atom::atomType::aFUNCDEF )
			{
				callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

				if ( !callerFDef->isConst )
				{
					throw errorNum::scSAFE_CALL;
				}

				// any NULL (not passed parameters... build that now
				for ( pIndex = callerFDef->nParams; pIndex > callerNParam; pIndex-- )
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

				callerNParam = callerFDef->nParams > callerNParam ? callerFDef->nParams : callerNParam;		// self + default + callerNParam

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
				instance->stack = stackSave - 1;
			} else if ( instance->atomTable->atomGetType ( callerAtom ) == atom::atomType::aCLASSDEF )
			{
				ops = op_objNewV ( instance, ops, funcDef, nParams, basePtr, true );
				instance->result = *(instance->stack - 1);
				instance->stack = stackSave - 1;
			} else
			{
				throw errorNum::scUNKNOWN_FUNCTION;
			}
			break;
		case slangType::eOBJECT_ROOT:
			callerNParam = ops->imm.dual.val2;
			if ( caller->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}
			callerClass = caller->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovFuncCall)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

			if ( cEntry->isVirtual )
			{
				callerFDef = instance->atomTable->atomGetFunc ( caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
			} else
			{
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}

			if ( !callerFDef->isConst )
			{
				throw errorNum::scSAFE_CALL;
			}
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

			if ( callerNParam )
			{
				// build the passed parameters
				instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );
			}

			// compute the number of passed parameteers.  we use callerFDef but subtract one during the comparison because we dont "pass" self.
			callerNParam = (callerFDef->nParams - 1 > callerNParam) ? callerFDef->nParams - 1 : callerNParam;
			// we then add self back in since it'll be on the stack (we push it ourself below)
			callerNParam += 1;

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			if ( instance->debug ) instance->debug->IP = ops;

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[cEntry->methodAccess.objectStart] );
			}
			// now call the new function
			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack = stackSave - 1;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
	return ops;
}

fglOp *op_callPPack ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	VAR *caller;
	VAR *stackSave;
	VAR *stackSave2;
	bcFuncDef *callerFDef;
	uint32_t				 callerAtom;
	uint32_t 				 pIndex;
	uint32_t				 callerNParam;
	bcClass *callerClass;
	bcClassSearchEntry *cEntry;

	stackSave = instance->stack;
	callerNParam = ops->imm.dual.val2;

	caller = instance->stack - 1;
	while ( TYPE ( caller ) == slangType::eREFERENCE ) caller = caller->dat.ref.v;

	switch ( caller->type )
	{
		case slangType::eCODEBLOCK_ROOT:
			// build the passed parameters
			stackSave2 = instance->stack;
			instance->stack = instance->interpretBCParam ( funcDef, 0, ops + 1, nParams, instance->stack, basePtr );

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			callerNParam = (uint32_t)(instance->stack - stackSave2);

			op_cbCall ( instance, ops + 1, funcDef, basePtr, callerNParam );
			instance->stack = stackSave - 1;
			break;
		case slangType::eSTRING:
		case slangType::eATOM:
			if ( caller->type == slangType::eSTRING )
			{
				callerAtom = instance->atomTable->atomFind ( caller->dat.str.c );
			} else
			{
				callerAtom = caller->dat.atom;
			}
			callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

			// we have no idea how many actual parameters will be pushed... for safety we need to push sufficient NULL's to make up the difference
			for ( pIndex = callerFDef->nParams; pIndex > callerNParam; pIndex-- )
			{
				(instance->stack++)->type = slangType::eNULL;
			}

			stackSave2 = instance->stack;

			// build the passed parameters
			instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			if ( (uint32_t)(instance->stack - stackSave2) > callerFDef->nParams )
			{
				// we've passed more parameters than necessary
				// TODO: throw an error if the function isn't var paramable (need to add mark)
				callerNParam = (uint32_t)(instance->stack - stackSave2);
			} else
			{
				callerNParam = callerFDef->nParams;
			}

			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef->atom, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack = stackSave - 1;
			break;
		case slangType::eOBJECT_ROOT:
			caller = --instance->stack;

			callerNParam = ops->imm.dual.val2;

			if ( caller->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}
			callerClass = caller->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovFuncCall)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

			if ( cEntry->isVirtual )
			{
				callerFDef = instance->atomTable->atomGetFunc ( caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
			} else
			{
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}

			// we have no idea how many actual parameters will be pushed... for safety we need to push sufficient NULL's to make up the difference
			for ( pIndex = callerFDef->nParams; pIndex > callerNParam; pIndex-- )
			{
				(instance->stack++)->type = slangType::eNULL;
			}

			stackSave2 = instance->stack;

			// build the passed parameters
			instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			if ( instance->debug ) instance->debug->IP = ops;

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[cEntry->methodAccess.objectStart] );
			}
			if ( (uint32_t)(instance->stack - stackSave2) > callerFDef->nParams )
			{
				// we've passed more parameters than necessary
				// TODO: throw an error if the function isn't var paramable (need to add mark)
				callerNParam = (uint32_t)(instance->stack - stackSave2);
			} else
			{
				callerNParam = callerFDef->nParams;
			}

			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack = stackSave - 1;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
	return ops;
}

fglOp *op_callPPackSafe ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr )
{
	VAR *caller;
	VAR *stackSave;
	VAR *stackSave2;
	bcFuncDef *callerFDef;
	uint32_t				 callerAtom;
	uint32_t 				 pIndex;
	uint32_t				 callerNParam;
	bcClass *callerClass;
	bcClassSearchEntry *cEntry;

	stackSave = instance->stack;
	callerNParam = ops->imm.dual.val2;

	caller = instance->stack - 1;
	while ( TYPE ( caller ) == slangType::eREFERENCE ) caller = caller->dat.ref.v;

	switch ( caller->type )
	{
		case slangType::eCODEBLOCK_ROOT:
			// build the passed parameters
			stackSave2 = instance->stack;
			instance->stack = instance->interpretBCParam ( funcDef, 0, ops + 1, nParams, instance->stack, basePtr );

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			callerNParam = (uint32_t)(instance->stack - stackSave2);

			op_cbCall ( instance, ops + 1, funcDef, basePtr, callerNParam );
			instance->stack = stackSave - 1;
			break;
		case slangType::eSTRING:
		case slangType::eATOM:
			if ( caller->type == slangType::eSTRING )
			{
				callerAtom = instance->atomTable->atomFind ( caller->dat.str.c );
			} else
			{
				callerAtom = caller->dat.atom;
			}
			callerFDef = instance->atomTable->atomGetFunc ( callerAtom );

			if ( !callerFDef->isConst )
			{
				throw errorNum::scSAFE_CALL;
			}

			// we have no idea how many actual parameters will be pushed... for safety we need to push sufficient NULL's to make up the difference
			for ( pIndex = callerFDef->nParams; pIndex > callerNParam; pIndex-- )
			{
				(instance->stack++)->type = slangType::eNULL;
			}

			stackSave2 = instance->stack;

			// build the passed parameters
			instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			if ( (uint32_t)(instance->stack - stackSave2) > callerFDef->nParams )
			{
				// we've passed more parameters than necessary
				// TODO: throw an error if the function isn't var paramable (need to add mark)
				callerNParam = (uint32_t)(instance->stack - stackSave2);
			} else
			{
				callerNParam = callerFDef->nParams;
			}

			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef->atom, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack = stackSave - 1;
			break;
		case slangType::eOBJECT_ROOT:
			caller = --instance->stack;

			callerNParam = ops->imm.dual.val2;

			if ( caller->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}
			callerClass = caller->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovFuncCall)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

			if ( cEntry->isVirtual )
			{
				callerFDef = instance->atomTable->atomGetFunc ( caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
			} else
			{
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}

			if ( !callerFDef->isConst )
			{
				throw errorNum::scSAFE_CALL;
			}

			// we have no idea how many actual parameters will be pushed... for safety we need to push sufficient NULL's to make up the difference
			for ( pIndex = callerFDef->nParams; pIndex > callerNParam; pIndex-- )
			{
				(instance->stack++)->type = slangType::eNULL;
			}

			stackSave2 = instance->stack;

			// build the passed parameters
			instance->stack = instance->interpretBCParam ( funcDef, callerFDef, ops + 1, nParams, instance->stack, basePtr );

			// the address of our next op is right after our code/ret sequence for paramaeter building
			ops = &funcDef->loadImage->csBase[ops->imm.dual.val1];

			if ( instance->debug ) instance->debug->IP = ops;

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[caller->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( caller, &caller->dat.ref.v[cEntry->methodAccess.objectStart] );
			}
			if ( (uint32_t)(instance->stack - stackSave2) > callerFDef->nParams )
			{
				// we've passed more parameters than necessary
				// TODO: throw an error if the function isn't var paramable (need to add mark)
				callerNParam = (uint32_t)(instance->stack - stackSave2);
			} else
			{
				callerNParam = callerFDef->nParams;
			}

			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops, callerNParam );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerNParam );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack = stackSave - 1;
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
	return ops;
}
