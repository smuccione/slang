#include "compilerBCGenerator.h"
#include "compilerOptimizer.h"
#include "transform.h"

void optimizer::simpleTransformBlock (astNode *block, symbolStack *sym )
{
	if ( block->left ) simpleTransformBlock ( block->left, sym );
	if ( block->right ) simpleTransformBlock ( block->right, sym );

	switch ( block->getOp() )
	{
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					simpleTransformBlock ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) ), sym );
					sym->push ( block->symbolDef() );
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							simpleTransformBlock ( (*symbol)[it]->initializer, sym );
						}
						sym->push ( block->symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass*>(block->symbolDef());
						sym->insert( block->symbolDef(), cls->insertPos );
					}
					break;
				default:
					sym->push ( block->symbolDef() );
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym->size ( );
				for ( auto it = block->basicData().blocks.begin ( ); it != block->basicData().blocks.end ( ); it++ )
				{
					if ( (*it) )
					{
						simpleTransformBlock ( (*it), sym );
					}
				}
				sym->resize ( size );
			}
			break;
		case astOp::btInline:
			if ( block->inlineFunc().node ) block->inlineFunc().node = transform->transform ( block->inlineFunc().node, sym, &transformMade );
			break;
		case astOp::btLabel:
		case astOp::btGoto:
		case astOp::btBreak:
		case astOp::btCont:
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( block->returnData().node )
			{
				block->returnData().node = transform->transform ( block->returnData().node, sym, &transformMade );
			}
			break;
		case astOp::btTry:
			if ( block->tryCatchData().tryBlock ) simpleTransformBlock ( block->tryCatchData().tryBlock, sym );
			if ( block->tryCatchData().catchBlock ) simpleTransformBlock ( block->tryCatchData().catchBlock, sym );
			if ( block->tryCatchData().finallyBlock ) simpleTransformBlock ( block->tryCatchData().finallyBlock, sym );
			break;
		case astOp::btFor:
			if ( block->loopData().initializers ) block->loopData().initializers = transform->transform ( block->loopData().initializers, sym, &transformMade );
			if ( block->loopData().condition ) block->loopData().condition = transform->transform ( block->loopData().condition, sym, &transformMade );
			if ( block->loopData().block ) simpleTransformBlock ( block->loopData().block, sym );
			if ( block->loopData().increase ) block->loopData().increase = transform->transform ( block->loopData().increase, sym, &transformMade );
			break;
		case astOp::btForEach:
			if ( block->forEachData().in) block->forEachData().in = transform->transform ( block->forEachData().in, sym, &transformMade );
			if ( block->forEachData().statement ) simpleTransformBlock ( block->forEachData().statement, sym );
			break;
		case astOp::btIf:
			for ( auto it = block->ifData().ifList.begin(); it != block->ifData().ifList.end(); it++ )
			{
				(*it)->condition = transform->transform ( (*it)->condition, sym, &transformMade );
				if ( (*it)->block ) simpleTransformBlock ( (*it)->block, sym );
			}
			if ( block->ifData().elseBlock ) simpleTransformBlock ( block->ifData().elseBlock, sym );
			break;
		case astOp::btWhile:
			if ( block->loopData().condition ) block->loopData().condition = transform->transform ( block->loopData().condition, sym, &transformMade );
			if ( block->loopData().block ) simpleTransformBlock ( block->loopData().block, sym );
			break;
		case astOp::btRepeat:
			simpleTransformBlock ( block->loopData().block, sym );
			if ( block->loopData().condition ) block->loopData().condition = transform->transform ( block->loopData().condition, sym, &transformMade );
			break;
		case astOp::btSwitch:
			std::vector<astCondBlock *>::size_type	loop;

			block->switchData().condition = transform->transform ( block->switchData().condition, sym, &transformMade );
			assert ( block->switchData().condition );
			for ( loop = 0; loop < block->switchData().caseList.size(); loop++ )
			{
				if ( block->switchData().caseList[loop]->condition ) block->switchData().caseList[loop]->condition = transform->transform ( block->switchData().caseList[loop]->condition, sym, &transformMade );
				if ( block->switchData().caseList[loop]->block ) simpleTransformBlock ( block->switchData().caseList[loop]->block, sym );
			}
			break;
		case astOp::conditional:
			block->conditionData().trueCond = transform->transform ( block->conditionData().trueCond, sym, &transformMade );
			block->conditionData().falseCond= transform->transform ( block->conditionData().falseCond, sym, &transformMade );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( loop = 0; loop < block->arrayData().nodes.size(); loop++ )
			{
				block->arrayData().nodes[loop] = transform->transform ( block->arrayData().nodes[loop], sym, &transformMade );
			}
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( loop = 0; loop < block->pList().param.size(); loop++ )
			{
				block->pList().param[loop] = transform->transform ( block->pList().param[loop], sym, &transformMade );
			}
			break;
		default:
			*block = *transform->transform ( block, sym, &transformMade );
			break;
	}
}

void optimizer::simpleTransformFunction ( opFunction *func )
{
	if ( func->isInterface || !func->codeBlock ) return;

	symbolStack sym ( file, func );
	simpleTransformBlock ( func->codeBlock, &sym );

	if ( func->params.size () )
	{
		for ( size_t it = 0; it < func->params.size (); it++ )
		{
			auto param = func->params[it];
			if ( param->initializer && param->initializer->getOp () == astOp::assign )
			{
				simpleTransformBlock ( param->initializer->right, &sym );
			}
		}
	}

}


void optimizer::simpleTransform ()
{
	for ( auto &it : file->symbols )
	{
		if ( it.second.loadTimeInitializable )
		{
			auto init = it.second.initializer;
			if ( init )
			{
				symbolStack sym ( file );
				if ( init->getOp ( ) == astOp::assign )
				{
					simpleTransformBlock (init->right, &sym );
				} else
				{
					simpleTransformBlock ( init, &sym );
				}
			}
		}
	}

	// compile the functions
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		errorLocality e ( &file->errHandler, (*it).second->location );
		simpleTransformFunction ( (*it).second );
	}
}

