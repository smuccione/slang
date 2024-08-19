/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"
#include "Utility/counter.h"

static astNode *fixupReturnCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, uint64_t value, opFunction *func )
{
	astNode	*newGoto;
	astNode	*newValue;

	switch ( node->getOp() )
	{
		case astOp::atomValue:
#if 0
			if ( !strstr ( node->data.s, "::" ) && strccmp ( node->data.s, "new" ) &&  strccmp ( node->data.s, "debugbreak" ) && strccmp ( node->data.s, "getenumerator" ) )
			{

				cacheString tmp = "::";
				tmp += node->data.s;
				node->data.s = tmp;
			}
#endif
			break;
		case astOp::btLabel:
			if ( node->label() != sym->file->barrierValue )
			{
				node->label () = sym->file->sCache.get ( MakeUniqueLabel ( node->label (), value ) );
			}
			break;
		case astOp::btGoto:
		case astOp::btCont:
		case astOp::btBreak:
			node->gotoData ().label = sym->file->sCache.get ( MakeUniqueLabel ( node->gotoData ().label, value ) );
			break;
		case astOp::btFor:
		case astOp::btWhile:
		case astOp::btRepeat:
			node->loopData().breakLabel = sym->file->sCache.get ( MakeUniqueLabel ( node->loopData().breakLabel, value ) );
			node->loopData().continueLabel = sym->file->sCache.get ( MakeUniqueLabel ( node->loopData().continueLabel, value ) );
//			node->loopData().continueLabel = node->loopData().continueLabel + "<fixupReturn>";
			break;
		case astOp::btSwitch:
			node->switchData ().breakLabel = sym->file->sCache.get ( MakeUniqueLabel ( node->switchData ().breakLabel, value ) );
			break;
		case astOp::btInlineRet:
			node->returnData().label = sym->file->sCache.get ( MakeUniqueLabel ( node->returnData ().label, value ) );
			break;
		case astOp::btReturn:
			// before we do anything else, we need to modify the label
			{
				if ( func->isProcedure )
				{
					newGoto = (new astNode ( sym->file->sCache, astOp::btGoto ))->setLocation ( node );
					newGoto->gotoData().nPop = node->returnData().nPop;
					newGoto->gotoData().finallyBlock = 0;
					newGoto->gotoData().label = sym->file->sCache.get ( MakeUniqueLabel ( node->returnData ().label, value ) );
					newValue = nullptr;

					//	emit Finally
					//	emit value
					//	goto ret label
					auto finallyBlock = node->returnData ().finallyBlock;
					node->returnData ().finallyBlock = nullptr;
					node->returnData ().node = nullptr;
					node->release ();
					*node = astNode ( sym->file->sCache, astOp::btBasic );
					if ( finallyBlock )
					{
						node->pushNode ( finallyBlock );
					}
					if ( newValue )
					{
						node->pushNode ( newValue );
					}
					node->pushNode ( newGoto );
				} else
				{
					// NOTE: we do NOT modify the label here... this will be further recursed and the resulting btSLide will be renamed... if we do it here we will get duplicate $value's appended!
					node->forceOp ( astOp::btInlineRet );
					node->returnData ().label = sym->file->sCache.get ( MakeUniqueLabel ( node->returnData ().label, value ) );
				}
			}
			break;
		case astOp::btExit:
			*node = astNode ( sym->file->sCache, astOp::nilValue );
			break;
		default:
			break;
	}
	return node;
}

void astNode::fixupReturn ( symbolStack *sym, opFunction *func )
{
	auto value = GetUniqueCounter ( );
	astNodeWalk ( this, sym, fixupReturnCB, value, func );
}
	