/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *validateCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, errorHandler *err )
{
	errorLocality e ( err, node );

	assert ( static_cast<size_t>( node->getOp ( )) <= static_cast<size_t>(astOp::endParse) );
	assert ( node->location.sourceIndex <= sym->file->srcFiles.getCount() );

	return node;
}

void astNode::validate ( symbolStack *sym, class errorHandler *err )
{
	astNodeWalk ( this, sym, validateCB, err );
}

