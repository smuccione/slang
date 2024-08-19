
#include "compilerBCGenerator.h"
#include "compilerOptimizer.h"

astNode *optimizer::removeUnusedVariables ( astNode *block, symbolStack *sym )
{
	if ( !block )
	{
		return nullptr;
	}

	switch ( block->getOp ( ) )
	{
		case astOp::btBasic:
			{
				astBasic newList;
				auto size = sym->size ();
				for ( auto &it : block->basicData().blocks )
				{
					astNode *newBlock = removeUnusedVariables ( it, sym );
					if ( newBlock )
					{
						newList.blocks.push_back ( newBlock );
					}
				}
				sym->resize ( size );
				block->basicData().blocks.swap ( newList.blocks );
				newList.blocks.clear ( );

				if ( !block->basicData().blocks.size ( ) )
				{
					delete block;
					return nullptr;
				}
			}
			break;
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					{
						auto init = block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName () );
						if ( !block->symbolDef()->isAccessed( block->symbolDef()->getSymbolName(), true ) )
						{
							if ( !block->symbolDef()->isAssigned ( block->symbolDef()->getSymbolName (), true ) && !block->symbolDef()->isClosedOver ( block->symbolDef()->getSymbolName () ) )
							{
								if ( (init->getOp () != astOp::assign) || (init->getOp () == astOp::assign && init->right->hasNoSideEffects( sym, false )) )
								{
									delete block;
									return nullptr;
								}
							}
						}
						block->symbolDef()->setInitializer ( block->symbolDef()->getSymbolName (), removeUnusedVariables ( init, sym ) );
						sym->push ( block->symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeParam:
					// we need to do this for the case of inlined functions that have child functions inlined.  this may remove the need for a specific parameter and we can eliminate them when not necessary
					dynamic_cast<symbolSpaceParams*>(block->symbolDef())->pruneUnused(sym);
					if (!dynamic_cast<symbolSpaceParams*>(block->symbolDef())->size())
					{
						delete block;
						return nullptr;
					}
					sym->push ( block->symbolDef() );
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass*>(block->symbolDef());
						sym->insert( block->symbolDef(), cls->insertPos );
					}
					break;
				default:
					sym->push ( block->symbolDef() );
					break;
			}
			break;
		case astOp::btInline:
			block->inlineFunc().node = removeUnusedVariables ( block->inlineFunc().node, sym );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			block->returnData().node = removeUnusedVariables ( block->returnData().node, sym );
			break;
		case astOp::btLabel:
		case astOp::btGoto:
		case astOp::btBreak:
		case astOp::btCont:
			assert ( !block->left );
			assert ( !block->right );
			break;
		case astOp::btTry:
			block->tryCatchData().tryBlock = removeUnusedVariables ( block->tryCatchData().tryBlock, sym );
			block->tryCatchData().finallyBlock = removeUnusedVariables ( block->tryCatchData().finallyBlock, sym );
			block->tryCatchData().catchBlock = removeUnusedVariables ( block->tryCatchData().catchBlock, sym );
			break;
		case astOp::btFor:
		case astOp::btWhile:
		case astOp::btRepeat:
			// emit entry condition code
			block->loopData().condition = removeUnusedVariables ( block->loopData().condition, sym );
			block->loopData().block = removeUnusedVariables ( block->loopData().block, sym );
			block->loopData().increase = removeUnusedVariables ( block->loopData().increase, sym );
			block->loopData().initializers = removeUnusedVariables ( block->loopData().initializers, sym );
			break;
		case astOp::btForEach:
			block->forEachData().statement = removeUnusedVariables ( block->forEachData().statement, sym );
			break;
		case astOp::btIf:
			for ( auto &it : block->ifData().ifList )
			{
				it->block = removeUnusedVariables ( it->block, sym );
				it->condition = removeUnusedVariables ( it->condition, sym );
			}
			block->ifData().elseBlock = removeUnusedVariables ( block->ifData().elseBlock, sym );
			break;
		case astOp::btSwitch:
			for ( auto &it : block->switchData().caseList )
			{
				it->block = removeUnusedVariables ( it->block, sym );
				it->condition = removeUnusedVariables ( it->condition, sym );
			}
			break;
		case astOp::conditional:
			block->conditionData().trueCond = removeUnusedVariables ( block->conditionData().trueCond, sym );
			block->conditionData().falseCond = removeUnusedVariables ( block->conditionData().falseCond, sym );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto &it : block->arrayData().nodes )
			{
				it = removeUnusedVariables ( it, sym );
				if ( !it )
				{
					it = new astNode ( file->sCache, astOp::nullValue );
				}
			}
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto &it : block->pList().param )
			{
				it = removeUnusedVariables ( it, sym );
				if ( !it )
				{
					it = new astNode ( file->sCache, astOp::nullValue );
				}
			}
			break;
		case astOp::pairValue:	/* do nothing for pair's... we need to have a case astOp::so we don't hit the default and eliminate a needed value*/
		case astOp::comp:		/* do nothing for compilation values... these are runtime evaluated and may have sideffects regardless of usage*/
		default:
			break;
	}
	if ( block->left ) block->left = removeUnusedVariables ( block->left, sym );
	if ( block->right ) block->right = removeUnusedVariables ( block->right, sym );
	return (block);
}

void optimizer::removeUnusedVariables ( opFunction *func )
{
	if ( func->isInterface || !func->codeBlock || !func->inUse || func->isIterator ) return;

	symbolStack	sym ( file, func );

	if ( !((func->codeBlock->getOp ( ) == astOp::btBasic) && !func->codeBlock->basicData().blocks.size ( )) )
	{
		func->codeBlock = removeUnusedVariables ( func->codeBlock, &sym );
	}
}


void optimizer::removeUnusedVariables ( )
{
	// compile the functions
	for ( auto it = file->functionList.begin ( ); it != file->functionList.end ( ); it++ )
	{
		errorLocality e ( &file->errHandler, (*it).second->location );
		removeUnusedVariables ( (*it).second );
	}
}

