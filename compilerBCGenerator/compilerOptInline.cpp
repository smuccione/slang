
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "compilerOptimizer.h"
#include "Utility/counter.h"

// TODO: add support for operator overload inlining.  partial supported for ovFuncCall support (necessary for lambda inlining which is by far the most important)

// TODO: replace this with a "fix naming conflict" iterator.  should be possible to find the fully qualified name of the child atom and replace
static astNode *hasNamingConflictCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, symbolStack *symParent, bool &hasNamingConflict )
{
	switch ( node->getOp () )
	{
		case astOp::atomValue:
			if ( symParent->isDefined ( node->symbolValue(), true ) || symParent->isDefined ( node->symbolValue(), false ) )
			{
				if ( !symParent->isFunc ( node->symbolValue(), true ) )
				{
					hasNamingConflict = true;
				}
			}
			break;
		default:
			break;
	}
	return node;
}

static bool hasNamingConflict ( symbolStack *parentSym, astNode *node )
{
	symbolStack	tmpSym ( parentSym->file );
	bool res = false;
	astNodeWalk ( node, &tmpSym, hasNamingConflictCB, parentSym, res );
	return res;
}

// this returns if the inlined function has a dependency on a load image... we can not do cross-load image includes currently as they require BSS to be correct
static astNode *hasInlineBlockerCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, bool &blockerPresent )
{
	switch ( node->getOp () )
	{
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
					blockerPresent = true;
					break;
				default:
					break;
			}
			break;
		case astOp::symbolValue:
			switch ( sym->getScope ( node->symbolValue(), isAccess ) )
			{
				case symbolSpaceScope::symLocalStatic:
				case symbolSpaceScope::symGlobal:
				case symbolSpaceScope::symClassConst:
				case symbolSpaceScope::symClassStatic:
					blockerPresent = true;
					break;
				default:
					break;
			}
			break;
		case astOp::atomValue:
			if ( node->symbolValue () == sym->file->pcountValue || node->symbolValue () == sym->file->paramValue )
			{
				blockerPresent = true;
			}
			break;
		default:
			break;
	}
	return node;
}

static bool hasInlineBlocker ( symbolStack *sym, opFunction *func )
{
	if ( func->inlineBlockerPresent ) return true;

	if ( func->classDef )
	{
		for ( uint32_t it = 1; it < func->params.size (); it++ )
		{
			auto param = (func->params[it]);
			if ( param->getType ().isStronglyTyped () )
			{
				func->inlineBlockerPresent = true;
				return func->inlineBlockerPresent;
			}
		}
	} else
	{
		for ( uint32_t it = 0; it < func->params.size (); it++ )
		{
			auto param = (func->params[it]);
			if ( param->getType ().isStronglyTyped () )
			{
				func->inlineBlockerPresent = true;
				return func->inlineBlockerPresent;
			}
		}
	}

	astNodeWalk ( func->codeBlock, sym, hasInlineBlockerCB, func->inlineBlockerPresent );
	return func->inlineBlockerPresent;
}

static astNode *hasClosureCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, bool &hasClosure )
{
	switch ( node->getOp () )
	{
		case astOp::comp:
			hasClosure = true;
			break;
		default:
			break;
	}
	return node;
}

static bool hasClosure ( symbolStack *sym, astNode *node )
{
	bool res = false;
	astNodeWalk ( node, sym, hasClosureCB, res );
	return res;
}

