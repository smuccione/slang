#pragma once

#include "astNode.h"
#include "compilerParser/symbolSpace.h"

template <typename F, typename ...T>
astNode *astNodeWalk_ ( astNode *block, astNode *parent, class symbolStack *sym, F *cb, bool isAccess, bool trailing, bool isInFunctionCall, T &&... t )
{
	if ( !block ) return block;

	if ( !trailing ) block = cb ( block, parent, sym, isAccess, isInFunctionCall, std::forward<T> ( t )... );

	switch ( block->getOp ( ) )
	{
		case astOp::assign:
		case astOp::addAssign:
		case astOp::subAssign:
		case astOp::mulAssign:
		case astOp::divAssign:
		case astOp::modAssign:
		case astOp::powAssign:
		case astOp::bwAndAssign:
		case astOp::bwOrAssign:
		case astOp::bwXorAssign:
		case astOp::shLeftAssign:
		case astOp::shRightAssign:
			block->left = astNodeWalk_ ( block->left, block, sym, cb, false, trailing, isInFunctionCall, std::forward<T> ( t )... );			// false because this is an LHS operand
			block->right = astNodeWalk_ ( block->right, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			return block;
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ) )
					{
						auto init = block->symbolDef ()->getInitializer ( block->symbolDef ()->getSymbolName () );
						if ( init->getOp() == astOp::assign )
						{
							init->right = astNodeWalk_ ( init->right, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
						}
					}
					if ( sym ) sym->push ( block->symbolDef() );
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							auto init = (*symbol)[it]->initializer;
							if ( init->getOp () == astOp::assign )
							{
								(*symbol)[it]->initializer->right = astNodeWalk_ ( (*symbol)[it]->initializer->right, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
							}
						}
						if ( sym ) sym->push ( block->symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass*>(block->symbolDef());
						if ( sym ) sym->insert ( block->symbolDef(), cls->insertPos );
					}
					break;
				default:
					if ( sym ) sym->push ( block->symbolDef() );
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym ? sym->size ( ) : 0;
				bool changed = false;

				astBasic basic;
				basic.blocks.reserve ( block->basicData().blocks.size ( ) );

				try
				{
					for ( auto it = block->basicData().blocks.begin ( ); it != block->basicData().blocks.end ( ); it++ )
					{
						auto expr = astNodeWalk_ ( (*it), block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
						if ( expr )
						{
							basic.blocks.push_back ( expr );
						}
						if ( expr != *it ) changed = true;
					}
				} catch ( ... )
				{
					// clear this so we don't delete blocks!
					// we're in an unstable state with some blocks in the copy and some in the origional
					basic.blocks.clear ();
					throw;
				}
				if ( changed )
				{
					std::swap ( block->basicData().blocks, basic.blocks );
				}
				basic.blocks.clear ( );

				if ( sym ) sym->resize ( size );
			}
			break;
		case astOp::btInline:
			if ( sym )
			{
				errorLocality e ( &sym->file->errHandler, block, true );
				block->inlineFunc ().node = astNodeWalk_ ( block->inlineFunc ().node, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			} else
			{
				block->inlineFunc ().node = astNodeWalk_ ( block->inlineFunc ().node, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			}
			break;
		case astOp::btInlineRet:
		case astOp::btReturn:
			if ( block->returnData().node )
			{
				block->returnData().node = astNodeWalk_ ( block->returnData().node, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			}
			break;
		case astOp::btTry:
			block->tryCatchData ().errSymbol = astNodeWalk_ ( block->tryCatchData ().errSymbol, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->tryCatchData().tryBlock = astNodeWalk_ ( block->tryCatchData().tryBlock, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->tryCatchData().catchBlock = astNodeWalk_ ( block->tryCatchData().catchBlock, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->tryCatchData().finallyBlock = astNodeWalk_ ( block->tryCatchData().finallyBlock, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			break;
		case astOp::btFor:
			block->loopData().initializers = astNodeWalk_ ( block->loopData().initializers, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->loopData().condition = astNodeWalk_ ( block->loopData().condition, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->loopData().block = astNodeWalk_ ( block->loopData().block, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->loopData().increase = astNodeWalk_ ( block->loopData().increase, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			break;
		case astOp::btForEach:
			block->forEachData ().var = astNodeWalk_ ( block->forEachData ().var, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->forEachData().in = astNodeWalk_ ( block->forEachData().in, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->forEachData().statement = astNodeWalk_ ( block->forEachData().statement, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			break;
		case astOp::btIf:
			for ( auto &it : block->ifData().ifList )
			{
				it->condition = astNodeWalk_ ( it->condition, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
				it->block = astNodeWalk_ ( it->block, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			}
			block->ifData().elseBlock = astNodeWalk_ ( block->ifData().elseBlock, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			break;
		case astOp::btWhile:
			block->loopData().condition = astNodeWalk_ ( block->loopData().condition, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->loopData().block = astNodeWalk_ ( block->loopData().block, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			break;
		case astOp::btRepeat:
			block->loopData().block = astNodeWalk_ ( block->loopData().block, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->loopData().condition = astNodeWalk_ ( block->loopData().condition, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			break;
		case astOp::btSwitch:
			assert ( block->switchData().condition );
			block->switchData().condition = astNodeWalk_ ( block->switchData().condition, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			assert ( block->switchData().condition );
			for ( auto &it : block->switchData().caseList )
			{
				it->condition = astNodeWalk_ ( it->condition, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
				it->block = astNodeWalk_ ( it->block, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			}
			break;
		case astOp::conditional:
			block->conditionData().trueCond = astNodeWalk_ ( block->conditionData().trueCond, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			block->conditionData().falseCond = astNodeWalk_ ( block->conditionData().falseCond, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto &it : block->pList().param )
			{
				it = astNodeWalk_ ( it, block, sym, cb, true, trailing, true, std::forward<T> ( t )... );
			}
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto &it : block->arrayData().nodes )
			{
				it = astNodeWalk_ ( it, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
			}
			break;
		case astOp::btBreak:
		case astOp::btLabel:
		case astOp::btGoto:
		case astOp::btCont:
		default:
			break;
	}
	if ( block->left ) block->left = astNodeWalk_ ( block->left, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );
	if ( block->right ) block->right = astNodeWalk_ ( block->right, block, sym, cb, true, trailing, isInFunctionCall, std::forward<T> ( t )... );

	if ( trailing ) block = cb ( block, parent, sym, isAccess, isInFunctionCall, std::forward<T> ( t )... );

	return block;
}

template <typename F, typename ...T>
astNode *astNodeWalk ( astNode *block, symbolStack *sym, F *cb, T &&... t )
{
	return  astNodeWalk_ ( block, 0, sym, cb, true, false, false, std::forward<T> ( t )... );
}

template <typename F, typename ...T>
astNode *astNodeWalkTrailing ( astNode *block, symbolStack *sym, F *cb, T &&... t )
{
	return  astNodeWalk_ ( block, 0, sym, cb, true, true, false, std::forward<T> ( t )... );
}

