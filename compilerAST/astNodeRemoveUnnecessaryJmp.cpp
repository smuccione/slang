/*

	astNode

*/

#include "compilerParser/fileParser.h"

static void removeUnnecessaryJmp ( symbolStack *sym, astNode *node, cacheString const &label )
{
	if ( !node ) return;

	switch ( node->getOp ( ) )
	{
		case astOp::btBasic:
			if ( node->basicData().blocks.size() )
			{
				auto end = *(--node->basicData().blocks.end ( ));
				switch ( end->getOp ( ) )
				{
					case astOp::btBasic:
						end->removeUnnecessaryJmp ( sym, label );
						break;
					case astOp::btReturn:
						if ( end->returnData().label == label )
						{
							end->returnData ().label = cacheString ();
						}
						break;
					case astOp::btCont:
					case astOp::btBreak:
					case astOp::btGoto:
						if ( end->gotoData().label == label )
						{
							end->gotoData().label = cacheString ();
						}
						break;
					default:
						break;
				}
			}
			break;
		case astOp::btReturn:
			if ( node->returnData().label == label )
			{
				node->returnData().label = cacheString ();
			}
			break;
		case astOp::btCont:
		case astOp::btBreak:
		case astOp::btGoto:
			if ( node->gotoData().label == label )
			{
				node->gotoData().label = cacheString ();
			}
			break;
		case astOp::btInline:
			removeUnnecessaryJmp ( sym, node->inlineFunc().node, cacheString() );
			break;
		case astOp::btTry:
			if ( node->tryCatchData().tryBlock )
			{
				if ( node->tryCatchData().finallyBlock )
				{
					removeUnnecessaryJmp ( sym, node->tryCatchData().finallyBlock, label );
				} else
				{
					removeUnnecessaryJmp ( sym, node->tryCatchData().tryBlock, label );
				}
			}
			break;
		case astOp::btFor:
			if ( !node->loopData().condition )
			{
				if ( node->loopData().block )
				{
					removeUnnecessaryJmp ( sym, node->loopData().block, label );
				}
			}
			break;
		case astOp::btIf:
			// to always return we must have an else block and all conditions and the else must return, otherwise there are paths that may not
			if ( node->ifData().elseBlock )
			{
				removeUnnecessaryJmp ( sym, node->ifData().elseBlock, label );
			} else
			{
				if ( node->ifData().ifList.size ( ) )
				{
					auto lastCond = *(--node->ifData().ifList.end ( ));
					removeUnnecessaryJmp ( sym, lastCond->block, label );
				}
			}
			break;
		case astOp::btWhile:
			if ( !node->loopData().condition )
			{
				if ( node->loopData().block )
				{
					removeUnnecessaryJmp ( sym, node->loopData().block, label );
				}
			}
			break;
		case astOp::btRepeat:
			// condition is in the way... if we optimize out condition this will be turned into a basic block and we'll get another chance at it
			break;
		case astOp::btSwitch:
			{
				if ( node->switchData().caseList.size ( ) )
				{
					auto lastCond = *(--node->switchData().caseList.end ( ));
					removeUnnecessaryJmp ( sym, lastCond->block, label );
				}
			}
			break;
		default:
			break;
	}
}

void astNode::removeUnnecessaryJmp ( symbolStack *sym, cacheString const &label )
{
	::removeUnnecessaryJmp ( sym, this, label );
}
