/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *forceVariantsCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAssign, bool isInFunctionCall )
{
	switch ( node->getOp() )
	{
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeLocal:
					dynamic_cast<symbolLocal *>(node->symbolDef())->forceAllVariant ( );
					break;
				case symbolSpaceType::symTypeClass:
					break;
				case symbolSpaceType::symTypeStatic:
					dynamic_cast<symbolStatic *>(node->symbolDef())->forceAllVariant ( );
					break;
				case symbolSpaceType::symTypeField:
					// nothing to do... alwasy variant
					break;
				case symbolSpaceType::symTypeParam:
					dynamic_cast<symbolSpaceParams *>(node->symbolDef())->forceVariant ( );
					break;
				case symbolSpaceType::symTypeInline:
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		default:
			break;
	}

	return node;
}

void astNode::forceVariants ( symbolStack *sym )
{
	astNodeWalk ( this, sym, forceVariantsCB );
}
