/*

	astNode

*/

#include "compilerParser/fileParser.h"

static bool alwaysReturns ( astNode *node, bool &hasJmp, bool &hasBreak, bool &hasContinue )
{
	if ( !node ) return false;

	switch ( node->getOp ( ) )
	{
		case astOp::btBasic:
			{
				bool alwaysRet = false;
				for ( auto &it : node->basicData().blocks )
				{
					if ( it->getOp ( ) == astOp::btLabel ) alwaysRet = false;
					if ( alwaysReturns ( it, hasJmp, hasBreak, hasContinue ) ) alwaysRet = true;
				}
				if ( alwaysRet ) return true;
			}
			break;
		case astOp::btReturn:
			if ( !node->returnData ().isYield )
			{
				return true;
			}
			break;
		case astOp:: btInline:
			return false;
		case astOp::btInlineRet:
			return true;
		case astOp::btCont:
			hasContinue = true;
			return false;
		case astOp::btBreak:
			hasBreak = true;
			return false;
		case astOp::btGoto:
			hasJmp = true;
			return false;
		case astOp::btTry:
			if ( !alwaysReturns ( node->tryCatchData().tryBlock, hasJmp, hasBreak, hasContinue ) )
			{
				return alwaysReturns ( node->tryCatchData().finallyBlock, hasJmp, hasBreak, hasContinue );
			}
			if ( node->tryCatchData().catchBlock )
			{
				if ( !alwaysReturns ( node->tryCatchData().catchBlock, hasJmp, hasBreak, hasContinue ) )
				{
					return alwaysReturns ( node->tryCatchData().finallyBlock, hasJmp, hasBreak, hasContinue );
				}
			}
			break;
		case astOp::btForEach:
			return false;
		case astOp::btFor:
			if ( !node->loopData().condition )
			{
				if ( node->loopData().block )
				{
					bool hb = false, hc = false;
					auto ret = alwaysReturns ( node->loopData().block, hasJmp, hb, hc );
					if ( hb ) return false;
					return ret;
				}
			}
			break;
		case astOp::btIf:
			// to always return we must have an else block and all conditions and the else must return, otherwise there are paths that may not
			if ( node->ifData().elseBlock )
			{
				for ( auto it = node->ifData().ifList.begin ( ); it != node->ifData().ifList.end ( ); it++ )
				{
					if ( (*it)->block ) if ( !alwaysReturns ( (*it)->block, hasJmp, hasBreak, hasContinue ) ) return false;
				}
				if ( !alwaysReturns ( node->ifData().elseBlock, hasJmp, hasBreak, hasContinue ) ) return false;
				return true;
			}
			return false;
		case astOp::btWhile:
			if ( !node->loopData().condition )
			{
				if ( node->loopData().block )
				{
					bool hb = false, hc = false;
					auto ret = alwaysReturns ( node->loopData().block, hasJmp, hb, hc );
					if ( hb ) return false;
					return ret;
				}
			}
			break;
		case astOp::btRepeat:
			if ( node->loopData().block )
			{
				bool hb = false, hc = false;
				auto ret = alwaysReturns ( node->loopData().block, hasJmp, hb, hc );
				if ( hb ) return false;
				// for repeat-until loops, if there is a continue and we have a condition then the condition may evaluate and we may exit
				if ( hc && node->loopData().condition ) return false;
				return ret;
			}
			break;
		case astOp::btSwitch:
			{
				bool hasDefault = false;
				for ( auto it = node->switchData().caseList.begin ( ); it != node->switchData().caseList.end ( ); it++ )
				{
					if ( !(*it)->condition ) hasDefault = true;
					if ( (*it)->block ) if ( !alwaysReturns ( (*it)->block, hasJmp, hasBreak, hasContinue ) ) return false;
				}
				return hasDefault;		// every block returns... if one of them is ALSO a default, then we always return... one MUST be a default or we can fallthrough without returning
			}
		default:
			return false;
	}
	return false;
}

bool astNode::alwaysReturns ()
{
	bool hasJmp = false, hasBreak = false, hasContinue = false;
	auto ret = ::alwaysReturns ( this, hasJmp, hasBreak, hasContinue );
	return !hasBreak && !hasContinue && !hasJmp && ret;
}
