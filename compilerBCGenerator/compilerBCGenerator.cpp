#include "compilerBCGenerator.h"

cacheString compExecutable::makeLabel ( char const *name )
{
	stringi tmp ( name );
	tmp = name + std::to_string ( labelId++ );

	return file->sCache.get ( tmp );
}

void compExecutable::compEmitContext ( class symbolStack *sym, compClassElementSearch *elem, int32_t index, bool isAccess, bool isVirtual )
{
	if ( elem->isVirtual && isVirtual )
	{
		if ( isAccess )
		{
			codeSegment.putOp ( fglOpcodes::pushContextVirt, index, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
		} else
		{
			codeSegment.putOp ( fglOpcodes::pushContextVirt, index, (int16_t)elem->assign.data.vTabEntry, 0 );
		}
	} else
	{
		if ( isAccess )
		{
			if ( elem->methodAccess.objectStart )
			{
				codeSegment.putOp ( fglOpcodes::pushContext, index, elem->methodAccess.objectStart, 0 );
			} else
			{
				codeSegment.putOp ( fglOpcodes::pushSymLocal, ( uint32_t) index );
			}
		} else
		{
			if ( elem->assign.objectStart )
			{
				codeSegment.putOp ( fglOpcodes::pushContext, index, elem->assign.objectStart, 0 );
			} else
			{
				codeSegment.putOp ( fglOpcodes::pushSymLocal, ( uint32_t) index );
			}
		}
	}
	sym->pushStack ( );
}
void compExecutable::compEmitContext ( class symbolStack *sym, cacheString const &symName, int32_t index, bool isAccess, bool isVirtual )
{
	if ( sym->isVirtual ( symName, isAccess ) && isVirtual )
	{
		codeSegment.putOp ( fglOpcodes::pushContextVirt, index, sym->getObjectOffset ( symName, isAccess ), 0 );
	} else
	{
		if ( sym->getObjectOffset ( symName, isAccess ) )
		{
			codeSegment.putOp ( fglOpcodes::pushContext, index, sym->getObjectOffset ( symName, isAccess ), 0 );
		} else
		{
			codeSegment.putOp ( fglOpcodes::pushSymLocal, (uint32_t) index );
		}
	}
	sym->pushStack ();
}

void compExecutable::compEmitLValue ( opFunction *funcDef, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue )
{
	compClass					*classDef;
	compClassElementSearch		*elem;
	symbolTypeClass				 type;

	switch ( node->getOp() )
	{
		case astOp::dummy:
			compEmitLValue ( funcDef, node->left, sym, needType, retType, safeCalls,  needValue );
			break;
		case astOp::symbolValue:
			switch ( sym->getScope ( node->symbolValue(), true ) )
			{
				case symbolSpaceScope::symGlobal:
				case symbolSpaceScope::symLocalStatic:
					codeSegment.putOp ( fglOpcodes::pushSymGlobalRef, sym->getIndex ( node->symbolValue(), true ) );
					sym->pushStack ( );
					break;
				case symbolSpaceScope::symClassStatic:
					if ( sym->isInterface ( node->symbolValue(), true ) )
					{
						if ( node->symbolValue().find ( "::" ) != stringi::npos )
						{
							auto className = node->symbolValue().substr ( 0, node->symbolValue().find ( "::" ) );
							auto message = node->symbolValue().substr ( node->symbolValue().find ( "::" ) + 2 );
							codeSegment.putOp ( fglOpcodes::pushClassGlobalRef, dataSegment.addString ( className ), dataSegment.addString ( message ) );
						} else
						{
							throw errorNum::scINTERNAL;
						}
					} else
					{
						codeSegment.putOp ( fglOpcodes::pushSymGlobalRef, sym->getIndex ( node->symbolValue(), true ) );
					}
					sym->pushStack ( );
					break;
				case symbolSpaceScope::symLocal:
				case symbolSpaceScope::symLocalParam:
					codeSegment.putOp ( fglOpcodes::promote, sym->getIndex ( node->symbolValue(), true ) );
					sym->pushStack ( );
					break;
				case symbolSpaceScope::symLocalField:
					throw errorNum::scFIELD_REF;
				case symbolSpaceScope::symClassInherit:
					throw errorNum::scNOT_AN_LVALUE;
				case symbolSpaceScope::symClassAccess:
					elem = sym->findClass ( node->getType ( sym ).getClass() )->getElement ( node->nameValue() );

					// push context of access method
					compEmitContext ( sym, elem, sym->getIndex ( file->selfValue, true ), true, node->symbolValue().find ( "::" ) == stringi::npos );
					// call the code
					compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, node, sym, true, 1, false, elem );
					break;
				case symbolSpaceScope::symClassIVar:
					if ( sym->isVirtual ( node->symbolValue(), true ) )
					{
						codeSegment.putOp ( fglOpcodes::pushObjVirtIVarRef, sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->symbolValue(), true ), 0 );
					} else
					{
						codeSegment.putOp ( fglOpcodes::pushObjIVarRef, sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->symbolValue(), true ), 0 );
					}
					sym->pushStack ( );
					break;
				default:
					throw errorNum::scINVALID_ASSIGN;
			}
			break;
		case astOp::sendMsg:
			if ( node->right->getOp() == astOp::nameValue )
			{
				cacheString symbolName;
				fgxClassElemSearchType	 searchType;

				if ( node->left->getType ( sym ).hasClass() && (classDef = sym->findClass ( node->left->getType ( sym ).getClass () )) )
				{
					if ( (node->left->getOp() == astOp::symbolValue) && node->left->symbolValue() == file->selfValue )
					{
						symbolName = file->selfValue;
						searchType = fgxClassElemSearchType::fgxClassElemPrivateAccess;
					} else
					{
						symbolName = cacheString();
						if ( node->left->getOp() == astOp::symbolValue )
						{
							switch ( sym->getScope ( node->left->symbolValue(), true ) )
							{
								case symbolSpaceScope::symLocal:
								case symbolSpaceScope::symLocalParam:
									symbolName = node->left->symbolValue();
									break;
								default:
									break;
							}
						}
						searchType = fgxClassElemSearchType::fgxClassElemPublicAccess;
					}
					elem = classDef->getElement ( searchType, node->right->nameValue() );

					if ( elem )
					{
						switch ( elem->type )
						{
							case fgxClassElementType::fgxClassType_iVar:
								if ( symbolName )
								{
									if ( elem->isVirtual )
									{
										codeSegment.putOp ( fglOpcodes::pushObjVirtIVarRef, sym->getIndex (symbolName, true ), (int16_t)elem->methodAccess.data.vTabEntry, 0 );
									} else
									{
										codeSegment.putOp ( fglOpcodes::pushObjIVarRef, sym->getIndex (symbolName, true ), (int16_t)elem->methodAccess.data.varIndex, 0 );
									}
									sym->pushStack ( );
								} else
								{
									compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls, true );
									if ( elem->isVirtual )
									{
										codeSegment.putOp ( fglOpcodes::pushObjVirtIVarRelRef, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
									} else
									{
										codeSegment.putOp ( fglOpcodes::pushObjIVarRelRef, 1, (int16_t)elem->methodAccess.data.varIndex, 0 );
									}
								}
								break;
							case fgxClassElementType::fgxClassType_static:
								if ( sym->isInterface ( node->symbolValue(), true ) )
								{
									if ( node->symbolValue().find ( "::" ) )
									{
										auto className = node->symbolValue().substr ( 0, node->symbolValue().find ( "::" ) );
										auto message = node->symbolValue().substr ( node->symbolValue().find ( "::" ) + 2 );
										codeSegment.putOp ( fglOpcodes::pushClassGlobalRef, dataSegment.addString ( className ), dataSegment.addString ( message ) );
									} else
									{
										throw errorNum::scINTERNAL;
									}
								} else
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobalRef, sym->getIndex ( node->symbolValue(), true ) );
								}
								sym->pushStack ( );
								break;
							case fgxClassElementType::fgxClassType_prop:
								// emit object context
								compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, node->right->nameValue().find ( "::" ) == stringi::npos );
								// call the code
								compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, node, sym, needValue, 1, false, elem );
								break;
							case fgxClassElementType::fgxClassType_inherit:
								if ( symbolName )
								{
									// push context of assignment method
									compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, node->right->nameValue().find ( "::" ) == stringi::npos );
								} else
								{
									// emit the object
									compEmitNode ( funcDef, node->left, sym, symWeakObjectType, retType, safeCalls,  true );
									// adjust the context of the emitted object
									if ( elem->isVirtual )
									{
										codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->assign.data.vTabEntry, 0 );
									} else
									{
										if ( elem->assign.objectStart )
										{
											codeSegment.putOp ( fglOpcodes::modifyContext, 1, (int16_t)elem->assign.objectStart, 0 );
										}
									}
								}
								break;
							case fgxClassElementType::fgxClassType_method:
							case fgxClassElementType::fgxClassType_const:
							default:
								throw errorNum::scILLEGAL_OPERATION;
						}
						break;
					} else
					{
						if ( (elem = classDef->getDefaultAccess ( )) )
						{
							if ( symbolName )
							{
								compEmitNode ( funcDef, node->right, sym, symStringType, retType, safeCalls,  true ); 		// push message
								compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, true );
								// call the code
								compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, node, sym, needValue, 2, false, elem );
							} else
							{
								compEmitNode ( funcDef, node->right, sym, symStringType, retType, safeCalls,  true ); 		// push message
							    compEmitNode ( funcDef, node->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
								if ( elem->isVirtual )
								{
									codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->assign.data.vTabEntry, 0 );
								} else
								{
									if ( elem->assign.objectStart )
									{
										codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->assign.objectStart, 0 );
									}
								}
								// call the code
								compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, node, sym, needValue, 2, false, elem );
							}
							break;
						} else
						{
							// no such element... we know class and it doesn't exist or have a default accessor
							throw errorNum::scILLEGAL_OPERATION;
						}
					}
				}
			}
			switch ( node->right->getOp() )
			{
				case astOp::nameValue:
					compEmitNode ( funcDef, node->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
					codeSegment.putOp ( fglOpcodes::objAccessRef, dataSegment.addString ( node->right->nameValue() ), bssSize );
					bssSize += sizeof ( bcObjQuick );
					break;
				default:
					compEmitNode ( funcDef, node->right, sym, symStringType, retType, safeCalls,  true ); 			// push message
					compEmitNode ( funcDef, node->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
					codeSegment.putOp ( fglOpcodes::objAccessRefInd );
					sym->popStack ( );
					break;
			}
			break;
		case astOp::arrayDeref:
			switch ( node->left->getType ( sym).compType() )
			{
				case symbolType::symArray:
				case symbolType::symVArray:
				case symbolType::symString:
				case symbolType::symObject:
				case symbolType::symVariant:
					compEmitNode ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true );
					for ( size_t index = 0; index < node->pList().param.size ( ); index++ )
					{
						compEmitNode ( funcDef, node->pList().param[index], sym, symVariantType, retType, safeCalls,  true );
						codeSegment.putOp ( fglOpcodes::arrDerefRef );
						sym->popStack ( );
					}
					if ( !needValue )
					{
						codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
						sym->popStack ( );
					}
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			break;
		case astOp::funcCall:
			type = compEmitNode ( funcDef, node, sym, needType, retType, safeCalls,  true ); // call the function
			switch ( type.compType() )
			{
				case symbolType::symObject:
				case symbolType::symVariant:
				case symbolType::symArray:
				case symbolType::symVArray:
					codeSegment.putOp ( fglOpcodes::castRef );		// hope that what we get back is acutally a reference
					break;
				default:
					throw errorNum::scNOT_A_REF;
			}
			break;
		case astOp::btInline:
			{
				errorLocality e ( &file->errHandler, node, true );
				compEmitNode ( funcDef, node, sym, needType, retType, safeCalls, true );
			}
			break;
		case astOp::refCreate:
		case astOp::refPromote:
			compEmitLValue ( funcDef, node->left, sym, needType, retType, safeCalls,  true );
			break;
		case astOp::conditional:
			if ( node->left->isTrue ( ) )
			{
				compEmitLValue ( funcDef, node->conditionData().trueCond, sym, needType, retType, safeCalls,  true );
			} else if ( node->left->isFalse ( ) )
			{
				compEmitLValue ( funcDef, node->conditionData().falseCond, sym, needType, retType, safeCalls,  true);
			} else
			{
				char			ifLabel[256];
				char			ifEndLabel[256];
				symbolTypeClass	condType;

				sprintf_s ( ifLabel, sizeof ( ifLabel ), "$cond_not_true%u", labelId++ );
				sprintf_s ( ifEndLabel, sizeof ( ifLabel ), "$cond_end%u", labelId++ );

				condType = compEmitNode ( funcDef, node->left, sym, symIntType, retType, safeCalls,  true );
				compEmitCast ( condType, symIntType );

				// if not true jmp to false condtion
				fixup->needFixup ( file->sCache.get ( ifLabel ), codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
				listing.emitOps ( sym );

				// emit true condition
				compEmitLValue ( funcDef, node->conditionData().trueCond, sym, needType, retType, safeCalls,  true );
				sym->popStack ( );

				// jmp to code after false code
				fixup->needFixup ( file->sCache.get ( ifEndLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
				listing.emitOps ( sym );


				// emit not true label
				codeSegment.emitLabel ( ifLabel );
				fixup->setRefPoint ( file->sCache.get ( ifLabel ), codeSegment.nextOp ( ) );
				listing.label ( sym, ifLabel );

				compEmitLValue ( funcDef, node->conditionData().falseCond, sym, needType, retType, safeCalls,  true );
				sym->popStack ( );

				// emit end of condition label
				codeSegment.emitLabel ( ifEndLabel );
				fixup->setRefPoint ( file->sCache.get ( ifEndLabel ), codeSegment.nextOp ( ) );
				listing.label ( sym, ifEndLabel );

				// we popped each of the true/false stack values... one WILL survive so we need to add a spot on the stack back for it
				sym->pushStack( );
			}
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

class symbolTypeClass compExecutable::compEmitIncOp ( opFunction *funcDef, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, bool pre, bool isInc, fglOpcodes localInt, fglOpcodes localDouble, fglOpcodes localVar, fglOpcodes globalInt, fglOpcodes globalDouble, fglOpcodes globalVar, fglOpcodes objVar, fglOpcodes objVarVirt, fgxOvOp ovOp )
{
	switch ( node->left->getOp() )
	{
		case astOp::symbolValue:
			switch ( sym->getScope ( node->left->symbolValue(), true ) )
			{
				case symbolSpaceScope::symClassConst:
				case symbolSpaceScope::symLocalConst:
					throw errorNum::scASSIGN_TO_CONST;
				case symbolSpaceScope::symClassIVar:
					if ( sym->isVirtual ( node->left->symbolValue(), true ) )
					{
						if ( !pre )
						{
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::pushObjVirtIVar, sym->getIndex ( file->selfValue, true ), sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
								sym->pushStack ( );
							}
						}
						codeSegment.putOp ( objVarVirt, sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
						if ( pre )
						{
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::pushObjVirtIVar, sym->getIndex ( file->selfValue, true ), sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
								sym->pushStack ( );
							}
						}
					} else
					{
						if ( !pre )
						{
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::pushObjIVar, sym->getIndex ( file->selfValue, true ), sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
								sym->pushStack ( );
							}
						}

						codeSegment.putOp ( objVar, sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
						if ( pre )
						{
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::pushObjIVar, sym->getIndex ( file->selfValue, true ), sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
								sym->pushStack ( );
							}
						}
					}
						
					break;
				case symbolSpaceScope::symClassAccess:
					{
						auto elem = sym->findClass ( sym->getType ( file->selfValue, true ).getClass () )->getElement ( node->symbolValue() );

						if ( !elem || !elem->assign.func || !elem->methodAccess.func )
						{
							throw errorNum::scILLEGAL_ASSIGNMENT;
						}

						// push context of access method
						compEmitContext ( sym, elem, sym->getIndex ( file->selfValue, true ), true, node->symbolValue().find ( "::" ) == stringi::npos );
						// call the code
						compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, node, sym, needValue, 1, false, elem );

						if ( needValue && !pre )
						{
							// post increment, so duplicate the value so we continue to have it
							codeSegment.putOp ( fglOpcodes::dupv, (uint32_t) 1 );
							sym->pushStack ();
						}
						codeSegment.putOp ( fglOpcodes::pushNumLong, (int64_t) 1 );
						sym->pushStack ();
						if ( isInc )
						{
							codeSegment.putOp ( fglOpcodes::addv );
						} else
						{
							codeSegment.putOp ( fglOpcodes::subv );
						}
						sym->popStack ();

						if ( needValue && !pre )
						{
							needValue = false;
						}

						// push context of assignment method
						compEmitContext ( sym, elem, sym->getIndex ( file->selfValue, true ), false, node->symbolValue().find ( "::" ) == stringi::npos );
						// call the code
						compEmitFuncCall ( funcDef, elem->assign.func->name, true, node, sym, needValue, 2, false, elem );
						if ( needValue && elem->assign.func->getReturnType ().compType () == symbolType::symVariant )
						{
							codeSegment.putOp ( fglOpcodes::deref, (uint32_t) 1 );
						}
					}
					break;
				case symbolSpaceScope::symLocal:
				case symbolSpaceScope::symLocalParam:
					switch ( node->getType ( sym ).compType() )
					{
						case symbolType::symInt:
							if ( !pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymLocal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							codeSegment.putOp ( localInt, sym->getIndex ( node->left->symbolValue(), true ), (uint32_t) ovOp );
							if ( pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymLocal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							break;
						case symbolType::symDouble:
							if ( !pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymLocal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							codeSegment.putOp ( localDouble, sym->getIndex ( node->left->symbolValue(), true ), (uint32_t) ovOp );
							if ( pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymLocal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							break;
						default:
//TODO: Fix this for variants!!!!!   we shouldn't push for object overloads since the value they return is what we want!
							if ( !pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymLocal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							codeSegment.putOp ( localVar, sym->getIndex ( node->left->symbolValue(), true ), (uint32_t) ovOp );
							if ( pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymLocal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							break;
					}
					break;
				case symbolSpaceScope::symClassStatic:
				case symbolSpaceScope::symGlobal:
				case symbolSpaceScope::symLocalStatic:
					switch ( node->getType ( sym ).compType() )
					{
						case symbolType::symInt:
							if ( !pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							codeSegment.putOp ( globalInt, sym->getIndex ( node->left->symbolValue(), true ), (uint32_t) ovOp );
							if ( pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							break;
						case symbolType::symDouble:
							if ( !pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							codeSegment.putOp ( globalDouble, sym->getIndex ( node->left->symbolValue(), true ), (uint32_t) ovOp );
							if ( pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							break;
						default:
							if ( !pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							codeSegment.putOp ( globalVar, sym->getIndex ( node->left->symbolValue(), true ), (uint32_t) ovOp);
							if ( pre )
							{
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
									sym->pushStack ( );
								}
							}
							break;
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		default:
			compEmitLValue ( funcDef, node->left, sym, needType, retType, safeCalls,  true );
			if ( needValue && !pre )
			{
				codeSegment.putOp ( fglOpcodes::dupv, (uint32_t) 1 );
				sym->pushStack ( );
				// convert back into an rvalue
				codeSegment.putOp ( fglOpcodes::deref, (uint32_t) 2 );
			}
			if ( isInc )
			{
				codeSegment.putOp ( fglOpcodes::incRef, (uint32_t) 0, (uint32_t) ovOp );
			} else
			{
				codeSegment.putOp ( fglOpcodes::decRef, (uint32_t) 0, (uint32_t) ovOp );
			}
			if ( !needValue || (needValue && !pre) )
			{
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
				sym->popStack ( );
			} else
			{
				codeSegment.putOp ( fglOpcodes::deref, (uint32_t) 1 );
			}
			break;
	}
	return node->getType ( sym );
}

class symbolTypeClass compExecutable::compEmitAssign ( opFunction *funcDef, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue )
{
	compClass					*classDef;
	compClassElementSearch		*elem;
	fgxClassElemSearchType		 searchType;
	cacheString					 symbolName;
	symbolTypeClass				 leftType = symVariantType;								// our return type... we'll override if we can be sure
	symbolTypeClass				 rightType;

	switch  ( node->left->getOp() )
	{
		case astOp::symbolPack:
			if ( !needValue && (node->right->getOp () == astOp::arrayValue) )
			{
				for ( size_t loop = 0; loop < node->left->arrayData().nodes.size (); loop++ )			// we use the LEFT size (the assignment) to determine the number of values to emit to be assigned
				{
					if ( loop < node->right->arrayData().nodes.size () )								// see if we have a value for this assignment
					{
						compEmitNode ( funcDef, node->right->arrayData().nodes[loop], sym, leftType, retType, safeCalls,  true );
					} else
					{
						if ( node->left->arrayData().nodes[loop]->getOp () != astOp::nilValue || node->left->arrayData ().nodes[loop]->getOp () != astOp::nullValue )		 // if it's not a nill or null than emit a null
						{
							codeSegment.putOp ( fglOpcodes::pushNull );
							sym->pushStack ();
						}
					}
				}
			} else
			{
				rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true );
				switch ( rightType.compType () )
				{
					case symbolType::symArray:
					case symbolType::symVArray:
					case symbolType::symVariant:
						break;
					default:
						throw errorNum::scINVALID_STORE_MULTI;
				}
				if ( needValue )
				{
					codeSegment.putOp ( fglOpcodes::storeMulti, (uint32_t)node->left->arrayData ().nodes.size () );
					sym->pushStack ( node->left->arrayData ().nodes.size () );
				} else
				{
					sym->popStack ();
					codeSegment.putOp ( fglOpcodes::storeMultiPop, (uint32_t)node->left->arrayData ().nodes.size () );
					sym->pushStack ( node->left->arrayData ().nodes.size () );
				}
			}
			for ( auto it = node->left->arrayData().nodes.rbegin(); it != node->left->arrayData().nodes.rend(); it++ )
			{
				astNode nilNode ( file->sCache, astOp::nilValue );
				astNode assignNode ( file->sCache, astOp::assign, (*it), &nilNode );

				if ( (*it) && ((*it)->getOp ( ) != astOp::nilValue && (*it)->getOp () != astOp::nullValue) )
				{
					compEmitAssign ( funcDef, &assignNode, sym, symVariantType, symVariantType, safeCalls, false );
				} else
				{
					codeSegment.putOp ( fglOpcodes::pop, (uint32_t ) 1 );
					sym->popStack ( );
				}
				assignNode.left = 0;
				assignNode.right = 0;
			}
			break;
		case astOp::symbolValue:
			// If we DONT'T have an lValue here then the very best we can do is to simply generate a variant store and hope that what we have is a reference.
			leftType = node->getType ( sym );
			switch ( sym->getScope ( node->left->symbolValue(), false ) )
			{
				case symbolSpaceScope::symLocal:
				case symbolSpaceScope::symLocalParam:
					switch ( leftType.compType() )
					{
						case symbolType::symObject:
							leftType = symVariantType;
							break;
						case symbolType::symVariant:
							break;
						default:
							if ( !sym->isAccessed ( node->left->symbolValue(), true ) )
							{
								// useless assignment... so no need emit of rhs, but we do have to try in case there are any side-effects
								rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  false ); 
								return rightType;
							}
							break;
					}
					rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true ); 
					if ( needValue )
					{
						switch ( leftType.compType() )
						{
							case symbolType::symVariant:
							case symbolType::symObject:
							case symbolType::symFunc:
								codeSegment.putOp ( fglOpcodes::storeLocalv, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
							default:
								compEmitCast ( rightType, leftType );
								codeSegment.putOp ( fglOpcodes::storeLocal, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
						}
					} else
					{
						switch ( leftType.compType () )
						{
							case symbolType::symVariant:
							case symbolType::symObject:
							case symbolType::symFunc:
								codeSegment.putOp ( fglOpcodes::storeLocalPopv, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
							default:
								codeSegment.putOp ( fglOpcodes::storeLocalPop, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
						}
						sym->popStack ( );
					}
					break;
				case symbolSpaceScope::symClassConst:
				case symbolSpaceScope::symLocalConst:
					{
						errorLocality e ( &sym->file->errHandler, node->left );
						throw errorNum::scASSIGN_TO_CONST;
					}
				case symbolSpaceScope::symClassStatic:
					rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true );
					compEmitCast ( rightType, leftType );
					if ( needValue )
					{
						if ( sym->isInterface ( node->symbolValue(), true ) )
						{
							if ( node->symbolValue().find ( "::" ) )
							{
								auto className = node->symbolValue().substr ( 0, node->symbolValue().find ( "::" ) );
								auto message = node->symbolValue().substr ( node->symbolValue().find ( "::" ) + 2 );
								codeSegment.putOp ( fglOpcodes::storeClassGlobal, dataSegment.addString ( className ), dataSegment.addString ( message ) );
							} else
							{
								throw errorNum::scINTERNAL;
							}
						} else
						{
							switch ( leftType.compType () )
							{
								case symbolType::symVariant:
								case symbolType::symObject:
									codeSegment.putOp ( fglOpcodes::storeGlobalv, sym->getIndex ( node->left->symbolValue(), true ) );
									break;
								default:
									codeSegment.putOp ( fglOpcodes::storeGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
									break;
							}
						}
					} else
					{
						if ( sym->isInterface ( node->symbolValue(), true ) )
						{
							if ( node->symbolValue().find ( "::" ) )
							{
								auto className = node->symbolValue().substr ( 0, node->symbolValue().find ( "::" ) );
								auto message = node->symbolValue().substr ( node->symbolValue().find ( "::" ) + 2 );
								codeSegment.putOp ( fglOpcodes::storeClassGlobalPop, dataSegment.addString ( className ), dataSegment.addString ( message ) );
							} else
							{
								throw errorNum::scINTERNAL;
							}
						} else
						{
							switch ( leftType.compType () )
							{
								case symbolType::symVariant:
								case symbolType::symObject:
									codeSegment.putOp ( fglOpcodes::storeGlobalPopv, sym->getIndex ( node->left->symbolValue(), true ) );
									break;
								default:
									codeSegment.putOp ( fglOpcodes::storeGlobalPop, sym->getIndex ( node->left->symbolValue(), true ) );
									break;
							}
						}
						sym->popStack ( );
					}
					break;
				case symbolSpaceScope::symGlobal:
				case symbolSpaceScope::symLocalStatic:
					rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true ); 
					compEmitCast ( rightType, leftType  );
					if ( needValue )
					{
						switch ( leftType.compType () )
						{
							case symbolType::symVariant:
							case symbolType::symObject:
								codeSegment.putOp ( fglOpcodes::storeGlobalv, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
							default:
								codeSegment.putOp ( fglOpcodes::storeGlobal, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
						}
					} else
					{
						switch ( leftType.compType () )
						{
							case symbolType::symVariant:
							case symbolType::symObject:
								codeSegment.putOp ( fglOpcodes::storeGlobalPopv, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
							default:
								codeSegment.putOp ( fglOpcodes::storeGlobalPop, sym->getIndex ( node->left->symbolValue(), true ) );
								break;
						}
						sym->popStack ( );
					}
					break;
				case symbolSpaceScope::symClassIVar:
					rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true ); 
					compEmitCast ( rightType, leftType  );
					if ( sym->isVirtual ( node->left->symbolValue(), true ) )
					{
						if ( needValue )
						{
							codeSegment.putOp ( fglOpcodes::storeClassVirtIVar, (int16_t) sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->left->symbolValue(), true ),0 );
						} else
						{
							codeSegment.putOp ( fglOpcodes::storeClassVirtIVarPop, (int16_t) sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
							sym->popStack ( );
						}
					} else
					{
						if ( needValue )
						{
							codeSegment.putOp ( fglOpcodes::storeClassIVar, (int16_t) sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
						} else
						{
							codeSegment.putOp ( fglOpcodes::storeClassIVarPop, (int16_t) sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->left->symbolValue(), true ), 0 );
							sym->popStack ( );
						}
					}
					break;
				case symbolSpaceScope::symClassAssign:
					elem = sym->findClass ( sym->getType ( file->selfValue, true ).getClass() )->getElement ( node->left->symbolValue() );
					// push value to assign
					rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
					// push context of assignment method
					compEmitContext ( sym, elem, sym->getIndex ( file->selfValue, true ), true, node->left->symbolValue().find ( "::" ) == stringi::npos );
					// call the code
					compEmitFuncCall ( funcDef, elem->assign.func->name, false, node, sym, needValue, 2, false, elem );
					break;
				default:
					{
						errorLocality e ( &sym->file->errHandler, node->left );
						throw errorNum::scILLEGAL_ASSIGNMENT;
					}
			}
			break;
		case astOp::sendMsg:
			if ( node->left->right->getOp() == astOp::nameValue )
			{
				// fastest mode - we're assigning to a local with a known type
				if ( node->left->left->getOp() == astOp::symbolValue && node->left->left->getType ( sym ).hasClass() && (classDef = sym->findClass ( node->left->left->getType ( sym ).getClass () )) )
				{
					if ( !strccmp ( node->left->left->symbolValue(), file->selfValue ) )
					{
						symbolName = file->selfValue;;
						searchType = fgxClassElemSearchType::fgxClassElemPrivateAssign;
					} else
					{
						symbolName = cacheString();
						switch ( sym->getScope ( node->left->left->symbolValue(), true ) )
						{
							case symbolSpaceScope::symLocal:
							case symbolSpaceScope::symLocalParam:
								symbolName = node->left->left->symbolValue();
								break;
							default:
								break;
						}
						searchType = fgxClassElemSearchType::fgxClassElemPublicAssign;
					}
					switch ( node->left->right->getOp() )
					{
						case astOp::nameValue:
							elem = classDef->getElement ( searchType, node->left->right->nameValue() );

							if ( elem )
							{
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_prop:
										if ( !elem->assign.func )
										{
											errorLocality e ( &sym->file->errHandler, node->left->right );
											throw errorNum::scUNKNOWN_IVAR;
										}
										// push value to assign
										rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 

										if ( symbolName )
										{
											// push context of assignment method
											compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, node->left->right->nameValue().find ( "::" ) == stringi::npos );
										} else
										{
											// emit the object
											compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls,  true );
											// adjust the context of the emitted object
											if ( elem->isVirtual )
											{
												codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->assign.data.vTabEntry, 0 );
											} else
											{
												if ( elem->assign.objectStart )
												{
													codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->assign.objectStart, 0 );
												}
											}
										}
										// call the code
										compEmitFuncCall ( funcDef, elem->assign.func->name, false, node, sym, needValue, 2, false, elem );
										break;
									case fgxClassElementType::fgxClassType_iVar:
										rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true ); 
										compEmitCast ( rightType, leftType  );
										if ( elem->isVirtual )
										{
											if ( symbolName )
											{
												if ( needValue )
												{
													codeSegment.putOp ( fglOpcodes::storeClassVirtIVar, sym->getIndex ( symbolName, true ), (int16_t)elem->methodAccess.data.vTabEntry, 0 );
												} else
												{
													codeSegment.putOp ( fglOpcodes::storeClassVirtIVarPop, sym->getIndex ( symbolName, true ), (int16_t)elem->methodAccess.data.vTabEntry, 0 );
													sym->popStack ( );
												}
											} else
											{
												// emit the object
												compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls,  true );
												if ( needValue )
												{
													codeSegment.putOp ( fglOpcodes::storeClassVirtIVarRel, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
												} else
												{
													codeSegment.putOp ( fglOpcodes::storeClassVirtIVarRelPop, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
													sym->popStack ( );
												}
											}
										} else
										{
											if ( symbolName )
											{
												if ( needValue )
												{
													codeSegment.putOp ( fglOpcodes::storeClassIVar, sym->getIndex ( symbolName, true ), (int16_t)elem->methodAccess.data.varIndex, 0 );
												} else
												{
													codeSegment.putOp ( fglOpcodes::storeClassIVarPop, sym->getIndex ( symbolName, true ), (int16_t)elem->methodAccess.data.varIndex, 0 );
													sym->popStack ( );
												}
											} else
											{
												// emit the object
												compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls,  true );
												if ( needValue )
												{
													codeSegment.putOp ( fglOpcodes::storeClassIVarRel, 1, elem->methodAccess.data.varIndex );
												} else
												{
													codeSegment.putOp ( fglOpcodes::storeClassIVarRelPop, 1, elem->methodAccess.data.varIndex );
													sym->popStack ( );
												}
											}
										}
										break;
									case fgxClassElementType::fgxClassType_static:
										rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true ); 
										compEmitCast ( rightType, leftType  );
										if ( needValue )
										{
											if ( classDef->isInterface )
											{
												codeSegment.putOp ( fglOpcodes::storeClassGlobal, dataSegment.addString ( classDef->name ), dataSegment.addString ( elem->name ) );
											} else
											{
												codeSegment.putOp ( fglOpcodes::storeGlobalv, sym->getIndex ( elem->symbolName, false ) );
											}
										} else
										{
											if ( classDef->isInterface )
											{
												codeSegment.putOp ( fglOpcodes::storeClassGlobalPop, dataSegment.addString ( classDef->name ), dataSegment.addString ( elem->name ) );
											} else
											{
												codeSegment.putOp ( fglOpcodes::storeGlobalPopv, sym->getIndex ( elem->symbolName, false ) );
											}
											sym->popStack ( );
										}
										break;
									default:
										throw errorNum::scILLEGAL_ASSIGNMENT;
								}
								return leftType;
							} else
							{
								if ( (elem = classDef->getDefaultAssign()) )
								{
									// push value to assign

									rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 

									// push the symbol name as a string parameter to the call
									compEmitNode ( funcDef, node->left->right, sym, symStringType, retType, safeCalls,  true ); 	// push message

									// push context of assignment method
									compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, node->left->right->nameValue().find ( "::" ) == stringi::npos );
									// call the code
									compEmitFuncCall ( funcDef, elem->assign.func->name, true, node, sym, needValue, 3, false, elem );
									return leftType;
								}
								throw errorNum::scUNKNOWN_IVAR;
							}
							return leftType;
						default:
							break;
					}
				}
 			} else if ( node->left->left->getType ( sym ).hasClass() && (classDef = sym->findClass ( node->left->left->getType ( sym ).getClass () )) )
			{
				switch ( node->left->right->getOp() )
				{
					case astOp::nameValue:
						elem = classDef->getElement ( fgxClassElemSearchType::fgxClassElemPublicAssign, node->left->right->nameValue() );

						if ( elem )
						{
							switch ( elem->type )
							{
								case fgxClassElementType::fgxClassType_prop:
									if ( !elem->assign.func ) throw errorNum::scINVALID_ACCESS;
									// push value to assign
									rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
									// emit the object
									compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls,  true );
									// adjust the context of the emitted object
									if ( elem->isVirtual )
									{
										codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->assign.data.vTabEntry, 0 );
									} else
									{
										if ( elem->assign.objectStart )
										{
											codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->assign.objectStart, 0 );
										}
									}
									// call the code
									compEmitFuncCall ( funcDef, elem->assign.func->name, false, node, sym, needValue, 2, false, elem );
									break;
								case fgxClassElementType::fgxClassType_iVar:
									rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true ); 
									compEmitCast ( rightType, leftType  );

									// emit the object
									compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls,  true );

									if ( needValue )
									{
										codeSegment.putOp ( fglOpcodes::storeClassIVarRel, 1, elem->methodAccess.data.varIndex );
										sym->popStack ( );
									} else
									{
										codeSegment.putOp ( fglOpcodes::storeClassIVarRelPop, 1, elem->methodAccess.data.varIndex );
										sym->popStack ( 2 );
									}
									break;
								case fgxClassElementType::fgxClassType_static:
									rightType = compEmitNode ( funcDef, node->right, sym, leftType, retType, safeCalls,  true ); 
									compEmitCast ( rightType, leftType  );
									if ( needValue )
									{
										if ( classDef->isInterface )
										{
											codeSegment.putOp ( fglOpcodes::storeClassGlobal, dataSegment.addString ( classDef->name ), dataSegment.addString ( elem->name ) );
										} else
										{
											codeSegment.putOp ( fglOpcodes::storeGlobalv, sym->getIndex ( elem->symbolName, false ) );
										}
									} else
									{
										if ( classDef->isInterface )
										{
											codeSegment.putOp ( fglOpcodes::storeClassGlobalPop, dataSegment.addString ( classDef->name ), dataSegment.addString ( elem->name ) );
										} else
										{
											codeSegment.putOp ( fglOpcodes::storeGlobalPopv, sym->getIndex ( elem->symbolName, false ));
										}
										sym->popStack ( );
									}
									break;
								default:
									throw errorNum::scILLEGAL_ASSIGNMENT;
							}
							return leftType;
						} else
						{
							if ( (elem = classDef->getDefaultAssign()) )
							{
								// push value to assign
								rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 

								// push the symbol name as a string parameter to the call
								compEmitNode ( funcDef, node->left->right, sym, symStringType, retType, safeCalls,  true ); 	// push message

								// emit the object
								compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls,  true );

								// adjust the context of the emitted object
								if ( elem->isVirtual )
								{
									codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->assign.data.vTabEntry, 0 );
								} else
								{
									if ( elem->assign.objectStart )
									{
										codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->assign.objectStart, 0 );
									}
								}
								// call the code
								compEmitFuncCall ( funcDef, elem->assign.func->name, true, node, sym, needValue, 3, false, elem );
								return leftType;
							}
							throw errorNum::scILLEGAL_ASSIGNMENT;
						}
						return leftType;
					default:
						break;
				}
			}
			// this isn't really a method thing... we want to generate an access to the ivar but then the actual assignment is a basic store (hopefully to reference)
			compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true );		// push rvalue
			switch ( node->left->right->getOp() )
			{
				case astOp::nameValue:
					compEmitNode ( funcDef, node->left->left, sym, needType, retType, safeCalls,  true ); 	
					if ( !needValue )
					{
						// either we don't need the value, or we're a post operation so we've already duplicated the needed value and need to dispose of this one
						codeSegment.putOp ( fglOpcodes::objStorePop, dataSegment.addString ( node->left->right->nameValue() ), bssSize );
						sym->popStack ( 2 );
					} else
					{
						codeSegment.putOp ( fglOpcodes::objStore, dataSegment.addString ( node->left->right->nameValue() ), bssSize );
						sym->popStack ( );
					}
					bssSize += sizeof ( bcObjQuick );
					break;
				default:
					{
						auto rType = compEmitNode ( funcDef, node->left->right, sym, symVariantType, retType, safeCalls,  true );
						switch ( rType.compType ( ) )
						{
							case symbolType::symVariant:
							case symbolType::symString:
								break;
							default:
								throw errorNum::scINVALID_MESSAGE;
						}
						auto lType = compEmitNode ( funcDef, node->left->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
						switch ( lType.compType ( ) )
						{
							case symbolType::symVariant:
							case symbolType::symObject:
								break;
							default:
								throw errorNum::scINVALID_MESSAGE;
						}
					}
					if ( !needValue )
					{
						// either we don't need the value, or we're a post operation so we've already duplicated the needed value and need to dispose of this one
						codeSegment.putOp ( fglOpcodes::objStoreIndPop, (uint32_t) 0 );
						sym->popStack ( 3 );
					} else
					{
						codeSegment.putOp ( fglOpcodes::objStoreInd, (uint32_t) 0 );
						sym->popStack ( 2 );
					}
					break;
			}
			break;
		case astOp::arrayDeref:
			switch ( node->left->left->getType ( sym).compType() )
			{
				case symbolType::symArray:
					if ( (node->left->pList().param.size() == 1) && (node->left->pList().param[0]->getType ( sym ) == symIntType ) )
					{
						// push value
						compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
						// push array
						compEmitNode ( funcDef, node->left->left, sym, symArrayType, retType, safeCalls,  true );
						// emit value
						compEmitNode ( funcDef, node->left->pList().param[0], sym, symIntType, retType, safeCalls,  true );
						// special case for fixed array defreferencing
						codeSegment.putOp ( fglOpcodes::arrFixedDerefRef );
						sym->popStack ( );
						if ( needValue )
						{
							codeSegment.putOp ( fglOpcodes::store );
							sym->popStack ( );
						} else
						{
							codeSegment.putOp ( fglOpcodes::storePop );
							sym->popStack ( 2 );
						}
						break;
					}
					[[fallthrough]];
				case symbolType::symVArray:
				case symbolType::symObject:
				case symbolType::symVariant:
					if ( node->left->pList().param.size() == 1 )
					{
						// push value
						compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
						// push index
						compEmitNode ( funcDef, node->left->pList().param[0], sym, symVariantType, retType, safeCalls,  true ); 
						// push object - may require intermediary array dereferences
						compEmitLValue ( funcDef, node->left->left, sym , symVariantType, retType, safeCalls,  true );
						if ( needValue )
						{
							codeSegment.putOp ( fglOpcodes::storeArray );
							sym->popStack ( 2 );
						} else
						{
							codeSegment.putOp ( fglOpcodes::storeArrayPop );
							sym->popStack ( 3 );
						}
					} else
					{
						// push value
						compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
						// push object - may require intermediary array dereferences
						compEmitLValue ( funcDef, node->left->left, sym , symVariantType, retType, safeCalls,  true );
						for ( size_t index = 0; index < node->left->pList().param.size(); index++ )
						{
							compEmitNode ( funcDef, node->left->pList().param[index], sym, symVariantType, retType, safeCalls,  true ); 
							codeSegment.putOp ( fglOpcodes::arrDerefRef );
							sym->popStack ( );
						}
						if ( needValue )
						{
							codeSegment.putOp ( fglOpcodes::store );
							sym->popStack ( );
						} else
						{
							codeSegment.putOp ( fglOpcodes::storePop );
							sym->popStack ( 2 );
						}
					}
					break;
				default:
					{
						errorLocality e ( &sym->file->errHandler, node->left->left );
						throw errorNum::scILLEGAL_OPERAND;
					}
			}
			break;
		case astOp::workAreaStart:
			switch ( node->left->right->getCompType ( sym ) )
			{
				case symbolType::symString:
					break;
				default:
					{
						errorLocality e ( &sym->file->errHandler, node->left->right );
						throw errorNum::scINVALID_FIELD;
					}
					break;
			}
			compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 		// RHS
			// below is LHS
			switch ( node->left->left->getOp() )
			{
				case astOp::symbolValue:
					if (  node->left->left->symbolValue() == file->fieldValue ) 
					{
						// special case for default workarea field access
						switch ( node->left->right->getOp() )
						{
							case astOp::stringValue:
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
									sym->popStack ( );
								}
								break;
							default:
								compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
									sym->popStack ( );
								}
								break;
						}
					} else
					{
						codeSegment.putOp ( fglOpcodes::waStarts, dataSegment.addString ( node->left->left->symbolValue() ) );						
						switch ( node->left->right->getOp() )
						{
							case astOp::stringValue:
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
									sym->popStack ( );
								}
								codeSegment.putOp ( fglOpcodes::waEnd );
								break;
							default:
								compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
									sym->popStack ( );
								}
								codeSegment.putOp ( fglOpcodes::waEnd );
								break;
						}
					}
					break;
				case astOp::intValue:
					codeSegment.putOp ( fglOpcodes::waStarti, (uint32_t) node->left->left->intValue() );						
					switch ( node->left->right->getOp() )
					{
						case astOp::stringValue:
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
								sym->popStack ( );
							}
							break;
						default:
							compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
								sym->popStack ( );
							}
							break;
					}
					codeSegment.putOp ( fglOpcodes::waEnd );
					break;
				default:
					rightType = compEmitNode ( funcDef, node->left->left, sym, symVariantType, retType, safeCalls,  true ); 
					switch ( rightType.compType() )
					{
						case symbolType::symString:
							codeSegment.putOp ( fglOpcodes::waStartInds );
							break;
						case symbolType::symInt:
							codeSegment.putOp ( fglOpcodes::waStartIndi );
							break;
						case symbolType::symVariant:
							codeSegment.putOp ( fglOpcodes::waStartInd );
							break;
						default:
							{
								errorLocality e ( &sym->file->errHandler, node->left->left );
								throw errorNum::scINVALID_WORKAREA;
							}
							break;
					}
					sym->popStack ( );
					switch ( node->left->right->getOp() )
					{
						case astOp::stringValue:
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
								sym->popStack ( );
							}
							break;
						default:
							compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
								sym->popStack ( );
							}
							break;
					}
					codeSegment.putOp ( fglOpcodes::waEnd );
					break;
			}
			break;
		default:
			// generic... storage into a non-local symbol
			if ( (symbolType) node->left->getType ( sym  ) != symVariantType )
			{
				// we KNOW that this will fail... at this point we can be assured that we don't have a variant
				// as an lValue, nor is it a symbol, and therfore we're doing rValue = rValue
				throw errorNum::scILLEGAL_ASSIGNMENT;
			}
			// value
			compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
			// object
			compEmitLValue ( funcDef, node->left,  sym, symVariantType, retType, safeCalls,  true ); 

			if ( needValue )
			{
				codeSegment.putOp ( fglOpcodes::store );
				sym->popStack ( );
			} else
			{
				codeSegment.putOp ( fglOpcodes::storePop );
				sym->popStack ( 2 );
			}
	}
	return leftType;
}

