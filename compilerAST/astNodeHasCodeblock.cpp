/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *hasCodeblockCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, bool &hasCB )
{
	switch ( node->getOp() )
	{
		case astOp::comp:
			hasCB = true;
			break;
		default:
			break;
	}
	return node;
}

bool astNode::hasCodeblock ( void )
{
	bool	hasCB = false;

	astNodeWalk ( this, nullptr, hasCodeblockCB, hasCB );
	return hasCB;
}
