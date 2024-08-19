/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *hasFunctionReferenceCB( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool isInFunctionCall, bool &hasFunction )
{
	switch ( node->getOp() )
	{
		case astOp::funcCall:
			switch ( node->left->getOp() )
			{
				case astOp::atomValue:
				case astOp::sendMsg:
					break;
				default:
					hasFunction = true;
					break;
			}
			break;
		case astOp::sendMsg:
			if ( node->left )
			{
				symbolTypeClass type = node->left->getType ( sym );
				if ( !type.isAnObject() )
				{
					switch ( node->right->getOp() )
					{
						case astOp::stringValue:
							break;
						default:
							hasFunction = true;
							break;
					}
				}
			}
			break;
		default:
			break;
	}
	return node;
}

bool astNode::hasFunctionReference ( symbolStack *sym )
{
	bool hasFunction = false;

	astNodeWalk ( this, sym, hasFunctionReferenceCB, hasFunction );
	return hasFunction;
}