class symbolTypeClass compExecutable::compEmitOpAssign ( opFunction *funcDef, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, fglOpcodes opCode, fglOpcodes opLocalV, fglOpcodes opGlobalv, fglOpcodes opClassIvar, fglOpcodes opClassVirtIVar, fglOpcodes opRefv, fgxOvOp ovOp )
{
	symbolTypeClass				 leftType;
	symbolTypeClass				 rightType;

	switch  ( node->left->getOp() )
	{
		case astOp::symbolValue:
			// here's where the magic happens...   this ENSURES that we have an lValue for the assignment and can drastically
			// optimize assignments.  If we DONT'T have an lValue here then the very best we can do is to simply generate
			// a variant store and hope that what we have is a reference.

			leftType = node->left->getType ( sym );
			switch ( sym->getScope ( node->left->symbolValue(), false ) )
			{
				case symbolSpaceScope::symLocal:
				case symbolSpaceScope::symLocalParam:
					switch ( leftType.compType() )
					{
						case symbolType::symObject:
						case symbolType::symVariant:
							break;
						default:
							if ( !sym->isAccessed ( node->left->symbolValue(), true ) )
							{
								// useless assignment... so just do a no need emit of rhs
								rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  false ); 
								return node->left->getType ( sym );
							}
							break;
					}
					rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true );
					switch ( leftType )
					{
						case symbolType::symVariant:
							compEmitCast ( rightType, leftType );
							break;
						case symbolType::symObject:		// this is an object... no casting since this will be handled in the op
						default:
							break;
					}
					codeSegment.putOp ( opLocalV, sym->getIndex ( node->left->symbolValue(), true ), needValue ? 0 : 1 );
					if ( !needValue )
					{
						sym->popStack ( );
					}
					break;
				case symbolSpaceScope::symClassAssign:
					{
						errorLocality e ( &sym->file->errHandler, node->left );
						throw errorNum::scNOT_ALLOWED_WITH_PROP_METHODS;
					}
				case symbolSpaceScope::symClassConst:
				case symbolSpaceScope::symLocalConst:
					{
						errorLocality e ( &sym->file->errHandler, node->left );
						throw errorNum::scASSIGN_TO_CONST;
					}
				case symbolSpaceScope::symGlobal:
				case symbolSpaceScope::symLocalStatic:
					rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
					codeSegment.putOp ( opGlobalv, sym->getIndex ( node->left->symbolValue(), true ), needValue ? 0 : 1 );
					if ( !needValue )
					{
						sym->popStack ( );
					}
					break;
				case symbolSpaceScope::symClassStatic:
					compEmitLValue ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true );
					compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true );
					codeSegment.putOp ( opRefv, 0, needValue ? 1 : 2 );
					sym->popStack ( );
					if ( !needValue )
					{
						sym->popStack ( );
					}
					break;

				case symbolSpaceScope::symClassIVar:
					rightType = compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
					compEmitCast ( rightType, leftType  );
					codeSegment.putOp ( opClassIvar, (uint16_t)sym->getIndex ( file->selfValue, true ), (int16_t) sym->getObjectOffset ( node->left->symbolValue(), true ), (uint16_t)sym->getIndex ( node->left->symbolValue(), true ) );
					if ( !needValue )
					{
						codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
						sym->popStack ( );
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case astOp::workAreaStart:
			// convert <workarea> <op>= <rhs> into  <workarea> = <workarea> <op> <rhs>
			compEmitNode ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true ); 
			compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true ); 
			codeSegment.putOp ( opCode );
			rightType = symVariantType;

			// now emit the LHS and assignement
			switch ( node->left->right->getOp() )
			{
				case astOp::intValue:
				case astOp::doubleValue:
				case astOp::varrayValue:
				case astOp::arrayValue:
				case astOp::symbolPack:
				case astOp::codeBlockValue:
				case astOp::atomValue:
				case astOp::nullValue:
				case astOp::nilValue:
				case astOp::fieldValue:
				case astOp::funcValue:
				case astOp::pairValue:
					{
						errorLocality e ( &sym->file->errHandler, node->left->right );
						throw errorNum::scINVALID_FIELD;
					}
				default:
					break;
			}
			switch ( node->left->left->getOp() )
			{
				case astOp::symbolValue:
					if ( !strccmp ( node->left->left->symbolValue(), "field" ) )
					{
						// special case for default workarea field access
						switch ( node->left->right->getOp() )
						{
							case astOp::stringValue:
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
									sym->popStack ( );
								}
								break;
							default:
								compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
									sym->popStack ( );
								}
								break;
						}
					} else
					{
						codeSegment.putOp ( fglOpcodes::waStarts, dataSegment.addString ( node->left->left->symbolValue() ) );						
						switch ( node->left->right->getOp() )
						{
							case astOp::stringValue:
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
									sym->popStack ( );
								}
								break;
							default:
								compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
								} else
								{
									codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
									sym->popStack ( );
								}
								break;
						}
						codeSegment.putOp ( fglOpcodes::waEnd );
					}
					break;
				case astOp::intValue:
					codeSegment.putOp ( fglOpcodes::waStarti, (uint32_t) node->left->left->intValue() );						
					switch ( node->left->right->getOp() )
					{
						case astOp::stringValue:
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
								sym->popStack ( );
							}
							break;
						default:
							compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
								sym->popStack ( );
							}
							break;
					}
					codeSegment.putOp ( fglOpcodes::waEnd );
					break;
				default:
					leftType = compEmitNode ( funcDef, node->left->left, sym, needType, retType, safeCalls,  true ); 
					switch ( leftType.compType() )
					{
						case symbolType::symString:
							codeSegment.putOp ( fglOpcodes::waStartInds );
							break;
						case symbolType::symInt:
							codeSegment.putOp ( fglOpcodes::waStartIndi );
							break;
						case symbolType::symVariant:
							codeSegment.putOp ( fglOpcodes::waStartInd );
							break;
						default:
							{
								errorLocality e ( &sym->file->errHandler, node->left->left );
								throw errorNum::scINVALID_WORKAREA;
							}
					}
					sym->popStack ( );
					switch ( node->left->right->getOp() )
					{
						case astOp::stringValue:
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreField, dataSegment.addString ( node->left->right->stringValue() ) );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldPop, dataSegment.addString ( node->left->right->stringValue() ) );
								sym->popStack ( );
							}
							break;
						default:
							compEmitNode ( funcDef, node->left->right, sym, needType, retType, safeCalls,  true ); 
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldInd );
							} else
							{
								codeSegment.putOp ( fglOpcodes::waStoreFieldIndPop );
								sym->popStack ( );
							}
							break;
					}
					codeSegment.putOp ( fglOpcodes::waEnd );
					break;
			}
			break;

			/*
					a.b += c

					in this case, if b has assign and access members, we do NOT break this up into a.b = a.b + c
					what is done is that the accessor is called and MUST return a reference (or error is thrown) at which point the reference is operated on

					breaking it up becomes an ambiguity

					if a.b returns an object we would then have 

					a.b + c (+ operator overload on b)
					and then a.b =  (assign operator overload)

					this is not what the programmer specified (+= operator)
			*/
		case astOp::sendMsg:
		default:
			compEmitLValue ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true );
			compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true );
			leftType = symbolType::symVariant;
			codeSegment.putOp ( opRefv, 0, needValue ? 1 : 2 );
			sym->popStack ( );
			if ( !needValue )
			{
				sym->popStack ( );
			}
			break;
	}
	return node->left->getType ( sym );
}

