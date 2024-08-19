/*

   Object/Class support - SLANG compiler

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "bcInterpreter/op_variant.h"
#include "bcVM/vmDebug.h"

vmDebugLocalVar::vmDebugLocalVar ( vmInstance *instance, char const *name, VAR *var )
{
	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}

	this->name = _strdup ( name );
	this->var = var;
	valueType = vmDebugLocalVar::debugValueType::vmVarVar;

	buildData ( instance, var );
}

vmDebugLocalVar::vmDebugLocalVar ( vmInstance *instance, int64_t num, VAR *var )
{
	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}
	sprintf_s ( tmpName, sizeof ( tmpName ), "%4I64u", num );
	this->name = tmpName;
	this->var = var;
	valueType = vmDebugLocalVar::debugValueType::vmVarVar;

	buildData ( instance, var );
}

vmDebugLocalVar::vmDebugLocalVar ( vmInstance *instance, double num, VAR *var )
{
	while ( TYPE ( var ) == slangType::eREFERENCE )
	{
		var = var->dat.ref.v;
	}
	sprintf_s ( tmpName, sizeof ( tmpName ), "%g", num );
	this->name = tmpName;
	this->var = var;
	valueType = vmDebugLocalVar::debugValueType::vmVarVar;

	buildData ( instance, var );
}

vmDebugLocalVar::vmDebugLocalVar ( vmInstance *instance, char const *name, uint8_t *data, size_t dataLen )
{
	this->name = _strdup ( name );
	this->cPtr = data;
	this->cLen = dataLen;
	hasChild = false;
	valueType = vmDebugLocalVar::debugValueType::vmVarDynamicPtr;
}

vmDebugLocalVar::vmDebugLocalVar ( vmInstance *instance, char const *name, const uint8_t *data, size_t dataLen )
{
	this->name = _strdup ( name );
	this->cPtr = (uint8_t *)data;
	this->cLen = dataLen;
	valueType = vmDebugLocalVar::debugValueType::vmVarStaticPtr;
	hasChild = false;
}

vmDebugLocalVar::vmDebugLocalVar ( const vmDebugLocalVar &old )
{
	name = _strdup ( old.name );
	var = old.var;
	hasChild = old.hasChild;
	isString = old.isString;
	valueType = old.valueType;
	cPtr = old.cPtr;
	cLen = old.cLen;

	memcpy ( data, old.data, sizeof ( old.data ) );
}

stringi vmDebugLocalVar::getType ()
{
	switch ( valueType )
	{
		case vmDebugLocalVar::debugValueType::vmVarStaticPtr:
		case vmDebugLocalVar::debugValueType::vmVarDynamicPtr:
		case vmDebugLocalVar::debugValueType::vmVarVar:
			return "string";
		case vmDebugLocalVar::debugValueType::vmVarData:
			switch ( var->type )
			{
				case slangType::eDOUBLE:
					return "double";
				case slangType::eLONG:
					return "integer";
				case slangType::eSTRING:
					return "string";
				case slangType::eARRAY_ROOT:
					switch ( var->dat.ref.v->type )
					{
						case slangType::eARRAY_SPARSE:
							return "dynamic array";
						case slangType::eARRAY_FIXED:
							return "fixed array";
						default:
							throw errorNum::scINTERNAL;
					}
					break;
				case slangType::eCODEBLOCK_ROOT:
					return "codeblock";
				case slangType::eNULL:
					return "null";
				case slangType::eOBJECT_ROOT:
					return stringi ( "object " ) + var->dat.ref.v->dat.obj.classDef->name;
				case slangType::eATOM:
					return "function";
				default:
					return "(internal)";
			}
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

void vmDebug::reset ()
{
	halt = false;
	halted = false;
	terminate = false;
	isStepping = false;
	IP = nullptr;
	stack = nullptr;

	breakPoints.clear ();

}
vmBPType vmDebug::getTracePointType ( fglOp *op )
{
	dbTmpBP *bp;

	for ( bp = tmpBP; bp; bp = bp->next )
	{
		if ( bp->address == op )
		{
			return bp->type;
		}
	}
	throw errorNum::scINTERNAL;
}

void vmDebug::RemoveTracePoint ( void )
{
	dbTmpBP *next;
	dbTmpBP *bp;

	if ( !tmpBP ) return;

	bp = tmpBP;

	while ( bp )
	{
		next = bp->next;
		*(bp->address) = bp->op;
		bp->next = bpFree;
		bpFree = bp;
		bp = next;
	}
	tmpBP = 0;
}

void vmDebug::RemoveTracePoint ( fglOp *op, vmBreakpoint const *vmBp )
{
	dbTmpBP *prev;
	dbTmpBP *bp;

	if ( !tmpBP ) return;

	bp = tmpBP;
	prev = 0;
	while ( bp )
	{
		if ( bp->address == op && bp->vmBp == vmBp )
		{
			*(bp->address) = bp->op;
			if ( prev )
			{
				prev->next = bp->next;
			} else
			{
				tmpBP = bp->next;
				bp->next = bpFree;
				bpFree = bp;
			}
			break;
		}
		prev = bp;
		bp = bp->next;
	}
}

void vmDebug::SetTracePoint ( vmBPType type, fglOp *op, vmBreakpoint const *vmBp )
{
	dbTmpBP *bp;

	if ( bpFree )
	{
		bp = bpFree;
		bpFree = bp->next;
	} else
	{
		bp = new dbTmpBP();

		if ( !bp ) throw errorNum::scMEMORY;
	}
	bp->next = tmpBP;
	bp->type = type;
	bp->op = *op;
	bp->address = op;
	bp->vmBp = vmBp;
	op->op = fglOpcodes::debugTrace;
	op->imm.bpInformation = bp;
	tmpBP = bp;
}

uint32_t vmDebug::GetCurrentLine ( bcFuncDef *funcDef, fglOp *op )
{
	bcLoadImage *image;

	image = funcDef->loadImage;
	image->populateDebug ();

	if ( !image->nDebugEntries ) return 0;		// no debug info present

	auto it = std::upper_bound ( image->debugByOp.begin (), image->debugByOp.end (), fgxDebugInfo{ 0, 0, op, 0 }, bcDebugEntryCompareByOp () );

	if ( it == image->debugByOp.end () )
	{
		return 0;
	}
	if ( (it->op > op) && !(it == image->debugByOp.begin ()) ) it--;
	if ( it->sourceIndex )
	{
		return it->lineNum;
	}
	return 0;
}

uint32_t vmDebug::GetCurrentSourceIndex ( bcFuncDef *funcDef, fglOp *op )
{
	bcLoadImage *image;

	image = funcDef->loadImage;
	image->populateDebug ();

	if ( !image->nDebugEntries ) return 0;		// no debug info present

	auto it = std::upper_bound ( image->debugByOp.begin (), image->debugByOp.end (), fgxDebugInfo{ 0, 0, op, 0 }, bcDebugEntryCompareByOp () );

	if ( (it->op > op) && !(it == image->debugByOp.begin ()) ) it--;

	return it->sourceIndex;
}

// step by line... step into steps through calls
void vmDebug::StepLine ( vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack )
{
	auto image = funcDef->loadImage;
	image->populateDebug ();

	if ( image->nDebugEntries < 2 ) return;		// no debug info present

	auto it = std::upper_bound ( image->debugByOp.begin (), image->debugByOp.end (), fgxDebugInfo{ 0, 0, op, 0 }, bcDebugEntryCompareByOp () );

	if ( (it->op > op) && !(it == image->debugByOp.begin ()) ) it--;

	fglOp *opStart = (*it).op;
	fglOp *opEnd;
	it++;
	if ( it == image->debugByOp.end () )
	{
		opEnd = image->debugByOp.crbegin ()->op;
	} else
	{
		opEnd = it->op;
	}

	if ( !opStart || !opEnd ) return;		// must be exiting or no debug info
	while ( op < opEnd )
	{
		switch ( op->op )
		{
			case fglOpcodes::ret:
				// find the caller and set a temporary breakpoint on the return instruction
				VAR *tmpStack;

				tmpStack = stack;
				for ( ; tmpStack > instance->eval; )
				{
					tmpStack--;
					if ( tmpStack->type == slangType::ePCALL && (tmpStack->dat.pCall.func->conv == fgxFuncCallConvention::opFuncType_Bytecode) )
					{
						// if no op then we're in main and will finish upon step					
						if ( tmpStack->dat.pCall.op )
						{
							SetTracePoint ( vmBPType::vmBPType_Trace, tmpStack->dat.pCall.op, nullptr );
						}
						break;
					}
				}
				break;
			case fglOpcodes::jmp:
			case fglOpcodes::jmpc:
			case fglOpcodes::jmpcv:
			case fglOpcodes::jmpcpop:
			case fglOpcodes::jmpcpopv:
			case fglOpcodes::jmpcpop2:
			case fglOpcodes::jmpnc:
			case fglOpcodes::jmpncv:
			case fglOpcodes::jmpncpop:
			case fglOpcodes::jmpncpopv:
			case fglOpcodes::jmpnnull:
				if ( (&image->csBase[op->imm.index] < opStart) || (&image->csBase[op->imm.index] > opEnd) )
				{
					// destination of the jump is outside of current code emitted for this line... set a temporary breakpoint on the destination
					SetTracePoint ( vmBPType::vmBPType_Trace, &image->csBase[op->imm.index], nullptr );
				}
				break;
			case fglOpcodes::once:
				if ( (&image->csBase[op->imm.dual.val1] < opStart) || (&image->csBase[op->imm.dual.val1] > opEnd) )
				{
					// destination of the jump is outside of current code emitted for this line... set a temporary breakpoint on the destination
					SetTracePoint ( vmBPType::vmBPType_Trace, &image->csBase[op->imm.dual.val1], nullptr );
				}
				break;
			default:
				break;
		}
		op++;
	}
	if ( opEnd )
	{
		// start of NEXT line... which is really where we want to be so long as we don't jump or call anything (which is taken care above)
		SetTracePoint ( vmBPType::vmBPType_Trace, opEnd, nullptr );
	}
}

VAR *getBasePtr ( bcFuncDef *funcDef, VAR *stack )
{
	while ( stack->type != slangType::ePCALL ) stack--;
	return stack - funcDef->nParams;
}

void vmDebug::StepOp ( vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack, bool stepInto, vmBPType type )
{
	VAR *dest;
	VAR *basePtr;
	VAR *caller;
	bcFuncDef *callerFDef;
	bcClassSearchEntry *cEntry;
	fgxOvOp					 ovOp;
	char const *message;

	callerFDef = 0;

	switch ( op->op )
	{
		case fglOpcodes::ret:
			// find the caller and set a temporary breakpoint on the return instruction
			for ( ;; )
			{
				stack--;
				if ( stack->type == slangType::ePCALL && (stack->dat.pCall.func->conv == fgxFuncCallConvention::opFuncType_Bytecode) )
				{
					// if no op then we're in main and will finish upon step					
					if ( stack->dat.pCall.op )
					{
						SetTracePoint ( type, stack->dat.pCall.op, nullptr );
					}
					break;
				}
			}
			break;
		case fglOpcodes::jmp:
		case fglOpcodes::jmpc:
		case fglOpcodes::jmpcv:
		case fglOpcodes::jmpcpop:
		case fglOpcodes::jmpcpopv:
		case fglOpcodes::jmpcpop2:
		case fglOpcodes::jmpnc:
		case fglOpcodes::jmpncv:
		case fglOpcodes::jmpncpop:
		case fglOpcodes::jmpncpopv:
		case fglOpcodes::jmpnnull:
			// destination of the jump is outside of current code emitted for this line... set a temporary breakpoint on the destination
			SetTracePoint ( type, &funcDef->loadImage->csBase[op->imm.index], nullptr );
			break;
		case fglOpcodes::callC:
		case fglOpcodes::callCPop:
			break;
		case fglOpcodes::callBC:
		case fglOpcodes::callBCPop:
		case fglOpcodes::callBCTail:
			if ( stepInto )
			{
				callerFDef = instance->atomTable->atomGetFunc ( (uint32_t)op->imm.dual.val1 );
				SetTracePoint ( type, &callerFDef->cs[0], nullptr );
			} else
			{
				SetTracePoint ( type, op, nullptr );
			}
			break;
		case fglOpcodes::callV:
		case fglOpcodes::callVPop:
		case fglOpcodes::callPPack:
		case fglOpcodes::callPPackPop:
			caller = stack - 1;
			while ( TYPE ( caller ) == slangType::eREFERENCE ) caller = caller->dat.ref.v;
			switch ( TYPE ( caller ) )
			{
				case slangType::eSTRING:
					callerFDef = instance->atomTable->atomGetFunc ( instance->atomTable->atomFind ( caller->dat.str.c ) );
					break;
				case slangType::eATOM:
					callerFDef = instance->atomTable->atomGetFunc ( caller->dat.atom );
					break;
				default:
					callerFDef = nullptr;
					break;
			}
			break;
		case fglOpcodes::callBCVirt:
		case fglOpcodes::callBCVirtPop:
			callerFDef = instance->atomTable->atomGetFunc ( (stack - 1)->dat.ref.v->dat.obj.vPtr[op->imm.objOp.objectIndex].atom );
			break;
		case fglOpcodes::store:
		case fglOpcodes::storePop:
		case fglOpcodes::storeArray:
		case fglOpcodes::storeArrayPop:
		case fglOpcodes::storeLocalv:
		case fglOpcodes::storeLocalPopv:
		case fglOpcodes::storeGlobalv:
		case fglOpcodes::storeGlobalPopv:
		case fglOpcodes::incLocalv:
		case fglOpcodes::incRef:
		case fglOpcodes::incGlobalv:
		case fglOpcodes::decLocalv:
		case fglOpcodes::decGlobalv:
		case fglOpcodes::decRef:
			ovOp = (fgxOvOp)op->imm.dual.val2;
			switch ( op->op )
			{
				case fglOpcodes::storeLocalv:
				case fglOpcodes::storeLocalPopv:
					basePtr = getBasePtr ( funcDef, stack );
					dest = &basePtr[op->imm.index];
					ovOp = fgxOvOp::ovAssign;
					break;
				case fglOpcodes::incLocalv:
				case fglOpcodes::decLocalv:
					basePtr = getBasePtr ( funcDef, stack );
					dest = &basePtr[op->imm.dual.val1];
					break;
				case fglOpcodes::storeGlobalv:
				case fglOpcodes::storeGlobalPopv:
					dest = funcDef->loadImage->globals[op->imm.index];
					ovOp = fgxOvOp::ovAssign;
					break;
				case fglOpcodes::incGlobalv:
				case fglOpcodes::decGlobalv:
					dest = funcDef->loadImage->globals[op->imm.dual.val1];
					break;
				case fglOpcodes::incRef:
				case fglOpcodes::decRef:
					dest = stack - 1;
					if ( TYPE ( dest ) != slangType::eREFERENCE ) goto breakNextOp;
					break;
				case fglOpcodes::store:
				case fglOpcodes::storePop:
					dest = stack - 1;
					break;
				case fglOpcodes::storeArray:
				case fglOpcodes::storeArrayPop:
					dest = stack - 1;
					ovOp = fgxOvOp::ovArrayDerefLHS;
					break;
				default:
					throw errorNum::scINTERNAL;
			}

			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;

			switch ( TYPE ( dest ) )
			{
				case slangType::eOBJECT_ROOT:
					dest = dest->dat.ref.v;
					if ( dest->type == slangType::eOBJECT )
					{
						break;
					}
					// intentional fallthorugh;
				case slangType::eOBJECT:
					cEntry = &dest->dat.obj.classDef->elements[dest->dat.obj.classDef->ops[int ( ovOp )] - 1];

					// see if it's really overridden or not
					if ( !dest->dat.obj.classDef->ops[int ( ovOp )] ) goto breakNextOp;

					if ( cEntry->isVirtual )
					{
						callerFDef = instance->atomTable->atomGetFunc ( dest->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
					} else
					{
						callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
					}
					break;
				default:
					callerFDef = nullptr;
					break;
			}
			break;
		case fglOpcodes::storeLocalAdd:
		case fglOpcodes::storeLocalSub:
		case fglOpcodes::storeLocalMul:
		case fglOpcodes::storeLocalDiv:
		case fglOpcodes::storeLocalMod:
		case fglOpcodes::storeLocalPow:
		case fglOpcodes::storeLocalBwAnd:
		case fglOpcodes::storeLocalBwOr:
		case fglOpcodes::storeLocalBwXor:
		case fglOpcodes::storeLocalShLeft:
		case fglOpcodes::storeLocalShRight:
		case fglOpcodes::storeGlobalAdd:
		case fglOpcodes::storeGlobalSub:
		case fglOpcodes::storeGlobalMul:
		case fglOpcodes::storeGlobalDiv:
		case fglOpcodes::storeGlobalMod:
		case fglOpcodes::storeGlobalPow:
		case fglOpcodes::storeGlobalBwAnd:
		case fglOpcodes::storeGlobalBwOr:
		case fglOpcodes::storeGlobalBwXor:
		case fglOpcodes::storeGlobalShLeft:
		case fglOpcodes::storeGlobalShRight:
			switch ( op->op )
			{
				case fglOpcodes::storeLocalAdd:
				case fglOpcodes::storeLocalSub:
				case fglOpcodes::storeLocalMul:
				case fglOpcodes::storeLocalDiv:
				case fglOpcodes::storeLocalMod:
				case fglOpcodes::storeLocalPow:
				case fglOpcodes::storeLocalBwAnd:
				case fglOpcodes::storeLocalBwOr:
				case fglOpcodes::storeLocalBwXor:
				case fglOpcodes::storeLocalShLeft:
				case fglOpcodes::storeLocalShRight:
					basePtr = getBasePtr ( funcDef, stack );
					dest = &basePtr[op->imm.dual.val1];
					break;
				case fglOpcodes::storeGlobalAdd:
				case fglOpcodes::storeGlobalSub:
				case fglOpcodes::storeGlobalMul:
				case fglOpcodes::storeGlobalDiv:
				case fglOpcodes::storeGlobalMod:
				case fglOpcodes::storeGlobalPow:
				case fglOpcodes::storeGlobalBwAnd:
				case fglOpcodes::storeGlobalBwOr:
				case fglOpcodes::storeGlobalBwXor:
				case fglOpcodes::storeGlobalShLeft:
				case fglOpcodes::storeGlobalShRight:
					dest = funcDef->loadImage->globals[op->imm.dual.val1];
					break;
				case fglOpcodes::storeRefAdd:
				case fglOpcodes::storeRefSub:
				case fglOpcodes::storeRefMul:
				case fglOpcodes::storeRefDiv:
				case fglOpcodes::storeRefMod:
				case fglOpcodes::storeRefPow:
				case fglOpcodes::storeRefBwAnd:
				case fglOpcodes::storeRefBwOr:
				case fglOpcodes::storeRefBwXor:
				case fglOpcodes::storeRefShLeft:
				case fglOpcodes::storeRefShRight:
					dest = stack - 2;
					if ( TYPE ( dest ) != slangType::eREFERENCE ) goto breakNextOp;
					break;
				case fglOpcodes::storeClassIVarAdd:
				case fglOpcodes::storeClassIVarSub:
				case fglOpcodes::storeClassIVarMul:
				case fglOpcodes::storeClassIVarDiv:
				case fglOpcodes::storeClassIVarMod:
				case fglOpcodes::storeClassIVarPow:
				case fglOpcodes::storeClassIVarBwAnd:
				case fglOpcodes::storeClassIVarBwOr:
				case fglOpcodes::storeClassIVarBwXor:
				case fglOpcodes::storeClassIVarShLeft:
				case fglOpcodes::storeClassIVarShRight:
					basePtr = getBasePtr ( funcDef, stack );
					dest = basePtr[op->imm.objOp.stackIndex].dat.ref.v;
					dest = &dest[op->imm.objOp.objectIndex];
					break;
				case fglOpcodes::storeClassVirtIVarAdd:
				case fglOpcodes::storeClassVirtIVarSub:
				case fglOpcodes::storeClassVirtIVarMul:
				case fglOpcodes::storeClassVirtIVarDiv:
				case fglOpcodes::storeClassVirtIVarMod:
				case fglOpcodes::storeClassVirtIVarPow:
				case fglOpcodes::storeClassVirtIVarBwAnd:
				case fglOpcodes::storeClassVirtIVarBwOr:
				case fglOpcodes::storeClassVirtIVarBwXor:
				case fglOpcodes::storeClassVirtIVarShLeft:
				case fglOpcodes::storeClassVirtIVarShRight:
					basePtr = getBasePtr ( funcDef, stack );
					dest = basePtr[op->imm.objOp.stackIndex].dat.ref.v;
					dest = &dest[dest->dat.obj.vPtr[op->imm.objOp.objectIndex].delta];
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			switch ( op->op )
			{
				case fglOpcodes::storeLocalAdd:
				case fglOpcodes::storeGlobalAdd:
				case fglOpcodes::storeRefAdd:
				case fglOpcodes::storeClassIVarAdd:
					ovOp = fgxOvOp::ovAddAssign;
					break;
				case fglOpcodes::storeLocalSub:
				case fglOpcodes::storeGlobalSub:
				case fglOpcodes::storeRefSub:
				case fglOpcodes::storeClassIVarSub:
					ovOp = fgxOvOp::ovSubAssign;
					break;
				case fglOpcodes::storeLocalMul:
				case fglOpcodes::storeGlobalMul:
				case fglOpcodes::storeRefMul:
				case fglOpcodes::storeClassIVarMul:
					ovOp = fgxOvOp::ovMulAssign;
					break;
				case fglOpcodes::storeLocalDiv:
				case fglOpcodes::storeGlobalDiv:
				case fglOpcodes::storeRefDiv:
				case fglOpcodes::storeClassIVarDiv:
					ovOp = fgxOvOp::ovDivAssign;
					break;
				case fglOpcodes::storeLocalMod:
				case fglOpcodes::storeGlobalMod:
				case fglOpcodes::storeRefMod:
				case fglOpcodes::storeClassIVarMod:
					ovOp = fgxOvOp::ovModAssign;
					break;
				case fglOpcodes::storeLocalPow:
				case fglOpcodes::storeGlobalPow:
				case fglOpcodes::storeRefPow:
				case fglOpcodes::storeClassIVarPow:
					ovOp = fgxOvOp::ovPowAssign;
					break;
				case fglOpcodes::storeLocalBwAnd:
				case fglOpcodes::storeGlobalBwAnd:
				case fglOpcodes::storeRefBwAnd:
				case fglOpcodes::storeClassIVarBwAnd:
					ovOp = fgxOvOp::ovBwAndAssign;
					break;
				case fglOpcodes::storeLocalBwOr:
				case fglOpcodes::storeGlobalBwOr:
				case fglOpcodes::storeRefBwOr:
				case fglOpcodes::storeClassIVarBwOr:
					ovOp = fgxOvOp::ovBwOrAssign;
					break;
				case fglOpcodes::storeLocalBwXor:
				case fglOpcodes::storeGlobalBwXor:
				case fglOpcodes::storeRefBwXor:
				case fglOpcodes::storeClassIVarBwXor:
					ovOp = fgxOvOp::ovBwXorAssign;
					break;
				case fglOpcodes::storeLocalShLeft:
				case fglOpcodes::storeGlobalShLeft:
				case fglOpcodes::storeRefShLeft:
				case fglOpcodes::storeClassIVarShLeft:
					ovOp = fgxOvOp::ovShLeftAssign;
					break;
				case fglOpcodes::storeLocalShRight:
				case fglOpcodes::storeGlobalShRight:
				case fglOpcodes::storeRefShRight:
				case fglOpcodes::storeClassIVarShRight:
					ovOp = fgxOvOp::ovShRightAssign;
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;

			switch ( TYPE ( dest ) )
			{
				case slangType::eOBJECT_ROOT:
					dest = dest->dat.ref.v;
					if ( dest->type == slangType::eOBJECT )
					{
						break;
					}
					// intentional fallthorugh;
				case slangType::eOBJECT:
					if ( !dest->dat.obj.classDef->ops[int ( ovOp )] )
					{
						goto breakNextOp;
					}
					cEntry = &dest->dat.obj.classDef->elements[dest->dat.obj.classDef->ops[int ( ovOp )] - 1];

					if ( cEntry->isVirtual )
					{
						callerFDef = instance->atomTable->atomGetFunc ( dest->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
					} else
					{
						callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
					}
					break;
				default:
					callerFDef = nullptr;
					break;
			}
			break;
		case fglOpcodes::addv:
		case fglOpcodes::subv:
		case fglOpcodes::divv:
		case fglOpcodes::mulv:
		case fglOpcodes::modv:
		case fglOpcodes::powv:
		case fglOpcodes::maxv:
		case fglOpcodes::minv:
		case fglOpcodes::ltv:
		case fglOpcodes::lteqv:
		case fglOpcodes::gtv:
		case fglOpcodes::gteqv:
		case fglOpcodes::eqv:
		case fglOpcodes::eqv1:
		case fglOpcodes::neqv:
		case fglOpcodes::bworv:
		case fglOpcodes::bwandv:
		case fglOpcodes::bwxorv:
		case fglOpcodes::shlv:
		case fglOpcodes::shrv:
		case fglOpcodes::arrDeref:
		case fglOpcodes::arrDerefRef:
		case fglOpcodes::objCallFuncOv:
		case fglOpcodes::objCallFuncOvPop:
			dest = stack - 2;

			switch ( op->op )
			{
				case fglOpcodes::addv:
					ovOp = fgxOvOp::ovAdd;
					break;
				case fglOpcodes::subv:
					ovOp = fgxOvOp::ovSubtract;
					break;
				case fglOpcodes::divv:
					ovOp = fgxOvOp::ovDivide;
					break;
				case fglOpcodes::mulv:
					ovOp = fgxOvOp::ovMultiply;
					break;
				case fglOpcodes::modv:
					ovOp = fgxOvOp::ovModulus;
					break;
				case fglOpcodes::powv:
					ovOp = fgxOvOp::ovPower;
					break;
				case fglOpcodes::maxv:
					ovOp = fgxOvOp::ovMax;
					break;
				case fglOpcodes::minv:
					ovOp = fgxOvOp::ovMin;
					break;
				case fglOpcodes::ltv:
					ovOp = fgxOvOp::ovLess;
					break;
				case fglOpcodes::lteqv:
					ovOp = fgxOvOp::ovLessEqual;
					break;
				case fglOpcodes::gtv:
					ovOp = fgxOvOp::ovGreater;
					break;
				case fglOpcodes::gteqv:
					ovOp = fgxOvOp::ovGreaterEqual;
					break;
				case fglOpcodes::eqv:
				case fglOpcodes::eqv1:
					ovOp = fgxOvOp::ovEqual;
					break;
				case fglOpcodes::neqv:
					ovOp = fgxOvOp::ovNotEqual;
					break;
				case fglOpcodes::bworv:
					ovOp = fgxOvOp::ovBitOr;
					break;
				case fglOpcodes::bwandv:
					ovOp = fgxOvOp::ovBitAnd;
					break;
				case fglOpcodes::bwxorv:
					ovOp = fgxOvOp::ovBitXor;
					break;
				case fglOpcodes::shlv:
					ovOp = fgxOvOp::ovShRightAssign;
					break;
				case fglOpcodes::shrv:
					ovOp = fgxOvOp::ovShLeftAssign;
					break;
				case fglOpcodes::arrDeref:
					ovOp = fgxOvOp::ovArrayDerefRHS;
					break;
				case fglOpcodes::arrDerefRef:
					ovOp = fgxOvOp::ovArrayDerefLHS;
					break;
				case fglOpcodes::objCallFuncOv:
				case fglOpcodes::objCallFuncOvPop:
					ovOp = fgxOvOp::ovFuncCall;
					dest = stack - 1;
					break;
				default:
					throw errorNum::scINTERNAL;
			}

			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;

			switch ( TYPE ( dest ) )
			{
				case slangType::eOBJECT_ROOT:
					dest = dest->dat.ref.v;
					if ( dest->type == slangType::eOBJECT )
					{
						break;
					}
					[[fallthrough]];
				case slangType::eOBJECT:
					if ( !dest->dat.obj.classDef->ops[int ( ovOp )] )
					{
						goto breakNextOp;
					}
					cEntry = &dest->dat.obj.classDef->elements[dest->dat.obj.classDef->ops[int ( ovOp )] - 1];

					if ( cEntry->isVirtual )
					{
						callerFDef = instance->atomTable->atomGetFunc ( dest->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
					} else
					{
						callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
					}
						break;
				default:
					callerFDef = nullptr;
					break;
			}
			break;
		case fglOpcodes::objConstruct:
			{
				auto cls = instance->atomTable->atomGetClass ( (uint32_t) op->imm.dual.val1 );
				if ( cls->newEntry )
				{
					callerFDef = instance->atomTable->atomGetFunc ( cls->newEntry->methodAccess.atom );
				} else
				{
					callerFDef = nullptr;
				}
			}
			break;
		case fglOpcodes::objConstructV:
			dest = stack - 1;
			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;
			if ( TYPE ( dest ) != slangType::eSTRING ) goto breakNextOp;

			callerFDef = instance->atomTable->atomGetFunc ( instance->atomTable->atomGetClass ( dest->dat.str.c )->newEntry->methodAccess.atom );
			break;
		case fglOpcodes::objRelease:
			dest = stack - 1;
			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;
			if ( TYPE ( dest ) != slangType::eOBJECT_ROOT ) goto breakNextOp;
			if ( dest->dat.ref.v->type != slangType::eOBJECT ) goto breakNextOp;
			dest = &dest->dat.ref.v[1];
			callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.classDef->releaseEntry->methodAccess.atom );
			break;
		case fglOpcodes::objAccess:
		case fglOpcodes::objAccessInd:
		case fglOpcodes::objAccessRef:
		case fglOpcodes::objAccessRefInd:
		case fglOpcodes::objCall:
		case fglOpcodes::objCallPop:
		case fglOpcodes::objCallPPack:
		case fglOpcodes::objCallPPackPop:
			switch ( op->op )
			{
				case fglOpcodes::objCall:
				case fglOpcodes::objCallPop:
				case fglOpcodes::objCallPPack:
				case fglOpcodes::objCallPPackPop:
					if ( TYPE ( stack - 2 ) != slangType::eSTRING ) goto breakNextOp;
					message = (stack - 2)->dat.str.c;
					dest = stack - 1;
					break;
				case fglOpcodes::objAccess:
				case fglOpcodes::objAccessRef:
					dest = stack - 1;
					message = (char *)(funcDef->loadImage->dsBase + op->imm.index + sizeof ( size_t ));
					break;
				case fglOpcodes::objAccessInd:
				case fglOpcodes::objAccessRefInd:
					dest = stack - 1;
					if ( TYPE ( stack - 2 ) != slangType::eSTRING ) goto breakNextOp;
					message = (stack - 2)->dat.str.c;
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;
			if ( TYPE ( dest ) != slangType::eOBJECT_ROOT )
			{
				// handle call chaining
				if ( instance->atomTable->atomGetType ( message ) == atom::atomType::aFUNCDEF )
				{
					callerFDef = instance->atomTable->atomGetFunc ( message );
				} else
				{
					goto breakNextOp;
				}
			} else
			{
				if ( dest->dat.ref.v->type != slangType::eOBJECT ) goto breakNextOp;

				switch ( op->op )
				{
					case fglOpcodes::objCall:
					case fglOpcodes::objCallPop:
					case fglOpcodes::objCallPPack:
					case fglOpcodes::objCallPPackPop:
						cEntry = findClassEntry ( dest->dat.ref.v->dat.obj.classDef, message, fgxClassElemSearchType::fgxClassElemPrivateMethod );
						break;
					case fglOpcodes::objAccess:
					case fglOpcodes::objAccessRef:
					case fglOpcodes::objStore:
					case fglOpcodes::objStorePop:
						cEntry = findClassEntry ( dest->dat.ref.v->dat.obj.classDef, message, fgxClassElemSearchType::fgxClassElemPrivateAccess );
						break;
					case fglOpcodes::objAccessInd:
					case fglOpcodes::objAccessRefInd:
					case fglOpcodes::objStoreInd:
					case fglOpcodes::objStoreIndPop:
						cEntry = findClassEntry ( dest->dat.ref.v->dat.obj.classDef, message, fgxClassElemSearchType::fgxClassElemPrivateAssign );
						break;
					default:
						throw errorNum::scINTERNAL;
				}
				if ( !cEntry )
				{
					switch ( op->op )
					{
						case fglOpcodes::objCall:
						case fglOpcodes::objCallPop:
						case fglOpcodes::objCallPPack:
						case fglOpcodes::objCallPPackPop:
							cEntry = dest->dat.ref.v->dat.obj.classDef->defaultMethodEntry;
							break;
						case fglOpcodes::objAccess:
						case fglOpcodes::objAccessRef:
						case fglOpcodes::objAccessInd:
						case fglOpcodes::objAccessRefInd:
							cEntry = dest->dat.ref.v->dat.obj.classDef->defaultAccessEntry;
							break;
						case fglOpcodes::objStoreInd:
						case fglOpcodes::objStoreIndPop:
						case fglOpcodes::objStore:
						case fglOpcodes::objStorePop:
							cEntry = dest->dat.ref.v->dat.obj.classDef->defaultAssignEntry;
							break;
						default:
							throw errorNum::scINTERNAL;
					}
				}
				if ( !cEntry ) goto breakNextOp;

				switch ( cEntry->type )
				{
					case fgxClassElementType::fgxClassType_inherit:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
					case fgxClassElementType::fgxClassType_iVar:
						goto breakNextOp;
					case fgxClassElementType::fgxClassType_method:
						if ( cEntry->isVirtual )
						{
							callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
						} else
						{
							callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
						}
						break;
					case fgxClassElementType::fgxClassType_prop:
						if ( cEntry->methodAccess.atom == -1 ) goto breakNextOp;
						if ( cEntry->isVirtual )
						{
							callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
						} else
						{
							callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
						}
						break;
					default:
						goto breakNextOp;
				}
			}
			break;
		case fglOpcodes::objStore:
		case fglOpcodes::objStoreInd:
		case fglOpcodes::objStorePop:
		case fglOpcodes::objStoreIndPop:
			switch ( op->op )
			{
				case fglOpcodes::objStore:
				case fglOpcodes::objStorePop:
					dest = stack - 1;
					message = (char *)(funcDef->loadImage->dsBase + op->imm.index + sizeof ( size_t ));
					break;
				case fglOpcodes::objStoreInd:
				case fglOpcodes::objStoreIndPop:
					dest = stack - 1;
					if ( TYPE ( stack - 2 ) != slangType::eSTRING ) goto breakNextOp;
					message = (stack - 2)->dat.str.c;
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;
			if ( TYPE ( dest ) != slangType::eOBJECT_ROOT )
			{
				// handle call chaining
				if ( instance->atomTable->atomGetType ( message ) == atom::atomType::aFUNCDEF )
				{
					callerFDef = instance->atomTable->atomGetFunc ( message );
				} else
				{
					goto breakNextOp;
				}
			} else
			{
				if ( dest->dat.ref.v->type != slangType::eOBJECT ) goto breakNextOp;

				switch ( op->op )
				{
					case fglOpcodes::objStore:
					case fglOpcodes::objStorePop:
						cEntry = findClassEntry ( dest->dat.ref.v->dat.obj.classDef, message, fgxClassElemSearchType::fgxClassElemPrivateAccess );
						break;
					case fglOpcodes::objStoreInd:
					case fglOpcodes::objStoreIndPop:
						cEntry = findClassEntry ( dest->dat.ref.v->dat.obj.classDef, message, fgxClassElemSearchType::fgxClassElemPrivateAssign );
						break;
					default:
						throw errorNum::scINTERNAL;
				}
				if ( !cEntry )
				{
					switch ( op->op )
					{
						case fglOpcodes::objStoreInd:
						case fglOpcodes::objStoreIndPop:
						case fglOpcodes::objStore:
						case fglOpcodes::objStorePop:
							cEntry = dest->dat.ref.v->dat.obj.classDef->defaultAssignEntry;
							break;
						default:
							throw errorNum::scINTERNAL;
					}
				}
				if ( !cEntry ) goto breakNextOp;

				switch ( cEntry->type )
				{
					case fgxClassElementType::fgxClassType_inherit:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_method:
						goto breakNextOp;
						break;
					case fgxClassElementType::fgxClassType_prop:
						if ( cEntry->assign.atom == -1 ) goto breakNextOp;
						if ( cEntry->isVirtual )
						{
							callerFDef = instance->atomTable->atomGetFunc ( dest->dat.ref.v->dat.obj.vPtr[cEntry->assign.vTableEntry].atom );
						} else
						{
							callerFDef = instance->atomTable->atomGetFunc ( cEntry->assign.atom );
						}
						break;
					default:
						goto breakNextOp;
				}
			}
			break;
		case fglOpcodes::notv:
		case fglOpcodes::twocv:
			switch ( op->op )
			{
				case fglOpcodes::notv:
					ovOp = fgxOvOp::ovTwosComplement;
					break;
				case fglOpcodes::twocv:
					ovOp = fgxOvOp::ovNegate;
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			dest = stack - 1;

			while ( TYPE ( dest ) == slangType::eREFERENCE ) dest = dest->dat.ref.v;

			switch ( TYPE ( dest ) )
			{
				case slangType::eOBJECT_ROOT:
					dest = dest->dat.ref.v;
					if ( dest->type != slangType::eOBJECT )
					{
						break;
					}
					[[fallthrough]];
				case slangType::eOBJECT:
					if ( !dest->dat.obj.classDef->ops[int ( ovOp )] ) goto breakNextOp;

					cEntry = &dest->dat.obj.classDef->elements[dest->dat.obj.classDef->ops[int ( ovOp )] - 1];

					if ( cEntry->isVirtual )
					{
						callerFDef = instance->atomTable->atomGetFunc ( dest->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
					} else
					{
						callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
					}
					break;
				default:
					callerFDef = nullptr;
					break;
			}
			break;
		default:
			break;
	}
	if ( callerFDef )
	{
		if ( stepInto )
		{
			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					SetTracePoint ( type, &callerFDef->cs[0], nullptr );
					break;
				default:
					break;
			}
		}
	}
breakNextOp:
	SetTracePoint ( type, op + 1, nullptr );
}
void vmDebug::SetProfiling ( vmInstance *instance )
{
	instance->debug->isProfiling = true;

	instance->atomTable->typeMap ( atom::atomType::aFUNCDEF, [&]( uint32_t index )
								   {
									   funcDef = instance->atomTable->atomGetFunc ( index );
									   funcDef->execNCalls = 0;
									   funcDef->execTimeLocal = 0;
									   funcDef->execTimeTotal = 0;
									   return false;
								   }
	);
}

// step by line... step into steps through calls
void vmDebug::StepOut ( vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack )
{
	bcLoadImage *image;

	image = funcDef->loadImage;

	if ( image->nDebugEntries < 2 ) return;		// no debug info present

	while ( 1 )
	{
		switch ( op->op )
		{
			case fglOpcodes::ret:
				// find the caller and set a temporary breakpoint on the return instruction
				for ( ;; )
				{
					stack--;
					if ( stack->type == slangType::ePCALL && (stack->dat.pCall.func->conv == fgxFuncCallConvention::opFuncType_Bytecode) )
					{
						// if no op then we're in main and will finish upon step					
						if ( stack->dat.pCall.op )
						{
							SetTracePoint ( vmBPType::vmBPType_Trace, stack->dat.pCall.op, nullptr );
						}
						break;
					}
				}
				return;
			default:
				break;
		}
		op++;
	}
}

std::vector<size_t> vmDebug::GetGotoTargets ( vmInstance *instance, bcFuncDef *funcDef )
{
	auto image = funcDef->loadImage;
	image->populateDebug ();

	std::vector<size_t> lines;

	if ( image->nDebugEntries < 2 ) return lines;		// no debug info present

	auto it = std::upper_bound ( image->debugByOp.begin (), image->debugByOp.end (), fgxDebugInfo{ 0, 0, funcDef->cs, 0 }, bcDebugEntryCompareByOp () );
	if ( it != image->debugByOp.begin () )
	{
		it++;
	}
	while ( 1 )
	{
		auto targetOp = it->op;
		if ( targetOp < funcDef->cs )
		{
			continue;
		}
		if ( !targetOp || targetOp >= funcDef->lastCs )
		{
			break;
		}
		lines.push_back ( it->lineNum );
		it++;
	}
	return lines;
}

// step by line... step into steps through calls
bool vmDebug::SetIP ( vmInstance *instance, bcFuncDef *funcDef, char const *fileName, size_t line )
{
	fglOp *targetOp = 0;
	fglOp *op;
	bcLoadImage *image;
	size_t				 srcIndex;
	uint32_t			 targetStack = 0;

	image = funcDef->loadImage;
	image->populateDebug ();

	if ( image->nDebugEntries < 2 ) return false;		// no debug info present

	if ( (srcIndex = image->srcFiles.getStaticIndex ( fileName )) != -1 )
	{
		auto dIt = image->debugByLine.lower_bound ( fgxDebugInfo{ (uint32_t)srcIndex, (uint32_t)line, 0, 0 } );
		if ( dIt->sourceIndex )
		{
			/* see if it's outside current function scope */
			while ( dIt->sourceIndex == srcIndex && dIt->lineNum == line )
			{
				targetOp = dIt->op;
				targetStack = dIt->stackHeight;

				if ( !targetOp || targetOp < funcDef->cs || targetOp >= funcDef->lastCs )
				{
					dIt++;
					continue;
				}
				op = funcDef->cs;
				for ( ;; )
				{
					if ( op->op == fglOpcodes::ret )
					{
						break;
					}
					op++;
				}
				if ( targetOp > op ) return false;

				/* target op has new execution line */
				instance->debug->IP = targetOp;
				instance->debug->stackHeight = targetStack;
				return true;
			}
		}
	}
	return false;
}

