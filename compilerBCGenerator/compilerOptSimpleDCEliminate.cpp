
#include "compilerBCGenerator.h"
#include "compilerOptimizer.h"

// note: these do NOT include compiler generated labels for backpatching loops and switch's... these are purely for user defined label/goto's

static stringi retValue { "$ret" };
static stringi barrierValue { "$barrier" };
static stringi exceptValue { "except" };

void optimizer::makeLabelList ( astNode *block, std::unordered_map<cacheString, std::pair<astNode *, bool> > &labels )
{
	if ( !block )
	{
		return;
	}

	switch ( block->getOp () )
	{
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					makeLabelList ( block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName () ), labels );
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							makeLabelList ( (*symbol)[it]->initializer, labels );
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			for ( auto it = block->basicData().blocks.begin (); it != block->basicData().blocks.end (); it++ )
			{
				makeLabelList ( (*it), labels );
			}
			break;
		case astOp::btInline:
			makeLabelList ( block->inlineFunc().node, labels );
			break;
		case astOp::btLabel:
			if ( labels.find ( block->label() ) == labels.end () )
			{
				if ( block->label() == compDef->file->barrierValue )
				{
					// barriers are always true
					labels[block->label()] = std::make_pair ( block, true );
				} else
				{
					labels[block->label()] = std::make_pair ( block, false );
				}
			}
			break;
		case astOp::btGoto:
		case astOp::btBreak:
		case astOp::btCont:
			if ( labels.find ( block->gotoData().label ) == labels.end () )
			{
				labels[block->gotoData().label] = std::make_pair ( block, true );
			} else
			{
				(labels[block->gotoData().label]).second = true;
			}
			break;
		case astOp::btInlineRet:
		case astOp::btReturn:
			if ( labels.find ( block->returnData().label ) == labels.end () )
			{
				labels[block->returnData().label] = std::make_pair ( block, true );
			} else
			{
				(labels[block->returnData().label]).second = true;
			}
			makeLabelList ( block->returnData().node, labels );
			break;
		case astOp::btTry:
			makeLabelList ( block->tryCatchData().tryBlock, labels );
			makeLabelList ( block->tryCatchData().finallyBlock, labels );
			makeLabelList ( block->tryCatchData().catchBlock, labels );
			break;
		case astOp::btFor:
			makeLabelList ( block->loopData().condition, labels );
			makeLabelList ( block->loopData().increase, labels );
			makeLabelList ( block->loopData().block, labels );
			makeLabelList ( block->loopData().initializers, labels );
			break;
		case astOp::btForEach:
			makeLabelList ( block->forEachData().var, labels );
			makeLabelList ( block->forEachData().in, labels );
			makeLabelList ( block->forEachData().statement, labels );
			break;
		case astOp::btIf:
			makeLabelList ( block->ifData().elseBlock, labels );
			for ( auto it = block->ifData().ifList.begin (); it != block->ifData().ifList.end (); it++ )
			{
				makeLabelList ( (*it)->condition, labels );
				makeLabelList ( (*it)->block, labels );
			}
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			makeLabelList ( block->loopData().condition, labels );
			makeLabelList ( block->loopData().block, labels );
			break;
		case astOp::btSwitch:
			makeLabelList ( block->switchData().condition, labels );
			for ( auto it = block->switchData().caseList.begin (); it != block->switchData().caseList.end (); it++ )
			{
				makeLabelList ( (*it)->block, labels );
				makeLabelList ( (*it)->condition, labels );
			}
			break;
		case astOp::conditional:
			makeLabelList ( block->conditionData().trueCond, labels );
			makeLabelList ( block->conditionData().falseCond, labels );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( size_t loop = 0; loop < block->arrayData().nodes.size (); loop++ )
			{
				makeLabelList ( block->arrayData().nodes[loop], labels );
			}
			break;
		case astOp::arrayDeref:
		case astOp::funcCall:
		case astOp::cArrayDeref:
			makeLabelList ( block->left, labels );
			for ( size_t loop = 0; loop < block->pList().param.size (); loop++ )
			{
				makeLabelList ( block->pList().param[loop], labels );
			}
			break;
		default:
			makeLabelList ( block->left, labels );
			makeLabelList ( block->right, labels );
			break;
	}
}

