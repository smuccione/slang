// codeBlock operations

#include "stdlib.h"

#include "bcInterpreter.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "compilerParser/fileParser.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmAtom.h"
#include "Target/vmTask.h"
#include "bcVM/bcVM.h"
#include "op_variant.h"

vmCodeBlock  *_compileIndexCodeblock ( class vmInstance *instance, char const *expr )
{
	vmCodeBlock			*cb;
	astNode				*cbNode;
	astCodeblock		 exp;
	opFile				 file;
	vmTaskInstance		*task = static_cast<vmTaskInstance *>(instance);
	source				 src ( &file.srcFiles, file.sCache, "(INTERNAL)", expr );

	cbNode = file.parseExpr ( src, true, false, 0, false, false );

	for ( auto &it : task->preLoadedSource )
	{
		auto obj = (char const *) std::get<1> ( it ).get ();

		file.addStream ( &obj, false, false );
	}

	try
	{
		symbolStack			 sym ( &file );
		compExecutable		 comp ( &file );
		compFixup			 fixup;
		astNode				*retNode;

		class opUsingList		useList;
		symbolSpaceNamespace	ns ( &file, &file.ns, &useList );

		sym.push ( &ns );

		cbNode->makeCodeblock ( &sym, &file.errHandler );

		comp.fixup = &fixup;

		retNode = new astNode ( file.sCache, astOp::btReturn );
		retNode->returnData().node = cbNode;

		astNode *basic = new astNode ( file.sCache, astOp::btBasic );
		basic->pushNode ( retNode );

		astNode *codeBlock = new astNode ( file.sCache, astOp::btBasic );
		codeBlock->pushNode ( basic);
		codeBlock->pushNode ( new astNode ( file.sCache, astOp::btLabel, "$ret" ) );
		codeBlock->pushNode ( new astNode ( file.sCache, astOp::btExit ) );

		errorLocality e ( &file.errHandler, srcLocation() );

		symbolSpaceLocalAdd localAdd;
		sym.push ( &localAdd );

		codeBlock->makeSymbolsCodeBlock ( &file.errHandler, &sym, 0, 0, 0, nullptr, &localAdd, 0, false );
		codeBlock->makeAtomsCodeBlock ( &sym, &file.errHandler );
		codeBlock->forceVariants ( &sym );

		comp.fixupGoto ( codeBlock );

		// convert any locals into fields
		for ( auto &it : codeBlock->basicData().blocks )
		{
			if ( it->getOp() == astOp::symbolDef )
			{
				if ( it->symbolDef()->symType == symbolSpaceType::symTypeLocal )
				{
					symbolField *field = new symbolField ( it->symbolDef()->getSymbolName ( ), "index field" );
					delete it->symbolDef();
					it->symbolDef() = field;
				}
			}
		}

		comp.compEmitNode ( 0, codeBlock, &sym, symVariantType, symVariantType, false, true );

		for ( auto &it : comp.atom.fixup )
		{
			comp.codeSegment.setdword ( (uint32_t) it->offset, instance->atomTable->atomFind ( comp.atom.atoms[it->atom]->name ) );
		}

		comp.fixup->applyFixups ( comp.codeSegment );

		bufferMakeRoom ( exp.buff, sizeof ( vmCodeBlock ) );
		bufferAssume ( exp.buff, sizeof ( vmCodeBlock ) );

		cb = (vmCodeBlock *) bufferBuff ( exp.buff );
		cb->nParams = 0;

		cb = (vmCodeBlock *) bufferBuff ( exp.buff );
		cb->csOffset = (uint32_t) bufferSize ( exp.buff );
		comp.codeSegment.serialize ( exp.buff );

		cb = (vmCodeBlock *) bufferBuff ( exp.buff );
		cb->dsOffset = (uint32_t) bufferSize ( exp.buff );
		comp.dataSegment.serialize ( exp.buff );

		cb = (vmCodeBlock *) bufferBuff ( exp.buff );
		cb->bssSize = comp.bssSize;

		cb = (vmCodeBlock *) bufferBuff ( exp.buff );
		cb->symOffset = (uint32_t) bufferSize ( exp.buff );
		cb->nSymbols = (uint32_t) comp.symbols.size ( );

		uint32_t symbolNameOffset = (uint32_t) (bufferSize ( exp.buff ) + sizeof ( fgxSymbols ) * comp.symbols.size ( ));
		for ( auto it = comp.symbols.begin ( ); it != comp.symbols.end ( ); it++ )
		{
			fgxSymbols	fgxSym = {};

			fgxSym.offsetName = symbolNameOffset;
			symbolNameOffset += (uint32_t) ((*it).name.size ( ) + 1);

			fgxSym.name = 0;
			fgxSym.validStart = (*it).validStart;
			fgxSym.validEnd = (*it).validEnd;
			fgxSym.index = (*it).index;
			fgxSym.noRef = (*it).noRef;
			fgxSym.isStackBased = (*it).isStackBased;
			fgxSym.isParameter = false;

			bufferWrite ( exp.buff, &fgxSym, sizeof ( fgxSym ) );
		}
		for ( auto it = comp.symbols.begin ( ); it != comp.symbols.end ( ); it++ )
		{
			bufferWrite ( exp.buff, (*it).name.c_str ( ), (*it).name.size ( ) + 1 );
		}

		cb = (vmCodeBlock *) bufferBuff ( exp.buff );
		cb->storageSize = (uint32_t) bufferSize ( exp.buff );
		cb->isStatic = false;

		delete codeBlock;
	} catch ( ... )
	{
		delete cbNode;
		throw;
	}

	cb = (vmCodeBlock *) malloc ( bufferSize ( exp.buff ) );
	memcpy ( cb, bufferBuff ( exp.buff ), bufferSize ( exp.buff ) );

	return cb;
}