bool hasParamPackCall ( astNode *node )
{
	if ( node->getOp() != astOp::funcCall ) return false;
	for ( auto it = node->pList().param.begin(); it != node->pList().param.end(); it++ )
	{
		if ( (*it) && (*it)->getOp() == astOp::paramExpand )
		{
			return true;
		}
	}
	return false;
}

void compExecutable::compEmitFuncCall ( opFunction *funcDef, cacheString const &name, bool isAccess, astNode *node, symbolStack *sym, bool needValue, uint32_t nParamsSent, bool tailCall, compClassElementSearch *elem )
{
	if ( sym->getFuncNumParams ( sym->getFuncName ( name, true ), true ) > (int32_t) nParamsSent )
	{
		this->file->errHandler.throwWarning ( false, warnNum::scwINCORRECT_NUM_PARAMS, "" );
	}
	if ( elem && elem->isVirtual )
	{
		if ( sym->isFuncBC ( name, isAccess ) )
		{
			if ( needValue )
			{
				codeSegment.putOp ( fglOpcodes::callBCVirt, elem->methodAccess.data.vTabEntry, nParamsSent );
				sym->popStack ( nParamsSent );
				sym->pushStack ( );
			} else
			{
				codeSegment.putOp ( fglOpcodes::callBCVirtPop, 1, (int16_t)elem->methodAccess.data.vTabEntry, nParamsSent );
				sym->popStack ( nParamsSent );
			}
		} else if ( sym->isFuncC ( name, isAccess ) )
		{
			if ( needValue )
			{
				codeSegment.putOp ( fglOpcodes::callCVirt, elem->methodAccess.data.vTabEntry, nParamsSent );
				sym->popStack ( nParamsSent );
				sym->pushStack ( );
			} else
			{
				codeSegment.putOp ( fglOpcodes::callCVirtPop, 1, (int16_t)elem->methodAccess.data.vTabEntry, nParamsSent );
				sym->popStack ( nParamsSent );
			}
		} else
		{
			throw errorNum::scINTERNAL;
		}
	} else
	{
		if ( sym->isFuncBC ( name, isAccess ) )
		{
			if ( needValue )
			{
				if ( tailCall )
				{
					if ( genProfiling ) codeSegment.putOp ( fglOpcodes::profFuncEnd );
					atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( sym->getFuncName ( name, isAccess ), atomListType::atomRef ), codeSegment.putOp ( fglOpcodes::callBCTail, (uint32_t) 0 , nParamsSent ) );
					sym->popStack ( nParamsSent );
					sym->pushStack ( );
				} else
				{
					atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( sym->getFuncName ( name, isAccess ), atomListType::atomRef ), codeSegment.putOp ( fglOpcodes::callBC, (uint32_t) 0 , nParamsSent ) );
					sym->popStack ( nParamsSent );
					sym->pushStack ( );
				}
			} else
			{
				atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( sym->getFuncName ( name, isAccess ), atomListType::atomRef ), codeSegment.putOp ( fglOpcodes::callBCPop, (uint32_t) 0 , nParamsSent ) );
				sym->popStack ( nParamsSent );
			}
		} else if ( sym->isFuncC ( name, isAccess ) )
		{
			if ( needValue )
			{
				atom.addFixup ( atom.add ( sym->getFuncName ( name, isAccess ), atomListType::atomRef ), codeSegment.putOp ( fglOpcodes::callC, (uint32_t) 0 , nParamsSent ) );
				sym->popStack ( nParamsSent );
				sym->pushStack ( );
			} else
			{
				atom.addFixup ( atom.add ( sym->getFuncName ( name, isAccess ), atomListType::atomRef ), codeSegment.putOp ( fglOpcodes::callCPop, (uint32_t) 0 , nParamsSent ) );
				sym->popStack ( nParamsSent );
			}
		} else
		{
			throw errorNum::scINTERNAL;
		}
	}
}

int32_t compExecutable::compEmitFuncParameters ( opFunction *funcDef, cacheString const &name, bool isAccess, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, int32_t nParamsNeeded, int32_t nParamsSent, int32_t numHidden, cacheString const &funcName, bool safeCalls )
{
	astNode				*newNode;
	int32_t				 nodeIndex;
	symbolTypeClass		 pType;

	if ( nParamsSent >= nParamsNeeded - numHidden )
	{
		// correct number of parameters
		for ( nodeIndex = nParamsSent; nodeIndex; nodeIndex-- )
		{
			if ( node->pList().param[(size_t)nodeIndex - 1] )
			{
				// push the callers value
				if ( sym->isVariantParam ( funcName, isAccess ) && (nParamsSent + numHidden - 1 >= nodeIndex) )
				{
					pType = compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
				} else
				{
					pType = compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ), retType, safeCalls,  true );
					compEmitCast ( pType, sym->getFuncParamType ( funcName, isAccess, static_cast<int32_t>(static_cast<size_t>(nodeIndex) - 1 + numHidden), accessorType(), nullptr ) );
				}
			} else
			{
				newNode = sym->getFuncDefaultParam ( funcName, isAccess, nodeIndex - 1 + numHidden );
				if ( newNode )
				{
					// no parameter, but we have a default so use that
					pType = compEmitNode ( funcDef, newNode->right, sym, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ), retType, safeCalls,  true );
					compEmitCast ( pType, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ).compType() );
				} else
				{
					if ( !funcDef->isFGL )
					{
						file->errHandler.throwWarning ( false, warnNum::scwINCORRECT_NULL_PARAM, 0 );
					}
					// no parameter, no default, push a NULL
					switch ( sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ).compType() )
					{
						case symbolType::symInt:
							codeSegment.putOp ( fglOpcodes::pushNumLong, (int64_t)0 );
							break;
						case symbolType::symDouble:
							codeSegment.putOp ( fglOpcodes::pushNumDouble, 0.0 );
							break;
						case symbolType::symString:
							codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( "" ) );
							break;
						case symbolType::symArray:
						case symbolType::symVArray:
							codeSegment.putOp ( fglOpcodes::pushNullArray );
							break;
						case symbolType::symVariant:
						default:
							codeSegment.putOp ( fglOpcodes::pushNull );
							break;
					}
					sym->pushStack ( );
				}
			}
		}
		return nParamsSent + numHidden;
	} else 
	{
		for ( nodeIndex = nParamsNeeded; nodeIndex > nParamsSent + numHidden; nodeIndex-- )
		{
			newNode = sym->getFuncDefaultParam ( funcName, isAccess, nodeIndex - 1 );
			if ( newNode->getOp () == astOp::assign )
			{
				// emit the default value1
				compEmitNode ( funcDef, newNode->right, sym, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1, accessorType(), nullptr ), retType, safeCalls,  true );
			} else
			{
				switch ( sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1, accessorType(), nullptr ).compType() )
				{
					case symbolType::symInt:
						codeSegment.putOp ( fglOpcodes::pushNumLong, (int64_t)0 );
						break;
					case symbolType::symDouble:
						codeSegment.putOp ( fglOpcodes::pushNumDouble, 0.0 );
						break;
					case symbolType::symString:
						codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( "" ) );
						break;
					case symbolType::symArray:
					case symbolType::symVArray:
						codeSegment.putOp ( fglOpcodes::pushNullArray );
						break;
					case symbolType::symVariant:
					default:
						codeSegment.putOp ( fglOpcodes::pushNull );
						break;
				}
				sym->pushStack ( );
			}
		}

		// normalized passed parameters
		for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
		{
			if ( node->pList().param[(size_t)nodeIndex - 1] )
			{
				// push the callers value
				pType = compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ).compType(), retType, safeCalls,  true );
				compEmitCast ( pType, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ).compType() );
			} else
			{
				newNode = sym->getFuncDefaultParam ( funcName, isAccess, nodeIndex - 1 + numHidden );
				if ( newNode->getOp () == astOp::assign )
				{
					// no parameter, but we have a default so use that
					pType = compEmitNode ( funcDef, newNode->right, sym, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ).compType(), retType, safeCalls,  true );
					compEmitCast ( pType, sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ).compType() );
				} else
				{
					// no parameter, no default, push a NULL
					switch ( sym->getFuncParamType ( funcName, isAccess, nodeIndex - 1 + numHidden, accessorType(), nullptr ).compType() )
					{
						case symbolType::symInt:
							codeSegment.putOp ( fglOpcodes::pushNumLong, (int64_t)0 );
							break;
						case symbolType::symDouble:
							codeSegment.putOp ( fglOpcodes::pushNumDouble, 0.0 );
							break;
						case symbolType::symString:
							codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( "" ) );
							break;
						case symbolType::symArray:
						case symbolType::symVArray:
							codeSegment.putOp ( fglOpcodes::pushNullArray );
							break;
						case symbolType::symVariant:
						default:
							codeSegment.putOp ( fglOpcodes::pushNull );
							break;
					}
					sym->pushStack ( );
				}
			}
		}
		return nParamsNeeded;
	}
}

class symbolTypeClass compExecutable::compEmitFunc( opFunction *funcDef, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, bool tailCall )
{
	int32_t								 nodeIndex;
	int32_t								 nParamsNeeded;
	int32_t							 	 nParamsSent;
	astNode								*newNode;
	cacheString							 funcName;
	cacheString							 symbolName;
	compClass							*classDef;
	compClassElementSearch				*elem;
	symbolTypeClass						 ret;
	cacheString							 tmpLabel1;

	if ( !needValue )
	{
		ret = symVariantType;
	}
	switch ( node->left->getOp() )
	{
		case astOp::atomValue:
			if ( node->left->symbolValue() == file->debugBreakValue )
			{
				codeSegment.putOp ( fglOpcodes::debugBreak );
				if ( needValue )
				{
					throw errorNum::scNO_RETURN_VALUE;
				}
			} else if ( node->left->symbolValue() == file->getEnumeratorValue )
			{
				if ( node->pList().param.size() != 1 )
				{
					errorLocality e ( &sym->file->errHandler, node );
					throw errorNum::scINVALID_PARAMETER;
				}
				compEmitNode ( funcDef, node->pList().param[0], sym, symVariantType, symVariantType, safeCalls, true );
				codeSegment.putOp ( fglOpcodes::makeEnumerator );
				if ( !needValue )
				{
					codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
					sym->popStack ( );
				} else
				{
					ret = symWeakObjectType;
				}
			} else if ( node->left->symbolValue() == file->newValue )
			{
				// new object creation - push parameter's FIRST!!   left most parameter is the name of the class to create
				switch ( node->pList().param[0]->getOp() )
				{
					case astOp::nameValue:
						{
							auto cls = sym->findClass ( node->pList().param[0]->nameValue() );
							if ( cls )
							{
								if ( cls->newIndex )
								{
									astNode newNode = *node;
									auto dstFunc = cls->elements[static_cast<size_t>(cls->newIndex) - 1].methodAccess.func;
									delete *newNode.pList ().param.begin ();
									newNode.pList().param.erase ( newNode.pList().param.begin ( ) );
									if ( !newNode.pList ().paramRegion.empty() )
									{
										newNode.pList ().paramRegion.erase ( newNode.pList ().paramRegion.begin () );
									}
									nParamsNeeded = sym->getFuncNumParams ( dstFunc->name, true );
									nParamsSent = (int32_t) newNode.pList().param.size ( );

									compEmitFuncParameters ( funcDef, dstFunc->name, true, &newNode, sym, needType, retType, nParamsNeeded, nParamsSent, 1, dstFunc->name, safeCalls );

									// nParamsNeeded - 1 is becasue we don't "need" the self object as that's createed by objConstruct
									nParamsSent = nParamsSent < nParamsNeeded - 1 ? nParamsNeeded - 1 : nParamsSent;

									atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( cls->name, atomListType::atomClass ), codeSegment.putOp ( fglOpcodes::objConstruct, (uint32_t) 0, nParamsSent ) );
									sym->popStack ( nParamsSent );
									sym->pushStack ( );
								} else
								{
									if ( node->pList().param.size ( ) > 1 )
									{
										throw errorNum::scNO_CONSTRUCTOR;
									}
									atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( cls->name, atomListType::atomClass ), codeSegment.putOp ( fglOpcodes::objConstruct, (uint32_t) 0, 0 ) );
									sym->pushStack ( );
								}
								if ( !needValue )
								{
									sym->popStack ( );
								} else
								{
									ret = symbolTypeClass ( symbolType::symWeakObject, node->pList().param[0]->symbolValue() );
								}
								break;
							}
						}
						[[fallthrough]];
					default:
						if ( node->pList().param.size() && node->pList().param[0] )
						{
							compEmitNode( funcDef, node->pList().param[0], sym, symStringType, retType, safeCalls,  true );
						} else
						{
							errorLocality e ( &sym->file->errHandler, node );
							throw errorNum::scINVALID_PARAMETER;
						}

						tmpLabel1 = makeLabel ( "obj_new%" );

						// create the object
						if ( genProfiling ) codeSegment.putOp( fglOpcodes::profCallStart );
						codeSegment.putOp( fglOpcodes::prefix, bssSize );
						bssSize += sizeof( bcObjQuick );
						fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::objConstructV, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) - 1 ) );

						// push the rest of the parameters
						for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex - 1; nodeIndex-- )
						{
							if ( node->pList().param[(size_t)nodeIndex - 1] )
							{
								// push the callers value
								compEmitNode( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
							} else
							{
								// try to find the def value code (this is a soft link so we don't have it now)
								codeSegment.putOp( fglOpcodes::callDefValue, (uint32_t)nodeIndex );
								sym->pushStack ( );
							}
						}
						// parameter return (we don't now what the constructor requirements are at compile time
						codeSegment.putOp ( fglOpcodes::pRet );
						codeSegment.emitLabel ( tmpLabel1 );
						fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
						listing.label ( sym, tmpLabel1 );
						sym->popStack ( node->pList().param.size ( ) );
						sym->pushStack ( );

						if ( genProfiling ) codeSegment.putOp( fglOpcodes::profCallEnd );

						// we're never going to inline a new (requires native and byte code).   so we just emit away knowing that we'll never be inlined and resize at the end
						if ( !needValue )
						{
							codeSegment.putOp( fglOpcodes::pop, (uint32_t)1 );
							sym->popStack ( );
						} else
						{
							ret = symWeakObjectType;
						}
						break;
				}
			} else
			{
				// emit function or method call
				nParamsNeeded = sym->getFuncNumParams( node->left->symbolValue(), true );
				nParamsSent = (int32_t)node->pList().param.size();
				if ( ((int) nParamsNeeded >= 0) )
				{
					if ( sym->isProperty ( node->left->symbolValue(), true ) )
					{
						throw errorNum::scNOT_A_FUNCTION;
					}
					if ( !needValue && sym->isConst ( node->left->symbolValue(), true ) )
					{
						// TODO:   move this into dead-code elimination
						// don't need the value and the function call has no side effects
						for ( nodeIndex = (int) node->pList().param.size (); nodeIndex; nodeIndex-- )
						{
							if ( node->pList().param[(size_t)nodeIndex - 1] && (node->pList().param[(size_t)nodeIndex - 1]->getOp() == astOp::refCreate) )
							{
								break;
							}
						}

						// even though a const function call is const, it MAY not actually be const if it's passed in a reference parameter.  if it is then we don't want to eliminate the call.
						if ( !nodeIndex )
						{
							for ( nodeIndex = (int) node->pList().param.size (); nodeIndex; nodeIndex-- )
							{
								if ( node->pList().param[(size_t)nodeIndex - 1] )
								{
									// emit any parameter, but with need as false so we just handle anything with side effects
									compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  false );
								}
							}
							ret = symVariantType;
							break;
						}
					}
					if ( hasParamPackCall( node ) )
					{
						// we found the function and know how to call it!
						// no prep is needed as we already know everything!

						if ( sym->isMethod( node->left->symbolValue(), true ) )
						{
							funcName = sym->getFuncName( node->left->symbolValue(), true );

							atom.addFixup( atom.add( funcName, atomListType::atomRef ), codeSegment.putOp( fglOpcodes::pushAtom, (uint32_t)0 ) );
							sym->pushStack ( );
							tmpLabel1 = makeLabel ( "func_call%" );

							if ( needValue )
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPack, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) ) );
							} else
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackPop, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) ) );
							}
							for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
							{
								if ( node->pList().param[(size_t)nodeIndex - 1] )
								{
									// push the callers value
									compEmitNode( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
								} else
								{
									newNode = sym->getFuncDefaultParam( node->left->symbolValue(), true, nodeIndex - 1 );
									if ( newNode )
									{
										// no parameter, but we have a default so use that
										compEmitNode( funcDef, newNode, sym, needType, retType, safeCalls,  true );
									} else
									{
										// no parameter, no default, push a NULL
										codeSegment.putOp( fglOpcodes::pushNull );
										sym->pushStack ( );
									}							
								}
							}
							// push the context
							compEmitContext ( sym, node->left->symbolValue(), sym->getIndex ( file->selfValue, true ), true, node->left->symbolValue().find ( "::" ) == stringi::npos );
							if ( node->pList().param.size() )
							{
								codeSegment.putOp ( fglOpcodes::pRet );
							}

							sym->popStack ( ); // pop off context;
							sym->popStack ( node->pList().param.size ( ) );
							sym->popStack ( ); // pop off atom;
							codeSegment.emitLabel ( tmpLabel1 );
							fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
							listing.label ( sym, tmpLabel1 );
						} else
						{
							if ( sym->isExtension ( node->left->symbolValue(), true ) )
							{
								throw errorNum::scEXTENSION_NOT_CLASS_MEMBER;
							}

							// emit callPPack
							ret = symVariantType;
							compEmitNode( funcDef, node->left, sym, needType, retType, safeCalls,  true );
							tmpLabel1 = makeLabel ( "func_call_ppack%" );

							if ( needValue )
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPack, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) ) );
							} else
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackPop, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) ) );
							}
							for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
							{
								if ( node->pList().param[(size_t)nodeIndex - 1] )
								{
									// push the callers value
									compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
								} else
								{
									newNode = sym->getFuncDefaultParam( node->left->symbolValue(), true, nodeIndex - 1 );
									if ( newNode )
									{
										// no parameter, but we have a default so use that
										compEmitNode ( funcDef, newNode, sym, needType, retType, safeCalls,  true );
									} else
									{
										// no parameter, no default, push a NULL
										codeSegment.putOp( fglOpcodes::pushNull );
									}
								}
							}
							if ( node->pList().param.size() )
							{
								codeSegment.putOp ( fglOpcodes::pRet );
							}
							sym->popStack ( node->pList().param.size ( ) );  // pop off parameters
							sym->popStack ( ); // pop off object
							codeSegment.emitLabel ( tmpLabel1 );
							fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
							listing.label ( sym, tmpLabel1 );
						}
						// for both ppack call cases
						if ( needValue )
						{
							sym->pushStack ( );
							ret = compEmitCast ( symVariantType, needType );
						}
						break;
					} else
					{
						// we found the function and know how to call it!
						// no prep is needed as we already know everything!
						if ( sym->isMethod ( node->left->symbolValue(), true ) )
						{
							funcName = sym->getFuncName ( node->left->symbolValue(), true );

							// emit the function parameters
							compEmitFuncParameters ( funcDef, sym->getFuncName ( node->left->symbolValue(), true ), true, node, sym, needType, retType, nParamsNeeded, nParamsSent, 1, funcName, safeCalls );
							compEmitContext ( sym, node->left->symbolValue(), sym->getIndex ( file->selfValue, true ), true, node->left->symbolValue().find ( "::" ) == stringi::npos );
						} else
						{
							if ( sym->isExtension ( node->left->symbolValue(), true ) )
							{
								throw errorNum::scEXTENSION_NOT_CLASS_MEMBER;
							}
							funcName = sym->getFuncName ( node->left->symbolValue(), true );
							// emit the function parameters
							compEmitFuncParameters( funcDef, funcName, true, node, sym, needType, retType, nParamsNeeded, nParamsSent, 0, funcName, safeCalls );
						}

						compEmitFuncCall( funcDef, funcName, true, node, sym, needValue, nParamsSent < nParamsNeeded ? nParamsNeeded : nParamsSent, tailCall, nullptr );
						if ( needValue )
						{
							if ( node->left->symbolValue() == file->varCopyValue )
							{
								ret = compEmitCast ( node->pList().param[0]->getType ( sym ), needType );
							} else
							{
								ret = compEmitCast ( sym->getFuncReturnType ( funcName, true, accessorType (), nullptr ), needType );
							}
						}
						break;
					}
				} else
				{
					// unknown atom... link time resolution
					compEmitNode ( funcDef, node->left, sym, symStringType, retType, safeCalls,  true );

					tmpLabel1 = makeLabel ( "func_call%" );

					if ( safeCalls )
					{
						if ( needValue )
						{
							fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafe, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
						} else
						{
							fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafePop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
						}
					} else
					{
						if ( needValue )
						{
							fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callV, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
						} else
						{
							fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVPop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
						}
					}
					for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
					{
						if ( node->pList().param[(size_t)nodeIndex - 1] )
						{
							// push the callers value
							compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
						} else
						{
							newNode = sym->getFuncDefaultParam( node->left->symbolValue(), true, nodeIndex - 1 );
							if ( newNode )
							{
								// no parameter, but we have a default so use that
								compEmitNode ( funcDef, newNode, sym, symVariantType, retType, safeCalls,  true );
							} else
							{
								// no parameter, no default, push a NULL
								codeSegment.putOp( fglOpcodes::pushNull );
								sym->pushStack ( );
							}
						}
					}
					if ( node->pList().param.size() )
					{
						codeSegment.putOp ( fglOpcodes::pRet );
					}
					codeSegment.emitLabel ( tmpLabel1 );
					sym->popStack ( ); // atom (string or somethign callable)
					fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
					listing.label ( sym, tmpLabel1 );
					sym->popStack ( (uint32_t) node->pList().param.size ( ) );
					if ( needValue )
					{
						sym->pushStack ( );
						ret = compEmitCast ( symVariantType, needType );
					}
					break;
				}
			}
			break;
		case astOp::sendMsg:
			if ( (node->left->right->getOp() == astOp::nameValue) && !strccmp( node->left->right->nameValue(), file->releaseValue ) )
			{
				errorLocality e ( &sym->file->errHandler, node->left->right );
				switch ( node->left->right->getOp() )
				{
					case astOp::symbolValue:
						// we can actually do better here.. we CAN link this at compile time... we can find the class from the static string create a static stack
						// TODO: enhance RELEASE!
					default:
						if ( node->pList().param.size ( ) )
						{
							throw errorNum::scINVALID_PARAMETER;
						}

						// needing the value of a released object is nonsensical   
						if ( needValue ) throw errorNum::scNO_RETURN;

						switch ( node->left->left->getType ( sym ).compType() )
						{
							case symbolType::symVariant:
							case symbolType::symObject:
								break;
							default:
								throw errorNum::scINVALID_PARAMETER;
						}

						// object to release
						compEmitNode ( funcDef, node->left->left, sym, symVariantType, symVariantType, safeCalls, true );
						codeSegment.putOp( fglOpcodes::objRelease );
						sym->popStack ( );
						break;
				}
				if ( needValue )
				{
					throw errorNum::scNO_RETURN_VALUE;
				}
				break;
			} else if ( (node->left->right->getOp ( ) == astOp::nameValue) && node->left->left->getType ( sym ).hasClass ( ) && (classDef = sym->findClass ( node->left->left->getType ( sym ).getClass () )) )
			{
				elem = 0;
				if ( (node->left->left->getOp() == astOp::symbolValue) && (node->left->left->symbolValue() == file->selfValue) )
				{
					symbolName = file->selfValue;;
					elem = classDef->getElement( fgxClassElemSearchType::fgxClassElemPrivateMethod, node->left->right->nameValue() );
					if ( !elem ) elem = classDef->getElement( fgxClassElemSearchType::fgxClassElemPrivateAccess, node->left->right->nameValue() );
				} else
				{
					symbolName = cacheString();
					if ( node->left->left->getOp() == astOp::symbolValue )
					{
						switch ( sym->getScope( node->left->left->symbolValue(), true ) )
						{
							case symbolSpaceScope::symLocal:
							case symbolSpaceScope::symLocalParam:
								symbolName = node->left->left->symbolValue();
								break;
							default:
								break;
						}
					}
					elem = classDef->getElement( fgxClassElemSearchType::fgxClassElemPublicMethod, node->left->right->nameValue() );
					if ( !elem ) elem = classDef->getElement( fgxClassElemSearchType::fgxClassElemPublicAccess, node->left->right->nameValue() );
				}
				if ( elem )
				{
					errorLocality e ( &sym->file->errHandler, node->left->right );
					switch ( elem->type )
					{
						case fgxClassElementType::fgxClassType_method:
							if ( hasParamPackCall( node ) )
							{
								// if we reached here then we have no choice but to do a slow dynamic dispatch call
								compEmitNode ( funcDef, node->left->right, sym, symStringType, retType, safeCalls,  true ); 			// message
								switch ( node->left->left->getType ( sym ).compType() )
								{
									case symbolType::symString:
									case symbolType::symVariant:
									case symbolType::symObject:
									case symbolType::symAtom:
										break;
									default:
										throw errorNum::scILLEGAL_OPERAND;
								}

								if ( !elem->elem->isStatic )
								{
									// <x>.<y>   <x> only emitted for methods and extension methods
									compEmitNode ( funcDef, node->left->left, sym, symVariantType, retType, safeCalls,  true ); 				// object
								}

								tmpLabel1 = makeLabel ( "%meth_call%" );
								if ( needValue )
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::objCallPPack, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) + 1 ) );
								} else
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::objCallPPackPop, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) + 1 ) );
								}
								for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
								{
									if ( node->pList().param[(size_t)nodeIndex - 1] )
									{
										// push the callers value
										compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
									} else
									{
										newNode = sym->getFuncDefaultParam( node->left->symbolValue(), true, nodeIndex - 1 );
										if ( newNode )
										{
											// no parameter, but we have a default so use that
											compEmitNode ( funcDef, newNode, sym, needType, retType, safeCalls,  true );
										} else
										{
											// no parameter, no default, push a NULL
											codeSegment.putOp( fglOpcodes::pushNull );
											sym->pushStack ( );
										}
									}
								}
								if ( node->pList().param.size() )
								{
									codeSegment.putOp ( fglOpcodes::pRet );
								}
								sym->popStack ( node->pList().param.size ( ) );	// pop off parameters
								sym->popStack ( ); // message
								sym->popStack ( ); // object

								codeSegment.emitLabel ( tmpLabel1 );
								fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
								listing.label ( sym, tmpLabel1 );
								if ( needValue )
								{
									sym->pushStack ( );	// push a value since the non-pop version will push one onto the stack
									ret = compEmitCast ( sym->getFuncReturnType ( elem->methodAccess.func->name, true, accessorType(), nullptr ), needType );
								}
							} else
							{
								nParamsNeeded = (int32_t)elem->methodAccess.func->params.size();
								nParamsSent = (int32_t)node->pList().param.size();

								funcName = elem->methodAccess.func->name;

								if ( symbolName )
								{
									// emit the parameters
									nParamsSent = compEmitFuncParameters( funcDef, funcName, true, node, sym, needType, retType, nParamsNeeded, nParamsSent, 1, funcName, safeCalls );
									if ( !elem->isStatic )
									{
										if ( !elem->methodAccess.func->isExtension )
										{
											compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, node->left->right->nameValue().find ( "::" ) == stringi::npos );
										} else
										{
											ret = compEmitNode ( funcDef, node->left->left, sym, symVariantType, retType, safeCalls,  true ); 			// object
											switch ( ret.compType () )
											{
												case symbolType::symVariant:
												case symbolType::symObject:
													break;
												default:
													throw errorNum::scILLEGAL_OPERAND;
											}
										}
									}
								} else
								{
									// emit the parameters
									nParamsSent = compEmitFuncParameters( funcDef, funcName, true, node, sym, needType, retType, nParamsNeeded, nParamsSent, !elem->isStatic ? 1 : 0, funcName, safeCalls );

									if ( !elem->isStatic )
									{
										// ok... we have to emit whatever is generating the object we're dispatching against.
										ret = compEmitNode ( funcDef, node->left->left, sym, symVariantType, retType, safeCalls,  true ); 			// object
										switch ( ret.compType () )
										{
											case symbolType::symVariant:
											case symbolType::symObject:
												break;
											default:
												throw errorNum::scILLEGAL_OPERAND;
										}

										// extension methods are always brought into the most derived class as first order memebers... they never modify context
										if ( !elem->methodAccess.func->isExtension )
										{
											// push context of the method
											if ( elem->isVirtual && node->left->right->nameValue().find ( "::" ) == stringi::npos )
											{
												codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
											} else
											{
												if ( elem->methodAccess.objectStart )
												{
													codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->methodAccess.objectStart, 0 );
												}
											}
										}
									}
								}
								// call the code
								compEmitFuncCall( funcDef, elem->methodAccess.func->name, true, node, sym, needValue, nParamsSent, tailCall, node->left->right->nameValue().find ( "::" ) == stringi::npos ? elem : nullptr );
								if ( needValue )
								{
									ret = compEmitCast ( sym->getFuncReturnType ( elem->methodAccess.func->name, true, accessorType(), nullptr ), needType );
								}
							}
							break;
						case fgxClassElementType::fgxClassType_prop:
							if ( !elem->methodAccess.func ) throw errorNum::scINVALID_ACCESS;
							[[fallthrough]];
						case fgxClassElementType::fgxClassType_iVar:
							[[fallthrough]];
						case fgxClassElementType::fgxClassType_const:
							// if we reached here then we have no choice but to do a slow dynamic dispatch call
							compEmitNode ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true ); 			// resolve the value

							tmpLabel1 = makeLabel ( "%access_call%" );
							if ( safeCalls )
							{
								if ( needValue )
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafe, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								} else
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafePop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								}
							} else
							{
								if ( needValue )
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callV, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								} else
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVPop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								}
							}
							for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
							{
								if ( node->pList().param[(size_t)nodeIndex - 1] )
								{
									// push the callers value
									compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
								} else
								{
									// try to find the def value code ( this is a soft link so we don't have it now)
									codeSegment.putOp( fglOpcodes::callDefValue, (uint32_t)nodeIndex );
									sym->pushStack ( );
								}
							}
							// this is to return from the parameter construction portion
							if ( node->pList().param.size() )
							{
								codeSegment.putOp ( fglOpcodes::pRet );
							}
							codeSegment.emitLabel ( tmpLabel1 );
							fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
							listing.label ( sym, tmpLabel1 );
							sym->popStack ();			// thing that's being called
							sym->popStack ( (uint32_t) node->pList().param.size ( ) );
							if ( needValue )
							{
								sym->pushStack ( );
							}
							ret = compEmitCast ( symVariantType, needType );
							break;
						default:
							throw errorNum::scNOT_A_METHOD;
					}
					break;
				} else
				{
					errorLocality e ( &sym->file->errHandler, node->left->right );
					if ( (elem = classDef->getDefaultMethod()) )
					{
						nParamsNeeded = (int32_t) elem->methodAccess.func->params.size();
						nParamsSent = (int32_t)node->pList().param.size();

						funcName = elem->methodAccess.func->name;

						// emit the parameters
						compEmitFuncParameters( funcDef, funcName, true, node, sym, needType, retType, nParamsNeeded, nParamsSent, 2, funcName, safeCalls );

						// push the symbol name as a string parameter to the call
						compEmitNode ( funcDef, node->left->right, sym, symStringType, retType, safeCalls,  true ); 			// message

						// push context of the method
						compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, node->left->right->nameValue().find ( "::" ) == stringi::npos );

						nParamsSent = nParamsSent + 2 < nParamsNeeded ? nParamsNeeded : nParamsSent + 2;
						// call the code
						compEmitFuncCall( funcDef, elem->methodAccess.func->name, true, node, sym, needValue, nParamsSent, tailCall, nullptr );
						ret = compEmitCast ( sym->getFuncReturnType ( elem->methodAccess.func->name, true, accessorType(), nullptr ), needType );
						break;
					} else
					{
						if ( (elem = classDef->getDefaultAccess ()) )
						{
							switch ( elem->methodAccess.func->getReturnType().compType () )
							{
								case symbolType::symObject:
								case symbolType::symVariant:
								case symbolType::symCodeblock:
								case symbolType::symAtom:
								case symbolType::symFunc:
#if 0
																												// push the symbol name as a string parameter to the call
									compEmitNode ( funcDef, block->right, sym, symStringType, retType, safeCalls,  true ); 	// push message

									if ( symbolName )
									{
										// push context of assignment method
										compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, block->right->data.s.find ( "::" ) == stringi::npos );
									} else
									{
										compEmitNode ( funcDef, block->left, sym, symObjectType, symObject, true );
										if ( elem->isVirtual )
										{
											codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, elem->methodAccess.data.vTabEntry, 0 );
										} else
										{
											if ( elem->methodAccess.objectStart )
											{
												codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->methodAccess.objectStart, 0 );
											}
										}
									}

									// call the code
									compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, block, sym, needValue, 2, false, elem );

									// we now have the 
