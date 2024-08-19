/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *firstBreakCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, astNode *&firstBreakOrCond )
{
	switch ( node->getOp () )
	{
		case astOp::btBreak:
			if ( !firstBreakOrCond )
			{
				firstBreakOrCond = node;
			}
			break;
		default:
			break;
	}
	return node;
}

astNode *astNode::firstBreak ( symbolStack *sym )
{
	astNode *firstBreakOrCond = nullptr;
	astNodeWalk ( this, sym, firstBreakCB, firstBreakOrCond );
	return firstBreakOrCond;
}
