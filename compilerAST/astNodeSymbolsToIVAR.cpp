/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

// note: because of the recursive call with prune, it makes it very, difficult to use this use node-walk since the callback itself doesn't recurse.

static astNode *symbolsToIvar ( astNode *node, cacheString const &name, symbolStack &sym, bool prepend )
{
	size_t				 loop;

	switch ( node->getOp() )
	{
		case astOp::symbolValue:
			if ( prepend )
			{
				if ( sym.getScope ( node->symbolValue(), true ) == symbolSpaceScope::symLocalParam )
				{
					// need to modify this... we have symbol referencing a parameter... need to change it to an ivar access through symbol 'name'
					astNode *newNode = (new astNode ( sym.file->sCache, astOp::sendMsg, (new astNode ( sym.file->sCache, astOp::symbolValue, name ) )->setLocation ( node ), node ))->setLocation ( node );
					*newNode->right = *astNode ( sym.file->sCache, astOp::nameValue, newNode->right->symbolValue() ).setLocation ( newNode->right ); // newNode->right is node
					return newNode;
				}
			}
			break;
		case astOp::cSend:
		case astOp::sendMsg:
			if ( node->left )  node->left  = symbolsToIvar ( node->left, name, sym, prepend );
			if ( node->right ) node->right = symbolsToIvar ( node->right, name, sym, false );
			break;
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					node->symbolDef()->setInitializer ( node->symbolDef()->getSymbolName ( ), symbolsToIvar ( node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName ( ) ), name, sym, true ) );
					sym.push ( node->symbolDef() );
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(node->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							(*symbol)[it]->initializer = symbolsToIvar ( (*symbol)[it]->initializer, name, sym, true );
						}
					}
					sym.push ( node->symbolDef() );
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass*>(node->symbolDef());
						sym.insert( node->symbolDef(), cls->insertPos );
					}
					break;
				default:
					sym.push ( node->symbolDef() );
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym.size ( );
				decltype(node->basicData().blocks) newBlockList;

				for ( auto it = node->basicData().blocks.begin(); it != node->basicData().blocks.end(); it++ )
				{
					newBlockList.push_back ( symbolsToIvar ( (*it), name, sym, true ) );
				}
				node->basicData().blocks.swap ( newBlockList );
				sym.resize ( size );
			}
			break;
		case astOp::btSwitch:
			if ( node->switchData().condition ) node->switchData().condition = symbolsToIvar ( node->switchData().condition, name, sym, true );
			assert ( node->switchData().condition );
			for ( loop = 0; loop < node->switchData().caseList.size(); loop++ )
			{
				if ( node->switchData().caseList[loop]->condition ) node->switchData().caseList[loop]->condition = symbolsToIvar ( node->switchData().caseList[loop]->condition, name, sym, true );
				if ( node->switchData().caseList[loop]->block ) node->switchData().caseList[loop]->block = symbolsToIvar ( node->switchData().caseList[loop]->block, name, sym, true );
			}
			break;
		case astOp::btIf:
			for ( loop = 0; loop < node->ifData().ifList.size(); loop++ )
			{
				node->ifData().ifList[loop]->condition = node->ifData().ifList[loop]->condition = symbolsToIvar ( node->ifData().ifList[loop]->condition, name, sym, true );
				node->ifData().ifList[loop]->block = node->ifData().ifList[loop]->block = symbolsToIvar ( node->ifData().ifList[loop]->block, name, sym, true );
			}
			if ( node->ifData().elseBlock ) node->ifData().elseBlock = node->ifData().elseBlock = symbolsToIvar ( node->ifData().elseBlock, name, sym, true );
			break;
		case astOp::btInline:
			if ( node->inlineFunc().node ) node->inlineFunc().node = node->inlineFunc().node = symbolsToIvar ( node, name, sym, true );
			break;
		case astOp::btFor:
			if ( node->loopData().initializers ) node->loopData().initializers =  symbolsToIvar ( node->loopData().initializers, name, sym, true );
			if ( node->loopData().condition ) node->loopData().condition =  symbolsToIvar ( node->loopData().condition, name, sym, true );
			if ( node->loopData().increase ) node->loopData().increase =  symbolsToIvar ( node->loopData().increase, name, sym, true );
			if ( node->loopData().block ) node->loopData().block = symbolsToIvar ( node->loopData().block, name, sym, true );
			break;
		case astOp::btForEach:
			if ( node->forEachData().var ) node->forEachData().var =  symbolsToIvar ( node->forEachData().var, name, sym, true );
			if ( node->forEachData().in ) node->forEachData().in =  symbolsToIvar ( node->forEachData().in, name, sym, true );
			if ( node->forEachData().statement ) node->forEachData().statement = symbolsToIvar ( node->forEachData().statement, name, sym, true );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			if ( node->loopData().block ) node->loopData().block = symbolsToIvar ( node->loopData().block, name, sym, true );
			if ( node->loopData().condition ) node->loopData().condition =  symbolsToIvar ( node->loopData().condition, name, sym, true );
			break;
		case astOp::btTry:
			if ( node->tryCatchData().catchBlock ) node->tryCatchData().catchBlock = symbolsToIvar ( node->tryCatchData().catchBlock, name, sym, true );
			if ( node->tryCatchData().finallyBlock ) node->tryCatchData().finallyBlock = symbolsToIvar ( node->tryCatchData().finallyBlock, name, sym, true );
			if ( node->tryCatchData().tryBlock ) node->tryCatchData().tryBlock = symbolsToIvar ( node->tryCatchData().tryBlock, name, sym, true );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( node->returnData().node ) node->returnData().node =  symbolsToIvar ( node->returnData().node, name, sym, false );
			break;
		case astOp::refCreate:
			break;
		case astOp::conditional:
			node->conditionData().trueCond = symbolsToIvar ( node->conditionData().trueCond, name, sym, true );
			node->conditionData().falseCond = symbolsToIvar ( node->conditionData().falseCond, name, sym, true );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( loop = 0; loop < node->arrayData().nodes.size(); loop++ )
			{
				node->arrayData().nodes[loop] = symbolsToIvar ( node->arrayData().nodes[loop], name, sym, false );
			}
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			node->left = symbolsToIvar ( node->left, name, sym, true );
			for ( loop = 0; loop < node->pList().param.size(); loop++ )
			{
				if ( node->pList().param[loop] ) node->pList().param[loop] = symbolsToIvar ( node->pList().param[loop], name, sym, true );
			}
			break;
		default:
			if ( node->left )  node->left  = symbolsToIvar ( node->left, name, sym, true );
			if ( node->right ) node->right = symbolsToIvar ( node->right, name, sym, true );
			break;
	}
	return node;
}

astNode	*astNode::symbolsToIVar ( cacheString const &name, class symbolStack &sym, bool prepend )
{
	return ::symbolsToIvar ( this, name, sym, prepend );
}