void op_cbFixupNoPromote ( class vmInstance *instance, bcFuncDef *funcDef, VAR *basePtr )
{
	VAR			*leftOperand;
	vmCodeBlock	*cb;
	uint32_t	 cbSymIndex;
	uint32_t	 funcSymIndex;
	uint32_t	 funcSymbols;
	
	leftOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	if ( TYPE ( leftOperand ) != slangType::eCODEBLOCK_ROOT )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}
	leftOperand = leftOperand->dat.ref.v;

	GRIP g1 ( instance );

	if ( funcDef )
	{
		cb = (vmCodeBlock *)leftOperand->dat.cb.cb;

		uint32_t nDetach = cb->nSymbols - cb->nParams;

		funcSymbols = funcDef->nSymbols;

		// need to externalize... this needs to be done both here and when we instantiate a pre-compiled code block
		// need to do fixup here

		fgxSymbols *fs;

		fs = (fgxSymbols *)((char *)cb + cb->symOffset);
		for ( cbSymIndex = 0; cbSymIndex < cb->nSymbols; cbSymIndex++ )
		{
			fs[cbSymIndex].name = ((char *)cb + fs[cbSymIndex].offsetName);
		}

		for ( cbSymIndex = 0; cbSymIndex < nDetach; cbSymIndex++ )
		{
			for ( funcSymIndex = 0; funcSymIndex < funcSymbols; funcSymIndex++ )
			{
				if ( !strccmp( fs[cbSymIndex + cb->nParams].name, funcDef->symbols[funcSymIndex].name ) )
				{
					leftOperand[cbSymIndex + 1] = basePtr[funcDef->symbols[funcSymIndex].index];
					break;
				}
			}
		}
	} else
	{
		cb = (vmCodeBlock *)leftOperand->dat.cb.cb;
		uint32_t nDetach = cb->nSymbols - cb->nParams;

		for ( cbSymIndex = 0; cbSymIndex < nDetach; cbSymIndex++ )
		{
			leftOperand[cbSymIndex + nDetach + 1].type = slangType::eNULL;
		}
	}
}