// this does it's own iteration because it does early-out... if a depth search of the block shows that it has an in-use label
// then it returns as quickly and doesn't continue to search
bool optimizer::doesBlockHaveNeededLabel ( astNode *block, std::unordered_map<cacheString, std::pair<astNode *, bool> > &labels )
{
	if ( !block )
	{
		return false;
	}

	switch ( block->getOp () )
	{
		case astOp::btBasic:
			for ( auto it = block->basicData().blocks.begin (); it != block->basicData().blocks.end (); it++ )
			{
				if ( doesBlockHaveNeededLabel ( (*it), labels ) ) return true;
			}
			break;
		case astOp::btLabel:
			if ( labels[block->label()].second ) return true;
			break;
		case astOp::btTry:
			if ( doesBlockHaveNeededLabel ( block->tryCatchData().tryBlock, labels ) ) return true;
			if ( doesBlockHaveNeededLabel ( block->tryCatchData().finallyBlock, labels ) ) return true;
			if ( doesBlockHaveNeededLabel ( block->tryCatchData().catchBlock, labels ) ) return true;
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
		case astOp::btFor:
			if ( doesBlockHaveNeededLabel ( block->loopData().increase, labels ) ) return true;
			if ( doesBlockHaveNeededLabel ( block->loopData().block, labels ) ) return true;
			if ( doesBlockHaveNeededLabel ( block->loopData().initializers, labels ) ) return true;
			break;
		case astOp::btForEach:
			if ( doesBlockHaveNeededLabel ( block->forEachData().var, labels ) ) return true;
			if ( doesBlockHaveNeededLabel ( block->forEachData().statement, labels ) ) return true;
			if ( doesBlockHaveNeededLabel ( block->forEachData().in, labels ) ) return true;
			break;
		case astOp::btIf:
			for ( auto it = block->ifData().ifList.begin (); it != block->ifData().ifList.end (); it++ )
			{
				if ( doesBlockHaveNeededLabel ( (*it)->block, labels ) ) return true;
			}
			if ( doesBlockHaveNeededLabel ( block->ifData().elseBlock, labels ) ) return true;
			break;
		case astOp::btSwitch:
			for ( auto it = block->switchData().caseList.begin (); it != block->switchData().caseList.end (); it++ )
			{
				if ( doesBlockHaveNeededLabel ( (*it)->block, labels ) ) return true;
			}
			break;
		case astOp::btExit:
			return true;
		default:
			break;
	}
	return false;
}

