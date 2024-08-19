/*

	block processor


*/
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "Utility/counter.h"

/********************************************************

foreach ( x in y )
{
	...
}

is converted into:

{
	local iter = y.getEnumerator() )
	while ( iter.MoveNext() )
	{
		local x = iter.GetCurrent();
		...
	}
}
*********************************************************/

static astNode *makeForEachCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, opFunction *func, bool isLS )
{
	switch ( node->getOp() )
	{
		case astOp::btForEach:
			if ( node->forEachData().in->getOp() == astOp::range &&
				 node->forEachData ().in->left && node->forEachData().in->left->getType( sym ) == symbolType::symInt && 
				 node->forEachData ().in->right && node->forEachData().in->right->getType ( sym ) == symbolType::symInt )
			{
				astNode	*basic = new astNode ( sym->file->sCache, astOp::btBasic );
				astNode *initializer = new astNode ( sym->file->sCache, astOp::assign ); //)->setLocation ( node->forEachData ().in );
				initializer->left = (new astNode ( sym->file->sCache, astOp::symbolValue, node->forEachData().var->symbolValue() ))->setLocation ( node->forEachData ().var );
				initializer->right = new astNode ( *node->forEachData ().in->left );

				basic->addSymbol ( node->forEachData().var->symbolValue(), node->forEachData().in->left->getType( sym ), *initializer->left, initializer );

				astNode *increase = new astNode ( sym->file->sCache, astOp::preInc );
				increase->left = new astNode ( sym->file->sCache, astOp::symbolValue, node->forEachData ().var->symbolValue () );

				astNode *condition = new astNode ( sym->file->sCache, astOp::lessEqual );
				condition->left = new astNode ( sym->file->sCache, astOp::symbolValue, node->forEachData ().var->symbolValue () );
				condition->right = new astNode ( *node->forEachData ().in->right );

				astNode *f = (new astNode ( sym->file->sCache, astOp::btFor ))->setLocation ( node );

				f->loopData ().block = node->forEachData ().statement;
				f->loopData().increase = increase;
				f->loopData().condition = condition;
				f->loopData().initializers = nullptr;

				node->forEachData ().statement = nullptr;

				basic->pushNode ( f );
				if ( isLS )
				{
					basic->pushNode ( (new astNode ( sym->file->sCache, astOp::range ))->setLocation ( node->forEachData ().in ) );
				}
				delete node;
				return  basic;
			} else if ( node->forEachData ().in->getOp () == astOp::range && !node->forEachData ().in->left && !node->forEachData ().in->right )
			{
				// do nothing... this is a LS condition
			} else
			{
/*
									=
							$iter				
											getEnumerator
										in		

*/
				char tmpName[65];
				sprintf_s ( tmpName, sizeof ( tmpName ), "$iter:%I64u", GetUniqueCounter() );

				auto iteratorVarName = sym->file->sCache.get ( tmpName );
				auto enumBlock		= new astNode ( sym->file->sCache, astOp::assign );
				enumBlock->left		= new astNode ( sym->file->sCache, astOp::symbolValue, iteratorVarName );
				enumBlock->right	= new astNode ( sym->file->sCache, astOp::getEnumerator, node->forEachData ().in );

				astNode	*basic = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( node );

				basic->addSymbol ( iteratorVarName, symbolType::symUnknown, *enumBlock, enumBlock );

				astNode *whileBlock = (new astNode ( sym->file->sCache, astOp::btWhile ))->setLocation ( node );
				whileBlock->loopData ().block = new astNode ( sym->file->sCache, astOp::btBasic );

				basic->pushNode ( whileBlock );

				astNode *symBlock;
				switch ( node->forEachData().var->getOp() )
				{
					case astOp::symbolValue:
						/* current only */
						symBlock = new astNode ( sym->file->sCache, astOp::assign );
						symBlock->left			= new astNode ( *node->forEachData().var );
						symBlock->right			= new astNode ( sym->file->sCache, astOp::sendMsg );
						symBlock->right->left	= new astNode ( sym->file->sCache, astOp::symbolValue, iteratorVarName );
						symBlock->right->right	= new astNode ( sym->file->sCache, astOp::nameValue, "current" );

						whileBlock->loopData ().block->addSymbol ( node->forEachData().var->symbolValue(), symbolType::symUnknown, *node->forEachData ().var, symBlock );
						break;
					case astOp::seq:
						/* current */
						symBlock = (new astNode ( sym->file->sCache, astOp::assign ))->setLocation ( node->forEachData ().var );
						symBlock->left			= new astNode ( *node->forEachData().var->left );
						symBlock->right			= new astNode ( sym->file->sCache, astOp::sendMsg );
						symBlock->right->left	= new astNode ( sym->file->sCache, astOp::symbolValue, iteratorVarName );
						symBlock->right->right	= new astNode ( sym->file->sCache, astOp::nameValue, "current" );

						whileBlock->loopData ().block->addSymbol ( node->forEachData().var->left->symbolValue(), symbolType::symUnknown, *node->forEachData ().var->left, symBlock );

						/* index */
						symBlock = new astNode ( sym->file->sCache, astOp::assign );
						symBlock->left			= new astNode ( *node->forEachData().var->right );
						symBlock->right			= new astNode ( sym->file->sCache, astOp::sendMsg );
						symBlock->right->left	= new astNode ( sym->file->sCache, astOp::symbolValue, iteratorVarName );
						symBlock->right->right	= new astNode ( sym->file->sCache, astOp::nameValue, "index" );

						whileBlock->loopData ().block->addSymbol ( node->forEachData().var->right->symbolValue(), symbolType::symUnknown, *node->forEachData ().var->right, symBlock );
						break;
					case astOp::arrayValue:
						// convert the arrayValue into a symbol pack.
						node->forEachData ().var->forceOp ( astOp::symbolPack );

						// current only, assignment to a symbol pack
						symBlock = new astNode ( sym->file->sCache, astOp::assign );
						symBlock->left			= new astNode ( *node->forEachData ().var );
						symBlock->right			= new astNode ( sym->file->sCache, astOp::sendMsg );
						symBlock->right->left	= new astNode ( sym->file->sCache, astOp::symbolValue, iteratorVarName );
						symBlock->right->right	= new astNode ( sym->file->sCache, astOp::nameValue, "current" );

						whileBlock->loopData ().block->addSymbol ( node->forEachData ().var->symbolValue (), symbolType::symUnknown, *node->forEachData ().var, symBlock );
						break;
					default:
						if ( !isLS ) throw errorNum::scINVALID_ITERATOR;
						enumBlock->right = nullptr;					// otherwise we'll delete it as we've already passed on ownership
						delete basic;
						return node;
						
				}
				whileBlock->loopData().block->pushNode ( node->forEachData().statement );
				whileBlock->loopData ().condition				= new astNode ( sym->file->sCache, astOp::funcCall );
				whileBlock->loopData ().condition->left			= new astNode ( sym->file->sCache, astOp::sendMsg );
				whileBlock->loopData ().condition->left->left	= new astNode ( sym->file->sCache, astOp::symbolValue, iteratorVarName );
				whileBlock->loopData ().condition->left->right	= new astNode ( sym->file->sCache, astOp::nameValue, "MoveNext" );

				delete node->forEachData().var;
				node->forEachData ().var = nullptr;
				node->forEachData ().in = nullptr;
				node->forEachData ().statement = nullptr;
				delete node;
				return basic;
			}
			break;
		default:
			break;
	}
	return node;
}

void compExecutable::compMakeForEach ( opFunction *func, bool isLS )
{
	if ( func->codeBlock )
	{
		symbolStack sym( file );
		astNodeWalk ( func->codeBlock, &sym, makeForEachCB, func, isLS );
	}
}

void compExecutable::compMakeForEach ( int64_t sourceIndex, bool isLS )
{
	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		if ( !sourceIndex || (it->second->location.sourceIndex == sourceIndex) )
		{
			try
			{
				errorLocality e ( &file->errHandler, (*it).second->location );
				compMakeForEach ( (*it).second, isLS );
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( isLS, err );
			}
		}
	}
}

