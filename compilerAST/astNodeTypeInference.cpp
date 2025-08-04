/*

	astNodebuildSymbolSearch ( false );

*/

#include "compilerParser/fileParser.h"
#include "compilerParser/classParser.h"
#include "bcInterpreter/bcInterpreter.h"

#define isInFunc(n) if ( std::holds_alternative<opFunction *>(acc) && std::get<opFunction *>(acc)->name == (n) )

static std::recursive_mutex	errorMutex;

bool scheduleMakeIterator( class opFile* file, opFunction * func, bool generic, accessorType const& acc, unique_queue<accessorType> *scanQueue, bool isLS )
{
	if ( !func->isIterator ) return false;
	if ( func->isInterface ) return false;
	if ( scanQueue ) scanQueue->push( new makeIteratorType { func, acc, generic } );
	return true;
}

void typeInferParams ( cacheString &funcName, astNode *funcCallNode, size_t numHidden, compExecutable *comp, accessorType const &acc, errorHandler *err, symbolStack *sym, symbolTypeClass *inlineRetType, bool interProcedural, bool needValue, bool markUsed, unique_queue<accessorType> *scanQueue, bool isLS )
{
	errorLocality e ( err, funcCallNode );
	for ( auto it = funcCallNode->pList ().param.begin (); it != funcCallNode->pList ().param.end (); it++ )
	{
		if ( interProcedural )
		{
			if ( (*it) )
			{
				auto type = (*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				if ( isLS )
				{
					sym->setParameterTypeNoThrow ( comp, sym, funcName, true, (uint32_t)(std::distance ( funcCallNode->pList ().param.begin (), it ) + numHidden), type, acc, scanQueue );
				} else
				{
					sym->setParameterType ( comp, sym, funcName, true, (uint32_t)(std::distance ( funcCallNode->pList ().param.begin (), it ) + numHidden), type, acc, scanQueue );
				}
			} else
			{
				auto defParam = sym->getFuncDefaultParam ( funcName, true, (int32_t) (std::distance ( funcCallNode->pList ().param.begin (), it ) + numHidden) );
				if ( defParam )
				{
					if ( defParam->getOp () == astOp::assign ) defParam = defParam->right;
					auto type = defParam->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					if ( isLS )
					{
						sym->setParameterType ( comp, sym, funcName, true, (uint32_t)(std::distance ( funcCallNode->pList ().param.begin (), it ) + numHidden), type, acc, scanQueue );
					} else
					{
						sym->setParameterTypeNoThrow ( comp, sym, funcName, true, (uint32_t)(std::distance ( funcCallNode->pList ().param.begin (), it ) + numHidden), type, acc, scanQueue );
					}
				} else
				{
					if ( isLS )
					{
						sym->setParameterTypeNoThrow ( comp, sym, funcName, true, (uint32_t)(std::distance ( funcCallNode->pList ().param.begin (), it ) + numHidden), symWeakVariantType, acc, scanQueue );
					} else
					{
						sym->setParameterType ( comp, sym, funcName, true, (uint32_t)(std::distance ( funcCallNode->pList ().param.begin (), it ) + numHidden), symWeakVariantType, acc, scanQueue );
					}
				}
			}
		} else
		{
			if ( (*it) ) (*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
		}
	}
	if ( interProcedural )
	{
		if ( sym->getFuncNumParams ( funcName, true ) && funcCallNode->pList ().param.size () < sym->getFuncNumParams ( funcName, true ) - numHidden )
		{
			for ( auto it = funcCallNode->pList ().param.size (); it < sym->getFuncNumParams ( funcName, true ) - numHidden; it++ )
			{
				auto defParam = sym->getFuncDefaultParam ( funcName, true, (int32_t) (it + numHidden) );
				if ( defParam )
				{
					if ( defParam->getOp () == astOp::assign ) defParam = defParam->right;
					auto type = defParam->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					if ( isLS )
					{
						sym->setParameterTypeNoThrow ( comp, sym, funcName, true, (uint32_t)(it + numHidden), type, acc, scanQueue );
					} else
					{
						sym->setParameterType ( comp, sym, funcName, true, (uint32_t)(it + numHidden), type, acc, scanQueue );
					}
				} else
				{
					if ( isLS )
					{
						sym->setParameterTypeNoThrow ( comp, sym, funcName, true, (uint32_t)(it + numHidden), symWeakVariantType, acc, scanQueue );
					} else
					{
						sym->setParameterType ( comp, sym, funcName, true, (uint32_t)(it + numHidden), symWeakVariantType, acc, scanQueue );
					}
				}
			}
		}
	}
}

void markOvOp ( class compExecutable *comp, astOp op, symbolStack *sym, accessorType const &acc, symbolTypeClass const &rightType, bool interProcedural, unique_queue<accessorType> *scanQueue, bool isLS, srcLocation &loc )
{
	auto opDef = opToOpDef[(size_t) op];

 	if ( opDef->overloadOp == fgxOvOp::ovNone ) return;

	for ( auto &elem : comp->opOverloads[int(opDef->overloadOp)] )
	{
		for ( auto &func : elem->elem->data.method.virtOverrides )
		{
			if ( interProcedural && func->classDef->inUse )
			{
				func->setAccessed ( acc, scanQueue, loc );
				if ( opDef->opCat == astOpCat::opBinary )
				{
					func->setParamTypeNoThrow ( comp, sym, 1, rightType, scanQueue );
				}
			}
		}
	}
	return;
}

void setOvOpCalled ( class compExecutable *comp, astOp op, accessorType const &acc, bool isLS, srcLocation &loc )
{
	if ( isLS && std::holds_alternative<opFunction *> ( acc ) )
	{
		auto opDef = opToOpDef[(size_t)op];

		if ( opDef->overloadOp == fgxOvOp::ovNone ) return;

		for ( auto &elem : comp->opOverloads[int ( opDef->overloadOp )] )
		{
			for ( auto &func : elem->elem->data.method.virtOverrides )
			{
				std::get<opFunction *> ( acc )->setCalled ( acc, loc );
			}
		}
	}
	return;
}

bool isFuncMarkable ( opFile *file, opFunction *func, accessorType const &acc )
{
	auto accFunc = std::get_if<opFunction *> ( &acc );
	if ( func->classDef )
	{
		if ( (*accFunc)->classDef )
		{
			if ( (*accFunc)->classDef->name == func->classDef->name )
			{
				return true;
			}
		}
		return false;
	}
	if ( func->parentClosure.size () )
	{
		return isFuncMarkable ( file, file->functionList.find( func->parentClosure )->second, acc );
	}
	return true;
}

void setCalled ( accessorType const &acc, opFunction *func, bool isLS, srcLocation &loc  )
{
	if ( isLS )
	{
		if ( std::holds_alternative<opFunction *> ( acc ) )
		{
			std::get<opFunction *> ( acc )->setCalled ( acc, loc );
		}
	}
}

void markClassInuse ( class compExecutable *comp, accessorType const &acc, class opClass *cls, symbolStack const *sym, unique_queue<accessorType> *scanQueue, bool isLS, srcLocation const &loc )
{
	if ( !cls->inUse )
	{
		std::lock_guard g1( cls->accessorAccess );
		if ( cls->inUse ) return;		// in case we were marked while waiting

		// we had some functions which interacted with classes based only upon method name and not class name.   we need to rescan these
		for ( auto &it : cls->accessors )
		{
			if ( scanQueue ) scanQueue->push ( it );
		}
		for ( auto &elem : cls->elems )
		{
			switch ( elem->type )
			{
				case fgxClassElementType::fgxClassType_inherit:
					{
						auto base = sym->findClass ( elem->name );
						if ( base )
						{
							markClassInuse ( comp, acc, base->oClass, sym, scanQueue, isLS, loc );
						}
					}
					break;
				default:
					break;
			}
		}
		for ( auto &name : cls->requiredClasses )
		{
			auto base = sym->findClass ( name );
			if ( base )
			{
			// mark everything as used... these are FFI used and therefore transparent to us
				markClassMethodsInuse ( comp, acc, base->oClass, sym, scanQueue, false, isLS, loc );
			}
		}

		// special functions must always exist when a class that contains them is instantiated.   They can, in general, not be easily traced in terms of callability.  They are invoked
		// via internal operations (pack/unpack), are invloved in the construction/destruction of objects (new/release, etc.) or are invoked by the VM on non-static messages (defaultXXX).

		auto specialFuncs = { cls->cClass->newBaseIndex,
								cls->cClass->newIndex,
								cls->cClass->cNewIndex,
								cls->cClass->releaseIndex,
								cls->cClass->releaseBaseIndex,
								cls->cClass->cReleaseIndex,
								cls->cClass->defaultMethodIndex,
								cls->cClass->defaultAssignIndex,
								cls->cClass->defaultAccessIndex,
								cls->cClass->packIndex,
								cls->cClass->unpackIndex
		};

		for ( size_t it : specialFuncs )
		{
			if ( it )
			{
				auto elem = &cls->cClass->elements[it - 1];
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_method:
						{
							auto callee = elem->methodAccess.func;
							callee->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc);
							callee->setAccessed ( acc, scanQueue, loc );
						}
						break;
					case fgxClassElementType::fgxClassType_prop:
						{
							if ( elem->methodAccess.func )
							{
								auto callee = elem->methodAccess.func;
								callee->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc );
								callee->setAccessed ( acc, scanQueue, loc );
							}
							if ( elem->assign.func )
							{
								auto callee = elem->assign.func;
								callee->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc );
								callee->setAccessed ( acc, scanQueue, loc );
							}
						}
						break;
					default:
						throw errorNum::scINTERNAL;
				}
			}
		}

		if ( cls->opOverload[int(fgxOvOp::ovFuncCall)] )
		{
			auto elem = &cls->cClass->elements[static_cast<size_t>(cls->cClass->overload[int(fgxOvOp::ovFuncCall)]) - 1];
			auto callee = elem->methodAccess.func;
			callee->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, loc);
			callee->setAccessed ( acc, scanQueue, loc );
		}

		auto it = cls->cClass->getElement ( sym->file->getEnumeratorValue );
		if ( it )
		{
			auto elem = &cls->cClass->elements[it->index];
			auto callee = elem->methodAccess.func;
			if ( std::holds_alternative<opFunction *> ( acc ) && std::get<opFunction *> ( acc ) != callee )
			{
				if ( scheduleMakeIterator ( sym->file, callee, false, acc, scanQueue, isLS ) )
				{
					auto cls = sym->findClass ( sym->file->queryableValue );
					if ( cls )
					{
						markClassMethodsInuse ( comp, acc, cls->oClass, sym, scanQueue, true, isLS, loc );
					}
				}
			} else if ( callee )
			{
				cls->inUse = true;
				markClassInuse ( comp, acc, callee->classDef, sym, scanQueue, isLS, loc );
			}
			if ( callee )
			{
				callee->setAccessed ( acc, scanQueue, loc );
			}
		}
		cls->inUse = true;
	}
}

