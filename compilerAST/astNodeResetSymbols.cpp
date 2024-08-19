/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *resetTypeCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool )
{
	switch ( node->getOp() )
	{
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName () )->resetType ( sym->file );
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(node->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							(*symbol)[it]->initializer->resetType ( sym->file );
						}
					}
					break;
				default:
					break;
			}
			node->symbolDef()->resetType ( sym->file );
			break;
		case astOp::btInline:
			node->inlineFunc().retType.resetType ( );
			break;
		default:
			break;
	}
	return node;
}

void astNode::resetType (  class opFile *file )
{
	symbolStack sym ( file );
	astNodeWalk ( this, &sym, resetTypeCB );
}