void op_cbFixup ( class vmInstance *instance, bcFuncDef *funcDef, VAR *basePtr )
{
	VAR *leftOperand;
	vmCodeBlock *cb;
	uint32_t	 cbSymIndex;
	uint32_t	 funcSymIndex;
	uint32_t	 funcSymbols;

	leftOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	if ( TYPE ( leftOperand ) != slangType::eCODEBLOCK_ROOT )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}
	leftOperand = leftOperand->dat.ref.v;

	GRIP g1 ( instance );

	if ( funcDef )
	{
		cb = (vmCodeBlock *)leftOperand->dat.cb.cb;

		uint32_t nDetach = cb->nSymbols - cb->nParams;

		funcSymbols = funcDef->nSymbols;

		// need to externalize... this needs to be done both here and when we instantiate a pre-compiled code block
		// need to do fixup here

		fgxSymbols *fs;

		fs = (fgxSymbols *)((char *)cb + cb->symOffset);
		for ( cbSymIndex = 0; cbSymIndex < cb->nSymbols; cbSymIndex++ )
		{
			fs[cbSymIndex].name = ((char *)cb + fs[cbSymIndex].offsetName);
		}

		for ( cbSymIndex = 0; cbSymIndex < nDetach; cbSymIndex++ )
		{
			for ( funcSymIndex = 0; funcSymIndex < funcSymbols; funcSymIndex++ )
			{
				if ( !strccmp ( fs[cbSymIndex + cb->nParams].name, funcDef->symbols[funcSymIndex].name ) )
				{
					if ( !funcDef->symbols[funcSymIndex].noRef && TYPE ( &basePtr[funcDef->symbols[funcSymIndex].index] ) != slangType::eREFERENCE )
					{
						// local isn't a reference, so we need to make it one now
						leftOperand[cbSymIndex + nDetach + 1] = basePtr[funcDef->symbols[funcSymIndex].index];
						basePtr[funcDef->symbols[funcSymIndex].index] = VAR_REF ( leftOperand, &leftOperand[cbSymIndex + nDetach + 1] );
					}
					leftOperand[cbSymIndex + 1] = basePtr[funcDef->symbols[funcSymIndex].index];
					break;
				}
			}
		}
	} else
	{
		cb = (vmCodeBlock *)leftOperand->dat.cb.cb;
		uint32_t nDetach = cb->nSymbols - cb->nParams;

		for ( cbSymIndex = 0; cbSymIndex < nDetach; cbSymIndex++ )
		{
			leftOperand[cbSymIndex + nDetach + 1].type = slangType::eNULL;
		}
	}
}

void  op_cbCall ( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, VAR *basePtr, uint32_t nParams )
{
	VAR							*leftOperand;
	vmCodeBlock					*cb;
	bcFuncDef					 cbFunc;
	bcLoadImage					 cbLoadImage ( instance, "cb" );
	static fgxTryCatchEntry		 nullTcList[] = { {0xFFFFFFFF, 0xFFFFFFFF, 0} };
	
	leftOperand = instance->stack - 1 - nParams;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	if ( TYPE ( leftOperand ) != slangType::eCODEBLOCK_ROOT )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}
	leftOperand = leftOperand->dat.ref.v;

	cb = (vmCodeBlock *)leftOperand->dat.cb.cb;

	if ( nParams < cb->nParams )
	{
		throw errorNum::scPARAMETER_COUNT_MISMATCH;
	}

	// copy the detached symbols over;
	memcpy( instance->stack, &leftOperand[1], sizeof ( VAR ) * (cb->nSymbols - cb->nParams) );
	instance->stack += cb->nSymbols - cb->nParams;

#if _DEBUG 
	memset ( instance->stack, 0, sizeof ( *instance->stack ) * 8 );
	instance->stack->type = slangType::eDEBUG_MARKER;
