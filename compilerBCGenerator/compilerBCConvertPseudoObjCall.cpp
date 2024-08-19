/*


	block processor


*/

#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"

static astNode *convertPseudoObjCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, bool isLS )
{
	switch ( node->getOp() )
	{
		case astOp::funcCall:
			if ( parent && parent->getOp ( ) == astOp::btBasic )
			{
				if ( node->left->getOp ( ) == astOp::atomValue && sym->isDefined ( node->left->symbolValue(), true ) )
				{
					if ( sym->isConst ( node->left->symbolValue(), true ) )
					{
						errorLocality e ( &sym->file->errHandler, node );
						if ( isLS )
						{
							sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUSELESS_FUNCTION_CALL, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->symbolValue ().c_str () ) ) );
							sym->file->warnings.back ().get ()->setLocation ( node->left );
						} else
						{
							sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->symbolValue ().c_str () );
						}
					}
				} else if ( node->left->getOp ( ) == astOp::sendMsg )
				{
					if ( !node->left->left->getType ( sym ).isAnObject ( ) )
					{
						// pseudo object calls
						if ( (node->left->right->getOp () == astOp::nameValue) && sym->isDefined ( node->left->right->nameValue (), true ) )
						{
							if ( sym->isConst ( node->left->left->nameValue (), true ) )
							{
								errorLocality e ( &sym->file->errHandler, node );
								if ( isLS )
								{
									sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUSELESS_FUNCTION_CALL, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->left->nameValue ().c_str () ) ) );
									sym->file->warnings.back ().get ()->setLocation ( node->left->left );
								} else
								{
									sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->left->nameValue ().c_str () );
								}
							}
						}
					}
				}
			}
			break;
		case astOp::sendMsg:
			if ( parent && parent->getOp ( ) == astOp::btBasic )
			{
				if ( node->left->getOp ( ) == astOp::atomValue )
				{
					// we're here if we're a pseudo object call (left is a function)
					if ( sym->isConst ( node->left->symbolValue(), true ) )
					{
						errorLocality e ( &sym->file->errHandler, node );
						if ( isLS )
						{
							sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUSELESS_FUNCTION_CALL, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->symbolValue ().c_str () ) ) );
							sym->file->warnings.back ().get ()->setLocation ( node->left );
						} else
						{
							sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->symbolValue ().c_str () );
						}
					}
				} else if ( node->left->getOp ( ) == astOp::sendMsg )
				{
					if ( !node->left->left->getType ( sym ).isAnObject ( ) )
					{
						// pseudo object calls
						if ( ((node->left->right->getOp ( ) == astOp::nameValue) || (node->left->right->getOp ( ) == astOp::atomValue)) && sym->isDefined ( node->left->right->nameValue(), true ) )
						{
							if ( sym->isConst ( node->left->left->symbolValue(), true ) )
							{
								errorLocality e ( &sym->file->errHandler, node );
								if ( isLS )
								{
									sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUSELESS_FUNCTION_CALL, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->left->nameValue ().c_str () ) ) );
									sym->file->warnings.back ().get ()->setLocation ( node->left->left );
								} else
								{
									sym->file->errHandler.throwWarning ( isLS, warnNum::scwUSELESS_FUNCTION_CALL, node->left->left->nameValue ().c_str () );
								}
							}
						}
					}
				}
			}
			break;
		default:
			break;
	}
	return node;
}

void compExecutable::convertPsuedoObjCall ( opFunction *func, bool isLS )
{
	symbolStack				sym ( file, func );
	errorLocality e ( &file->errHandler, func->location );

	if ( func->codeBlock )
	{
		astNodeWalk ( func->codeBlock, &sym, convertPseudoObjCB, isLS );
	}
}

void compExecutable::convertPsuedoObjCall ( bool isLS )
{
	for ( auto &it : file->symbols )
	{
		if ( it.second.loadTimeInitializable )
		{
			auto init = it.second.initializer;
			if ( init )
			{
				symbolStack				sym ( file );
				errorLocality e ( &file->errHandler, init );

				if ( init->getOp ( ) == astOp::assign )
				{
					astNodeWalk ( init->right, &sym, convertPseudoObjCB, isLS );
				} else
				{
					astNodeWalk ( init , &sym, convertPseudoObjCB, isLS );
				}
			}
		}
	}

	for (auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		convertPsuedoObjCall ( (*it).second, isLS );
	}
}
