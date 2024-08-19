

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *inlineWeight ( astNode *node, astNode *parent, symbolStack *sym, bool isAssign, bool, size_t &weight )
{
	switch ( node->getOp () )
	{
		case astOp::btFor:
		case astOp::btWhile:
		case astOp::btRepeat:
			weight += 2;
			break;
		case astOp::btSwitch:
			weight += static_cast<uint32_t>(node->switchData().caseList.size () * 2);
			break;
		case astOp::btIf:
			weight += static_cast<uint32_t>(node->ifData().ifList.size () * 2);
			if ( node->ifData().elseBlock ) weight += 1;
			break;
		case astOp::btAnnotation:
		case astOp::btInline:
		case astOp::btBasic:
			break;
		case astOp::funcCall:
			weight += 10;
			break;
		default:
			weight += 1;
			break;
	}
	return node;
}

size_t astNode::complexity ()
{
	size_t weight = 0;

	astNodeWalk ( this, nullptr, inlineWeight, weight );

	return weight;
}