#endif
									compEmitNode ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true ); 	// push message

									tmpLabel1 = makeLabel ( "func_call%" );

									if ( safeCalls )
									{
										if ( needValue )
										{
											fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafe, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
										} else
										{
											fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafePop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
										}
									} else
									{
										if ( needValue )
										{
											fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callV, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
										} else
										{
											fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVPop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
										}
									}
									for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
									{
										if ( node->pList().param[(size_t)nodeIndex - 1] )
										{
											// push the callers value
											compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
										} else
										{
											newNode = sym->getFuncDefaultParam( node->left->symbolValue(), true, nodeIndex - 1 );
											if ( newNode )
											{
												// no parameter, but we have a default so use that
												compEmitNode ( funcDef, newNode, sym, symVariantType, retType, safeCalls,  true );
											} else
											{
												// no parameter, no default, push a NULL
												codeSegment.putOp( fglOpcodes::pushNull );
												sym->pushStack ( );
											}
										}
									}
									if ( node->pList().param.size() )
									{
										codeSegment.putOp ( fglOpcodes::pRet );
									}
									codeSegment.emitLabel ( tmpLabel1 );
									sym->popStack ( ); // atom (string or somethign callable)
									fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
									listing.label ( sym, tmpLabel1 );
									sym->popStack ( (uint32_t) node->pList().param.size ( ) );
									if ( needValue )
									{
										sym->pushStack ( );
										ret = compEmitCast ( symVariantType, needType );
									}
									break;
								default:
									throw errorNum::scUNKNOWN_METHOD;
							}
							break;
						}
					}
				}
			}
			// if we reached here then we have no choice but to do a slow dynamic dispatch call
			compEmitNode ( funcDef, node->left->right, sym, symStringType, retType, safeCalls,  true ); 			// message
			switch ( node->left->getType ( sym ).compType() )
			{
				case symbolType::symString:
				case symbolType::symVariant:
				case symbolType::symObject:
				case symbolType::symAtom:
				case symbolType::symUnknown:
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			compEmitNode ( funcDef, node->left->left, sym, symVariantType, retType, safeCalls,  true ); 				// object - may NOT be an object if we're doing a pseudo-object call

			tmpLabel1 = makeLabel ( "func_call%" );
			codeSegment.putOp( fglOpcodes::prefix, bssSize );
			if ( needValue )
			{
				fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::objCall, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) ) );
			} else
			{
				fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::objCallPop, (uint32_t) 0, (uint32_t) node->pList().param.size ( ) ) );
			}
			bssSize += sizeof (bcObjQuick);
			for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
			{
				if ( node->pList().param[(size_t)nodeIndex - 1] )
				{
					// push the callers value
					compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
				} else
				{
					// try to find the def value code ( this is a soft link so we don't have it now)
					codeSegment.putOp( fglOpcodes::callDefValue, (uint32_t)nodeIndex );
					sym->pushStack ( );
				}
			}
			if ( node->pList().param.size() )
			{
				// this is to return from the parameter construction portion
				codeSegment.putOp ( fglOpcodes::pRet );
			}
			sym->popStack ( (uint32_t) node->pList().param.size ( ) );
			if ( needValue )
			{
				sym->popStack ( 2ull - 1 );
			} else
			{
				sym->popStack ( 2 );
			}
			codeSegment.emitLabel ( tmpLabel1 );
			fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
			listing.label ( sym, tmpLabel1 );
			ret = compEmitCast ( symVariantType, needType );
			break;
		default:
			ret = symVariantType;
			switch ( node->left->getType ( sym ).compType() )
			{
				case symbolType::symObject:
					if ( node->left->getType( sym ).hasClass() && (classDef = sym->findClass ( node->left->getType ( sym ).getClass () )) )
					{
						if ( !classDef->overload[int(fgxOvOp::ovFuncCall)] )
						{
							throw errorNum::scNO_OPERATOR_OVERLOAD;
						}
						elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(fgxOvOp::ovFuncCall)]) - 1];
						nParamsNeeded = (int32_t) elem->methodAccess.func->params.size();
						nParamsSent = (int32_t)node->pList().param.size();

						// emit the parameters
						compEmitFuncParameters( funcDef, elem->methodAccess.func->name, true, node, sym, needType, retType, nParamsNeeded, nParamsSent, 1, elem->methodAccess.func->name, safeCalls );

						// push object
						compEmitNode ( funcDef, node->left, sym, symWeakObjectType, retType, safeCalls,  true );

						nParamsSent = nParamsSent + 1 < nParamsNeeded ? nParamsNeeded : nParamsSent + 1;
						// write out the codeblock call operator
						if ( needValue )
						{
							atom.addFixup( atom.add( elem->methodAccess.func->name, atomListType::atomRef ), codeSegment.putOp( fglOpcodes::objCallFuncOv, (uint32_t)0, nParamsSent ) );
							ret = compEmitCast ( elem->methodAccess.func->getReturnType(), needType );
							sym->popStack ( nParamsSent );
							sym->pushStack ( );
						} else
						{
							atom.addFixup( atom.add( elem->methodAccess.func->name, atomListType::atomRef ), codeSegment.putOp( fglOpcodes::objCallFuncOvPop, (uint32_t)0, nParamsSent ) );
							sym->popStack ( nParamsSent );
						}
					} else
					{
						// dispatch against an overloaded object that we cant reason the class for... we dynamic dispatch it an test at runtime for overload
						// push object
						compEmitNode ( funcDef, node->left, sym, symWeakObjectType, retType, safeCalls,  true );

						tmpLabel1 = makeLabel ( "%meth_call%" );

						if ( safeCalls )
						{
							if ( needValue )
							{
								if ( hasParamPackCall ( node ) )
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackSafe, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								} else
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafe, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								}
							} else
							{
								if ( hasParamPackCall ( node ) )
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackSafePop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								} else
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafePop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								}
							}
						} else
						{
							if ( needValue )
							{
								if ( hasParamPackCall ( node ) )
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPack, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								} else
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callV, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								}
							} else
							{
								if ( hasParamPackCall ( node ) )
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackPop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								} else
								{
									fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVPop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
								}
							}
						}
						for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
						{
							if ( node->pList().param[(size_t)nodeIndex - 1] )
							{
								// push the callers value
								compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
							} else
							{
								// try to find the def value code ( this is a soft link so we don't have it now)
								codeSegment.putOp( fglOpcodes::callDefValue, (uint32_t)nodeIndex );
								sym->pushStack ( );
							}
						}
						if ( node->pList().param.size() )
						{
							codeSegment.putOp ( fglOpcodes::pRet );
						}
						sym->popStack ( node->pList().param.size ( ) );
						sym->popStack ( ); // object;
						codeSegment.emitLabel ( tmpLabel1 );
						fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
						listing.label ( sym, tmpLabel1 );
						if ( needValue )
						{
							sym->pushStack ( );
							ret = compEmitCast ( symVariantType, needType );
						}
					}
					break;
				case symbolType::symCodeblock:
					compEmitNode ( funcDef, node->left, sym, needType, retType, safeCalls,  true );
					for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
					{
						if ( node->pList().param[(size_t)nodeIndex - 1] )
						{
							// push the callers value
							compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, retType, safeCalls,  true );
						} else
						{
							// no parameter passed... so push a null... codeblocks can't have default values
							codeSegment.putOp( fglOpcodes::pushNull );
						}
					}
					// write out the codeblock call operator
					if ( needValue )
					{
						codeSegment.putOp( fglOpcodes::callCB, (uint32_t)node->pList().param.size() );
					} else
					{
						codeSegment.putOp( fglOpcodes::callCBPop, (uint32_t)node->pList().param.size() );
					}
					sym->popStack ( (uint32_t) node->pList().param.size ( ) );
					if ( !needValue )
					{
						sym->popStack ( );
					} else
					{
						ret = compEmitCast ( symVariantType, needType );
					}
					break;
				case symbolType::symVariant:
				case symbolType::symAtom:
				case symbolType::symFunc:
				case symbolType::symString:
					compEmitNode ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true );

					tmpLabel1 = makeLabel ( "func_call%" );

					if ( safeCalls )
					{
						if ( needValue )
						{
							if ( hasParamPackCall ( node ) )
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackSafe, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							} else
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafe, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							}
						} else
						{
							if ( hasParamPackCall ( node ) )
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackSafePop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							} else
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVSafePop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							}
						}
					} else
					{
						if ( needValue )
						{
							if ( hasParamPackCall ( node ) )
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPack, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							} else
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callV, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							}
						} else
						{
							if ( hasParamPackCall ( node ) )
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callPPackPop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							} else
							{
								fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::callVPop, (uint32_t)0, (uint32_t)node->pList ().param.size () ) );
							}
						}
					}
					for ( nodeIndex = (int32_t)node->pList().param.size(); nodeIndex; nodeIndex-- )
					{
						if ( node->pList().param[(size_t)nodeIndex - 1] )
						{
							// push the callers value
							compEmitNode ( funcDef, node->pList().param[(size_t)nodeIndex - 1], sym, symVariantType, symVariantType, safeCalls, true );
						} else
						{
							// try to find the def value code ( this is a soft link so we don't have it now)
							codeSegment.putOp( fglOpcodes::callDefValue, (uint32_t)nodeIndex );
							sym->pushStack ( );
						}
					}
					if ( node->pList().param.size() )
					{
						codeSegment.putOp ( fglOpcodes::pRet );
					}
					sym->popStack ( node->pList().param.size ( ) );
					sym->popStack ( ); // thing to dispatch against
					codeSegment.emitLabel ( tmpLabel1 );
					fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
					listing.label ( sym, tmpLabel1 );
					if ( needValue )
					{
						sym->pushStack ( );
						ret = compEmitCast ( symVariantType, needType );
					}
					break;
				default:
					throw errorNum::scNOT_A_FUNCTION;
			}
			break;
	}
	return ret;
}

uint32_t compExecutable::compEmitConcat( opFunction *funcDef, astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue )
{
	uint32_t	cnt = 0;

	if ( node->getOp() == astOp::add )
	{
		cnt += compEmitConcat( funcDef, node->left, sym, symStringType, retType, safeCalls,  needValue );
		cnt += compEmitConcat( funcDef, node->right, sym, symStringType, retType, safeCalls,  needValue );
	} else
	{
		auto resType = compEmitNode ( funcDef, node, sym, symStringType, retType, safeCalls,  needValue );
		compEmitCast ( resType, needType );
		cnt++;
	}
	return cnt;
}

class symbolTypeClass compExecutable::compEmitBinaryOp ( opFunction *funcDef, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, fglOpcodes intOp, fglOpcodes doubleOp, fglOpcodes stringOp, fglOpcodes varOp, fglOpcodes intOpImm, fglOpcodes doubleOpImm )
{
	symbolTypeClass	leftType = node->left->getType ( sym );
	symbolTypeClass rightType = node->right->getType ( sym );

	if ( !leftType.convPossible ( needType ) )
	{
		throw errorNum::scINVALID_OPERATOR;
	}

	switch ( leftType.compType() )
	{
		case symbolType::symInt:
			if ( intOp == fglOpcodes::noop )
			{ 
				throw errorNum::scILLEGAL_OPERATION;
			}
			if ( needValue )
			{
				switch ( node->right->getOp() )
				{
					case astOp::intValue:
						if ( needType == symbolType::symDouble )
						{
							// variant operation will convert type autoimatically
							compEmitNode ( funcDef, node->left, sym, symDoubleType, retType, safeCalls,  true ); 
							if ( doubleOpImm != fglOpcodes::noop )
							{
								codeSegment.putOp ( doubleOpImm, (double) node->right->intValue() );
							} else
							{
								codeSegment.putOp ( fglOpcodes::pushNumDouble, (double) node->right->intValue() );
								sym->pushStack ( );
								codeSegment.putOp ( doubleOp );
								sym->popStack ( );
							}
							return symbolType::symDouble;
						} else
						{
							compEmitNode ( funcDef, node->left, sym, symIntType, retType, safeCalls,  true );
							if ( intOpImm != fglOpcodes::noop )
							{
								codeSegment.putOp ( intOpImm, node->right->intValue() );
							} else
							{
								codeSegment.putOp ( fglOpcodes::pushNumLong, node->right->intValue() );
								sym->pushStack ( );
								codeSegment.putOp ( intOp );
								sym->popStack ( );
							}
						}
						break;
					case astOp::doubleValue:
						if ( needType == symbolType::symInt )
						{
							// variant operation will convert type autoimatically
							compEmitNode ( funcDef, node->left, sym, symIntType, retType, safeCalls,  true );
							if ( intOpImm != fglOpcodes::noop )
							{
								codeSegment.putOp ( intOpImm, (int64_t)node->right->doubleValue() );
							} else
							{
								codeSegment.putOp ( fglOpcodes::pushNumLong, (int64_t) node->right->doubleValue() );
								sym->pushStack ( );
								codeSegment.putOp ( intOp );
								sym->popStack ( );
							}
							return symIntType;
						} else
						{
							compEmitNode ( funcDef, node->left, sym, symDoubleType, retType, safeCalls,  true );
							if ( doubleOpImm != fglOpcodes::noop )
							{
								codeSegment.putOp ( doubleOpImm, node->right->doubleValue() );
							} else
							{
								codeSegment.putOp ( fglOpcodes::pushNumDouble, node->right->doubleValue() );
								sym->pushStack ( );
								codeSegment.putOp ( doubleOp );
								sym->popStack ( );
							}
						}
						break;
					case astOp::symbolValue:
						if ( needType == symbolType::symDouble )
						{
							// variant operation will convert type autoimatically
							compEmitNode ( funcDef, node->left, sym, symDoubleType, retType, safeCalls,  needValue );
							compEmitNode ( funcDef, node->right, sym, symDoubleType, retType, safeCalls,  needValue );
							codeSegment.putOp ( doubleOp );
							sym->popStack ( );
							return symDoubleType;
						} else
						{
							compEmitNode ( funcDef, node->left, sym, symIntType, retType, safeCalls,  needValue );
							compEmitNode ( funcDef, node->right, sym, symIntType, retType, safeCalls,  needValue );
							codeSegment.putOp ( intOp );
							sym->popStack ( );
						}
						break;
					case astOp::nullValue:
					case astOp::nilValue:
						compEmitNode ( funcDef, node->left, sym, needType, retType, safeCalls,  needValue );
						break;
					case astOp::varrayValue:
					case astOp::arrayValue:
					case astOp::codeBlockValue:
					case astOp::atomValue:
					case astOp::fieldValue:
					case astOp::funcValue:
					case astOp::pairValue:
					case astOp::symbolPack:
						throw errorNum::scTYPE_CONVERSION;
					default:
						if ( leftType == needType )
						{
							switch ( needType.compType ( ) )
							{
								case symbolType::symInt:
									compEmitNode ( funcDef, node->left, sym, needType, retType, safeCalls,  true );
									compEmitNode ( funcDef, node->right, sym, needType, retType, safeCalls,  true );
									codeSegment.putOp ( intOp );
									break;
								case symbolType::symDouble:
									compEmitNode ( funcDef, node->left, sym, needType, retType, safeCalls,  true );
									compEmitNode ( funcDef, node->right, sym, needType, retType, safeCalls,  true );
									codeSegment.putOp ( doubleOp );
									break;
								default:
									compEmitNode ( funcDef, node->left, sym, leftType, retType, safeCalls,  true );
									compEmitNode ( funcDef, node->right, sym, rightType, retType, safeCalls,  true );
									codeSegment.putOp ( varOp );
									break;
							}
						} else
						{
							compEmitNode ( funcDef, node->left, sym, leftType, retType, safeCalls,  true );
							compEmitNode ( funcDef, node->right, sym, rightType, retType, safeCalls,  true );
							codeSegment.putOp ( varOp );
						}
						sym->popStack ( );
						break;
				}
			} else
			{
				// don't need the lhs and rhs for an operation... emit them but with need to false to make sure we capture any invariant's
				compEmitNode ( funcDef, node->left,  sym, symVariantType, symVariantType, safeCalls, false );
				compEmitNode ( funcDef, node->right, sym, symVariantType, symVariantType, safeCalls, false );
			}
			break;
		case symbolType::symDouble:
			if ( doubleOp == fglOpcodes::noop )
			{ 
				throw errorNum::scILLEGAL_OPERATION;
			}
			if ( needValue )
			{
				compEmitNode ( funcDef, node->left, sym, symDoubleType, retType, safeCalls,  true );
				switch ( node->right->getOp() )
				{
					case astOp::intValue:
						if ( doubleOpImm != fglOpcodes::noop )
						{
							codeSegment.putOp ( doubleOpImm, (double)node->right->intValue() );
						} else
						{
							codeSegment.putOp ( fglOpcodes::pushNumDouble, (double)node->right->intValue() );
							sym->pushStack ( );
							codeSegment.putOp ( doubleOp );
							sym->popStack ( );
						}
						break;
					case astOp::doubleValue:
						if ( doubleOpImm != fglOpcodes::noop )
						{
							codeSegment.putOp ( doubleOpImm, node->right->doubleValue() );
						} else
						{
							codeSegment.putOp ( fglOpcodes::pushNumDouble, node->right->doubleValue() );
							sym->pushStack ( );
							codeSegment.putOp ( doubleOp );
							sym->popStack ( );
						}
						break;
					case astOp::symbolValue:
						compEmitNode ( funcDef, node->right, sym, symDoubleType, retType, safeCalls,  true );
						codeSegment.putOp ( doubleOp  );
						sym->popStack ( );
						break;
					case astOp::nullValue:
					case astOp::nilValue:
						compEmitNode ( funcDef, node->left, sym, symDoubleType, retType, safeCalls,  needValue);
						break;
					case astOp::varrayValue:
					case astOp::arrayValue:
					case astOp::symbolPack:
					case astOp::codeBlockValue:
					case astOp::atomValue:
					case astOp::fieldValue:
					case astOp::funcValue:
					case astOp::pairValue:
						throw errorNum::scTYPE_CONVERSION;
					default:
						compEmitNode ( funcDef, node->right, sym, symDoubleType, retType, safeCalls,  true );
						switch ( needType.compType ( ) )
						{
							case symbolType::symInt:
								codeSegment.putOp ( intOp );
								break;
							case symbolType::symDouble:
								codeSegment.putOp ( doubleOp );
								break;
							default:
								codeSegment.putOp ( varOp );
								break;
						}
						sym->popStack ( );
						break;
				}
			} else
			{
				// don't need the lhs and rhs for an operation... emit them but with need to false to make sure we capture any invariant's
				compEmitNode ( funcDef, node->left,  sym, symVariantType, symVariantType, safeCalls, false );
				compEmitNode ( funcDef, node->right, sym, symVariantType, symVariantType, safeCalls, false );
			}
			break;
		case symbolType::symString:
			if ( stringOp != fglOpcodes::noop )
			{
				switch ( node->right->getOp () )
				{
					case astOp::nullValue:
					case astOp::nilValue:
						compEmitNode ( funcDef, node->left, sym, needType, retType, safeCalls,  needValue );
						break;
					default:
						if ( needValue )
						{
							compEmitNode ( funcDef, node->left, sym, symStringType, retType, safeCalls,  true );
							rightType = compEmitNode ( funcDef, node->right, sym, symStringType, retType, safeCalls,  true );
							compEmitCast ( rightType, symStringType );
							codeSegment.putOp ( stringOp );
							compEmitCast ( rightType, needType );
							sym->popStack ();
						} else
						{
							compEmitNode ( funcDef, node->left, sym, symVariantType, symVariantType, safeCalls, false );
							compEmitNode ( funcDef, node->right, sym, symVariantType, symVariantType, safeCalls, false );
						}
						break;
				}
			} else
			{
				throw errorNum::scILLEGAL_OPERATION;
			}
			break;
		default:
			// no needValue processing for variants... it may be an object that's doing operator overloading which can only be determined at run time
			compEmitNode ( funcDef, node->left,  sym, symVariantType, retType, safeCalls,  true ); 
			compEmitNode ( funcDef, node->right, sym, symVariantType, retType, safeCalls,  true );
			codeSegment.putOp ( varOp );
			sym->popStack ( );
			if ( !needValue )
			{
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
				sym->popStack ( );
			} else
			{
				codeSegment.putOp ( fglOpcodes::deref, (uint32_t) 1 );
			}
			break;
	}
	return compEmitCast ( leftType, needType );
}

