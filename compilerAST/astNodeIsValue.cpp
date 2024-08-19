/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

bool astNode::isValue ( )
{
	size_t	loop;
	switch ( op )
	{
		case astOp::stringValue:
		case astOp::intValue:
		case astOp::doubleValue:
		case astOp::atomValue:
		case astOp::nullValue:
		case astOp::funcValue:
		case astOp::codeBlockExpression:
		case astOp::codeBlockValue:
			return (true);
		case astOp::varrayValue:
		case astOp::arrayValue:
			for ( loop = 0; loop < arrayData().nodes.size ( ); loop++ )
			{
				if ( !arrayData().nodes[loop]->isValue ( ) ) return false;
			}
			return true;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			if ( !left->isValue ( ) ) return false;
			for ( loop = 0; loop < pList().param.size ( ); loop++ )
			{
				if ( pList().param[loop] && !pList().param[loop]->isValue ( ) ) return false;
			}
			return true;
		default:
			break;
	}
	return (false);
}

bool astNode::isValueType ( )
{
	switch ( op )
	{
		case astOp::stringValue:
		case astOp::intValue:
		case astOp::doubleValue:
		case astOp::symbolValue:				
		case astOp::codeBlockValue:
		case astOp::codeBlockExpression:
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::nullValue:
		case astOp::funcValue:
		case astOp::senderValue:
		case astOp::getEnumerator:
		case astOp::compValue:
		case astOp::errorValue:
			return (true);
		default:
			break;
	}
	return (false);
}

bool astNode::isTrue ( )
{
	switch ( op )
	{
		case astOp::logicalNot:
			if ( left->isFalse ( ) )
			{
				return true;
			}
			break;
		case astOp::intValue:
			if ( (bool)intValue() )
			{
				return true;
			}
			break;
		case astOp::doubleValue:
			if ( (bool)doubleValue() )
			{
				return true;
			}
			break;
		case astOp::stringValue:
			if ( (bool)stringValue().size ( ) )
			{
				return true;
			}
			break;
		case astOp::codeBlockExpression:
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::codeBlockValue:
		case astOp::atomValue:
		case astOp::funcValue:
		case astOp::pairValue:
			return true;
		default:
			break;
	}
	return false;
}

bool astNode::isFalse ( )
{
	switch ( op )
	{
		case astOp::logicalNot:
			if ( left->isTrue ( ) )
			{
				return true;
			}
			break;
		case astOp::intValue:
			if ( !(bool)intValue() )
			{
				return true;
			}
			break;
		case astOp::doubleValue:
			if ( !(bool)doubleValue() )
			{

				return true;
			}
			break;
		case astOp::stringValue:
			if ( !(bool) stringValue().size ( ) )
			{
				return true;
			}
			break;
		case astOp::nullValue:
			return true;
		default:
			break;
	}
	return false;
}

bool astNode::isNull ( )
{
	switch ( op )
	{
		case astOp::nullValue:
			return true;
		default:
			break;
	}
	return false;
}