void vmDebug::RunToLine ( vmInstance *instance, char const *fileName, size_t line )
{
	size_t	srcIndex = 0;

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   image->populateDebug ();

									   if ( (srcIndex = image->srcFiles.getStaticIndex ( fileName )) != -1 )
									   {
										   auto dIt = image->debugByLine.lower_bound ( fgxDebugInfo{ (uint32_t)srcIndex, (uint32_t)line, 0, 0 } );
										   if ( dIt != image->debugByLine.end () )
										   {
											   SetTracePoint ( vmBPType::vmBPType_Trace, dIt->op, nullptr );
											   return true;
										   }
									   }
									   return false;
								   }
	);
}

char const *vmDebugLocalVar::getName ( void )
{
	return name;
}

size_t vmDebugLocalVar::getNameLen ( void )
{
	return strlen ( name );
}

char const *vmDebugLocalVar::getValue ( void )
{
	switch ( valueType )
	{
		case vmDebugLocalVar::debugValueType::vmVarStaticPtr:
		case vmDebugLocalVar::debugValueType::vmVarDynamicPtr:
			return (char *)cPtr;
		case vmDebugLocalVar::debugValueType::vmVarData:
			return data;
		case vmDebugLocalVar::debugValueType::vmVarVar:
			return var->dat.str.c;
		default:
			throw errorNum::scINTERNAL;
	}
}

