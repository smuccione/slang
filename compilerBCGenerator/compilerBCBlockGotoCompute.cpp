
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "Utility/counter.h"

// note: these do NOT include compiler generated labels for backpatching loops and switch's... these are purely for user defined label/goto's

using blockNestDef = std::vector<std::pair<astNode *, bool>>;		// the basic block and if it has allocations
using labelsDef = std::unordered_map<stringi, std::pair<blockNestDef, size_t>>;		// the label location -> the basic blocks enclosing the label and the needed stack size at that label

static void makeLabelList ( opFile *file, errorHandler *err, astNode *block, astNode *basic, size_t &stackSize, blockNestDef &blockNest, labelsDef &labels, cacheString const &breakLabel, cacheString const &continueLabel )
{
	if ( !block )
	{
		return;
	}

	errorLocality e ( err, block );

	switch ( block->getOp () )
	{
		case astOp::symbolDef:
			switch ( block->symbolDef ()->symType )
			{
				case symbolSpaceType::symTypeStatic:
					if ( block->symbolDef ()->getInitializer ( block->symbolDef ()->getSymbolName () ) )
					{
						makeLabelList ( file, err, block->symbolDef ()->getInitializer ( block->symbolDef ()->getSymbolName () ), block, stackSize, blockNest, labels, cacheString (), cacheString () );
					}
					break;
				case symbolSpaceType::symTypeLocal:
					if ( block->symbolDef ()->getInitializer ( block->symbolDef ()->getSymbolName () ) )
					{
						makeLabelList ( file, err, block->symbolDef ()->getInitializer ( block->symbolDef ()->getSymbolName () ), block, stackSize, blockNest, labels, cacheString (), cacheString () );
					}
					stackSize++;
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef ());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							makeLabelList ( file, err, (*symbol)[it]->initializer, block, stackSize, blockNest, labels, cacheString (), cacheString () );
							stackSize++;
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto stackSave = stackSize;
				blockNest.push_back ( { block, false } );
				for ( auto it = block->basicData ().blocks.begin (); it != block->basicData ().blocks.end (); it++ )
				{
					makeLabelList ( file, err, (*it), block, stackSize, blockNest, labels, breakLabel, continueLabel );
					if ( stackSize != stackSave )
					{
						std::get<1> ( blockNest.back () ) = true;		// has allocations
					}
				}
				stackSize = stackSave;
				blockNest.pop_back ();
			}
			break;
		case astOp::btInline:
			{
				errorLocality e ( err, block, true );
				size_t inlineStack = 0;
				makeLabelList ( file, err, block->inlineFunc ().node, basic, inlineStack, blockNest, labels, cacheString (), cacheString () );
			}
			break;
		case astOp::btLabel:
			labels[block->label ().operator const stringi & ()] = { blockNest, stackSize };
			break;
		case astOp::btBreak:
			if ( !breakLabel )
			{
				throw errorNum::scNO_BREAK_POINT;
			}
			block->gotoData ().label = breakLabel;
			break;
		case astOp::btCont:
			if ( !continueLabel )
			{
				throw errorNum::scNO_CONTINUE_POINT;
			}
			block->gotoData ().label = continueLabel;
			break;
		case astOp::btGoto:
			break;
		case astOp::btReturn:
			if ( !block->returnData ().label )
			{
				block->returnData ().label = file->retValue;
			}
			labels[block->returnData ().label.operator const stringi &()] = { blockNest, stackSize };
			makeLabelList ( file, err, block->returnData ().node, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
			break;
		case astOp::btInlineRet:
			makeLabelList ( file, err, block->returnData ().node, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
			break;
		case astOp::btTry:
			makeLabelList ( file, err, block->tryCatchData ().tryBlock, basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			makeLabelList ( file, err, block->tryCatchData ().finallyBlock, basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			makeLabelList ( file, err, block->tryCatchData ().catchBlock, basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			break;
		case astOp::btFor:
			block->loopData ().breakLabel = file->sCache.get ( MakeUniqueLabel ( block->loopData ().breakLabel ) );
			block->loopData ().continueLabel = file->sCache.get ( MakeUniqueLabel ( block->loopData ().continueLabel ) );

			labels[block->loopData ().breakLabel.operator const stringi & ()] = { blockNest, stackSize };
			labels[block->loopData ().continueLabel.operator const stringi & ()] = { blockNest, stackSize };

			makeLabelList ( file, err, block->loopData ().condition, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
			makeLabelList ( file, err, block->loopData ().increase, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
			makeLabelList ( file, err, block->loopData ().block, basic, stackSize, blockNest, labels, block->loopData ().breakLabel, block->loopData ().continueLabel );
			makeLabelList ( file, err, block->loopData ().initializers, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
			break;
		case astOp::btForEach:
			makeLabelList ( file, err, block->forEachData ().var, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
			makeLabelList ( file, err, block->forEachData ().in, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
			makeLabelList ( file, err, block->forEachData ().statement, basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			break;
		case astOp::btIf:
			makeLabelList ( file, err, block->ifData ().elseBlock, basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			for ( auto it = block->ifData ().ifList.begin (); it != block->ifData ().ifList.end (); it++ )
			{
				makeLabelList ( file, err, (*it)->condition, basic, stackSize, blockNest, labels, cacheString (), cacheString () );
				makeLabelList ( file, err, (*it)->block, basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			}
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			block->loopData ().breakLabel = file->sCache.get ( MakeUniqueLabel ( block->loopData ().breakLabel ) );
			block->loopData ().continueLabel = file->sCache.get ( MakeUniqueLabel ( block->loopData ().continueLabel ) );

			labels[block->loopData ().breakLabel.operator const stringi & ()] = { blockNest, stackSize };
			labels[block->loopData ().continueLabel.operator const stringi & ()] = { blockNest, stackSize };

			makeLabelList ( file, err, block->loopData().condition, basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			makeLabelList ( file, err, block->loopData().block, basic, stackSize, blockNest, labels, block->loopData().breakLabel, block->loopData().continueLabel );
			break;
		case astOp::btSwitch:
			block->switchData().breakLabel = file->sCache.get ( MakeUniqueLabel ( block->switchData().breakLabel ) );

			labels[block->switchData ().breakLabel.operator const stringi & ()] = { blockNest, stackSize };

			makeLabelList ( file, err, block->switchData().condition, basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			for ( auto it = block->switchData().caseList.begin(); it != block->switchData().caseList.end(); it++ )
			{
				makeLabelList ( file, err, (*it)->block, basic, stackSize, blockNest, labels, block->switchData().breakLabel, continueLabel );
				makeLabelList ( file, err, (*it)->condition, basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			}
			break;
		case astOp::conditional:
			makeLabelList ( file, err, block->conditionData().trueCond, basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			makeLabelList ( file, err, block->conditionData().falseCond, basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = block->arrayData().nodes.begin(); it != block->arrayData().nodes.end(); it++ )
			{
				makeLabelList ( file, err, (*it), basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			}
			break;
		case astOp::arrayDeref:
		case astOp::funcCall:
		case astOp::cArrayDeref:
			makeLabelList ( file, err, block->left, basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			for ( auto it = block->pList().param.begin(); it != block->pList().param.end(); it++ )
			{
				makeLabelList ( file, err, (*it), basic, stackSize, blockNest, labels, cacheString(), cacheString() );
			}
			break;
		default:
			if ( block->left ) makeLabelList ( file, err, block->left, basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			if ( block->right ) makeLabelList ( file, err, block->right , basic, stackSize, blockNest, labels, breakLabel, continueLabel );
			break;
	}
}

static uint32_t computeStackPop ( stringi const &label, size_t stackSize, blockNestDef &blockNest, labelsDef &labels )
{
	auto const &dest = std::get<0>( labels.find ( label )->second );
	auto const &destAlloc = std::get<1> ( labels.find ( label )->second );

	uint32_t loop = 0;
	while ( (loop < dest.size ( )) || (loop < blockNest.size ( )) )
	{
		if ( (loop < dest.size ( )) && (loop < blockNest.size ( )) )
		{
			if ( blockNest[loop].first != std::get<0>(dest[loop]) && std::get<1>(dest[loop]) )
			{
				throw errorNum::scILLEGAL_GOTO;
			}
		} else if ( loop < dest.size ( ) )
		{
			if ( std::get<1>(dest[loop]) )
			{
				throw errorNum::scILLEGAL_GOTO;
			}
		}
		loop++;
	}

	return (uint32_t) (stackSize - destAlloc);
}

static void computeGotoBlockNest ( opFile *file, opFunction *func, astNode *block, astNode *basic, size_t &stackSize, blockNestDef &blockNest, labelsDef &labels )
{
	if ( !block )
	{
		return;
	}

	errorLocality e1 ( &file->errHandler, block );

	switch ( block->getOp() )
	{
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
					if ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ) )
					{
						computeGotoBlockNest ( file, func, block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ), block, stackSize, blockNest, labels );
					}
					break;
				case symbolSpaceType::symTypeLocal:
					if ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ) )
					{
						computeGotoBlockNest ( file, func, block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ), block, stackSize, blockNest, labels );
					}
					stackSize++;
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							computeGotoBlockNest ( file, func, (*symbol)[it]->initializer, block, stackSize, blockNest, labels );
							stackSize++;
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto stackSave = stackSize;
				blockNest.push_back ( { block, false } );
				for ( auto it = block->basicData().blocks.begin ( ); it != block->basicData().blocks.end ( ); it++ )
				{
					computeGotoBlockNest ( file, func, (*it), block, stackSize, blockNest, labels );
					if ( stackSize != stackSave )
					{
						std::get<1> ( blockNest.back () ) = true;		// has allocations
					}
				}
				stackSize = stackSave;
				blockNest.pop_back ( );
			}
			break;
		case astOp::btInline:
			{
				errorLocality e ( &file->errHandler, block, true );
				size_t inlineStack = 0;
				computeGotoBlockNest ( file, func, block->inlineFunc().node, basic, inlineStack, blockNest, labels );
			}
			break;
		case astOp::btLabel:
			break;
			// we broke these apart to make it possible to break during debugging on individual lines... can be recombined
		case astOp::btBreak:
		case astOp::btCont:
		case astOp::btGoto:
			block->gotoData().nPop = computeStackPop ( block->gotoData().label, stackSize, blockNest, labels );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			block->returnData().nPop = computeStackPop ( block->returnData().label, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->returnData().node, basic, stackSize, blockNest, labels );
			break;
		case astOp::btTry:
			computeGotoBlockNest ( file, func, block->tryCatchData().tryBlock, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->tryCatchData().finallyBlock, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->tryCatchData().catchBlock, basic, stackSize, blockNest, labels );
			break;
		case astOp::btFor:
			computeGotoBlockNest ( file, func, block->loopData().condition, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->loopData().increase, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->loopData().block, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->loopData().initializers, basic, stackSize, blockNest, labels );
			break;
		case astOp::btForEach:
			computeGotoBlockNest ( file, func, block->forEachData().var, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->forEachData().in, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->forEachData().statement, basic, stackSize, blockNest, labels );
			break;
		case astOp::btIf:
			computeGotoBlockNest ( file, func, block->ifData().elseBlock, basic, stackSize, blockNest, labels );
			for ( auto it = block->ifData().ifList.begin(); it != block->ifData().ifList.end(); it++ )
			{
				computeGotoBlockNest ( file, func, (*it)->condition, basic, stackSize, blockNest, labels );
				computeGotoBlockNest ( file, func, (*it)->block, basic, stackSize, blockNest, labels );
			}
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			computeGotoBlockNest ( file, func, block->loopData().condition, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->loopData().block, basic, stackSize, blockNest, labels );
			break;
		case astOp::btSwitch:
			computeGotoBlockNest ( file, func, block->switchData().condition, basic, stackSize, blockNest, labels );
			for ( auto it = block->switchData().caseList.begin(); it != block->switchData().caseList.end(); it++ )
			{
				computeGotoBlockNest ( file, func, (*it)->block, basic, stackSize, blockNest, labels );
				computeGotoBlockNest ( file, func, (*it)->condition, basic, stackSize, blockNest, labels );
			}
			break;
		case astOp::conditional:
			computeGotoBlockNest ( file, func, block->conditionData().trueCond, basic, stackSize, blockNest, labels );
			computeGotoBlockNest ( file, func, block->conditionData().falseCond, basic, stackSize, blockNest, labels );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = block->arrayData().nodes.begin(); it != block->arrayData().nodes.end(); it++ )
			{
				computeGotoBlockNest ( file, func, (*it), basic, stackSize, blockNest, labels );
			}
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			computeGotoBlockNest ( file, func, block->left, basic, stackSize, blockNest, labels );
			for ( auto it = block->pList().param.begin(); it != block->pList().param.end(); it++ )
			{
				computeGotoBlockNest ( file, func, (*it), basic, stackSize, blockNest, labels );
			}
			break;
		default:
			if ( block->left ) computeGotoBlockNest ( file, func, block->left, basic, stackSize, blockNest, labels );
			if ( block->right ) computeGotoBlockNest ( file, func, block->right , basic, stackSize, blockNest, labels );
			break;

	}
}

void compExecutable::fixupGoto ( astNode *node )
{
	blockNestDef blockNest;
	labelsDef labels;

	size_t stackSize = 0;
	makeLabelList ( file, &file->errHandler, node, 0, stackSize, blockNest, labels, cacheString(), cacheString() );

	assert ( !stackSize );
	computeGotoBlockNest ( file, nullptr, node, 0, stackSize, blockNest, labels );
}

void compExecutable::fixupGoto ( opFunction *func )
{
	if ( func->conv != fgxFuncCallConvention::opFuncType_Bytecode ) return;
	if ( !func->codeBlock ) return;

	blockNestDef blockNest;
	labelsDef labels;

	if ( !((func->codeBlock->getOp() == astOp::btBasic) && !func->codeBlock->basicData().blocks.size()) )
	{
		size_t stackSize = 0;
		makeLabelList ( file, &file->errHandler, func->codeBlock, 0, stackSize, blockNest, labels, cacheString(), cacheString() );
		assert ( !stackSize );
		computeGotoBlockNest ( file, func, func->codeBlock, 0, stackSize, blockNest, labels);
	}
}

void compExecutable::fixupGoto ()
{
	// compile the functions
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		try	{
			errorLocality e ( &file->errHandler, (*it).second->location );
			fixupGoto ( (*it).second );
		} catch ( errorNum err )
		{
			file->errHandler.throwFatalError ( false, err );
		}
	}
}