static astNode *hasSymbolCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, bool &hasSymbol, cacheString &symbolName )
{
	switch ( node->getOp () )
	{
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( node->symbolDef()->getSymbolName () == symbolName )
					{
						hasSymbol = true;
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(node->symbolDef());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							if ( !strccmp ( (*symbol)[it]->getName (), symbolName ) )
							{
								hasSymbol = true;
								break;
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	return node;
}

static bool hasSymbol ( symbolStack *sym, astNode *node, cacheString &symbol )
{
	bool res = false;
	astNodeWalk ( node, sym, hasSymbolCB, res, symbol );
	return res;
}

static astNode *stripRenameCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool )
{
	switch ( node->getOp () )
	{
		case astOp::symbolValue:
			if ( !memcmp ( node->symbolValue().c_str (), "$$", 2 ) )
			{
				node->symbolValue () = sym->file->sCache.get ( node->symbolValue ().c_str () + 2 );
			}
			break;
		default:
			break;
	}
	return node;
}

void optimizer::funcInlineWeight ( opFunction *func )
{
	symbolStack sym ( file, func );

	if ( func->codeBlock )
	{
		func->weight = func->codeBlock->complexity ();
	}
}

static void paramToSymLocal ( opFunction *func, astNode *callingNode, astNode *funcNode, symbolSpaceParams *params, symbolStack *sym, astNode *object, bool isExtension )
{
	astNode *newNode;
	uint32_t numHidden = 0;

	if ( func->params.size () )
	{
		if ( func->params[0]->name == sym->file->selfValue )
		{
			numHidden = 1;
			if ( object )
			{
				if ( object->getOp () != astOp::assign )
				{
					object = (new astNode ( sym->file->sCache, astOp::assign,
											(new astNode ( sym->file->sCache, astOp::symbolValue, func->params[0]->name ))->setLocation ( func->params[0]->location ),
											object
					));
				}
				auto typeObject = object->right;
				switch ( object->getOp () )
				{
					case astOp::symbolValue:
						if ( strccmp ( object->symbolValue(), sym->file->selfValue ) || isExtension )
						{
							if ( func->params[0]->isAccessed () || func->params[0]->isAssigned () || func->params[0]->isClosedOver () || !typeObject->hasNoSideEffects ( sym, false ) )
							{
								object->setLocation ( func->params[0]->location );
								params->add ( func->params[0]->name, symbolTypeClass ( typeObject->getType ( sym ).getClass () ), func->params[0]->location, object, false, stringi () );
							} else
							{
								delete object;
							}
						} else
						{
							// if we get here we're calling a method in the same class... both are accessing the same self so we need no do anything 
							delete object;
						}
						break;
					default:
						{
							object->setLocation ( func->params[0]->location );
							params->add ( func->params[0]->name, symbolTypeClass ( typeObject->getType ( sym ).getClass () ), func->params[0]->location, object, false, stringi () );
						}
						break;
				}
			}
		} else if ( isExtension )
		{
			numHidden = 1;
			if ( object )
			{
				if ( object->getOp () != astOp::assign )
				{
					object = (new astNode ( sym->file->sCache, astOp::assign,
											(new astNode ( sym->file->sCache, astOp::symbolValue, func->params[0]->name ))->setLocation ( func->params[0]->location ),
											object
					));
				}
				auto typeObject = object->right;
				switch ( object->getOp () )
				{
					case astOp::symbolValue:
						if ( func->params[0]->isAccessed () || func->params[0]->isAssigned () || func->params[0]->isClosedOver () || !object->hasNoSideEffects ( sym, false ) )
						{
							object ->setLocation ( func->params[0]->location );
							params->add ( func->params[0]->name, symbolTypeClass ( typeObject->getType ( sym ).getClass () ), func->params[0]->location, object, false, stringi () );
						} else
						{
							delete object;
						}
						break;
					default:
						{
							auto newSymbol = sym->file->sCache.get ( (stringi) func->params[0]->name + "$" + GetUniqueCounter () );
							funcNode->renameLocal ( func->params[0]->name, newSymbol );
							object->setLocation ( func->params[0]->location );
							params->add ( newSymbol, symbolTypeClass ( typeObject->getType ( sym ).getClass () ), func->params[0]->location, object, false, stringi () );
						}
						break;
				}
			}
		} else
		{
			if ( object )
			{
				// object does not have self as first parameter
				delete object;
				assert ( 0 );
			}
		}
	} else
	{
		if ( object )
		{
			// object does not have self as first parameter
			delete object;
			assert ( 0 );
		}
	}

	/*
		two cases:

		method:
			func ( self, a, b )
			caller ( a, b )

			param :  2..1
			passed:  1..0

		function
			func ( a, b )
			caller ( a, b )

			param:	 1..0
			passed:  1.. 0
	*/

	for ( uint32_t index = (uint32_t) func->params.size (); index > numHidden; index-- )
	{
		uint32_t param = index - 1;
		uint32_t passed = param - numHidden;

		if ( callingNode && (passed < callingNode->pList().param.size ()) && callingNode->pList().param[passed] )
		{
			// passed a value into this position
			switch ( callingNode->pList().param[passed]->getOp () )
			{
				case astOp::refCreate:
					if ( (callingNode->pList().param[passed]->left->getOp () != astOp::symbolValue) || hasSymbol ( sym, funcNode, callingNode->pList().param[passed]->left->symbolValue() ) )  // can't rename if inlined function already has a symbol of the same name
					{
						auto newSymbol = sym->file->sCache.get ( (stringi) func->params[param]->name + "$" + GetUniqueCounter () );
						funcNode->renameLocal ( func->params[param]->name, newSymbol );

						astNode *init = new astNode ( sym->file->sCache, astOp::assign,
													  new astNode ( sym->file->sCache, astOp::symbolValue, newSymbol ),
													  new astNode ( *callingNode->pList ().param[passed] )
						);

						params->add ( newSymbol, symUnknownType, callingNode->location, init, false, stringi () );
					} else
					{
						funcNode->renameLocal ( func->params[param]->name, callingNode->pList().param[passed]->left->symbolValue() );
					}
					break;
				case astOp::symbolValue:
					if ( func->params[param]->isAssigned () || 
						 hasSymbol ( sym, funcNode, callingNode->pList().param[passed]->symbolValue() ) || 
						 (sym->getScope ( callingNode->pList().param[passed]->symbolValue(), true ) != symbolSpaceScope::symLocal && sym->getScope ( callingNode->pList().param[passed]->symbolValue(), true ) != symbolSpaceScope::symLocalParam) )// can't rename if inlined function already has a symbol of the same name
					{
						auto newSymbol = sym->file->sCache.get ( (stringi) func->params[param]->name + "$" + GetUniqueCounter () );
						funcNode->renameLocal ( func->params[param]->name, newSymbol );

						astNode *init = new astNode ( sym->file->sCache, astOp::assign,
													  new astNode ( sym->file->sCache, astOp::symbolValue, newSymbol ),
													  new astNode ( *callingNode->pList ().param[passed] )
						);
						params->add ( newSymbol, symUnknownType, callingNode->pList().param[passed]->location, init, false, stringi () );
					} else
					{
						// we never change the variable, so in this instance we can simply rename the variable to be the caller's variable name
						funcNode->renameLocal ( func->params[param]->name, callingNode->pList().param[passed]->symbolValue() );
					}
					break;
				case astOp::intValue:
				case astOp::doubleValue:
				case astOp::stringValue:
				case astOp::atomValue:
				case astOp::nullValue:
				case astOp::funcValue:
					// if it's an access only symbol then we can simply replace it with the constant
					if ( !func->params[param]->isAssigned () )
					{
						funcNode->propagateValue ( func->params[param]->name, callingNode->pList().param[passed] );
						break;
					}
					[[fallthrough]];
				default:
					if ( !func->params[param]->isAccessed () )
					{
						if ( callingNode->pList().param[passed]->hasNoSideEffects ( sym, false ) )
						{
							break;
						}
					}
					{
						auto newSymbol = sym->file->sCache.get ( (stringi) func->params[param]->name + "$" + GetUniqueCounter () );
						funcNode->renameLocal ( func->params[param]->name, newSymbol );

						astNode *init = new astNode ( sym->file->sCache, astOp::assign,
										  new astNode ( sym->file->sCache, astOp::symbolValue, newSymbol ),
													  new astNode ( *callingNode->pList ().param[passed] )
										  );

						params->add ( newSymbol, symUnknownType, callingNode->pList().param[passed]->location, init, false, stringi () );
					}
					break;
			}
		} else if ( param < (uint32_t) func->params.size () )
		{
			// no parameter passed see if it has a default otherwise set an appropriate initializer
			auto newSymbol = sym->file->sCache.get ( (stringi) func->params[param]->name + "$" + GetUniqueCounter () );
			newNode = func->params[param]->initializer;
			if ( newNode )
			{
				funcNode->renameLocal ( func->params[param]->name, newSymbol );
				newNode->renameLocal ( func->params[param]->name, newSymbol );
				params->add ( newSymbol, symUnknownType, func->params[param]->location, new astNode ( *newNode ), false, stringi () );
			} else
			{
				// no default
				funcNode->renameLocal ( func->params[param]->name, newSymbol );
				params->add ( newSymbol, symUnknownType, func->params[param]->location, nullptr, false, stringi () );
			}

		}
	}
	astNodeWalk ( funcNode, sym, stripRenameCB );
}

static bool inlineBlock ( compExecutable *comp, astNode *block, symbolStack *sym, opFunction *func, std::vector<opFunction *> &inlineNest, bool needValue )
{
	opFunction *childFunc;
	uint32_t				 nNest;
	compClassElementSearch *elem;
	bool					 refParam = false;
	symbolTypeClass			 leftType;

	if ( !block )
	{
		return false;
	}

	switch ( block->getOp () )
	{
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass *>(block->symbolDef());
						sym->insert ( block->symbolDef(), cls->insertPos );
					}
					break;
				case symbolSpaceType::symTypeLocal:
					{
						auto init = block->symbolDef ()->getInitializer ( block->symbolDef ()->getSymbolName () );

						if ( init->getOp () == astOp::assign )
						{
							inlineBlock ( comp, init->right, sym, func, inlineNest, true );
						}
					}
					sym->push ( block->symbolDef() );
					break;
				case symbolSpaceType::symTypeStatic:
					// DO NOT INLINE!!!
				default:
					sym->push ( block->symbolDef() );
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym->size ();
				for ( auto it = block->basicData().blocks.begin (); it != block->basicData().blocks.end (); it++ )
				{
					inlineBlock ( comp, (*it), sym, func, inlineNest, false );
				}
				sym->resize ( size );
			}
			break;
		case astOp::btInline:
			inlineNest.push_back ( sym->file->functionList.find ( block->inlineFunc().funcName )->second );
			inlineBlock ( comp, block->inlineFunc().node, sym, func, inlineNest, true );
			inlineNest.pop_back ();
			break;
		case astOp::btLabel:
		case astOp::btGoto:
		case astOp::btBreak:
		case astOp::btCont:
			break;
		case astOp::btInlineRet:
			inlineBlock ( comp, block->returnData ().node, sym, func, inlineNest, true );
			break;
		case astOp::btReturn:
			if ( block->returnData().node && (block->returnData().node->getOp () == astOp::funcCall) && (block->returnData().node->left->getOp () == astOp::atomValue) && (block->returnData().node->left->symbolValue() == func->name) )
			{
				// do not inline tail-recursion
			} else
			{
				inlineBlock ( comp, block->returnData().node, sym, func, inlineNest, true );
			}
			break;
		case astOp::btTry:
			inlineBlock ( comp, block->tryCatchData().tryBlock, sym, func, inlineNest, true );
			inlineBlock ( comp, block->tryCatchData().finallyBlock, sym, func, inlineNest, true );
			inlineBlock ( comp, block->tryCatchData().catchBlock, sym, func, inlineNest, true );
			break;
		case astOp::btFor:
			inlineBlock ( comp, block->loopData().condition, sym, func, inlineNest, true );
			inlineBlock ( comp, block->loopData().increase, sym, func, inlineNest, true );
			inlineBlock ( comp, block->loopData().block, sym, func, inlineNest, true );
			inlineBlock ( comp, block->loopData().initializers, sym, func, inlineNest, true );
			break;
		case astOp::btForEach:
			inlineBlock ( comp, block->forEachData().var, sym, func, inlineNest, true );
			inlineBlock ( comp, block->forEachData().in, sym, func, inlineNest, true );
			inlineBlock ( comp, block->forEachData().statement, sym, func, inlineNest, true );
			break;
		case astOp::btIf:
			inlineBlock ( comp, block->ifData().elseBlock, sym, func, inlineNest, true );
			for ( auto it = block->ifData().ifList.begin (); it != block->ifData().ifList.end (); it++ )
			{
				inlineBlock ( comp, (*it)->condition, sym, func, inlineNest, true );
				inlineBlock ( comp, (*it)->block, sym, func, inlineNest, true );
			}
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			inlineBlock ( comp, block->loopData().condition, sym, func, inlineNest, true );
			inlineBlock ( comp, block->loopData().block, sym, func, inlineNest, true );
			break;
		case astOp::btSwitch:
			inlineBlock ( comp, block->switchData().condition, sym, func, inlineNest, true );
			for ( auto it = block->switchData().caseList.begin (); it != block->switchData().caseList.end (); it++ )
			{
				inlineBlock ( comp, (*it)->block, sym, func, inlineNest, true );
				inlineBlock ( comp, (*it)->condition, sym, func, inlineNest, true );
			}
			break;
		case astOp::conditional:
			inlineBlock ( comp, block->conditionData().trueCond, sym, func, inlineNest, true );
			inlineBlock ( comp, block->conditionData().falseCond, sym, func, inlineNest, true );
			break;
		case astOp::funcCall:
			leftType = block->left->getType ( sym );

			inlineBlock ( comp, block->left, sym, func, inlineNest, true );
			inlineBlock ( comp, block->right, sym, func, inlineNest, true );

			inlineNest.push_back ( func );
			for ( size_t loop = 0; loop < block->pList().param.size (); loop++ )
			{
				if ( block->pList().param[loop] )
				{
					if ( block->pList().param[loop]->getOp () == astOp::refCreate ) refParam = true;
					inlineBlock ( comp, block->pList().param[loop], sym, func, inlineNest, true );
				}
			}
			inlineNest.pop_back ();

			switch ( block->left->getOp () )
			{
				case astOp::atomValue:
					if ( !sym->isFunc ( block->left->symbolValue(), true ) ) break;	// check to see if we have a function definition available (if not this is dynamic loaded)

					childFunc = sym->file->functionList.find ( sym->getFuncName ( block->left->symbolValue(), true ) )->second;

					if ( childFunc->classDef )
					{
						if ( childFunc->classDef->cClass->newIndex )
						{
							if ( childFunc->classDef->cClass->elements[childFunc->classDef->cClass->newIndex - 1].methodAccess.func == childFunc )
							{
								// TODO: need to modify to call objBuild instead of objConstruct
								break;
							}
						}
					}
					switch ( sym->getScope ( block->left->symbolValue(), true ) )
					{
						case symbolSpaceScope::symFunction:
						case symbolSpaceScope::symClassMethod:
//							if ( sym->getFuncNumParams ( block->left->data.s, true ) != block->pList().param.size () + (sym->isMethod ( block->left->data.s, true ) ? 1 : 0) ) break;	// TODO: handle default parameters;
							for ( auto it = block->pList().param.begin (); it != block->pList().param.end (); it++ )
							{
								if ( !*it ) break;
								if ( (*it)->getOp () == astOp::paramExpand ) break;
								// passing self as a parameter has implications for renaming, etc.   we use selfValue as a known value within the compiler so renaming to something else will not work as desired
								// because of this we simply don't inline things that pass self
								if ( (*it)->getOp () == astOp::symbolValue && (*it)->symbolValue() == sym->file->selfValue ) break;
							}
							nNest = 0;
							for ( auto &it : inlineNest )
							{
								if ( it->name == childFunc->name )
								{
									nNest++;
									break;
								}
							}

							// TODO: make configurable
							if ( nNest >= 1 ) break;

							if ( childFunc->codeBlock && childFunc->weight && (childFunc->weight < 150) )
							{
								if ( sym->isVariantParam ( block->left->symbolValue(), true ) ) break;
								if ( hasClosure ( sym, childFunc->codeBlock ) ) break;
								if ( hasInlineBlocker ( sym, childFunc ) ) break;
								if ( hasNamingConflict ( sym, childFunc->codeBlock ) ) break;
								if ( !needValue && sym->isConst ( childFunc->name, true ) && !refParam ) break;
								if ( sym->isExtension ( block->left->symbolValue(), true ) ) break;						// this is not allowed, we'll throw eventually
								if ( block->left->symbolValue() == sym->file->releaseValue ) break;
								if ( sym->isMethod( block->left->symbolValue(), true ) && sym->isVirtual( block->left->symbolValue(), true ) ) break;

								assert ( !childFunc->isIterator );

								// delete old node contents;
								if ( sym->isMethod ( block->left->symbolValue(), true ) )
								{
									// this is a SELF method call... we already have self defined so we don't need to emit it.
									if ( childFunc->params.size () > block->pList().param.size () + 1 )
									{
										break;
									}
								} else
								{
									// just a normal function call
									if ( childFunc->params.size () > block->pList().param.size () )
									{
										break;
									}
								}

								if ( !func->nonInlinedBlock )
								{
									// copy off this code block
									func->nonInlinedBlock = new astNode ( *func->codeBlock );
								}

								symbolSpaceParams *params = new symbolSpaceParams ();
								astNode *funcNode = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( childFunc->codeBlock );

								if ( sym->isMethod ( block->left->symbolValue(), true ) )
								{

									funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined direct method: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );
								} else
								{
									funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined function: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );
								}
								astNode *newChild;

								if ( childFunc->nonInlinedBlock )
								{
									newChild = (new astNode ( *childFunc->nonInlinedBlock ))->setLocation ( childFunc->codeBlock );
								} else
								{
									newChild = (new astNode ( *childFunc->codeBlock ))->setLocation ( childFunc->codeBlock );
								}

								if ( childFunc->classDef && (memcmp ( block->left->symbolValue().c_str (), "::", 2 ) != 0) && block->left->symbolValue().find ( "::" ) != cacheString::npos && !sym->isStatic ( block->left->symbolValue(), true ) )
								{
									astNode *object = nullptr;
									auto className = block->left->symbolValue().substr ( 0, block->left->symbolValue().rfind ( "::" ) );
									auto &instantiationClass = className;
									auto start = className.rfind ( "::" );
									if ( start != cacheString::npos )
									{
										// need to do this to handle initializers
										className = className.substr ( className.rfind ( "::" ) + 2, cacheString::npos );
									}
									if ( func->isFGL )
									{
										object = new astNode ( sym->file->sCache, astOp::assign, new astNode ( sym->file->sCache, astOp::symbolValue, sym->file->selfValue ), new astNode ( sym->file->sCache, astOp::sendMsg, new astNode ( sym->file->sCache, astOp::symbolValue, sym->file->selfValue ), new astNode ( sym->file->sCache, astOp::nameValue, instantiationClass ) ) );
									} else
									{
										object = new astNode ( sym->file->sCache, astOp::assign, new astNode ( sym->file->sCache, astOp::symbolValue, sym->file->selfValue ), new astNode ( sym->file->sCache, astOp::symbolValue, instantiationClass ) );
									}
									newChild->pushNodeFront ( (new astNode ( new symbolClass ( sym->file, sym->file->sCache.get ( className ), 1 ) ))->setLocation ( childFunc->codeBlock ) );
									paramToSymLocal ( childFunc, block, newChild, params, sym, object, sym->isExtension ( block->left->symbolValue(), true ) );
								} else
								{
									paramToSymLocal ( childFunc, block, newChild, params, sym, nullptr, sym->isExtension ( block->left->symbolValue(), true ) );
								}

								// if the call target has evaluated as a method then we KNOW that this is in the same class as the caller.  we need not do anything with self
								// if this is not the case, it never would have resolved the method name (e.g. you would have had to do a.b() )   and that is resolved below
								newChild->pushNodeFront ( (new astNode ( params ))->setLocation ( childFunc->codeBlock ) );

								// inline
								// params
								// class   (swap with params)

								auto *retLabel = newChild->basicData().blocks.back ();
								newChild->basicData().blocks.pop_back ();

								funcNode->pushNode ( newChild );
								funcNode->pushNode ( retLabel );

								block->release ();

								block->setOp ( sym->file->sCache, astOp::btInline );
								block->inlineFunc().funcName = childFunc->name;
								block->inlineFunc ().retType = symUnknownType;
								block->inlineFunc().node = funcNode;

								block->fixupReturn ( sym, childFunc );
								block->typeInfer ( comp, func, &sym->file->errHandler, sym, &block->inlineFunc().retType, false, true, false, nullptr, false );
								inlineNest.push_back ( childFunc );
								inlineBlock ( comp, block, sym, func, inlineNest, true );
								inlineNest.pop_back ();
								return true;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::sendMsg:
/* verified */
					if ( block->left->right->getOp () == astOp::nameValue )
					{
						fgxClassElemSearchType	 searchType;
						compClass *classDef;
						if ( block->left->left->getType ( sym ).hasClass () )
						{
							if ( (block->left->left->getOp () == astOp::symbolValue) && block->left->left->symbolValue() == sym->file->selfValue )
							{
								classDef = sym->findClass ( sym->getType ( sym->file->selfValue, true ).getClass () );
								searchType = fgxClassElemSearchType::fgxClassElemAllPrivate;
							} else
							{
								classDef = sym->findClass ( block->left->left->getType ( sym ).getClass () );
								searchType = fgxClassElemSearchType::fgxClassElemAllPublic;
							}
							if ( !classDef ) break;		// may not have a definition... may be dynamically loaded at runtime

							elem = classDef->getElement ( searchType, block->left->right->symbolValue() );
							
// TODO: add inlining for default methods!!!
							if ( elem )
							{
								switch ( elem->type )
								{
									case fgxClassElementType::fgxClassType_method:
										{
											if ( (elem->methodAccess.objectStart != 0) && !elem->methodAccess.func->isExtension ) break;
											if ( elem->isVirtual ) break;
											if ( elem->methodAccess.func->isVariantParam ) break;
											assert ( !elem->methodAccess.func->isIterator );

											for ( auto it = block->pList().param.begin (); it != block->pList().param.end (); it++ )
											{
												if ( !*it ) break;
												if ( (*it)->getOp () == astOp::paramExpand ) break;
												// passing self as a parameter has implications for renaming, etc.   we use selfValue as a known value within the compiler so renaming to something else will not work as desired
												// because of this we simply don't inline things that pass self
												if ( (*it)->getOp () == astOp::symbolValue && (*it)->symbolValue() == sym->file->selfValue ) break;
											}

											childFunc = elem->methodAccess.func;

											if ( childFunc->classDef )
											{
												if ( childFunc->classDef->cClass->newIndex )
												{
													if ( childFunc->classDef->cClass->elements[childFunc->classDef->cClass->newIndex - 1].methodAccess.func == childFunc )
													{
														break;
													}
												}
											}

											nNest = 0;
											for ( auto &it : inlineNest )
											{
												if ( it->name == childFunc->name )
												{
													nNest++;
													break;
												}
											}
											// TODO: make configurable
											if ( nNest >= 1 ) break;

											if ( childFunc->codeBlock && childFunc->weight && (childFunc->weight < 200) )
											{

												if ( sym->isVariantParam ( childFunc->name, true ) ) break;
												if ( hasClosure ( sym, childFunc->codeBlock ) ) break;
												if ( hasInlineBlocker ( sym, childFunc ) ) break;
												if ( hasNamingConflict ( sym, childFunc->codeBlock ) ) break;
												if ( !needValue && sym->isConst ( childFunc->name, true ) && !refParam ) break;
												if ( block->left->right->symbolValue() == sym->file->releaseValue )
													break;

												if ( !func->nonInlinedBlock )
												{
													// copy off this code block
													func->nonInlinedBlock = new astNode ( *func->codeBlock );
												}
												symbolSpaceParams *params = new symbolSpaceParams ();
												astNode *funcNode = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( childFunc->codeBlock );
												astNode *newChild;
												if ( childFunc->nonInlinedBlock )
												{
													newChild = (new astNode ( *childFunc->nonInlinedBlock ))->setLocation ( childFunc->codeBlock );
												} else
												{
													newChild = (new astNode ( *childFunc->codeBlock ))->setLocation ( childFunc->codeBlock );
												}

												if ( !childFunc->inUse )
												{
													unique_queue<accessorType> scanQueue;

													typeInfer ( comp, comp->file, childFunc, false, true, &scanQueue, true );
												}
												if ( elem->methodAccess.func->isExtension )
												{
													paramToSymLocal ( childFunc, block, newChild, params, sym, new astNode ( *block->left->left ), true );
													funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined extension method: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );
												} else if ( elem->isStatic )
												{
													paramToSymLocal ( childFunc, block, newChild, params, sym, nullptr, false );
													funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined static method: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );
												} else
												{
													paramToSymLocal ( childFunc, block, newChild, params, sym, new astNode ( *block->left->left ), false );
													funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined method: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );
													if ( childFunc->isFGL )
													{
														// nothing to do here...   in this case astOp::self is constructed in paramToSymLocal
													} else
													{
														newChild->pushNodeFront ( (new astNode ( new symbolClass ( sym->file, classDef->oClass->name, 1 ) ))->setLocation ( childFunc->codeBlock ) );
													}
												}
												newChild->pushNodeFront ( (new astNode ( params ))->setLocation ( childFunc->codeBlock ) );

												auto *retLabel = newChild->basicData().blocks.back ();
												newChild->basicData().blocks.pop_back ();

												funcNode->pushNode ( newChild );
												funcNode->pushNode ( retLabel );

												block->left->left = 0;
												block->release ();

												block->setOp ( sym->file->sCache, astOp::btInline );
												block->inlineFunc().funcName = childFunc->name;
												block->inlineFunc().retType = symUnknownType;
												block->inlineFunc().node = funcNode;

												block->fixupReturn ( sym, childFunc );

												inlineNest.push_back ( childFunc );
												block->typeInfer ( comp, func, &sym->file->errHandler, sym, &block->inlineFunc().retType, false, true, false, nullptr, false );
												inlineBlock ( comp, block, sym, func, inlineNest, true );
												inlineNest.pop_back ();
												return true;
											}
										}
										break;
									case fgxClassElementType::fgxClassType_iVar:
									case fgxClassElementType::fgxClassType_static:
									case fgxClassElementType::fgxClassType_const:
										leftType = block->left->getType ( sym );
										goto inline_functor;
									default:
										break;
								}
							}
						}
					}
					break;
				case astOp::symbolValue:
					switch ( sym->getScope ( block->left->symbolValue(), true ) )
					{
						case symbolSpaceScope::symGlobal:
						case symbolSpaceScope::symLocal:
						case symbolSpaceScope::symLocalStatic:
						case symbolSpaceScope::symLocalParam:
						case symbolSpaceScope::symLocalConst:
						case symbolSpaceScope::symClassConst:
						case symbolSpaceScope::symClassStatic:
						case symbolSpaceScope::symClassIVar:
/* verified */
							/* functor */
							leftType = block->left->getType ( sym );
inline_functor:
							if ( leftType.hasClass () )
							{
								fgxClassElementScope	 scope;
								compClass *classDef;

								classDef = sym->findClass ( leftType.getClass () );
								scope = fgxClassElementScope::fgxClassScope_public;

								if ( classDef->overload[int(fgxOvOp::ovFuncCall)] )
								{
									elem = &classDef->elements[classDef->overload[int(fgxOvOp::ovFuncCall)] - 1];
									childFunc = sym->file->functionList.find ( sym->getFuncName ( elem->methodAccess.func->name, true ) )->second;

									if ( elem->scope > scope ) break;
									if ( elem->type != fgxClassElementType::fgxClassType_method ) break;
									if ( !elem->methodAccess.func ) break;		// really an error state... we're just not throwing it here
									// push context of access method
									if ( elem->methodAccess.objectStart != 0 ) break;
									if ( elem->isVirtual ) break;
									if ( sym->isVariantParam ( childFunc->name, true ) ) break;

									// TODO: use namespace to emit for non-base class member

									nNest = 0;
									for ( auto &it : inlineNest )
									{
										if ( it->name == childFunc->name )
										{
											nNest++;
											break;
										}
									}
									// TODO: make configurable
									if ( nNest >= 1 ) break;

									if ( childFunc->codeBlock && childFunc->weight && (childFunc->weight < 200) )
									{
										// delete old node contents;
										// this is a SELF method call... we already have self defined so we don't need to emit it.
										if ( childFunc->params.size () > block->pList().param.size () + 1 ) break;
										if ( !needValue && sym->isConst ( childFunc->name, true ) && !refParam ) break;

										if ( !func->nonInlinedBlock )
										{
											// copy off this code block
											func->nonInlinedBlock = new astNode ( *func->codeBlock );
										}

										symbolSpaceParams *params = new symbolSpaceParams ();
										astNode *funcNode = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( childFunc->codeBlock );

										funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined functor: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );

										astNode *newChild;

										if ( childFunc->nonInlinedBlock )
										{
											newChild = (new astNode ( *childFunc->nonInlinedBlock ))->setLocation ( childFunc->codeBlock );
										} else
										{
											newChild = (new astNode ( *childFunc->codeBlock ))->setLocation ( childFunc->codeBlock );
										}

										paramToSymLocal ( childFunc, block, newChild, params, sym, new astNode ( *block->left ), false );

										newChild->pushNodeFront ( (new astNode ( new symbolClass ( sym->file, classDef->oClass->name, 1 ) ))->setLocation ( childFunc->codeBlock ) );
										newChild->pushNodeFront ( (new astNode ( params ))->setLocation ( childFunc->codeBlock ) );

										auto *retLabel = newChild->basicData().blocks.back ();
										newChild->basicData().blocks.pop_back ();

										funcNode->pushNode ( newChild );
										funcNode->pushNode ( retLabel );

										block->release ();

										block->setOp ( sym->file->sCache, astOp::btInline );
										block->inlineFunc().funcName = childFunc->name;
										block->inlineFunc().retType = symUnknownType;
										block->inlineFunc().node = funcNode;

										block->fixupReturn ( sym, childFunc );

										inlineNest.push_back ( childFunc );
										block->typeInfer ( comp, func, &sym->file->errHandler, sym, &block->inlineFunc().retType, false, true, false, nullptr, false );
										inlineBlock ( comp, block, sym, func, inlineNest, true );
										inlineNest.pop_back ();
										return true;
									}
								}
							}
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			break;
		case astOp::symbolValue:
			switch ( sym->getScope ( block->symbolValue(), true ) )
			{
				case symbolSpaceScope::symClassAccess:
/* verified */
					elem = sym->findClass ( sym->getType ( sym->file->selfValue, true ).getClass () )->getElement ( block->symbolValue() );
					childFunc = sym->file->functionList.find ( sym->getFuncName ( elem->methodAccess.func->name, true ) )->second;

					if ( elem->type != fgxClassElementType::fgxClassType_prop ) break;
					if ( !elem->methodAccess.func ) break;		// really an error state... we're just not throwing it here
					// push context of access method
					if ( elem->methodAccess.objectStart != 0 ) break;
					if ( elem->isVirtual ) break;
					if ( sym->isVariantParam ( childFunc->name, true ) ) break;

					// TODO: use namespace to emit for non-base class member

					nNest = 0;
					for ( auto &it : inlineNest )
					{
						if ( it->name == childFunc->name )
						{
							nNest++;
							break;
						}
					}
					// TODO: make configurable
					if ( nNest >= 1 ) break;

					if ( childFunc->codeBlock && childFunc->weight && (childFunc->weight < 200) )
					{
						if ( hasClosure ( sym, childFunc->codeBlock ) ) break;
						if ( hasInlineBlocker ( sym, childFunc ) ) break;
						if ( hasNamingConflict ( sym, childFunc->codeBlock ) ) break;

						if ( !func->nonInlinedBlock )
						{
							// copy off this code block
							func->nonInlinedBlock = new astNode ( *func->codeBlock );
						}

						astNode *funcNode = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( childFunc->codeBlock );
						funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined accessor: %s-", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );

						if ( childFunc->codeBlock->isReturn ( sym ) )
						{
							*block = *(childFunc->codeBlock->isReturn ( sym )->returnData().node);
						} else
						{
							astNode *newChild;
							if ( childFunc->nonInlinedBlock )
							{
								newChild = (new astNode ( *childFunc->nonInlinedBlock ))->setLocation ( childFunc->codeBlock );
							} else
							{
								newChild = (new astNode ( *childFunc->codeBlock ))->setLocation ( childFunc->codeBlock );
							}

							auto *retLabel = newChild->basicData().blocks.back ();
							newChild->basicData().blocks.pop_back ();

							funcNode->pushNode ( newChild );
							funcNode->pushNode ( retLabel );

							block->release ();

							/*	no parameters are necessary... normally the only one would be SELF but since this is an access from within a method fo the same class we already have
								self visible on the symbol stack
							*/

							block->setOp ( sym->file->sCache, astOp::btInline );
							block->inlineFunc().funcName = childFunc->name;
							block->inlineFunc().retType = symUnknownType;
							block->inlineFunc().node = funcNode;

							block->fixupReturn ( sym, func );

							inlineNest.push_back ( childFunc );
							block->typeInfer ( comp, func, &sym->file->errHandler, sym, &block->inlineFunc().retType, false, true, false, nullptr, false );
							inlineBlock ( comp, block, sym, func, inlineNest, true );
							inlineNest.pop_back ();
							return true;
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::assign:
			switch ( block->left->getOp () )
			{
				case astOp::symbolValue:
/* verified */
					switch ( sym->getScope ( block->left->symbolValue(), false ) )
					{
						case symbolSpaceScope::symClassAssign:
							elem = sym->findClass ( sym->getType ( sym->file->selfValue, true ).getClass () )->getElement ( block->left->symbolValue() );
							childFunc = sym->file->functionList.find ( sym->getFuncName ( elem->assign.func->name, false ) )->second;

							if ( elem->type != fgxClassElementType::fgxClassType_prop ) break;
							if ( !elem->assign.func ) break;
							// push context of access method
							if ( elem->assign.objectStart != 0 ) break;
							if ( elem->isVirtual ) break;;
							if ( sym->isVariantParam ( childFunc->name, true ) ) break;

							// TODO: use the object field in localhost to emit for a context other than 0?

							nNest = 0;
							for ( auto &it : inlineNest )
							{
								if ( it->name == childFunc->name )
								{
									nNest++;
									break;
								}
							}
							// TODO: make configurable
							if ( nNest >= 1 ) break;

							if ( childFunc->codeBlock && childFunc->weight && (childFunc->weight < 200) )
							{
								if ( hasClosure ( sym, childFunc->codeBlock ) ) break;
								if ( hasInlineBlocker ( sym, childFunc ) ) break;
								if ( hasNamingConflict ( sym, childFunc->codeBlock ) ) break;

								if ( !func->nonInlinedBlock )
								{
									// copy off this code block
									func->nonInlinedBlock = new astNode ( *func->codeBlock );
								}

								symbolSpaceParams *params = new symbolSpaceParams ();
								astNode *funcNode = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( childFunc->codeBlock );
								funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined assigner: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );

								astNode *newChild;
								if ( childFunc->nonInlinedBlock )
								{
									newChild = (new astNode ( *childFunc->nonInlinedBlock ))->setLocation ( childFunc->codeBlock );
								} else
								{
									newChild = (new astNode ( *childFunc->codeBlock ))->setLocation ( childFunc->codeBlock );
								}

								astNode f ( sym->file->sCache, astOp::funcCall );
								f.pList().param.push_back ( block->right );
								f.pList ().paramRegion.push_back ( *block->right );
								paramToSymLocal ( childFunc, &f, newChild, params, sym, 0, false );
								f.pList().param.clear ();
								f.pList ().paramRegion.clear ();

								newChild->pushNodeFront ( (new astNode ( params ))->setLocation ( childFunc->codeBlock ) );

								auto *retLabel = newChild->basicData().blocks.back ();
								newChild->basicData().blocks.pop_back ();

								funcNode->pushNode ( newChild );
								funcNode->pushNode ( retLabel );

								block->right = 0;
								block->release ();

								block->setOp ( sym->file->sCache, astOp::btInline );
								block->inlineFunc().funcName = childFunc->name;
								block->inlineFunc().retType = symUnknownType;
								block->inlineFunc().node = funcNode;

								block->fixupReturn ( sym, childFunc );

								inlineNest.push_back ( childFunc );
								block->typeInfer ( comp, func, &sym->file->errHandler, sym, &block->inlineFunc().retType, false, true, false, nullptr, false );
								inlineBlock ( comp, block, sym, func, inlineNest, true );
								inlineNest.pop_back ();
								return true;
							}
							break;
						default:
							break;
					}
					break;
				case astOp::sendMsg:
/* verified */
					if ( block->left->right->getOp () == astOp::symbolValue )
					{
						fgxClassElemSearchType	 searchType;
						compClass *classDef;

						if ( block->left->left->getType ( sym ).hasClass () )
						{
							if ( (block->left->left->getOp () == astOp::symbolValue) && block->left->left->symbolValue() == sym->file->selfValue )
							{
								classDef = sym->findClass ( sym->getType ( sym->file->selfValue, true ).getClass () );
								searchType = fgxClassElemSearchType::fgxClassElemPrivateAssign;
							} else
							{
								classDef = sym->findClass ( block->left->left->getType ( sym ).getClass () );
								searchType = fgxClassElemSearchType::fgxClassElemPublicAssign;
							}
							if ( !classDef ) break;		// may not have a definition... may be dynamically loaded at runtime

							elem = classDef->getElement ( searchType, block->left->right->symbolValue() );

							if ( elem )
							{
								if ( elem->type == fgxClassElementType::fgxClassType_prop )
								{
									childFunc = elem->assign.func;

									if ( !elem->assign.func ) break;
									if ( elem->assign.objectStart != 0 ) break;
									if ( elem->isVirtual ) break;
									if ( sym->isVariantParam ( childFunc->name, true ) ) break;

									nNest = 0;
									for ( auto &it : inlineNest )
									{
										if ( it->name == childFunc->name )
										{
											nNest++;
											break;
										}
									}
									// TODO: make configurable
									if ( nNest >= 1 ) break;

									if ( childFunc->codeBlock && childFunc->weight && (childFunc->weight < 200) )
									{
										if ( hasClosure ( sym, childFunc->codeBlock ) ) break;
										if ( hasInlineBlocker ( sym, childFunc ) ) break;
										if ( hasNamingConflict ( sym, childFunc->codeBlock ) ) break;

										// left->left is our object
										// left->right is our method name
										// right is our value

										if ( !func->nonInlinedBlock )
										{
											// copy off this code block
											func->nonInlinedBlock = new astNode ( *func->codeBlock );
										}

										symbolSpaceParams *params = new symbolSpaceParams ();
										astNode *funcNode = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( childFunc->codeBlock );
										funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined object assigner: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );

										astNode *newChild;
										if ( childFunc->nonInlinedBlock )
										{
											newChild = (new astNode ( *childFunc->nonInlinedBlock ))->setLocation ( childFunc->codeBlock );
										} else
										{
											newChild = (new astNode ( *childFunc->codeBlock ))->setLocation ( childFunc->codeBlock );
										}

										astNode f ( sym->file->sCache, astOp::funcCall );
										f.pList().param.push_back ( block->right );
										f.pList ().paramRegion.push_back (* block->right );

										paramToSymLocal ( childFunc, &f, newChild, params, sym, new astNode ( *block->left->left ), false );
										f.pList().param.clear ();		// so we don't do any cleanup here... nothing should get deleted
										f.pList ().paramRegion.clear ();

										if ( childFunc->isFGL )
										{
											// self is constructed in paramToSymLocal so nothing to do here
										} else
										{
											newChild->pushNodeFront ( (new astNode ( new symbolClass ( sym->file, classDef->oClass->name, 1 ) ))->setLocation ( childFunc->codeBlock ) );
										}
										newChild->pushNodeFront ( (new astNode ( params ))->setLocation ( childFunc->codeBlock ) );

										auto *retLabel = newChild->basicData().blocks.back ();
										newChild->basicData().blocks.pop_back ();

										funcNode->pushNode ( newChild );
										funcNode->pushNode ( retLabel );

										block->left->left = 0;
										block->right = 0;

										block->release ();

										block->setOp ( sym->file->sCache, astOp::btInline );
										block->inlineFunc().funcName = childFunc->name;
										block->inlineFunc().retType = symUnknownType;
										block->inlineFunc().node = funcNode;

										block->fixupReturn ( sym, childFunc );

										inlineNest.push_back ( childFunc );
										block->typeInfer ( comp, func, &sym->file->errHandler, sym, &block->inlineFunc().retType, false, true, false, nullptr, false );
										inlineBlock ( comp, block, sym, func, inlineNest, true );
										inlineNest.pop_back ();
										return true;
									}
								}
							}
						}
					}
					break;
				default:
					break;
			}
			inlineBlock ( comp, block->right, sym, func, inlineNest, true );
			break;
		case astOp::sendMsg:
/* verified */
			inlineBlock ( comp, block->left, sym, func, inlineNest, true );
			inlineBlock ( comp, block->right, sym, func, inlineNest, true );

			if ( block->right->getOp () == astOp::symbolValue )
			{
				fgxClassElemSearchType	 searchType;
				compClass *classDef;

				if ( block->left->getType ( sym ).hasClass () )
				{
					if ( (block->left->getOp () == astOp::symbolValue) && (block->left->symbolValue() == sym->file->selfValue) )
					{
						classDef = sym->findClass ( sym->getType ( sym->file->selfValue, true ).getClass () );
						searchType = fgxClassElemSearchType::fgxClassElemPrivateAccess;
					} else
					{
						classDef = sym->findClass ( block->left->getType ( sym ).getClass () );
						searchType = fgxClassElemSearchType::fgxClassElemPublicAccess;
					}
					if ( !classDef ) break;		// may not have a definition... may be dynamically loaded at runtime

					elem = classDef->getElement ( searchType, block->right->symbolValue() );

					if ( elem )
					{
						if ( elem->type == fgxClassElementType::fgxClassType_prop )
						{
							childFunc = elem->methodAccess.func;

							if ( !elem->methodAccess.func ) break;
							if ( elem->methodAccess.objectStart != 0 ) break;
							if ( elem->isVirtual ) break;
							if ( sym->isVariantParam ( childFunc->name, true ) ) break;

							nNest = 0;
							for ( auto &it : inlineNest )
							{
								if ( it->name == childFunc->name )
								{
									nNest++;
									break;
								}
							}
							// TODO: make configurable
							if ( nNest >= 1 ) break;

							if ( childFunc->codeBlock && childFunc->weight && (childFunc->weight < 200) )
							{
								if ( hasClosure ( sym, childFunc->codeBlock ) ) break;
								if ( hasInlineBlocker ( sym, childFunc ) ) break;
								if ( hasNamingConflict ( sym, childFunc->codeBlock ) ) break;

								if ( !func->nonInlinedBlock )
								{
									// copy off this code block
									func->nonInlinedBlock = new astNode ( *func->codeBlock );
								}

								symbolSpaceParams *params = new symbolSpaceParams ();
								astNode *funcNode = (new astNode ( sym->file->sCache, astOp::btBasic ))->setLocation ( childFunc->codeBlock );
								funcNode->pushNode ( (new astNode ( sym->file->sCache, astOp::btAnnotation, "Start of inlined object accessor: %s", childFunc->name.c_str () ))->setLocation ( childFunc->codeBlock ) );

								astNode *newChild;
								if ( childFunc->nonInlinedBlock )
								{
									newChild = (new astNode ( *childFunc->nonInlinedBlock ))->setLocation ( childFunc->codeBlock );
								} else
								{
									newChild = (new astNode ( *childFunc->codeBlock ))->setLocation ( childFunc->codeBlock );
								}

								astNode f ( sym->file->sCache, astOp::funcCall );
								paramToSymLocal ( childFunc, &f, newChild, params, sym, new astNode ( *block->left ), false );
								f.pList().param.clear ();		// so we don't do any cleanup here... nothing should get deleted

								if ( childFunc->isFGL )
								{
									// nothing to do here... as self is constructed in paramToSymLocal
								} else
								{
									newChild->pushNodeFront ( (new astNode ( new symbolClass ( sym->file, childFunc->classDef->name, 1 ) ))->setLocation ( childFunc->codeBlock ) );
								}
								newChild->pushNodeFront ( (new astNode ( params ))->setLocation ( childFunc->codeBlock ) );

								auto *retLabel = newChild->basicData().blocks.back ();
								newChild->basicData().blocks.pop_back ();

								funcNode->pushNode ( newChild );
								funcNode->pushNode ( retLabel );

								block->left = 0;
								block->release ();

								block->setOp ( sym->file->sCache, astOp::btInline );
								block->inlineFunc().funcName = childFunc->name;
								block->inlineFunc().retType = symUnknownType;
								block->inlineFunc().node = funcNode;

								block->fixupReturn ( sym, childFunc );

								inlineNest.push_back ( childFunc );
								block->typeInfer ( comp, func, &sym->file->errHandler, sym, &block->inlineFunc().retType, false, true, false, nullptr, false );
								inlineBlock ( comp, block, sym, func, inlineNest, true );
								inlineNest.pop_back ();
								return true;
							}
						}
					}
				}
			}
			break;
		case astOp::getEnumerator:
			inlineBlock ( comp, block->left, sym, func, inlineNest, true );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( size_t loop = 0; loop < block->arrayData().nodes.size (); loop++ )
			{
				inlineBlock ( comp, block->arrayData().nodes[loop], sym, func, inlineNest, true );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			inlineBlock ( comp, block->left, sym, func, inlineNest, true );
			for ( size_t loop = 0; loop < block->pList().param.size (); loop++ )
			{
				inlineBlock ( comp, block->pList().param[loop], sym, func, inlineNest, true );
			}
			break;
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
		case astOp::postInc:
		case astOp::postDec:
		case astOp::preInc:
		case astOp::preDec:
		case astOp::negate:
			// DON'T inline the LVALUE  we need it to be the value itself or we'll emit incorrect code.
			// TODO: convert these cases to lhs ( rhs <op> value ) and inline
			inlineBlock ( comp, block->right, sym, func, inlineNest, true );
			break;
		default:
			inlineBlock ( comp, block->left, sym, func, inlineNest, true );
			inlineBlock ( comp, block->right, sym, func, inlineNest, true );
			break;
	}
	return false;
}

void optimizer::funcInline ( opFunction *func )
{
	size_t	weight;

	if ( func->isInterface ) return;

	symbolStack sym ( file, func );
	std::vector<opFunction *> inlineNest;

	if ( func->codeBlock )
	{
		weight = func->weight;
		inlineBlock ( compDef, func->codeBlock, &sym, func, inlineNest, false );

		if ( func->weight != weight )
		{
			compDef->fixupGoto ( func );
		}
	}
}

void optimizer::findInlineCandidates ()
{
	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		(*it).second->weight = 0;
		if ( (*it).second->codeBlock )
		{
			funcInlineWeight ( (*it).second );
		}
	}
}

void optimizer::funcInline ()
{
	findInlineCandidates ();

	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		if ( (*it).second->inUse )
		{
			funcInline ( (*it).second );
		}
	}
}
