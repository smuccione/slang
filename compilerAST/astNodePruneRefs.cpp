
/*

	astNode

*/

#include "compilerParser/fileParser.h"

// note: because of the recursive call with prune, it makes it very, difficult to use this use node-walk since the callback itself doesn't recurse.

astNode *astNode::pruneRefs ( bool prune )
{
	astNode		*oldNode;
	size_t	 loop;

	if ( (op == astOp::refCreate) && prune )
	{
		switch ( left->getOp() )
		{
			case astOp::symbolValue:
			case astOp::arrayDeref:
			case astOp::cArrayDeref:
			case astOp::sendMsg:
				break;
			default:
				throw errorNum::scILLEGAL_REFERENCE;
		}
		oldNode = left;
		left = 0;
		delete this;
		return oldNode->pruneRefs ( true );
	}

	switch ( op )
	{
		case astOp::symbolDef:
			switch ( symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					symbolDef ()->getInitializer ( symbolDef ()->getSymbolName () )->pruneRefs ( true );
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							(*symbol)[it]->initializer->pruneRefs ( true );
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			for ( auto it = basicData().blocks.begin ( ); it != basicData().blocks.end ( ); it++ )
			{
				(*it)->pruneRefs ( true );
			}
			break;
		case astOp::btSwitch:
			if ( switchData().condition ) switchData().condition =  switchData().condition->pruneRefs ( true );
			assert ( switchData().condition );
			for ( loop = 0; loop < switchData().caseList.size(); loop++ )

			{
				if ( switchData().caseList[loop]->condition ) switchData().caseList[loop]->condition->pruneRefs ( true );
				if ( switchData().caseList[loop]->block ) switchData().caseList[loop]->block->pruneRefs ( true );
			}
			break;
		case astOp::btIf:
			for ( loop = 0; loop < ifData().ifList.size(); loop++ )
			{
				if ( ifData().ifList[loop]->condition ) ifData().ifList[loop]->condition = ifData().ifList[loop]->condition->pruneRefs ( true );
				if ( ifData().ifList[loop]->block ) ifData().ifList[loop]->block = ifData().ifList[loop]->block->pruneRefs ( true );
			}
			if ( ifData().elseBlock ) ifData().elseBlock = ifData().elseBlock->pruneRefs ( true );
			break;
		case astOp::btInline:
			if ( inlineFunc().node ) inlineFunc().node = inlineFunc().node->pruneRefs ( true );
			break;
		case astOp::btFor:
			if ( loopData().initializers ) loopData().initializers =  loopData().initializers->pruneRefs ( true );
			if ( loopData().condition ) loopData().condition =  loopData().condition->pruneRefs ( true );
			if ( loopData().increase ) loopData().increase =  loopData().increase->pruneRefs ( true );
			if ( loopData().block ) loopData().block->pruneRefs ( true );
			break;
		case astOp::btForEach:
			if ( forEachData().var ) forEachData().var =  forEachData().var->pruneRefs ( true );
			if ( forEachData().in ) forEachData().in =  forEachData().in->pruneRefs ( true );
			if ( forEachData().statement ) forEachData().statement->pruneRefs ( true );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			if ( loopData().block ) loopData().block = loopData().block->pruneRefs ( true );
			if ( loopData().condition ) loopData().condition =  loopData().condition->pruneRefs ( true );
			break;
		case astOp::btTry:
			if ( tryCatchData().catchBlock ) tryCatchData().catchBlock = tryCatchData().catchBlock->pruneRefs ( true );
			if ( tryCatchData().finallyBlock ) tryCatchData().finallyBlock = tryCatchData().finallyBlock->pruneRefs ( true );
			if ( tryCatchData().tryBlock ) tryCatchData().tryBlock = tryCatchData().tryBlock->pruneRefs ( true );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( returnData().node )
			{
				returnData().node =  returnData().node->pruneRefs ( false );
				if ( returnData().node->getOp() == astOp::refCreate )
				{
					returnData().node->forceOp ( astOp::refPromote );
				}
			}
			break;
		case astOp::refCreate:
			if ( left->getOp() == astOp::arrayDeref || left->getOp() == astOp::cArrayDeref )
			{
				op = astOp::refPromote;
			}
			switch ( left->getOp() )
			{
				case astOp::symbolValue:
				case astOp::arrayDeref:
				case astOp::cArrayDeref:
				case astOp::sendMsg:
					break;
				default:
					throw errorNum::scILLEGAL_REFERENCE;
			}
			break;
		case astOp::assign:
			left = left->pruneRefs ( false );
			if ( right->getOp() == astOp::refCreate )
			{
				right->forceOp ( astOp::refPromote );
			} else
			{
				right = right->pruneRefs ( true );
			}
			break;
		case astOp::conditional:
			conditionData().trueCond = conditionData().trueCond->pruneRefs ( true );
			conditionData().falseCond = conditionData().falseCond->pruneRefs ( true );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( loop = 0; loop < arrayData().nodes.size(); loop++ )
			{
				arrayData().nodes[loop] = arrayData().nodes[loop]->pruneRefs ( false );
			}
			break;
		case astOp::funcCall:
			left = left->pruneRefs ( true );
			for ( loop = 0; loop < pList().param.size (); loop++ )
			{
				if ( pList().param[loop] ) pList().param[loop] = pList().param[loop]->pruneRefs ( false );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			left = left->pruneRefs ( true );
			for ( loop = 0; loop < pList().param.size(); loop++ )
			{
				if ( pList().param[loop] ) pList().param[loop] = pList().param[loop]->pruneRefs ( true );
			}
			break;
		case astOp::sendMsg:
			if ( left )  left = left->pruneRefs ( false );
			if ( right ) right = right->pruneRefs ( true );
			break;
		case astOp::dummy:
			if ( left )  left = left->pruneRefs ( false );
			break;
		default:
			if ( left )  left = left->pruneRefs ( true );
			if ( right ) right = right->pruneRefs ( true );
			break;
	}
	return this;
}
