/*

	astNode

*/


#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

struct renameLocalParams {
	char const *oldName;
	char const *newName;
};

void astNode::renameLocal ( cacheString const &oldName, cacheString const &newName )
{
	switch ( op )
	{
		case astOp::symbolValue:
			if ( symbolValue() == oldName )
			{
				symbolValue() = newName;
			}
			break;
		case astOp::funcValue:
			break;
		case astOp::symbolDef:
			switch ( symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( symbolDef()->getSymbolName ( ) == oldName )
					{
						throw errorNum::scINTERNAL;
					}
					if ( symbolDef()->getInitializer ( symbolDef()->getSymbolName ( ) )->getOp() == astOp::assign )
					{
						symbolDef()->getInitializer ( symbolDef()->getSymbolName ( ) )->right->renameLocal ( oldName, newName );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							if ( (*symbol)[it]->name == oldName )
							{
								throw errorNum::scINTERNAL;
							}
							if ( (*symbol)[it]->initializer->getOp () == astOp::assign )
							{
								(*symbol)[it]->initializer->right->renameLocal ( oldName, newName );
							}
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
				if ( (*it)->getOp() == astOp::symbolDef )
				{
					switch ( (*it)->symbolDef()->symType )
					{
						case symbolSpaceType::symTypeStatic:
						case symbolSpaceType::symTypeLocal:
							if ( (*it)->symbolDef()->getSymbolName ( ) == oldName )
							{
								// we're hiding the origional symbol so stop replacement as this is no longer the correct symbol
								return;
							}
							break;
						case symbolSpaceType::symTypeParam:
							{
								auto symbol = dynamic_cast<symbolSpaceParams *>((*it)->symbolDef());
								for ( uint32_t it = 0; it < symbol->size ( ); it++ )
								{
									if ( (*symbol)[it]->name == oldName )
									{
										return;
									}
								}
							}
							break;
						default:
							break;
					}
				}
				(*it)->renameLocal ( oldName, newName );
			}
			break;
		case astOp::btSwitch:
			if ( switchData().condition ) switchData().condition->renameLocal ( oldName, newName );
			for ( auto it = switchData().caseList.begin ( ); it != switchData().caseList.end ( ); it++ )
			{
				if ( (*it)->block ) (*it)->block->renameLocal ( oldName, newName );
				if ( (*it)->condition ) (*it)->condition->renameLocal ( oldName, newName );
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData().ifList.begin ( ); it != ifData().ifList.end ( ); it++ )
			{
				if ( (*it)->block ) (*it)->block->renameLocal ( oldName, newName );
				if ( (*it)->condition ) (*it)->condition->renameLocal ( oldName, newName );
			}
			if ( ifData().elseBlock ) ifData().elseBlock->renameLocal ( oldName, newName );
			break;
		case astOp::btInline:
			if ( inlineFunc().node ) inlineFunc().node->renameLocal ( oldName, newName );
			break;
		case astOp::btFor:
			if ( loopData().initializers ) loopData().initializers->renameLocal ( oldName, newName );
			if ( loopData().condition ) loopData().condition->renameLocal ( oldName, newName );
			if ( loopData().increase ) loopData().increase->renameLocal ( oldName, newName );
			if ( loopData().block ) loopData().block->renameLocal ( oldName, newName );
			break;
		case astOp::btForEach:
			if ( forEachData().in ) forEachData().in->renameLocal ( oldName, newName );
			if ( forEachData().statement ) forEachData().statement->renameLocal ( oldName, newName );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			if ( loopData().block ) loopData().block->renameLocal ( oldName, newName );
			if ( loopData().condition ) loopData().condition->renameLocal ( oldName, newName );
			break;
		case astOp::btTry:
			if ( tryCatchData().catchBlock ) tryCatchData().catchBlock->renameLocal ( oldName, newName );
			if ( tryCatchData().finallyBlock ) tryCatchData().finallyBlock->renameLocal ( oldName, newName );
			if ( tryCatchData().tryBlock ) tryCatchData().tryBlock->renameLocal ( oldName, newName );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( returnData().node ) returnData().node->renameLocal ( oldName, newName );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = arrayData().nodes.begin ( ); it != arrayData().nodes.end ( ); it++ )
			{
				(*it)->renameLocal ( oldName, newName );
			}
			break;
		case astOp::funcCall:
			for ( auto it = pList().param.begin ( ); it != pList().param.end ( ); it++ )
			{
				if ( (*it) ) (*it)->renameLocal ( oldName, newName );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto it = pList().param.begin ( ); it != pList().param.end ( ); it++ )
			{
				(*it)->renameLocal ( oldName, newName );
			}
			left->renameLocal ( oldName, newName );
			break;
		case astOp::conditional:
			left->renameLocal ( oldName, newName );
			conditionData().trueCond->renameLocal ( oldName, newName );
			conditionData().falseCond->renameLocal ( oldName, newName );
			break;
		default:
			break;
	}
	if ( left )	left->renameLocal ( oldName, newName );
	if ( right ) right->renameLocal ( oldName, newName );
}

