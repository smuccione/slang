/* ****************************************************
	converts:

	iterator getCount ( start, stop )
	{
		for ( loop = start; loop < stop; loop++ )
		{
			yield loop;
		}
	}

	into

	class _getCount
	{
		local	start
		local	stop
		local	state
		local	_current
		local	count_1

		method new ()
		{
			state = 0;
		}

		method moveNext()
		{
			switch ( state )
			{
				case 0:
					goto label_0;
				case 1:
					goto label_1;
				default:
					return false
			}

			label label_0;

			count_1 = start
			for ( count_1 = start; count_1 < stop; count_1++ )
			{
				state = 1;
				_current = count_1
				return true
				label label_1;
			}
			state = -1;
			return false;
		}

		method getEnumerator()
		{
			return self
		}

		access current()
		{
			return _current
		}
	
	}

	function getCount ( start, stop )
	{
		local o = new _getCount();
		o.start = start;
		o.stop = stop;
		return o;
	}

**************************************************** */

#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "Utility/counter.h"

struct  localCnt {
	std::vector<uint64_t>	indexStack;
	uint64_t				maxIndex = 0;
	bool					noCnt;

	localCnt ( bool noCnt = false ) : noCnt ( noCnt )
	{
		indexStack.push_back ( maxIndex );
	}
	void push()
	{
		maxIndex++;
		indexStack.push_back ( maxIndex );
	}
	void pop()
	{
		indexStack.pop_back();
	}
	uint64_t current ( void )
	{
		return indexStack.back();
	}
	uint64_t cnt ( void )
	{
		return maxIndex;
	}
	size_t size ( )
	{
		return indexStack.size ( );
	}
	void resize ( size_t size )
	{
		indexStack.resize ( size );
	}
	stringi getExtension ( void )
	{
		if ( noCnt )
		{
			return "";
		} else
		{
			return stringi ( "$") + current();
		}
	}
};

static astNode *hasNVReturnCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, bool &hasNVReturn )
{
	switch ( node->getOp() )
	{
		case astOp::btReturn:
			if ( node->returnData().isYield )
			{
				if ( node->returnData().node->getOp() == astOp::seq )
				{
					hasNVReturn = true;
				}
			}
			break;
		default:
			break;
	}
	return node;
}

bool hasNVReturn( astNode *block, symbolStack *sym )
{
	bool present = false;
	astNodeWalk ( block, sym, hasNVReturnCB, present );

	return present;
}

