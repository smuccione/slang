/*

	block processor


*/
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"

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

static astNode *genErrorsWarningsCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, opFunction *func, bool isLS )
{
	switch ( node->getOp () )
	{
		case astOp::btBasic:
			{
				bool doEmit = true;
				bool unreachableEmitted = false;
				for ( auto &it : node->basicData ().blocks )
				{
					if ( it )
					{
						switch ( it->getOp () )
						{
							case astOp::btReturn:
								if ( !doEmit && !unreachableEmitted )
								{
									errorLocality e ( &sym->file->errHandler, it );
									if ( isLS )
									{
										srcLocation	callLoc = it->location;
										it->adjustLocation ( callLoc );

										sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNREACHABLE_CODE, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNREACHABLE_CODE ) ) );
										sym->file->warnings.back ().get ()->setLocation ( callLoc );
									} else
									{
										sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNREACHABLE_CODE );
									}
									unreachableEmitted = true;
								}
								if ( !it->returnData ().isYield )
								{
									doEmit = false;
								}
								break;
							case astOp::btGoto:
							case astOp::btBreak:
							case astOp::btCont:
							case astOp::btExit:
								if ( !doEmit && !unreachableEmitted )
								{
									errorLocality e ( &sym->file->errHandler, it );
									if ( isLS )
									{
										srcLocation	callLoc = it->location;
										it->adjustLocation ( callLoc );

										sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNREACHABLE_CODE, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNREACHABLE_CODE ) ) );
										sym->file->warnings.back ().get ()->setLocation ( callLoc );
									} else
									{
										sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNREACHABLE_CODE );
									}
									unreachableEmitted = true;
								}
								doEmit = false;
								break;
							case astOp::btLabel:
								doEmit = true;
								unreachableEmitted = false;
								break;
							case astOp::btAnnotation:
							case astOp::btStatement:
								break;
							default:
								if ( !doEmit )
								{
									if ( !unreachableEmitted && !it->isMetadata () )
									{
										errorLocality e ( &sym->file->errHandler, it );
										if ( isLS )
										{
											srcLocation	callLoc = it->location;
											it->adjustLocation ( callLoc );

											sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNREACHABLE_CODE, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNREACHABLE_CODE ) ) );
											sym->file->warnings.back ().get ()->setLocation ( callLoc );
										} else
										{
											sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNREACHABLE_CODE );
										}
									}
									unreachableEmitted = true;
								} else if ( !it->hasNoSideEffects ( sym, false ) )
								{
									for ( auto tmp = it; tmp; tmp = tmp->left )
									{
										if ( tmp->getOp () == astOp::logicalNot )
										{
											errorLocality e ( &sym->file->errHandler, it );
											if ( isLS )
											{
												srcLocation	callLoc = it->location;
												it->adjustLocation ( callLoc );

												sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwNO_EFFECT, sym->file->errHandler.throwWarning ( isLS, warnNum::scwNO_EFFECT ) ) );
												sym->file->warnings.back ().get ()->setLocation ( callLoc );
											} else
											{
												sym->file->errHandler.throwWarning ( isLS, warnNum::scwNO_EFFECT );
											}
											break;
										}
									}
									switch ( it->getOp () )
									{
										case astOp::funcCall:
											if ( it->left && it->left->getOp () == astOp::atomValue )
											{
												if ( it->left->symbolValue () == sym->file->exceptValue )
												{
													doEmit = false;
												}
											}
											break;
										case astOp::btIf:
										case astOp::btWhile:
										case astOp::btRepeat:
										case astOp::btFor:
										case astOp::btForEach:
										case astOp::btSwitch:
										case astOp::btBasic:
											if ( it->alwaysReturns () )
											{
												doEmit = false;
											}
											break;
										default:
											break;
									}
								} else
								{
									if ( (it->getOp () != astOp::symbolValue) && (it->getOp () != astOp::dummy) && (it->getOp () != astOp::btBasic) && (it->getOp () != astOp::funcCall) )
									{
										errorLocality e ( &sym->file->errHandler, it );
										if ( isLS )
										{
											srcLocation	callLoc = it->location;
											it->adjustLocation ( callLoc );

											sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwNO_EFFECT, sym->file->errHandler.throwWarning ( isLS, warnNum::scwNO_EFFECT ) ) );
											sym->file->warnings.back ().get ()->setLocation ( callLoc );
										} else
										{
											sym->file->errHandler.throwWarning ( isLS, warnNum::scwNO_EFFECT );
										}
									} else  if ( (it->getOp () == astOp::funcCall) )
									{
										errorLocality e ( &sym->file->errHandler, it );
										if ( isLS )
										{
											srcLocation	callLoc = it->location;
											it->adjustLocation ( callLoc );

											sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwNO_EFFECT, sym->file->errHandler.throwWarning ( isLS, warnNum::scwNO_EFFECT ) ) );
											sym->file->warnings.back ().get ()->setLocation ( callLoc );
										} else
										{
											sym->file->errHandler.throwWarning ( isLS, warnNum::scwNO_EFFECT );
										}
									}
								}
								break;
						}
					}
				}
			}
			break;
		case astOp::symbolDef:
			switch ( node->symbolDef ()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( node->symbolDef ()->getSymbolName ()[0] != '$' )	// don't throw warnings for compiler generated temps
					{
						if ( !func->isInterface )
						{
							if ( !node->symbolDef ()->isAccessed ( node->symbolDef ()->getSymbolName (), true ) && !node->symbolDef ()->isClosedOver ( node->symbolDef ()->getSymbolName () ) )
							{
								if ( !node->symbolDef ()->isAssigned ( node->symbolDef ()->getSymbolName (), true ) )
								{
									errorLocality e ( &sym->file->errHandler, node );
									if ( isLS )
									{
										sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNUSED_VARIABLE, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNUSED_VARIABLE, node->symbolDef ()->getSymbolName ().c_str () ) ) );
										sym->file->warnings.back ().get ()->setLocation ( node );
									} else
									{
										sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNUSED_VARIABLE, node->symbolDef ()->getSymbolName ().c_str () );
									}
								}
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::symbolValue:
			if ( sym->isDefined ( node->symbolValue (), isAccess ) )
			{
				switch ( sym->getScope ( node->symbolValue (), true ) )
				{
					case symbolSpaceScope::symLocal:
					case symbolSpaceScope::symLocalStatic:
						if ( !sym->isAssigned ( node->symbolValue (), true ) )
						{
							errorLocality e ( &sym->file->errHandler, node );
							if ( isLS )
							{
								sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwVARIABLE_USED_BEFORE_INITIALIZED, sym->file->errHandler.throwWarning ( isLS, warnNum::scwVARIABLE_USED_BEFORE_INITIALIZED, node->symbolValue ().c_str () ) ) );
								sym->file->warnings.back ().get ()->setLocation ( node );
							} else
							{
								sym->file->errHandler.throwWarning ( isLS, warnNum::scwVARIABLE_USED_BEFORE_INITIALIZED, node->symbolValue ().c_str () );
							}
						}
						break;
					case symbolSpaceScope::symScopeUnknown:
						{
							errorLocality e ( &sym->file->errHandler, node );
							if ( isLS )
							{
								sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNKNOWN_SYMBOL, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_SYMBOL, node->symbolValue ().c_str () ) ) );
								sym->file->warnings.back ().get ()->setLocation ( node );
							} else
							{
								sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_SYMBOL, node->symbolValue ().c_str () );
							}
						}
						break;
					default:
						break;
				}
			} else if ( strchr ( node->symbolValue (), ':' ) )
			{
				errorLocality e ( &sym->file->errHandler, node );
				sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_SYMBOL, *node ) );
				sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_SYMBOL );
			} else
			{
				errorLocality e ( &sym->file->errHandler, node );
				if ( isLS )
				{
					sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNKNOWN_SYMBOL, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_SYMBOL, node->symbolValue ().c_str () ) ) );
					sym->file->warnings.back ().get ()->setLocation ( node );
				} else
				{
					sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_SYMBOL, node->symbolValue ().c_str () );
				}
			}
			break;
		case astOp::assign:
			{
				compClass *classDef;
				compClassElementSearch *elem;

				switch ( node->left->getOp () )
				{
					case astOp::symbolPack:
						break;
					case astOp::symbolValue:
						if ( node->left->getOp () == astOp::symbolValue )
						{
							switch ( sym->getScope ( node->left->symbolValue (), false ) )
							{
								case symbolSpaceScope::symLocal:
								case symbolSpaceScope::symLocalParam:
								case symbolSpaceScope::symGlobal:
								case symbolSpaceScope::symLocalField:
								case symbolSpaceScope::symLocalStatic:
								case symbolSpaceScope::symClassStatic:
								case symbolSpaceScope::symClassIVar:
								case symbolSpaceScope::symClassAssign:
									break;
								default:
									{
										errorLocality e ( &sym->file->errHandler, node );
										sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_ASSIGNMENT, *node->left ) );
										sym->file->errHandler.throwFatalError ( isLS, errorNum::scILLEGAL_ASSIGNMENT );
									}
									break;
							}
						} else if ( node->left->getOp () == astOp::symbolPack )
						{
						} else if ( node->left->getOp () == astOp::sendMsg )
						{
							auto leftType = node->left->getType ( sym );

							if ( leftType.isAnObject () )
							{
								if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
								{
									if ( classDef && node->left->right->getOp () == astOp::nameValue )
									{
										// find the element we're assigning to
										elem = classDef->getElement ( node->left->right->nameValue () );
										if ( elem )
										{
											// what exactly are we trying to assign to
											switch ( elem->type )
											{
												case fgxClassElementType::fgxClassType_prop:
												case fgxClassElementType::fgxClassType_iVar:
												case fgxClassElementType::fgxClassType_static:
												default:
													{
														errorLocality e ( &sym->file->errHandler, node );
														sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_ASSIGNMENT, *node->left ) );
														sym->file->errHandler.throwFatalError ( isLS, errorNum::scILLEGAL_ASSIGNMENT );
													}
													break;
											}
										} else if ( classDef->defaultAssignIndex )
										{
										} else
										{
											errorLocality e ( &sym->file->errHandler, node->left->right );
											sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_IVAR, *node->left ) );
											sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_IVAR );
										}
									}
								}
							} else if ( leftType == symUnknownType )
							{
							} else
							{
								errorLocality e ( &sym->file->errHandler, node );
								sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_ASSIGNMENT, *node ) );
								sym->file->errHandler.throwFatalError ( isLS, errorNum::scILLEGAL_ASSIGNMENT );
							}
						}
						break;
					case astOp::sendMsg:
						if ( node->left->right->getOp () == astOp::nameValue )
						{
							auto leftType = node->left->left->getType ( sym );
							// fastest mode - we're assigning to a local with a known type
							if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
							{
								cacheString symbolName;

								fgxClassElemSearchType searchType;
								compClassElementSearch *elem;

								if ( node->left->left->getOp () == astOp::symbolValue )
								{
									if ( !strccmp ( node->left->left->symbolValue (), sym->file->selfValue ) )
									{
										searchType = fgxClassElemSearchType::fgxClassElemPrivateAssign;
									} else
									{
										searchType = fgxClassElemSearchType::fgxClassElemPublicAssign;
									}
								} else
								{
									searchType = fgxClassElemSearchType::fgxClassElemPublicAssign;
								}
								elem = classDef->getElement ( searchType, node->left->right->nameValue () );

								if ( elem )
								{
									switch ( elem->type )
									{
										case fgxClassElementType::fgxClassType_iVar:
										case fgxClassElementType::fgxClassType_static:
											break;
										case fgxClassElementType::fgxClassType_prop:
											if ( !elem->assign.func )
											{
												errorLocality e ( &sym->file->errHandler, node );
												sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_ASSIGNMENT, *node->left->right ) );
												sym->file->errHandler.throwFatalError ( isLS, errorNum::scILLEGAL_ASSIGNMENT );
											}
											break;
										default:
											{
												errorLocality e ( &sym->file->errHandler, node );
												sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_ASSIGNMENT, *node->left->right ) );
												sym->file->errHandler.throwFatalError ( isLS, errorNum::scILLEGAL_ASSIGNMENT );
											}
											break;
									}
								} else if ( !classDef->getDefaultAssign () )
								{
									errorLocality e ( &sym->file->errHandler, node );
									sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_IVAR, *node->left->right ) );
									sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_IVAR );
								}
							} else
							{
								switch ( leftType.compType() )
								{
									case symbolType::symObject:
									case symbolType::symVariant:
									case symbolType::symUnknown:
										break;
									default:
										{
											errorLocality e ( &sym->file->errHandler, node );
											sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scSEND_TO_NONOBJECT, *node ) );
											sym->file->errHandler.throwFatalError ( isLS, errorNum::scSEND_TO_NONOBJECT );
										}
										break;
								}
								switch ( node->left->right->getCompType ( sym ) )
								{
									case symbolType::symString:
									case symbolType::symVariant:
										break;
									default:
										{
											errorLocality e ( &sym->file->errHandler, node );
											sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_IVAR, *node->left->right ) );
											sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_IVAR );
										}
										break;
								}
							}
						} else if ( node->left->left->getType ( sym ).hasClass () && sym->findClass ( node->left->left->getType ( sym ).getClass () ) )
						{
							switch ( node->left->right->getCompType ( sym ) )
							{
								case symbolType::symString:
								case symbolType::symVariant:
									break;
								default:
									{
										errorLocality e ( &sym->file->errHandler, node );
										sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_IVAR, *node->left->right ) );
										sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_IVAR );
									}
									break;
							}
						}
						break;
					case astOp::arrayDeref:
						switch ( node->left->left->getType ( sym ).compType () )
						{
							case symbolType::symArray:
								[[fallthrough]];
							case symbolType::symVArray:
							case symbolType::symObject:
							case symbolType::symVariant:
							case symbolType::symUnknown:
								break;
							default:
								{
									errorLocality e ( &sym->file->errHandler, node );
									sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_OPERAND, *node->left->left ) );
									sym->file->errHandler.throwFatalError ( isLS, errorNum::scILLEGAL_OPERAND );
								}
								break;
						}
						break;
					case astOp::workAreaStart:
						switch ( node->left->right->getCompType ( sym ) )
						{
							case symbolType::symString:
							case symbolType::symInt:
							case symbolType::symVariant:
							case symbolType::symUnknown:
								break;
							default:
								{
									errorLocality e ( &sym->file->errHandler, node );
									sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_FIELD, *node->left->left ) );
									sym->file->errHandler.throwFatalError ( isLS, errorNum::scINVALID_FIELD );
								}
								break;
						}
						// don't need to veryfy the workarea->left as that's done below... for the assign we just need to verify the RHS of the workarea is an lvalue (a field or a variant of some sort)
						break;
					default:
						// generic... storage into a non-local symbol
						if ( (symbolType)node->left->getType ( sym ) != symVariantType )
						{
							errorLocality e ( &sym->file->errHandler, node );
							sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_ASSIGNMENT, *node->left ) );
							sym->file->errHandler.throwFatalError ( isLS, errorNum::scILLEGAL_ASSIGNMENT );
						}
				}
				break;
		case astOp::funcCall:
			if ( node->left->getOp () == astOp::atomValue )
			{
				if ( node->left->symbolValue () == sym->file->newValue )
				{
					if ( node->pList ().param.size () )
					{
						if ( (node->pList ().param[0]->getOp () == astOp::nameValue) )
						{
							// we know the class name
							auto cl = sym->findClass ( node->pList ().param[0]->nameValue () );
							if ( !cl )
							{
								errorLocality e ( &sym->file->errHandler, node->pList ().param[0] );
								if ( isLS )
								{
									sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNDEFINED_CLASS, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNDEFINED_CLASS, node->pList ().param[0]->nameValue ().c_str () ) ) );
									sym->file->warnings.back ().get ()->setLocation ( node->pList ().param[0] );
								} else
								{
									sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNDEFINED_CLASS, node->pList ().param[0]->nameValue ().c_str () );
								}
							}
						}
					} else
					{
						if ( isLS )
						{
							sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_CLASSNAME, *node->left ) );
						}
						errorLocality e ( &sym->file->errHandler, node->left );
						sym->file->errHandler.throwFatalError ( isLS, errorNum::scMISSING_CLASSNAME );
					}
				} else if ( node->left->symbolValue () == sym->file->exceptValue ||
							node->left->symbolValue () == sym->file->typeValue ||
							node->left->symbolValue () == sym->file->paramValue
							)
				{
					if ( node->pList ().param.size () != 1 )
					{
						errorLocality e ( &sym->file->errHandler, node->left );
						sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_PARAMETER, *node->left ) );
						sym->file->errHandler.throwFatalError ( isLS, errorNum::scINVALID_PARAMETER );
					}
				} else if ( node->left->symbolValue () == sym->file->unpackValue ||
							node->left->symbolValue () == sym->file->varCopyValue
							)
				{
					if ( node->pList ().param.size () > 2 )
					{
						errorLocality e ( &sym->file->errHandler, node->left );
						sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_PARAMETER, *node->left ) );
						sym->file->errHandler.throwFatalError ( isLS, errorNum::scINVALID_PARAMETER );
					}
				} else
				{
					if ( sym->isDefined ( node->left->symbolValue (), true ) )
					{
						size_t nParamsSent = node->pList ().param.size ();
						// standard function call
						if ( sym->isMethod ( node->left->symbolValue (), true ) ) nParamsSent++;
						if ( (int32_t)nParamsSent > sym->getFuncNumParams ( sym->getFuncName ( node->left->symbolValue (), true ), true ) && !sym->isVariantParam ( node->left->symbolValue (), true ) && !sym->isPcountParam ( node->left->symbolValue (), true ) )
						{
							errorLocality e ( &sym->file->errHandler, node->left );
							if ( isLS )
							{
								srcLocation	callLoc = node->location;

								sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwINCORRECT_NUM_PARAMS, sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_NUM_PARAMS, node->left->symbolValue ().c_str () ) ) );
								sym->file->warnings.back ().get ()->setLocation ( callLoc );
							} else
							{
								sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_NUM_PARAMS, node->left->symbolValue ().c_str () );
							}
						}
						if ( func && !func->isFGL )
						{
							auto funcIndex = sym->isMethod ( node->left->symbolValue (), true ) ? 1 : 0;
							size_t paramIndex = 0;
							for ( ; funcIndex < sym->getFuncNumParams ( sym->getFuncName ( node->left->symbolValue (), true ), true ); funcIndex++, paramIndex++ )
							{
								astNode *param = nullptr;
								if ( paramIndex < node->pList ().param.size () )
								{
									param = node->pList ().param[paramIndex];
								}
								if ( !param )
								{
									param = sym->getFuncDefaultParam ( sym->getFuncName ( node->left->symbolValue (), true ), true, funcIndex );
								}
								if ( !param )
								{
									errorLocality e ( &sym->file->errHandler, node->left );
									if ( isLS )
									{
										sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwMISSING_PARAMS, sym->file->errHandler.throwWarning ( isLS, warnNum::scwMISSING_PARAMS, node->left->symbolValue ().c_str () ) ) );
										sym->file->warnings.back ().get ()->setLocation ( node->location );
									} else
									{
										sym->file->errHandler.throwWarning ( isLS, warnNum::scwMISSING_PARAMS, node->left->symbolValue ().c_str () );
									}
								}
							}
						}
					} else
					{
						switch ( sym->getScope ( node->left->symbolValue (), true ) )
						{
							case symbolSpaceScope::symClass:
								break;
							default:
								{
									if ( isLS )
									{
										sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNKNOWN_FUNCTION, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_FUNCTION, node->left->symbolValue ().c_str () ) ) );
										sym->file->warnings.back ().get ()->setLocation ( node->left );
									} else
									{
										errorLocality e ( &sym->file->errHandler, node->left );
										sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_FUNCTION, node->left->symbolValue ().c_str () );
									}
								}
								break;
						}
					}
				}
			} else if ( node->left->getOp () == astOp::sendMsg )
			{
				// method calls
				symbolTypeClass type = node->left->left->getType ( sym );
				if ( type.isAnObject () )
				{
					compClass *classDef = nullptr;
					if ( type.hasClass () && (classDef = sym->findClass ( type.getClass () )) )
					{
						if ( classDef && node->left->right->getOp () == astOp::nameValue )
						{
							compClassElementSearch *elem = classDef->getElement ( node->left->right->nameValue () );

							if ( elem )
							{
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_method:
										break;
									case fgxClassElementType::fgxClassType_prop:
									case fgxClassElementType::fgxClassType_inherit:
										{
											errorLocality e ( &sym->file->errHandler, node );
											elem = nullptr;
											if ( isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_ALLOWED_WITH_PROP_METHODS, *node ) );
											sym->file->errHandler.throwFatalError ( isLS, errorNum::scNOT_ALLOWED_WITH_PROP_METHODS );
										}
										break;
									default:
										elem = nullptr;
										break;
								}
							} else
							{
								if ( classDef->defaultAccessIndex )
								{
									elem = &classDef->elements[(int64_t)classDef->defaultAccessIndex - 1];
								} else
								{
									errorLocality e ( &sym->file->errHandler, node->left->right );
									if ( isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_METHOD, *node->left->right ) );
									sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_METHOD );
								}
							}
							if ( elem )
							{
								size_t nParamsSent = node->pList ().param.size ();
								if ( !elem->isStatic ) nParamsSent++;
								if ( (int32_t)nParamsSent > elem->methodAccess.func->params.size () && !sym->isVariantParam ( elem->methodAccess.func->name, true ) && !sym->isPcountParam ( elem->methodAccess.func->name, true ) )
								{
									errorLocality e ( &sym->file->errHandler, node );
									if ( isLS )
									{
										sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwINCORRECT_NUM_PARAMS, sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_NUM_PARAMS, elem->name.c_str () ) ) );
										sym->file->warnings.back ().get ()->setLocation ( node->left->right );
									} else
									{
										sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_NUM_PARAMS, elem->name.c_str () );
									}
								}

								if ( func && !func->isFGL )
								{
									auto funcIndex = 1;
									size_t paramIndex = 0;
									for ( ; funcIndex < elem->methodAccess.func->params.size (); funcIndex++, paramIndex++ )
									{
										astNode *param = nullptr;
										if ( paramIndex < node->pList ().param.size () )
										{
											param = node->pList ().param[paramIndex];
										}
										if ( !param )
										{
											param = elem->methodAccess.func->params[funcIndex]->initializer;
										}
										if ( !param )
										{
											errorLocality e ( &sym->file->errHandler, node->left );
											if ( isLS )
											{
												srcLocation	callLoc = node->location;

												sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwMISSING_PARAMS, sym->file->errHandler.throwWarning ( isLS, warnNum::scwMISSING_PARAMS, node->left->right->nameValue ().c_str () ) ) );
												sym->file->warnings.back ().get ()->setLocation ( callLoc );
											} else
											{
												sym->file->errHandler.throwWarning ( isLS, warnNum::scwMISSING_PARAMS, node->left->right->nameValue ().c_str () );
											}
										}
									}
								}
							}
						}
					}
				}
			} else
			{
				auto leftType = node->left->getType ( sym );
				compClass *classDef = nullptr;
				if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
				{
					if ( classDef && classDef->overload[int ( fgxOvOp::ovFuncCall )] )
					{
						auto elem = &classDef->elements[(int64_t)classDef->overload[int ( fgxOvOp::ovFuncCall )] - 1];
						if ( elem )
						{
							break;
						}
					}
					errorLocality e ( &sym->file->errHandler, node );
					if ( isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_METHOD, *node ) );
					sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_METHOD );
				}
			}
			}
			break;
		case astOp::cSend:
		case astOp::sendMsg:
			if ( node->left )
			{
				auto leftType = node->left->getType ( sym );
				compClass *classDef = nullptr;
				if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
				{
					if ( classDef )
					{
						if ( node->right->getOp () == astOp::nameValue )
						{
							fgxClassElemSearchType searchType;

							if ( isAccess )	// assignment is handled in astOp::assign case
							{
								if ( node->left->getOp () == astOp::symbolValue )
								{
									if ( !strccmp ( node->left->symbolValue (), sym->file->selfValue ) )
									{
										searchType = fgxClassElemSearchType::fgxClassElemPrivateAccess;
									} else
									{
										searchType = fgxClassElemSearchType::fgxClassElemPublicAccess;
									}
								} else
								{
									searchType = fgxClassElemSearchType::fgxClassElemPublicAccess;
								}

								compClassElementSearch *elem = classDef->getElement ( searchType, node->right->nameValue () );

								if ( elem || classDef->defaultAccessIndex )
								{
								} else if ( parent->getOp () != astOp::funcCall )
								{
									errorLocality e ( &sym->file->errHandler, node->right );
									if ( isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_IVAR, *node->right ) );
									sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_IVAR );
								}
							}
						}
					} else
					{
						if ( node->right->getOp () != astOp::nameValue )
						{
							auto rightType = node->right->getCompType ( sym );
							switch ( rightType )
							{
								case symbolType::symString:
								case symbolType::symVariant:
								case symbolType::symUnknown:
									break;
								default:
									{
										errorLocality e ( &sym->file->errHandler, node );
										if ( !isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scUNKNOWN_IVAR, *node ) );
										sym->file->errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_IVAR );
									}
									break;
							}
						}
					}
				} else
				{
					auto rightType = node->left->getCompType ( sym );
					switch ( rightType )
					{
						case symbolType::symObject:
						case symbolType::symVariant:
						case symbolType::symUnknown:
							break;
						case symbolType::symAtom:
							{
								auto cls = sym->findClass ( node->left->symbolValue() );
								if ( cls )
								{
									if ( node->right->getOp () == astOp::nameValue )
									{
										auto it = cls->getElement ( node->right->nameValue() );

										if ( it )
										{
											if ( it->type != fgxClassElementType::fgxClassType_static && it->type != fgxClassElementType::fgxClassType_const )
											{
												errorLocality e ( &sym->file->errHandler, node->right );
												if ( !isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scNOT_STATIC_MEMBER, *node->right ) );
												sym->file->errHandler.throwFatalError ( isLS, errorNum::scNOT_STATIC_MEMBER );
											}
										} else
										{
											errorLocality e ( &sym->file->errHandler, node->right );
											if ( !isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scCLASS_MEMBER_NOT_FOUND, *node->right ) );
											sym->file->errHandler.throwFatalError ( isLS, errorNum::scCLASS_MEMBER_NOT_FOUND );
										}
									}
								} else
								{
									errorLocality e ( &sym->file->errHandler, node->left );
									if ( !isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scSEND_TO_NONOBJECT, *node->left ) );
									sym->file->errHandler.throwFatalError ( isLS, errorNum::scSEND_TO_NONOBJECT );
								}
							}
							break;
						default:
							{
								errorLocality e ( &sym->file->errHandler, node );
								if ( !isLS ) sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scSEND_TO_NONOBJECT, *node->left ) );
								sym->file->errHandler.throwFatalError ( isLS, errorNum::scSEND_TO_NONOBJECT );
							}
							break;
					}
				}
			}
			break;
		case astOp::logicalAnd:
			if ( node->left->getOp () == astOp::logicalOr && node->right->getOp () == astOp::logicalOr )
			{
				errorLocality e ( &sym->file->errHandler, node );
				if ( isLS )
				{
					sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwINCORRECT_MUTUAL_EXCLUSION, sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_MUTUAL_EXCLUSION ) ) );
					sym->file->warnings.back ().get ()->setLocation ( node );
				} else
				{
					sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_MUTUAL_EXCLUSION );
				}
			}
			break;
		case astOp::less:
		case astOp::lessEqual:
		case astOp::greater:
		case astOp::greaterEqual:
		case astOp::equal:
		case astOp::equali:
			switch ( node->left->getOp () )
			{
				case astOp::less:
				case astOp::lessEqual:
				case astOp::greater:
				case astOp::greaterEqual:
				case astOp::equal:
				case astOp::equali:
					{
						errorLocality e ( &sym->file->errHandler, node );
						if ( isLS )
						{
							sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwINCORRECT_LOGIC, sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_LOGIC ) ) );
							sym->file->warnings.back ().get ()->setLocation ( node );
						} else
						{
							sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_LOGIC );
						}
					}
					break;
				default:
					break;
			}
			switch ( node->right->getOp () )
			{
				case astOp::less:
				case astOp::lessEqual:
				case astOp::greater:
				case astOp::greaterEqual:
				case astOp::equal:
				case astOp::equali:
					{
						errorLocality e ( &sym->file->errHandler, node );
						if ( isLS )
						{
							sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwINCORRECT_LOGIC, sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_LOGIC ) ) );
							sym->file->warnings.back ().get ()->setLocation ( node );
						} else
						{
							sym->file->errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_LOGIC );
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::workAreaStart:
			switch ( node->left->getCompType ( sym ) )
			{
				case symbolType::symString:
				case symbolType::symInt:
				case symbolType::symVariant:
				case symbolType::symUnknown:
					break;
				default:
					{
						errorLocality e ( &sym->file->errHandler, node );
						sym->file->errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_FIELD, *node->left->left ) );
						sym->file->errHandler.throwFatalError ( isLS, errorNum::scINVALID_FIELD );
					}
					break;
			}
			break;
		default:
			break;
	}
	return node;
}

void compExecutable::compGenErrorsWarnings ( opFunction *func, bool isLS )
{
	if ( !func->inUse || func->isIterator || func->isInterface ) return;
	if ( func->codeBlock && func->location.sourceIndex )
	{
		symbolStack sym ( file, func );
		astNodeWalk ( func->codeBlock, &sym, genErrorsWarningsCB, func, isLS );
	}
}

void compExecutable::compGenErrorsWarnings ( bool isLS )
{
	for ( auto &it : file->functionList )
	{
		compGenErrorsWarnings ( it.second, isLS );
	}
}