class symbolTypeClass compExecutable::compEmitUnaryOp ( opFunction *funcDef, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, fglOpcodes intOp, fglOpcodes doubleOp, fglOpcodes stringOp, fglOpcodes varOp )
{
	switch ( node->left->getType ( sym).compType() )
	{
		case symbolType::symInt:
			if ( intOp != fglOpcodes::noop )
			{
				if ( needValue )
				{
					compEmitNode ( funcDef, node->left, sym, symIntType, retType, safeCalls,  true ); 
					codeSegment.putOp ( intOp );
				} else
				{
					compEmitNode ( funcDef, node->left, sym, symIntType, retType, safeCalls,  false );
				}
			} else
			{
				throw errorNum::scILLEGAL_OPERATION;
			}
			break;
		case symbolType::symDouble:
			if ( doubleOp != fglOpcodes::noop )
			{
				if ( needValue )
				{
					compEmitNode ( funcDef, node->left, sym, symDoubleType, retType, safeCalls,  true );
					codeSegment.putOp ( doubleOp );
				} else
				{
					compEmitNode ( funcDef, node->left, sym, symDoubleType, retType, safeCalls,  false );
				}
			} else
			{
				throw errorNum::scILLEGAL_OPERATION;
			}
			break;
		case symbolType::symString:
			if ( stringOp != fglOpcodes::noop )
			{
				if ( needValue )
				{
					compEmitNode ( funcDef, node->left, sym, symStringType, retType, safeCalls,  true );
					codeSegment.putOp ( stringOp );
				} else
				{
					compEmitNode ( funcDef, node->left, sym, symStringType, retType, safeCalls,  false );
				}
			} else
			{
				throw errorNum::scILLEGAL_OPERATION;
			}
			break;
		default:
			// no needValue processing for variants... it may be an object that's doing operator overloading which can only be determined at run time
			compEmitNode ( funcDef, node->left, sym, symVariantType, retType, safeCalls,  true ); 
			codeSegment.putOp ( varOp );
			if ( !needValue )
			{
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
				sym->popStack ();
			}
			break;
	}
	return node->getType ( sym );
}

class symbolTypeClass compExecutable::compEmitCast ( symbolTypeClass const &type, symbolTypeClass const &needType )
{
	if ( type.needConversion ( needType  ) )
	{
		if ( !type.convPossible ( needType ) ) 
		{
			throw errorNum::scTYPE_CONVERSION;
		}
		switch ( needType.compType() )
		{
			case symbolType::symBool:
				codeSegment.putOp ( fglOpcodes::castBool );
				break;
			case symbolType::symDouble:
				codeSegment.putOp ( fglOpcodes::castDouble );
				break;
			case symbolType::symInt:
				codeSegment.putOp ( fglOpcodes::castInt );
				break;
			case symbolType::symString:
				codeSegment.putOp ( fglOpcodes::castString );
				break;
			case symbolType::symVariant:
				// nothing to do here
				break;
			case symbolType::symObject:
				if ( (type.compType () != symbolType::symVariant) && (type.compType () != symbolType::symObject) )
				{
					throw errorNum::scINTERNAL;
				}
				if ( needType.hasClass() )
				{
					codeSegment.putOp ( fglOpcodes::castObject, dataSegment.addString ( needType.getClass() ) );
				}
				break;
			default:
				if ( type.compType () != symbolType::symVariant )
				{
					throw errorNum::scINTERNAL;
				}
				break;
		}
		return needType;
	}
	return type;
}

