/*

astNode

*/
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"

bool isInRegion ( opFile const *file, astNode const *node )
{
	return file->isInRegion ( node->location );
}

static void getElemInfo ( opFile *file, compClassElementSearch *elem, astNode *node, accessorType const &acc, bool isAccess, astLSInfo &info, bool refOnly )
{
	switch ( elem->type )
	{
		case fgxClassElementType::fgxClassType_method:
			{
				auto func = elem->methodAccess.func;
				if ( !refOnly && file->isInRegion ( *node ) )
				{
					if ( func->isIterator )
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::methodIterator, node->right->location, func, isAccess } );
					} else
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::method, node->right->location, func, isAccess } );
					}
				}
				info.symbolReferences.insert ( astLSInfo::symbolReference { node->right->location, elem->elem->name, node, elem->elem } );
			}
			break;
		case fgxClassElementType::fgxClassType_iVar:
		case fgxClassElementType::fgxClassType_static:
		case fgxClassElementType::fgxClassType_const:
			if ( !refOnly && file->isInRegion ( *node ) )
			{
				info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::ivar, node->right->location, elem->elem, isAccess, elem->elem->getType( elem->name ) } );
			}
			info.symbolReferences.insert ( astLSInfo::symbolReference { node->right->location, elem->elem->name, node, elem->elem } );
			break;
		case fgxClassElementType::fgxClassType_inherit:
			if ( !refOnly && file->isInRegion( *node ) ) info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::ns, node->right->location, elem->elem, isAccess } );
			info.symbolReferences.insert( astLSInfo::symbolReference{ node->right->location, elem->elem->name, node, elem->elem } );
			break;
		case fgxClassElementType::fgxClassType_prop:
			if ( elem->methodAccess.func && isAccess )
			{
				if ( !refOnly && file->isInRegion ( *node ) ) info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::property, node->right->location, elem->methodAccess.func, isAccess } );
				info.symbolReferences.insert ( astLSInfo::symbolReference { node->right->location, elem->elem->name, node, elem->elem } );
			} else if ( elem->assign.func && !isAccess )
			{
				if ( !refOnly && file->isInRegion ( *node ) ) info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::property, node->right->location, elem->assign.func, isAccess } );
				info.symbolReferences.insert ( astLSInfo::symbolReference { node->right->location, elem->elem->name, node, elem->elem } );
			}
			break;
		default:
			break;
	}
}

