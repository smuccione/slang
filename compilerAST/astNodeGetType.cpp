/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "compilerParser/classParser.h"
#include "bcVM/fglTypes.h"

symbolTypeClass astNode::getType ( symbolStack *sym )
{
	symbolTypeClass  leftType;
	symbolTypeClass  rightType;
	compClass		*classDef = nullptr;

	switch ( op )
	{
		case astOp::assign:
			if ( left->getOp() == astOp::symbolValue )
			{
				return sym->getType ( left->symbolValue(), false );
			} else
			{
				return symWeakVariantType;
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			leftType = left->getType ( sym );
			if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
			{
				if ( classDef && classDef->overload[int(fgxOvOp::ovArrayDerefRHS)] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(fgxOvOp::ovArrayDerefRHS)]) - 1];
					if ( elem )
					{
						return sym->getFuncReturnType ( elem->methodAccess.func->name, true, accessorType(), nullptr );
					}
				}
			}
			return symWeakVariantType;
		case astOp::coalesce:
			leftType = left->getType ( sym );
			if ( (symbolTypeClass const&) leftType != right->getType ( sym ) )
			{
				return symWeakVariantType;
			} else
			{
				return leftType;
			}
			break;
		case astOp::divAssign:
		case astOp::divide:
			leftType = left->getType ( sym );
			if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])]) - 1];
					if ( elem )
					{
						return sym->getFuncReturnType( elem->methodAccess.func->name, true, accessorType(), nullptr );
					}
				}
			} else if ( leftType.isAnObject() )
			{
				return symWeakVariantType;
			}
			return symWeakDoubleType;
		case astOp::addAssign:
		case astOp::subAssign:
		case astOp::mulAssign:
		case astOp::powAssign:
			leftType = left->getType ( sym );
			if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])]) - 1];
					if ( elem )
					{
						return sym->getFuncReturnType( elem->methodAccess.func->name, true, accessorType(), nullptr );
					}
				}
			} else if ( leftType.isAnObject() )
			{
				return symWeakVariantType;
			}
			return leftType;
		case astOp::modAssign:
		case astOp::bwAndAssign:
		case astOp::bwOrAssign:
		case astOp::bwXorAssign:
		case astOp::shLeftAssign:
		case astOp::shRightAssign:
		case astOp::bitAnd:
		case astOp::bitOr:
		case astOp::bitXor:
		case astOp::shiftLeft:
		case astOp::shiftRight:
		case astOp::modulus:
		case astOp::logicalAnd:
		case astOp::logicalOr:
		case astOp::logicalNot:
		case astOp::equal:
		case astOp::notEqual:
		case astOp::less:
		case astOp::lessEqual:
		case astOp::greater:
		case astOp::greaterEqual:
			leftType = left->getType ( sym );
			if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])]) - 1];
					if ( elem )
					{
						return sym->getFuncReturnType( elem->methodAccess.func->name, true, accessorType(), nullptr );
					}
				}
			} else if ( leftType.isAnObject() )
			{
				return symbolType::symWeakVariant;
			}
			return symWeakIntType;
		case astOp::postInc:
		case astOp::preInc:
		case astOp::postDec:
		case astOp::preDec:
			leftType = left->getType ( sym );
			if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])]) - 1];
					if ( elem )
					{
						return sym->getFuncReturnType( elem->methodAccess.func->name, true, accessorType(), nullptr );
					}
				}
			} else if ( leftType.isAnObject() )
			{
				return symWeakVariantType;
			}
			return leftType;
		case astOp::twosComplement:
			leftType = left->getType ( sym );
			if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp())])]) - 1];
					if ( elem )
					{
						return sym->getFuncReturnType( elem->methodAccess.func->name, true, accessorType(), nullptr );
					}
				}
			} else if ( leftType.isAnObject() )
			{
			}
			return symWeakIntType;

		case astOp::symbolValue:
			return sym->getType ( symbolValue(), true );
		case astOp::atomValue:
			return symWeakAtomType;
		case astOp::funcValue:
			return symWeakFuncType;
		case astOp::codeBlockValue:
		case astOp::codeBlockExpression:
			return symWeakCodeblockType;
		case astOp::stringValue:
			return symWeakStringType;
		case astOp::symbolPack:
			return symWeakVariantType;
		case astOp::varrayValue:
			return symWeakVArrayType;
		case astOp::arrayValue:
			if ( arrayData().nodes.size () )
			{
				if ( arrayData().nodes[0]->getOp () == astOp::pairValue )
				{
					return symbolTypeClass ( symbolType::symWeakObject, sym->file->aArrayValue );
				}
				return symWeakArrayType;
			} else
			{
				return symWeakVArrayType;
			}
		case astOp::intValue:
			return symWeakIntType;
		case astOp::doubleValue:
			return symWeakDoubleType;
		case astOp::fieldValue:
		case astOp::nullValue:
		case astOp::nilValue:
			 return symWeakVariantType;
		case astOp::range:
			return sym->getFuncReturnType ( sym->file->rangeValue, true, accessorType (), nullptr );
		case astOp::getEnumerator:
			{
				auto leftType = left->getType ( sym );
				switch ( leftType )
				{
					case symbolType::symArray:
					case symbolType::symWeakArray:
						return symbolTypeClass( symbolType::symWeakObject, sym->file->fixedArrayEnumeratorValue );
					case symbolType::symVArray:
					case symbolType::symWeakVArray:
						return symbolTypeClass ( symbolType::symWeakObject, sym->file->sparseArrayEnumeratorValue );
					case symbolType::symString:
					case symbolType::symWeakString:
						return symbolTypeClass ( symbolType::symWeakObject, sym->file->stringEnumeratorValue );
					default:
						break;
				}
				compClass *classDef;
				if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
				{
					auto elem = classDef->getElement( sym->file->getEnumeratorValue );
					if ( elem )
					{
						return sym->getFuncReturnType( elem->methodAccess.func->name, true, accessorType(), nullptr );
					}
				}
				return symWeakObjectType;
			}
			break;
		case astOp::funcCall:
			if ( left->getOp() == astOp::atomValue )
			{
				if ( (left->symbolValue() == sym->file->newValue) && (pList().param[0]->getOp() == astOp::nameValue ) )
				{
					auto cl = sym->findClass ( pList().param[0]->nameValue() );
					if ( cl )
					{
						return symbolTypeClass ( symbolType::symObject, cl->oClass->name );
					} else
					{
						return symObjectType;
					}
				} else if ( left->symbolValue() == sym->file->varCopyValue )
				{
					return (*pList().param.begin ( ))->getType ( sym );
				} else if ( left->symbolValue() == sym->file->unpackValue || left->symbolValue() == sym->file->exceptValue )
				{
					return symWeakVariantType;
				} else
				{
					auto retType = sym->getFuncReturnType ( left->symbolValue(), true, accessorType(), nullptr );

					if ( (const symbolTypeClass)retType != symUnknownType )
					{
						return retType;
					}
				}
			} else if ( left->getOp ( ) == astOp::sendMsg )
			{
				symbolTypeClass type = left->left->getType ( sym );
				compClass *classDef;
				if ( type.hasClass ( ) && (classDef = sym->findClass ( type.getClass ( ) )) )
				{
					if ( classDef && left->right->getOp ( ) == astOp::nameValue  )
					{
						compClassElementSearch *elem = classDef->getElement ( left->right->nameValue() );

						if ( elem )
						{
							if ( elem->elem->isVirtual )
							{
								return symWeakObjectType;
							} else
							{
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_method:
										return elem->methodAccess.func->getReturnType();;
									case fgxClassElementType::fgxClassType_prop:
										if ( elem->methodAccess.func )
										{
											return elem->methodAccess.func->getReturnType();;
										}
										return symWeakVariantType;
									case fgxClassElementType::fgxClassType_iVar:
									case fgxClassElementType::fgxClassType_static:
									case fgxClassElementType::fgxClassType_const:
										leftType = elem->elem->symType;
										goto func_call_by_type;
									case fgxClassElementType::fgxClassType_inherit:
										return symbolTypeClass ( symbolType::symWeakObject, classDef->oClass->name );
									default:
										break;
								}
							}
						}
						if ( classDef->defaultAccessIndex )
						{
							auto elem = &classDef->elements[static_cast<size_t>(classDef->defaultAccessIndex) - 1];
							leftType = elem->elem->symType;
							goto func_call_by_type;
						}
						// really an error state
						return symWeakVariantType;
					} else
					{
						return symWeakVariantType;
					}
				}
				if ( !type.isAnObject () && type.compType () != symbolType::symUnknown )
				{
					// pseudo object calls
					if ( ((left->right->getOp () == astOp::nameValue) || (left->right->getOp () == astOp::atomValue)) )
					{
						auto retType = sym->getFuncReturnType ( left->right->symbolValue (), true, accessorType (), nullptr );

						if ( (const symbolTypeClass)retType != symUnknownType )
						{
							return retType;
						}
					}
				}
			} else
			{
				leftType = left->getType ( sym );
func_call_by_type:
				if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
				{
					if ( classDef && classDef->overload[int(fgxOvOp::ovFuncCall)] )
					{
						auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(fgxOvOp::ovFuncCall)]) - 1];
						if ( elem )
						{
							return sym->getFuncReturnType( elem->methodAccess.func->name, true, accessorType(), nullptr );
						}
					}
					return symWeakVariantType;
				}
			}
			return symWeakVariantType;
		case astOp::sendMsg:
			{
				compClass *classDef;
				symbolTypeClass const type = left->getType ( sym );

				if ( type.hasClass ( ) && (right->getOp ( ) == astOp::nameValue) && (classDef = sym->findClass ( type.getClass ( ) )) )
				{
					fgxClassElemSearchType searchType;
					compClassElementSearch *elem;

					if ( left->getOp () == astOp::symbolValue )
					{
						if ( !strccmp ( left->symbolValue (), sym->file->selfValue ) )
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
					elem = classDef->getElement ( searchType, right->nameValue () );

					if ( elem )
					{
						if ( elem->elem->isVirtual )
						{
							return symWeakObjectType;
						} else
						{
							switch ( elem->type )
							{
								case fgxClassElementType::fgxClassType_method:
								case fgxClassElementType::fgxClassType_prop:
									return elem->methodAccess.func->getReturnType();;
								case fgxClassElementType::fgxClassType_iVar:
								case fgxClassElementType::fgxClassType_static:
								case fgxClassElementType::fgxClassType_const:
									return elem->elem->symType;
								case fgxClassElementType::fgxClassType_inherit:
									return symbolTypeClass ( symbolType::symWeakObject, elem->name );
								default:
									break;
							}
						}
					}
				} else if ( !type.isAnObject ( ) && right->getOp ( ) == astOp::symbolValue )
				{
					if ( left->getOp ( ) == astOp::atomValue )
					{
						symbolTypeClass const retType = sym->getFuncReturnType ( right->symbolValue(), true, accessorType(), nullptr );

						if ( retType != symUnknownType )
						{
							return std::forward<symbolTypeClass const > ( retType );
						}
					}
				}
			}
			return symWeakVariantType;
		case astOp::refCreate:
		case astOp::comp:
			return symWeakVariantType;
		case astOp::btInline:
			return this->inlineFunc().retType;
		case astOp::workAreaStart:
			if ( left->getOp() == astOp::symbolValue )
			{
				if ( right->getOp() == astOp::symbolValue )
				{
					// must be a field
					return symWeakVariantType;
				}
			}
			return right->getType ( sym );		// some sort of expression with a workarea alias.. type is type of expression
		case astOp::conditional:
			{
				symbolTypeClass type = conditionData().trueCond->getType ( sym );
				if ( type != conditionData().falseCond->getType ( sym ) )
				{
					return symWeakVariantType;
				} else
				{
					return type;
				}
			}
		default:
			if ( left )
			{
				return left->getType ( sym );
			}
			if ( right )
			{
				return right->getType ( sym );
			}
			return symWeakVariantType;
	}
	// should never get here
	return symWeakVariantType;
}

symbolType astNode::getCompType ( symbolStack *sym )
{
	return getType( sym ).compType();
}
