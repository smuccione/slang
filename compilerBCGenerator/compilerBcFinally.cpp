#include "compilerBCGenerator.h"

/*
	try {
		try {
			goto here
		} finally {
			print ( "hello " )
		}
	} finally {
		println ( "world" )
	}
	here:


	the above should print "hello world"  

*/

static void iterateLabels ( opFile *file, astNode *block, std::vector<astNode*> &trys, std::map<cacheString, std::vector<astNode *>> &tryLabelMap )
{
	switch ( block->getOp() )
	{
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ) )
					{
						iterateLabels ( file, block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ), trys, tryLabelMap);
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							if ( (*symbol)[it]->initializer->getOp () == astOp::assign )
							{
								iterateLabels ( file, (*symbol)[it]->initializer, trys, tryLabelMap );
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			for ( auto &it : block->basicData().blocks )
			{
				iterateLabels ( file, it, trys, tryLabelMap);
			}
			break;
		case astOp::btInline:
			iterateLabels ( file, block->inlineFunc().node, trys, tryLabelMap);
			break;
		case astOp::btLabel:
			break;
		case astOp::btGoto:
		case astOp::btBreak:
		case astOp::btCont:
			assert ( !block->gotoData().finallyBlock );
			{
				auto tryStack = tryLabelMap.find ( block->gotoData().label );
				if (tryStack != tryLabelMap.end ( ) )
				{
					size_t loop;
					for ( loop = 0; (loop < tryStack->second.size ( )) && (loop < trys.size ( )); loop++ )
					{
						if ( tryStack->second[loop] != trys[loop] )
						{
							break;
						}
					}
					for ( size_t loop2 = trys.size ( ); loop2 >loop; loop2-- )
					{
						if ( trys[loop2 - 1]->tryCatchData().finallyBlock )
						{
							if ( !block->gotoData().finallyBlock )
							{
								block->gotoData().finallyBlock = new astNode ( file->sCache, astOp::btBasic );
							}
							block->gotoData().finallyBlock->basicData().blocks.push_back ( new astNode ( *trys[loop2 - 1]->tryCatchData().finallyBlock ) );
						}
					}
				}
			}
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			assert ( !block->returnData().finallyBlock );
			for ( auto it = trys.rbegin ( ); it != trys.crend ( ); it++ )
			{
				if ( (*it)->tryCatchData().finallyBlock )
				{
					if ( !block->returnData().finallyBlock )

					{
						block->returnData().finallyBlock = new astNode ( file->sCache, astOp::btBasic );
					}
					block->returnData().finallyBlock->basicData().blocks.push_back ( new astNode ( *(*it)->tryCatchData().finallyBlock ) );
				}
			}
			if ( block->returnData().node ) iterateLabels ( file, block->returnData().node, trys, tryLabelMap );
			break;
		case astOp::btTry:
			trys.push_back ( block );
			if ( block->tryCatchData().tryBlock )iterateLabels ( file, block->tryCatchData().tryBlock, trys, tryLabelMap);
			if ( block->tryCatchData().catchBlock ) iterateLabels ( file, block->tryCatchData().catchBlock, trys, tryLabelMap);
			trys.pop_back ( );
			if ( block->tryCatchData().finallyBlock) iterateLabels ( file, block->tryCatchData().finallyBlock, trys, tryLabelMap);
			break;
		case astOp::btFor:
		case astOp::btWhile:
		case astOp::btRepeat:
			if ( block->loopData().increase ) iterateLabels ( file, block->loopData().increase, trys, tryLabelMap);
			if ( block->loopData().initializers ) iterateLabels ( file, block->loopData().initializers, trys, tryLabelMap);
			if ( block->loopData().block ) iterateLabels ( file, block->loopData().block, trys, tryLabelMap);
			break;
		case astOp::btForEach:
			if ( block->forEachData().statement ) iterateLabels ( file, block->forEachData().statement, trys, tryLabelMap);
			break;
		case astOp::btIf:
			for ( auto &it : block->ifData().ifList )
			{
				if ( it->block ) iterateLabels ( file, it->block, trys, tryLabelMap );
				if ( it->condition ) iterateLabels ( file, it->condition, trys, tryLabelMap );
			}

			if ( block->ifData().elseBlock )
			{
				iterateLabels ( file, block->ifData().elseBlock, trys, tryLabelMap);
			}
			break;
		case astOp::btSwitch:
			std::vector<astCondBlock *>::size_type	loop;
			if ( block->switchData().condition ) iterateLabels ( file, block->switchData().condition, trys, tryLabelMap);
			for ( loop = 0; loop < block->switchData().caseList.size(); loop++ )
			{
				if ( block->switchData().caseList[loop]->block ) iterateLabels ( file, block->switchData().caseList[loop]->block, trys, tryLabelMap );
			}
			break;
		default:
			if ( block->left ) iterateLabels ( file, block->left, trys, tryLabelMap);
			if ( block->right ) iterateLabels ( file, block->right, trys, tryLabelMap);
			break;
	}
}

