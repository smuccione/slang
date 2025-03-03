/*

	astNode

*/

#include "compilerParser/fileParser.h"

size_t astNode::infixPriority()
{
	if ( opToOpDef[(size_t)op] )
	{
		return opToOpDef[(size_t)op]->infixPriority;
	} else return 1;
}

size_t astNode::stackPriority()
{
	if ( opToOpDef[(size_t)op] )
	{
		return opToOpDef[(size_t)op]->stackPriority;
	} else return 1;
}

bool astNode::isLeft()
{
	if ( opToOpDef[(size_t)op] )
	{
		return opToOpDef[(size_t)op]->isLeft;
	} else return true;
}

bool astNode::isUnaryOp()
{
	switch ( op )
	{
		case astOp::codeBlockExpression:
		case astOp::stringValue:
		case astOp::intValue:
		case astOp::doubleValue:
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
		case astOp::codeBlockValue:
		case astOp::funcValue:
		case astOp::symbolValue:
		case astOp::atomValue:
		case astOp::nullValue:
		case astOp::fieldValue:
		case astOp::nilValue:
		case astOp::getEnumerator:
		case astOp::dummy:
		case astOp::singleLineComment:
		case astOp::commentBlock:
		case astOp::metadata:
		case astOp::btBasic:					// this is really an error condition but can be present during LS processing
			return false;
		default:
			break;
	}

	return (opToOpDef[(size_t) op]->opCat == astOpCat::opUnary || opToOpDef[(size_t) op]->opCat == astOpCat::opNSSelf ? true : false);

#if 0
	for ( size_t loop = 0; opList[loop].c; loop++ )
	{
		if ( opList[loop].op == op )
		{
			assert ( (opList[loop].opCat == astOpCat::opUnary || opList[loop].opCat == astOpCat::opNSSelf ? true : false) ==
					 (opToOpDef[(size_t) op]->opCat == astOpCat::opUnary || opToOpDef[(size_t) op]->opCat == astOpCat::opNSSelf ? true : false)
					);
			return ( opList[loop].opCat == astOpCat::opUnary || opList[loop].opCat == astOpCat::opNSSelf ? true : false );
		}
	}
	return ( false );
#endif
}

bool astNode::isBinaryOp ()
{
	switch ( op )
	{
		case astOp::codeBlockExpression:
		case astOp::stringValue:
		case astOp::intValue:
		case astOp::doubleValue:
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
		case astOp::codeBlockValue:
		case astOp::funcValue:
		case astOp::symbolValue:
		case astOp::atomValue:
		case astOp::nullValue:
		case astOp::fieldValue:
		case astOp::nilValue:
		case astOp::getEnumerator:
		case astOp::dummy:
		case astOp::singleLineComment:
		case astOp::commentBlock:
		case astOp::metadata:
		case astOp::btBasic:					// this is really an error condition but can be present during LS processing
			return false;
		default:
			break;
	}

	if ( !opToOpDef[(size_t)op] )
	{
		return false;
	}
	return true;
}

