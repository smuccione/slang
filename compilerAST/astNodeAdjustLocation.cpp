/*

	astNode

*/

#include "compilerParser/fileParser.h"

void astNode::adjustLocation ( srcLocation &stretchLocation )
{
	if ( location.sourceIndex == stretchLocation.sourceIndex )
	{
		if ( location.lineNumberStart < stretchLocation.lineNumberStart )
		{
			stretchLocation.lineNumberStart = location.lineNumberStart;
			stretchLocation.columnNumber = location.columnNumber;
		} else if ( location.lineNumberStart == stretchLocation.lineNumberStart )
		{
			if ( location.columnNumber < stretchLocation.columnNumber )
			{
				stretchLocation.columnNumber = location.columnNumber;
			}
		}
		if ( location.lineNumberEnd > stretchLocation.lineNumberEnd )
		{
			stretchLocation.lineNumberEnd = location.lineNumberEnd;
			stretchLocation.columnNumberEnd = location.columnNumber;
		} else if ( location.lineNumberEnd == stretchLocation.lineNumberEnd )
		{
			if ( location.columnNumber < stretchLocation.columnNumber )
			{
				stretchLocation.columnNumberEnd = location.columnNumber;
			}
		}
	}

	switch ( op )
	{
		case astOp::symbolDef:
			switch ( symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					symbolDef ()->getInitializer ( symbolDef ()->getSymbolName () )->adjustLocation ( stretchLocation );
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							symbolDef()->getInitializer ( symbolDef()->getSymbolName ( ) )->adjustLocation ( stretchLocation );
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			{
				for ( auto *it : basicData().blocks )
				{
					if ( it ) it->adjustLocation ( stretchLocation );
				}
			}
			break;
		case astOp::btSwitch:
			if ( switchData().condition ) switchData().condition->adjustLocation ( stretchLocation );
			for ( auto it = switchData().caseList.begin(); it != switchData().caseList.end(); it++ )
			{
				if ( (*it)->condition ) (*it)->condition->adjustLocation ( stretchLocation );
				if ( (*it)->block ) (*it)->block->adjustLocation ( stretchLocation );
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData().ifList.begin(); it!= ifData().ifList.end(); it++ )
			{
				if ( (*it)->condition ) (*it)->condition->adjustLocation ( stretchLocation );
				if ( (*it)->block ) (*it)->block->adjustLocation ( stretchLocation );
			}
			if ( ifData().elseBlock ) ifData().elseBlock->adjustLocation ( stretchLocation );
			break;
		case astOp::btInline:
			if ( inlineFunc().node ) inlineFunc().node->adjustLocation ( stretchLocation );
			break;
		case astOp::btFor:
			if ( loopData().initializers ) loopData().initializers->adjustLocation ( stretchLocation );
			if ( loopData().condition ) loopData().condition->adjustLocation ( stretchLocation );
			if ( loopData().increase ) loopData().increase->adjustLocation ( stretchLocation );
			if ( loopData().block ) loopData().block->adjustLocation ( stretchLocation );
			break;
		case astOp::btForEach:
			if ( forEachData().statement )
			{
				forEachData().statement->adjustLocation ( stretchLocation );
			}
			break;
		case astOp::btWhile:
			if ( loopData().condition ) loopData().condition->adjustLocation ( stretchLocation );
			if ( loopData().block ) loopData().block->adjustLocation ( stretchLocation );
			break;
		case astOp::btRepeat:
			if ( loopData().block ) loopData().block->adjustLocation ( stretchLocation );
			if ( loopData().condition ) loopData().condition->adjustLocation ( stretchLocation );
			break;
		case astOp::btTry:
			if ( tryCatchData().catchBlock ) tryCatchData().catchBlock->adjustLocation ( stretchLocation );
			if ( tryCatchData().finallyBlock ) tryCatchData().finallyBlock->adjustLocation ( stretchLocation );
			if ( tryCatchData().tryBlock ) tryCatchData().tryBlock->adjustLocation ( stretchLocation );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( returnData().node ) returnData().node->adjustLocation ( stretchLocation );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for (auto& it : arrayData().nodes)
			{
				if (it) it->adjustLocation(stretchLocation);
			}
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for (auto& it : pList().param )
			{
				if ( it ) it->adjustLocation(stretchLocation);
			}
			break;
		case astOp::cSend:
		case astOp::sendMsg:
			break;
		case astOp::conditional:
			if ( conditionData().trueCond ) conditionData().trueCond->adjustLocation ( stretchLocation );
			if ( conditionData().falseCond ) conditionData().falseCond->adjustLocation ( stretchLocation );
			break;
		default:
			break;
	}
	if ( left )	left->adjustLocation ( stretchLocation );
	if ( right ) right->adjustLocation ( stretchLocation );
}