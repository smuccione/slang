/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

static astNode *makeAtomsCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool isInFunctionCall, class errorHandler *err, bool isLS, opFunction *func )
{
	errorLocality e ( err, node );

	switch ( node->getOp() )
	{
		case astOp::symbolValue:
			if ( sym->isDefinedNS ( node->symbolValue(), isAccess ) )
			{
				node->symbolValue () = sym->getResolvedName ( node->symbolValue (), isAccess );
				if ( sym->isFunc ( node->symbolValue(), isAccess ) && !sym->isProperty ( node->symbolValue(), isAccess ) )
				{
					node->forceOp ( astOp::atomValue );
				}
				switch ( sym->getScope ( node->symbolValue(), isAccess ) )
				{
					case symbolSpaceScope::symFunction:
					case symbolSpaceScope::symClassMethod:
						node->forceOp ( astOp::atomValue );
						break;
					default:
						break;
				}
			}
			break;
		case astOp::funcCall:
			if ( !node->left ) throw errorNum::scINTERNAL;
			if ( node->left->getOp() == astOp::symbolValue )
			{
				if ( sym->isDefinedNS ( node->left->symbolValue (), true ) )
				{
					node->left->symbolValue () = sym->getResolvedName ( node->left->symbolValue (), isAccess );
					switch ( sym->getScope ( node->left->symbolValue (), true ) )
					{
						case symbolSpaceScope::symFunction:
						case symbolSpaceScope::symClassMethod:
							if ( func )
							{
								if ( node->left->symbolValue () == sym->file->typeValue ||
									 node->left->symbolValue () == sym->file->typeXValue ||
									 node->left->symbolValue () == sym->file->typeOfValue
									 )
								{
									func->params.forceStrongVariant ();
								}
							}
							node->left->forceOp ( astOp::atomValue );
							if ( node->left->symbolValue () == sym->file->newValue )
							{
								if ( node->pList ().param.size () )
								{
									if ( node->pList().param[0]->getOp() == astOp::stringValue )
									{
										*node->pList().param[0] = *astNode ( sym->file->sCache, astOp::nameValue, node->pList().param[0]->stringValue().c_str () ).setLocation ( node->pList().param[0] );
									}
								}
							}
							break;
						case symbolSpaceScope::symClass:
							node->pList().param.insert ( node->pList().param.begin ( ), node->left );
							node->pList ().paramRegion.insert ( node->pList ().paramRegion.begin (), *node->left );
							node->left = new astNode ( sym->file->sCache, astOp::atomValue, "new" );
							*node->pList().param[0] = *astNode ( sym->file->sCache, astOp::nameValue, node->pList().param[0]->symbolValue() ).setLocation ( node->pList().param[0] );
							break;
						default:
							break;
					}
				} else 
				{
					errorLocality e ( &sym->file->errHandler, node->left );
					node->left->forceOp(astOp::atomValue);
					if ( isLS )
					{
						sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNDEFINED_FUNCTION, err->throwWarning ( isLS, warnNum::scwUNDEFINED_FUNCTION, node->left->symbolValue ().c_str () ) ) );
						sym->file->warnings.back ().get ()->setLocation ( node->left );
						
						// we NEED some function going forward to operate on... or we have to do a lot of extra crap...
						// the solution to allow the other modules to keep working when in LS mode is to simply create a dummy function to act as a place holder for missing functions
						opFunction *newFunc = new opFunction ( sym->file );
						if ( !memcmp( node->left->symbolValue(), "::", 2 ) )
						{
							node->left->symbolValue() = sym->file->sCache.get( node->left->symbolValue().c_str() + 2 );
						}
						newFunc->name = node->left->symbolValue ();
						newFunc->location = node->left->location;
						newFunc->nameLocation = node->left->location;
						newFunc->isVariantParam = true;					// mark it variant so we don't have extraneous warnings about extra parameters for someting we don't even know about
						
						sym->file->functionList.insert( { newFunc->name, newFunc } );
					} else
					{
						err->throwWarning ( isLS, warnNum::scwUNDEFINED_FUNCTION, node->left->symbolValue ().c_str () );
					}
				}
			}
			break;
		case astOp::workAreaStart:
			if ( !node->left ) throw errorNum::scINTERNAL;
			if ( !node->right ) throw errorNum::scINTERNAL;
			if ( node->left->getOp() == astOp::symbolValue )
			{
				if ( node->left->symbolValue() == sym->file->fieldValue )
				{
					if ( node->right->getOp() != astOp::symbolValue )
					{
						errorLocality e ( &sym->file->errHandler, node->right );
						if ( !isLS ) throw errorNum::scINVALID_FIELD;
						sym->file->errHandler.throwFatalError ( isLS, errorNum::scINVALID_FIELD );
						break;
					}
					auto symbol = node->right->symbolValue();
					*node = astNode ( sym->file->sCache, astOp::fieldValue, symbol );
				} else
				{
					// our symbol is really a string constant (the workarea alias)
					*node->left = astNode ( sym->file->sCache, astOp::stringValue, node->left->symbolValue ().c_str () );
				}
			}
			break;
		default:
			break;
	}
	return node;
}

void astNode::makeAtoms ( symbolStack *sym, class errorHandler *err, bool isLS, opFunction *func )
{
	try {
		astNodeWalk ( this, sym, makeAtomsCB, err, isLS, func );
	} catch ( errorNum e )
	{
		err->throwFatalError ( isLS, e );
	}
}

static astNode *makeAtomsCodeBlockCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool isInFunctionCall, class errorHandler *err )
{
	errorLocality e ( err, node );

	switch ( node->getOp() )
	{
		case astOp::symbolValue:
			break;
		case astOp::funcCall:
			if ( !node->left ) throw errorNum::scINTERNAL;
			if ( node->left->getOp() == astOp::symbolValue )
			{
				// for code-blocks we don't hard-link (otherwise we can't save the resulting code off for another time.   so all functions become runtime string-calls
				*node->left = *astNode ( sym->file->sCache, astOp::stringValue, node->left->symbolValue ().c_str () ).setLocation ( node->left );
			}
			break;
		case astOp::workAreaStart:
			if ( !node->left ) throw errorNum::scINTERNAL;
			if ( !node->right ) throw errorNum::scINTERNAL;
			if ( node->left->getOp() == astOp::symbolValue )
			{
				if ( node->left->symbolValue() == sym->file->fieldValue )
				{
					if ( node->right->getOp() != astOp::symbolValue )
					{
						errorLocality e ( &sym->file->errHandler, node->right );
						throw errorNum::scINVALID_FIELD;
					}
					auto symbol = node->right->symbolValue();
					node->release ( );
					*node = astNode ( sym->file->sCache, astOp::fieldValue, symbol );
				} else
				{
					// our symbol is really a string constant (the workarea alias)
					node->left->forceOp ( astOp::symbolValue );
				}
			}
			break;
		case astOp::cSend:
		case astOp::sendMsg:
			if ( node->right->getOp() == astOp::atomValue )
			{
				node->right->forceOp ( astOp::symbolValue );
			}
			if ( node->right->getOp() == astOp::symbolValue )
			{
				node->right->forceOp ( astOp::symbolValue );
			}
			break;
		default:
			break;
	}
	return node;
}

void astNode::makeAtomsCodeBlock ( symbolStack *sym, class errorHandler *err )
{
	try {
		astNodeWalk ( this, sym, makeAtomsCodeBlockCB, err );
	} catch ( errorNum e )
	{
		err->throwFatalError ( false, e );
	}
}