/*
	basic algorithm...

	we start off with code emission enabled
	if we have somethign that does not need to be emitted (e.g. inside some conditional that is always false or would otherwise never be called) we stop emitting
		UNLESS what is inside has a label that is an active jump target.   if that's the case astOp::we emit anyway

		a couple of things to keep in-mind.
			simpleDCEliminateBlock is only called on code for which we wish to emit.
			if we ever wish to NOT emit code we need to first call doesBlockHaveNeededLabel to see if we are OK with eliminating code emission
			otherwise we always emit the block.

			only a few things can stop emission
				a basic block may stop emission if it finds that there are elements change code flow such as
					returns, slides, goto, break, cont and exit
					if this is the case astOp::the basic block will emit that node and then turn off emission.
					it will reenable emission if entry(or children) has a label which is an active jump target.
				looping constructs may not emit if a condition allows them to avoid emission, again, a jump target will not allow emission to be disabled
				if statements may a avoid emission of always-false conditions unless there is a jump target.

*/
astNode *optimizer::simpleDCEliminateBlock ( astNode *block, std::unordered_map<cacheString, std::pair<astNode *, bool> > &labels, symbolStack *sym )
{
	if ( !block )
	{
		return nullptr;
	}

	switch ( block->getOp () )
	{
		case astOp::btBasic:
			{
				astBasic newList;
				bool doEmit = true;
				astNode *prevNode = nullptr;
				auto size = sym->size ();
				for ( auto &it : block->basicData().blocks )
				{
					if ( !doEmit && !doesBlockHaveNeededLabel ( it, labels ) )
					{
						transformMade = true;
						delete it;
						it = 0;
					} else
					{
						doEmit = true;
						switch ( it->getOp () )
						{
							case astOp::btInlineRet:
							case astOp::btReturn:
								// a return can become meaningless...   this is true IF it has nothing to value (we're a procedure), does not generate any stack pop's
								// and if it has no desitnation label (the target is immediately after the return).   In this case we can simply delete the return
								// this then lets us do the same for previous returns...
								if ( it->returnData().node || it->returnData().nPop || it->returnData().label.size () )
								{
									it->returnData().node = simpleDCEliminateBlock ( it->returnData().node, labels, sym );
									newList.blocks.push_back ( it );
								} else
								{
									transformMade = true;
									delete it;
									it = 0;
								}
								doEmit = false;
								break;
							case astOp::btGoto:
							case astOp::btBreak:
							case astOp::btCont:
							case astOp::btExit:
								newList.blocks.push_back ( it );
								doEmit = false;
								break;
							case astOp::btLabel:
								if ( prevNode )
								{
									switch ( prevNode->getOp () )
									{
										case astOp::btReturn:
										case astOp::btInlineRet:
											if ( prevNode->returnData().label == it->symbolValue() )
											{
												transformMade = true;
												prevNode->returnData().label = cacheString();
											}
											break;
										case astOp::btGoto:
										case astOp::btBreak:
										case astOp::btCont:
											if ( prevNode->gotoData().label == it->symbolValue() )
											{
												transformMade = true;
												prevNode->gotoData().label = cacheString();
											}
											break;
										default:
											prevNode->removeUnnecessaryJmp ( sym, it->symbolValue() );
											break;
									}
								}
								{
									it = simpleDCEliminateBlock ( it, labels, sym );
									if ( it )
									{
										newList.blocks.push_back ( it );
									}
								}
								break;
							case astOp::btAnnotation:
								newList.blocks.push_back ( it );
								break;
							case astOp::btStatement:
								break;
							default:
								if ( it && !it->hasNoSideEffects ( sym, false ) )
								{
									astNode *newBlock = simpleDCEliminateBlock ( it, labels, sym );
									if ( newBlock )
									{
										newList.blocks.push_back ( newBlock );
										switch ( newBlock->getOp () )
										{
											case astOp::funcCall:
												if ( newBlock->left->getOp () == astOp::atomValue )
												{
													if ( newBlock->left->symbolValue() == compDef->file->exceptValue )
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
												if ( newBlock->alwaysReturns () )
												{
													doEmit = false;
												}
												break;
											default:
												break;
										}
									} else
									{
										transformMade = true;
									}
									it = newBlock;
								} else
								{
									delete it;
									it = 0;
								}
								break;
						}
					}
					if ( it && (it->getOp () != astOp::btLabel) )
					{
						if ( newList.blocks.size () )
						{
							prevNode = newList.blocks.back ();
						} else
						{
							prevNode = nullptr;
						}
					}
				}
				block->basicData().emitPops = doEmit;
				sym->resize ( size );
				block->basicData().blocks.swap ( newList.blocks );
				newList.blocks.clear ();

				if ( !block->basicData().blocks.size () )
				{
					delete block;
					return nullptr;
				}
			}
			break;
		case astOp::symbolDef:
			switch ( block->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					{
						auto init = block->symbolDef()->getInitializer ( block->symbolDef()->getSymbolName () );
						if ( init->getOp () == astOp::assign )
						{
							init->right = simpleDCEliminateBlock ( init->right, labels, sym );
							block->symbolDef()->setInitializer ( block->symbolDef()->getSymbolName (), init );
						}
						sym->push ( block->symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(block->symbolDef());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							auto init = (*symbol)[it]->initializer;
							if ( init )
							{
								if ( init->getOp () == astOp::assign )
								{
									init->right = simpleDCEliminateBlock ( init->right, labels, sym );
								} else
								{
									init = simpleDCEliminateBlock ( init, labels, sym );
								}
							}
							(*symbol)[it]->initializer = init;
						}
						sym->push ( block->symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass *>(block->symbolDef());
						sym->insert ( block->symbolDef(), cls->insertPos );
					}
					break;
				default:
					sym->push ( block->symbolDef() );
					break;
			}
			break;
		case astOp::btInline:
			block->inlineFunc().node = simpleDCEliminateBlock ( block->inlineFunc().node, labels, sym );
			if	(	block->inlineFunc().node->getOp () == astOp::btBasic &&
					block->inlineFunc().node->basicData().blocks[1]->basicData().blocks.size () == 3 &&
					block->inlineFunc().node->basicData().blocks[1]->basicData().blocks[0]->getOp () == astOp::symbolDef &&
					block->inlineFunc().node->basicData().blocks[1]->basicData().blocks[1]->getOp () == astOp::btInlineRet && 
					!block->inlineFunc ().node->basicData ().blocks[1]->basicData ().blocks[1]->returnData().nPop
				)
			{
				auto ret = block->inlineFunc ().node->basicData ().blocks[1]->basicData ().blocks[1]->returnData ().node;
				block->inlineFunc().node->basicData().blocks[1]->basicData().blocks[1]->node() = nullptr;
				delete block;
				block = ret;
			}
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			// this is a do-nothing return.   we've optimized out the jmp, the pop and it has no value so can be entirely removed
			if ( !block->returnData().label.size () &&
				 !block->returnData().node &&
				 !block->returnData().nPop )
			{
				transformMade = true;
				delete block;
				return nullptr;
			}
			break;
		case astOp::btLabel:
			assert ( !block->left );
			assert ( !block->right );
			if ( !labels[block->label()].second )
			{
				delete block;
				return nullptr;
			}
			break;
		case astOp::btGoto:
		case astOp::btBreak:
		case astOp::btCont:
			assert ( !block->left );
			assert ( !block->right );
			break;
		case astOp::btTry:
			{
				block->tryCatchData().tryBlock = simpleDCEliminateBlock ( block->tryCatchData().tryBlock, labels, sym );
				block->tryCatchData().finallyBlock = simpleDCEliminateBlock ( block->tryCatchData().finallyBlock, labels, sym );
				if ( !block->tryCatchData().tryBlock && !block->tryCatchData().finallyBlock )
				{
					transformMade = true;
					delete block;
					return nullptr;
				}
				block->tryCatchData().catchBlock = simpleDCEliminateBlock ( block->tryCatchData().catchBlock, labels, sym );
			}
			break;
		case astOp::btFor:
			// emit entry condition code
			if ( block->loopData().condition )
			{
				if ( block->loopData().condition->isFalse () )
				{
					astNode *newBlock;

					// for loop will never exit... transform the for loop into a simple expr for the initializers and delete the old for loop
					newBlock = (new astNode ( file->sCache, astOp::btBasic ))->setLocation ( block );
					newBlock->pushNode ( block->loopData().initializers );

					if ( doesBlockHaveNeededLabel ( block->loopData().block, labels ) )
					{
						newBlock->pushNode ( block->loopData().block );
						block->loopData().block = nullptr;
					}
					transformMade = true;
					delete block;
					return newBlock;
				}
				if ( block->loopData().condition->isTrue () )
				{
					// useless condition... always true
					transformMade = true;
					delete block->loopData().condition;
					block->loopData().condition = 0;
				}
				block->loopData().block = simpleDCEliminateBlock ( block->loopData().block, labels, sym );
			} else
			{
				// no condition... loop forever
				block->loopData().block = simpleDCEliminateBlock ( block->loopData().block, labels, sym );
				if ( !block->loopData().block )
				{
					if ( !block->loopData().increase )
					{
						transformMade = true;
						delete block;
						return nullptr;
					}
				}
			}
			break;
		case astOp::btForEach:
			if ( !block->forEachData().statement )
			{
				transformMade = true;
				delete block;
				return nullptr;
			}
			break;
		case astOp::btIf:
			{
				astIf  newList;
				bool   tmpEmit = true;
				for ( auto it = block->ifData().ifList.begin (); it != block->ifData().ifList.end (); it++ )
				{
					(*it)->condition = simpleDCEliminateBlock ( (*it)->condition, labels, sym );
					(*it)->block = simpleDCEliminateBlock ( (*it)->block, labels, sym );

					if ( !(*it)->block )
					{
						if ( (*it)->condition->hasNoSideEffects ( sym, false ) && (it + 1 == block->ifData().ifList.end ()) )
						{
							transformMade = true;
							// do nothing... don't push it.. useless condition
							continue;
						}
					}
					if ( !tmpEmit || ((*it)->condition && (*it)->condition->isFalse () && (*it)->condition->hasNoSideEffects ( sym, false )) )
					{
						if ( !(*it)->condition->hasNoSideEffects ( sym, false ) )
						{
							newList.ifList.push_back ( (*it) );
						} else if ( doesBlockHaveNeededLabel ( (*it)->block, labels ) )
						{
							// we won't emit any conditional check unless it has side effects... we'll just jump around the condition
							// but we will emit the block...
							if ( (*it)->condition && !(*it)->condition->hasNoSideEffects ( sym, false ) )
							{
								transformMade = true;
								delete (*it)->condition;
								(*it)->condition = 0;
							}
							newList.ifList.push_back ( (*it) );
						} else
						{
							// do nothing... don't push it as it will be deleted
							// can never be executed (condition is always false), and the 
						}
					} else if ( (*it)->condition && (*it)->condition->isTrue () )
					{
						newList.ifList.push_back ( (*it) );
						tmpEmit = false;
					} else
					{
						newList.ifList.push_back ( (*it) );
					}
				}

				astIf  newList2;
				for ( auto &it : newList.ifList )
				{
					if ( it->condition )
					{
						newList2.ifList.push_back ( it );
					}
				}
				for ( auto &it : newList.ifList )
				{
					if ( !it->condition )
					{
						newList2.ifList.push_back ( it );
					}
				}
				std::swap ( newList.ifList, newList2.ifList );
				newList2.ifList.clear ();

				block->ifData().ifList.swap ( newList.ifList );
				newList.ifList.clear ();

				if ( block->ifData().elseBlock )
				{
					// if tmpEmit is false, than we have a previous block in the if that always evaluates to true the else will never be executed
					if ( !tmpEmit )
					{
						if ( !doesBlockHaveNeededLabel ( block->ifData().elseBlock, labels ) )
						{
							transformMade = true;
							delete block->ifData().elseBlock;
							block->ifData().elseBlock = 0;
						} else
						{
							block->ifData().elseBlock = simpleDCEliminateBlock ( block->ifData().elseBlock, labels, sym );
						}
					} else
					{
						block->ifData().elseBlock = simpleDCEliminateBlock ( block->ifData().elseBlock, labels, sym );
					}
				}
			}
			if ( !block->ifData().ifList.size () && !block->ifData().elseBlock )
			{
				transformMade = true;
				delete block;
				return nullptr;
			}
			break;
		case astOp::btWhile:
			if ( block->loopData().condition )
			{
				if ( block->loopData().condition->isFalse () && block->loopData().condition->hasNoSideEffects ( sym, false ) )
				{
					if ( !doesBlockHaveNeededLabel ( block->loopData().block, labels ) )
					{
						transformMade = true;
						delete block;
						return nullptr;
					}
				}
				if ( block->loopData().condition->isTrue () && block->loopData().condition->hasNoSideEffects ( sym, false ) )
				{
					transformMade = true;
					delete block->loopData().condition;
					block->loopData().condition = 0;
				}
			}
			block->loopData().condition = simpleDCEliminateBlock ( block->loopData().condition, labels, sym );

			if ( block->loopData().block )
			{
				block->loopData().block = simpleDCEliminateBlock ( block->loopData().block, labels, sym );
			}
			break;
		case astOp::btRepeat:
			block->loopData().block = simpleDCEliminateBlock ( block->loopData().block, labels, sym );
			if ( block->loopData().condition->isTrue () && block->loopData().condition->hasNoSideEffects ( sym, false ) )
			{
				transformMade = true;
				delete block->loopData().condition;
				block->loopData().condition = 0;
			}
			break;
		case astOp::btSwitch:
			if ( block->switchData().condition->isValue () )
			{
				astCondBlock *defaultBlock = nullptr;
				for ( auto &it : block->switchData().caseList )
				{
					if ( it->condition )
					{
						if ( *it->condition == *block->switchData().condition )
						{
							// TODO: convert breaks to jumps to a new tmp label
							// ensure that we're either a break AND there are no other embedded breaks
							if	( it->block->basicData().blocks.back () == it->block->firstBreak ( sym ) )
							{
								// TODO: handle fall thorugh case... this would NOT have a trailing break... need to examine follow on blocks to combine and remove trailing breaks...
								auto node = it->block;
								it->block = nullptr;
								delete block;

								// remove trailing break;
								delete node->basicData().blocks.back ();
								node->basicData().blocks.pop_back ();
								return simpleDCEliminateBlock ( node, labels, sym );;
							}
							// handle block always returning
							if ( it->block->alwaysReturns () )
							{
								auto node = it->block;
								it->block = nullptr;
								delete block;
								return node;// simpleDCEliminateBlock ( node, labels, sym );;
							}
						}
					} else
					{
						defaultBlock = it;
					}
				}
				if ( defaultBlock )
				{
					auto node = defaultBlock->block;
					defaultBlock->block = nullptr;
					delete block;
					return node;
				}
			} else
			{
				for ( auto &it : block->switchData().caseList )
				{
					if ( it->block )
					{
						it->block = simpleDCEliminateBlock ( it->block, labels, sym );
					}
					if ( it->condition )
					{
						it->condition = simpleDCEliminateBlock ( it->condition, labels, sym );
					}
				}
			}
			break;
		case astOp::assign:
			block->left = simpleDCEliminateBlock ( block->left, labels, sym );
			block->right = simpleDCEliminateBlock ( block->right, labels, sym );
			if ( block->left->getOp () == astOp::symbolValue )
			{
				switch ( sym->getType ( block->left->symbolValue (), false ) )
				{
					case symbolType::symVariant:
					case symbolType::symObject:
						break;
					default:
						if ( !sym->isAccessed ( block->left->symbolValue (), false ) )
						{
							// remove the unneeded assignment
							auto ret = block->right;
							block->right = nullptr;
							delete block;
							return ret;
						}
						break;
				}
			}
			break;
		case astOp::conditional:
			block->conditionData().trueCond = simpleDCEliminateBlock ( block->conditionData().trueCond, labels, sym );
			block->conditionData().falseCond = simpleDCEliminateBlock ( block->conditionData().falseCond, labels, sym );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( size_t loop = 0; loop < block->arrayData().nodes.size (); loop++ )
			{
				block->arrayData().nodes[loop] = simpleDCEliminateBlock ( block->arrayData().nodes[loop], labels, sym );
				if ( !block->arrayData().nodes[loop] )
				{
					block->arrayData().nodes[loop] = new astNode ( file->sCache, astOp::nullValue );
				}
			}
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( size_t loop = 0; loop < block->pList().param.size (); loop++ )
			{
				block->pList().param[loop] = simpleDCEliminateBlock ( block->pList().param[loop], labels, sym );
				if ( !block->pList().param[loop] )
				{
					block->pList().param[loop] = new astNode ( file->sCache, astOp::nullValue );
				}
			}
			return block;

		case astOp::pairValue:	/* do nothing for pair's... we need to have a case astOp::so we don't hit the default and eliminate a needed value*/
		case astOp::comp:		/* do nothing for compilation values... these are runtime evaluated and may have sideffects regardless of usage*/
		default:
			break;
	}
	if ( block->left ) block->left = simpleDCEliminateBlock ( block->left, labels, sym );
	if ( block->right ) block->right = simpleDCEliminateBlock ( block->right, labels, sym );
	return (block);
}

void optimizer::dcEliminateFunction ( opFunction *func )
{
	if ( func->isInterface || !func->codeBlock || func->isIterator ) return;

	symbolStack	sym ( file, func );
	func->labelList.clear ();
	func->labelList[compDef->file->retValue].second = true;
	func->labelList[compDef->file->barrierValue].second = true;

	if ( !((func->codeBlock->getOp () == astOp::btBasic) && !func->codeBlock->basicData().blocks.size ()) )
	{
		makeLabelList ( func->codeBlock, func->labelList );
		func->codeBlock = simpleDCEliminateBlock ( func->codeBlock, func->labelList, &sym );
	}
}



void optimizer::simpleDCEliminate ()
{
	// compile the functions
	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		errorLocality e ( &file->errHandler, (*it).second->location );
		dcEliminateFunction ( (*it).second );
	}
}