void compExecutable::compEmitDefaultValue ( opFunction *funcDef, symbolStack *sym, symbolTypeClass const &needType )
{
	switch ( needType.compType() )
	{
		case symbolType::symInt:
		case symbolType::symBool:				// used for casting only... 
			codeSegment.putOp ( fglOpcodes::pushNumLong, (int64_t)0 );
			break;
		case symbolType::symDouble:
			codeSegment.putOp ( fglOpcodes::pushNumLong, 0.0 );
			break;
		case symbolType::symString:
		case symbolType::symArray:
		case symbolType::symVArray:
		case symbolType::symVariant:
		case symbolType::symObject:
		case symbolType::symAtom:
		case symbolType::symCodeblock:
			codeSegment.putOp ( fglOpcodes::pushNull );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

symbolTypeClass compExecutable::compEmitNode ( opFunction *funcDef, astNode *block, symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue )
{
	uint32_t					 index;
	compClass					*classDef;
	compClassElementSearch		*elem;
	fgxClassElemSearchType		 searchType;
	symbolTypeClass				 resType;
	symbolTypeClass				 condType;
	cacheString symbolName;
	errorLocality				 e ( &file->errHandler, block );

	// for debug purposes only
	// auto codeArr = codeSegment.getOps();
	// listing.annotate ( sym, "[%x] ", block);
	// resType = block->getType ( sym).compType();		// why do we do this at all?   each element should have the proper res type

	if ( !needValue )
	{
		resType = symVariantType;
	}

	if ( block->forceEmitDebug )
	{
		debug.addEntry ( block->location, codeSegment.getNumOps (), sym->getStackSize () );
	}

	switch ( block->getOp() )
	{
		/*
			dummy is special... they are inserted into the ast tree when we have an open perentheses
			normally we wouldn't care about this as the precedence system takes care of ensuring the ast is proper and we can
			eliminate them... however they are required to allow us to differentiate between
			a()
			and 
			(a)()

			the first one is a function call to the function named a
			the second one is a function call to the entity contained in the local variable a
		*/
		case astOp::dummy:
			if ( block->left )
			{
				resType = compEmitNode ( funcDef, block->left, sym, needType, retType, safeCalls,  needValue );
			} else
			{
				if ( needValue )
				{
					throw errorNum::scINTERNAL;
				}
				resType = symVariantType;
			}
			break;
		case astOp::nullValue:
			if ( needValue )
			{
				codeSegment.putOp ( fglOpcodes::pushNull );
				sym->pushStack ( );
			}
			resType = symVariantType;
			break;
		case astOp::senderValue:
			if ( needValue )
			{
				codeSegment.putOp ( fglOpcodes::pushSender, sym->getIndex ( file->selfValue, true ) );
				sym->pushStack ( );
				resType = block->getType ( sym );
			}
			resType = symObjectType;
			break;
		case astOp::intValue:
			if ( needValue )
			{
				switch ( needType.compType() )
				{
					case symbolType::symBool:
					case symbolType::symInt:
					case symbolType::symVariant:
						codeSegment.putOp ( fglOpcodes::pushNumLong, block->intValue() );
						break;
					case symbolType::symDouble:
						codeSegment.putOp ( fglOpcodes::pushNumDouble, (double)block->intValue() );
						break;
					case symbolType::symString:
						sprintf_s ( tmpStringBuffer, "%lli", block->intValue() );
						codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( tmpStringBuffer ) );
						break;
					default:
						throw errorNum::scTYPE_CONVERSION;
				}
				sym->pushStack ( );
			}
			resType = needType;
			break;
		case astOp::doubleValue:
			if ( needValue )
			{
				switch ( needType.compType() )
				{
					case symbolType::symBool:
					case symbolType::symInt:
						codeSegment.putOp ( fglOpcodes::pushNumLong, (int64_t)block->doubleValue() );
						break;
					case symbolType::symVariant:
					case symbolType::symDouble:
						codeSegment.putOp ( fglOpcodes::pushNumDouble, block->doubleValue() );
						break;
					case symbolType::symString:
				
						sprintf_s ( tmpStringBuffer, "%e", block->doubleValue() );
						codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( tmpStringBuffer ) );
						break;
					default:
						throw errorNum::scTYPE_CONVERSION;
				}
				sym->pushStack ( );
			}
			resType = needType;
			break;
		case astOp::stringValue:
			if ( needValue )
			{
				switch ( needType.compType() )
				{
					case symbolType::symBool:
					case symbolType::symInt:
						codeSegment.putOp ( fglOpcodes::pushNumLong, _atoi64 ( block->stringValue() ) );
						break;
					case symbolType::symDouble:
						codeSegment.putOp ( fglOpcodes::pushNumDouble, atof ( block->stringValue() ) );
						break;
					case symbolType::symVariant:
					case symbolType::symString:
						codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( block->stringValue() ) );
						break;
					default:
						throw errorNum::scTYPE_CONVERSION;
				}
				sym->pushStack ( );
			}
			resType = needType;
			break;
		case astOp::nameValue:
			if ( needValue )
			{
				switch ( needType.compType() )
				{
					case symbolType::symBool:
					case symbolType::symInt:
						codeSegment.putOp ( fglOpcodes::pushNumLong, _atoi64 ( block->nameValue() ) );
						break;
					case symbolType::symDouble:
						codeSegment.putOp ( fglOpcodes::pushNumDouble, atof ( block->nameValue() ) );
						break;
					case symbolType::symVariant:
					case symbolType::symString:
						codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( block->nameValue() ) );
						break;
					default:
						throw errorNum::scTYPE_CONVERSION;
				}
				sym->pushStack ( );
			}
			resType = needType;
			break;
		case astOp::symbolValue:
			switch ( sym->getScope ( block->symbolValue(), true ) )
			{
				case symbolSpaceScope::symGlobal:
				case symbolSpaceScope::symLocalStatic:
				case symbolSpaceScope::symLocalConst:
					if ( needValue )
					{
						codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( block->symbolValue(), true ) );
						resType = sym->getType ( block->symbolValue(), true );
						sym->pushStack ( );
					}
					break;
				case symbolSpaceScope::symClassStatic:
				case symbolSpaceScope::symClassConst:
					if ( needValue )
					{
						if ( sym->isInterface ( block->symbolValue(), true ) )
						{
							if ( block->symbolValue().find ( "::" ) )
							{
								auto className = block->symbolValue().substr ( 0, block->symbolValue().find ( "::" ) );
								auto message = block->symbolValue().substr ( block->symbolValue().find ( "::" ) + 2 );
								codeSegment.putOp ( fglOpcodes::pushClassGlobal, dataSegment.addString ( className ), dataSegment.addString ( message ) );
								resType = sym->getType ( block->symbolValue(), true );
							} else
							{
								throw errorNum::scINTERNAL;
							}
						} else
						{
							codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( block->symbolValue(), true ) );
							resType = sym->getType ( block->symbolValue(), true );
						}
						sym->pushStack ( );
					}
					break;
				case symbolSpaceScope::symLocal:
				case symbolSpaceScope::symLocalParam:
					if ( needValue )
					{
						codeSegment.putOp ( fglOpcodes::pushSymLocal, sym->getIndex ( block->symbolValue(), true ) );
						resType = sym->getType ( block->symbolValue(), true );
						sym->pushStack ( );
					}
					break;
				case symbolSpaceScope::symLocalField:
					codeSegment.putOp ( fglOpcodes::waPushField, dataSegment.addString ( block->symbolValue() ) );
					resType = symVariantType;
					sym->pushStack ( );
					break;
				case symbolSpaceScope::symClassAccess:
					elem = sym->findClass ( sym->getType ( file->selfValue, true ).getClass() )->getElement ( block->symbolValue() );
					if ( !elem->methodAccess.func ) throw errorNum::scINVALID_ACCESS;

					// push context of access method
					compEmitContext ( sym, elem, sym->getIndex ( file->selfValue, true ), true, block->symbolValue().find ( "::" ) == stringi::npos );
					// call the code
					compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, block, sym, needValue, 1, false, nullptr );
					if ( needValue && elem->methodAccess.func->getReturnType ().compType () == symbolType::symVariant )
					{
						codeSegment.putOp ( fglOpcodes::deref, (uint32_t) 1 );
					}
					resType = elem->methodAccess.func->getReturnType();
					break;
				case symbolSpaceScope::symClassIVar:
					if ( needValue )
					{
						if ( sym->isVirtual ( block->symbolValue(), true ) )
						{
							codeSegment.putOp ( fglOpcodes::pushObjVirtIVar, sym->getIndex ( file->selfValue, true ), sym->getObjectOffset ( block->symbolValue(), true ), 0 );
						} else
						{
							codeSegment.putOp ( fglOpcodes::pushObjIVar, sym->getIndex ( file->selfValue, true ), sym->getObjectOffset ( block->symbolValue(), true ), 0 );
						}
						resType = sym->getType ( block->symbolValue(), true );
						sym->pushStack ( );
					}
					break;
				case symbolSpaceScope::symClassInherit:
					if ( needValue )
					{
						compEmitContext ( sym, block->symbolValue(), sym->getIndex ( file->selfValue, true ), true, block->symbolValue().find ( "::" ) == stringi::npos );
						resType = symbolTypeClass ( symbolType::symObject, block->symbolValue() );
					}
					break;
				case symbolSpaceScope::symFunction:
					if ( needValue )
					{
						atom.addFixup ( atom.add ( block->symbolValue(), atomListType::atomRef ), codeSegment.putOp ( fglOpcodes::pushAtom, (uint32_t) 0 ) );
						resType = sym->getType ( block->symbolValue(), true );
						sym->pushStack ( );
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case astOp::codeBlockExpression:
			// should never get here as these should be already converted to codeBlockValues before code emission occurs
			throw errorNum::scINTERNAL;
		case astOp::codeBlockValue:
			if ( needValue )
			{
				codeSegment.putOp ( fglOpcodes::pushCodeblock, dataSegment.addString ( (uint32_t) bufferSize ( block->codeBlock().buff ), bufferBuff ( block->codeBlock().buff ) ) );
				sym->pushStack ( );
			}
			resType = symCodeblockType;
			break;
		case astOp::atomValue:
			if ( needValue )
			{
				if ( needType == symbolType::symString )
				{
					codeSegment.putOp ( fglOpcodes::pushString, dataSegment.addString ( block->symbolValue() ) );
					resType = symStringType;
				} else
				{
					atom.addFixup ( atom.add ( block->symbolValue(), atomListType::atomRef ), codeSegment.putOp ( fglOpcodes::pushAtom, (uint32_t) 0 ) );
					resType = symAtomType;
				}
				sym->pushStack ( );
			} else
			{
				resType = needType;
			}
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
			if ( needValue )
			{
				switch ( needType.compType() )
				{
					case symbolType::symObject:
					case symbolType::symVariant:
						// build all the node values on the stack
						if ( block->arrayData().nodes.size() )
						{
							if ( block->arrayData().nodes[0]->getOp() == astOp::pairValue )
							{
								for ( auto &it : block->arrayData ().nodes )
								{
									// name
									compEmitNode ( funcDef, it->left, sym, symVariantType, retType, safeCalls, true );
									//value
									if ( it->right->getOp () == astOp::nilValue )
									{
										codeSegment.putOp ( fglOpcodes::pushNull );
										sym->pushStack ();
									} else
									{
										compEmitNode ( funcDef, it->right, sym, symVariantType, retType, safeCalls, true );
									}
								}
								// now make the array
								codeSegment.putOp ( fglOpcodes::pushAArray, (uint32_t)block->arrayData().nodes.size() );
								sym->popStack ( block->arrayData().nodes.size ( )  * 2 - 1 );
								resType = symbolTypeClass ( symbolType::symObject, file->aArrayValue );
							} else
							{
								for ( auto &it : block->arrayData ().nodes )
								{
									if ( it->getOp () == astOp::nilValue )
									{
										codeSegment.putOp ( fglOpcodes::pushNull );
										sym->pushStack ();
									} else
									{
										compEmitNode ( funcDef, it, sym, symVariantType, retType, safeCalls, true );
									}
								}
								// now make the array
								if ( block->getOp() == astOp::varrayValue )
								{
									codeSegment.putOp ( fglOpcodes::pushVariableArray, (uint32_t) block->arrayData().nodes.size ( ) );
									resType = symVArrayType;
								} else
								{
									codeSegment.putOp ( fglOpcodes::pushFixedArray, (uint32_t) block->arrayData().nodes.size ( ) );
									resType = symArrayType;
								}
								sym->popStack( block->arrayData().nodes.size() - 1 );
							}
						} else
						{
							codeSegment.putOp ( fglOpcodes::pushNullArray );
							resType = symObjectType;
							sym->pushStack ( );
						}
						break;
					case symbolType::symArray:
					case symbolType::symVArray:
						// build all the node values on the stack
						if ( block->arrayData().nodes.size() )
						{
							if ( block->arrayData().nodes[0]->getOp() == astOp::pairValue )
							{
								throw errorNum::scARRAY_ENUMERATION;
							} else
							{
								for ( auto &it : block->arrayData().nodes )
								{
									if ( it->getOp () == astOp::nilValue )
									{
										codeSegment.putOp ( fglOpcodes::pushNull );
										sym->pushStack ();
									} else
									{
										compEmitNode ( funcDef, it, sym, symVariantType, retType, safeCalls, true );
									}
								}
								// now make the array
								if ( block->getOp() == astOp::varrayValue )
								{
									codeSegment.putOp ( fglOpcodes::pushVariableArray, (uint32_t) block->arrayData().nodes.size ( ) );
									resType = symVArrayType;
								} else
								{
									codeSegment.putOp ( fglOpcodes::pushFixedArray, (uint32_t) block->arrayData().nodes.size ( ) );
									resType = symArrayType;
								}
								sym->popStack ( block->arrayData().nodes.size ( ) - 1 );
							}
						} else
						{
							// no elements... this is a 0 element variable array
							codeSegment.putOp ( fglOpcodes::pushNullArray );
							resType = symVArrayType;
							sym->pushStack ( );
						}
						break;
					default:
						throw errorNum::scILLEGAL_OPERAND;
				}
			} else
			{
				resType = needType;
			}
			break;
		case astOp::fieldValue:
			if ( needValue )
			{
				codeSegment.putOp ( fglOpcodes::waPushField, dataSegment.addString ( block->symbolValue () ) );
				sym->pushStack ();
			}
			resType = symVariantType;
			break;
		case astOp::pairValue:
			throw errorNum::scINTERNAL;

		case astOp::intCast:
			resType = compEmitCast ( symVariantType, symIntType );
			break;
		case astOp::doubleCast:
			resType = compEmitCast ( symVariantType, symDoubleType );
			break;
		case astOp::stringCast:
			resType = compEmitCast ( symVariantType, symStringType );
			break;

		case astOp::assign:
			resType = compEmitAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue );
			break;

		case astOp::divAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls, needValue, fglOpcodes::divv, fglOpcodes::storeLocalDiv, fglOpcodes::storeGlobalDiv, fglOpcodes::storeClassIVarDiv, fglOpcodes::storeClassVirtIVarDiv, fglOpcodes::storeRefDiv, fgxOvOp::ovDivAssign );
			break;
		case astOp::addAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::addv, fglOpcodes::storeLocalAdd, fglOpcodes::storeGlobalAdd, fglOpcodes::storeClassIVarAdd, fglOpcodes::storeClassVirtIVarAdd, fglOpcodes::storeRefAdd, fgxOvOp::ovAddAssign );
			break;
		case astOp::subAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::subv, fglOpcodes::storeLocalSub, fglOpcodes::storeGlobalSub, fglOpcodes::storeClassIVarSub, fglOpcodes::storeClassVirtIVarSub, fglOpcodes::storeRefSub, fgxOvOp::ovSubAssign );
			break;
		case astOp::mulAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::mulv, fglOpcodes::storeLocalMul, fglOpcodes::storeGlobalMul, fglOpcodes::storeClassIVarMul, fglOpcodes::storeClassVirtIVarMul, fglOpcodes::storeRefMul, fgxOvOp::ovMulAssign );
			break;
		case astOp::powAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::powv, fglOpcodes::storeLocalPow, fglOpcodes::storeGlobalPow, fglOpcodes::storeClassIVarPow, fglOpcodes::storeClassVirtIVarPow, fglOpcodes::storeRefPow, fgxOvOp::ovPowAssign );
			break;
		case astOp::modAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::modv, fglOpcodes::storeLocalMod, fglOpcodes::storeGlobalMod, fglOpcodes::storeClassIVarMod, fglOpcodes::storeClassVirtIVarMod, fglOpcodes::storeRefMod, fgxOvOp::ovModAssign );
			break;
		case astOp::bwAndAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::bwandv, fglOpcodes::storeLocalBwAnd, fglOpcodes::storeGlobalBwAnd, fglOpcodes::storeClassIVarBwAnd, fglOpcodes::storeClassVirtIVarBwAnd, fglOpcodes::storeRefBwAnd, fgxOvOp::ovBwAndAssign );
			break;
		case astOp::bwOrAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::bworv, fglOpcodes::storeLocalBwOr, fglOpcodes::storeGlobalBwOr, fglOpcodes::storeClassIVarBwOr, fglOpcodes::storeClassVirtIVarBwOr, fglOpcodes::storeRefBwOr, fgxOvOp::ovBwOrAssign );
			break;
		case astOp::bwXorAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::bwxorv, fglOpcodes::storeLocalBwXor, fglOpcodes::storeGlobalBwXor, fglOpcodes::storeClassIVarBwXor, fglOpcodes::storeClassVirtIVarBwXor, fglOpcodes::storeRefBwXor, fgxOvOp::ovBwXorAssign );
			break;
		case astOp::shLeftAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::shlv, fglOpcodes::storeLocalShLeft, fglOpcodes::storeGlobalShLeft, fglOpcodes::storeClassIVarShLeft, fglOpcodes::storeClassVirtIVarShLeft, fglOpcodes::storeRefShLeft, fgxOvOp::ovShLeftAssign );
			break;
		case astOp::shRightAssign:
			resType = compEmitOpAssign ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::shrv, fglOpcodes::storeLocalShRight, fglOpcodes::storeGlobalShRight, fglOpcodes::storeClassIVarShRight, fglOpcodes::storeClassVirtIVarShRight, fglOpcodes::storeRefShRight, fgxOvOp::ovShRightAssign );
			break;

		case astOp::preInc:
			resType = compEmitIncOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, true, true, fglOpcodes::incLocali, fglOpcodes::incLocald, fglOpcodes::incLocalv, fglOpcodes::incGlobalv, fglOpcodes::incGlobalv, fglOpcodes::incGlobalv, fglOpcodes::incClassIVar, fglOpcodes::incClassVirtIVar, fgxOvOp::ovPreInc );
			break;
		case astOp::postInc:
			resType = compEmitIncOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, false, true, fglOpcodes::incLocali, fglOpcodes::incLocald, fglOpcodes::incLocalv, fglOpcodes::incGlobalv, fglOpcodes::incGlobalv, fglOpcodes::incGlobalv, fglOpcodes::incClassIVar, fglOpcodes::incClassVirtIVar, fgxOvOp::ovPostInc );
			break;
		case astOp::preDec:
			resType = compEmitIncOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, true, false, fglOpcodes::decLocali, fglOpcodes::decLocald, fglOpcodes::decLocalv, fglOpcodes::decGlobalv, fglOpcodes::decGlobalv, fglOpcodes::decGlobalv, fglOpcodes::decClassIVar, fglOpcodes::decClassVirtIVar, fgxOvOp::ovPreDec );
			break;
		case astOp::postDec:
			resType = compEmitIncOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, false, false, fglOpcodes::decLocali, fglOpcodes::decLocald, fglOpcodes::decLocalv, fglOpcodes::decGlobalv, fglOpcodes::decGlobalv, fglOpcodes::decGlobalv, fglOpcodes::decClassIVar, fglOpcodes::decClassVirtIVar, fgxOvOp::ovPostDec );
			break;

		case astOp::add:
			if ( needType == symbolType::symString && block->left->getType ( sym ) == symbolType::symString )
			{
				uint32_t cnt = compEmitConcat( funcDef, block, sym, symStringType, retType, safeCalls,  needValue );
				if ( needValue )
				{
					if ( cnt != 2 )
					{
						codeSegment.putOp( fglOpcodes::addsmulti, cnt );
					} else
					{
						codeSegment.putOp( fglOpcodes::adds, cnt );
					}
					sym->popStack ( static_cast<size_t>(cnt) - 1 );
				}
				resType = symStringType;
			} else
			{
				resType = compEmitBinaryOp( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::addi, fglOpcodes::addd, fglOpcodes::adds, safeCalls ? fglOpcodes::addSafev : fglOpcodes::addv, fglOpcodes::addiImm, fglOpcodes::adddImm );
				resType = compEmitCast ( resType, needType );
			}
			break;
		case astOp::subtract:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::subi, fglOpcodes::subd, fglOpcodes::cmps, safeCalls ? fglOpcodes::subSafev : fglOpcodes::subv, fglOpcodes::subiImm, fglOpcodes::subdImm);
			break;
		case astOp::multiply:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::muli, fglOpcodes::muld, fglOpcodes::noop, safeCalls ? fglOpcodes::mulSafev : fglOpcodes::mulv, fglOpcodes::muliImm, fglOpcodes::muldImm );
			break;
		case astOp::divide:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::divi, fglOpcodes::divd, fglOpcodes::noop, safeCalls ? fglOpcodes::divSafev : fglOpcodes::divv, fglOpcodes::diviImm, fglOpcodes::divdImm );
			break;
		case astOp::modulus:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::modi, fglOpcodes::modv, fglOpcodes::noop, safeCalls ? fglOpcodes::modSafev : fglOpcodes::modv, fglOpcodes::modiImm, fglOpcodes::noop );
			break;
		case astOp::power:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::powi, fglOpcodes::powd, fglOpcodes::noop, safeCalls ? fglOpcodes::powSafev : fglOpcodes::powv, fglOpcodes::powiImm, fglOpcodes::noop );
			break;
		case astOp::max:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::maxi, fglOpcodes::maxd, fglOpcodes::noop, safeCalls ? fglOpcodes::maxSafev : fglOpcodes::maxv, fglOpcodes::maxiImm, fglOpcodes::maxdImm );
			break;
		case astOp::min:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::mini, fglOpcodes::maxv, fglOpcodes::noop, safeCalls ? fglOpcodes::minSafev : fglOpcodes::minv, fglOpcodes::miniImm, fglOpcodes::mindImm );
			break;
		case astOp::bitAnd:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::bwandv, fglOpcodes::noop, fglOpcodes::noop, safeCalls ? fglOpcodes::bwandSafev : fglOpcodes::bwandv, fglOpcodes::bwandiImm, fglOpcodes::noop );
			break;
		case astOp::bitOr:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::bworv, fglOpcodes::noop, fglOpcodes::noop, safeCalls ? fglOpcodes::bworSafev : fglOpcodes::bworv, fglOpcodes::bworiImm, fglOpcodes::noop );
			break;
		case astOp::bitXor:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::bwxorv, fglOpcodes::noop, fglOpcodes::noop, safeCalls ? fglOpcodes::bwxorSafev : fglOpcodes::bwxorv, fglOpcodes::bwxoriImm, fglOpcodes::noop );
			break;
		case astOp::shiftLeft:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::shlv, fglOpcodes::noop, fglOpcodes::noop, safeCalls ? fglOpcodes::shlSafev : fglOpcodes::shlv, fglOpcodes::shliImm, fglOpcodes::noop );
			break;
		case astOp::shiftRight:
			resType = compEmitBinaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::shrv, fglOpcodes::noop, fglOpcodes::noop, safeCalls ? fglOpcodes::shrSafev : fglOpcodes::shrv, fglOpcodes::shriImm, fglOpcodes::noop );
			break;

		case astOp::less:
			resType = compEmitBinaryOp ( funcDef, block, sym, symVariantType, retType, safeCalls,  needValue, fglOpcodes::lti, fglOpcodes::ltd, fglOpcodes::lts, safeCalls ? fglOpcodes::ltSafev : fglOpcodes::ltv, fglOpcodes::ltiImm, fglOpcodes::ltdImm );
			break;
		case astOp::lessEqual:
			resType = compEmitBinaryOp ( funcDef, block, sym, symVariantType, retType, safeCalls,  needValue, fglOpcodes::lteqi, fglOpcodes::lteqd, fglOpcodes::lteqs, safeCalls ? fglOpcodes::lteqSafev : fglOpcodes::lteqv, fglOpcodes::lteqiImm, fglOpcodes::lteqdImm );
			break;
		case astOp::greater:
			resType = compEmitBinaryOp ( funcDef, block, sym, symVariantType, retType, safeCalls,  needValue, fglOpcodes::gti, fglOpcodes::gtd, fglOpcodes::gts, safeCalls ? fglOpcodes::gtSafev : fglOpcodes::gtv, fglOpcodes::gtiImm, fglOpcodes::gtdImm );
			break;
		case astOp::greaterEqual:
			resType = compEmitBinaryOp ( funcDef, block, sym, symVariantType, retType, safeCalls,  needValue, fglOpcodes::gteqi, fglOpcodes::gteqd, fglOpcodes::gteqs, safeCalls ? fglOpcodes::gteqSafev : fglOpcodes::gteqv, fglOpcodes::gteqiImm, fglOpcodes::gteqdImm );
			break;
		case astOp::equal:
			resType = compEmitBinaryOp ( funcDef, block, sym, symVariantType, retType, safeCalls,  needValue, fglOpcodes::eqi, fglOpcodes::eqd, fglOpcodes::eqs, safeCalls ? fglOpcodes::eqSafev : fglOpcodes::eqv, fglOpcodes::eqiImm, fglOpcodes::eqdImm );
			break;
		case astOp::equali:
			resType = compEmitBinaryOp ( funcDef, block, sym, symVariantType, retType, safeCalls,  needValue, fglOpcodes::eqi, fglOpcodes::eqd, fglOpcodes::aeqs, safeCalls ? fglOpcodes::aeqSafev : fglOpcodes::aeqv, fglOpcodes::eqiImm, fglOpcodes::eqdImm );
			break;
		case astOp::notEqual:
			resType = compEmitBinaryOp ( funcDef, block, sym, symVariantType, retType, safeCalls,  needValue, fglOpcodes::neqi, fglOpcodes::neqd, fglOpcodes::neqs, safeCalls ? fglOpcodes::neqSafev : fglOpcodes::neqv, fglOpcodes::neqiImm, fglOpcodes::neqdImm );
			break;

		case astOp::negate:
			resType = compEmitUnaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::negi, fglOpcodes::negd, fglOpcodes::noop, safeCalls ? fglOpcodes::negSafev : fglOpcodes::negv );
			break;
		case astOp::twosComplement:
			resType = compEmitUnaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::twoci, fglOpcodes::noop, fglOpcodes::noop, safeCalls ? fglOpcodes::twocSafev : fglOpcodes::twocv );
			break;
		case astOp::logicalNot:
			resType = compEmitUnaryOp ( funcDef, block, sym, needType, retType, safeCalls,  needValue, fglOpcodes::noti, fglOpcodes::notd, fglOpcodes::nots, safeCalls ? fglOpcodes::notSafev : fglOpcodes::notv );
			switch ( block->getType ( sym ).compType() )
			{
				case symbolType::symInt:
				case symbolType::symDouble:
				case symbolType::symString:
					resType = symWeakBoolType;
					break;
				default:
					break;
			}
			break;
		case astOp::seq:
			compEmitNode ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  false ); 
			resType = compEmitNode ( funcDef, block->right, sym, needType, retType, safeCalls,  needValue ); 
			break;
		case astOp::funcCall:
			resType = compEmitFunc ( funcDef, block, sym, needType, retType, safeCalls,  needValue, false );
			break;
		case astOp::comp:
			compEmitNode ( funcDef, block->left, sym, needType, retType, safeCalls,  true ); 
			codeSegment.putOp ( fglOpcodes::evaluate );
			if ( !needValue )
			{
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
				sym->popStack ( );
			}
			resType = symVariantType;
			break;

		case astOp::coalesce:
			if ( block->left->isNull ( ) )
			{
				// trivial case, left is always null so just emit right
				resType = compEmitNode ( funcDef, block->right, sym, needType, retType, safeCalls,  needValue );
			} else
			{
				symbolTypeClass resType1;
				symbolTypeClass	resType2;
				char			ifEndLabel[256];

				sprintf_s ( ifEndLabel, sizeof ( ifEndLabel ), "$is_null%u", labelId++ );

				// process primary condition
				resType1 = compEmitNode ( funcDef, block->left, sym, symVariantType, symVariantType, safeCalls, true );

				fixup->needFixup ( file->sCache.get ( ifEndLabel ), codeSegment.putOp ( fglOpcodes::jmpnnull, (uint32_t) 0 ) );
				sym->popStack ( );

				// emit null coalesced condition
				resType2 = compEmitNode ( funcDef, block->right, sym, needType, retType, safeCalls,  true );

				// emit end of condition label
				codeSegment.emitLabel ( ifEndLabel );
				fixup->setRefPoint ( file->sCache.get ( ifEndLabel ), codeSegment.nextOp ( ) );
				listing.label ( sym, ifEndLabel );

				if ( needValue )
				{
					if ( resType1 == resType2 )
					{
						resType = resType1;
					} else
					{
						resType = symVariantType;
					}
				} else
				{
					codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
					sym->popStack ( );
				}
			}
			break;
		case astOp::conditional:
			{
				char						 ifLabel[256];
				char						 ifEndLabel[256];
				if ( block->left->isTrue ( ) )
				{
					compEmitNode ( funcDef, block->conditionData().trueCond, sym, needType, retType, safeCalls,  needValue );
					resType = compEmitCast ( block->conditionData().trueCond->getType ( sym ), needType );
				} else if ( block->left->isFalse ( ) )
				{
					compEmitNode ( funcDef, block->conditionData().falseCond, sym, needType, retType, safeCalls,  needValue );
					resType = compEmitCast ( block->conditionData().falseCond->getType ( sym ), needType );
				} else
				{
					symbolTypeClass resType1;
					symbolTypeClass	resType2;

					sprintf_s ( ifLabel, sizeof ( ifLabel ), "$cond_not_true%u", labelId++ );
					sprintf_s ( ifEndLabel, sizeof ( ifLabel ), "$cond_end%u", labelId++ );

					condType = compEmitNode ( funcDef, block->left, sym, symVariantType, symVariantType, safeCalls, true );
					if ( (condType.compType ( ) != symIntType) || (condType.compType ( ) != symBoolType) )
					{
						compEmitCast ( condType, symBoolType );
					}
					// if not true jmp to false condtion
					fixup->needFixup ( file->sCache.get ( ifLabel ), codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
					sym->popStack ( );
					listing.emitOps ( sym );

					// emit true condition
					resType1 = compEmitNode ( funcDef, block->conditionData().trueCond, sym, needType, retType, safeCalls,  true );

					// jmp to code after false code
					fixup->needFixup ( file->sCache.get ( ifEndLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
					listing.emitOps ( sym );

					// emit not true label
					codeSegment.emitLabel ( ifLabel );
					fixup->setRefPoint ( file->sCache.get ( ifLabel ), codeSegment.nextOp ( ) );
					listing.label ( sym, ifLabel );

					// we may actually need this, but for bookeeping we remove it here to be replaced by the false value... the jump before this selects
					sym->popStack ( );

					resType2 = compEmitNode ( funcDef, block->conditionData().falseCond, sym, needType, retType, safeCalls,  true );

					// emit end of condition label
					codeSegment.emitLabel ( ifEndLabel );
					fixup->setRefPoint ( file->sCache.get ( ifEndLabel ), codeSegment.nextOp ( ) );
					listing.label ( sym, ifEndLabel );

					if ( needValue )
					{
						if ( resType1 == resType2 )
						{
							resType = resType1;
						} else
						{
							resType = symVariantType;
						}
					} else
					{
						codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
						sym->popStack ( );
					}
				}
			}
			break;
		case astOp::logicalOr:
			if ( needValue )
			{
				// we have already reduced in strength... so to get here we must have to emit a full logicalOr operation
				auto tmpLabel1 = makeLabel ( "logicalOr%" );

				// emit the left condition
				condType = compEmitNode ( funcDef, block->left, sym, symBoolType, retType, safeCalls,  true );
				compEmitCast ( condType, symIntType );

				// conditional jump over right condition
				fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpc, (uint32_t) 0 ) );
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t)1 );
				sym->popStack ();

				// emit the right condition
				resType = compEmitNode ( funcDef, block->right, sym, symBoolType, retType, safeCalls,  true );
				compEmitCast ( resType, symIntType );
				sym->popStack ( );

				listing.emitOps ( sym );
				codeSegment.emitLabel ( tmpLabel1 );
				fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
				listing.label ( sym, tmpLabel1 );

				// NOTE: the sequences of popStack are not necessarily correct here.   They dont' need to be.  
				//		they simply need to ensure that any calls to compEmitNode() have the correct count at
				//		the time they are called and that the correct count is done at the time we exit
				///		above we've removed both stack entries... this keepps the emitnode call intact
				///		but we still need to have one at the end
				sym->pushStack( );
			} else
			{
				compEmitNode ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  false );
				compEmitNode ( funcDef, block->right, sym, symVariantType, retType, safeCalls,  false );
			}
			resType = symIntType;
			break;
		case astOp::logicalAnd:
			// TODO: optimize this for chained &&'s   right now this logic will jump from one jmpnc to the next to the next until the last one is reached.   we should be able to 
			//		 chain emissions so that we jump to the last one on an AND failure
			if ( needValue )
			{
				auto tmpLabel1 = makeLabel ( "logicalAnd%" );

				// emit left hand
				condType= compEmitNode ( funcDef, block->left, sym, symBoolType, retType, safeCalls,  true );

				// conditional jump
				fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpnc, (uint32_t) 0 ) );
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t)1 );
				sym->popStack ();

				resType = compEmitNode ( funcDef, block->right, sym, symBoolType, retType, safeCalls,  true );
				sym->popStack ( );

				listing.emitOps ( sym );
				codeSegment.emitLabel ( tmpLabel1 );
				listing.label ( sym, tmpLabel1 );
				fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );

				// NOTE: the sequences of popStack are not necessarily correct here.   They dont' need to be.  
				//		they simply need to ensure that any calls to compEmitNode() have the correct count at
				//		the time they are called and that the correct count is done at the time we exit
				///		above we've removed both stack entries... this keepps the emitnode call intact
				///		but we still need to have one at the end
				sym->pushStack ( );
			} else
			{
				compEmitNode ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  false );
				compEmitNode ( funcDef, block->right, sym, symVariantType, retType, safeCalls,  false );
			}
			resType = symIntType;
			break;
		case astOp::arrayDeref:
			switch ( block->left->getType ( sym).compType() )
			{
				case symbolType::symArray:
				case symbolType::symVArray:
				case symbolType::symString:
				case symbolType::symObject:
				case symbolType::symVariant:
					compEmitNode ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  true );
					for ( size_t index = 0; index < block->pList().param.size() - 1; index++ )
					{
						compEmitNode ( funcDef, block->pList().param[index], sym, symVariantType, retType, safeCalls,  true ); 
						codeSegment.putOp ( fglOpcodes::arrDerefRef );
						sym->popStack ( );
					}
					compEmitNode ( funcDef, block->pList().param[block->pList().param.size ( ) - 1], sym, symVariantType, retType, safeCalls,  true );
					codeSegment.putOp ( fglOpcodes::arrDeref );
					sym->popStack ( );
					if ( !needValue )
					{
						codeSegment.putOp ( fglOpcodes::pop, (uint32_t ) 1 );
						sym->popStack ( );
					}
					break;
				default:
					throw errorNum::scILLEGAL_OPERAND;
			}
			resType = symVariantType;
			break;

		case astOp::sendMsg:
			if (  (block->right->getOp() == astOp::nameValue) && block->left->getType ( sym ).hasClass() && (classDef = sym->findClass ( block->left->getType ( sym ).getClass () )) )
			{
				if ( (block->left->getOp() == astOp::symbolValue) && (block->left->symbolValue() == file->selfValue) )
				{
					symbolName	= file->selfValue;;
					searchType	= fgxClassElemSearchType::fgxClassElemPrivateAccess;
				} else
				{
					symbolName = cacheString ();
					if ( block->left->getOp() == astOp::symbolValue )
					{
						switch ( sym->getScope ( block->left->symbolValue(), true ) )
						{
							case symbolSpaceScope::symLocal:
							case symbolSpaceScope::symLocalParam:
								symbolName = block->left->symbolValue();
								break;
							default:
								break;
						}
					}

					searchType	= fgxClassElemSearchType::fgxClassElemPublicAccess;
				}
				elem = classDef->getElement ( searchType, block->right->nameValue() );

				if ( elem )
				{
					switch ( elem->type )
					{
						case fgxClassElementType::fgxClassType_iVar:
							if ( symbolName )
							{
								if ( needValue )
								{
									if ( elem->isVirtual )
									{
										codeSegment.putOp ( fglOpcodes::pushObjVirtIVar, sym->getIndex ( symbolName, true ), (int16_t)elem->methodAccess.data.vTabEntry, 0 );
									} else
									{
										codeSegment.putOp ( fglOpcodes::pushObjIVar, sym->getIndex ( symbolName, true ), (int16_t)elem->methodAccess.data.varIndex, 0);
									}
									sym->pushStack ( );
									resType = elem->elem->symType;
								}
							} else
							{
								if ( needValue )
								{
									compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true );
									if ( elem->isVirtual )
									{
										codeSegment.putOp ( fglOpcodes::pushObjVirtIVarRel, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
									} else
									{
										codeSegment.putOp ( fglOpcodes::pushObjIVarRel, 1, (int16_t)elem->methodAccess.data.varIndex, 0);
									}
									sym->pushStack ( );
									codeSegment.putOp ( fglOpcodes::popadup, (uint32_t)1 );	// move us down one stack pos... we need to push ivar, not object/ivar
									sym->popStack( );
									resType = symVariantType;
								}
							}
							break;
						case fgxClassElementType::fgxClassType_prop:
							// push context of assignment method
							if ( !elem->methodAccess.func ) throw errorNum::scINVALID_ACCESS;
							if ( symbolName )
							{
								compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, block->right->nameValue().find ( "::" ) == stringi::npos );
							} else
							{
								compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true );
								if ( elem->isVirtual )
								{
									codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
								} else
								{
									if ( elem->methodAccess.objectStart )
									{
										codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->methodAccess.objectStart, 0 );
									}
								}
							}

							// call the code - result is that the object on the stack is replaced with the result of the function call
							compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, block, sym, needValue, 1, false, elem );
							if ( needValue )
							{
								resType = elem->methodAccess.func->getReturnType();;
							}
							break;
						case fgxClassElementType::fgxClassType_inherit:
							if ( needValue )
							{
								if ( symbolName )
								{
									compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, block->right->nameValue().find ( "::" ) == stringi::npos );
								} else
								{
									compEmitNode ( funcDef, block->left, sym, symWeakObjectType, symObjectType, safeCalls, true );
									if ( elem->isVirtual )
									{
										codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
									} else
									{
										if ( elem->methodAccess.objectStart )
										{
											codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->methodAccess.objectStart, 0 );
										}
									}
								}
								resType = symbolTypeClass ( symbolType::symObject, elem->name );
							}
							break;
						case fgxClassElementType::fgxClassType_static:
						case fgxClassElementType::fgxClassType_const:
							if ( needValue )
							{
								if ( classDef->isInterface )
								{
									codeSegment.putOp ( fglOpcodes::pushClassGlobal, dataSegment.addString ( classDef->name ), dataSegment.addString ( elem->name ) );
								} else
								{
									codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( elem->symbolName, true ) );
								}
								sym->pushStack ( );
								resType = symVariantType;
							}
							break;
						default:
							throw errorNum::scINTERNAL;
					}
					break;
				} else
				{
					if ( (elem = classDef->getDefaultAccess()) )
					{
						// push the symbol name as a string parameter to the call
						compEmitNode ( funcDef, block->right, sym, symStringType, retType, safeCalls,  true ); 	// push message

						if ( symbolName )
						{
							// push context of assignment method
							compEmitContext ( sym, elem, sym->getIndex ( symbolName, true ), true, block->right->nameValue().find ( "::" ) == stringi::npos );
						} else
						{
							compEmitNode ( funcDef, block->left, sym, symObjectType, symObjectType, safeCalls, true );
							if ( elem->isVirtual )
							{
								codeSegment.putOp ( fglOpcodes::modifyContextVirt, 1, (int16_t)elem->methodAccess.data.vTabEntry, 0 );
							} else
							{
								if ( elem->methodAccess.objectStart )
								{
									codeSegment.putOp ( fglOpcodes::modifyContext, 1, elem->methodAccess.objectStart, 0 );
								}
							}
						}

						// call the code
						compEmitFuncCall ( funcDef, elem->methodAccess.func->name, true, block, sym, needValue, 2, false, elem );
						resType = elem->methodAccess.func->getReturnType();;
						break;
					}

					// can't static link, have to generate a dynamic access
					switch ( block->right->getOp() )
					{
						case astOp::nameValue:
							compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
							codeSegment.putOp ( fglOpcodes::objAccess, dataSegment.addString ( block->right->nameValue() ), bssSize );
							bssSize += sizeof ( bcObjQuick );
							break;
						default:
							compEmitNode ( funcDef, block->right, sym, symStringType, retType, safeCalls,  true ); 	// push message
							compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
							codeSegment.putOp ( safeCalls ? fglOpcodes::objAccessIndSafe : fglOpcodes::objAccessInd, (uint32_t) 0 );
							sym->popStack ( );
							break;
					}
					if ( !needValue )
					{
						codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
						sym->popStack ( );
					} else
					{
						resType = symVariantType;
					}
					file->errHandler.throwWarning ( false, warnNum::scwUNDEFINED_CLASS_ENTRY, classDef->name, block->right->nameValue().c_str() );
					break;
				}
			} else if ( (block->right->getOp() == astOp::nameValue) && (block->left->getOp() == astOp::symbolValue) )
			{
				switch ( sym->getScope ( block->left->symbolValue(), true ) )
				{
					case symbolSpaceScope::symClass:
						classDef = sym->findClass (block->left->symbolValue());
						elem = classDef ->getElement ( block->right->nameValue() );

						switch ( elem->type )
						{
							case fgxClassElementType::fgxClassType_static:
							case fgxClassElementType::fgxClassType_const:
								if ( needValue )
								{
									if ( classDef->isInterface )
									{
										codeSegment.putOp ( fglOpcodes::pushClassGlobal, dataSegment.addString ( classDef->name ), dataSegment.addString ( elem->name ) );
									} else
									{
										codeSegment.putOp ( fglOpcodes::pushSymGlobal, sym->getIndex ( elem->symbolName, true ) );
									}
									sym->pushStack ( );
									resType = symVariantType;
								}
								break;
							default:
								throw errorNum::scNOT_STATIC_MEMBER;
						}
						break;
					default:
						// can't static link, have to generate a dynamic access
						switch ( block->right->getOp() )
						{
							case astOp::nameValue:
								compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
								codeSegment.putOp ( fglOpcodes::objAccess, dataSegment.addString ( block->right->nameValue() ), bssSize );
								bssSize += sizeof ( bcObjQuick );
								break;
							default:
								compEmitNode ( funcDef, block->right, sym, symStringType, retType, safeCalls,  true ); 	// push message
								compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
								codeSegment.putOp ( safeCalls ? fglOpcodes::objAccessIndSafe : fglOpcodes::objAccessInd, (uint32_t) 0 );
								break;
						}
						if ( !needValue )
						{
							codeSegment.putOp ( fglOpcodes::pop, (uint32_t ) 1 );
							sym->popStack ( );
						} else
						{
							resType = symVariantType;
						}
						break;
				}
				break;
			}

			// can't static link, have to generate a dynamic access
			switch ( block->right->getOp() )
			{
				case astOp::nameValue:
					compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
					codeSegment.putOp ( fglOpcodes::objAccess, dataSegment.addString ( block->right->nameValue() ), bssSize );
					bssSize += sizeof ( bcObjQuick );
					break;
				default:
					compEmitNode ( funcDef, block->right, sym, symStringType, retType, safeCalls,  true ); 	// push message
					compEmitNode ( funcDef, block->left, sym, symWeakObjectType, retType, safeCalls,  true ); 		// push object
					codeSegment.putOp ( safeCalls ? fglOpcodes::objAccessIndSafe : fglOpcodes::objAccessInd, (uint32_t) 0 );
					sym->popStack ( );
					break;
			}
			if ( !needValue )
			{
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t ) 1 );
				sym->popStack ( );
			} else
			{
				resType = symVariantType;
			}
			break;

		case astOp::getEnumerator:
			switch ( block->left->getType ( sym ).compType() )
			{
				case symbolType::symArray:
					{
						auto cls = sym->findClass ( file->fixedArrayEnumeratorValue );
						compEmitNode ( funcDef, block->left, sym, symArrayType, retType, safeCalls,  true ); 		// push array
						atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( cls->name, atomListType::atomClass ), codeSegment.putOp ( fglOpcodes::objConstruct, (uint32_t) 0, 1 ) );
					}
					break;
				case symbolType::symVArray:
					{
						auto cls = sym->findClass ( file->sparseArrayEnumeratorValue );
						compEmitNode ( funcDef, block->left, sym, symVArrayType, retType, safeCalls,  true ); 		// push array
						atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( cls->name, atomListType::atomClass ), codeSegment.putOp ( fglOpcodes::objConstruct, (uint32_t) 0, 1 ) );
					}
					break;
				case symbolType::symString:
					{
						auto cls = sym->findClass ( file->stringEnumeratorValue );
						compEmitNode ( funcDef, block->left, sym, symStringType, retType, safeCalls,  true ); 		// push array
						atom.addFixup ( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add ( cls->name, atomListType::atomClass ), codeSegment.putOp ( fglOpcodes::objConstruct, (uint32_t) 0, 1 ) );
					}
					break;
				case symbolType::symObject:
					{
						astNode newNode ( file->sCache, astOp::funcCall );
						newNode.left		= (new astNode ( file->sCache, astOp::sendMsg ))->setLocation ( block );
						newNode.left->left	= block->left;
						newNode.left->right	= (new astNode ( file->sCache, astOp::nameValue, "GetEnumerator" ))->setLocation ( block );
						compEmitNode ( funcDef, &newNode, sym, symWeakObjectType, retType, safeCalls,  true );

						newNode.left->left = 0;
					}
					break;
				case symbolType::symVariant:
					compEmitNode ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  true ); 		// push variant
					codeSegment.putOp ( fglOpcodes::makeEnumerator, (uint32_t ) 1 );
					break;
				default:
					throw errorNum::scNOT_ENUMERABLE;
			}
			if ( !needValue )
			{
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t ) 1 );
				sym->popStack ( );
			}
			resType = symWeakObjectType;
			break;
		case astOp::range:
			compEmitNode ( funcDef, block->right, sym, symIntType, retType, safeCalls,  needValue );
			compEmitNode ( funcDef, block->left, sym, symIntType, retType, safeCalls,  needValue );
			if ( needValue )
			{
				atom.addFixup( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add( sym->getFuncName( file->rangeValue, true ), atomListType::atomRef ), codeSegment.putOp( fglOpcodes::callBC, (uint32_t)0, 2) );
				sym->popStack ( 1 ); 	// pop 2 params, push result
			} else
			{
				atom.addFixup( fgxAtomFixupType::fgxAtomFixup_bcFunc, atom.add( sym->getFuncName( file->rangeValue, true ), atomListType::atomRef ), codeSegment.putOp( fglOpcodes::callBCPop, (uint32_t)0, 2 ) );
				sym->popStack ( 2 );	// pop 2 params
			}
			resType = symWeakObjectType;
			break;

		case astOp::workAreaStart:
			switch ( block->right->getOp() )
			{
				case astOp::intValue:
				case astOp::doubleValue:
				case astOp::varrayValue:
				case astOp::arrayValue:
				case astOp::symbolPack:
				case astOp::codeBlockValue:
				case astOp::atomValue:
				case astOp::nullValue:
				case astOp::nilValue:
				case astOp::fieldValue:
				case astOp::funcValue:
				case astOp::pairValue:
					throw errorNum::scINVALID_FIELD;
				default:
					break;
			}
			switch ( block->left->getOp() )
			{
				case astOp::symbolValue:
					if ( !strccmp ( block->left->symbolValue(), "field" ) )
					{
						// special case for default workarea field access
						switch ( block->right->getOp() )
						{
							case astOp::stringValue:
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waPushField, dataSegment.addString ( block->right->stringValue() ) );
									sym->pushStack ( );
								}
								break;
							default:
								compEmitNode ( funcDef, block->right, sym, symVariantType, retType, safeCalls,  true ); 
								codeSegment.putOp ( fglOpcodes::waPushFieldInd );
								break;
						}
					} else
					{
						switch ( block->right->getOp() )
						{
							case astOp::stringValue:
								if ( needValue )
								{
									codeSegment.putOp ( fglOpcodes::waStarts, dataSegment.addString ( block->left->symbolValue() ) );						
									codeSegment.putOp ( fglOpcodes::waPushField, dataSegment.addString ( block->right->stringValue() ) );
									sym->pushStack ( );
									codeSegment.putOp ( fglOpcodes::waEnd );
								}
								break;
							default:
								codeSegment.putOp ( fglOpcodes::waStarts, dataSegment.addString ( block->left->symbolValue() ) );						
								compEmitNode ( funcDef, block->right, sym, symVariantType, retType, safeCalls,  true ); 
								codeSegment.putOp ( fglOpcodes::waPushFieldInd );
								codeSegment.putOp ( fglOpcodes::waEnd );
								break;
						}
					}
					resType = symVariantType;
					break;
				case astOp::intValue:
					switch ( block->right->getOp() )
					{
						case astOp::stringValue:
							if ( needValue )
							{
								codeSegment.putOp ( fglOpcodes::waStarti, (uint32_t) block->left->intValue() );						
								codeSegment.putOp ( fglOpcodes::waPushField, dataSegment.addString ( block->right->stringValue() ) );
								sym->pushStack ( );
								codeSegment.putOp ( fglOpcodes::waEnd );
							}
							break;
						default:
							codeSegment.putOp ( fglOpcodes::waStarti, (uint32_t) block->left->intValue() );						
							compEmitNode ( funcDef, block->right, sym, symVariantType, retType, safeCalls,  true ); 
							codeSegment.putOp ( fglOpcodes::waPushFieldInd );
							codeSegment.putOp ( fglOpcodes::waEnd );
							break;
					}
					resType = symVariantType;
					break;
				default:
					switch ( block->right->getOp() )
					{
						case astOp::stringValue:
							if ( needValue )
							{
								compEmitNode ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  true ); 
								switch ( block->left->getType ( sym).compType() )
								{
									case symbolType::symString:
										codeSegment.putOp ( fglOpcodes::waStartInds );
										break;
									case symbolType::symInt:
										codeSegment.putOp ( fglOpcodes::waStartIndi );
										break;
									case symbolType::symVariant:
										codeSegment.putOp ( fglOpcodes::waStartInd );
										break;
									default:
										throw errorNum::scINVALID_WORKAREA;
								}
								sym->popStack ( );
								codeSegment.putOp ( fglOpcodes::waPushField, dataSegment.addString ( block->right->stringValue() ) );
								sym->pushStack ( );
								codeSegment.putOp ( fglOpcodes::waEnd );
							}
							resType = symVariantType;
							break;
						default:
							compEmitNode ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  true ); 
							switch ( block->left->getType ( sym).compType() )
							{
								case symbolType::symString:
									codeSegment.putOp ( fglOpcodes::waStartInds );
									break;
								case symbolType::symInt:
									codeSegment.putOp ( fglOpcodes::waStartIndi );
									break;
								case symbolType::symVariant:
									codeSegment.putOp ( fglOpcodes::waStartInd );
									break;
								default:
									throw errorNum::scINVALID_WORKAREA;
							}
							sym->popStack ( );
							resType = compEmitNode ( funcDef, block->right, sym, symVariantType, retType, safeCalls,  true );
							codeSegment.putOp ( fglOpcodes::waEnd );
							break;
					}
					break;
			}
			if ( !needValue )
			{
				codeSegment.putOp ( fglOpcodes::pop, (uint32_t ) 1 );
				sym->popStack ( );
			}
			break;
			
		case astOp::refCreate:
