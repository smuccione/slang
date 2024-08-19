/*

	astNode

*/

#include "compilerParser/fileParser.h"

void astNode::combineBlocks ( void )
{
	switch ( op )
	{
		case astOp::symbolDef:
			switch ( symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					{
						auto init = symbolDef ()->getInitializer ( symbolDef ()->getSymbolName () );
						if ( init->getOp () == astOp::assign )
						{
							init->right->combineBlocks ();
						}

					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef());
						for ( size_t it = symbol->size ( ); it; it-- )
						{
							auto init = (*symbol)[(uint32_t) it - 1]->initializer;
							if ( init->getOp () == astOp::assign )
							{
								init->right->combineBlocks ( );
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			{
				astBasic basic;

				try
				{
					for ( auto it = basicData().blocks.begin ( ); it != basicData().blocks.end ( ); it++ )
					{
						// do a depth-first combination so recurse first
						if ( (*it) ) (*it)->combineBlocks ( );
						if ( (*it)->getOp ( ) == astOp::btBasic )
						{
							if ( (*it)->basicData().isCombinable ( ) )
							{
								// combine elements from the child block into ours... the child has no local definitions so we do not need to do any stack manipulation
								// this block structure is simply an outcome of parsing or other steps that lead to unneeded complexity
								for ( auto it2 : (*it)->basicData().blocks )
								{
									basic.blocks.push_back ( it2 );
								}
								(*it)->basicData().blocks.clear ( );
								delete *it;
							} else
							{
								basic.blocks.push_back ( *it );
							}
						} else
						{
							basic.blocks.push_back ( *it );
						}
					}
					std::swap ( basicData().blocks, basic.blocks );
				} catch ( ... )
				{
					basic.blocks.clear ( );
					throw;
				}
				basic.blocks.clear ( );
			}
			break;
		case astOp::btSwitch:
			if ( switchData().condition ) switchData().condition->combineBlocks();
			for ( auto it = switchData().caseList.begin(); it != switchData().caseList.end(); it++ )
			{
				if ( (*it)->block ) (*it)->block->combineBlocks();
				if ( (*it)->condition ) (*it)->condition->combineBlocks();
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData().ifList.begin(); it!= ifData().ifList.end(); it++ )
			{
				if ( (*it)->block ) (*it)->block->combineBlocks();
				if ( (*it)->condition ) (*it)->condition->combineBlocks();
			}
			if ( ifData().elseBlock ) ifData().elseBlock->combineBlocks();
			break;
		case astOp::btInline:
			if ( inlineFunc().node ) inlineFunc().node->combineBlocks();
			break;
		case astOp::btFor:
			if ( loopData().initializers ) loopData().initializers->combineBlocks();
			if ( loopData().condition ) loopData().condition->combineBlocks();
			if ( loopData().increase ) loopData().increase->combineBlocks();
			if ( loopData().block ) loopData().block->combineBlocks();
			break;
		case astOp::btForEach:
			if ( forEachData().in ) forEachData().in->combineBlocks();
			if ( forEachData().statement ) forEachData().statement->combineBlocks();
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			if ( loopData().block ) loopData().block->combineBlocks();
			if ( loopData().condition ) loopData().condition->combineBlocks();
			break;
		case astOp::btTry:
			if ( tryCatchData().catchBlock ) tryCatchData().catchBlock->combineBlocks();
			if ( tryCatchData().finallyBlock ) tryCatchData().finallyBlock->combineBlocks();
			if ( tryCatchData().tryBlock ) tryCatchData().tryBlock->combineBlocks();
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( returnData().node ) returnData().node->combineBlocks();
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = arrayData().nodes.begin(); it != arrayData().nodes.end(); it++ )
			{
				if ( (*it) ) (*it)->combineBlocks();
			}
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto it = pList().param.begin(); it != pList().param.end(); it++ )
			{
				if ( (*it) ) (*it)->combineBlocks ();
			}
			break;
		case astOp::conditional:
			conditionData().trueCond->combineBlocks();
			conditionData().falseCond->combineBlocks();
			break;
		default:
			break;
	}
	if ( left )	left->combineBlocks ();
	if ( right ) right->combineBlocks ();
}
