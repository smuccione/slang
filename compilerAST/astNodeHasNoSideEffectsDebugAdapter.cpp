/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static bool hasNoSideEffects ( symbolStack *sym, astNode *node, bool allowArrayAccess )
{
	if ( !node ) return true;

	switch ( node->getOp () )
	{
		case astOp::sendMsg:
			// for debugAdapter generation we'll NEVER know the type here... it'll alwasy be a variant so the best we can do is safe checking at compile time.
			return true;

		case astOp::funcValue:
			{
				auto lFunc = sym->file->functionList.find( node->symbolValue() )->second;
				if ( !lFunc->isConst )
				{
					return false;
				}
			}
			break;
		case astOp::symbolValue:
			// we'll never know the symbol type... so best we can do is true;
			return true;
		case astOp::btBasic:
			{
				auto size = sym->size ();
				for ( auto it = node->basicData().blocks.begin (); it != node->basicData().blocks.end (); it++ )
				{
					if ( !hasNoSideEffects ( sym, (*it), allowArrayAccess ) )
					{
						sym->resize ( size );
						return false;
					}
				}
				sym->resize ( size );
				if ( !node->basicData().blocks.size () ) return false;
			}
			break;
		case astOp::symbolDef:
		case astOp::btInline:
		case astOp::btLabel:
		case astOp::btInlineRet:
		case astOp::btGoto:
		case astOp::btReturn:
		case astOp::btBreak:
		case astOp::btCont:
		case astOp::btTry:
		case astOp::btForEach:
		case astOp::btFor:
		case astOp::btIf:
		case astOp::btWhile:
		case astOp::btRepeat:
		case astOp::btSwitch:
			// should never get here... we're in a debug adapter so no inlining and/or blocks
			assert ( false );
			break;
		case astOp::conditional:
			if ( !hasNoSideEffects ( sym, node->conditionData().trueCond, allowArrayAccess ) ) return false;
			if ( !hasNoSideEffects ( sym, node->conditionData().falseCond, allowArrayAccess ) ) return false;
			break;
		case astOp::funcCall:
			for ( auto it = node->pList().param.begin (); it != node->pList().param.end (); it++ )
			{
				if ( (*it) )
				{
					if ( (*it)->getOp () == astOp::refCreate )
					{
						return false;
					}
					if ( (*it) ) if ( !hasNoSideEffects ( sym, (*it), allowArrayAccess ) ) return false;
				}
			}
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
			for ( auto it = node->arrayData().nodes.begin (); it != node->arrayData().nodes.end (); it++ )
			{
				if ( !hasNoSideEffects ( sym, (*it), allowArrayAccess ) ) return false;
			}
			break;
		case astOp::range:
		case astOp::refCreate:
		case astOp::refPromote:
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
		case astOp::postDec:
		case astOp::postInc:
		case astOp::preDec:
		case astOp::preInc:
		case astOp::comp:
		case astOp::symbolPack:
		case astOp::getEnumerator:
			// TODO: add support for symbolPack
			return false;
		case astOp::arrayDeref: 			// may expand through a NULL so we have to assume side-effect here
		case astOp::cArrayDeref: 			// may expand through a NULL so we have to assume side-effect here
			if ( node->left->getCompType ( sym ) != symbolType::symArray ) return false;
			for ( auto &it : node->pList ().param )
			{
				if ( !hasNoSideEffects ( sym, it, allowArrayAccess ) ) return false;
			}
			break;
		default:
			// can't do much more... everything are variants so we can't determin at compile time if the variant is an object with an overloaded operator
			break;
	}
	// just put all of these here rather than break them out for all the other searches
	if ( node->left )
	{
		if ( !hasNoSideEffects ( sym, node->left, allowArrayAccess ) ) return false;
	}
	if ( node->right )
	{
		if ( !hasNoSideEffects ( sym, node->right, allowArrayAccess ) ) return false;
	}
	return true;
}

bool astNode::hasNoSideEffectsDebugAdapter ( symbolStack *sym, bool allowArrayAccess )
{
	symbolStack localSym = *sym;
	auto ret = ::hasNoSideEffects ( &localSym, this, allowArrayAccess );
	return ret;
}