size_t vmDebugLocalVar::getValueLen ( void )
{
	switch ( valueType )
	{
		case vmDebugLocalVar::debugValueType::vmVarStaticPtr:
		case vmDebugLocalVar::debugValueType::vmVarDynamicPtr:
			return cLen;
		case vmDebugLocalVar::debugValueType::vmVarData:
			return strlen ( data );
		case vmDebugLocalVar::debugValueType::vmVarVar:
			return var->dat.str.len;
		default:
			throw errorNum::scINTERNAL;
	}
}

bool vmDebugLocalVar::hasChildren ( void )
{
	return hasChild;
}

bool vmDebugLocalVar::isStringValue ( void )
{
	return isString;
}

vmInspectList *vmDebugLocalVar::getChildren ( vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
{
	vmInspectList *vars;
	bcClassSearchEntry *entry;
	int32_t				 loop;
	VAR *var;

	var = this->var;
	for ( ;; )
	{
		while ( TYPE ( var ) == slangType::eREFERENCE ) var = var->dat.ref.v;

		switch ( TYPE ( var ) )
		{
			case slangType::eLONG:
			case slangType::eDOUBLE:
			case slangType::eNULL:
			case slangType::eATOM:
				return 0;
			case slangType::eCODEBLOCK:
				{
					fgxSymbols *fs;

					fs = (fgxSymbols *)((char *)var->dat.cb.cb + var->dat.cb.cb->symOffset);
					vars = new vmInspectList ();
					for ( loop = 0; loop < (int32_t)var->dat.cb.cb->nSymbols - (int32_t)var->dat.cb.cb->nParams; loop++ )
					{
						vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, ((char *)var->dat.cb.cb + fs[loop].offsetName), &var[loop + 1] )) );
					}
				}
				return vars;

			case slangType::eOBJECT:
				vars = new vmInspectList ();

				for ( loop = 0; loop < (int32_t) var->dat.obj.classDef->nElements; loop++ )
				{
					entry = &var->dat.obj.classDef->elements[loop];

					switch ( entry->type )
					{
						case fgxClassElementType::fgxClassType_iVar:
							if ( !entry->isVirtual )
							{
								vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, entry->name, &var[entry->methodAccess.iVarIndex] )) );
							}
							break;
						case fgxClassElementType::fgxClassType_static:
							vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, entry->name, funcDef->loadImage->globals[entry->methodAccess.data] )) );
							break;
						default:
							break;
					}
				}
				return vars;
			case slangType::eARRAY_SPARSE:
				vars = new vmInspectList ();
				var = var->dat.aSparse.v;
				while ( var )
				{
					vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, var->dat.aElem.elemNum, var->dat.aElem.var )) );
					var = var->dat.aElem.next;
				}
				return vars;
			case slangType::eARRAY_FIXED:
				vars = new vmInspectList ();
				for ( int64_t loop = 0; loop < (var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1); loop++ )
				{
					vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, (var->dat.arrayFixed.startIndex + loop), &var[loop + 1] )) );
				}
				return vars;
			case slangType::eOBJECT_ROOT:
				if ( var->dat.ref.v->type == slangType::eOBJECT )
				{
					if ( var->dat.ref.v->dat.obj.classDef->cInspectorCB )
					{
						return var->dat.ref.v->dat.obj.classDef->cInspectorCB ( instance, funcDef, var, (uint32_t)start, (uint32_t)end );
					} else
					{
						var = var->dat.ref.v;
					}
				} else
				{
					return 0;
				}
				break;
			case slangType::eARRAY_ROOT:
			case slangType::eCODEBLOCK_ROOT:
				var = var->dat.ref.v;
				break;
			case slangType::ePCALL:
				size_t	 paramNum;
				VAR *param;

				paramNum = var->dat.pCall.func->nSymbols;
				param = var;

				vars = new vmInspectList ();
				while ( paramNum )
				{
					param--;		// on entry this is pcall... first param is right below
					paramNum--;
					vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, var->dat.pCall.func->symbols[paramNum].name, param )) );
				}
				return vars;
			default:
				throw errorNum::scINTERNAL;
		}
	}
	return 0;
}