static astNode *getInfoCB( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool isInFunctionCall, accessorType const &acc, astLSInfo &info, bool refOnly )
{
	try
	{
		switch ( node->getOp() )
		{
			case astOp::errorValue:
				info.errors.push_back( node );
				break;
			case astOp::warnValue:
				info.warnings.push_back( node );
				break;
			case astOp::funcCall:
				if ( !refOnly )
				{
					if ( node->left->getOp() == astOp::atomValue )
					{
						auto func = sym->file->functionList.find( sym->getFuncName( node->left->symbolValue(), true ) )->second;

						if ( node->location.sourceIndex )
						{
							if ( isInRegion ( sym->file, node ) )
							{
								info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::keywordOperator, node->location, node, isAccess, func->retType, node, false } );
							}
						}

						if ( node->pList().paramRegion.size() )
						{
							srcLocation	callLoc = node->pList().paramRegion.front();
							callLoc.setEnd( node->pList().paramRegion.back() );
							info.signatureHelp.insert( astLSInfo::signatureHelpDef{ callLoc, func, node } );
						}
						if ( node->left->symbolValue() == sym->file->newValue )
						{
							if ( !node->pList().param.empty() )
							{
								if ( node->pList().param[0]->getOp() == astOp::nameValue )
								{
									auto cls = sym->findClass( node->pList().param[0]->nameValue() );
									if ( cls )
									{
										auto elem = cls->getElement ( sym->file->newValue );
										if ( elem )
										{
											info.symbolReferences.insert ( astLSInfo::symbolReference{ elem->elem->location, node->pList ().param[0]->nameValue (), node->pList ().param[0], cls->oClass } );
											if ( elem && isInRegion ( sym->file, node->pList ().param[0] ) )
											{
												info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::method, node->pList ().param[0]->location, elem->elem, isAccess } );
											}
										}
									}
								}
							}
						}
					} else if ( node->left->getOp() == astOp::sendMsg && node->left->right->getOp() == astOp::nameValue )
					{
						auto type = node->left->left->getType( sym );

						if ( type.isAnObject() && type.hasClass() )
						{
							auto cls = sym->findClass( type.getClass() );
							if ( cls )
							{
								auto elem = cls->getElement( node->left->right->nameValue() );
								if ( elem && elem->type == fgxClassElementType::fgxClassType_method )
								{
									if ( node->location.sourceIndex )
									{
										if ( isInRegion ( sym->file, node ) )
										{
											info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::method, node->left->right->location, elem->elem, isAccess, elem->methodAccess.func->retType, node, false} );
										}
									}
									if ( !node->pList().paramRegion.empty() )
									{
										srcLocation	callLoc = node->pList().paramRegion.front();
										callLoc.setEnd( node->pList().paramRegion.back() );
										info.signatureHelp.insert( astLSInfo::signatureHelpDef{ callLoc, sym->file->functionList.find( elem->elem->data.method.func )->second, node } );
									}
								}
							}
						}
					}
				}
				break;
			case astOp::atomValue:
				{
					auto func = sym->file->functionList.find( sym->getFuncName( node->symbolValue(), true ) )->second;
					if ( !refOnly && isInRegion( sym->file, node ) )
					{
						if ( !func->classDef )
						{
							if ( func->isIterator )
							{
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::iterator, node->location, func, isAccess, func->retType, nullptr, isInFunctionCall } );
							} else
							{
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::function, node->location, func, isAccess, func->retType, nullptr, isInFunctionCall } );
							}
						} else
						{
							if ( func->isIterator )
							{
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::methodIterator, node->location, func, isAccess, func->retType, nullptr, isInFunctionCall } );
							} else
							{
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::method, node->location, func, isAccess, func->retType, nullptr, isInFunctionCall } );
							}
						}
					}
					info.symbolReferences.insert( astLSInfo::symbolReference{ node->location, node->symbolValue(), node, sym->getSymbolSource( node->symbolValue() ) } );
				}
				break;
			case astOp::sendMsg:
				if ( node->location.sourceIndex )
				{
					if ( !refOnly && isInRegion ( sym->file, node ) )
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::keywordOperator, node->location, node, isAccess } );
					}
				}
				if ( node->left && !refOnly )
				{
					auto type = node->left->getType( sym );
					if ( type.isAnObject() && type.hasClass() )
					{
						srcLocation src = node->location;
						node->adjustLocation( src );
						info.objCompletions.insert( astLSInfo::objectCompletions{ src, type } );
					}
				}

				if ( parent->getOp() == astOp::funcCall && node->left && node->right && node->right->getOp() == astOp::nameValue )
				{
					auto type = node->left->getType( sym );
					if ( type.isAnObject() )
					{
						compClass *cls = nullptr;
						if ( type.hasClass() && (cls = sym->findClass( type.getClass() )) )
						{
							auto elem = cls->getElement( node->right->nameValue() );
							if ( !elem )
							{
								if ( cls->defaultMethodIndex )
								{
									elem = &cls->elements[static_cast<size_t>(cls->defaultMethodIndex) - 1];
								}
							}
							if ( elem )
							{
								getElemInfo( sym->file, elem, node, acc, isAccess, info, refOnly );
							}
						} else
						{
							if ( !refOnly && isInRegion ( sym->file, node->right ) )
							{
								info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::method, node->right->location, node->right, isAccess} );
							}
						}
					} else if ( !refOnly && isInRegion( sym->file, node->right ) )
					{
						info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::name, node->right->location, node->right, isAccess } );
					}
				} else if ( parent->getOp() == astOp::assign && node->left && node->right && node->right->getOp() == astOp::nameValue )
				{
					auto type = node->left->getType( sym );
					if ( type.isAnObject() )
					{
						compClass *cls = nullptr;
						if ( type.hasClass() && (cls = sym->findClass( type.getClass() )) )
						{
							auto elem = cls->getElement( node->right->nameValue() );
							if ( !elem )
							{
								if ( cls->defaultAssignIndex )
								{
									elem = &cls->elements[static_cast<size_t>(cls->defaultAssignIndex) - 1];
								}
							}
							if ( elem )
							{
								getElemInfo( sym->file, elem, node, acc, isAccess, info, refOnly );
							}
						} else
						{
							if ( !refOnly && isInRegion ( sym->file, node->right ) )
							{
								info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::method, node->right->location, node->right, isAccess} );
							}
						}
					} else if ( !refOnly && isInRegion( sym->file, node->right ) )
					{
						info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::name, node->right->location, node->right, isAccess } );
					}
				} else if ( node->left && node->right && node->right->getOp() == astOp::nameValue )
				{
					auto type = node->left->getType( sym );
					if ( type.isAnObject() )
					{
						compClass *cls = nullptr;
						if ( type.hasClass() && (cls = sym->findClass( type.getClass() )) )
						{
							auto elem = cls->getElement( node->right->nameValue() );
							if ( !elem )
							{
								if ( cls->defaultAccessIndex )
								{
									elem = &cls->elements[static_cast<size_t>(cls->defaultAccessIndex) - 1];
								}
							}
							if ( elem )
							{
								getElemInfo( sym->file, elem, node, acc, isAccess, info, refOnly );
							}
						} else
						{
							if ( !refOnly && isInRegion ( sym->file, node->right ) )
							{
								info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::method, node->right->location, node->right, isAccess} );
							}
						}
					} else if ( !refOnly && isInRegion( sym->file, node->right ) )
					{
						info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::name, node->right->location, node->right, isAccess } );
					}
				} else if ( node->left && node->right && node->right->getOp() == astOp::symbolValue )
				{
					auto type = node->left->getType( sym );
					if ( type.isAnObject() )
					{
						compClass *cls = nullptr;
						if ( type.hasClass() && (cls = sym->findClass( type.getClass() )) )
						{
							auto elem = cls->getElement( node->right->symbolValue() );
							if ( !elem )
							{
								if ( cls->defaultAccessIndex )
								{
									elem = &cls->elements[static_cast<size_t>(cls->defaultAccessIndex) - 1];
								}
							}
							if ( elem )
							{
								getElemInfo( sym->file, elem, node, acc, isAccess, info, refOnly );
							}
						} else
						{
							if ( !refOnly && isInRegion ( sym->file, node->right ) )
							{
								info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::method, node->right->location, node->right, isAccess} );
							}
						}
					} else if ( !refOnly && isInRegion( sym->file, node->right ) )
					{
						info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::name, node->right->location, node->right, isAccess } );
					}
				}
				break;
			case astOp::symbolDef:
				switch ( node->symbolDef()->symType )
				{
					case symbolSpaceType::symTypeStatic:
					case symbolSpaceType::symTypeLocal:
						info.symbolReferences.insert( astLSInfo::symbolReference{ node->location, node->symbolDef()->getSymbolName(), node, node->symbolDef()->getSymbolSource( node->symbolDef()->getSymbolName() ) } );
						info.symbolDefinitions.insert( astLSInfo::symbolDefinition{ node->location, node->symbolDef()->getSymbolName(), std::get<opFunction *>( acc ), node->symbolDef()->getType( node->symbolDef()->getSymbolName(), isAccess ) } );
						if ( !refOnly && isInRegion( sym->file, node ) )
						{
							assert( node->location.sourceIndex );
							auto init = node->symbolDef()->getInitializer( node->symbolDef()->getSymbolName() );
							if ( init->getOp() == astOp::assign )
							{
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::variable, init->left->location, node->symbolDef(), isAccess, node->symbolDef()->getType ( node->symbolDef()->getSymbolName(), isAccess ), nullptr, isInFunctionCall } );
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::keywordOperator, init->location, init, isAccess } );
							} else
							{
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::variable, init->location, node->symbolDef(), isAccess, node->symbolDef ()->getType ( node->symbolDef ()->getSymbolName (), isAccess ), nullptr, isInFunctionCall } );
							}
						}
						break;
					case symbolSpaceType::symTypeParam:
						assert( false ); // should NOT occur... this happens during inlining and we don't do inlining in LS mode
						break;
					default:
						break;
				}
				break;
			case astOp::symbolValue:
				if ( sym->isDefined( node->symbolValue(), isAccess ) )
				{
					info.symbolReferences.insert( astLSInfo::symbolReference{ node->location, node->symbolValue(), node, sym->getSymbolSource( node->symbolValue() ) } );
					if ( !refOnly && isInRegion( sym->file, node ) )
					{
						switch ( sym->getScope( node->symbolValue(), true ) )
						{
							case symbolSpaceScope::symClassConst:
							case symbolSpaceScope::symClassStatic:
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::ivar, node->location, sym->getSymbolSource( node->symbolValue() ), isAccess, node->getType ( sym ), nullptr, isInFunctionCall } );
								break;
							case symbolSpaceScope::symClassAssign:
							case symbolSpaceScope::symClassAccess:
								{
									auto func = sym->file->functionList.find( sym->getFuncName( node->symbolValue(), isAccess ) )->second;
									info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::property, node->location, func, isAccess, func->retType, nullptr, isInFunctionCall } );
								}
								break;
							case symbolSpaceScope::symClassMethod:
								{
									auto func = sym->file->functionList.find( sym->getFuncName( node->symbolValue(), isAccess ) )->second;
									if ( func->isIterator )
									{
										info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::methodIterator, node->location, func, isAccess, func->retType, nullptr, isInFunctionCall } );
									} else
									{
										info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::method, node->location, func, isAccess, func->retType, nullptr, isInFunctionCall } );
									}
								}
								break;
							case symbolSpaceScope::symClassInherit:
								{
									info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::ns, node->location, sym->getSymbolSource( node->symbolValue() ), isAccess } );
								}
								break;
							case symbolSpaceScope::symClassIVar:
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::ivar, node->location, sym->getSymbolSource( node->symbolValue() ), isAccess, node->getType ( sym ), nullptr, isInFunctionCall } );
								break;
							case symbolSpaceScope::symLocalField:
								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::field, node->location, node, isAccess, node->getType ( sym ), nullptr, isInFunctionCall } );
								break;
							case symbolSpaceScope::symLocalParam:
								if ( node->symbolValue() != sym->file->outputStringValue )
								{
									info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::parameter, node->location, sym->getSymbolSource( node->symbolValue() ), isAccess, node->getType ( sym ), nullptr, isInFunctionCall } );
								}
								break;
							case symbolSpaceScope::symClass:
								info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::type, node->location, sym->getSymbolSource ( node->symbolValue () ),  isAccess, node->getType ( sym ), nullptr, isInFunctionCall} );
								break;
							case symbolSpaceScope::symLocalConst:
							case symbolSpaceScope::symLocal:
							case symbolSpaceScope::symLocalStatic:
								info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::variable, node->location, sym->getSymbolSource ( node->symbolValue () ),  isAccess, node->getType ( sym ), nullptr, isInFunctionCall} );
								break;
							case symbolSpaceScope::symGlobal:
//								info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::variable, node->location, sym->getSymbolSource( node->symbolValue() ),  isAccess, node->getType ( sym ), nullptr, isInFunctionCall } );
								break;
							case symbolSpaceScope::symFunction:
								break;
							case symbolSpaceScope::symPCall:
							case symbolSpaceScope::symScopeUnknown:
								break;
						}
					}
				}
				break;
			case astOp::btStatement:
				if ( !refOnly && isInRegion( sym->file, node ) )
				{
					info.semanticTokens.insert( astLSInfo::symbolRange{ acc, node->getSemanticType(), node->location, node, isAccess } );
				}
				break;
			case astOp::btSwitch:
				if ( !refOnly && isInRegion( sym->file, node ) )
				{
					srcLocation base = node->location;
					node->adjustLocation( base );
					info.foldingRanges.insert( base );

					for ( auto &it : node->switchData().caseList )
					{
						if ( it->block )
						{
							srcLocation base = it->block->location;
							it->block->adjustLocation( base );
							info.foldingRanges.insert( base );
						}
					}
				}
				break;
			case astOp::btIf:
				if ( !refOnly && isInRegion( sym->file, node ) )
				{
					srcLocation base = node->location;
					node->adjustLocation( base );
					info.foldingRanges.insert( base );

					for ( auto &it : node->ifData().ifList )
					{
						if ( it->block )
						{
							srcLocation base = it->block->location;
							it->block->adjustLocation( base );
							info.foldingRanges.insert( base );
						}
					}

					if ( node->ifData().elseBlock )
					{
						srcLocation base = node->ifData().elseBlock->location;
						node->ifData().elseBlock->adjustLocation( base );
						info.foldingRanges.insert( base );
					}
				}
				break;
			case astOp::btWhile:
			case astOp::btFor:
			case astOp::btForEach:
			case astOp::btBasic:
			case astOp::btRepeat:
				if ( !refOnly && isInRegion( sym->file, node ) )
				{
					info.foldingRanges.insert( node->location );
				}
				break;
			case astOp::btTry:
				if ( !refOnly && isInRegion( sym->file, node ) )
				{
					srcLocation base = node->location;
					node->adjustLocation( base );
					info.foldingRanges.insert( base );

					if ( node->tryCatchData().tryBlock )
					{
						base = node->tryCatchData().tryBlock->location;
						node->tryCatchData().tryBlock->adjustLocation( base );
						info.foldingRanges.insert( base );
					}

					if ( node->tryCatchData().catchBlock )
					{
						base = node->tryCatchData().catchBlock->location;
						node->tryCatchData().catchBlock->adjustLocation( base );
						info.foldingRanges.insert( base );
					}

					if ( node->tryCatchData().finallyBlock )
					{
						base = node->tryCatchData().finallyBlock->location;
						node->tryCatchData().finallyBlock->adjustLocation( base );
						info.foldingRanges.insert( base );
					}
				}
				break;
			case astOp::doubleValue:
			case astOp::intValue:
				if ( !refOnly && isInRegion ( sym->file, node ) )
				{
					info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::numberLiteral, node->location, node, isAccess } );
				}
				break;
			case astOp::nullValue:
			case astOp::nilValue:
			case astOp::codeBlockValue:
			case astOp::compValue:
				if ( !refOnly && isInRegion( sym->file, node ) ) info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::customLiteral, node->location, node, isAccess } );
				break;
			case astOp::stringValue:
				if ( parent && parent->getOp() == astOp::pairValue && parent->left == node )
				{
					if ( !refOnly && isInRegion ( sym->file, node ) )
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::name, node->location, node, isAccess} );
					}
				} else
				{
					if ( !refOnly && isInRegion ( sym->file, node ) )
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::stringLiteral, node->location, node, isAccess} );
					}
				}
				break;
			case astOp::nameValue:
				break;
			case astOp::openPeren:
			case astOp::closePeren:
			case astOp::add:
			case astOp::subtract:
			case astOp::multiply:
			case astOp::divide:
			case astOp::modulus:
			case astOp::power:
			case astOp::max:
			case astOp::min:
			case astOp::negate:
			case astOp::equal:
			case astOp::equali:
			case astOp::notEqual:
			case astOp::less:
			case astOp::lessEqual:
			case astOp::greater:
			case astOp::greaterEqual:
			case astOp::logicalAnd:
			case astOp::logicalOr:
			case astOp::logicalNot:
			case astOp::bitAnd:
			case astOp::bitOr:
			case astOp::bitXor:
			case astOp::shiftLeft:
			case astOp::shiftRight:
			case astOp::arrayDeref:
			case astOp::assign:
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
			case astOp::postInc:
			case astOp::postDec:
			case astOp::preInc:
			case astOp::preDec:
			case astOp::twosComplement:
			case astOp::refCreate:
			case astOp::refPromote:
			case astOp::nsDeref:
			case astOp::getEnumerator:
			case astOp::seq:
			case astOp::workAreaStart:
			case astOp::comp:
			case astOp::conditional:
			case astOp::econditional:
			case astOp::inc:
			case astOp::dec:
			case astOp::lambdaExpr:
			case astOp::range:
			case astOp::selfSend:
			case astOp::dummy:
			case astOp::coalesce:
			case astOp::cSend:
			case astOp::cArrayDeref:
			default:
				if ( node->location.sourceIndex )
				{
					if ( !refOnly && isInRegion( sym->file, node ) )
					{
						info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::keywordOperator, node->location, node, isAccess } );
					}
				}
				break;
			case astOp::addAssign:
				if ( node->left->getOp() == astOp::symbolValue && node->left->symbolValue() == sym->file->outputStringValue )
				{
					srcLocation tmpLocation = node->left->location;
					tmpLocation.columnNumberEnd = tmpLocation.columnNumber;
					tmpLocation.lineNumberEnd = tmpLocation.lineNumberStart;
					if ( !refOnly && isInRegion( sym->file, node ) )info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::keywordOperator, tmpLocation, node, isAccess } );
				} else
				{
					if ( node->location.sourceIndex )
					{
						if ( !refOnly && isInRegion( sym->file, node ) )info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::keywordOperator, node->location, node, isAccess } );
					}
				}
				break;
			case astOp::btLabel:
				if ( !refOnly && isInRegion( sym->file, node ) )
				{
					if ( node->location.sourceIndex )
					{
						info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::label, node->location, node, isAccess } );
					}
				}
				break;
			case astOp::btGoto:
				if ( !refOnly && isInRegion( sym->file, node ) )
				{
					if ( node->location.sourceIndex )
					{
						info.semanticTokens.insert( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::keywordOperator, node->location, node, isAccess } );
					}
				}
				break;
			case astOp::pairValue:
				if ( !refOnly && isInRegion ( sym->file, node ) )
				{
					if ( node->location.sourceIndex )
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{acc, astLSInfo::semanticSymbolType::keywordOperator, node->location, node, isAccess} );
					}
				}
				break;
			case astOp::funcValue:
			case astOp::btInlineRet:
			case astOp::btReturn:
			case astOp::btInline:
			case astOp::btCont:
			case astOp::btBreak:
			case astOp::singleLineComment:
			case astOp::commentBlock:
			case astOp::codeBlockExpression:
			case astOp::btAnnotation:
			case astOp::btExcept:
			case astOp::metadata:
			case astOp::btExit:
			case astOp::arrayValue:
			case astOp::varrayValue:
			case astOp::fieldValue:
			case astOp::senderValue:
			case astOp::paramPack:
			case astOp::paramExpand:
			case astOp::symbolPack:
			case astOp::intCast:
			case astOp::doubleCast:
			case astOp::stringCast:
			case astOp::endParse:
				break;
		}
	} catch ( ... )
	{
	}
	return node;
}

