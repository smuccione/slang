/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static bool hasNoSideEffects (astNode *node )
{
	if ( !node ) return true;

	switch ( node->getOp () )
	{
		case astOp::singleLineComment:
		case astOp::commentBlock:
		case astOp::btAnnotation:
		case astOp::btStatement:
		case astOp::btLabel:
			return true;
		case astOp::btBasic:
			for ( auto it = node->basicData().blocks.begin (); it != node->basicData().blocks.end (); it++ )
			{
				if ( !hasNoSideEffects ( *it ) )
				{
					return false;
				}
			}
			return true;
		default:
			return false;
	}
}

bool astNode::isMetadata (  )
{
	return ::hasNoSideEffects ( this );
}