void vmDebugLocalVar::buildData ( vmInstance *instance, VAR *var )
{
	bcLoadImage *loadImage;

	*data = 0;
	hasChild = false;
	switch ( TYPE ( var ) )
	{
		case slangType::eSTRING:
			this->var = var;
			isString = true;
			break;
		default:
			isString = false;
			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "%I64i", var->dat.l );
					break;
				case slangType::eDOUBLE:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "%f", var->dat.d );
					break;
				case slangType::eOBJECT_ROOT:
				case slangType::eARRAY_ROOT:
				case slangType::eCODEBLOCK_ROOT:
					buildData ( instance, var->dat.ref.v );
					return;
				case slangType::eOBJECT:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "(object) %s", var->dat.obj.classDef->name );
					hasChild = true;
					break;
				case slangType::eARRAY_SPARSE:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "(array)[%I64i]", var->dat.aSparse.maxI );
					hasChild = true;
					break;
				case slangType::eARRAY_FIXED:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "(fixed array)[%I64i]", var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1 );
					hasChild = true;
					break;
				case slangType::eCODEBLOCK:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "(code block)" );
					hasChild = true;
					break;
				case slangType::eNULL:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "(null)" );
					break;
				case slangType::eATOM:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "(function) %s", instance->atomTable->atomGetName ( var->dat.atom ) );
					break;
				case slangType::ePCALL:
					{
						VAR *param;

						uint32_t paramNum = 0;
						auto used = sprintf_s ( data, sizeof ( data ), "%s ( ", var->dat.pCall.func->name );

						auto nParams = var->dat.pCall.nParams;
						param = var;

						loadImage = var->dat.pCall.func->loadImage;

						while ( nParams )
						{
							param--;		// on entry this is pcall... first param is right below
							if ( paramNum < var->dat.pCall.func->nParams )
							{
								vmDebugLocalVar	var2 ( instance, loadImage->symbols[paramNum].name, param );

								used += sprintf_s ( &data[(size_t)used], sizeof ( data ) - used, "%s", var2.getValue () );
							} else
							{
								vmDebugLocalVar	var2 ( instance, "", param );

								used += sprintf_s ( &data[(size_t)used], sizeof ( data ) - used, "%s", var2.getValue () );
							}
							nParams--;
							paramNum++;
							if ( nParams ) used += sprintf_s ( &data[(size_t)used], sizeof ( data ) - used, ", " );
						}
						sprintf_s ( &data[(size_t)used], sizeof ( data ) - used, " )" );

						if ( var->dat.pCall.func->nSymbols ) hasChild = true;
					}
					break;
				default:
					_snprintf_s ( data, sizeof ( data ), _TRUNCATE, "(internal error)" );
					//					throw errorNum::scINTERNAL;
					break;
			}
			valueType = vmDebugLocalVar::debugValueType::vmVarData;
			cLen = strlen ( data );
	}
}