#if 1
			if ( needValue )
			{
				compEmitLValue ( funcDef, block->left, sym, symVariantType, retType, safeCalls,  needValue );
				resType = symVariantType;
			}
			break;
#else
			//TODO:  need some work before we can re-enable this. lambda's take captures as references and save them away.  however if the reference is to a stack variable then it's pointing to a temp
			//		 to do this we need to prove a few things... that the parameter never has it's reference taken (e.g used non-dereferenced), and not used in an initializer (which does an implicit @)
			if ( needValue )
			{
				switch ( block->left->getOp() )
				{
					case astOp::symbolValue:
						codeSegment.putOp ( fglOpcodes::pushSymLocalRef, sym->getIndex ( block->left->data.s, true ) );
						sym->pushStack ( );
						break;
					case astOp::sendMsg:
						compEmitLValue ( funcDef, block, sym, symVariantType, retType, safeCalls,  true );
						break;
					default:
						throw errorNum::scILLEGAL_REFERENCE;
				}
			}
			resType = symVariantType;
			break;
#endif
		case astOp::refPromote:
			compEmitLValue ( funcDef, block->left, sym, needType, retType, safeCalls,  needValue );
			resType = symVariantType;
			break;
		case astOp::paramExpand:
			// DANGER   after this emissions we can NO LONGER ensure that the sym matches the stack.   Because of this we must ensure that nothing else is inlined.
			// THEREFORE we must ensure that any function calls have NO parameter expansion in them when we're inlining!!!
			compEmitNode ( funcDef, block->left, sym, symArrayType, retType, safeCalls,  true );
			codeSegment.putOp ( fglOpcodes::pushParamPack );
			resType = symVariantType;
			break;
		case astOp::symbolDef:
			{
				listing.emitSymbolInfo ( sym, block->symbolDef() );
				listing.emitContSource ( block->location );
				switch ( block->symbolDef()->symType )
				{
					case symbolSpaceType::symTypeStatic:
						{
							auto init = block->symbolDef()->getInitializer (block->symbolDef()->getSymbolName () );

							// emit a once around the static initializer
							auto tmpLabel1 = makeLabel ( "static_once%" );
							fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::once, (uint32_t) 0, bssSize ) );
							bssSize++;

							if ( init->getOp() == astOp::assign )
							{
								debug.addEntry ( init->location, codeSegment.getNumOps (), sym->getStackSize () );
								compEmitNode ( funcDef, init->right, sym, block->symbolDef()->getType ( block->symbolDef()->getSymbolName ( ), true ), symVariantType, safeCalls, true );
							} else
							{
								compEmitDefaultValue ( funcDef, sym, block->symbolDef ()->getType ( block->symbolDef ()->getSymbolName (), true ) );
								sym->pushStack ( );
							}
							sym->popStack ( );				// this doesn't really occur in real life here... it will get popped at the sstore, howeve we need to do it now or w'll pop the symbolDef() push!
							sym->push ( block->symbolDef() );

							// store it into the global's area
							codeSegment.putOp ( fglOpcodes::storeGlobalPop, sym->getIndex ( block->symbolDef()->getSymbolName(), true ) );

							// label for the static initializer once instruction
							codeSegment.emitLabel ( tmpLabel1 );
							fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
							listing.label ( sym, tmpLabel1 );
						}
						break;
					case symbolSpaceType::symTypeLocal:
						{
							auto init = block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName ( ) );

							if ( init->getOp () == astOp::assign )
							{
								debug.addEntry ( init->right->location, codeSegment.getNumOps ( ), sym->getStackSize () );
								compEmitNode ( funcDef, init->right, sym, block->symbolDef()->getType ( block->symbolDef()->getSymbolName ( ), true ), symVariantType, safeCalls, true );
							} else
							{
								compEmitDefaultValue ( funcDef, sym, block->symbolDef ()->getType ( block->symbolDef ()->getSymbolName (), true ) );
								sym->pushStack ( );
							}

							// need to pop off our stack" value indicator and replace it with the symbol definition
							sym->popStack ( );
							sym->push ( block->symbolDef() );
						}
						break;
					case symbolSpaceType::symTypeParam:
						{
							auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
							for ( size_t it = symbol->size ( ); it; it-- )
							{
								auto init = (*symbol)[(size_t) it - 1]->initializer;
								if ( init->getOp ( ) == astOp::assign )
								{
									debug.addEntry ( init->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
									compEmitNode ( funcDef, init->right, sym, (*symbol)[(size_t) it - 1]->getType ( ), symVariantType, safeCalls, true );
								} else
								{
									compEmitDefaultValue ( funcDef, sym, (*symbol)[(size_t) it - 1]->getType ( ) );
									sym->pushStack ();
								}
							}
							// it's still on the symbol stack but we need to pop the temporary stack value definitions.   It's "ownership" will then be covered by the symmbol definition rather than the stack value
							sym->popStack ( symbol->size() );
							sym->push ( block->symbolDef() );
						}
						break;
					case symbolSpaceType::symTypeClass:
						{
							auto cl = dynamic_cast<symbolClass *>(block->symbolDef());
							sym->insert ( block->symbolDef(), cl->insertPos );
						}
						break;
					case symbolSpaceType::symTypeInline:
					case symbolSpaceType::symTypeField:
						sym->push ( block->symbolDef() );
						break;
					default:
						throw errorNum::scILLEGAL_OPERAND;
				}
				listing.emitOps ( sym );

				visibleSymbols	fgxSym;

				for ( size_t loop = 0; loop < block->symbolDef()->size ( ); loop++ )
				{
					if ( block->symbolDef()->getFgxSymbol ( fgxSym, loop ) )
					{
						fgxSym.isStackBased = block->symbolDef()->isStackBased ();
						fgxSym.index = sym->getIndex ( fgxSym.name, true );
						fgxSym.validStart = codeSegment.getLen ( );
						fgxSym.validEnd = 0;
						fgxSym.isParameter = block->symbolDef ()->symType == symbolSpaceType::symTypeParam;
						symbols.push_back ( fgxSym );
					}
				}
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym->size ( );
				auto nPop = sym->getStackSize ( );
				auto symbolStart = symbols.size ( );
				for ( auto it = block->basicData().blocks.begin ( ); it != block->basicData().blocks.end ( ); it++ )
				{
					switch ( (*it)->getOp() )
					{
						case astOp::btBasic:
						case astOp::btInlineRet:
						case astOp::btFor:
						case astOp::btForEach:
						case astOp::btReturn:
						case astOp::btLabel:
						case astOp::btGoto:
						case astOp::btInline:
						case astOp::btIf:
						case astOp::btWhile:
						case astOp::btRepeat:
						case astOp::btSwitch:
						case astOp::btTry:
						case astOp::btCont:
						case astOp::btBreak:
						case astOp::btAnnotation:
						case astOp::btExit:
						case astOp::btExcept:
						case astOp::symbolDef:
							// don't emit debug entries for things we won't really be emitting code for directly
							// don't emit source for the above... the entries will handle it individually
							break;
						default:
							debug.addEntry ( (*it)->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
							listing.emitContSource ( (*it)->location );
							break;
					}
					compEmitNode ( funcDef, (*it), sym, symVariantType, retType, safeCalls,  false );
					listing.emitOps ( sym );
				}
				auto newSize = sym->getStackSize ();
				if ( newSize > nPop )
				{
					nPop = sym->getStackSize () - nPop;
					if ( block->basicData().emitPops )
					{
						codeSegment.putOp ( fglOpcodes::pop, (uint32_t) nPop );
					}
				}
				if ( sym->size () > size )
				{
					sym->resize ( size );
				}
				for ( size_t loop = symbolStart; loop < symbols.size ( ); loop++ )
				{
					if ( !symbols[loop].validEnd )
					{
						symbols[loop].validEnd = codeSegment.getLen ( ) - 1;
					}
				}
			}
			// need to do this for inlining purposes... we emit with a need type, and this may cause a conversion error if we don't make resType something valid
			// inline emits with a need so if we have something complex inlined it will be a basic block as the first thing and that may cause an error without this
			resType = retType;
			break;
		case astOp::btInlineRet:
			if ( block->returnData ().finallyBlock )
			{
				debug.addEntry ( block->returnData ().finallyBlock->location, codeSegment.getNumOps (), sym->getStackSize () );
				compEmitNode ( funcDef, block->returnData ().finallyBlock, sym, symVariantType, symVariantType, safeCalls, false );
			}
			debug.addEntry ( block->location, codeSegment.getNumOps (), sym->getStackSize () );
			if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
			listing.emitContSource ( block->location );
			if ( block->returnData ().node )
			{
				resType = compEmitNode ( funcDef, block->returnData ().node, sym, retType, retType, safeCalls, sym->getInlineFuncNeedValue () );

				if ( sym->getInlineFuncNeedValue () )
				{
					codeSegment.putOp ( fglOpcodes::move, sym->getInlineFuncRetIndex () );
					sym->popStack ();
				}
			} else
			{
				// we do NOT need to push a null... a null is pushed when we begin our inline as the placeholder for the return, so we need do nothing.
				// codeSegment.putOp ( fglOpcodes::pushNull );
			}
			if ( block->returnData ().nPop )
			{
				codeSegment.putOp ( fglOpcodes::pop, block->returnData ().nPop );
			}
			if ( block->returnData ().label.size () )
			{
				fixup->needFixup ( block->returnData ().label, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t)0 ) );
			}
			listing.emitOps ( sym );
			break;
		case astOp::btInline:
			{
				errorLocality e ( &file->errHandler, block, true );

				// while it might seem to make sense to start the symbol stack from scratch, we have a number of issues...
				// a simple thing like inline a method in the same class will have problems because self will belong to the caller
				// we may rename variables rather than create a new one and copy for variables that are never modified.  those would no longer be accessible

				// TODO: construct a NEW symbol stack, this one has the proper class, namespace, from the INLINED function, but all the stack visible scope from the OLD
				//		 this should simplify the symbol lookup immensly as we don't have to have "modes" any more
				// start listing at start of inlined node
				listing.pushLine ( block->inlineFunc ().node->location );
				listing.emitContSource ( block->inlineFunc ().node->location );
				debug.addEntry ( block->inlineFunc ().node->location, codeSegment.getNumOps (), sym->getStackSize () );

				symbolSpaceInline inlineSym ( block->inlineFunc ().funcName );

				inlineSym.needValue = needValue;
				if ( needValue )
				{
					inlineSym.retIndex = sym->getStackSize ();
					codeSegment.putOp ( fglOpcodes::pushNull );	// return location
					sym->pushStack ( "inline return for: %s", block->inlineFunc ().funcName.c_str() );
				}

				sym->push ( &inlineSym );

				compEmitNode ( funcDef, block->inlineFunc ().node, sym, needType, needType, safeCalls, needValue );

				sym->pop ();
				listing.popLine ( block->inlineFunc ().node->location );
				listing.emitOps ( sym );
				listing.printf ( ";                                                                          end of inline fuction: %s\r\n", block->inlineFunc ().funcName.c_str () );
				resType = needType;
			}
			break;
		case astOp::btLabel:
			listing.emitContSource ( block->location );
			codeSegment.emitLabel ( block->label() );
			fixup->setRefPoint ( block->label(), codeSegment.nextOp() );
			listing.label ( sym, block->label() );
			break;
		case astOp::btGoto:
			listing.emitContSource ( block->location );
			if ( block->gotoData().finallyBlock )
			{
				debug.addEntry ( block->gotoData().finallyBlock->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
				compEmitNode ( funcDef, block->gotoData().finallyBlock, sym, symVariantType, symVariantType, safeCalls, false );
			}
			debug.addEntry ( block->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
			if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
			if ( block->gotoData().nPop )
			{
				codeSegment.putOp ( fglOpcodes::pop, block->gotoData().nPop );
			}
			if ( block->gotoData().label.size ( ) )
			{
				fixup->needFixup ( block->gotoData().label, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
			}
			listing.emitOps ( sym );
			break;
		case astOp::btReturn:
			if ( block->returnData().finallyBlock )
			{
				debug.addEntry ( block->returnData().finallyBlock->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
				compEmitNode ( funcDef, block->returnData().finallyBlock, sym, symVariantType, symVariantType, safeCalls, false );
			}
			debug.addEntry ( block->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
			listing.emitContSource ( block->location );
			if ( !funcDef || (funcDef && !funcDef->isProcedure) )
			{
				if ( block->returnData().node )
				{
					debug.addEntry ( block->returnData().node->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
					if ( (block->returnData().node->getOp ( ) == astOp::funcCall) && (block->returnData().node->getType ( sym ) == retType ) )
					{
						resType = compEmitFunc ( funcDef, block->returnData().node, sym, needType, retType, safeCalls,  true, true );
					} else
					{
						resType = compEmitNode ( funcDef, block->returnData().node, sym, retType, retType, safeCalls, true );
					}
				} else
				{
					codeSegment.putOp ( fglOpcodes::pushNull );
					sym->pushStack ( );
					resType = symVariantType;
				}
				switch ( resType.compType ( ) )
				{
					case symbolType::symVariant:
						// need to dereference the value.
						if ( !block->returnData().node || (block->returnData().node && (block->returnData().node->getOp ( ) == astOp::refPromote)) )
						{
							codeSegment.putOp ( fglOpcodes::result, (uint32_t) 0 );
						} else
						{
							codeSegment.putOp ( fglOpcodes::resultv, (uint32_t) 0 );
						}
						break;
					default:
						codeSegment.putOp ( fglOpcodes::result, (uint32_t) 0 );
						break;
				}
				sym->popStack ( );
			}
			if ( block->returnData().nPop )
			{
				// we don't care about this pop... this is a jmp pop and does not effect the stack state for code emissions purposes
				// do we really need this?  stack is restored by caller to proper spot so cleanup need not be done
				// codeSegment.putOp ( fglOpcodes::pop, block->returnData().nPop );
			}
			listing.emitOps( sym );
			if ( block->returnData().label.size ( ) )
			{
				fixup->needFixup ( block->returnData().label, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
			}
			listing.emitOps ( sym );
			break;
		case astOp::btBreak:
			if ( block->gotoData().finallyBlock )
			{
				debug.addEntry ( block->gotoData().finallyBlock->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
				compEmitNode ( funcDef, block->gotoData().finallyBlock, sym, symVariantType, symVariantType, safeCalls, false );
			}
			debug.addEntry ( block->location, codeSegment.getNumOps(), sym->getStackSize () );
			listing.emitContSource ( block->location );
			if ( block->gotoData().nPop )
			{
				codeSegment.putOp ( fglOpcodes::pop, block->gotoData().nPop );
			}
			if ( block->gotoData().label.size ( ) )
			{
				fixup->needFixup ( block->gotoData().label, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
			}
			break;
		case astOp::btCont:
			if ( block->gotoData().finallyBlock )
			{
				debug.addEntry ( block->gotoData().finallyBlock->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
				compEmitNode ( funcDef, block->gotoData().finallyBlock, sym, symVariantType, symVariantType, safeCalls, false );
			}
			debug.addEntry ( block->location, codeSegment.getNumOps(), sym->getStackSize () );
			listing.emitContSource ( block->location );
			if ( block->gotoData().nPop )
			{
				codeSegment.putOp ( fglOpcodes::pop, block->gotoData().nPop );
			}
			if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
			if ( block->gotoData().label.size ( ) )
			{
				fixup->needFixup ( block->gotoData().label, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
			}
			break;
		case astOp::btTry:
			{
				index = codeSegment.nextOp ();		// start of try block

				// this is the stack size we need to be at if we need to execute the catch block... any temporary or block locals emitted during the try/final combination will need to be removed
				//    this is the stack size we need to remove to and is encoded in the try/catch information 
				auto stackRestoreSize = (uint32_t)sym->getStackSize ();

				if ( block->tryCatchData().tryBlock )
				{
					listing.emitContSource ( block->location );
					codeSegment.emitPush ( );
					compEmitNode ( funcDef, block->tryCatchData().tryBlock, sym, needType, retType, safeCalls, false );
					codeSegment.emitPop ( );
				}
				if ( block->tryCatchData().finallyBlock )
				{
					codeSegment.emitPush ( );
					compEmitNode ( funcDef, block->tryCatchData().finallyBlock, sym, needType, retType, safeCalls, false );
					codeSegment.emitPop ( );
				}

				auto tmpLabel1 = makeLabel ( "try_end%" );
				fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t)0 ) );
				listing.emitOps ( sym );
				if ( block->tryCatchData().tryBlock )
				{
					listing.emitContSourceEnd ( block->tryCatchData().tryBlock->location );
				}
				tcList.add ( index, codeSegment.nextOp (), sym->getIndex ( block->tryCatchData().errSymbol->symbolValue(), true ), stackRestoreSize );
				if ( block->tryCatchData().catchBlock )
				{
					listing.emitContSource ( block->tryCatchData().catchBlock->location );
					codeSegment.emitPush ();
					if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugTrap );
					compEmitNode ( funcDef, block->tryCatchData().catchBlock, sym, needType, retType, safeCalls, false );
					codeSegment.emitPop ();
					listing.emitOps ( sym );
				} else
				{
					codeSegment.emitPush ();
					if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugTrap );
					codeSegment.emitPop ();
					listing.emitOps ( sym );
				}

				if ( block->tryCatchData().finallyBlock )
				{
					codeSegment.emitPush ();
					compEmitNode ( funcDef, block->tryCatchData().finallyBlock, sym, needType, retType, safeCalls,  false );
					codeSegment.emitPop ();
				}
				codeSegment.emitLabel ( tmpLabel1 );
				fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp () );
				listing.label ( sym, tmpLabel1 );
			}
			break;
		case astOp::btFor:
			listing.emitContSource ( block->location );

			// emit initializers
			if ( block->loopData().initializers )
			{
				debug.addEntry ( block->loopData().initializers->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
				compEmitNode ( funcDef, block->loopData().initializers, sym, symVariantType, retType, safeCalls,  false );
			}

			if ( !block->loopData().condition || (block->loopData().condition && !block->loopData().condition->isFalse ()) )
			{
				// emit entry condition code
				auto tmpLabel2 = makeLabel ( "for_end%" );

				bool complexCondition = false;
				auto complexConditionLabel = makeLabel ( "for_alt_loop%" );

				// for complex conditions, where repeating the body in the code can lead to massive code bloat (large inlined conditions being one)
				// we emit a slightly less optimal code shape.   rather then doing condition body condition, we do a condition body and then loop back to start of condition;
				if ( block->loopData().condition->complexity () > 30 )
				{
					complexCondition = true;
					codeSegment.emitLabel ( complexConditionLabel );
					fixup->setRefPoint ( complexConditionLabel, codeSegment.nextOp ( ) );
					listing.label ( sym, complexConditionLabel );
				}

				if ( block->loopData().condition && !block->loopData().condition->isTrue() )
				{
					listing.emitOps( sym );
					listing.emitContSource( block->loopData().condition->location );
					debug.addEntry ( block->loopData().condition->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );

					if ( block->loopData().condition->getOp() == astOp::logicalNot )
					{
						condType = compEmitNode ( funcDef, block->loopData().condition->left, sym, symVariantType, retType, safeCalls,  true );
						if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
						switch ( condType.compType ( ) )
						{
							case symbolType::symInt:
							case symbolType::symBool:
								fixup->needFixup ( tmpLabel2, codeSegment.putOp ( fglOpcodes::jmpcpop, (uint32_t) 0 ) );
								break;
							default:
								fixup->needFixup ( tmpLabel2, codeSegment.putOp ( fglOpcodes::jmpcpopv, (uint32_t) 0 ) );
								break;
						}
						fixup->needFixup ( tmpLabel2, codeSegment.putOp ( fglOpcodes::jmpcpop, (uint32_t) 0 ) );
						sym->popStack ( );
					} else
					{
						condType = compEmitNode ( funcDef, block->loopData().condition, sym, symVariantType, retType, safeCalls,  true );
						if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
						switch ( condType.compType ( ) )
						{
							case symbolType::symInt:
							case symbolType::symBool:
								fixup->needFixup ( tmpLabel2, codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
								break;
							default:
								fixup->needFixup ( tmpLabel2, codeSegment.putOp ( fglOpcodes::jmpncpopv, (uint32_t) 0 ) );
								break;
						}
						sym->popStack ( );
					}
				}

				auto tmpLabel1 = makeLabel ( "for_loop%" );
				codeSegment.emitLabel ( tmpLabel1 );
				fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
				listing.label ( sym, tmpLabel1 );

				// emit the block
				if ( block->loopData().block )
				{
					listing.emitOps( sym );
					listing.emitContSource( block->loopData().block->location );
					codeSegment.emitPush();
					debug.addEntry ( block->loopData().block->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
					compEmitNode ( funcDef, block->loopData().block, sym, symVariantType, retType, safeCalls,  false );
					codeSegment.emitPop();
				}

				debug.addEntry ( block->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );

				codeSegment.emitLabel( block->loopData().continueLabel );
				fixup->setRefPoint( block->loopData().continueLabel, codeSegment.nextOp() );

				listing.label( sym, block->loopData().continueLabel );

				if ( !block->loopData().block->alwaysReturns () )
				{
					// emit the increasers
					if ( block->loopData().increase )
					{
						compEmitNode ( funcDef, block->loopData().increase, sym, symVariantType, retType, safeCalls,  false );
					}

					listing.emitOps ( sym );
					listing.emitContSourceEnd ( block->location );

					// emit loop condition code
					if ( block->loopData().condition && !block->loopData().condition->isTrue ( ) )
					{
						if ( complexCondition )
						{
							fixup->needFixup ( complexConditionLabel, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
						} else
						{
							block->loopData().condition->fixupReturn ( sym, funcDef );
							// emit the condition
							if ( block->loopData().condition->getOp () == astOp::logicalNot )
							{
								condType = compEmitNode ( funcDef, block->loopData().condition->left, sym, symIntType, retType, safeCalls,  true );
								if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
								switch ( condType.compType () )
								{
									case symbolType::symInt:
									case symbolType::symBool:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
										break;
									default:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpncpopv, (uint32_t) 0 ) );
										break;
								}
								sym->popStack ();

								codeSegment.emitLabel ( tmpLabel2 );
								fixup->setRefPoint ( tmpLabel2, codeSegment.nextOp () );
								listing.label ( sym, tmpLabel2 );
							} else
							{
								condType = compEmitNode ( funcDef, block->loopData().condition, sym, symVariantType, retType, safeCalls,  true );
								if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
								switch ( condType.compType () )
								{
									case symbolType::symInt:
									case symbolType::symBool:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpcpop, (uint32_t) 0 ) );
										break;
									default:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpcpopv, (uint32_t) 0 ) );
										break;
								}
								sym->popStack ();
							}
						}
					} else
					{
						// no conditions specified... this means always true;
						fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
					}
				}

				codeSegment.emitLabel ( tmpLabel2 );
				fixup->setRefPoint ( tmpLabel2, codeSegment.nextOp ( ) );
				listing.label ( sym, tmpLabel2 );

				codeSegment.emitLabel( block->loopData().breakLabel );
				fixup->setRefPoint( block->loopData().breakLabel, codeSegment.nextOp() );
				listing.label( sym, block->loopData().breakLabel );
			}
			break;
		case astOp::btIf:
			{
				char						 ifLabel[256];
				char						 ifEndLabel[256];

				if ( funcDef && funcDef->location.sourceIndex == 4 )
				{
					printf ( "" );
				}

				listing.emitContSource ( block->location );

				sprintf_s ( ifEndLabel, sizeof ( ifLabel ), "$if_end%u", labelId++ );

				for ( auto it = block->ifData().ifList.begin ( ); it != block->ifData().ifList.end ( ); it++ )
				{
					bool emitIfLabel = false;

					sprintf_s ( ifLabel, sizeof ( ifLabel ), "$if_label%u", labelId++ );

					// emit any condition side effects
					if ( (*it)->condition )
					{
						if ( !(*it)->block && (it + 1 == block->ifData().ifList.end ( )) && !block->ifData().elseBlock )
						{
							if ( !(*it)->condition->hasNoSideEffects ( sym, false ) )
							{
								listing.emitOps ( sym );
								listing.emitContSourceEnd ( (*it)->condition->location );
								debug.addEntry ( (*it)->condition->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
								compEmitNode ( funcDef, (*it)->condition, sym, symVariantType, symVariantType, safeCalls, false );
							}
							break;
						}
						if ( (*it)->condition->isTrue ( ) )
						{
							if ( !(*it)->condition->hasNoSideEffects ( sym, false ) )
							{
								// condition has side-effects so emit it
								listing.emitOps ( sym );
								listing.emitContSourceEnd ( (*it)->condition->location );
								debug.addEntry ( (*it)->condition->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
								compEmitNode ( funcDef, (*it)->condition, sym, symVariantType, symVariantType, safeCalls, false );
							}
							// emit the block
							listing.emitOps ( sym );
							codeSegment.emitPush ( );
							compEmitNode ( funcDef, (*it)->block, sym, symVariantType, symVariantType, safeCalls, false );
							codeSegment.emitPop ( );
							if ( block->ifData().elseBlock && !(*it)->block->alwaysReturns() )
							{
								fixup->needFixup ( file->sCache.get ( ifEndLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
							}
							listing.emitOps ( sym );
						} else
						{
							bool negated = false;
							listing.emitOps ( sym );
							listing.emitContSourceEnd ( (*it)->condition->location );
							debug.addEntry ( (*it)->condition->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
							if ( (*it)->condition->getOp ( ) == astOp::logicalNot )
							{
								negated = true;
								condType = compEmitNode ( funcDef, (*it)->condition->left, sym, symVariantType, retType, safeCalls, true );
							} else
							{
								condType = compEmitNode ( funcDef, (*it)->condition, sym, symVariantType, retType, safeCalls, true );
							}
							switch ( condType.compType ( ) )
							{
								case symbolType::symInt:
								case symbolType::symBool:
									if ( negated )
									{
										fixup->needFixup ( file->sCache.get ( ifLabel ), codeSegment.putOp ( fglOpcodes::jmpcpop, (uint32_t) 0 ) );
									} else
									{
										fixup->needFixup ( file->sCache.get ( ifLabel ), codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
									}
									break;
								default:
									if ( negated )
									{
										fixup->needFixup ( file->sCache.get ( ifLabel ), codeSegment.putOp ( fglOpcodes::jmpcpopv, (uint32_t) 0 ) );
									} else
									{
										fixup->needFixup ( file->sCache.get ( ifLabel ), codeSegment.putOp ( fglOpcodes::jmpncpopv, (uint32_t) 0 ) );
									}
									break;
							}
							emitIfLabel = true;
							sym->popStack ( );
							if ( (*it)->block )
							{
								listing.emitOps ( sym );
								listing.emitContSource ( (*it)->block->location );
								codeSegment.emitPush ( );
								compEmitNode ( funcDef, (*it)->block, sym, symVariantType, retType, safeCalls,  false );
								codeSegment.emitPop ( );
								listing.emitOps ( sym );
								if ( (!(it + 1 == block->ifData().ifList.end ( )) || block->ifData().elseBlock) && !(*it)->block->alwaysReturns () )
								{
									fixup->needFixup ( file->sCache.get ( ifEndLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
								}
							} else 
							{
								if ( (!(it + 1 == block->ifData().ifList.end ( )) || block->ifData().elseBlock) && !(*it)->block->alwaysReturns () )
								{
									listing.emitOps ( sym );
									fixup->needFixup ( file->sCache.get ( ifEndLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
								}
							}
						}
					} else // if it doesn't have a condition, this is an always false case, but the block has a valid jmp target so we still need to emit the block
					{
						if ( (*it)->block )
						{
							if ( (it + 1 != block->ifData().ifList.end ( )) && !block->ifData().elseBlock )
							{
								emitIfLabel = true;
								fixup->needFixup ( file->sCache.get ( ifLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
							}
							codeSegment.emitPush ( );
							compEmitNode ( funcDef, (*it)->block, sym, symVariantType, retType, safeCalls,  false );
							codeSegment.emitPop ( );
							if ( (block->ifData().elseBlock || (it + 1 != block->ifData().ifList.end ())) && !(*it)->block->alwaysReturns () )
							{
								fixup->needFixup ( file->sCache.get ( ifEndLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
							}
							listing.emitOps ( sym );
						}
					}
					listing.emitOps ( sym );
					if ( (*it)->block )
					{
						listing.emitContSourceEnd ( (*it)->block->location );
					} else
					{
						listing.emitContSourceEnd ( (*it)->condition->location );
					}
					if ( emitIfLabel )
					{
						codeSegment.emitLabel ( ifLabel );
						fixup->setRefPoint ( file->sCache.get ( ifLabel ), codeSegment.nextOp ( ) );
						listing.label ( sym, ifLabel );
					}
				}
				if ( block->ifData().elseBlock )
				{
					codeSegment.emitPush ( );
					compEmitNode ( funcDef, block->ifData().elseBlock, sym, symVariantType, retType, safeCalls,  false );
					codeSegment.emitPop ( );
					listing.emitOps ( sym );
					listing.emitContSourceEnd ( block->ifData().elseBlock->location );
				}
				listing.emitOps ( sym );
				codeSegment.emitLabel ( ifEndLabel );
				listing.label ( sym, ifEndLabel );
				fixup->setRefPoint ( file->sCache.get ( ifEndLabel ), codeSegment.nextOp ( ) );
				listing.emitOps ( sym );
			}
			break;
		case astOp::btForEach:
			throw errorNum::scINTERNAL;		// for each should have been transformed before this point
		case astOp::btWhile:
			{
				listing.emitContSource ( block->location );

				bool complexCondition = false;
				auto complexConditionLabel = makeLabel ( "while_alt_loop%" );

				if ( block->loopData().condition->complexity () > 30 )
				{
					complexCondition = true;
					codeSegment.emitLabel ( complexConditionLabel );
					fixup->setRefPoint ( complexConditionLabel, codeSegment.nextOp ( ) );
					listing.label ( sym, complexConditionLabel );
				}

				// emit entry condition code
				if ( block->loopData().condition )
				{
					debug.addEntry ( block->loopData().condition->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
					if ( block->loopData().condition->getOp ( ) == astOp::logicalNot )
					{
						condType = compEmitNode ( funcDef, block->loopData().condition->left, sym, symVariantType, retType, safeCalls,  true );
						if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
						switch ( condType.compType ( ) )
						{
							case symbolType::symInt:
							case symbolType::symBool:
								fixup->needFixup ( block->loopData().breakLabel, codeSegment.putOp ( fglOpcodes::jmpcpop, (uint32_t) 0 ) );
								break;
							default:
								fixup->needFixup ( block->loopData().breakLabel, codeSegment.putOp ( fglOpcodes::jmpcpopv, (uint32_t) 0 ) );
								break;
						}
						sym->popStack ( );
					} else
					{
						condType = compEmitNode ( funcDef, block->loopData().condition, sym, symVariantType, retType, safeCalls,  true );
						if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
						switch ( condType.compType ( ) )
						{
							case symbolType::symInt:
							case symbolType::symBool:
								fixup->needFixup ( block->loopData().breakLabel, codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
								break;
							default:
								fixup->needFixup ( block->loopData().breakLabel, codeSegment.putOp ( fglOpcodes::jmpncpopv, (uint32_t) 0 ) );
								break;
						}
						sym->popStack ( );
					}
				}

				// this is our loop point
				auto tmpLabel1 = makeLabel ( "while_loop%" );
				codeSegment.emitLabel ( tmpLabel1 );
				fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
				listing.label ( sym, tmpLabel1 );

				// emit the block
				if ( block->loopData().block )
				{
					codeSegment.emitPush ( );
					compEmitNode ( funcDef, block->loopData().block, sym, symVariantType, retType, safeCalls,  false );
					codeSegment.emitPop ( );
				}

				debug.addEntry ( block->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );

				codeSegment.emitLabel ( block->loopData().continueLabel );
				listing.label ( sym, block->loopData().continueLabel );
				fixup->setRefPoint ( block->loopData().continueLabel, codeSegment.nextOp ( ) );

				listing.emitOps ( sym );
				listing.emitContSourceEnd ( block->location );

				if ( !block->loopData().block->alwaysReturns () )
				{
					// emit loop condition code
					if ( complexCondition )
					{
						fixup->needFixup ( complexConditionLabel, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
					} else
					{
						if ( block->loopData().condition )
						{
							if ( block->loopData().condition->getOp () == astOp::logicalNot )
							{
								block->loopData().condition->fixupReturn ( sym, funcDef );
								// emit the condition
								condType = compEmitNode ( funcDef, block->loopData().condition->left, sym, symVariantType, retType, safeCalls,  true );
								if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
								switch ( condType.compType () )
								{
									case symbolType::symInt:
									case symbolType::symBool:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
										break;
									default:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpncpopv, (uint32_t) 0 ) );
										break;
								}
							} else
							{
								block->loopData().condition->fixupReturn ( sym, funcDef );
								// emit the condition
								condType = compEmitNode ( funcDef, block->loopData().condition, sym, symVariantType, retType, safeCalls,  true );
								if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
								switch ( condType.compType () )
								{
									case symbolType::symInt:
									case symbolType::symBool:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpcpop, (uint32_t) 0 ) );
										break;
									default:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpcpopv, (uint32_t) 0 ) );
										break;
								}
							}
							sym->popStack ();
						} else
						{
							fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
						}
					}
				}

				codeSegment.emitLabel ( block->loopData().breakLabel );
				listing.label ( sym, block->loopData().breakLabel );
				fixup->setRefPoint ( block->loopData().breakLabel, codeSegment.nextOp ( ) );
			}
			break;
		case astOp::btRepeat:
			{
				listing.emitContSource ( block->location );

				auto tmpLabel1 = makeLabel ( "repeat_loop%" );
				codeSegment.emitLabel ( tmpLabel1 );
				fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp ( ) );
				listing.label ( sym, tmpLabel1 );

				codeSegment.emitPush ( );
				compEmitNode ( funcDef, block->loopData().block, sym, symVariantType, retType, safeCalls,  false );
				codeSegment.emitPop ( );

				codeSegment.emitLabel ( block->loopData().continueLabel );
				listing.label ( sym, block->loopData().continueLabel );
				fixup->setRefPoint ( block->loopData().continueLabel, codeSegment.nextOp ( ) );

				listing.emitOps ( sym );
				listing.emitContSourceEnd ( block->location );

				if ( !block->loopData().block->alwaysReturns () && block->loopData().condition )
				{
					debug.addEntry ( block->loopData().condition->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
					if ( block->loopData().condition->isTrue ( ) )
					{
						// always loop
						if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
						fixup->needFixup ( block->loopData().continueLabel, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
					} else
					{
						if ( !block->loopData().condition->isFalse ( ) )
						{
							if ( block->loopData().condition->getOp ( ) == astOp::logicalNot )
							{
								condType = compEmitNode ( funcDef, block->loopData().condition->left, sym, symVariantType, retType, safeCalls,  true );
								if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
								switch ( condType.compType ( ) )
								{
									case symbolType::symInt:
									case symbolType::symBool:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpcpop, (uint32_t) 0 ) );
										break;
									default:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpcpopv, (uint32_t) 0 ) );
										break;
								}
								sym->popStack ( );
							} else
							{
								condType = compEmitNode ( funcDef, block->loopData().condition, sym, symVariantType, retType, safeCalls,  true );
								if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
								switch ( condType.compType ( ) )
								{
									case symbolType::symInt:
									case symbolType::symBool:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpncpop, (uint32_t) 0 ) );
										break;
									default:
										fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpncpopv, (uint32_t) 0 ) );
										break;
								}
								sym->popStack ( );
							}
						}
					}
				} else
				{
					// no condition... loop ever
					fixup->needFixup ( block->loopData().continueLabel, codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
				}
				codeSegment.emitLabel ( block->loopData().breakLabel );
				listing.label ( sym, block->loopData().breakLabel );
				fixup->setRefPoint ( block->loopData().breakLabel, codeSegment.nextOp ( ) );
			}
			break;
		case astOp::btSwitch:
			{
				char				caseLabel[256];
				symbolTypeClass		switchType;
				symbolTypeClass		caseType;

				listing.emitContSource ( block->location );

				bool hasDefault;

				auto unique = fixup->getUnique ( );

				switchType = block->switchData().condition->getType ( sym );

				debug.addEntry ( block->switchData().condition->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
				condType = compEmitNode ( funcDef, block->switchData().condition, sym, symVariantType, symVariantType, safeCalls, true );

				hasDefault = false;
				for ( uint32_t loop = 0; loop < block->switchData().caseList.size ( ); loop++ )
				{
					if ( block->switchData().caseList[loop]->condition )
					{
						sprintf_s ( caseLabel, sizeof ( caseLabel ), "%u:case_%u", unique, loop );

						caseType = compEmitNode ( funcDef, block->switchData().caseList[loop]->condition, sym, condType, retType, safeCalls,  true );

						if ( switchType.compType ( ) == caseType.compType ( ) )
						{
							switch ( switchType.compType ( ) )
							{
								case symbolType::symInt:
									codeSegment.putOp ( fglOpcodes::eqi1 );
									break;
								case symbolType::symDouble:
									codeSegment.putOp ( fglOpcodes::eqd1 );
									break;
								case symbolType::symString:
									codeSegment.putOp ( fglOpcodes::eqs1 );
									break;
								default:
									codeSegment.putOp ( fglOpcodes::eqv1 );
									break;
							}
						} else
						{
							codeSegment.putOp ( fglOpcodes::eqv1 );
						}
						// TODO: this is a bogus hack... it specializes jmp threading for this one particular case... this should be generalized in the IL to allow
						// DC elimination to remove the unneeded blocks
						if ( block->switchData().caseList[loop]->block )
						{
							switch ( block->switchData().caseList[loop]->block->getOp () )
							{
								case astOp::btGoto:
									fixup->needFixup ( block->switchData().caseList[loop]->block->gotoData().label, codeSegment.putOp ( fglOpcodes::jmpcpop2, (uint32_t) 0 ) );
									break;
								case astOp::btBreak:
									fixup->needFixup ( block->switchData().breakLabel, codeSegment.putOp ( fglOpcodes::jmpcpop2, (uint32_t) 0 ) );
									break;
								default:
									fixup->needFixup ( file->sCache.get ( caseLabel ), codeSegment.putOp ( fglOpcodes::jmpcpop2, (uint32_t) 0 ) );
									break;
							}
						} else
						{
							fixup->needFixup ( file->sCache.get ( caseLabel ), codeSegment.putOp ( fglOpcodes::jmpcpop2, (uint32_t) 0 ) );
						}
						sym->popStack ( );
					} else
					{
						// default block... no condition
						hasDefault = true;
					}
				}

				if ( hasDefault )
				{
					sprintf_s ( caseLabel, sizeof ( caseLabel ), "%u:case_default", unique );
					codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
					fixup->needFixup ( file->sCache.get ( caseLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
				} else
				{
					sprintf_s ( caseLabel, sizeof ( caseLabel ), "%u:case_end", unique );
					codeSegment.putOp ( fglOpcodes::pop, (uint32_t) 1 );
					fixup->needFixup ( file->sCache.get ( caseLabel ), codeSegment.putOp ( fglOpcodes::jmp, (uint32_t) 0 ) );
				}
				sym->popStack ( );

				for ( uint32_t loop = 0; loop < block->switchData().caseList.size ( ); loop++ )
				{
					if ( !block->switchData().caseList[loop]->condition )
					{
						sprintf_s ( caseLabel, sizeof ( caseLabel ), "%u:case_default", unique );
						codeSegment.emitLabel ( caseLabel );
						listing.label ( sym, caseLabel );
						fixup->setRefPoint ( file->sCache.get ( caseLabel ), codeSegment.nextOp ( ) );
					} else
					{
						sprintf_s ( caseLabel, sizeof ( caseLabel ), "%u:case_%u", unique, loop );
						codeSegment.emitLabel ( caseLabel );
						listing.label ( sym, caseLabel );
						fixup->setRefPoint ( file->sCache.get ( caseLabel ), codeSegment.nextOp ( ) );
					}
					if ( block->switchData().caseList[loop]->block )
					{
						switch ( block->switchData().caseList[loop]->block->getOp () )
						{
							case astOp::btGoto:
							case astOp::btBreak:
								break;
							default:
								listing.emitOps ( sym );
								listing.emitContSource ( block->switchData().caseList[loop]->block->location );
								debug.addEntry ( block->switchData().caseList[loop]->block->location, codeSegment.getNumOps ( ), sym->getStackSize ( ) );
								codeSegment.emitPush ( );
								compEmitNode ( funcDef, block->switchData().caseList[loop]->block, sym, symVariantType, retType, safeCalls,  false );
								codeSegment.emitPop ( );
								break;
						}
					}
				}

				sprintf_s ( caseLabel, sizeof ( caseLabel ), "%u:case_end", unique );
				codeSegment.emitLabel ( caseLabel );
				codeSegment.emitLabel ( block->switchData().breakLabel );
				listing.label ( sym, caseLabel );
				listing.label ( sym, block->switchData().breakLabel );
				fixup->setRefPoint ( file->sCache.get ( caseLabel ), codeSegment.nextOp ( ) );
				fixup->setRefPoint ( block->switchData().breakLabel, codeSegment.nextOp ( ) );
			}
			break;
		case astOp::btExit:
			debug.addEntry ( block->location, codeSegment.getNumOps(), sym->getStackSize () );
			listing.emitContSource ( block->location );
			if ( genDebug ) codeSegment.putOp ( fglOpcodes::debugCheckHalt );
			codeSegment.putOp ( fglOpcodes::ret );
			listing.emitOps ( sym );
			break;
		case astOp::btAnnotation:
			listing.emitContSource ( block->location );
			listing.annotate ( sym, block->stringValue() );
			break;
		case astOp::nilValue:
			resType = needType;
			break;
		case astOp::errorValue:
		case astOp::warnValue:
			throw errorNum::scINTERNAL;
			break;
		default:
			__assume (false);
			throw errorNum::scINTERNAL;
			break;
	}
	if ( needValue )
	{
		resType = compEmitCast ( resType, needType );
	}
	listing.emitOps ( sym );

	return resType;
}