// processReturn is used to denote lambda processing.   This routine should be renamed...
static void compMakeIterator ( compExecutable *comp, opFunction *func, opClass *oClass, astNode *node, uint64_t *nReturn, std::unordered_map<cacheString, localCnt> *symbols, symbolStack *sym, bool isAssign, bool processReturn, bool doesReturnIndex, std::vector<symbol *> &cleanupSymbols )
{
	assert ( processReturn );
	if ( !node ) return;
	switch ( node->getOp() )
	{
		case astOp::funcValue:
			assert ( false );
			break;
		case astOp::sendMsg:
			if ( node->left ) compMakeIterator ( comp, func, oClass, node->left, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->right )
			{
				if ( node->right->getOp() != astOp::nameValue )
				{
					if ( node->right ) compMakeIterator ( comp, func, oClass, node->right, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
				}
			}
			break;

		case astOp::symbolValue:
			if ( memcmp ( node->symbolValue(), "::", 2 ) != 0 )
			{
				auto symName = symbols->find ( node->symbolValue() );
				if ( symName == symbols->end() )
				{
					if ( sym->isDefined ( node->symbolValue(), true ) )
					{
						switch ( sym->getScope ( node->symbolValue(), isAssign )  )
						{
							case symbolSpaceScope::symLocal:
								/* do nothing... all symbols that we care about (e.g. not parameters) will have been in the symbols table when their def was processed, if not it's a parameter
								   if it is in the symbols table it simply gets handled by the else down below
								*/
								break;
							case symbolSpaceScope::symClassConst:
							case symbolSpaceScope::symClassStatic:
								node->symbolValue () = comp->file->sCache.get ( (stringi)func->classDef->name + "::" + (stringi) node->symbolValue () );
								node->symbolValue () = sym->getResolvedName ( node->symbolValue (), true );
								break;
							case symbolSpaceScope::symClassAssign:
							case symbolSpaceScope::symClassAccess:
							case symbolSpaceScope::symClassIVar:
							case symbolSpaceScope::symClassInherit:
								{
									*node = astNode ( comp->file->sCache,
														astOp::sendMsg,
														(new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->_selfValue ))->setLocation ( node ),
														(new astNode ( comp->file->sCache, astOp::nameValue, node->symbolValue () ))->setLocation ( node )
													);
								}
								break;
							case symbolSpaceScope::symClassMethod:
								if ( sym->isStatic ( node->symbolValue(), true ) || sym->isExtension ( node->symbolValue(), true ) )
								{
									node->symbolValue () = comp->file->sCache.get ( (stringi)func->classDef->name + "::" + (stringi)node->symbolValue () );
									node->symbolValue () = sym->getResolvedName ( node->symbolValue (), true );
								} else
								{
									*node = astNode ( comp->file->sCache,
													  astOp::sendMsg,
													  (new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->_selfValue ))->setLocation ( node ),
													  (new astNode ( comp->file->sCache, astOp::nameValue, node->symbolValue () ))->setLocation ( node )
									);
								}
								break;
							default:
								break;
						}
					}
				} else
				{
					node->symbolValue () = comp->file->sCache.get ( (stringi) node->symbolValue () + symbols->find ( node->symbolValue () )->second.getExtension () );
				}
			}
			break;
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass*>(node->symbolDef());
						sym->insert( node->symbolDef(), cls->insertPos );
					}
					break;
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeField:
					sym->push ( node->symbolDef() );
					break;
				case symbolSpaceType::symTypeLocal:
					// TODO: we DON'T need to turn EVERY symbol into an instance variable... block locals for which we don't yield inside the block can be left as regular locals.
					//       we would still need to rename/convert all other symbols inside that block, but new definitions do not need to be converted

					{
						astNode *init = node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName ( ) );

						if ( init->getOp() == astOp::assign )
						{
							compMakeIterator ( comp, func, oClass, init->right, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
						}

						if ( symbols->find ( node->symbolDef()->getSymbolName ( ) ) == symbols->end ( ) )
						{
							(*symbols)[node->symbolDef()->getSymbolName ( )] = localCnt ( );
						}
						symbols->find ( node->symbolDef()->getSymbolName ( ) )->second.push ( );

						cacheString newName;
						if ( init->getOp () == astOp::assign )
						{
							newName = comp->file->sCache.get ( (stringi)init->left->symbolValue () + symbols->find ( node->symbolDef ()->getSymbolName () )->second.getExtension () );
						}

						/* we're changing this to an expression or a dummy op... but we need to delete the symbol at some point
						   we can't do that now because we need it, so we save it to a vector which is cleaned up after makeIterator completes
						*/
						cleanupSymbols.push_back ( node->symbolDef() );
						/* need to rename these so that they have unique values... have to worry about this in case astOp::we have definitions with the same name */
						sym->push ( node->symbolDef() );
						node->symbolDef () = nullptr;
						node->release ( );

						if ( init->getOp () == astOp::assign )
						{
							// as part of a make iterator, we will no longer have any LOCAL symbol def's.   Any local symbols become instance variables in the surrounding class.
							// because of this we need to change the op type from a symbolDef into an expr.
							*node = *init;

							// can't simply us the init as the assignment string... we need to rename the LHS to be whatever the proper class ivar symbol name should be
							node->left->symbolValue() = newName;
						}
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(node->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							compMakeIterator ( comp, func, oClass, (*symbol)[it]->initializer, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
						}
						sym->push ( symbol );
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym->size ( );
				std::vector<std::pair<cacheString, size_t>>	symState;
				symState.reserve ( symbols->size ( ) );

				for ( auto &it : *symbols )
				{
					symState.push_back ( std::make_pair<> ( it.first, it.second.size() ) );
				}

				for ( auto &it : node->basicData().blocks )
				{
					compMakeIterator ( comp, func, oClass, it, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
				}
				for ( auto &it : symState )
				{
					(*symbols)[it.first].resize ( it.second );
				}
				sym->resize ( size );
			}
			break;
		case astOp::btInlineRet:
			// this MUST be done before inlining!   an iterator needs to be converted to an object and this is part of that conversion
			assert ( false );
			throw errorNum::scINTERNAL;
		case astOp::btInline:
			compMakeIterator ( comp, func, oClass, node->inlineFunc().node, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::btLabel:
		case astOp::btGoto:
			break;
		case astOp::btReturn:
			if ( !node->returnData().isYield )
			{
				if ( processReturn )
				{
					astNode *item;
					astNode block = astNode ( comp->file->sCache, astOp::btBasic );

					item = (new astNode ( comp->file->sCache, astOp::assign ))->setLocation ( node );
					item->left = (new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->sCache.get ( "$state" ) ))->setLocation ( node );
					item->right = (new astNode ( comp->file->sCache, astOp::intValue, (int64_t)-1 ))->setLocation ( node );
					block.pushNode( item );

					// 		 return false;
					item = (new astNode ( comp->file->sCache, astOp::btReturn ))->setLocation ( node );
					item->returnData().isYield = true;
					item->returnData().node = (new astNode ( comp->file->sCache, astOp::intValue, (int64_t)0 ))->setLocation ( node );
					block.pushNode( item );

					node->setOp ( comp->file->sCache, astOp::btBasic );
					node->basicData().blocks.swap( block.basicData().blocks );
				} else
				{
					// here we're doing lambda closure processing so we need to change symbols, but DONT modify the return
					compMakeIterator( comp, func, oClass, node->returnData().node, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
				}
			} else
			{
				// we must always process in case astOp::we're doing lambda closure handling
				compMakeIterator( comp, func, oClass, node->returnData().node, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );

				if ( processReturn )
				{
					astNode block = astNode ( comp->file->sCache, astOp::btBasic );
					astNode *item;

					(*nReturn)++;

					if ( !doesReturnIndex )
					{
						// 		 $current = x;
						item = (new astNode ( comp->file->sCache, astOp::assign ))->setLocation ( node );
						item->left = (new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->sCache.get ( "$current" ) ))->setLocation ( node );
						if ( node->returnData().node )
						{
							item->right = node->returnData().node;
							node->returnData().node = nullptr;
						} else
						{
							item->right = ((new astNode ( comp->file->sCache, astOp::nullValue )))->setLocation ( node );
						}
						block.pushNode( item );
					} else
					{
						if ( !node->returnData().node || node->returnData().node->getOp() != astOp::seq ) throw errorNum::scFOR_EACH_RETURN;

						// 		 current$0 = x;
						item = (new astNode ( comp->file->sCache, astOp::assign ))->setLocation ( node );
						item->left = (new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->sCache.get ( "$current" ) ))->setLocation ( node );
						item->right = node->returnData().node->left;
						block.pushNode( item );

						// 		 index$0 = x;
						item = (new astNode ( comp->file->sCache, astOp::assign ))->setLocation ( node );
						item->left = (new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->sCache.get ( "$index" ) ))->setLocation ( node );
						item->right = node->returnData().node->right;
						block.pushNode( item );

						node->returnData().node->left = 0;
						node->returnData().node->right = 0;
					}

					// 		 state = nReturn;
					item = (new astNode ( comp->file->sCache, astOp::assign ))->setLocation ( node );
					item->left = (new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->sCache.get ( "$state" ) ))->setLocation ( node );
					item->right = (new astNode ( comp->file->sCache, astOp::intValue, (int64_t)*nReturn ))->setLocation ( node );
					block.pushNode( item );

					// 		 return true;
					item = (new astNode ( comp->file->sCache, astOp::btReturn ))->setLocation ( node );
					item->returnData().label = node->returnData().label;
					item->returnData ().isYield = true;
					item->returnData().node = (new astNode ( comp->file->sCache, astOp::intValue, (int64_t)1 ))->setLocation ( node );
					block.pushNode( item );

					// 		 label
					item = (new astNode ( comp->file->sCache, astOp::btLabel ))->setLocation ( node );
					item->label () = comp->file->sCache.get ( stringi ( "$resume$" ) + *nReturn );
					block.pushNode( item );

					node->setOp ( comp->file->sCache, astOp::btBasic );
					node->basicData().blocks.swap( block.basicData().blocks );
				}
			}
			break;
		case astOp::btBreak:
		case astOp::btCont:
			break;
		case astOp::btTry:
			compMakeIterator ( comp, func, oClass, node->tryCatchData().tryBlock, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->tryCatchData().catchBlock ) compMakeIterator ( comp, func, oClass, node->tryCatchData().catchBlock, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->tryCatchData().finallyBlock ) compMakeIterator ( comp, func, oClass, node->tryCatchData().finallyBlock, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::btForEach:
			// must be run after compMakeIterator() which will remove this block;
			throw errorNum::scINTERNAL;
		case astOp::btFor:
			if ( node->loopData().initializers ) compMakeIterator ( comp, func, oClass, node->loopData().initializers, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->loopData().condition ) compMakeIterator ( comp, func, oClass, node->loopData().condition, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->loopData().block ) compMakeIterator ( comp, func, oClass, node->loopData().block, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->loopData().increase ) compMakeIterator ( comp, func, oClass, node->loopData().increase, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::btIf:
			for ( auto it = node->ifData().ifList.begin(); it != node->ifData().ifList.end(); it++ )
			{
				if ( (*it)->condition) compMakeIterator ( comp, func, oClass, (*it)->condition, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
				if ( (*it)->block ) compMakeIterator ( comp, func, oClass, (*it)->block, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			}
			if ( node->ifData().elseBlock ) compMakeIterator ( comp, func, oClass, node->ifData().elseBlock, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::btWhile:
			if ( node->loopData().condition ) compMakeIterator ( comp, func, oClass, node->loopData().condition, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->loopData().block ) compMakeIterator ( comp, func, oClass, node->loopData().block, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::btRepeat:
			if ( node->loopData().block ) compMakeIterator ( comp, func, oClass, node->loopData().block, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			if ( node->loopData().condition ) compMakeIterator ( comp, func, oClass, node->loopData().condition, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::btSwitch:
			for ( auto it = node->switchData().caseList.begin(); it != node->switchData().caseList.end(); it++ )
			{
				if ( (*it)->condition) compMakeIterator ( comp, func, oClass, (*it)->condition, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
				if ( (*it)->block ) compMakeIterator ( comp, func, oClass, (*it)->block, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			}
			compMakeIterator ( comp, func, oClass, node->switchData().condition, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::conditional:
			compMakeIterator ( comp, func, oClass, node->conditionData().trueCond, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			compMakeIterator ( comp, func, oClass, node->conditionData().falseCond, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			break;
		case astOp::funcCall:
			if ( node->left->getOp() == astOp::symbolValue )
			{
				if ( sym->isDefined ( node->left->symbolValue(), true ) )
				{
					switch ( sym->getScope ( node->left->symbolValue(), isAssign ) )
					{
						case symbolSpaceScope::symLocal:
							if ( symbols->find ( node->left->symbolValue() ) != symbols->end ( ) )
							{
								node->left->symbolValue () = comp->file->sCache.get ( (stringi) node->left->symbolValue () + symbols->find ( node->left->symbolValue () )->second.getExtension () );
							}
							break;
						default:
							break;
					}
				}
			} else if ( node->left->getOp() == astOp::atomValue && node->left->symbolValue() != comp->file->newValue )
			{
				if ( sym->isDefined ( node->left->symbolValue(), true ) )
				{
					switch ( sym->getScope ( node->left->symbolValue(), isAssign )  )
					{
						case symbolSpaceScope::symClassMethod:
							{
								node->left->right = ((new astNode ( comp->file->sCache, astOp::nameValue, node->left->symbolValue() )))->setLocation ( node );
								node->left->left = ((new astNode ( comp->file->sCache, astOp::symbolValue, comp->file->_selfValue  )))->setLocation ( node );
								node->left->setOp ( comp->file->sCache, astOp::sendMsg );
							}
							break;
						default:
							break;
					}
				}
			} else
			{
				compMakeIterator ( comp, func, oClass, node->left, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			}
			for ( auto it = node->pList().param.begin(); it != node->pList().param.end(); it++ )
			{
				if ( (*it) ) compMakeIterator ( comp, func, oClass, (*it), nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto it = node->pList().param.begin(); it != node->pList().param.end(); it++ )
			{
				if ( (*it) ) compMakeIterator ( comp, func, oClass, (*it), nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			}
			break;
		case astOp::symbolPack:
		case astOp::varrayValue:
		case astOp::arrayValue:
			for ( auto it = node->arrayData().nodes.begin(); it != node->arrayData().nodes.end(); it++ )
			{
				compMakeIterator ( comp, func, oClass, (*it), nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
			}
			break;
		default:
			break;
	}
	if ( node->left )
	{
		switch ( node->getOp() )
		{
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
			case astOp::postInc:
			case astOp::postDec:
			case astOp::preInc:
			case astOp::preDec:
				compMakeIterator ( comp, func, oClass, node->left, nReturn, symbols, sym, true, processReturn, doesReturnIndex, cleanupSymbols );
				break;
			default:
				compMakeIterator ( comp, func, oClass, node->left, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
				break;
		}
	}
	if ( node->right )
	{
		switch ( node->getOp() )
		{
			case astOp::sendMsg:
				break;
			default:
				compMakeIterator ( comp, func, oClass, node->right, nReturn, symbols, sym, false, processReturn, doesReturnIndex, cleanupSymbols );
				break;
		}
	}
}
	
/*
function getCount ( start, stop )
	local o = new _getCount();
	o.start = start;
	o.stop = stop;
	return o;
end
*/

//TODO: make namespace compatible
bool compExecutable::compMakeIterator ( opFile *file, opFunction *func, bool generic, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS )
{
	if ( !func->isIterator ) return false;
	if ( func->isInterface ) return false;

	char const *expr;
	astNode *item;
	opFunction *newFunc;
	opClassElement *elem;
	bool doesReturnIndex = false;
	bool isExtension = false;;

	// so that we can identify what needs to be put into the class
	func->isIterator = false;

	bool isStatic = func->isStatic;

	if ( func->classDef )
	{
		isStatic = func->isStatic;
		isExtension = func->isExtension;
	}

	makeSymbols ( func, false );

	opClass *itClass = new opClass ();

	itClass->isInterface = false;
	itClass->usingList = file->ns.getUseList ();
	itClass->name = file->sCache.get ( (stringi) func->name + "." + GetUniqueCounter () );
	itClass->location = func->location;
	itClass->nameLocation = func->nameLocation;

	symbolStack sym ( file, func );
	std::unordered_map<cacheString, localCnt> symbols;
	uint64_t nReturn = 0;

	if ( func->classDef )
	{
		markClassInuse ( this, acc, func->classDef, &sym, scanQueue, false );
	}

	doesReturnIndex = hasNVReturn( func->codeBlock, &sym );

	symbols[file->sCache.get ( "$state" ) ] = localCnt ( true );
	symbols[file->sCache.get ( "$current" ) ] = localCnt ( true );
	if ( doesReturnIndex )
	{
		symbols[file->sCache.get ( "$index" )] = localCnt( true );
	}

	for ( uint32_t loop = func->classDef && !isExtension && !isStatic ? 1 : 0; loop < func->params.size(); loop++ )
	{
		symbols[func->params[loop]->getName()] = localCnt ( true );
	}
	
	std::vector<symbol *> cleanupSymbols;

	::compMakeIterator ( this, func, itClass, func->codeBlock, &nReturn, &symbols, &sym, false, true, doesReturnIndex, cleanupSymbols );

	for ( auto &it : cleanupSymbols )
	{
		delete it;
	}

	for ( auto it = symbols.begin(); it != symbols.end(); it++ )
	{
		if ( (*it).second.noCnt )
		{
			opClassElement *opElem = new opClassElement();
			opElem->isVirtual				= false;
			opElem->isStatic				= false;
			opElem->type					= fgxClassElementType::fgxClassType_iVar;
			opElem->scope					= fgxClassElementScope::fgxClassScope_public;
			opElem->location				= func->location;
			opElem->name					= (*it).first;
			opElem->data.iVar.initializer	= (new astNode ( file->sCache, astOp::symbolValue, opElem->name ))->setLocation ( func->location );
			itClass->elems.push_back ( opElem );
		} else
		{
			for ( uint64_t cnt = 1; cnt <= (*it).second.cnt(); cnt++ )
			{
				auto opElem	= new opClassElement();
				opElem->isVirtual				= false;
				opElem->isStatic				= false;
				opElem->type					= fgxClassElementType::fgxClassType_iVar;
				opElem->scope					= fgxClassElementScope::fgxClassScope_public;
				opElem->location				= func->location;
				opElem->name					= file->sCache.get ( (stringi) (*it).first + "$" + cnt );
				opElem->data.iVar.initializer	= (new astNode ( file->sCache, astOp::symbolValue, opElem->name ))->setLocation ( func->location );
				itClass->elems.push_back ( opElem );
			}
		}
	}

	if ( func->isExtension )
	{
		for ( auto &it : itClass->elems )
		{
			if ( it->name == func->params[0]->getName () )
			{
				if ( !generic && !isLS )		// if were an LS we not going to monomorphize so we MUST not force a type here
				{
					it->symType = symbolTypeClass ( symbolType::symObject, func->classDef->name );
				} else
				{
					it->symType = symbolTypeClass ( symbolType::symVariant );
				}
				break;
			}
		}
	}

	elem = new opClassElement ();
	elem->isVirtual = false;
	elem->isStatic = false;
	elem->type = fgxClassElementType::fgxClassType_inherit;
	elem->scope = fgxClassElementScope::fgxClassScope_public;
	elem->name = file->queryableValue;
	itClass->elems.push_back ( elem );

	if ( func->classDef && !isStatic && !isExtension )
	{
		auto opElem = new opClassElement();
		opElem->isVirtual				= false;
		opElem->isStatic				= false;
		opElem->type					= fgxClassElementType::fgxClassType_iVar;
		opElem->scope					= fgxClassElementScope::fgxClassScope_public;
		opElem->location				= func->location;
		opElem->name					= file->_selfValue;
		opElem->data.iVar.initializer	= (new astNode ( file->sCache, astOp::symbolValue, opElem->name ))->setLocation ( func->location );
		itClass->elems.push_back ( opElem );
	}

	/**************************   new method   **********************/

	elem = new opClassElement();
	elem->isVirtual			= false;
	elem->isStatic			= false;
	elem->type				= fgxClassElementType::fgxClassType_method;
	elem->scope				= fgxClassElementScope::fgxClassScope_public;
	elem->name				= file->newValue;
	expr = "(){\r\nreturn\r\n}";
	{
		source src ( &file->srcFiles, file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none, func->location.lineNumberStart );

		// 		 return false;
		// state = -1
		item = (new astNode ( file->sCache, astOp::assign ))->setLocation ( src );
		item->left	= (new astNode ( file->sCache, astOp::symbolValue, file->sCache.get ( "$state" )))->setLocation ( src );
		item->right	= (new astNode ( file->sCache, astOp::intValue, (int64_t)0 ))->setLocation ( src );

		newFunc = file->parseMethod ( src, itClass, buildString ( itClass->name.c_str ( ), elem->name, "method" ).c_str ( ), true, false, false, srcLocation () );
	}
	file->functionList.insert( { newFunc->name, newFunc } );
	elem->data.method.func = newFunc->name;
	elem->data.method.virtOverrides.insert ( newFunc );
	newFunc->codeBlock->basicData().blocks.insert ( newFunc->codeBlock->basicData().blocks.begin(), item );
	newFunc->isInterface = false;
	newFunc->classDef = itClass;
	itClass->elems.push_back ( elem );

	/**************************   reset method   ***********************/

	elem = new opClassElement();
	elem->isVirtual			= false;
	elem->isStatic			= false;
	elem->type				= fgxClassElementType::fgxClassType_method;
	elem->scope				= fgxClassElementScope::fgxClassScope_public;
	elem->location			= func->location;
	elem->name = file->sCache.get ( "reset" );
	char tmp[1024] = {};
	expr = tmp;
	sprintf ( tmp, "(){\r\nexcept ( new systemError ( %u ) ); \r\nreturn\r\n}", uint32_t(errorNum::scRESET_NOT_APPLICABLE) );
	{
		source src ( &file->srcFiles, file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none, func->location.lineNumberStart );
		newFunc = file->parseMethod ( src, itClass, buildString ( itClass->name.c_str ( ), elem->name, "method" ).c_str ( ), true, false, false, srcLocation () );
	}
	file->functionList.insert( { newFunc->name, newFunc } );
	elem->data.method.func = newFunc->name;
	elem->data.method.virtOverrides.insert ( newFunc );
	newFunc->isInterface = false;
	newFunc->classDef = itClass;
	itClass->elems.push_back ( elem );

	/**************************   getEnumerator method   ***********************/


	//		return self
	elem	= new opClassElement();
	elem->isVirtual			= false;
	elem->isStatic			= false;
	elem->type				= fgxClassElementType::fgxClassType_method;
	elem->scope				= fgxClassElementScope::fgxClassScope_public;
	elem->name				= file->getEnumeratorValue;
	expr = "(){\r\nreturn self\r\n}";
	{
		source src ( &file->srcFiles, file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none, func->location.lineNumberStart );
		newFunc = file->parseMethod ( src, itClass, buildString ( itClass->name.c_str ( ), elem->name, "method" ).c_str ( ), true, false, false, srcLocation () );
	}
	file->functionList.insert( { newFunc->name, newFunc } );
	elem->data.method.func	= newFunc->name;
	elem->data.method.virtOverrides.insert ( newFunc );
	newFunc->isProcedure = false;
	newFunc->isInterface = false;
	newFunc->classDef = itClass;
	newFunc->retType = symbolTypeClass ( symbolType::symObject, itClass->name );
	itClass->elems.push_back ( elem );

	/**************************   current accessor ***********************/

	elem	= new opClassElement();
	elem->isVirtual			= false;
	elem->isStatic			= false;
	elem->type				= fgxClassElementType::fgxClassType_prop;
	elem->scope				= fgxClassElementScope::fgxClassScope_public;
	elem->name				=  file->sCache.get (  "current" );
	expr = "(){\r\nreturn\r\n}";
	{
		source src ( &file->srcFiles, file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none, func->location.lineNumberStart );
		newFunc = file->parseMethod ( src, itClass, buildString ( itClass->name.c_str ( ), elem->name, "access" ).c_str ( ), true, false, false, srcLocation () );
	}
	file->functionList.insert( { newFunc->name, newFunc } );
	elem->data.prop.access = newFunc->name;
	elem->data.prop.accessVirtOverrides.insert ( newFunc );
	newFunc->codeBlock->basicData().blocks[0]->returnData().node = (new astNode ( file->sCache, astOp::symbolValue, file->sCache.get ( "$current" ) ))->setLocation ( func->codeBlock );
	newFunc->isProcedure = false;
	newFunc->isInterface = false;
	newFunc->classDef = itClass;
	itClass->elems.push_back ( elem );

	if ( doesReturnIndex )
	{
		/**************************   index accessor ***********************/

		elem = new opClassElement();
		elem->isVirtual			= false;
		elem->isStatic			= false;
		elem->type				= fgxClassElementType::fgxClassType_prop;
		elem->scope				= fgxClassElementScope::fgxClassScope_public;
		elem->name				= file->sCache.get ( "index" );
		expr = "(){\r\nreturn\r\n}";
		{
			source src ( &file->srcFiles, file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none, func->location.lineNumberStart );
			newFunc = file->parseMethod ( src, itClass, buildString ( itClass->name.c_str (), elem->name, "access" ).c_str (), true, false, false, srcLocation () );
		}
		file->functionList.insert( { newFunc->name,newFunc } );
		elem->data.prop.access = newFunc->name;
		elem->data.prop.accessVirtOverrides.insert ( newFunc );
		newFunc->codeBlock->basicData().blocks[0]->returnData().node = (new astNode ( file->sCache, astOp::symbolValue, file->sCache.get ( "$index" ) ))->setLocation ( func->codeBlock );
		newFunc->isProcedure = false;
		newFunc->isInterface = false;
		newFunc->classDef = itClass;
		itClass->elems.push_back( elem );
	}

	/**************************   MoveNext method  ***********************/

	elem = new opClassElement ( );
	elem->isVirtual		= false;
	elem->isStatic		= false;
	elem->type			= fgxClassElementType::fgxClassType_method;
	elem->scope			= fgxClassElementScope::fgxClassScope_public;
	elem->location		= func->location;
	elem->name			= file->sCache.get ( "moveNext" );
	expr = "(){\r\nreturn\r\n}";
	{
		source src ( &file->srcFiles, file->sCache, file->srcFiles.getName ( func->location.sourceIndex ), expr, sourceFile::sourceFileType::none, func->location.lineNumberStart );
		newFunc = file->parseMethod ( src, itClass, buildString ( itClass->name.c_str ( ), elem->name, "method" ).c_str ( ), true, false, false, srcLocation () );
	}
	file->functionList.insert( { newFunc->name, newFunc } );
	elem->data.method.func = newFunc->name;
	elem->data.method.virtOverrides.insert ( newFunc );

	// attach it to our class
	newFunc->classDef		= itClass;
	newFunc->isInterface	= false;
	newFunc->isProcedure	= false;
	itClass->elems.push_back ( elem );

	auto block = newFunc->codeBlock;
	assert ( block->getOp() == astOp::btBasic );
	for ( auto& it : block->basicData().blocks )
	{
		delete it;
	}
	block->basicData().blocks.clear ( );		// clear out the dummy from our parse build above... we'll construct the real one below
	block->setLocation ( func->codeBlock );
	auto switchNode = (new astNode ( file->sCache, astOp::btSwitch ))->setLocation ( func->codeBlock );

	// switch ( $state )
	switchNode->switchData().condition = (new astNode ( file->sCache, astOp::symbolValue, file->sCache.get ( "$state" ) ) )->setLocation ( func->codeBlock );
	for ( uint64_t loop = 0; loop < nReturn + 1; loop++ )
	{
		astCondBlock *condBlock = new astCondBlock;
		// case astOp::<loop>;
		condBlock->condition = (new astNode ( file->sCache, astOp::intValue, (int64_t) loop ))->setLocation ( func->codeBlock );
		//    goto resume$<loop>
		condBlock->block	 = (new astNode ( file->sCache, astOp::btGoto ))->setLocation ( func->codeBlock );
		condBlock->block->gotoData ().label = file->sCache.get ( stringi ( "$resume$" ) + loop );
		switchNode->switchData().caseList.push_back ( condBlock );
	}

	// default
	astCondBlock *condBlock = new astCondBlock;
	condBlock->condition = 0;

	//  return 0;
	condBlock->block	 = (new astNode ( file->sCache, astOp::btReturn ))->setLocation ( func->codeBlock );
	condBlock->block->returnData().isYield = false;
	condBlock->block->returnData().node = (new astNode ( file->sCache, astOp::intValue, (int64_t) 0 ))->setLocation ( func->codeBlock );
	switchNode->switchData().caseList.push_back ( condBlock );

	block->pushNode ( switchNode );

	// label resume0;
	item = (new astNode ( file->sCache, astOp::btLabel ))->setLocation ( func->codeBlock );
	item->label () = file->sCache.get ( stringi ( "$resume$" ) + (uint64_t) 0 );
	block->pushNode (  item );

	// iterator function block	- this is the "guts", the real user-codeblock that was modified to use ivars rather than locals
	block->pushNode ( func->codeBlock );

	// state = -1
	item = (new astNode ( file->sCache, astOp::assign ))->setLocation ( func->codeBlock );
	item->left	= (new astNode ( file->sCache, astOp::symbolValue, file->sCache.get ( "$state" )))->setLocation ( func->codeBlock );
	item->right	= (new astNode ( file->sCache, astOp::intValue, (int64_t) -1 ))->setLocation ( func->codeBlock );
	block->pushNode ( item );

	// 		 return false;
	item = (new astNode ( file->sCache, astOp::btReturn ))->setLocation ( func->codeBlock );
	item->returnData().isYield = false;
	item->returnData().node = (new astNode ( file->sCache, astOp::intValue, (int64_t)0 ))->setLocation ( func->codeBlock );
	block->pushNode ( item );

	// add our class it to the atom table
	atom.add ( itClass->name.c_str (), atomListType::atomClass );

	/**************************   Iterator function itself ***********************/
	/* let's make function that will replace the code in our existing iterator with code that creates the iterator object*/
	BUFFER buff;
	bufferPrintf ( &buff, "local _o = new ( \"%s\" )\r\n", itClass->name.c_str() );

	for ( uint32_t loop = (func->classDef ? (isStatic || isExtension ? 0 : 1) : 0); loop < func->params.size(); loop++ )
	{
		bufferPrintf ( &buff, "_o.%s = %s\r\n", func->params[loop]->name.c_str(), func->params[loop]->name.c_str() );
	}
	if ( func->classDef && !isStatic && !isExtension )
	{
		bufferPrintf ( &buff, "_o._self = self\r\n" );
	}
	bufferPrintf ( &buff, "return _o\r\nend\r\n" );
	expr = bufferBuff ( &buff );
	{
		source src ( &file->srcFiles, file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none, func->location.lineNumberStart );
		func->codeBlock = file->parseBlock ( src, func, false, true, isLS, false, src );
	}
	
	func->isIterator = false;
	func->retType = symbolTypeClass ( symbolType::symObject, itClass->name );

	file->classList.insert( { itClass->name, itClass } );

	makeSymbols ( func, isLS );
	makeAtoms	( func, isLS );
	pruneRefs	( func );
	genWrapper	( func );

	if ( scanQueue ) scanQueue->push ( func );

	makeConstructorDestructors ( itClass, itClass->location.sourceIndex );
	if ( !makeExtensionMethod ( itClass, generic, acc, scanQueue ) )
	{
		makeClass ( itClass, isLS );
	}

	// the below have already been run on all other functions.   we need to do it on the newly created ones.
	// we can't simply move this until after iterator creation on the other functions as we have to have done this already in order to 
	// properly make the iterators in the first place.   it has not choice but to be multipass.

	makeSymbols ( itClass->cClass->getElement( file->sCache.get ( "reset" ) )->methodAccess.func, isLS );
	makeAtoms	( itClass->cClass->getElement( file->sCache.get ( "reset" ) )->methodAccess.func, isLS );
	pruneRefs	( itClass->cClass->getElement( file->sCache.get ( "reset" ) )->methodAccess.func );
	genWrapper  ( itClass->cClass->getElement( file->sCache.get ( "reset" ) )->methodAccess.func );

	makeSymbols ( itClass->cClass->getElement( file->sCache.get ( "moveNext" ) )->methodAccess.func, isLS );
	makeAtoms	( itClass->cClass->getElement( file->sCache.get ( "moveNext" ) )->methodAccess.func, isLS );
	pruneRefs	( itClass->cClass->getElement( file->sCache.get ( "moveNext" ) )->methodAccess.func );
	genWrapper  ( itClass->cClass->getElement( file->sCache.get ( "moveNext" ) )->methodAccess.func );

	makeSymbols ( itClass->cClass->getElement( file->newValue )->methodAccess.func, isLS );
	makeAtoms	( itClass->cClass->getElement( file->newValue )->methodAccess.func, isLS );
	pruneRefs	( itClass->cClass->getElement( file->newValue )->methodAccess.func );
	genWrapper  ( itClass->cClass->getElement( file->newValue )->methodAccess.func );

	makeSymbols ( itClass->cClass->getElement( file->getEnumeratorValue )->methodAccess.func, isLS );
	makeAtoms	( itClass->cClass->getElement( file->getEnumeratorValue )->methodAccess.func, isLS );
	pruneRefs	( itClass->cClass->getElement( file->getEnumeratorValue )->methodAccess.func );
	genWrapper	( itClass->cClass->getElement( file->getEnumeratorValue )->methodAccess.func );

	makeSymbols ( itClass->cClass->getElement( file->sCache.get ( "current" ) )->methodAccess.func, isLS );
	makeAtoms	( itClass->cClass->getElement( file->sCache.get ( "current" ) )->methodAccess.func, isLS );
	pruneRefs	( itClass->cClass->getElement( file->sCache.get ( "current" ) )->methodAccess.func );
	genWrapper	( itClass->cClass->getElement( file->sCache.get ( "current" ) )->methodAccess.func );

	if ( doesReturnIndex )
	{
		makeSymbols	( itClass->cClass->getElement( file->sCache.get ( "index" ) )->methodAccess.func, isLS );
		makeAtoms	( itClass->cClass->getElement( file->sCache.get ( "index" ) )->methodAccess.func, isLS );
		pruneRefs	( itClass->cClass->getElement( file->sCache.get ( "index" ) )->methodAccess.func );
		genWrapper	( itClass->cClass->getElement( file->sCache.get ( "index" ) )->methodAccess.func );
	}

	file->buildSymbolSearch(false);

	return true;
}

void compExecutable::compMakeIterator ( bool generic, bool isLS )
{
	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		if ( (*it).second->isIterator )
		{
			try {
				errorLocality e ( &file->errHandler, (*it).second->location );
				compMakeIterator ( file, (*it).second, generic, accessorType(), nullptr, isLS );
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( isLS, err );
			}
		}
	}
}