vmCallstackEntry::vmCallstackEntry ( vmInstance *instance, bcFuncDef *func, size_t lineNum, size_t nParams, VAR *param )
{
	size_t paramNum;

	fileName = _strdup ( func->loadImage->srcFiles.getName ( instance->debug->GetCurrentSourceIndex ( func, func->cs ) ) );
	funcName = _strdup ( func->name );
	funcId = (size_t)func;
	line = lineNum;
	funcDef = func;
	paramNum = 0;

	while ( nParams )
	{
		param--;		// on entry this is pcall... first param is right below
		if ( func->isMethod && !param )
		{
			break;
		}
		if ( paramNum < func->nParams )
		{
			params.push_back ( static_cast<vmDebugVar *> (new vmDebugLocalVar ( instance, func->symbols[paramNum].name, param )) );
		} else
		{
			params.push_back ( static_cast<vmDebugVar *> (new vmDebugLocalVar ( instance, "", param )) );
		}
		nParams--;
		paramNum++;
	}
}

vmCallStack *vmDebug::GetCallstack ( vmInstance *instance )
{
	vmCallStack *callStack;
	VAR *stack;
	fglOp *op;

	stack = instance->stack - 1;

	callStack = new vmCallStack ();

	op = instance->debug->IP;
	while ( stack >= instance->eval )
	{
		if ( stack->type == slangType::ePCALL )
		{
			callStack->entry.push_back ( vmCallstackEntry ( instance, stack->dat.pCall.func, GetCurrentLine ( stack->dat.pCall.func, op ), stack->dat.pCall.nParams, stack ) );
			op = stack->dat.pCall.op;
		}
		stack--;
	}

	return callStack;
}

bcFuncDef *vmDebug::Func ( vmInstance *instance, size_t stackFrame )
{
	VAR *stack;

	stack = instance->stack - 1;

	while ( stack >= instance->eval )
	{
		if ( stack->type == slangType::ePCALL && !(stackFrame--) )
		{
			return stack->dat.pCall.func;
		}
		stack--;
	}

	return nullptr;
}

