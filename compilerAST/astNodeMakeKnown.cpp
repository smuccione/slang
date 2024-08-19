/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *makeKnownCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool isInFunctionCall, bool &needScan )
{

	switch ( node->getOp() )
	{
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeLocal:
					needScan |= dynamic_cast<symbolLocal *>(node->symbolDef())->makeKnown ( );
					break;
				case symbolSpaceType::symTypeClass:
					break;
				case symbolSpaceType::symTypeStatic:
					needScan |= dynamic_cast<symbolStatic *>(node->symbolDef())->makeKnown ( );
					break;
				case symbolSpaceType::symTypeField:
					needScan |= dynamic_cast<symbolField *>(node->symbolDef())->makeKnown ( );
					break;
				case symbolSpaceType::symTypeParam:
					needScan |= dynamic_cast<symbolSpaceParams *>(node->symbolDef())->makeKnown ( );
					break;
				case symbolSpaceType::symTypeInline:
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case astOp::btInline:
			needScan |= node->inlineFunc().retType.makeKnown ( );
			break;
		default:
			break;
	}
	return node;
}

bool astNode::makeKnown (  class opFile *file )
{
	symbolStack sym ( file );
	bool needScan = false;
	astNodeWalk ( this, &sym, makeKnownCB, needScan );
	return needScan;
}
