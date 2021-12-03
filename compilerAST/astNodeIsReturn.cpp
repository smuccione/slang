/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *isReturn ( symbolStack *sym, astNode *node )
{
	if ( !node ) return nullptr;

	switch ( node->getOp ( ) )
	{
		case astOp::btBasic:
			{
				auto size = sym->size ( );
				for ( auto it : node->basicData().blocks )
				{
					auto node = isReturn ( sym, it );
					if ( !node ) return nullptr;
					if ( node->getOp ( ) == astOp::btReturn )
					{
						return node;
					}
				}
				sym->resize ( size );
			}
			break;
		case astOp::btReturn:
		case astOp::btLabel:
			return node;
		default:
			return nullptr;
	}
	return nullptr;
}

astNode *astNode::isReturn ( symbolStack *sym )
{
	return ::isReturn ( sym, this );
}