VAR *vmDebug::BasePtr ( vmInstance *instance, size_t stackFrame )
{
	VAR *stack;

	stack = instance->stack - 1;

	while ( stack >= instance->eval )
	{
		if ( stack->type == slangType::ePCALL && !(stackFrame--) )
		{
			return stack - stack->dat.pCall.func->nParams;
		}
		stack--;
	}

	return nullptr;
}

vmFunctionListEntry::vmFunctionListEntry ( vmInstance *instance, uint32_t atomNum )
{
	char	tempName[MAX_NAME_SZ];
	char	funcName[MAX_NAME_SZ];
	char	className[MAX_NAME_SZ];
	char	type[MAX_NAME_SZ];


	bcFuncDef *funcDef = instance->atomTable->atomGetFunc ( atomNum );

	if ( !funcDef || !funcDef->loadImage )
	{
		sprintf_s ( funcName, sizeof ( funcName ), "(%s) %s", "not linked", instance->atomTable->atomGetName ( atomNum ) );
		this->funcName = funcName;
		this->fileName = "unknown";
		this->atomNum = 0;
		this->lineNum = 0;
		this->className = "";
		this->isUsercode = false;
		return;
	}

	if ( (strncmp ( LAMBDA_ID, funcDef->name, sizeof ( LAMBDA_ID ) - 1 ) != 0) && (_at ( ".", (char *)funcDef->name ) != -1) )
	{
		_tokenn ( funcDef->name, ".", 1, className, sizeof ( className ) );
		_tokenn ( funcDef->name, ".", 2, tempName, sizeof ( tempName ) );
		_tokenn ( funcDef->name, ".", 3, type, sizeof ( type ) );
		if ( !*type )
		{
			strcpy_s ( type, sizeof ( type ), "lambda" );
		}
		sprintf_s ( funcName, sizeof ( funcName ), "(%s) %s", type, tempName );
		this->className = _strdup ( className );
	} else
	{
		this->className = "";
		strcpy_s ( funcName, funcDef->name );
	}
	this->funcName = funcName;
	this->fileName = funcDef->loadImage->srcFiles.getName( instance->debug->GetCurrentSourceIndex( funcDef, funcDef->cs ) );
	this->atomNum = funcDef->atom;
	this->lineNum = instance->debug->GetCurrentLine ( funcDef, funcDef->cs );

	this->isUsercode = true;
	switch ( funcDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
		case fgxFuncCallConvention::opFuncType_Bytecode_Native:
			if ( !strccmp ( this->fileName, "(INTERNAL)" ) )
			{
				this->isUsercode = false;
			}
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			this->isUsercode = false;
			break;
	}
}

vmFunctionList::vmFunctionList ( vmInstance *instance )
{
	instance->atomTable->typeMap ( atom::atomType::aFUNCDEF, [&]( uint32_t index )
								   {
									   entry.push_back ( vmFunctionListEntry ( instance, index ) );
									   return false;
								   }
	);
}

vmFunctionList *vmDebug::GetFunctionList ( vmInstance *instance )
{
	return new vmFunctionList ( instance );
}

void vmDebug::RegisterInspector ( char const *name, vmInspector::vmInspectorCb cb )
{
	if ( !(ptrdiff_t)this ) return;

	for ( auto &it : inspectors )
	{
		if ( !strccmp ( name, it.name ) )
		{
			return;
		}
	}
	inspectors.emplace_back (  name, cb );
}

// top level local inspector
static vmInspectList *debugLocalInspector ( vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack, vmDebugVar *pVar )
{
	vmInspectList *vars;
	size_t			 loop;
	bcLoadImage *image;

	vars = new vmInspectList ();

	stack -= funcDef->nParams;
	image = funcDef->loadImage;

	for ( loop = image->nSymbols; loop > 0; loop-- )
	{
		if ( ((uint8_t *)op >= (uint8_t *)image->csBase + image->symbols[loop - 1].validStart) && ((uint8_t *)op <= (uint8_t *)image->csBase + image->symbols[loop - 1].validEnd) )
		{
			std::string baseSymbolName;
			char const *str = image->symbols[loop - 1].name;
			while ( *str && *str != '$' )
			{
				baseSymbolName += *(str++);
			}

			if ( baseSymbolName.size () )
			{
				bool found = false;
				for ( auto &it : vars->entry )
				{
					if ( !strccmp ( (*it).getName (), baseSymbolName.c_str () ) )
					{
						found = true;
						break;
					}
				}

				if ( !found )
				{
					if ( image->symbols[loop - 1].isStackBased )
					{
						vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, baseSymbolName.c_str (), stack + image->symbols[loop - 1].index )) );
					} else
					{
						vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, baseSymbolName.c_str (), image->globals[image->symbols[loop - 1].index] )) );
					}
				}
			}
		}
	}

	if ( (op->op == fglOpcodes::ret) || ((op->op == fglOpcodes::debugCheckHalt) && (op[1].op == fglOpcodes::ret)) )
	{
		vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, "$ReturnValue", &instance->result )) );
	}

	return vars;
}

static vmInspectList *debugGlobalInspector ( vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack, vmDebugVar *pVar )
{
	char tmpName[MAX_NAME_SZ] = {};

	auto vars = new vmInspectList ();

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   for ( size_t loop = 0; loop < image->nGlobals; loop++ )
									   {
										   if ( image->globalSymbolTable[loop].isExportable )
										   {
											   sprintf_s ( tmpName, sizeof ( tmpName ), "%s:%s", image->name, image->globalSymbolTable[loop].name );

											   vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, tmpName, image->globals[image->globalSymbolTable[loop].index] )) );
										   }
									   }
									   return false;
								   }
	);
	return vars;
}

vmInspectorList *vmDebug::GetInspectors ( vmInstance *instance )
{
	vmInspectorList *list;

	list = new vmInspectorList ();

	for ( auto it = inspectors.begin (); it != inspectors.end (); it++ )
	{
		list->entry.push_back ( it->name );
	}

	return list;
}

// expansion callback
vmInspectList *vmDebug::Inspect ( vmInstance *instance, size_t stackFrame, vmDebugVar *var, uint32_t start, uint32_t end )
{
	VAR *stack;

	stack = instance->stack - 1;

	while ( stack >= instance->eval )
	{
		if ( stack->type == slangType::ePCALL )
		{
			if ( !stackFrame-- )
			{
				return var->getChildren ( instance, stack->dat.pCall.func, start, end );
			}
		}
		stack--;
	}
	throw errorNum::scINTERNAL;
}

// top level
vmInspectList *vmDebug::Inspect ( vmInstance *instance, size_t num, size_t stackFrame )
{
	VAR *stack;

	stack = instance->stack - 1;
	auto op = instance->debug->IP;
	while ( stack >= instance->eval )
	{
		if ( stack->type == slangType::ePCALL )
		{
			if ( !stackFrame-- )
			{
				return inspectors[num].cb ( instance, stack->dat.pCall.func, op, stack, 0 );
			}
			op = stack->dat.pCall.op;
		}
		stack--;
	}
	throw errorNum::scINTERNAL;
}

vmFunctionProfile::vmFunctionProfile ( vmInstance *instance, uint32_t atomNum )
{
	bcFuncDef *funcDef = instance->atomTable->atomGetFunc ( atomNum );

	funcName = funcDef->name;
	fileName = funcDef->loadImage->srcFiles.getName ( instance->debug->GetCurrentSourceIndex ( funcDef, funcDef->cs ) );
	numCalls = funcDef->execNCalls;
	numMicroSecsLocal = funcDef->execTimeLocal;
	numMicroSecsTotal = funcDef->execTimeTotal;
	lineNum = 0;
	percentTotal = 0;
	percentLocal = 0;
}

vmProfileData *vmDebug::GetExecutionProfile ( vmInstance *instance )
{
	vmProfileData *pData = new vmProfileData ();
	uint64_t		 totalExecTime = 0;

	instance->atomTable->typeMap ( atom::atomType::aFUNCDEF, [&]( uint32_t index )
								   {
									   auto func = instance->atomTable->atomGetFunc ( index );
									   if ( func && func->execNCalls )
									   {
										   pData->func.push_back ( vmFunctionProfile ( instance, index ) );
									   }
									   return false;
								   }
	);

	totalExecTime = instance->atomTable->atomGetFunc ( "main" )->execTimeTotal;

	size_t loop;
	for ( loop = 0; loop < (*pData).size (); loop++ )
	{
		(*pData)[loop]->percentLocal = ((float)(*pData)[loop]->numMicroSecsLocal / (float)totalExecTime) * 100;
		(*pData)[loop]->percentTotal = ((float)(*pData)[loop]->numMicroSecsTotal / (float)totalExecTime) * 100;
	}
	return pData;
}

size_t vmDebug::SetBreakpoint ( vmInstance *instance, stringi const &fileName, size_t lineNum, stringi const &expression, stringi const &count, stringi const &logMessage )
{
	lineNum = debugAdjustBreakpoint ( instance, fileName, lineNum );

	if ( lineNum )
	{
		if ( breakPoints.find ( vmBreakpoint ( fileName, lineNum, 0, stringi(), stringi(), stringi() ) ) == breakPoints.end () )
		{
			instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t atm )
										   {
											   auto image = instance->atomTable->atomGetLoadImage ( atm );
											   image->populateDebug ();
											   auto srcIndex = image->srcFiles.getStaticIndex ( fileName );
											   if ( srcIndex != -1 )
											   {
												   auto dIt = image->debugByLine.lower_bound ( fgxDebugInfo{ (uint32_t)srcIndex, (uint32_t)lineNum, 0, 0 } );
												   if ( dIt != image->debugByLine.end () && dIt->lineNum != 4294967295 )
												   {
													   breakPoints.insert ( vmBreakpoint ( fileName, lineNum, (*dIt).op, expression, count, logMessage ) );
													   return true;
												   }
											   }
											   return false;
										   }
			);
		}
	}
	return lineNum;
}

void vmDebug::ClearBreakpoint ( vmInstance *instance, stringi const &fileName, size_t lineNum )
{
	lineNum = debugAdjustBreakpoint ( instance, fileName, lineNum );

	if ( lineNum )
	{
		auto it = breakPoints.find ( vmBreakpoint ( fileName, lineNum, 0, stringi(), stringi(), stringi() ) );

		if ( it != breakPoints.end () )
		{
			breakPoints.erase ( it );
		}
	}
	// not found... just ignore
}