void astNode::propagateValue ( cacheString const &oldName, astNode *value )
{
	switch ( op )
	{
		case astOp::symbolValue:
			if ( symbolValue() == oldName )
			{
				release ( );
				*this = *value;
			}
			break;
		case astOp::funcValue:
			break;
		case astOp::symbolDef:
			switch ( symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( symbolDef()->getSymbolName ( ) == oldName )
					{
						throw errorNum::scINTERNAL;
					}
					if ( symbolDef()->getInitializer ( symbolDef()->getSymbolName ( ) )->right )
					{
						symbolDef()->getInitializer ( symbolDef()->getSymbolName ( ) )->right->propagateValue ( oldName, value );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							if ( (*symbol)[it]->name == oldName )
							{
								throw errorNum::scINTERNAL;
							}
							if ( (*symbol)[it]->initializer->getOp () == astOp::assign )
							{
								(*symbol)[it]->initializer->right->propagateValue ( oldName, value );
							}
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
				if ( (*it)->getOp ( ) == astOp::symbolDef )
				{
					switch ( (*it)->symbolDef()->symType )
					{
						case symbolSpaceType::symTypeStatic:
						case symbolSpaceType::symTypeLocal:
							if ( (*it)->symbolDef()->getSymbolName ( ) == oldName )
							{
								// we're hiding the origional symbol so stop replacement as this is no longer the correct symbol
								return;
							}
							break;
						case symbolSpaceType::symTypeParam:
							{
								auto symbol = dynamic_cast<symbolSpaceParams *>((*it)->symbolDef());
								for ( uint32_t it = 0; it < symbol->size ( ); it++ )
								{
									if ( (*symbol)[it]->name == oldName )
									{
										return;
									}
								}
							}
							break;
						default:
							break;
					}
				}
				(*it)->propagateValue ( oldName, value );
			}
			break;
		case astOp::btSwitch:
			if ( switchData().condition ) switchData().condition->propagateValue ( oldName, value );
			for ( auto it = switchData().caseList.begin ( ); it != switchData().caseList.end ( ); it++ )
			{
				if ( (*it)->block ) (*it)->block->propagateValue ( oldName, value );
				if ( (*it)->condition ) (*it)->condition->propagateValue ( oldName, value );
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData().ifList.begin ( ); it != ifData().ifList.end ( ); it++ )
			{
				if ( (*it)->block ) (*it)->block->propagateValue ( oldName, value );
				if ( (*it)->condition ) (*it)->condition->propagateValue ( oldName, value );
			}
			if ( ifData().elseBlock ) ifData().elseBlock->propagateValue ( oldName, value );
			break;
		case astOp::btInline:
			if ( inlineFunc().node ) inlineFunc().node->propagateValue ( oldName, value );
			break;
		case astOp::btFor:
			if ( loopData().initializers ) loopData().initializers->propagateValue ( oldName, value );
			if ( loopData().condition ) loopData().condition->propagateValue ( oldName, value );
			if ( loopData().increase ) loopData().increase->propagateValue ( oldName, value );
			if ( loopData().block ) loopData().block->propagateValue ( oldName, value );
			break;
		case astOp::btForEach:
			if ( forEachData().in ) forEachData().in->propagateValue ( oldName, value );
			if ( forEachData().statement ) forEachData().statement->propagateValue ( oldName, value );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			if ( loopData().block ) loopData().block->propagateValue ( oldName, value );
			if ( loopData().condition ) loopData().condition->propagateValue ( oldName, value );
			break;
		case astOp::btTry:
			if ( tryCatchData().catchBlock ) tryCatchData().catchBlock->propagateValue ( oldName, value );
			if ( tryCatchData().finallyBlock ) tryCatchData().finallyBlock->propagateValue ( oldName, value );
			if ( tryCatchData().tryBlock ) tryCatchData().tryBlock->propagateValue ( oldName, value );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( returnData().node ) returnData().node->propagateValue ( oldName, value );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = arrayData().nodes.begin ( ); it != arrayData().nodes.end ( ); it++ )
			{
				(*it)->propagateValue ( oldName, value );
			}
			break;
		case astOp::funcCall:
			for ( auto it = pList().param.begin ( ); it != pList().param.end ( ); it++ )
			{
				if ( (*it) ) (*it)->propagateValue ( oldName, value );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto it = pList().param.begin ( ); it != pList().param.end ( ); it++ )
			{
				(*it)->propagateValue ( oldName, value );
			}
			left->propagateValue ( oldName, value );
			break;
		case astOp::conditional:
			left->propagateValue ( oldName, value );
			conditionData().trueCond->propagateValue ( oldName, value );
			conditionData().falseCond->propagateValue ( oldName, value );
			break;
		default:
			break;
	}
	if ( left )	left->propagateValue ( oldName, value );
	if ( right ) right->propagateValue ( oldName, value );
}