void markClassMethodsInuse ( class compExecutable *comp, accessorType const &acc, opClass *cls, symbolStack const *sym, unique_queue<accessorType> *scanQueue, bool genericIterators, bool isLS, srcLocation const &loc )
{
	std::lock_guard g1( cls->accessorAccess );

	if ( !cls->inUse )
	{
		// we had some functions which interacted with classes based only upon method name and not class name.   we need to rescan these
		for ( auto &it : cls->accessors )
		{
			if ( scanQueue ) scanQueue->push ( it );
		}
		cls->inUse = true;
	}

	for ( auto &elem : cls->elems )
	{
		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_inherit:
				{
					auto base = sym->findClass ( elem->name );
					if ( base )
					{
						markClassInuse ( comp, acc, base->oClass, sym, scanQueue, isLS, loc );
					}
				}
				break;
			case fgxClassElementType::fgxClassType_method:
				{
					auto callee = sym->file->functionList.find( elem->data.method.func )->second;
					callee->setAccessed ( acc, scanQueue, loc );
					if ( std::holds_alternative<opFunction *> ( acc ) && std::get<opFunction *> ( acc ) != callee )
					{
						if ( scheduleMakeIterator ( sym->file, callee, genericIterators, acc, scanQueue, isLS ) )
						{
							if ( cls->name != sym->file->queryableValue )
							{
								auto cls = sym->findClass ( sym->file->queryableValue );
								if ( cls )
								{
									markClassMethodsInuse ( comp, acc, cls->oClass, sym, scanQueue, true, isLS, loc );
								}
							}
						}
					}
				}
				break;
			case fgxClassElementType::fgxClassType_prop:
				{
					if ( elem->data.prop.access.size () )
					{
						auto callee = sym->file->functionList.find( elem->data.prop.access )->second;
						callee->setAccessed ( acc, scanQueue, loc );
					}
					if ( elem->data.prop.assign.size () )
					{
						auto callee = sym->file->functionList.find( elem->data.prop.assign )->second;
						callee->setAccessed ( acc, scanQueue, loc );
					}
				}
				break;
			default:
				break;
		}
	}
	for ( auto &name : cls->requiredClasses )
	{
		auto base = sym->findClass ( name );
		if ( base )
		{
			markClassInuse ( comp, acc, base->oClass, sym, scanQueue, isLS, loc );
		}
	}
	for ( auto &it : cls->opOverload )
	{
		if ( it )
		{
			sym->file->functionList.find( it->data.method.func)->second->setAccessed ( acc, scanQueue, loc );
		}
	}
}

symbolTypeClass astNode::typeInfer ( compExecutable *comp, accessorType const &acc, errorHandler *err, symbolStack *sym, symbolTypeClass *inlineRetType, bool interProcedural, bool needValue, bool markUsed, unique_queue<accessorType> *scanQueue, bool isLS )
{
	if ( !(ptrdiff_t) this ) return symWeakVariantType;

	symbolTypeClass  leftType;
	symbolTypeClass  rightType;
	compClass *classDef = nullptr;

	switch ( op )
	{
		case astOp::errorValue:
			return symUnknownType;
		case astOp::intCast:
			return symWeakIntType;
		case astOp::doubleCast:
			return symWeakDoubleType;
		case astOp::stringCast:
			return symWeakStringType;
		case astOp::nilValue:
		case astOp::nullValue:
			return symWeakVariantType;
		case astOp::symbolDef:
			switch ( symbolDef ()->symType )
			{
				case symbolSpaceType::symTypeLocal:
					{
						auto init = symbolDef ()->getInitializer ( symbolDef ()->getSymbolName () );
						if ( init && init->getOp() == astOp::assign )
						{
							auto type = init->right->typeInfer ( comp, acc, err, sym, nullptr, interProcedural, true, markUsed, scanQueue, isLS );
							symbolDef ()->setInitType ( init->left->symbolValue (), true, type, acc, scanQueue );
						}
						sym->push ( symbolDef () );
					}
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass *>(symbolDef ());
						markClassInuse ( comp, acc, cls->getClass ()->oClass, sym, scanQueue, isLS, location );
						sym->insert ( symbolDef (), cls->insertPos );
					}
					break;
				case symbolSpaceType::symTypeStatic:
					{
						auto init = symbolDef ()->getInitializer ( symbolDef ()->getSymbolName () );
						if ( init && init->getOp () == astOp::assign )
						{
							auto type = init->right->typeInfer ( comp, acc, err, sym, nullptr, interProcedural, true, markUsed, scanQueue, isLS );
							symbolDef ()->setInitType ( symbolDef ()->getSymbolName(), true, type, acc, scanQueue );
						} else
						{
							symbolDef ()->setInitType ( symbolDef ()->getSymbolName (), true, symWeakVariantType, acc, scanQueue );
						}
						sym->push ( symbolDef () );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef ());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							auto s = (*symbol)[it];

							if ( s->initializer && s->initializer->getOp () == astOp::assign )
							{
								s->setInitType ( s->initializer->right->typeInfer ( comp, acc, err, sym, nullptr, interProcedural, true, markUsed, scanQueue, isLS ), acc, scanQueue );
							} else
							{
								s->setInitType ( symWeakVariantType, acc, scanQueue );
							}
						}
						sym->push ( symbolDef () );
					}
					break;
				default:
					sym->push ( symbolDef () );
					break;
			}
			break;
		case astOp::btBasic:
			{
				// save and restore our symbol table as we traverse
				auto size = sym->size ();
				for ( auto it = basicData ().blocks.begin (); it != basicData ().blocks.end (); it++ )
				{
					(*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
				}
				sym->resize ( size );
			}
			break;
		case astOp::btSwitch:
			switchData ().condition->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
			for ( auto it = switchData ().caseList.begin (); it != switchData ().caseList.end (); it++ )
			{
				(*it)->block->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
				(*it)->condition->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData ().ifList.begin (); it != ifData ().ifList.end (); it++ )
			{
				(*it)->block->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
				(*it)->condition->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			}

			ifData ().elseBlock->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
			break;
		case astOp::btInline:
			{
				errorLocality e ( err, this, true );
				auto func = sym->file->functionList.find ( inlineFunc ().funcName )->second;
				func->setInlineUsed ( acc, scanQueue, location );
				inlineFunc ().node->typeInfer ( comp, acc, err, sym, &inlineFunc ().retType, interProcedural, false, markUsed, scanQueue, isLS );
			}
			return inlineFunc ().retType;
		case astOp::btFor:
			loopData ().initializers->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			loopData ().condition->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			loopData ().increase->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
			loopData ().block->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
			break;
		case astOp::btForEach:
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			loopData ().block->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
			loopData ().condition->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			break;
		case astOp::btTry:
			tryCatchData ().catchBlock->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
			tryCatchData ().errSymbol->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
			tryCatchData ().finallyBlock->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
			tryCatchData ().tryBlock->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, false, markUsed, scanQueue, isLS );
			sym->setType ( tryCatchData ().errSymbol->symbolValue (), false, symVariantType, acc, scanQueue );
			break;
		case astOp::btReturn:
			if ( !std::holds_alternative<opFunction *> ( acc ) )
			{
				errorLocality e ( err, this );
				throw errorNum::scINTERNAL;
			}
			if ( returnData ().node )
			{
				auto func = std::get<opFunction *> ( acc );
				symbolTypeClass newType = returnData ().node->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				func->setReturnType ( newType, scanQueue, location );
				return func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
			}
			return symWeakVariantType;
		case astOp::btInlineRet:
			if ( returnData ().node )
			{
				assert ( inlineRetType );

				symbolTypeClass newType = returnData ().node->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );

				if ( newType != symbolType::symUnknown )
				{
					switch ( *inlineRetType )
					{
						case symbolType::symVariant:
						case symbolType::symWeakVariant:
							// do nothing... once a variant, always a variant
							break;
						case symbolType::symUnknown:
							*inlineRetType = newType;
							break;
						default:
							if ( ((symbolTypeClass const &)*inlineRetType) != newType )
							{
								(*inlineRetType).forceVariant ();
							}
							break;
					}
				}
			}
			break;

		case astOp::assign:
//isInFunc ( "aareverse" )
//if ( location.lineNumberStart == 366 )
//	printf ( "" );
			rightType = right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			if ( left->getOp () == astOp::symbolValue )
			{
				switch ( sym->getScope ( left->symbolValue (), false ) )
				{
					case symbolSpaceScope::symLocal:
					case symbolSpaceScope::symLocalParam:
						leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
						// see if it's an object that has overload assignment
						if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
						{
							if ( classDef && classDef->overload[int(fgxOvOp::ovAssign)] )
							{
								auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[size_t(fgxOvOp::ovAssign)]) - 1];
								if ( elem )
								{
									if ( interProcedural )
									{
										errorLocality e ( err, this );
										if ( isLS )
										{
											sym->setParameterTypeNoThrow ( comp, sym, elem->methodAccess.func->name, true, 1, rightType, acc, scanQueue );
										} else
										{
											sym->setParameterType ( comp, sym, elem->methodAccess.func->name, true, 1, rightType, acc, scanQueue );
										}
									}
									setCalled ( acc, elem->methodAccess.func, isLS, location );
									sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
									if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst &= sym->isConst( elem->methodAccess.func->name, false );
									if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure &= sym->isPure( elem->methodAccess.func->name, false );
									return elem->methodAccess.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
								}
							}
						} else if ( leftType.isAnObject () )
						{
							setOvOpCalled ( comp, this->getOp(), acc, isLS, location );
							markOvOp ( comp, this->getOp (), sym, acc, rightType, interProcedural, scanQueue, isLS, location );
							if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
							if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
						}
						sym->setType ( left->symbolValue (), false, rightType, acc, scanQueue );
						return sym->getType ( left->symbolValue (), false );
					case symbolSpaceScope::symGlobal:
						sym->setType ( left->symbolValue (), false, rightType, acc, scanQueue );
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
						return sym->getType ( left->symbolValue (), false );
					case symbolSpaceScope::symLocalField:
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
						return symWeakVariantType;
					case symbolSpaceScope::symLocalStatic:
						sym->setType ( left->symbolValue (), false, rightType, acc, scanQueue );
						return sym->getType ( left->symbolValue (), false );
					case symbolSpaceScope::symClassStatic:
						if ( std::holds_alternative<opFunction *> ( acc ) )
						{
							sym->setAccessed ( sym->file->selfValue, true, acc, scanQueue, location );
						}
						if ( interProcedural )
						{
							sym->setType ( left->symbolValue (), false, rightType, acc, scanQueue );
						}
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
						return sym->getType ( left->symbolValue (), false );
					case symbolSpaceScope::symClassIVar:
						sym->setAccessed ( sym->file->selfValue, true, acc, scanQueue, location );
						if ( interProcedural )
						{
							sym->setType ( left->symbolValue (), false, rightType, acc, scanQueue );
						}
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
						return sym->getType ( left->symbolValue (), false );
					case symbolSpaceScope::symClassAssign:
						{
							auto elem = sym->findClass ( sym->getType ( sym->file->selfValue, true ).getClass () )->getElement ( left->symbolValue () );
							sym->setAccessed ( sym->file->selfValue, true, acc, scanQueue, location );
							if ( interProcedural )
							{
								errorLocality e ( err, left );
								if ( isLS )
								{
									sym->setParameterTypeNoThrow ( comp, sym, left->symbolValue (), false, 1, rightType, acc, scanQueue );
								} else
								{
									sym->setParameterType ( comp, sym, left->symbolValue (), false, 1, rightType, acc, scanQueue );
								}
							}
							setCalled ( acc, elem->assign.func, isLS, location );
							sym->setAccessed ( left->symbolValue (), false, acc, scanQueue, location );
							if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst &= sym->isConst( elem->assign.func->name, false );
							if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure &= sym->isPure( elem->assign.func->name, false );
							return elem->assign.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
						}
					default:
						{
							if ( !isLS )
							{
								errorLocality e ( err, this );
								throw errorNum::scILLEGAL_ASSIGNMENT;
							}
							return symWeakVariantType;
						}
				}
			} else if ( left->getOp () == astOp::symbolPack )
			{
				// {a,b} = {b,a} assignment... lots of work to do here.. we need to iterate over all the symbols on the left hand side and set types equivalent to right
				if ( right->getOp () == astOp::arrayValue )
				{
					size_t nParams = left->arrayData ().nodes.size () < right->arrayData ().nodes.size () ? left->arrayData ().nodes.size () : right->arrayData ().nodes.size ();
					for ( size_t loop = 0; loop < nParams; loop++ )
					{
						// for each of the pairs, do type inference as if it was a direct assignment
						// create an assign op with the lhs and rhs for each of the array entries and type infer over the assignment op to propagate types appropriately
						astNode a ( sym->file->sCache, astOp::assign );
						a.left = left->arrayData ().nodes[loop];
						a.right = right->arrayData ().nodes[loop];

						if ( a.left && a.right )
						{
							a.typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
							a.left = 0;
							a.right = 0;
						}
					}
				} else
				{
					// we don't know for sure what's on the right hand side so don't know what's being assigned so need to make all the lhs variables variant
					size_t nParams = left->arrayData ().nodes.size ();
					for ( size_t loop = 0; loop < nParams; loop++ )
					{
						// create an assign op with the lhs and rhs a nullValue and type infer over this assign op to simulate the assignment that will occur
						astNode a ( sym->file->sCache, astOp::assign );
						astNode n ( sym->file->sCache, astOp::nullValue );
						if ( left->arrayData ().nodes[loop] && left->arrayData ().nodes[loop]->getOp() == astOp::symbolValue )
						{
							a.left = left->arrayData ().nodes[loop];
							a.right = &n;

							a.typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
						}
						a.left = 0;
						a.right = 0;
					}
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				}
				return symWeakVariantType;
			} else if ( left->getOp () == astOp::sendMsg )
			{
				left->right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				leftType = left->left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				if ( leftType.isAnObject () )
				{
					if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
					{
						if ( classDef && left->right->getOp () == astOp::nameValue )
						{
							// find the element we're assigning to
							auto elem = classDef->getElement ( left->right->nameValue () );
							if ( elem )
							{
								// what exactly are we trying to assign to
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_prop:
										{
											symbolTypeClass retType = symUnknownType;
											for ( auto &func : elem->elem->data.prop.assignVirtOverrides )
											{
												func->setAccessed ( acc, scanQueue, location );
												if ( interProcedural )
												{
													errorLocality e ( err, left );
													func->setParamType ( comp, sym, 1, rightType, scanQueue );
												}
												retType &= func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
												setCalled ( acc, func, isLS, location );
											}
										}
										break;
									case fgxClassElementType::fgxClassType_iVar:
										if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
										if ( interProcedural )
										{
											elem->elem->setType ( false, rightType, acc, scanQueue );
											return elem->elem->getType ( false );
										} else
										{
											return symWeakVariantType;
										}
									case fgxClassElementType::fgxClassType_static:
										if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
										if ( interProcedural )
										{
											sym->setType ( elem->symbolName, false, rightType, acc, scanQueue );
											return sym->getType ( elem->symbolName, true );
										} else
										{
											return symWeakVariantType;
										}
									default:
										{
											if ( !isLS )
											{
												errorLocality e ( err, this );
												throw errorNum::scILLEGAL_ASSIGNMENT;
											}
											return symWeakVariantType;
										}
								}
							} else if ( classDef->defaultAssignIndex )
							{
								auto elem = &classDef->elements[static_cast<size_t>(classDef->defaultAssignIndex) - 1];
								if ( elem->assign.func )
								{
									if ( interProcedural )
									{
										errorLocality e ( err, left );
										if ( isLS )
										{
											sym->setParameterTypeNoThrow ( comp, sym, elem->assign.func->name, false, 2, rightType, acc, scanQueue );
										} else
										{
											sym->setParameterType ( comp, sym, elem->assign.func->name, false, 2, rightType, acc, scanQueue );
										}
									}
									elem->assign.func->setAccessed ( acc, scanQueue, location );
									setCalled ( acc, elem->assign.func, isLS, location );
									return elem->assign.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
								}
							} else
							{
								if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
								if ( !isLS )
								{
									errorLocality e ( err, this );
									throw errorNum::scUNKNOWN_IVAR;
								}
								return symWeakVariantType;
							}
						} else
						{
							if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
							// we're assigning, but can't tell which element we're assigning to since it's a run-time value
							auto isSelf = (left->left->getOp () == astOp::symbolValue) && (left->left->symbolValue () == sym->file->selfValue);

							// the only thing we can do is to set the 1st parmaeter of all property setters
							for ( auto &it : classDef->elements )
							{
								if ( isSelf || (it.scope == fgxClassElementScope::fgxClassScope_public) )
								{
									switch ( it.type )
									{
										case fgxClassElementType::fgxClassType_prop:
											if ( interProcedural )
											{
												if ( it.assign.func )
												{
													errorLocality e ( err, left );
													sym->setAccessed ( it.assign.func->name, false, acc, scanQueue, location );
													sym->setParameterTypeNoThrow ( comp, sym, it.assign.func->name, false, 1, rightType, acc, scanQueue );
													it.assign.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
												}
											}
											break;
										case fgxClassElementType::fgxClassType_iVar:
											if ( interProcedural )
											{
												it.elem->setTypeNoThrow ( false, rightType, acc, scanQueue );
											}
											break;
										case fgxClassElementType::fgxClassType_static:
											if ( interProcedural )
											{
												sym->setType ( it.symbolName, false, rightType, acc, scanQueue );
//												it.elem->setTypeNoThrow ( false, rightType, acc, scanQueue );
											}
											break;
										default:
											break;
									}
								}
							}
							return symWeakVariantType;
						}
					} else
					{
						// unknown object
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
						if ( interProcedural )
						{
							if ( left->right->getOp () == astOp::nameValue )
							{
								if ( !isLS )
								{
									// don't have a class type, but we DO have a method name... for each of the methods in all classes that match that method name we need to force types to variant
									for ( auto &cls : sym->file->classList )
									{
										// NOTE: we never drag in classes... the only place that classes are marked as in-use is when we do a new...  if we don't create it we don't need to worry about it
										if ( cls.second->inUse )
										{
											auto elem = cls.second->cClass->getElement ( left->right->nameValue () );
											if ( elem && elem->scope == fgxClassElementScope::fgxClassScope_public )
											{
												switch ( elem->type )
												{
													case fgxClassElementType::fgxClassType_prop:
														if ( elem->assign.func )
														{
															errorLocality e ( err, this );
															elem->assign.func->setAccessed ( acc, scanQueue, location );
															elem->assign.func->setParamTypeNoThrow ( comp, sym, 1, rightType, scanQueue );
															elem->assign.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
														}
														break;
													case fgxClassElementType::fgxClassType_iVar:
														elem->elem->setTypeNoThrow ( false, rightType, acc, scanQueue );
														break;
													case fgxClassElementType::fgxClassType_static:
														elem->elem->symType ^= rightType;
														sym->setAccessed ( elem->symbolName, false, acc, scanQueue, location );
														break;
													default:
														break;
												}
											}
										} else
										{
											std::lock_guard g1 ( cls.second->accessorAccess );
											cls.second->accessors.insert ( acc );
										}
									}
								}
							} else
							{
								if ( !isLS )
								{
									// worst case astOp::of all... we don't now object and we don't know accessor so force all parameters of all accessors to variant
									for ( auto &cls : sym->file->classList )
									{
										// NOTE: we never drag in classes... the only place that classes are marked as in-use is when we do a new...  if we don't create it we don't need to worry about it
										if ( cls.second->inUse )
										{
											for ( auto &elem : cls.second->cClass->elements )
											{
												if ( elem.scope == fgxClassElementScope::fgxClassScope_public )
												{
													switch ( elem.type )
													{
														case fgxClassElementType::fgxClassType_prop:
															if ( elem.assign.func )
															{
																errorLocality e ( err, this );
																elem.assign.func->setAccessed ( acc, scanQueue, location );
																elem.assign.func->setParamTypeNoThrow ( comp, sym, 1, symWeakVariantType, scanQueue );
															}
															break;
														case fgxClassElementType::fgxClassType_iVar:
															elem.elem->setTypeNoThrow ( false, rightType, acc, scanQueue );
															break;
														case fgxClassElementType::fgxClassType_static:
															elem.elem->symType ^= rightType;
															sym->setAccessed ( elem.symbolName, false, acc, scanQueue, location );
															break;
														default:
															break;
													}
												}
											}
										} else
										{
											std::lock_guard g1 ( cls.second->accessorAccess );
											cls.second->accessors.insert ( acc );
										}
									}
								}
							}
						}
						return symWeakVariantType;
					}
				} else if ( leftType == symUnknownType )
				{
					return symUnknownType;
				} else
				{
					// this is an error case... what does x.b = a mean when x is not an object.
					if ( !isLS )
					{
						errorLocality e ( err, this );
						throw errorNum::scILLEGAL_ASSIGNMENT;
					}
					return symWeakVariantType;
				}
			} else if ( left->getOp () == astOp::arrayDeref || left->getOp () == astOp::cArrayDeref )
			{
				if ( left->left->getOp () == astOp::symbolValue )
				{
					sym->setType ( left->left->symbolValue (), true, symUnknownType, acc, scanQueue );
					sym->setAccessed ( left->left->symbolValue (), true, acc, scanQueue, location );
				}
				leftType = left->left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				for ( auto it = left->pList ().param.begin (); it != left->pList ().param.end (); it++ )
				{
					// need to do this incase the parameters have embedded assignments
					(*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				}
				if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
				{
					if ( classDef && classDef->overload[int(fgxOvOp::ovArrayDerefLHS)] )
					{
						auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(fgxOvOp::ovArrayDerefLHS)]) - 1];
						if ( elem )
						{
							if ( interProcedural )
							{
								errorLocality e ( err, this );
								if ( isLS )
								{
									sym->setParameterTypeNoThrow ( comp, sym, elem->methodAccess.func->name, true, 2, rightType, acc, scanQueue );
								} else
								{
									sym->setParameterType ( comp, sym, elem->methodAccess.func->name, true, 2, rightType, acc, scanQueue );
								}
							}
							setCalled ( acc, elem->methodAccess.func, isLS, location );
							sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
							return elem->methodAccess.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
						}
					}
				} else if ( leftType.isAnObject () )
				{
					setOvOpCalled ( comp, left->getOp (), acc, isLS, location );
					markOvOp ( comp, left->getOp (), sym, acc, rightType, interProcedural, scanQueue, isLS, location );
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				}
				return symWeakVariantType;
			} else
			{
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				// right must be come computed value (by ref function return, array, etc.)
				// we might be able to do someting with this for arrays in the future but for now we don't
				return symWeakVariantType;
			}
			break;
		case astOp::range:
			if ( sym->isIterator ( sym->file->rangeValue, true ) )
			{
				// only need to do this the first time a range is encountered... this constructs the iterator
				if ( scheduleMakeIterator ( sym->file, sym->file->findFunc ( sym->file->rangeValue ), false, acc, scanQueue, isLS ) )
				{
					markClassMethodsInuse ( comp, acc, sym->findClass ( sym->file->queryableValue )->oClass, sym, scanQueue, true, isLS, location );
				}
			}
			if ( left && right )
			{
				astNode newNode ( sym->file->sCache, astOp::funcCall );
				newNode.setLocation ( this );
				newNode.left = (new astNode ( sym->file->sCache, astOp::atomValue, sym->file->rangeValue ))->setLocation ( this );
				newNode.pList ().param.push_back ( left );
				newNode.pList ().paramRegion.push_back ( *left );
				newNode.pList ().param.push_back ( right );
				newNode.pList ().paramRegion.push_back ( *right );

				left = nullptr;
				right = nullptr;
				release ();
				*this = std::move ( newNode );
				return typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			}
			return symWeakVariantType;
		case astOp::symbolValue:
			sym->setAccessed ( symbolValue (), true, acc, scanQueue, location );
			switch ( sym->getScope ( symbolValue (), true ) )
			{
				case symbolSpaceScope::symClassConst:
				case symbolSpaceScope::symClassStatic:
					if ( std::holds_alternative<opFunction *> ( acc ) )
					{
						sym->setAccessed ( sym->file->selfValue, true, acc, scanQueue, location );
					}
					break;
				case symbolSpaceScope::symClassAssign:
				case symbolSpaceScope::symClassAccess:
					if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure = false;
					sym->setAccessed( sym->file->selfValue, true, acc, scanQueue, location );
					break;
				case symbolSpaceScope::symClassMethod:
				case symbolSpaceScope::symClassInherit:
					sym->setAccessed ( sym->file->selfValue, true, acc, scanQueue, location );
					break;
				case symbolSpaceScope::symClassIVar:
					sym->setAccessed ( sym->file->selfValue, true, acc, scanQueue, location );
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					break;
				case symbolSpaceScope::symGlobal:
				case symbolSpaceScope::symLocalField:
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					break;
				case symbolSpaceScope::symFunction:
					setCalled ( acc, sym->file->functionList.find ( symbolValue () )->second, isLS, location );
					break;
				case symbolSpaceScope::symClass:
				case symbolSpaceScope::symLocalParam:
				case symbolSpaceScope::symPCall:
				case symbolSpaceScope::symLocalConst:
				case symbolSpaceScope::symScopeUnknown:
					//  TODO: SSA needs to be done here so we can get proper assignment/access information on branches/goto's... REALLY need the CFG!
				case symbolSpaceScope::symLocal:
				case symbolSpaceScope::symLocalStatic:
					break;
			}
			return sym->getType ( symbolValue (), true );
		case astOp::stringValue:
			return symWeakStringType;
		case astOp::intValue:
			return symWeakIntType;
		case astOp::doubleValue:
			return symWeakDoubleType;
		case astOp::symbolPack:
			return symWeakVariantType;
		case astOp::varrayValue:
			for ( auto it = arrayData ().nodes.begin (); it != arrayData ().nodes.end (); it++ )
			{
				(*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			}
			break;
		case astOp::arrayValue:
			for ( auto it = arrayData ().nodes.begin (); it != arrayData ().nodes.end (); it++ )
			{
				(*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			}
			if ( arrayData ().nodes.size () )
			{
				if ( arrayData ().nodes[0]->getOp () == astOp::pairValue )
				{
					markClassMethodsInuse ( comp, acc, sym->file->classList.find ( sym->file->aArrayValue )->second, sym, scanQueue, false, isLS, location );
					return symbolTypeClass ( symbolType::symWeakObject, sym->file->aArrayValue );
				}
				return symWeakArrayType;
			} else
			{
				return symWeakVArrayType;
			}
			break;
		case astOp::funcCall:
			if ( left->getOp () == astOp::atomValue )
			{
				if ( left->symbolValue () == sym->file->newValue )
				{
					sym->file->functionList.find( sym->file->newValue )->second->setAccessed ( acc, scanQueue, location );
					if ( pList ().param.size () )
					{
						if ( (pList ().param[0]->getOp () == astOp::nameValue) )
						{
							// we know the class name
							auto cl = sym->findClass ( pList ().param[0]->nameValue () );
							if ( cl )
							{
								// found the class
								// we MUST do this every time... even if it's already in use since each usage may have different parameter types
								if ( cl->newIndex )
								{
									// and it has a constructor
									auto destFunc = cl->elements[static_cast<size_t>(cl->newIndex) - 1].methodAccess.func;
									for ( auto it = pList ().param.begin () + 1; it != pList ().param.end (); it++ )
									{
										if ( interProcedural )
										{
											errorLocality e ( err, this );
											if ( (*it) )destFunc->setParamType ( comp, sym, (uint32_t) std::distance ( pList ().param.begin (), it ), (*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ), scanQueue );
										} else
										{
											if ( (*it) ) (*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
										}
									}
									setCalled ( acc, destFunc, isLS, location );
									destFunc->setAccessed ( acc, scanQueue, location );
									if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst &= sym->isConst( destFunc->name, false );
									if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure &= sym->isPure( destFunc->name, false );
								} else
								{
									for ( auto &it : pList ().param )
									{
										if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
									}
								}
								markClassInuse ( comp, acc, cl->oClass, sym, scanQueue, isLS, location );
								return symbolTypeClass ( symbolType::symWeakObject, cl->oClass->name );
							} else
							{
								for ( auto &it : pList ().param )
								{
									if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
								}
								if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
								if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
								if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->unknownClassInstantiation = true;		// not "unknownable", but unlinkable in current environmentx
								return symbolTypeClass ( symbolType::symWeakObject );
							}
						}
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
						// specific class being allocated can not be known at compile time... drag them all in
						for ( auto &cls : sym->file->classList )
						{
							if ( !cls.second->inUse )
							{
								markClassInuse ( comp, acc, cls.second, sym, scanQueue, isLS, location );
							}
						}
						for ( auto &it : pList ().param )
						{
							if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
						}
						return symWeakObjectType;
					} else
					{
						if ( !isLS )
						{
							errorLocality e ( err, this );
							throw errorNum::scMISSING_CLASSNAME;
						}
						return symWeakVariantType;
					}
				} else if ( left->symbolValue () == sym->file->exceptValue )
				{
					if ( pList ().param.empty() )
					{
						if ( !isLS )
						{
							errorLocality e ( err, this );
							throw errorNum::scINVALID_PARAMETER;
						}
						return symWeakVariantType;
					}
					// this effects global state since we may catch many functions up (if at all)
					for ( auto &it : pList ().param )
					{
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					sym->file->functionList.find( sym->file->exceptValue )->second->setAccessed ( acc, scanQueue, location );
					setCalled ( acc, sym->file->functionList.find ( sym->file->exceptValue )->second, isLS, location );
					return symWeakVariantType;
				} else if ( left->symbolValue () == sym->file->typeValue || left->symbolValue () == sym->file->typeXValue || left->symbolValue () == sym->file->typeOfValue )
				{
					// this effects global state since we may catch many functions up (if at all)
					if ( pList ().param.empty () )
					{
						if ( !isLS )
						{
							errorLocality e ( err, this );
							throw errorNum::scINVALID_PARAMETER;
						}
						return symWeakVariantType;
					}					
					for ( auto &it : pList ().param )
					{
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					if ( pList ().param[0]->getOp () == astOp::symbolValue )
					{
// why do we have this here?  It's obviously set to force the parameter to TYPE to a variant, but why do we need to do this?  What's wrong with our type inferencing that we need to force this?
// ideally we should see what type the variable is an inline the proper return and avoid the call altogethere.
// so the question is why???
//						sym->setType ( pList ().param[0]->symbolValue (), true, symWeakVariantType, acc, scanQueue );
					}
					setCalled ( acc, sym->file->functionList.find ( sym->file->typeValue )->second, isLS, location );
					sym->file->functionList.find( sym->file->typeValue )->second->setAccessed ( acc, scanQueue, location );
					return symWeakStringType;
				} else if ( left->symbolValue () == sym->file->paramValue )
				{
					// this effects global state since we may catch many functions up (if at all)
					if ( pList ().param.empty () )
					{
						if ( !isLS )
						{
							errorLocality e ( err, this );
							throw errorNum::scINVALID_PARAMETER;
						}
						return symWeakVariantType;
					}
					for ( auto &it : pList ().param )
					{
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPcountParam = true;
					setCalled ( acc, sym->file->functionList.find ( sym->file->paramValue )->second, isLS, location );
					return symWeakStringType;
				} else if ( left->symbolValue () == sym->file->pcountValue )
				{
					for ( auto &it : pList ().param )
					{
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPcountParam = true;
					setCalled ( acc, sym->file->functionList.find ( sym->file->pcountValue )->second, isLS, location );
					return symWeakStringType;
				} else if ( left->symbolValue () == sym->file->varCopyValue )
				{
					if ( pList ().param.empty () )
					{
						if ( !isLS )
						{
							errorLocality e ( err, this );
							throw errorNum::scINVALID_PARAMETER;
						}
						return symWeakVariantType;
					}
					// objects may have copy methods which we can't currently reason about
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					sym->file->functionList.find( sym->file->varCopyValue )->second->setAccessed ( acc, scanQueue, location );
					if ( pList ().param.size () )
					{
						for ( auto &it : pList ().param )
						{
							if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
						}
						return (*pList ().param.begin ())->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					setCalled ( acc, sym->file->functionList.find ( sym->file->varCopyValue )->second, isLS, location );
				} else if ( left->symbolValue () == sym->file->unpackValue )
				{
					if ( pList ().param.empty () )
					{
						if ( !isLS )
						{
							errorLocality e ( err, this );
							throw errorNum::scINVALID_PARAMETER;
						}
						return symWeakVariantType;
					}					// same as copy.. objects may have unpack methods
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->unknownClassInstantiation = true;
					// specific class being allocated can not be known at compile time... drag them all in
					for ( auto &cls : sym->file->classList )
					{
						if ( !cls.second->inUse )
						{
							markClassInuse ( comp, acc, cls.second, sym, scanQueue, isLS, location );
						}
					}
					for ( auto &it : pList ().param )
					{
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					sym->file->functionList.find( sym->file->unpackValue )->second->setAccessed ( acc, scanQueue, location );
					setCalled ( acc, sym->file->functionList.find ( sym->file->unpackValue )->second, isLS, location );
					return symWeakVariantType;
				} else
				{
					if ( sym->isDefined ( left->symbolValue(), true ) && sym->isFunc ( left->symbolValue (), true ) )
					{
						// standard function call
						if ( sym->isIterator ( left->symbolValue (), true ) )
						{
							if ( scheduleMakeIterator ( sym->file, sym->file->findFunc ( left->symbolValue () ), false, acc, scanQueue, isLS ) )
							{
								markClassMethodsInuse ( comp, acc, sym->findClass ( sym->file->queryableValue )->oClass, sym, scanQueue, true, isLS, location );
							}
						}
						if ( sym->isMethod ( left->symbolValue (), true ) ) sym->setAccessed ( sym->file->selfValue, true, acc, scanQueue, location );
						sym->setAccessed ( left->symbolValue (), true, acc, scanQueue, location );
						setCalled ( acc, sym->file->findFunc ( left->symbolValue () ), isLS, location );
						typeInferParams ( left->symbolValue (), this, sym->isMethod ( left->symbolValue (), true ) ? 1 : 0, comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );

						if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst &= sym->isConst ( left->symbolValue(), false );
						if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure &= sym->isPure( left->symbolValue(), false );
						return sym->getMarkFuncReturnType ( comp, left->symbolValue (), true, acc, scanQueue, isLS, location );
					} else
					{
						switch ( sym->getScope ( left->symbolValue (), true ) )
						{
							case symbolSpaceScope::symClass:
								// case astOp::where we use the name of the class to instantiate the class
								{
									// convert to a regular new call
									pList ().param.insert ( pList ().param.begin (), (new astNode ( sym->file->sCache, astOp::nameValue, left->symbolValue() ))->setLocation ( left )  );
									left = new astNode ( sym->file->sCache, astOp::atomValue, sym->file->newValue );
									right = 0;

									// now do type infrencing on it
									return this->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
								}
							default:
								// don't know what the hell we're calling... something not defined
								if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst = false;
								if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure = false;
								return symWeakVariantType;
						}
					}
				}
			} else if ( left->getOp () == astOp::sendMsg )
			{
				// method calls
				symbolTypeClass type = left->left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				if ( type == symUnknownType )
				{
					for ( auto &it : pList ().param )
					{
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					return symbolType::symUnknown;
				}
				if ( type.isAnObject () )
				{
					if ( type.hasClass () && (classDef = sym->findClass ( type.getClass () )) )
					{
						if ( classDef && left->right->getOp () == astOp::nameValue )
						{
							compClassElementSearch *elem = classDef->getElement ( left->right->nameValue () );

							if ( elem )
							{
								{
									std::lock_guard g1 ( elem->elem->accessorsAccess );
									elem->elem->accessors.insert ( {acc, location} );
								}

								symbolTypeClass retType = symUnknownType;

								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_method:
										// propagate calling params to method paramters
										for ( auto &func : elem->elem->data.method.virtOverrides )
										{
											if ( func->isIterator )
											{
												if ( scheduleMakeIterator ( sym->file, func, false, acc, scanQueue, isLS ) )
												{
													markClassMethodsInuse ( comp, acc, sym->findClass ( sym->file->queryableValue )->oClass, sym, scanQueue, true, isLS, location );
												}
											}
											if ( func->isStatic )
											{
												typeInferParams ( func->name, this, 0, comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
											} else if ( func->isExtension )
											{
												if ( interProcedural )
												{
													errorLocality e ( err, this );
													func->setParamType ( comp, sym, 0, left->left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ), scanQueue );
												}

												typeInferParams ( func->name, this, 1, comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
											} else
											{
												typeInferParams ( func->name, this, 1, comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
											}
											func->setAccessed ( acc, scanQueue, location );
											setCalled ( acc, func, isLS, location );

											// this code is here entirely to support native functions that return known object types.
											// normally we would be able to detect an instantiation of any class in BC code... however we don't have that ability for native code
											// and we don't simply want to drag every possibilty in.  so if we see something returning a known object type we need to mark it as in use
											retType &= func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );

											if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst &= func->isConst;
											if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure &= func->isPure;
										}
										return retType;
									case fgxClassElementType::fgxClassType_prop:
										if ( !isLS )
										{
											errorLocality e ( err, this );
											throw errorNum::scNOT_ALLOWED_WITH_PROP_METHODS;
										}
										return symWeakVariantType;
									case fgxClassElementType::fgxClassType_iVar:
									case fgxClassElementType::fgxClassType_static:
									case fgxClassElementType::fgxClassType_const:
										// indirect function call to unknown target
										leftType = elem->elem->symType;
										goto func_call_by_type;
									case fgxClassElementType::fgxClassType_inherit:
										if ( !isLS )
										{
											errorLocality e ( err, this );
											throw errorNum::scNOT_A_FUNCTION;
										}
										return symWeakVariantType;
									default:
										break;
								}
							}
							if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
							if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
							if ( classDef->defaultAccessIndex )
							{
								elem = &classDef->elements[static_cast<size_t>(classDef->defaultAccessIndex) - 1];

								auto func = elem->methodAccess.func;
								func->isForcedVariant = true;
								func->params.forceVariant ( 1 );
								sym->setAccessed ( elem->methodAccess.func->name, true, acc, scanQueue, location );
								setCalled ( acc, elem->methodAccess.func, isLS, location );

								leftType = func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
								goto func_call_by_type;
							}
							if ( !isLS )
							{
								errorLocality e ( err, this );
								err->throwFatalError ( isLS, errorNum::scUNKNOWN_METHOD );
							}
							// really an error state
							return symWeakVariantType;
						} else
						{
							if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
							if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
							for ( auto &it : pList ().param )
							{
								if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
							}
							// we know the class, but the method name is runtime computed.... for all methods of this class we need force params to variant 
							for ( auto &it : classDef->elements )
							{
								switch ( it.type )
								{
									case fgxClassElementType::fgxClassType_method:
										for ( auto &func : it.elem->data.method.virtOverrides )
										{
											func->params.forceVariant ( 1 );
										}
										break;
									default:
										break;
								}
							}
						}
					} else
					{
						// unknown object
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
						if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
						for ( auto &it : pList ().param )
						{
							if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
						}
						if ( interProcedural && (type.compType () != symbolType::symUnknown) )
						{
							if ( left->right->getOp () == astOp::nameValue )
							{
								if ( !isLS )
								{
								// don't have a class type, but we DO have a method name... for each of the methods in all classes that match that method name we need to force types to variant
									for ( auto &cls : sym->file->classList )
									{
										// only mark the class elements IF the class is in use... otherwise there's no reason to scan
										if ( cls.second->inUse )
										{
											auto elem = cls.second->cClass->getElement ( left->right->nameValue () );
											if ( elem && elem->scope == fgxClassElementScope::fgxClassScope_public )
											{
												switch ( elem->type )
												{
													case fgxClassElementType::fgxClassType_method:
														if ( !elem->methodAccess.func->isForcedVariant )
														{
															auto func = elem->methodAccess.func;
															func->isForcedVariant = true;
															func->params.forceVariant ( 1 );
															sym->setAccessed ( func->name, true, acc, scanQueue, location );
														}
														break;
													default:
														break;
												}
											}
										} else
										{
											std::lock_guard g1 ( cls.second->accessorAccess );
											cls.second->accessors.insert ( acc );
										}
									}
								}
								// handle the case where this is a pseudo-op call
								if ( sym->isDefined ( left->right->nameValue (), true ) && sym->isFunc ( left->right->nameValue(), true ) )
								{
									if ( sym->isIterator ( left->right->symbolValue (), true ) )
									{
										if ( scheduleMakeIterator ( sym->file, sym->file->findFunc ( left->right->symbolValue () ), false, acc, scanQueue, isLS ) )
										{
											markClassMethodsInuse ( comp, acc, sym->findClass ( sym->file->queryableValue )->oClass, sym, scanQueue, true, isLS, location );
										}
									}
									errorLocality e ( err, this );
									sym->setAccessed ( left->right->symbolValue (), true, acc, scanQueue, location );
									sym->setParameterTypeNoThrow ( comp, sym, left->right->symbolValue (), true, 0, type, acc, scanQueue );
									typeInferParams ( left->right->symbolValue (), this, 1, comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
								}
							} else
							{
								if ( !isLS )
								{
									// worst case astOp::of all... we don't now object and we don't know method so force all parameters of all methods to variant
									for ( auto &cls : sym->file->classList )
									{
										// same thing as above, only mark class's that are in use.
										if ( cls.second->inUse )
										{
											for ( auto &elem : cls.second->cClass->elements )
											{
												if ( elem.scope == fgxClassElementScope::fgxClassScope_public )
												{
													switch ( elem.type )
													{
														case fgxClassElementType::fgxClassType_method:
															if ( !elem.methodAccess.func->isForcedVariant )
															{
																auto func = elem.methodAccess.func;
																func->isForcedVariant = true;
																func->params.forceVariant ( 1 );
																sym->setAccessed ( func->name, true, acc, scanQueue, location );
															}
															break;
														default:
															break;
													}
												}
											}
										} else
										{
											std::lock_guard g1 ( cls.second->accessorAccess );
											cls.second->accessors.insert ( acc );
										}
									}
									// have to drag in any functions as well as this may be a pseudo-object call
									for ( auto &func : sym->file->functionList )
									{
										func.second->isForcedVariant = true;
										func.second->params.forceVariant ( 1 );
										sym->setAccessed ( func.second->name, true, acc, scanQueue, location );
									}
								}
							}
							return symWeakVariantType;
						}
						if ( type.compType () != symbolType::symUnknown )
						{
							return symWeakVariantType;
						}
						return symUnknownType;
					}
				} else if ( type.compType () != symbolType::symUnknown )
				{
					// pseudo object calls
					if ( left->right->getOp () == astOp::nameValue )
					{
						// don't do pseudo-call conversion yet... we can ONLY do that after makeknown has run and all types have settled.
						// however, what we NEED to do is to actually 
						if ( sym->isDefined ( left->right->nameValue (), true ) )
						{
							// standard function call
							if ( sym->isIterator ( left->right->symbolValue (), true ) )
							{
								if ( scheduleMakeIterator ( sym->file, sym->file->findFunc ( left->right->symbolValue () ), false, acc, scanQueue, isLS ) )
								{
									markClassMethodsInuse ( comp, acc, sym->findClass ( sym->file->queryableValue )->oClass, sym, scanQueue, true, isLS, location );
								}
							}
							sym->setAccessed ( left->right->symbolValue (), true, acc, scanQueue, location );
							if ( interProcedural )
							{
								errorLocality e ( err, this );
								if ( isLS )
								{
									sym->setParameterTypeNoThrow ( comp, sym, left->right->symbolValue (), true, 0, type, acc, scanQueue );
								} else
								{
									sym->setParameterType ( comp, sym, left->right->symbolValue (), true, 0, type, acc, scanQueue );
								}
							}
							typeInferParams ( left->right->symbolValue (), this, 1, comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
						} 
						return sym->getMarkFuncReturnType ( comp, left->right->symbolValue (), true, acc, scanQueue, isLS, location );
					}
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					for ( auto &it : pList ().param )
					{
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					if ( markUsed )
					{
						// function destination is unknowable (runtime computed) at compile time, set all parameters to variant on all non-methods
						for ( auto &it : sym->file->functionList )
						{
							if ( isFuncMarkable ( sym->file, it.second, acc ) )
							{
								if ( !it.second->isForcedVariant && !it.second->classDef )
								{
									it.second->params.forceVariant ( 1 );
									it.second->setAccessed ( acc, scanQueue, location );
									it.second->isForcedVariant = true;
									it.second->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );				// ensures we bring all return class types in
								}
							}
						}
					}
					return symWeakVariantType;
				}
			} else
			{
				leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
func_call_by_type:
				if ( leftType.isPotentiallyVariant () )
				{
					for ( auto &it : pList ().param )
					{
						it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
					}
					return symbolType::symUnknown;
				}
				if ( leftType.hasClass() && (classDef = sym->findClass( leftType.getClass() )) )
				{
					if ( classDef && classDef->overload[int(fgxOvOp::ovFuncCall)] )
					{
						auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(fgxOvOp::ovFuncCall)]) - 1];
						if ( elem )
						{
							typeInferParams( elem->methodAccess.func->name, this, 1, comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
							sym->setAccessed( elem->methodAccess.func->name, false, acc, scanQueue, location );
							setCalled ( acc, elem->methodAccess.func, isLS, location );
							if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst &= elem->methodAccess.func->isConst;
							if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure &= elem->methodAccess.func->isPure;
							return elem->methodAccess.func->getMarkReturnType( comp, sym, acc, scanQueue, isLS, location );
						}
					}
					if ( !isLS )
					{
						errorLocality e ( err, this );
						throw errorNum::scUNKNOWN_METHOD;
					}
					return symWeakVariantType;
				}
				if ( leftType.isAnObject () )
				{
					for ( auto &it : pList ().param )
					{
						it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
					}
					for ( auto &cls : sym->file->classList )
					{
						if ( cls.second->inUse )
						{
							if ( cls.second->cClass->overload[int(fgxOvOp::ovFuncCall)] )
							{
								auto elem = &cls.second->cClass->elements[static_cast<size_t>(cls.second->cClass->overload[int(fgxOvOp::ovFuncCall)]) - 1];
								if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isConst &= elem->methodAccess.func->isConst;
								if ( std::holds_alternative<opFunction *>( acc ) ) std::get<opFunction *>( acc )->isPure &= elem->methodAccess.func->isPure;
								if ( elem )
								{
									if ( !elem->methodAccess.func->isForcedVariant )
									{
										elem->methodAccess.func->params.forceVariant ( 1 );
									}
									elem->methodAccess.func->setAccessed ( acc, scanQueue, location );
									elem->methodAccess.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
								}
							}
						} else
						{
							if ( std::holds_alternative<opFunction *> ( acc ) )
							{
								std::lock_guard g1 ( cls.second->accessorAccess );
								cls.second->accessors.insert ( acc );
							}
						}
					}
				} else if ( leftType.compType () != symbolType::symUnknown )
				{
					// dynamic call so we can't tell what we're calling!  set all NON-METHOD function params to variant
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *> ( acc ) )
					{
						std::get<opFunction *> ( acc )->unknownFunctionCall = true;
					}
					for ( auto &it : pList ().param )
					{
						// we must do parameter inferencing even though it's not necessarily interprocedureal as the parameter evaluation may have side effects
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
					if ( markUsed )
					{
						for ( auto it = sym->file->functionList.begin (); it != sym->file->functionList.end (); it++ )
						{
							if ( isFuncMarkable ( sym->file, (*it).second, acc ) )
							{
								(*it).second->setAccessed ( acc, scanQueue, location );
								if ( !it->second->isForcedVariant && !it->second->classDef )
								{
									(*it).second->params.forceVariant ( 0 );
									(*it).second->isForcedVariant = true;
									if ( scheduleMakeIterator ( sym->file, (*it).second, true, acc, scanQueue, isLS ) )
									{
										markClassMethodsInuse ( comp, acc, sym->findClass ( sym->file->queryableValue )->oClass, sym, scanQueue, true, isLS, location );
									}
								}
								it->second->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
							}
						}
					}
				} else
				{
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					for ( auto &it : pList ().param )
					{
						// we must do parameter inferencing even though it's not necessarily interprocedureal as the parameter evaluation may have side effects
						if ( it ) it->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					}
				}
				return symUnknownType;
			}
			// if we haven't returned early it's because we haven't been able to isolate the particular class/method or function.   because of that we can only return variant
			// it *may* be possible to narrow down the possibilities (e.g. all <x>.print() return an int or someting... we could add it but is it worth the extra work?
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
			return symUnknownType;
		case astOp::getEnumerator:
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			switch ( leftType.compType () )
			{
				case symbolType::symArray:
					markClassInuse ( comp, acc, sym->file->classList.find( sym->file->fixedArrayEnumeratorValue )->second, sym, scanQueue, isLS, location );
					return symbolTypeClass ( symbolType::symWeakObject, sym->file->fixedArrayEnumeratorValue );
				case symbolType::symVArray:
					markClassInuse ( comp, acc, sym->file->classList.find( sym->file->sparseArrayEnumeratorValue )->second, sym, scanQueue, isLS, location );
					return symbolTypeClass ( symbolType::symWeakObject, sym->file->sparseArrayEnumeratorValue );
				case symbolType::symString:
					markClassInuse ( comp, acc, sym->file->classList.find( sym->file->stringEnumeratorValue )->second, sym, scanQueue, isLS, location );
					return symbolTypeClass ( symbolType::symWeakObject, sym->file->stringEnumeratorValue );
				default:
					break;
			}
			if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
			{
				auto elem = classDef->getElement ( sym->file->getEnumeratorValue );
				if ( elem )
				{
					astNode newNode ( sym->file->sCache, astOp::funcCall );
					newNode.setLocation ( this->left );
					newNode.left = (new astNode ( sym->file->sCache, astOp::sendMsg ))->setLocation ( this );
					newNode.left->left = left;
					newNode.left->right = (new astNode ( sym->file->sCache, astOp::nameValue, sym->file->getEnumeratorValue ))->setLocation ( this );

					left = nullptr;
					release ();
					*this = std::move ( newNode );

					auto retType = this->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
					return retType;
				}
			}
			if ( !(leftType == symUnknownType) )
			{
				// can't determine what we're enumerating... so mark the three enumerators called by the VM
				markClassInuse ( comp, acc, sym->file->classList.find ( sym->file->fixedArrayEnumeratorValue )->second, sym, scanQueue, isLS, location );
				markClassInuse ( comp, acc, sym->file->classList.find( sym->file->sparseArrayEnumeratorValue )->second, sym, scanQueue, isLS, location );
				markClassInuse ( comp, acc, sym->file->classList.find( sym->file->stringEnumeratorValue )->second, sym, scanQueue, isLS, location );

				if ( !isLS )
				{
					// don't have a class type, but we DO have a method name... for each of the methods in all classes that match that method name we need to force types to variant
					// it's unlikely that GetEnumerator will take any parameters... but do we wan't to restrict it?
					for ( auto &cls : sym->file->classList )
					{
						// only mark the class elements IF the class is in use... otherwise there's no reason to scan
						if ( cls.second->inUse )
						{
							auto elem = cls.second->cClass->getElement ( sym->file->getEnumeratorValue );
							if ( elem && elem->scope == fgxClassElementScope::fgxClassScope_public )
							{
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_method:
										for ( auto &func : elem->elem->data.method.virtOverrides )
										{
											if ( !func->isForcedVariant )
											{
												func->isForcedVariant = true;
												func->params.forceVariant ( 1 );
											}
											func->setAccessed ( acc, scanQueue, location );
											func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
										}
										break;
									default:
										break;
								}
							}
						} else
						{
							std::lock_guard g1 ( cls.second->accessorAccess );
							cls.second->accessors.insert ( acc );
						}
					}
				}
			}
			return symUnknownType;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			// no way of knowing for sure at this time...
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			// these aren't really "assignment's" per-se.  They are operations on already assigned values and therefore must
			// have already had a valid type assigned (that or they are being operated before being assigned)
			if ( left->getOp () == astOp::symbolValue )
			{
				sym->setType ( left->symbolValue (), true, symUnknownType, acc, scanQueue );
				sym->setAccessed ( left->symbolValue (), true, acc, scanQueue, location );
			}
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );

			for ( auto it = pList ().param.begin (); it != pList ().param.end (); it++ )
			{
				(*it)->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			}
			if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
			{
				if ( classDef && classDef->overload[int(fgxOvOp::ovArrayDerefRHS)] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(fgxOvOp::ovArrayDerefRHS)]) - 1];
					if ( elem )
					{
						elem->methodAccess.func->setAccessed ( acc, scanQueue, location );
						return elem->methodAccess.func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
					}
				}
			} else if ( leftType.isAnObject () )
			{
				markOvOp ( comp, this->getOp (), sym, acc, rightType, interProcedural, scanQueue, isLS, location );
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			}
			return symWeakVariantType;
		case astOp::codeBlockExpression:
			return symWeakCodeblockType;
		case astOp::codeBlockValue:
			vmCodeBlock *cb;
			fgxSymbols *symbols;

			cb = (vmCodeBlock *) bufferBuff ( codeBlock ().buff );

			symbols = (fgxSymbols *) ((char *) cb + cb->symOffset);

			for ( size_t loop = cb->nParams; loop < cb->nSymbols; loop++ )
			{
				if ( strccmp ( symbols[loop].offsetName + (char *) (cb), "self" ) )
				{
					sym->setType ( sym->file->sCache.get ( stringi ( (char *) (cb) +symbols[loop].offsetName ) ), true, symWeakVariantType, acc, scanQueue );
				}
			}
			// runtime compiled codeblocks are the ultimate "we don't know what the fuck it's going to do"... they will bind and capture any symbol
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->unknownFunctionCall = true;
			sym->setAllLocalAccessed ( acc, scanQueue, location );
			if ( markUsed )
			{
				for ( auto &it : sym->file->functionList )
				{
					if ( !it.second->isForcedVariant && !it.second->classDef && !it.second->isIterator )
					{
						it.second->setAccessed ( acc, scanQueue, location );
						it.second->params.forceVariant ( 0 );
						it.second->isForcedVariant = true;
					}
				}
			}
			return symWeakCodeblockType;
		case astOp::comp:
			left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->unknownFunctionCall = true;
			sym->setAllLocalAccessed ( acc, scanQueue, location );
			if ( markUsed )
			{
				for ( auto &it : sym->file->functionList )
				{
					if ( !it.second->isForcedVariant && !it.second->classDef && !it.second->isIterator )
					{
						it.second->setAccessed ( acc, scanQueue, location );
						it.second->params.forceVariant ( 0 );
						it.second->isForcedVariant = true;
					}
				}
			}
			return symWeakVariantType;
		case astOp::funcValue:
			assert ( false );
			break;
		case astOp::fieldValue:
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			return symWeakVariantType;
		case astOp::atomValue:
			sym->setAccessed ( symbolValue (), true, acc, scanQueue, location );
			return symWeakAtomType;
		case astOp::refPromote:
		case astOp::refCreate:
			if ( left )
			{
				if ( left->getOp () == astOp::symbolValue )
				{
					sym->setType ( left->symbolValue (), true, symWeakVariantType, acc, scanQueue );
				}
				left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			}
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			return symWeakVariantType;
		case astOp::cSend:
		case astOp::sendMsg:
			if ( !left || !right )
			{
				return symWeakVariantType;
			}
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			if ( leftType.isPotentiallyVariant () )
			{
				right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				return symUnknownType;
			}
			if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
			{
				if ( classDef )
				{
					if ( right->getOp () == astOp::nameValue )
					{
						compClassElementSearch *elem = classDef->getElement ( right->nameValue () );

						if ( elem )
						{
							{
								std::lock_guard g1 ( elem->elem->accessorsAccess );
								elem->elem->accessors.insert ( {acc, location} );
							}
							switch ( elem->type )
							{
								// METHODS and PROPERTIES are OK to use return types even without interprocedural analyis enabled.
								// with interprocedural analysis disabled, all input parameters will be variant.  a return type will
								// always be variant if they directly depend on a parameter type, otherwise they are invariant to the parameter type
								// and the actual return type is correct
								case fgxClassElementType::fgxClassType_method:
									{
										symbolTypeClass retType = symUnknownType;

										if ( elem->isVirtual )
										{
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
										}

										for ( auto &func : elem->elem->data.method.virtOverrides )
										{
											sym->setAccessed ( func->name, false, acc, scanQueue, location );
											retType &= func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
										}
										return retType;
									}
								case fgxClassElementType::fgxClassType_prop:
									{
										if ( elem->isVirtual )
										{
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
										}
										symbolTypeClass retType = symUnknownType;

										for ( auto &func : elem->elem->data.prop.accessVirtOverrides )
										{
											sym->setAccessed ( func->name, false, acc, scanQueue, location );
											retType &= func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
										}
										return retType;
									}
									break;
								case fgxClassElementType::fgxClassType_const:
									sym->setAccessed ( elem->symbolName, true, acc, scanQueue, location );
									if ( interProcedural )
									{
										return elem->elem->symType;
									} else
									{
										return symWeakVariantType;
									}
								case fgxClassElementType::fgxClassType_iVar:
									std::get<opFunction *> ( acc )->isPure = false;
									if ( interProcedural )
									{
										return elem->elem->symType;
									} else
									{
										return symWeakVariantType;
									}
								case fgxClassElementType::fgxClassType_static:
									sym->setAccessed ( elem->symbolName, true, acc, scanQueue, location );
									std::get<opFunction *> ( acc )->isPure = false;
									if ( interProcedural )
									{
										return elem->elem->symType;
									} else
									{
										return symWeakVariantType;
									}
								case fgxClassElementType::fgxClassType_inherit:
									{
										auto cls = sym->findClass ( elem->name );
										if ( cls )
										{
											return symbolTypeClass ( symbolType::symWeakObject, cls->oClass->name );
										}
										return symWeakVariantType;
									}
								default:
									if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
									if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
									break;
							}
						} else if ( classDef->defaultAccessIndex )
						{
							elem = &classDef->elements[static_cast<size_t>(classDef->defaultAccessIndex) - 1];

							if ( elem->isVirtual )
							{
								if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
								if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
							}
							symbolTypeClass retType = symUnknownType;

							for ( auto &func : elem->elem->data.prop.accessVirtOverrides )
							{
								sym->setAccessed ( func->name, false, acc, scanQueue, location );
								retType &= func->getMarkReturnType ( comp, sym, acc, scanQueue, isLS, location );
							}
							return retType;
						} else
						{
							if ( !isLS )
							{
								errorLocality e ( err, right );
								throw errorNum::scUNKNOWN_IVAR;
							}
							return symWeakVariantType;
						}
					} else
					{
						right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
						markClassMethodsInuse ( comp, acc, classDef->oClass, sym, scanQueue, true, isLS, location );
					}
				} else
				{
					// we have a class, but no definition
					if ( !isLS )
					{
						errorLocality e ( err, this );
						throw errorNum::scINTERNAL;
					}
					return symWeakVariantType;
				}
			}
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
			if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
			if ( right->getOp () == astOp::nameValue )
			{
				if ( leftType == symAtomType )
				{
					if ( left->getOp () == astOp::symbolValue )
					{
						auto cls = sym->findClass ( left->symbolValue () );
						if ( cls )
						{
							if ( right->getOp () == astOp::nameValue )
							{
								auto elem = cls->getElement ( right->nameValue () );

								if ( elem )
								{
									if ( !(elem->type != fgxClassElementType::fgxClassType_static && elem->type != fgxClassElementType::fgxClassType_const) )
									{
										// accessing a constant or static from outside the object (via the class)
										errorLocality e ( err, this );
										elem->elem->setAccessed ( sym->file, right->nameValue(), true, acc, scanQueue, location );
										return elem->elem->symType;
									}
								}
							}
						}
					}
				}
				if ( !isLS )
				{
					// don't have a class type, but we DO have a method name... for each of the methods in all classes that match that method name we need to force types to variant
					for ( auto &cls : sym->file->classList )
					{
						// NOTE: we never drag in classes... the only place that classes are marked as in-use is when we do a new...  if we don't create it we don't need to worry about it
						if ( cls.second->inUse )
						{
							auto elem = cls.second->cClass->getElement ( right->nameValue () );
							if ( elem && elem->scope == fgxClassElementScope::fgxClassScope_public )
							{
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_prop:
										if ( elem->methodAccess.func )
										{
											errorLocality e ( err, this );
											elem->methodAccess.func->setAccessed ( acc, scanQueue, location );
											elem->methodAccess.func->setParamTypeNoThrow ( comp, sym, 1, rightType, scanQueue );
										}
										break;
									case fgxClassElementType::fgxClassType_iVar:
										elem->elem->setTypeNoThrow ( false, rightType, acc, scanQueue );
										break;
									case fgxClassElementType::fgxClassType_static:
										elem->elem->symType ^= rightType;
										sym->setAccessed ( elem->symbolName, false, acc, scanQueue, location );
										break;
									default:
										break;
								}
							}
							if ( cls.second->cClass->defaultAccessIndex )
							{
								elem = &cls.second->cClass->elements[static_cast<size_t>(cls.second->cClass->defaultAccessIndex) - 1];

								if ( elem->isVirtual )
								{
									if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
									if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
								}
								for ( auto &func : elem->elem->data.prop.accessVirtOverrides )
								{
									sym->setAccessed ( func->name, false, acc, scanQueue, location );
								}
							}
						} else
						{
							std::lock_guard g1 ( cls.second->accessorAccess );
							cls.second->accessors.insert ( acc );
						}
					}
				}
			}  else
			{
				right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
#if 0
				// worst case... unknown class type and unknown ivar name
				for ( auto &cls : sym->file->classList )
				{
					// same thing as above, only mark class's that are in use.
					if ( cls.second->inUse )
					{
						for ( auto &elem : cls.second->cClass->elements )
						{
							if ( elem.scope == fgxClassElementScope::fgxClassScope_public )
							{
								switch ( elem.type )
								{
									// METHODS and PROPERTIES are OK to use return types even without interprocedural analyis enabled.
									// with interprocedural analysis disabled, all input parameters will be variant.  a return type will
									// always be variant if they directly depend on a parameter type, otherwise they are invariant to the parameter type
									// and the actual return type is correct
									case fgxClassElementType::fgxClassType_method:
										if ( elem.isVirtual )
										{
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
										}
										for ( auto &func : elem.elem->data.method.virtOverrides )
										{
											sym->setAccessed ( func->name, false, acc, scanQueue );
										}
										break;
									case fgxClassElementType::fgxClassType_prop:
										if ( elem.isVirtual )
										{
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
											if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
										}
										for ( auto &func : elem.elem->data.prop.accessVirtOverrides )
										{
											sym->setAccessed ( func->name, false, acc, scanQueue );
										}
										break;
									case fgxClassElementType::fgxClassType_const:
										sym->setAccessed ( elem.symbolName, true, acc, scanQueue );
										break;
									case fgxClassElementType::fgxClassType_iVar:
										std::get<opFunction *> ( acc )->isPure = false;
										break;
									case fgxClassElementType::fgxClassType_static:
										sym->setAccessed ( elem.symbolName, true, acc, scanQueue );
										std::get<opFunction *> ( acc )->isPure = false;
										break;
									case fgxClassElementType::fgxClassType_inherit:
										break;
									default:
										if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
										if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
										break;
								}
							}
						}
					} else
					{
						std::lock_guard g1 ( cls.second->accessorAccess );
						cls.second->accessors.insert ( acc );
					}
				}
#endif
			}
			if ( leftType.isAnObject () )
			{
				// we know it's an object but we're unable to resolve... we won't get any better so we need to return variant
				return symWeakVariantType;
			}
			return symUnknownType;
		case astOp::coalesce:
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			if ( (symbolTypeClass const &) leftType != right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ) )
			{
				return symWeakVariantType;
			} else
			{
				return leftType;
			}
			break;
		case astOp::conditional:
			left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			leftType = conditionData ().trueCond->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			if ( (symbolTypeClass const &) leftType != conditionData ().falseCond->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ) )
			{
				return symWeakVariantType;
			} else
			{
				return leftType;
			}
		case astOp::addAssign:
		case astOp::subAssign:
		case astOp::mulAssign:
		case astOp::powAssign:
			// these aren't really "assignment's" per-se.  They are operations on already assigned values and therefore must
			// have already had a valid type assigned (that or they are being operated before being assigned)
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			rightType = right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
			if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])]) - 1];
					if ( elem )
					{
						if ( interProcedural )
						{
							errorLocality e ( err, this );
							if ( isLS )
							{
								sym->setParameterTypeNoThrow ( comp, sym, elem->methodAccess.func->name, true, 1, rightType, acc, scanQueue );
							} else
							{
								sym->setParameterType ( comp, sym, elem->methodAccess.func->name, true, 1, rightType, acc, scanQueue );
							}
						}
						sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
						return sym->getMarkFuncReturnType ( comp, elem->methodAccess.func->name, true, acc, scanQueue, isLS, location );
					}
				}
			} else if ( leftType.isAnObject () )
			{
				if ( left->getOp () == astOp::symbolValue )
				{
					sym->setType ( left->symbolValue (), false, leftType, acc, scanQueue );
				}
				markOvOp ( comp, this->getOp (), sym, acc, rightType, interProcedural, scanQueue, isLS, location );
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				return symWeakVariantType;
			}
			if ( left->getOp () == astOp::symbolValue )
			{
				sym->setType ( left->symbolValue (), false, leftType, acc, scanQueue );
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
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
			rightType = right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
			if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])]) - 1];
					if ( elem )
					{
						if ( interProcedural )
						{
							errorLocality e ( err, this );
							if ( isLS )
							{
								sym->setParameterTypeNoThrow ( comp, sym, elem->methodAccess.func->name, true, 1, rightType, acc, scanQueue );
							} else
							{
								sym->setParameterType ( comp, sym, elem->methodAccess.func->name, true, 1, rightType, acc, scanQueue );
							}
						}
						sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
						return sym->getMarkFuncReturnType ( comp, elem->methodAccess.func->name, true, acc, scanQueue, isLS, location );
					}
				}
			} else if ( leftType.isAnObject () )
			{
				if ( left->getOp () == astOp::symbolValue )
				{
					sym->setType ( left->symbolValue (), false, leftType, acc, scanQueue );
				}
				markOvOp ( comp, this->getOp (), sym, acc, rightType, interProcedural, scanQueue, isLS, location );
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				return symWeakVariantType;
			}
			switch ( getOp () )
			{
				case astOp::modAssign:
				case astOp::bwAndAssign:
				case astOp::bwOrAssign:
				case astOp::bwXorAssign:
				case astOp::shLeftAssign:
				case astOp::shRightAssign:
					if ( left->getOp () == astOp::symbolValue )
					{
						sym->setType ( left->symbolValue (), false, symWeakIntType, acc, scanQueue );
					}
					break;
				default:
					break;
			}
			return symWeakIntType;
		case astOp::divAssign:
		case astOp::divide:
			// divide always results in a double
			// TODO: we can optimize this potentially based upon known constant division
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
			rightType = right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
			if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])]) - 1];
					if ( elem )
					{
						if ( interProcedural )
						{
							errorLocality e ( err, this );
							if ( isLS )
							{
								sym->setParameterTypeNoThrow ( comp, sym, elem->methodAccess.func->name, true, 1, right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ), acc, scanQueue );
							} else
							{
								sym->setParameterType ( comp, sym, elem->methodAccess.func->name, true, 1, right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ), acc, scanQueue );
							}
						}
						sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
						return sym->getMarkFuncReturnType ( comp, elem->methodAccess.func->name, true, acc, scanQueue, isLS, location );
					}
				}
			} else if ( leftType.isAnObject () )
			{
				if ( left->getOp () == astOp::symbolValue )
				{
					sym->setType ( left->symbolValue (), false, leftType, acc, scanQueue );
				}
				markOvOp ( comp, this->getOp (), sym, acc, rightType, interProcedural, scanQueue, isLS, location );
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				return symWeakVariantType;
			}
			if ( left->getOp () == astOp::symbolValue )
			{
				if ( getOp () == astOp::divAssign )
				{
					sym->setType ( left->symbolValue (), false, symWeakDoubleType, acc, scanQueue );
				}
			}
			return symWeakDoubleType;
		case astOp::postInc:
		case astOp::preInc:
		case astOp::postDec:
		case astOp::preDec:
			if ( left->getOp () == astOp::sendMsg )
			{
				astNode one ( sym->file->sCache, astOp::intValue, (int64_t) 1 );
				switch ( getOp () )
				{
					case astOp::postInc:
					case astOp::preInc:
						{
							astNode addNode ( sym->file->sCache, astOp::add, new astNode ( *left ), new astNode ( one ) );
							auto newLeft = new astNode ( *left );
							auto newRight = new astNode ( addNode );
							astNode assignNode ( sym->file->sCache, astOp::assign, newLeft, newRight );
							assignNode.right->forceLocation ( srcLocation () );
							if ( isLS ) sym->file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOperator, *this) );
							assignNode.typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
							std::swap ( assignNode, *this );
						}
						return leftType;
					case astOp::postDec:
					case astOp::preDec:
						{
							astNode subNode ( sym->file->sCache, astOp::subtract, new astNode ( *left ), new astNode ( one ));
							auto newLeft = new astNode ( *left );
							auto newRight = new astNode ( subNode );
							astNode assignNode ( sym->file->sCache, astOp::assign, newLeft, newRight );
							assignNode.right->forceLocation ( srcLocation () );
							if ( isLS ) sym->file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOperator, *this ) );
							assignNode.typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
							std::swap ( assignNode, *this );
						}
						return leftType;
					default:
						errorLocality e ( err, this );
						throw errorNum::scINTERNAL;
				}
			} else
			{
				leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
				if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
				{
					if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])] )
					{
						auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])]) - 1];
						if ( elem )
						{
							sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
							return sym->getMarkFuncReturnType ( comp, elem->methodAccess.func->name, true, acc, scanQueue, isLS, location );
						}
					}
				} else if ( leftType.isAnObject () )
				{
					if ( left->getOp () == astOp::symbolValue )
					{
						sym->setType ( left->symbolValue (), false, leftType, acc, scanQueue );
					}
					markOvOp ( comp, this->getOp (), sym, acc, symUnknownType, interProcedural, scanQueue, isLS, location );
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
					return symWeakVariantType;
				}
			}
			if ( left->getOp () == astOp::symbolValue )
			{
				sym->setType ( left->symbolValue (), false, leftType, acc, scanQueue );
			}
			return leftType;
		case astOp::twosComplement:
			leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS );
			if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
			{
				if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])] )
				{
					auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])]) - 1];
					if ( elem )
					{
						sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
						return sym->getMarkFuncReturnType ( comp, elem->methodAccess.func->name, true, acc, scanQueue, isLS, location );
					}
				}
			} else if ( leftType.isAnObject () )
			{
				markOvOp ( comp, this->getOp (), sym, acc, symUnknownType, interProcedural, scanQueue, isLS, location );
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
				if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				return symWeakVariantType;
			}
			return symWeakIntType;
		case astOp::workAreaStart:
			left->typeInfer(comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS);
			return right->typeInfer(comp, acc, err, sym, inlineRetType, interProcedural, needValue, markUsed, scanQueue, isLS);
			break;
		default:
			// all other operands take their value from the left side
			if ( left )
			{
				leftType = left->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
				if ( leftType.hasClass () && (classDef = sym->findClass ( leftType.getClass () )) )
				{
					if ( classDef && classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])] )
					{
						auto elem = &classDef->elements[static_cast<size_t>(classDef->overload[int(opToOvXref[static_cast<size_t>(this->getOp ())])]) - 1];
						if ( elem )
						{
							if ( interProcedural && right )
							{
								errorLocality e ( err, this );
								if ( isLS )
								{
									sym->setParameterTypeNoThrow ( comp, sym, elem->methodAccess.func->name, true, 1, right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ), acc, scanQueue );
								} else
								{
									sym->setParameterType ( comp, sym, elem->methodAccess.func->name, true, 1, right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS ), acc, scanQueue );
								}
							}
							sym->setAccessed ( elem->methodAccess.func->name, false, acc, scanQueue, location );
							return sym->getMarkFuncReturnType ( comp, elem->methodAccess.func->name, true, acc, scanQueue, isLS, location );
						}
					}
				} else if ( leftType.isAnObject () )
				{
					markOvOp ( comp, this->getOp (), sym, acc, rightType, interProcedural, scanQueue, isLS, location );
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isConst = false;
					if ( std::holds_alternative<opFunction *> ( acc ) ) std::get<opFunction *> ( acc )->isPure = false;
				}
				switch ( leftType )
				{
					case symbolType::symInt:
					case symbolType::symWeakInt:
						if ( right )
						{
							rightType = right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
							switch ( rightType )
							{
								case symbolType::symDouble:
								case symbolType::symWeakDouble:
									return symWeakDoubleType;
								default:
									return leftType;
							}
						}
						break;
					default:
						if ( right )
						{
							right->typeInfer ( comp, acc, err, sym, inlineRetType, interProcedural, true, markUsed, scanQueue, isLS );
						}
						return leftType;
				}
			}
			if ( right )
			{
				// if we get here then we have just a right path... this should never occur and is an error
				errorLocality e ( err, this );
				throw errorNum::scINTERNAL;
			}
			break;
	}
	// should never get here
	return symUnknownType;
}