void vmDebug::ClearAllBreakpoints( vmInstance *instance, stringi const &fileName )
{
	for ( auto it = breakPoints.begin(); it != breakPoints.end(); )
	{
		if ( it->fName == fileName )
		{
			it = breakPoints.erase( it );
		} else
		{
			it++;
		}
	}
	// not found... just ignore
}

void vmDebug::MakeBreakpoints ( vmInstance *instance, fglOp *op )
{
	if ( breakPoints.size () )
	{
		instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
									   {
										   auto image = instance->atomTable->atomGetLoadImage ( index );
										   image->populateDebug ();

										   for ( auto it = breakPoints.begin (); it != breakPoints.end (); it++ )
										   {
											   if ( it->op && it->op != op )
											   {
												   switch ( it->op->op )
												   {
													   case fglOpcodes::debugBreak:
														   // don't sent a break point on an already set breakpoint!
//														   SetTracePoint( vmBPType::vmBPType_Breakpoint, it->op, &(*it) );
														   break;
													   case fglOpcodes::debugTrace:
														   // don't sent a break point on an already set breakpoint!
														   if ( !it->op->imm.bpInformation || !((dbTmpBP *)it->op->imm.bpInformation)->vmBp )
														   {
//															   SetTracePoint( vmBPType::vmBPType_Breakpoint, it->op, &(*it) );
														   }
														   break;
													   default:
														   SetTracePoint ( vmBPType::vmBPType_Breakpoint, it->op, &(*it) );
														   break;
												   }
											   }
										   }
										   return false;
									   }
		);
	}
}

vmDebug::exceptionInfo vmDebug::GetExceptionInfo ( vmInstance *instance )
{
	vmDebug::exceptionInfo res;

	if ( instance->error.valid )
	{
		res.description = scCompErrorAsText ( instance->error.errorNum );
		res.exceptionId = instance->error.errorNum;
		res.exceptionObject = nullptr;
		res.exceptionObjectType = "integer";
		res.breakMode = instance->error.breakMode;
	}
	return res;
}

errorLocation::exceptionBreakType vmDebug::GetExceptionBreakType ( errorNum err )
{
	if ( globalBreakMode == errorLocation::exceptionBreakType::always )
	{
		return errorLocation::exceptionBreakType::always;
	}

	if ( (errorNum ( err ) < errorNum::ERROR_BASE_ERROR) || (errorNum ( err ) > errorNum::ERROR_MAX_ERROR) )
	{
		return errorLocation::exceptionBreakType::always;		// windows errors can't currently be filtered.
	}
	auto type = exceptionFilters[size_t ( err ) - size_t ( errorNum::ERROR_BASE_ERROR )];

	switch ( type )
	{
		case errorLocation::exceptionBreakType::never:
			return globalBreakMode;
		case errorLocation::exceptionBreakType::always:
			return errorLocation::exceptionBreakType::always;
		case errorLocation::exceptionBreakType::unhandled:
			if ( globalBreakMode == errorLocation::exceptionBreakType::handled )
			{
				return errorLocation::exceptionBreakType::always;
			}
			return errorLocation::exceptionBreakType::unhandled;
		case errorLocation::exceptionBreakType::handled:
			if ( globalBreakMode == errorLocation::exceptionBreakType::unhandled )
			{
				return errorLocation::exceptionBreakType::always;
			}
			return errorLocation::exceptionBreakType::handled;
	}
	return errorLocation::exceptionBreakType::never;
}
void vmDebug::SetExceptionBreakType ( errorNum err, errorLocation::exceptionBreakType type )
{
	if ( (errorNum ( err ) < errorNum::ERROR_BASE_ERROR) || (errorNum ( err ) > errorNum::ERROR_MAX_ERROR) )
	{
		return;		// windows errors can't currently be filtered.
	}
	exceptionFilters[size_t ( err ) - size_t ( errorNum::ERROR_BASE_ERROR )] = type;
}

errorLocation::exceptionBreakType vmDebug::GetExceptionGlobalBreakType ()
{
	return globalBreakMode;
}
void vmDebug::SetExceptionGlobalBreakType ( errorLocation::exceptionBreakType type )
{
	globalBreakMode = type;
}


size_t vmDebug::AdjustBreakpoint ( vmInstance *instance, stringi const &fileName, size_t lineNum )
{
	uint32_t  adjustedLineNum = 0;
	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   image->populateDebug ();
									   auto srcIndex = image->srcFiles.getStaticIndex ( fileName );
									   if ( srcIndex != -1 )
									   {
										   auto dIt = image->debugByLine.lower_bound ( fgxDebugInfo{ (uint32_t)srcIndex, (uint32_t)lineNum, 0, 0 } );
										   if ( dIt != image->debugByLine.end () )
										   {
											   adjustedLineNum = dIt->lineNum;
											   return true;
										   }
									   }
									   return false;
								   }
	);
	return adjustedLineNum;
}

vmDebug::vmDebug ()
{
	tmpBP = 0;
	IP = 0;
	stack = 0;
	bpFree = 0;
	this->nCalls = 0;

	continueSemaphore = CreateSemaphore ( 0, 0, 10000000, 0 );

	RegisterInspector ( "Locals", debugLocalInspector );
	RegisterInspector ( "Globals", debugGlobalInspector );
}

vmDebug::~vmDebug ()
{
	while ( bpFree )
	{
		auto next = bpFree->next;
		delete bpFree;
		bpFree = next;
	}
	while ( tmpBP )
	{
		auto next = tmpBP->next;
		delete tmpBP;
		tmpBP = next;
	}
	if ( continueSemaphore )
	{
		CloseHandle ( continueSemaphore );
	}
}

