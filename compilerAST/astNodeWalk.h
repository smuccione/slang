#pragma once

#include "astNode.h"
#include "compilerParser/symbolSpace.h"

template <typename F, typename ...T>
astNode *_astNodeWalk ( astNode *block, astNode *parent, class symbolStack *sym, F *cb, bool isAccess, T &&... t )
{
	if ( !block ) return block;

	block = cb ( block, parent, sym, isAccess, std::forward<T> ( t )... );

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
			block->left = _astNodeWalk ( block->left, block, sym, cb, false, std::forward<T> ( t )... );			// false because this is an LHS operand
			block->right = _astNodeWalk ( block->right, block, sym, cb, true, std::forward<T> ( t )... );
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
							init->right = _astNodeWalk ( init->right, block, sym, cb, true, std::forward<T> ( t )... );
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
								(*symbol)[it]->initializer->right = _astNodeWalk ( (*symbol)[it]->initializer->right, block, sym, cb, true, std::forward<T> ( t )... );
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
						auto expr = _astNodeWalk ( (*it), block, sym, cb, true, std::forward<T> ( t )... );
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
		case astOp::btInlineRet:
			block->node() = _astNodeWalk ( block->node(), block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btInline:
			block->inlineFunc().node = _astNodeWalk ( block->inlineFunc().node, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btReturn:
			if ( block->returnData().node )
			{
				block->returnData().node = _astNodeWalk ( block->returnData().node, block, sym, cb, true, std::forward<T> ( t )... );
			}
			break;
		case astOp::btTry:
			block->tryCatchData ().errSymbol = _astNodeWalk ( block->tryCatchData ().errSymbol, block, sym, cb, true, std::forward<T> ( t )... );
			block->tryCatchData().tryBlock = _astNodeWalk ( block->tryCatchData().tryBlock, block, sym, cb, true, std::forward<T> ( t )... );
			block->tryCatchData().catchBlock = _astNodeWalk ( block->tryCatchData().catchBlock, block, sym, cb, true, std::forward<T> ( t )... );
			block->tryCatchData().finallyBlock = _astNodeWalk ( block->tryCatchData().finallyBlock, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btFor:
			block->loopData().initializers = _astNodeWalk ( block->loopData().initializers, block, sym, cb, true, std::forward<T> ( t )... );
			block->loopData().condition = _astNodeWalk ( block->loopData().condition, block, sym, cb, true, std::forward<T> ( t )... );
			block->loopData().block = _astNodeWalk ( block->loopData().block, block, sym, cb, true, std::forward<T> ( t )... );
			block->loopData().increase = _astNodeWalk ( block->loopData().increase, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btForEach:
			block->forEachData ().var = _astNodeWalk ( block->forEachData ().var, block, sym, cb, true, std::forward<T> ( t )... );
			block->forEachData().in = _astNodeWalk ( block->forEachData().in, block, sym, cb, true, std::forward<T> ( t )... );
			block->forEachData().statement = _astNodeWalk ( block->forEachData().statement, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btIf:
			for ( auto &it : block->ifData().ifList )
			{
				it->condition = _astNodeWalk ( it->condition, block, sym, cb, true, std::forward<T> ( t )... );
				it->block = _astNodeWalk ( it->block, block, sym, cb, true, std::forward<T> ( t )... );
			}
			block->ifData().elseBlock = _astNodeWalk ( block->ifData().elseBlock, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btWhile:
			block->loopData().condition = _astNodeWalk ( block->loopData().condition, block, sym, cb, true, std::forward<T> ( t )... );
			block->loopData().block = _astNodeWalk ( block->loopData().block, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btRepeat:
			block->loopData().block = _astNodeWalk ( block->loopData().block, block, sym, cb, true, std::forward<T> ( t )... );
			block->loopData().condition = _astNodeWalk ( block->loopData().condition, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::btSwitch:
			assert ( block->switchData().condition );
			block->switchData().condition = _astNodeWalk ( block->switchData().condition, block, sym, cb, true, std::forward<T> ( t )... );
			assert ( block->switchData().condition );
			for ( auto &it : block->switchData().caseList )
			{
				it->condition = _astNodeWalk ( it->condition, block, sym, cb, true, std::forward<T> ( t )... );
				it->block = _astNodeWalk ( it->block, block, sym, cb, true, std::forward<T> ( t )... );
			}
			break;
		case astOp::conditional:
			block->conditionData().trueCond = _astNodeWalk ( block->conditionData().trueCond, block, sym, cb, true, std::forward<T> ( t )... );
			block->conditionData().falseCond = _astNodeWalk ( block->conditionData().falseCond, block, sym, cb, true, std::forward<T> ( t )... );
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto &it : block->pList().param )
			{
				it = _astNodeWalk ( it, block, sym, cb, true, std::forward<T> ( t )... );
			}
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto &it : block->arrayData().nodes )
			{
				it = _astNodeWalk ( it, block, sym, cb, true, std::forward<T> ( t )... );
			}
			break;
		case astOp::btBreak:
		case astOp::btLabel:
		case astOp::btSlide:
		case astOp::btGoto:
		case astOp::btCont:
		default:
			break;
	}
	if ( block->left ) block->left = _astNodeWalk ( block->left, block, sym, cb, true, std::forward<T> ( t )... );
	if ( block->right ) block->right = _astNodeWalk ( block->right, block, sym, cb, true, std::forward<T> ( t )... );
	return block;
}

template <typename F, typename ...T>
astNode *astNodeWalk ( astNode *block, symbolStack *sym, F *cb, T &&... t )
{
	return _astNodeWalk ( block, 0, sym, cb, true, std::forward<T> ( t )... );
}