static void markLabels ( astNode *block, std::vector<astNode*> trys, std::map<cacheString, std::vector<astNode*>>& tryLabelMap )
{
	switch ( block->getOp() )
	{
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ) )
					{
						markLabels ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ), trys, tryLabelMap );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							if ( (*symbol)[it]->initializer->getOp() == astOp::assign )
							{
								markLabels ( (*symbol)[it]->initializer, trys, tryLabelMap );
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			for ( auto &it : block->basicData().blocks )
			{
				markLabels ( it, trys, tryLabelMap );
			}
			break;
		case astOp::btInline:
			markLabels ( block->inlineFunc().node, trys, tryLabelMap );
			break;
		case astOp::btLabel:
			tryLabelMap[block->label()] = trys;
			break;
		case astOp::btGoto:
		case astOp::btBreak:
		case astOp::btCont:
			break;
		case astOp::btReturn:
			if ( block->returnData().node ) markLabels ( block->returnData().node, trys, tryLabelMap );
			break;
		case astOp::btTry:
			trys.push_back ( block );
			if ( block->tryCatchData().tryBlock )markLabels ( block->tryCatchData().tryBlock, trys, tryLabelMap );
			if ( block->tryCatchData().catchBlock ) markLabels ( block->tryCatchData().catchBlock, trys, tryLabelMap );
			trys.pop_back ( );
			if ( block->tryCatchData().finallyBlock ) markLabels ( block->tryCatchData().finallyBlock, trys, tryLabelMap );
			break;
		case astOp::btFor:
		case astOp::btWhile:
		case astOp::btRepeat:
			tryLabelMap[block->loopData().breakLabel] = trys;
			tryLabelMap[block->loopData().continueLabel] = trys;
			if ( block->loopData().condition ) markLabels ( block->loopData().condition, trys, tryLabelMap );
			if ( block->loopData().increase ) markLabels ( block->loopData().increase, trys, tryLabelMap );
			if ( block->loopData().block ) markLabels ( block->loopData().block, trys, tryLabelMap );
			break;
		case astOp::btForEach:
			if ( block->forEachData().statement ) markLabels ( block->forEachData().statement, trys, tryLabelMap );
			break;
		case astOp::btIf:
			for ( auto& it : block->ifData().ifList )
			{
				if ( it->block ) markLabels ( it->block, trys, tryLabelMap );
				if ( it->condition ) markLabels ( it->condition, trys, tryLabelMap );
			}
			if ( block->ifData().elseBlock ) markLabels ( block->ifData().elseBlock, trys, tryLabelMap );
			break;
		case astOp::btSwitch:
			std::vector<astCondBlock *>::size_type	loop;
			tryLabelMap[block->switchData().breakLabel] = trys;
			for ( loop = 0; loop < block->switchData().caseList.size(); loop++ )
			{
				if ( block->switchData().caseList[loop]->block ) markLabels ( block->switchData().caseList[loop]->block, trys, tryLabelMap );
				if ( block->switchData().caseList[loop]->condition ) markLabels ( block->switchData().caseList[loop]->condition, trys, tryLabelMap );
			}
			break;
		default:
			if ( block->left ) markLabels ( block->left, trys, tryLabelMap );
			if ( block->right ) markLabels ( block->right, trys, tryLabelMap );
			break;
	}
}

void compExecutable::compTagFinally ( )
{
	std::map<cacheString, std::vector< astNode* >> tryLabelMap;
	std::vector<astNode*>		trys;

	// compile the functions
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		if ( (*it).second->codeBlock )
		{
			// first build our map of labels and the nested try/catches enclosing that label
			markLabels ( (*it).second->codeBlock, trys, tryLabelMap );
			// for each exit point we then look up the labels and at the divergence point iterate downards and build add the finally blocks to the exit points finally
			iterateLabels ( file, (*it).second->codeBlock, trys, tryLabelMap );
		}
	}
}