// debug event sink for VM
fglOp *debugSignal ( vmInstance *instance, vmStateType state, fglOp *op, bcFuncDef *funcDef, VAR *stackPtr )
{
	vmDebugMessage	 msg = {};

	instance->debug->stackHeight = (uint32_t)(instance->stack - instance->eval);

	if ( !instance->debug || !instance->debug->enabled )
	{
		/* no debugger so just exit */
		//instance->debug->RemoveTracePoint();
		if ( op && (op->op == fglOpcodes::debugBreak || op->op == fglOpcodes::debugTrap) )
		{
			return op + 1;
		}
		return op;
	}

	if ( state == vmStateType::vmProfile )
	{
		instance->debug->isProfiling = true;
		return op;
	}

	msg.instance = instance;
	msg.state = state == vmStateType::vmHalt ? vmStateType::vmTrap : state;

	if ( instance->debug->terminate )
	{
		/* process is terminating... no more debug events */
		instance->debug->RemoveTracePoint ();
		return op;
	}

	instance->debug->halt = false;
	instance->debug->funcDef = funcDef;
	instance->debug->stack = stackPtr;
	instance->debug->IP = op;

	/* do any special processing each message may need */
	switch ( state )
	{
		case vmStateType::vmLoad:
			instance->debug->isProfiling = false;
			instance->debug->isResuming = false;
			instance->debug->stepInto = false;
			instance->debug->isStepping = false;
			instance->debug->halt = false;
			break;
		case vmStateType::vmDebugBreak:
			instance->debug->RemoveTracePoint ();
			break;
		case vmStateType::vmThreadStart:
		case vmStateType::vmThreadEnd:
			break;
		case vmStateType::vmHalt:
			instance->debug->RemoveTracePoint ();
			instance->debug->terminate = true;
			break;
		case vmStateType::vmHaltTrapped:
			instance->debug->RemoveTracePoint ();
			break;
		case vmStateType::vmTrap:
			if ( instance->debug->isResuming )
			{
				instance->debug->isResuming = false;
				instance->debug->halt = false;
				instance->debug->RemoveTracePoint ();
				instance->debug->MakeBreakpoints ( instance, 0 );
				return op;
			}
			instance->debug->RemoveTracePoint ();
			break;
		case vmStateType::vmTrace:
			if ( op->imm.bpInformation )
			{
				auto tmpBp = reinterpret_cast<dbTmpBP *> (op->imm.bpInformation);

				if ( tmpBp )
				{
					if ( tmpBp->type == vmBPType::vmBPType_TraceContinue )
					{
						instance->debug->isResuming = false;
						instance->debug->halt = false;
						instance->debug->RemoveTracePoint( op, tmpBp->vmBp );
						instance->debug->MakeBreakpoints( instance, 0 );
						return op;
					}

					auto vmBp = tmpBp->vmBp;

					if ( vmBp )
					{
						if ( vmBp->expression.size() )
						{
							VAR resultSave = instance->result;
							auto stackSave = instance->stack;
							auto opSave = instance->debug->IP;
							auto stateSave = instance->debug->enabled;
							auto errorSave = instance->error;
							auto funcDef = instance->debug->Func( instance, 0 );
							auto basePtr = instance->debug->BasePtr( instance, 0 );
							auto enabled = instance->debug->enabled;

							instance->debug->enabled = false;
							*instance->stack = VAR_STR( vmBp->expression.c_str(), vmBp->expression.size() );
							instance->stack++;

							try
							{
								if ( op_compile( instance, funcDef, false ) )
								{
									op_cbFixupNoPromote( instance, funcDef, basePtr );
									op_cbCall( instance, instance->debug->IP, funcDef, basePtr, 0 );
								} else
								{
									instance->result.type = slangType::eNULL;
								}
							} catch ( ... )
							{
								instance->result.type = slangType::eNULL;
							}
							bool conditionMet = instance->result.isTrue();

							// restore execution state
							instance->debug->enabled = stateSave;
							instance->debug->IP = opSave;
							instance->stack = stackSave;
							instance->result = resultSave;
							instance->error = errorSave;
							instance->debug->enabled = enabled;

							if ( !conditionMet )
							{
								instance->debug->isResuming = false;
								instance->debug->halt = false;
								instance->debug->RemoveTracePoint();
								instance->debug->StepOp( instance, funcDef, op, instance->stack, true, vmBPType::vmBPType_TraceContinue );
								return op;
							}
						}
						tmpBp->count++;
						if ( vmBp->count.size() )
						{
							VAR resultSave = instance->result;
							auto stackSave = instance->stack;
							auto opSave = instance->debug->IP;
							auto stateSave = instance->debug->enabled;
							auto errorSave = instance->error;
							auto funcDef = instance->debug->Func( instance, 0 );
							auto basePtr = instance->debug->BasePtr( instance, 0 );
							auto enabled = instance->debug->enabled;

							instance->debug->enabled = false;

							stringi expression = stringi( tmpBp->count ) + "==" + vmBp->count;
							*instance->stack = VAR_STR( expression.c_str(), expression.size() );
							instance->stack++;

							try
							{
								if ( op_compile( instance, funcDef, false ) )
								{
									op_cbFixupNoPromote( instance, funcDef, basePtr );
									op_cbCall( instance, instance->debug->IP, funcDef, basePtr, 0 );
								} else
								{
									instance->result.type = slangType::eNULL;
								}
							} catch ( ... )
							{
								instance->result.type = slangType::eNULL;
							}
							bool conditionMet = instance->result.isTrue();

							// restore execution state
							instance->debug->enabled = stateSave;
							instance->debug->IP = opSave;
							instance->stack = stackSave;
							instance->result = resultSave;
							instance->error = errorSave;
							instance->debug->enabled = enabled;

							if ( !conditionMet )
							{
								instance->debug->isResuming = false;
								instance->debug->halt = false;
								instance->debug->RemoveTracePoint();
								instance->debug->StepOp( instance, funcDef, op, instance->stack, true, vmBPType::vmBPType_TraceContinue );
								return op;
							}
						}
						if ( vmBp->logMessage.size() )
						{
							VAR resultSave = instance->result;
							auto stackSave = instance->stack;
							auto opSave = instance->debug->IP;
							auto stateSave = instance->debug->enabled;
							auto errorSave = instance->error;
							auto funcDef = instance->debug->Func( instance, 0 );
							auto basePtr = instance->debug->BasePtr( instance, 0 );
							auto enabled = instance->debug->enabled;

							instance->debug->enabled = false;

							stringi expression = stringi( "$\"" ) + vmBp->logMessage + "\" + \"\\n\"";
							*instance->stack = VAR_STR( expression.c_str(), expression.size() );
							instance->stack++;

							try
							{
								if ( op_compile( instance, funcDef, false ) )
								{
									op_cbFixupNoPromote( instance, funcDef, basePtr );
									op_cbCall( instance, instance->debug->IP, funcDef, basePtr, 0 );
								} else
								{
									instance->result.type = slangType::eNULL;
								}
							} catch ( ... )
							{
								instance->result.type = slangType::eNULL;
							}

							if ( instance->result.type == slangType::eSTRING )
							{
								if ( instance->outputDesc )
								{
									_write( instance->outputDesc, instance->result.dat.str.c, (uint32_t) instance->result.dat.str.len );
								} else
								{
									printf( "%s", instance->result.dat.str.c );
								}
							} else
							{
								if ( instance->outputDesc )
								{
									_write( instance->outputDesc, vmBp->logMessage.c_str(), (uint32_t) vmBp->logMessage.size() );
								} else
								{
									printf( "%s", vmBp->logMessage.c_str() );
								}
							}

							// restore execution state
							instance->debug->enabled = stateSave;
							instance->debug->IP = opSave;
							instance->stack = stackSave;
							instance->result = resultSave;
							instance->error = errorSave;
							instance->debug->enabled = enabled;

							// logging breakpoints never break
							instance->debug->isResuming = false;
							instance->debug->halt = false;
							instance->debug->RemoveTracePoint();
							instance->debug->StepOp( instance, funcDef, op, instance->stack, true, vmBPType::vmBPType_TraceContinue );
							return op;
						}
					}
				}
			}
			instance->debug->isResuming = true;
			instance->debug->halt = false;
			instance->debug->RemoveTracePoint();
			switch ( op->op )
			{
				case fglOpcodes::debugFuncStart:
				case fglOpcodes::debugCheckHalt:
				case fglOpcodes::debugBreak:
				case fglOpcodes::debugTrace:
				case fglOpcodes::debugTrap:
					break;
				default:
					instance->debug->StepOp( instance, funcDef, op, instance->stack, true, vmBPType::vmBPType_TraceContinue );
					break;
			}
			break;
		default:
			throw errorNum::scINTERNAL;
	}

	if ( op )
	{
		strcpy_s ( msg.location.fName, sizeof ( msg.location.fName ), funcDef->loadImage->srcFiles.getName ( instance->debug->GetCurrentSourceIndex ( funcDef, op ) ) );
		strcpy_s ( msg.location.functionName, sizeof ( msg.location.functionName ), funcDef->name );
		msg.location.funcId = (size_t)funcDef;
		msg.location.instructionAddr = (size_t)op;
		msg.location.stackAddr = (size_t)stackPtr;
		msg.location.line = instance->debug->GetCurrentLine ( funcDef, op );
		msg.location.err = (errorNum)instance->error.errorNum;
		msg.continueEvent = instance->debug->continueSemaphore;

		if ( !msg.location.line )
		{
			instance->debug->terminate = false;
			instance->debug->StepLine ( instance, funcDef, op, stackPtr );
			return (op);
		}
	} else
	{
		// this is for messages when we are not executing (load, etc.)
		auto funcDef = instance->atomTable->atomGetFunc ( "main" );
		strcpy_s ( msg.location.fName, sizeof ( msg.location.fName ), funcDef->loadImage->srcFiles.getName ( instance->debug->GetCurrentSourceIndex ( funcDef, funcDef->cs ) ) );
		strcpy_s ( msg.location.functionName, sizeof ( msg.location.functionName ), "main" );
		msg.location.funcId = (size_t)funcDef;
		msg.location.instructionAddr = 0;
		msg.location.stackAddr = 0;
		msg.location.line = 1;
		msg.location.err = errorNum::ERROR_OK;
		msg.continueEvent = instance->debug->continueSemaphore;
	}

	instance->debug->halted = true;
	instance->debug->isStepping = false;
	instance->debug->stepInto = false;

	instance->debug->Interrupt ( &msg );		// send message to client

	if ( instance->debug->terminate )
	{
		// otherwise debugSignal will end prematurely
		instance->debug->terminate = 0;

		debugSignal ( instance, vmStateType::vmThreadEnd, op, funcDef, stackPtr );

		/* call taskEnd to make sure we do all the proper cleanup */
		throw errorNum::scUSER_TERMINATE;
	}

	instance->debug->MakeBreakpoints( instance, op );

	instance->debug->halted = 0;

	return instance->debug->IP;
}


void debugRegister ( vmInstance *instance, std::shared_ptr<vmDebug> &debug )
{
	assert ( !instance->debug );
	instance->debug = debug;
}

void debugContinue ( vmDebugMessage *msg )
{
	msg->instance->debug->isResuming = true;
	msg->instance->debug->Continue ( msg );
}

void debugStepInto ( vmInstance *instance )
{
	instance->debug->isStepping = true;
	instance->debug->stepInto = true;
	instance->debug->StepLine ( instance, instance->debug->funcDef, instance->debug->IP, instance->debug->stack );
}

void debugStepOver ( vmInstance *instance )
{
	instance->debug->isStepping = true;
	instance->debug->stepInto = false;
	instance->debug->StepLine ( instance, instance->debug->funcDef, instance->debug->IP, instance->debug->stack );
}

void debugTerminate ( vmInstance *instance )
{
	instance->debug->terminate = true;
}

void debugHalt ( vmInstance *instance )
{
	instance->debug->halt = true;
}

void debugRunToLine ( vmInstance *instance, char const *fileName, size_t line )
{
	instance->debug->isStepping = false;
	instance->debug->stepInto = false;
	instance->debug->RunToLine ( instance, fileName, line );
}

void debugStepOut ( vmInstance *instance )
{
	instance->debug->isStepping = false;
	instance->debug->stepInto = false;
	instance->debug->StepOut ( instance, instance->debug->funcDef, instance->debug->IP, instance->debug->stack );
}

bool debugSetIP ( vmInstance *instance, char const *fileName, size_t line )
{
	instance->debug->isResuming = false;
	instance->debug->halt = false;
	instance->debug->isStepping = false;
	instance->debug->stepInto = false;
	instance->debug->terminate = false;
	instance->error.valid = false;
	return instance->debug->SetIP ( instance, instance->debug->funcDef, fileName, line );
}

vmCallStack *debugGetCallstack ( vmInstance *instance )
{
	return instance->debug->GetCallstack ( instance );
}

vmFunctionList *debugGetFunctionList ( vmInstance *instance )
{
	return instance->debug->GetFunctionList ( instance );
}

vmInspectorList *debugGetInspectors ( vmInstance *instance )
{
	return instance->debug->GetInspectors ( instance );
}

vmInspectList *debugInspect ( vmInstance *instance, size_t num, size_t stackFrame )
{
	return instance->debug->Inspect ( instance, num, stackFrame );
}

vmInspectList *debugInspect ( vmInstance *instance, size_t stackFrame, vmDebugVar *var, uint32_t start, uint32_t end )
{
	return instance->debug->Inspect ( instance, stackFrame, var, start, end );
}

size_t debugSetBreakpoint ( vmInstance *instance, stringi const &fileName, size_t addr, stringi const &expr, stringi const &count, stringi const &logMessage )
{
	return instance->debug->SetBreakpoint ( instance, fileName, addr, expr, count, logMessage );
}

void debugClearBreakpoint ( vmInstance *instance, stringi const &fileName, size_t addr )
{
	instance->debug->ClearBreakpoint ( instance, fileName, addr );
}

void debugClearAllBreakpoints ( vmInstance *instance, stringi const &fileName )
{
	instance->debug->ClearAllBreakpoints ( instance, fileName );
}

bool debugIsProfiling ( vmInstance *instance )
{
	return instance->debug->isProfiling;
}

vmProfileData *debugGetExecutionProfile ( vmInstance *instance )
{
	return instance->debug->GetExecutionProfile ( instance );
}

size_t debugAdjustBreakpoint ( vmInstance *instance, stringi const &fName, size_t lineNum )
{
	return instance->debug->AdjustBreakpoint ( instance, fName, lineNum );
}

vmDebug::exceptionInfo debugExceptionInfo ( vmInstance *instance )
{
	return instance->debug->GetExceptionInfo ( instance );
}

errorLocation::exceptionBreakType debugGetExceptionBreakType ( vmInstance *instance, errorNum errNum )
{
	return instance->debug->GetExceptionBreakType ( errNum );
}

errorLocation::exceptionBreakType debugGetExceptionGlobalBreakType ( vmInstance *instance )
{
	return instance->debug->GetExceptionGlobalBreakType (  );
}

void debugSetExceptionBreakType ( vmInstance *instance, errorNum errNum, errorLocation::exceptionBreakType breakType )
{
	return instance->debug->SetExceptionBreakType ( errNum, breakType );
}

void debugSetExceptionGlobalBreakType ( vmInstance *instance, errorLocation::exceptionBreakType breakType )
{
	return instance->debug->SetExceptionGlobalBreakType ( breakType );
}
vmDebug::exceptionInfo debugGetExceptionInfo ( vmInstance *instance )
{
	return instance->debug->GetExceptionInfo ( instance );
}