void astNode::markInUse ( class compExecutable *comp, accessorType const &acc, errorHandler *err, symbolStack *sym, bool interProcedural, unique_queue<accessorType> *scanQueue, bool isLS )
{
	if ( !(ptrdiff_t) this ) return;

	switch ( op )
	{
		case astOp::symbolDef:
			switch ( symbolDef ()->symType )
			{
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass *>(symbolDef ());
						markClassInuse ( comp, acc, cls->getClass ()->oClass, sym, scanQueue, isLS, location );
						sym->insert ( symbolDef (), cls->insertPos );
					}
					break;
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					{
						auto init = symbolDef ()->getInitializer ( symbolDef ()->getSymbolName () );
						if ( init->getOp() == astOp::assign )
						{
							init->right->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
						}
						sym->push ( symbolDef () );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef ());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							auto s = (*symbol)[it];
							if ( s->initializer->getOp () == astOp::assign )
							{
								s->initializer->right->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
							}
						}
						sym->push ( symbolDef () );
					}
					break;
				default:
					sym->push ( symbolDef () );
					break;
			}
			break;
		case astOp::btBasic:
			{
				// save and restore our symbol table as we traverse
				auto size = sym->size ();
				for ( auto it = basicData ().blocks.begin (); it != basicData ().blocks.end (); it++ )
				{
					(*it)->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
				}
				sym->resize ( size );
			}
			break;
		case astOp::btSwitch:
			switchData ().condition->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			for ( auto it = switchData ().caseList.begin (); it != switchData ().caseList.end (); it++ )
			{
				(*it)->block->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
				(*it)->condition->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData ().ifList.begin (); it != ifData ().ifList.end (); it++ )
			{
				(*it)->block->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
				(*it)->condition->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			ifData ().elseBlock->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		case astOp::btInline:
			sym->file->functionList.find( inlineFunc ().funcName )->second->setInlineUsed ( acc, scanQueue, location );
			inlineFunc ().node->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		case astOp::btFor:
			loopData ().initializers->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			loopData ().condition->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			loopData ().increase->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			loopData ().block->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		case astOp::btForEach:
			forEachData ().var->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			forEachData ().in->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			forEachData ().statement->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			loopData ().block->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			loopData ().condition->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		case astOp::btTry:
			tryCatchData ().catchBlock->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			tryCatchData ().errSymbol->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			tryCatchData ().finallyBlock->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			tryCatchData ().tryBlock->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		case astOp::btReturn:
			if ( !std::holds_alternative<opFunction *> ( acc ) )
			{
				errorLocality e ( err, this );
				throw errorNum::scINTERNAL;
			}
			if ( returnData ().node )
			{
				returnData ().node->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			break;
		case astOp::btInlineRet:
			if ( returnData ().node )
			{
				returnData ().node->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			break;
		case astOp::varrayValue:
			for ( auto it = arrayData ().nodes.begin (); it != arrayData ().nodes.end (); it++ )
			{
				(*it)->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			break;
		case astOp::arrayValue:
			for ( auto it = arrayData ().nodes.begin (); it != arrayData ().nodes.end (); it++ )
			{
				(*it)->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			break;
		case astOp::funcCall:
			for ( auto it = pList ().param.begin (); it != pList ().param.end (); it++ )
			{
				if ( (*it) ) (*it)->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			if ( left->getOp () == astOp::atomValue )
			{
				if ( left->symbolValue () == sym->file->newValue )
				{
					if ( pList ().param.size () )
					{
						if ( pList ().param[0]->getOp () == astOp::nameValue )
						{
							// we know the class name
							auto cl = sym->findClass ( pList ().param[0]->nameValue () );
							if ( cl )
							{
								markClassMethodsInuse ( comp, acc, cl->oClass, sym, scanQueue, false, isLS, location );
							}
							break;
						}
						// specific class being allocated can not be known at compile time... drag them all in
						for ( auto &cls : sym->file->classList )
						{
							if ( !cls.second->inUse )
							{
								cls.second->inUse = true;
								for ( auto &it : cls.second->cClass->elements )
								{
									switch ( it.type )
									{
										case fgxClassElementType::fgxClassType_inherit:
											// need not wory about inherit here... we're looping over all class's so any possible inherited classes will be dragged in.
											// don't worry about inherit errors here, that'll be detected during makeClass
											break;
										case fgxClassElementType::fgxClassType_method:
											for ( auto &func : it.elem->data.method.virtOverrides )
											{
												func->setAccessed ( acc, scanQueue, location );
											}
											break;
										case fgxClassElementType::fgxClassType_prop:
											for ( auto &func : it.elem->data.prop.assignVirtOverrides )
											{
												func->setAccessed ( acc, scanQueue, location );
											}
											for ( auto &func : it.elem->data.prop.accessVirtOverrides )
											{
												func->setAccessed ( acc, scanQueue, location );
											}
											break;
										default:
											break;
									}
								}
							}
						}
						break;
					}
				} else if ( left->symbolValue () == sym->file->unpackValue )
				{
						// specific class being allocated can not be known at compile time... drag them all in
					for ( auto &cls : sym->file->classList )
					{
						if ( !cls.second->inUse )
						{
							cls.second->inUse = true;
							for ( auto &it : cls.second->cClass->elements )
							{
								switch ( it.type )
								{
									case fgxClassElementType::fgxClassType_inherit:
										// need not wory about inherit here... we're looping over all class's so any possible inherited classes will be dragged in.
										// don't worry about inherit errors here, that'll be detected during makeClass
										break;
									case fgxClassElementType::fgxClassType_method:
										it.methodAccess.func->setAccessed ( acc, scanQueue, location );
										break;
									case fgxClassElementType::fgxClassType_prop:
										if ( it.methodAccess.func ) it.methodAccess.func->setAccessed ( acc, scanQueue, location );
										if ( it.assign.func ) it.assign.func->setAccessed ( acc, scanQueue, location );
										break;
									default:
										break;
								}
							}
						}
					}
				} else
				{
					switch ( sym->getScope ( left->symbolValue (), true ) )
					{
						// case astOp::where we use the name of the class to instantiate the class
						case symbolSpaceScope::symClass:
							{
								// we know the class name
								auto cl = sym->findClass ( left->symbolValue () );
								if ( cl )
								{
									markClassInuse ( comp, acc, cl->oClass, sym, scanQueue, isLS, location );
								}
							}
							break;
						default:
							sym->setAccessed ( left->symbolValue (), true, acc, scanQueue, location );
							for ( auto it = pList ().param.begin (); it != pList ().param.end (); it++ )
							{
								if ( (*it) ) (*it)->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
							}
							break;
					}
				}
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto it = pList ().param.begin (); it != pList ().param.end (); it++ )
			{
				(*it)->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			}
			left->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		case astOp::funcValue:
			{
				auto f = sym->file->functionList.find( symbolValue () )->second;
				f->setAccessed ( f, scanQueue, location );
				if ( f->classDef )
				{
					// need to do this just in case astOp::we're reference a static class function
					auto cls = sym->findClass ( f->classDef->name );
					if ( cls )
					{
						markClassInuse ( comp, acc, cls->oClass, sym, scanQueue, isLS, location );
					}
				}
			}
			break;
		case astOp::conditional:
			conditionData ().trueCond->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			conditionData ().falseCond->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
			break;
		default:
			break;
	}
	left->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
	right->markInUse ( comp, acc, err, sym, interProcedural, scanQueue, isLS );
}