#endif

	GRIP g1 ( instance );
	cbFunc.name					 = "(codeblock)";
	cbFunc.symbols				 = (fgxSymbols *)((uint8_t *)cb + cb->symOffset);
	cbFunc.conv					 = fgxFuncCallConvention::opFuncType_Bytecode;
	cbFunc.retType				 = bcFuncPType::bcFuncPType_Variant;
	cbFunc.nParams				 = cb->nSymbols;
	cbFunc.nSymbols				 = cb->nSymbols;
	cbFunc.tcList				 = nullTcList;
	cbFunc.loadImage			 = &cbLoadImage;
	cbFunc.cs					 = (fglOp *)((uint8_t *)cb + cb->csOffset) + 1;
	cbLoadImage.csBase			 = (fglOp *)((uint8_t *)cb + cb->csOffset);
	cbLoadImage.dsBase			 = (uint8_t *)((uint8_t *)cb + cb->dsOffset);
	if ( cb->bssSize )
	{
		cbLoadImage.bssBase			 = (uint8_t *)instance->calloc ( cb->bssSize, sizeof ( uint8_t ) );
	} else
	{
		cb->bssSize = 0;
	}

	try
	{
		instance->interpretBC ( &cbFunc, ops + 1, nParams );
	} catch ( ... )
	{
		throw;
	}
	instance->stack -= cb->nSymbols - cb->nParams;
}

