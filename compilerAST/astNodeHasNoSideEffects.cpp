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
		case astOp::funcValue:
			{
				auto lFunc = sym->file->functionList.find( node->symbolValue() )->second;
				if ( !lFunc->isConst )
				{
					return false;
				}
			}
			break;
		case astOp::sendMsg:
			if ( node->left->getType ( sym ).isAnObject () )
			{
				// method calls
				symbolTypeClass type = node->getType ( sym );

				if ( node->right->getOp () == astOp::symbolValue )
				{
					compClass *classDef;

					if ( node->getType ( sym ).hasClass () && (classDef = sym->findClass ( type.getClass () )) )
					{
						compClassElementSearch *elem = classDef->getElement ( node->right->symbolValue() );
						if ( elem )
						{
							opFunction *func;
							switch ( elem->type )
							{
								case fgxClassElementType::fgxClassType_prop:
									func = sym->file->functionList.find( elem->methodAccess.func->name )->second;
									if ( !func->isConst ) return false;
									break;
								case fgxClassElementType::fgxClassType_iVar:
								case fgxClassElementType::fgxClassType_const:
								case fgxClassElementType::fgxClassType_inherit:
									// these dont have side-effects
									return true;
								default:
									return false;
							}
							break;
						} else
						{
							// can't determine element
							return false;
						}
					} else
					{
						// can't determine class
						return false;
					}
				} else
				{
					// unknown method
					return false;
				}
			} else
			{
				// not an object...  later will throw 
				return false;
			}
			break;

		case astOp::symbolValue:
			if ( sym->isDefined ( node->symbolValue(), true ) )
			{
				switch ( sym->getScope ( node->symbolValue(), false ) )
				{
					case symbolSpaceScope::symLocal:
					case symbolSpaceScope::symClassConst:
					case symbolSpaceScope::symClassStatic:
					case symbolSpaceScope::symClassInherit:
					case symbolSpaceScope::symClassIVar:
						// just accessing a variable, this has no side-effects
						break;
					case symbolSpaceScope::symClassAccess:
						{
							auto elem = sym->findClass ( sym->getType ( sym->file->selfValue, true ).getClass () )->getElement ( node->symbolValue() );
							if ( !elem->methodAccess.func->isConst ) return false;
						}
						break;
					case symbolSpaceScope::symClassMethod:
					case symbolSpaceScope::symClassAssign:
						// shouldn't get here... we don't allow method "pointers" such as this...
						return false;
					default:
						break;
				}
			}
			break;
		case astOp::symbolDef:
			// symbol definitions always have side-effects... they CAN be eliminated but not via side effect discovery, only by non-access/assignment
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass *>(node->symbolDef());
						sym->insert ( node->symbolDef(), cls->insertPos );
					}
					break;
				default:
					sym->push ( node->symbolDef() );
					break;
			}
			return false;
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
		case astOp::btInline:
			if ( !hasNoSideEffects ( sym, node->inlineFunc().node, allowArrayAccess ) ) return false;
			break;
		case astOp::btLabel:
			break;
		case astOp::btInlineRet:
		case astOp::btGoto:
		case astOp::btReturn:
		case astOp::btBreak:
		case astOp::btCont:
			/* flow control is always a side-effect */
			return false;
		case astOp::btTry:
			if ( !hasNoSideEffects ( sym, node->tryCatchData().tryBlock, allowArrayAccess ) ) return false;
			if ( node->tryCatchData().catchBlock ) if ( !hasNoSideEffects ( sym, node->tryCatchData().catchBlock, allowArrayAccess ) ) return false;
			if ( node->tryCatchData().finallyBlock ) if ( !hasNoSideEffects ( sym, node->tryCatchData().finallyBlock, allowArrayAccess ) ) return false;
			break;
		case astOp::btForEach:
			if ( !hasNoSideEffects ( sym, node->forEachData().in, allowArrayAccess ) ) return false;
			if ( node->forEachData().statement )
			{
				if ( node->forEachData().var->getOp () == astOp::symbolValue )
				{
					symbolLocal tmp ( node->forEachData().var->symbolValue(), symWeakVariantType, node->location, stringi() );
					sym->push ( &tmp );
					auto hs = hasNoSideEffects ( sym, node->forEachData().statement, allowArrayAccess );
					sym->pop ();
					return hs;
				} else if ( node->forEachData ().var->getOp () == astOp::pairValue )
				{
					symbolLocal tmp ( node->forEachData ().var->left->symbolValue (), symWeakVariantType, node->location, stringi());
					sym->push ( &tmp );
					symbolLocal tmp2 ( node->forEachData ().var->right->symbolValue (), symWeakVariantType, node->location, stringi());
					sym->push ( &tmp2 );
					if ( !hasNoSideEffects ( sym, node->forEachData().statement, allowArrayAccess ) ) return false;
					sym->pop ( 2 );
				} else
				{
					if ( !hasNoSideEffects ( sym, node->forEachData ().statement, allowArrayAccess ) ) return false;
				}
			}
			break;
		case astOp::btFor:
			if ( node->loopData().initializers ) if ( !hasNoSideEffects ( sym, node->loopData().initializers, allowArrayAccess ) ) return false;
			if ( node->loopData().condition ) if ( !hasNoSideEffects ( sym, node->loopData().condition, allowArrayAccess ) ) return false;
			if ( node->loopData().block ) if ( !hasNoSideEffects ( sym, node->loopData().block, allowArrayAccess ) ) return false;
			if ( node->loopData().increase ) if ( !hasNoSideEffects ( sym, node->loopData().increase, allowArrayAccess ) ) return false;
			break;
		case astOp::btIf:
			for ( auto it = node->ifData().ifList.begin (); it != node->ifData().ifList.end (); it++ )
			{
				if ( (*it)->condition ) if ( !hasNoSideEffects ( sym, (*it)->condition, allowArrayAccess ) ) return false;
				if ( (*it)->block ) if ( !hasNoSideEffects ( sym, (*it)->block, allowArrayAccess ) ) return false;
			}
			if ( node->ifData().elseBlock ) if ( !hasNoSideEffects ( sym, node->ifData().elseBlock, allowArrayAccess ) ) return false;
			break;
		case astOp::btWhile:
			if ( node->loopData().condition ) if ( !hasNoSideEffects ( sym, node->loopData().condition, allowArrayAccess ) ) return false;
			if ( node->loopData().block ) if ( !hasNoSideEffects ( sym, node->loopData().block, allowArrayAccess ) ) return false;
			break;
		case astOp::btRepeat:
			if ( node->loopData().block ) if ( !hasNoSideEffects ( sym, node->loopData().block, allowArrayAccess ) ) return false;
			if ( node->loopData().condition ) if ( !hasNoSideEffects ( sym, node->loopData().condition, allowArrayAccess ) ) return false;
			break;
		case astOp::btSwitch:
			for ( auto it = node->switchData().caseList.begin (); it != node->switchData().caseList.end (); it++ )
			{
				if ( (*it)->condition ) if ( !hasNoSideEffects ( sym, (*it)->condition, allowArrayAccess ) ) return false;
				if ( (*it)->block ) if ( !hasNoSideEffects ( sym, (*it)->block, allowArrayAccess ) ) return false;
			}
			if ( !hasNoSideEffects ( sym, node->switchData().condition, allowArrayAccess ) ) return false;
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
			if ( node->left->getOp () == astOp::atomValue )
			{
				switch ( sym->getScope ( node->left->symbolValue(), true ) )
				{
					case symbolSpaceScope::symFunction:
					case symbolSpaceScope::symClassMethod:
						{
							auto func = sym->file->functionList.find( sym->getFuncName( node->left->symbolValue(), true ) )->second;
							if ( func->name == sym->file->newValue )
							{
								if ( node->pList().param[0]->getOp () == astOp::nameValue )
								{
									auto classDef = sym->findClass ( node->pList().param[0]->nameValue() );
									if ( classDef )
									{
										if ( classDef->newIndex )
										{
											auto destFunc = classDef->elements[classDef->newIndex - 1].methodAccess.func;
											if ( !destFunc->isConst )
											{
												return false;
											}
										}
										// no new function so no side effects
									} else
									{
										return false;
									}
								} else
								{
									return false;
								}
							} else
							{
								if ( !func->isConst )
								{
									return false;
								}
							}
						}
						break;
					default:
						// don't know what we're calling so return false
						return false;
				}
			} else if ( node->left->getOp () == astOp::sendMsg )
			{
				if ( node->left->left->getType ( sym ).isAnObject () )
				{
					// method calls
					symbolTypeClass type = node->left->getType ( sym );

					if ( node->left->right->getOp () == astOp::symbolValue )
					{
						compClass *classDef;

						if ( node->left->getType ( sym ).hasClass () && (classDef = sym->findClass ( type.getClass () )) )
						{
							compClassElementSearch *elem = classDef->getElement ( node->left->right->symbolValue() );
							if ( elem )
							{
								opFunction *func;
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_method:
										func = sym->file->functionList.find( elem->methodAccess.func->name )->second;
										if ( !func->isConst ) return false;
										break;
									default:
										return false;
								}
								break;
							} else
							{
								// can't determine element
								return false;
							}
						} else
						{
							// can't determine class
							return false;
						}
					} else
					{
						// unknown method
						return false;
					}
				} else
				{
					// not an object...  later will throw 
					return false;
				}
			} else
			{
				// indirect function call...
				return false;
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
			for ( auto &it : node->pList ().param )
			{
				if ( !hasNoSideEffects ( sym, it, allowArrayAccess ) ) return false;
			}
			if ( !allowArrayAccess )
			{
				auto type = node->left->getType ( sym );
				switch ( type.compType() )
				{
					case symbolType::symArray:
					case symbolType::symString:
						// we can't expand through a fixed array so this won't cause expansion side-effects
						return true;
					case symbolType::symUnknown:
					case symbolType::symVariant:
					case symbolType::symVArray:
						// in these cases we may well expand or call an overload, if so we
						return false;
					case symbolType::symObject:
						{
							compClass *classDef;

							if ( type.hasClass () && (classDef = sym->findClass ( type.getClass () )) )
							{
								// only need to worry about RHS accesses... LHS is an assign which is alwasy returned before this as having side-effects;
								if ( classDef && classDef->overload[int(fgxOvOp::ovArrayDerefRHS)] )
								{
									auto elem = &classDef->elements[classDef->overload[int(fgxOvOp::ovArrayDerefRHS)] -1];
									if ( elem )
									{
										if ( elem->isVirtual || !elem->methodAccess.func->isConst )
										{
											return false;
										}
										return hasNoSideEffects ( sym, node->right, allowArrayAccess );
									}
								}
							}
							// if this is true, we MAY be calling an overload... (either it will error out if the overload isn't present, or it's runtime decidable).   in any case
							// we do not want to eliminate the operation
							return false;
						}
					default:
						break;
				}
			}
			break;
		default:
			if ( node->left )
			{
				auto type = node->left->getType ( sym );
				if ( type.isAnObject () )
				{
					compClass *classDef;

					if ( type.hasClass () && (classDef = sym->findClass ( type.getClass () )) )
					{
						if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(node->getOp ())])] )
						{
							auto elem = &classDef->elements[classDef->overload[int(opToOvXref[static_cast<size_t>(node->getOp ())])] - 1];
							if ( elem )
							{
								if ( elem->isVirtual || !elem->methodAccess.func->isConst )
								{
									return false;
								}
								return hasNoSideEffects ( sym, node->right, allowArrayAccess );
							}
						}
					}
					// if this is true, we MAY be calling an overload... (either it will error out if the overload isn't present, or it's runtime decidable).   in any case
					// we do not want to eliminate the operation
					return false;
				}
			}
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

bool astNode::hasNoSideEffects ( symbolStack *sym, bool allowArrayAccess )
{
	symbolStack localSym = *sym;
	auto ret = ::hasNoSideEffects ( &localSym, this, allowArrayAccess );
	return ret;
}