static void getInfo( symbolStack *sym, accessorType const &acc, astNode *node, astLSInfo &info, bool refOnly )
{
	astNodeWalk ( node, sym, getInfoCB, acc, info, refOnly );
}

void compExecutable::getInfo ( astLSInfo &info, uint32_t sourceIndex )
{
	info.clear ();

	for ( auto &it : file->statements )
	{
		if ( it.get()->location.sourceIndex == sourceIndex && file->isInRegion ( *it.get() ) )
		{
			info.semanticTokens.insert ( astLSInfo::symbolRange{ accessorType (), it.get ()->getSemanticType (), it.get ()->location, it.get (), true } );
		}
	}

	for ( auto &it : file->symbols )
	{
		if ( it.second.name != file->outputStringValue )
		{
			if ( it.second.symClass == opSymbol::symbolClass::symbolClass_global || it.second.symClass == opSymbol::symbolClass::symbolClass_globalConst )
			{
				if ( it.second.location.sourceIndex == sourceIndex && file->isInRegion ( it.second.location ) )
				{
					assert ( it.second.location.sourceIndex );

					if ( it.second.initializer->getOp () == astOp::assign )
					{
						symbolStack sym ( file );

						info.semanticTokens.insert ( astLSInfo::symbolRange{ accessorType (), astLSInfo::semanticSymbolType::variable, it.second.initializer->left->location, &it.second, true, it.second.getType() } );
						info.semanticTokens.insert ( astLSInfo::symbolRange{ accessorType (), astLSInfo::semanticSymbolType::keywordOperator, it.second.initializer->location, it.second.initializer, true } );
						::getInfo ( &sym, accessorType (), it.second.initializer->right, info, false );
					} else
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{ accessorType (), astLSInfo::semanticSymbolType::variable, it.second.location, &it.second, true, it.second.getType () } );
					}
				}
				info.symbolDefinitions.insert ( astLSInfo::symbolDefinition{ it.second.location, it.second.name, nullptr, it.second.getType() } );
			}
			info.symbolReferences.insert ( astLSInfo::symbolReference{ it.second.location, it.first, nullptr, &it.second } );
		}
	}

	for ( auto &it : file->classList )
	{
		auto *cls = it.second;
		if ( cls->location.sourceIndex == sourceIndex )
		{
			info.symbolReferences.insert ( astLSInfo::symbolReference { cls->nameLocation, cls->name, nullptr, cls } );
			if ( file->isInRegion ( cls->location ) ) info.semanticTokens.insert ( astLSInfo::symbolRange{ accessorType(), astLSInfo::semanticSymbolType::type, cls->nameLocation, cls, true } );

			info.foldingRanges.insert ( cls->location );
			for ( auto &elem : cls->elems )
			{
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_method:
						{
							auto func = file->functionList.find ( elem->data.method.func )->second;
							if ( func->nameLocation.sourceIndex == sourceIndex && !func->lsIgnore )
							{
								// need to do an additional check here for system consturcted new methods
								info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::method, func->nameLocation, func, true, func->retType } );
								info.symbolReferences.insert ( astLSInfo::symbolReference{ func->nameLocation, file->sCache.get ( func->getSimpleName () ), nullptr, elem } );
							}
						}
						break;
					case fgxClassElementType::fgxClassType_prop:
						if ( elem->data.prop.access )
						{
							auto func = file->functionList.find ( elem->data.prop.access )->second;

							file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::property, func->nameLocation ) );

							info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::property, func->nameLocation, func, true, func->retType } );
							info.symbolReferences.insert ( astLSInfo::symbolReference { func->nameLocation, file->sCache.get ( func->getSimpleName () ), nullptr, elem } );
						}
						if ( elem->data.prop.assign )
						{
							auto func = file->functionList.find ( elem->data.prop.assign )->second;

							file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::property, func->nameLocation ) );

							info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::property, func->nameLocation, func, false } );
							info.symbolReferences.insert ( astLSInfo::symbolReference { func->nameLocation, file->sCache.get ( func->getSimpleName () ), nullptr, elem } );
						}
						break;
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
						if ( file->isInRegion ( elem->location ) )
						{
							auto init = elem->data.iVar.initializer;
							if ( init->getOp () == astOp::assign )
							{
								accessorType acc;
								symbolStack sym ( file );

								info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::ivar, init->left->location, elem, true, elem->getType( true ) } );
								info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::keywordOperator, init->location, init, true } );
								::getInfo ( &sym, acc, init->right, info, false );
							} else
							{
								info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::ivar, init->location, elem, true, elem->getType ( true ) } );
							}
						}
						info.symbolReferences.insert ( astLSInfo::symbolReference { elem->location, elem->name, nullptr, elem } );
						break;
					case fgxClassElementType::fgxClassType_inherit:
						info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::customLiteral, elem->location, elem } );
						break;
					case fgxClassElementType::fgxClassType_message:
						info.semanticTokens.insert ( astLSInfo::symbolRange{ elem, astLSInfo::semanticSymbolType::customLiteral, elem->location, elem, true } );
						info.symbolReferences.insert ( astLSInfo::symbolReference { elem->location, elem->name, nullptr, elem } );
						break;
				}
			}
		} else
		{
			// even though we're NOT in the current file, we still want to show all the references to this entry
			for ( auto &elem : cls->elems )
			{
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_method:
						{
							auto func = file->functionList.find ( elem->data.method.func )->second;
							info.symbolReferences.insert ( astLSInfo::symbolReference { func->nameLocation, file->sCache.get ( func->getSimpleName () ), nullptr, elem } );
						}
						break;
					case fgxClassElementType::fgxClassType_prop:
						if ( elem->data.prop.access )
						{
							auto func = file->functionList.find ( elem->data.prop.access )->second;
							info.symbolReferences.insert ( astLSInfo::symbolReference { func->nameLocation, file->sCache.get ( func->getSimpleName () ), nullptr, elem } );
						}
						if ( elem->data.prop.assign )
						{
							auto func = file->functionList.find ( elem->data.prop.assign )->second;
							info.symbolReferences.insert ( astLSInfo::symbolReference { func->nameLocation, file->sCache.get ( func->getSimpleName () ), nullptr, elem } );
						}
						break;
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
						info.symbolReferences.insert ( astLSInfo::symbolReference { elem->location, elem->name, nullptr, elem } );
						break;
					case fgxClassElementType::fgxClassType_inherit:
					case fgxClassElementType::fgxClassType_message:
						break;
				}
			}
		}
	}
	for ( auto &it : file->functionList )
	{
		auto *func = it.second;
		if ( !func->isBaseMethod && !func->lsIgnore )
		{
			accessorType acc( func );
			symbolStack sym( file, func );
			if ( func->location.sourceIndex == sourceIndex )
			{
				if ( func->params.size() )
				{
					opFunction paramFunc( file );
					paramFunc.usingList = func->usingList;
					symbolStack paramSym( file, &paramFunc );

					for ( int32_t it = (int32_t)func->params.size(); it; it-- )
					{
						auto init = func->params[(size_t)(it - 1)]->initializer;
						if ( init )
						{
							if ( init->getOp() == astOp::assign )
							{
								::getInfo( &paramSym, acc, init->right, info, false );
								info.semanticTokens.insert( astLSInfo::symbolRange{ func, astLSInfo::semanticSymbolType::keywordOperator, *init, init, true } );
								info.semanticTokens.insert( astLSInfo::symbolRange{ func, astLSInfo::semanticSymbolType::parameter, *init->left, func->params[(uint32_t)it - 1], true, func->params[(uint32_t) it - 1]->type } );
							} else
							{
								::getInfo( &paramSym, acc, init, info, false );
							}
						}
						info.symbolReferences.insert( astLSInfo::symbolReference{ func->params[(uint32_t)it - 1]->location, func->params[(uint32_t)it - 1]->name, nullptr, func->params[(uint32_t)it - 1] } );
					}
				}
				::getInfo( &sym, acc, func->codeBlock, info, false );
				info.foldingRanges.insert( func->location );
				if ( func->nameLocation.sourceIndex == sourceIndex && !func->classDef )
				{
					// semantic sugar generated lambda's will not match 
					info.semanticTokens.insert( astLSInfo::symbolRange{ func, astLSInfo::semanticSymbolType::function, func->nameLocation, func, true, func->retType } );
				}
			} else
			{
				if ( func->nameLocation.sourceIndex )
				{
					::getInfo( &sym, acc, func->codeBlock, info, true );
				}
			}
			if ( func->nameLocation.sourceIndex )
			{
				info.symbolReferences.insert( astLSInfo::symbolReference{ func->nameLocation, func->name, nullptr, func } );
			}
		}
		if ( !func->lsIgnore )
		{
			accessorType acc ( func );
			opFunction paramFunc ( file );
			paramFunc.usingList = func->usingList;
			symbolStack paramSym ( file, &paramFunc );

			auto symbol = &func->params;

			for ( uint32_t it = 0; it < symbol->size (); it++ )
			{
				auto s = (*symbol)[it];
				if ( s->initializer )
				{
					if ( s->initializer->getOp () == astOp::assign )
					{
						info.symbolReferences.insert ( astLSInfo::symbolReference { *s->initializer->left, s->name, s->initializer->right, s } );
						if ( s->location.sourceIndex == sourceIndex && file->isInRegion ( s->initializer->location ) )
						{
							info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::parameter, *s->initializer->left, s, true, s->getType () } );
						}
						::getInfo ( &paramSym, acc, s->initializer->right, info, !(func->location.sourceIndex == sourceIndex) );
					} else
					{
						info.symbolReferences.insert ( astLSInfo::symbolReference { *s->initializer, s->name, s->initializer, s } );
						if ( s->location.sourceIndex == sourceIndex && file->isInRegion ( s->initializer->location ) )
						{
							info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::parameter, *s->initializer, s, true, s->getType () } );
						}
					}
				} else
				{
					info.symbolReferences.insert ( astLSInfo::symbolReference { s->location, s->name, nullptr, s } );
					if ( s->location.sourceIndex == sourceIndex && file->isInRegion ( s->initializer->location ) )
					{
						info.semanticTokens.insert ( astLSInfo::symbolRange{ acc, astLSInfo::semanticSymbolType::parameter, s->location, s, true, s->getType () } );
					}
				}
			}
		}
	}
	for ( auto &it : file->warnings )
	{
		if ( it->location.sourceIndex == sourceIndex )
		{
			info.warnings.push_back ( it.get () );
		}
	}
	for ( auto &it : file->errors )
	{
		if ( it->location.sourceIndex == sourceIndex )
		{
			info.errors.push_back ( it.get () );
		}
	}
}