bool op_compile ( class vmInstance *instance, bcFuncDef *funcDef, bool allowSideEffects )
{
	VAR					*leftOperand;
	vmCodeBlock			*cb;
	astNode				*cbNode;
	char const			*expr;
	astCodeblock		 exp;
	opFile				 file;
	GRIP				 g1 ( instance );

	leftOperand = instance->stack - 1;

	while ( TYPE ( leftOperand ) == slangType::eREFERENCE )
	{
		leftOperand = leftOperand->dat.ref.v;
	}

	if ( TYPE ( leftOperand ) != slangType::eSTRING )
	{
		throw errorNum::scILLEGAL_OPERAND;
	}

	expr = leftOperand->dat.str.c;

	source src ( &file.srcFiles, file.sCache, "(INTERNAL)", expr );
	cbNode = file.parseExpr ( src, true, false, 0, true, false );

	try {
		opFunction			 oFunc ( &file);
		symbolSpaceLocalAdd  localAdd;
		symbolStack			 sym ( &file, &oFunc, &localAdd );
		compExecutable		 comp ( &file );
		astNode				*retNode;

		if ( !allowSideEffects && !cbNode->hasNoSideEffectsDebugAdapter ( &sym, true ) )
		{
			return false;
		}

		cbNode->makeCodeblock ( &sym, &file.errHandler );

		comp.listing.level = compLister::listLevel::VERBOSE;

		comp.fixup = new compFixup;

		retNode = new astNode ( file.sCache, astOp::btReturn );
		retNode->returnData().node	= cbNode;

		astNode *codeBlock = new astNode ( file.sCache, astOp::btBasic );
		codeBlock->pushNode ( retNode );
		codeBlock->pushNode ( new astNode ( file.sCache, astOp::btLabel, "$ret" ) );
		codeBlock->pushNode ( new astNode ( file.sCache, astOp::btExit ) );

		errorLocality e ( &file.errHandler, srcLocation() );

		codeBlock->makeSymbolsCodeBlock ( &file.errHandler, &sym, 0, 0, 0, nullptr, &localAdd, 0, false );
		codeBlock->makeAtomsCodeBlock ( &sym, &file.errHandler );
		codeBlock->forceVariants( &sym );

		comp.fixupGoto ( codeBlock );

		for ( auto it = codeBlock->basicData().blocks.begin ( ); it != codeBlock->basicData().blocks.end ( ); it++ )
		{
			if ( (*it)->getOp() == astOp::symbolDef )
			{
				visibleSymbols	fgxSym;

				if ( (*it)->symbolDef()->getFgxSymbol ( fgxSym ) )
				{
					fgxSym.index = sym.getIndex ( fgxSym.name, true );
					fgxSym.validStart = 0;
					fgxSym.validEnd = 0;
					comp.symbols.push_back ( fgxSym );
				}
				(*it)->release ( );
			}
		}

		comp.compEmitNode ( 0, codeBlock, &sym, symVariantType, symVariantType, !allowSideEffects, true );

		for ( auto &it : comp.atom.fixup )
		{
			comp.codeSegment.setdword ( (uint32_t)it->offset, instance->atomTable->atomFind ( comp.atom.atoms[it->atom]->name ) );
		}

		comp.fixup->applyFixups ( comp.codeSegment );
		delete comp.fixup;
		comp.fixup = 0;

		bufferMakeRoom ( exp.buff, sizeof ( vmCodeBlock ) );
		bufferAssume ( exp.buff, sizeof ( vmCodeBlock ) );

		cb = (vmCodeBlock *)bufferBuff ( exp.buff );
		cb->nParams	 = 0;

		cb = (vmCodeBlock *)bufferBuff ( exp.buff );
		cb->csOffset = (uint32_t) bufferSize ( exp.buff );
		comp.codeSegment.serialize ( exp.buff );

		cb = (vmCodeBlock *)bufferBuff ( exp.buff );
		cb->dsOffset = (uint32_t) bufferSize ( exp.buff );
		comp.dataSegment.serialize ( exp.buff );

		cb = (vmCodeBlock *)bufferBuff ( exp.buff );
		cb->bssSize	 = comp.bssSize;

		cb = (vmCodeBlock *)bufferBuff ( exp.buff );
		cb->symOffset = (uint32_t) bufferSize ( exp.buff );
		cb->nSymbols	= (uint32_t)comp.symbols.size();

		// symbols are filled in during processing of a symbolDef operator
		uint32_t symbolNameOffset	= (uint32_t)(bufferSize ( exp.buff ) + sizeof ( fgxSymbols ) * comp.symbols.size());
		for ( auto it = comp.symbols.begin(); it != comp.symbols.end(); it++ )
		{
			fgxSymbols	fgxSym = {};

			fgxSym.offsetName		 =  symbolNameOffset;
			symbolNameOffset		+=  (uint32_t)((*it).name.size() + 1);

			fgxSym.name = 0;
			fgxSym.validStart		= (*it).validStart;
			fgxSym.validEnd			= (*it).validEnd;
			fgxSym.index			= (*it).index;
			fgxSym.noRef			= (*it).noRef;
			fgxSym.isStackBased		= (*it).isStackBased;
			fgxSym.isParameter		= false;

			bufferWrite ( exp.buff, &fgxSym, sizeof ( fgxSym ) );
		}
		for ( auto it = comp.symbols.begin(); it != comp.symbols.end(); it++ )
		{
			bufferWrite ( exp.buff, (*it).name.c_str ( ), (*it).name.size ( ) + 1 );
		}

		cb = (vmCodeBlock *)instance->om->alloc ( bufferSize ( exp.buff ) );
		memcpy ( cb, bufferBuff ( exp.buff ), bufferSize ( exp.buff ) );

		cb->storageSize = (uint32_t) bufferSize ( exp.buff );
		cb->isStatic = false;

		delete codeBlock;
	} catch ( ... )
	{
		delete cbNode;
		throw;
	}

	(instance->stack - 1)->type = slangType::eCODEBLOCK_ROOT;
	(instance->stack - 1)->dat.ref.v = instance->om->allocVar ( cb->nSymbols - cb->nParams + 1 );
	memset ( &(instance->stack - 1)->dat.ref.v[1], 0, sizeof ( VAR ) * (cb->nSymbols - cb->nParams) );
	(instance->stack - 1)->dat.ref.obj = (instance->stack - 1)->dat.ref.v;
	(instance->stack - 1)->dat.ref.v->type = slangType::eCODEBLOCK;
	(instance->stack - 1)->dat.ref.v->dat.cb.cb = cb;

	return true;
}

