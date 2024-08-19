/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "compilerParser/classParser.h"
#include "bcVM/fglTypes.h"

// TODO: we can optimize creation of automatic symbols.
//		 if the basic block for which we're adding happens to be the basic block that is our direct parent then we can actually remove the assignement and in it's place
//		 create a symbolDef with the RHS of the assignment as the initalizer.  We can do this because the ONLY time the add point and the parent block are the same is if we're
//		 doing straight line code inside the uppermost block of the function and as such we know there will never be any prior uses of the symbol.   Of course, we can ONLY 
//		 do this if we don't have any goto's in the function (that may negate the prior statement)


// NOTE: make symbols MUST add any automatics BEFORE all local definitions... this is because a local definition's initializer may access, and therefore declare an automatic (which is initialized to null
//		 but still allowable 

void astNode::makeSymbols ( errorHandler *err, symbolStack *sym, astNode *addPt, symbolStack *parentSym, opFunction *func, compExecutable *comp, symbolSpaceLocalAdd *localAdd, symbolSpaceClosures *closureAdd, bool isLS, bool isAccess )
{
	size_t				loop;
	errorLocality		e ( err, this );

	switch ( op )
	{
		case astOp::funcValue:
			{
				opFunction *child;

				child = sym->file->functionList.find( symbolValue() )->second;
				if ( func ) child->classDef = func->classDef;

				std::vector<symbolLocal>	closures;
				symbolSpaceClosures childClosureAdd( &closures );
				symbolSpaceLocalAdd childLocalAdd;

				if ( func && child->codeBlock )
				{
					symbolStack funcSym( sym->file, child );

					funcSym.push( &childClosureAdd );
					funcSym.push( &childLocalAdd );

					child->codeBlock->makeSymbols( err, &funcSym, 0, sym, child, comp, &childLocalAdd, &childClosureAdd, isLS, true );
				}
				// free the old function definition
				release();

				// turn it into a function call
				*this = *astNode( sym->file->sCache, astOp::funcCall ).setLocation( this );

				auto className = comp->compMakeLambda( sym->file, child, closures, func ? func->isStatic : false, func && func->classDef ? func->isExtension : false, isLS );
				child->classDef = sym->file->classList.find( className )->second;

				pList().param.push_back ( new astNode( sym->file->sCache, astOp::nameValue, className ) );
				pList ().paramRegion.push_back ( *this );

				if ( func && func->classDef && !func->isStatic && !func->isExtension )
				{
					pList().param.push_back ( new astNode( sym->file->sCache, astOp::symbolValue, sym->file->selfValue ) );
					pList ().paramRegion.push_back ( *this );
				}

				// convert the closures into ref parameters
				for ( auto &it : closures )
				{
					pList().param.push_back ( new astNode( sym->file->sCache, astOp::refPromote, new astNode( sym->file->sCache, astOp::symbolValue, it.getSymbolName() ) ) );
					pList ().paramRegion.push_back ( *this );
				}

				left = (new astNode ( sym->file->sCache, astOp::atomValue, "new" ))->setLocation ( this );
			}
			break;
		case astOp::symbolDef:
			switch ( symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					{
						auto init = symbolDef()->getInitializer( symbolDef()->getSymbolName() );
						if ( init->getOp () == astOp::assign )
						{
							init->right->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
							symbolDef ()->setInitialized ( symbolDef ()->getSymbolName (), false );
						}
						sym->push( symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef());
						for ( uint32_t it = 0; it < symbol->size(); it++ )
						{
							auto init = (*symbol)[it]->initializer;
							if ( init->getOp() == astOp::assign )
							{
								symbolDef()->getInitializer( symbolDef()->getSymbolName() )->right->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
							}
						}
						sym->push( symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass *>(symbolDef());
						sym->insert( symbolDef(), cls->insertPos );
					}
					break;
				default:
					sym->push( symbolDef() );
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym->size();
				if ( !addPt )
				{
					astNode basic( sym->file->sCache, astOp::btBasic );

					for ( auto &it : basicData().blocks )
					{
						basic.pushNode( it );
						it->makeSymbols( err, sym, &basic, parentSym, func, comp, localAdd, closureAdd, isLS, true );
					}
					std::swap( basicData().blocks, basic.basicData().blocks );
					basic.basicData().blocks.clear();
				} else
				{
					for ( auto it = basicData().blocks.begin(); it != basicData().blocks.end(); it++ )
					{
						(*it)->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
					}
				}
				sym->resize( size );
			}
			break;
		case astOp::btSwitch:
			if ( switchData().condition ) switchData().condition->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			for ( auto it = switchData().caseList.begin(); it != switchData().caseList.end(); it++ )
			{
				if ( (*it)->condition ) (*it)->condition->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
				if ( (*it)->block ) (*it)->block->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData().ifList.begin(); it != ifData().ifList.end(); it++ )
			{
				if ( (*it)->condition ) (*it)->condition->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
				if ( (*it)->block ) (*it)->block->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			if ( ifData().elseBlock ) ifData().elseBlock->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
		case astOp::btInline:
			{
				errorLocality e ( err, this, true );
				if ( inlineFunc ().node ) inlineFunc ().node->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			break;
		case astOp::btFor:
			if ( loopData().initializers ) loopData().initializers->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( loopData().condition ) loopData().condition->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( loopData().increase ) loopData().increase->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( loopData().block ) loopData().block->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
		case astOp::btForEach:
			if ( forEachData().statement )
			{
				if ( forEachData().var->getOp() == astOp::symbolValue )
				{
					symbolLocal tmp( forEachData().var->symbolValue(), symWeakVariantType, location, stringi());
					sym->push( &tmp );
					forEachData().statement->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
					sym->pop();
				} else if ( forEachData ().var->getOp () == astOp::pairValue )
				{
					symbolLocal tmp ( forEachData ().var->left->symbolValue (), symWeakVariantType, location, stringi());
					sym->push ( &tmp );
					symbolLocal tmp2 ( forEachData ().var->right->symbolValue (), symWeakVariantType, location, stringi());
					sym->push ( &tmp2 );
					forEachData().statement->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
					sym->pop ( 2 );
				} else
				{
					forEachData ().statement->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
				}
			}
			break;
		case astOp::btWhile:
			if ( loopData().condition ) loopData().condition->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( loopData().block ) loopData().block->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
		case astOp::btRepeat:
			if ( loopData().block ) loopData().block->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( loopData().condition ) loopData().condition->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
		case astOp::btTry:
			if ( !sym->isDefined( tryCatchData().errSymbol->symbolValue(), true ) && !sym->isDefined ( tryCatchData ().errSymbol->symbolValue (), false ) )
			{
				localAdd->addFront( addPt->addSymbolFront( tryCatchData().errSymbol->symbolValue(), symWeakVariantType, location ) );
			}
			if ( tryCatchData().catchBlock ) tryCatchData().catchBlock->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( tryCatchData().finallyBlock ) tryCatchData().finallyBlock->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( tryCatchData().tryBlock ) tryCatchData().tryBlock->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( returnData().node ) returnData().node->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
		case astOp::symbolValue:
			if ( strchr( symbolValue(), ':' ) )
			{	// NOLINT (bugprone-branch-clone)	
				// this shows up as a duplicate in static analysis... but it's not... This condition needs to run in the outer comparison case and then ONLY below if the intermdiary isn't executed
				if ( sym->isDefinedNS( symbolValue(), isAccess ) )
				{
					symbolValue() = sym->getResolvedName( symbolValue(), isAccess );
				}
			} else if ( !sym->isDefinedNS ( symbolValue(), isAccess ) || (func && func->isFGL && (sym->isFunc( symbolValue(), isAccess ) || sym->isClass( symbolValue(), isAccess ))) )
			{
				if ( func )
				{
					if ( !closureAdd->isDefined( symbolValue(), isAccess ) )
					{
						if ( parentSym && parentSym->isDefined( symbolValue(), isAccess ) && (parentSym->getScope( symbolValue(), isAccess ) == symbolSpaceScope::symLocal || parentSym->getScope( symbolValue(), isAccess ) == symbolSpaceScope::symLocalParam) )
						{
							closureAdd->add( symbolValue(), symWeakVariantType );
							// make the parent enclosed value a variant
							parentSym->setType( symbolValue(), isAccess, symbolTypeClass( symWeakVariantType ), accessorType(), nullptr );
							parentSym->setClosedOver( symbolValue() );
							break;
						}
					}
				}
				if ( symbolValue() == sym->file->selfValue && parentSym )
				{
					localAdd->addFront( addPt->addSymbolFront( symbolValue(), symbolTypeClass( symbolType::symObject, parentSym->getClass()->oClass->name ), location ) );
				} else
				{
					if ( func && !func->isInterface )
					{
						if ( sym->isDefined( symbolValue(), isAccess ) )
						{
							if ( isLS )
							{
								sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwAUTOMATIC_VARIABLE_MASK, sym->file->errHandler.throwWarning ( isLS, warnNum::scwAUTOMATIC_VARIABLE_MASK, symbolValue ().c_str () ) ) );
								sym->file->warnings.back ().get ()->setLocation ( this );
							} else
							{
								sym->file->errHandler.throwWarning ( isLS, warnNum::scwAUTOMATIC_VARIABLE_MASK, symbolValue ().c_str () );
							}
						} else
						{
							if ( isLS )
							{
								sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwAUTOMATIC_VARIABLE, sym->file->errHandler.throwWarning ( isLS, warnNum::scwAUTOMATIC_VARIABLE, symbolValue ().c_str () ) ) );
								sym->file->warnings.back ().get ()->setLocation ( this );
							} else
							{
								sym->file->errHandler.throwWarning ( isLS, warnNum::scwAUTOMATIC_VARIABLE, symbolValue ().c_str () );
							}
						}
					}
					localAdd->addFront( addPt->addSymbolFront( symbolValue(), symbolTypeClass( symWeakVariantType ), location ) );
				}
			} else
			{
				if ( sym->isDefinedNS ( symbolValue (), isAccess ) )
				{
					symbolValue () = sym->getResolvedName ( symbolValue (), isAccess );
				}
			}
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = arrayData().nodes.begin(); it != arrayData().nodes.end(); it++ )
			{
				(*it)->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			break;
		case astOp::codeBlockValue:
			vmCodeBlock *cb;
			fgxSymbols *symbols;

			cb = (vmCodeBlock *) bufferBuff( codeBlock().buff );

			symbols = (fgxSymbols *) ((char *) cb + cb->symOffset);

			for ( loop = cb->nParams; loop < cb->nSymbols; loop++ )
			{
				auto name = sym->file->sCache.get( stringi( symbols[loop].offsetName + (char *) (cb) ) );
				if ( !sym->isDefined( name, true ) && !sym->isDefined ( name, false ) )
				{
					localAdd->addFront( addPt->addSymbolFront( name, symWeakVariantType, location ) );
				}
			}
			break;
		case astOp::funcCall:
			// we never default a function call symbol to a local
			if ( left->getOp() != astOp::symbolValue )
			{
				left->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			} else
			{
				if ( strchr( left->symbolValue(), ':' ) )
				{
					// is some sort of globally accessed symbol... 
				} else if ( !sym->isDefined( left->symbolValue(), true ) )
				{
					if ( func )
					{
						if ( !closureAdd->isDefined( left->symbolValue(), true ) )
						{
							if ( parentSym && parentSym->isDefined( left->symbolValue(), true ) && (parentSym->getScope( left->symbolValue(), true ) == symbolSpaceScope::symLocal || parentSym->getScope( left->symbolValue(), true ) == symbolSpaceScope::symLocalParam) )
							{
								closureAdd->add( left->symbolValue(), symWeakVariantType );
								// make the parent enclosed value a variant
								parentSym->setType( left->symbolValue(), true, symbolTypeClass( symWeakVariantType ), accessorType(), nullptr );
								parentSym->setClosedOver( left->symbolValue() );
								break;
							}
						}
					}
				}
			}
			for ( auto it = pList().param.begin(); it != pList().param.end(); it++ )
			{
				if ( (*it) ) (*it)->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			left->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			for ( auto it = pList().param.begin(); it != pList().param.end(); it++ )
			{
				(*it)->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			break;
		case astOp::cSend:
		case astOp::sendMsg:
			left->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( right )
			{
				if ( right->getOp() == astOp::stringValue )
				{
					// convert any strings to nameValues
					*right = *astNode ( sym->file->sCache, astOp::nameValue, sym->file->sCache.get ( right->stringValue () ) ).setLocation ( right );
				} else if ( right->getOp() == astOp::atomValue || right->getOp() == astOp::symbolValue )
				{
					*right = *astNode ( sym->file->sCache, astOp::nameValue, right->symbolValue() ).setLocation ( right );
				}

				if ( right->getOp() != astOp::nameValue )
				{
					right->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
				}

				if ( !isLS )
				{
					// don't do this in LS mode... it will fugger up the code formating since it is removing nodes
					if ( left->getOp () == astOp::symbolValue )
					{
						if ( sym->isClass ( left->symbolValue (), true ) )
						{
							if ( right->getOp () == astOp::nameValue )
							{
								// converting a class.elem syntax into a class::elem symbol
								stringi elemName = left->symbolValue ();
								elemName += "::";
								elemName += right->nameValue ();

								setOp ( sym->file->sCache, astOp::symbolValue );
								symbolValue () = sym->file->sCache.get ( elemName );
							}
						}
					}
				}
			}
			break;
		case astOp::conditional:
			left->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			conditionData().trueCond->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			conditionData().falseCond->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
		case astOp::workAreaStart:
			if ( left->getOp() == astOp::symbolValue )
			{
				*left = *astNode ( sym->file->sCache, astOp::stringValue, left->symbolValue().c_str () ).setLocation ( left );
			} else
			{
				left->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			if ( right->getOp() == astOp::symbolValue )
			{
				*right = *astNode ( sym->file->sCache, astOp::stringValue, right->symbolValue().c_str () ).setLocation ( right );
			} else
			{
				right->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			}
			break;
		case astOp::logicalAnd:
			if ( left )	left->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( right ) right->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
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
			if ( left )	left->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, false );
			if ( right ) right->makeSymbols( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;

		default:
			// all other operands take their value from the left side
			if ( left )	left->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			if ( right ) right->makeSymbols ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, isLS, true );
			break;
	}
}

void astNode::makeSymbolsCodeBlock( errorHandler *err, symbolStack *sym, astNode *addPt, symbolStack *parentSym, opFunction *func, compExecutable *comp, symbolSpaceLocalAdd *localAdd, symbolSpaceClosures *closureAdd, bool isAccess )
{
	size_t				loop;
	errorLocality		e ( err, this );

	switch ( op )
	{
		case astOp::funcValue:
			throw errorNum::scLAMBDA_NOT_ALLOWED_IN_CODEBLOCK;
		case astOp::symbolDef:
			switch ( symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					{
						auto init = symbolDef()->getInitializer ( symbolDef()->getSymbolName ( ) );
						if ( init->getOp () == astOp::assign )
						{
							init->right->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
						}
						sym->push ( symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							auto init = (*symbol)[it]->initializer;
							if ( init->getOp () == astOp::assign )
							{
								init->right->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
							}
						}
						sym->push ( symbolDef() );
					}
					break;
				case symbolSpaceType::symTypeClass:
					{
						auto cls = dynamic_cast<symbolClass*>(symbolDef());
						sym->insert( symbolDef(), cls->insertPos );
					}
					break;
				default:
					sym->push ( symbolDef() );
					break;
			}
			break;
		case astOp::btBasic:
			{
				auto size = sym->size ( );
				if ( !addPt )
				{
					astNode basic ( sym->file->sCache, astOp::btBasic );

					for ( auto it = basicData().blocks.begin ( ); it != basicData().blocks.end ( ); it++ )
					{
						basic.pushNode ( (*it) );
						(*it)->makeSymbolsCodeBlock ( err, sym, &basic, parentSym, func, comp, localAdd, closureAdd, true );
					}
					std::swap ( basicData().blocks, basic.basicData().blocks );
					basic.basicData().blocks.clear ( );
				} else
				{
					for ( auto it = basicData().blocks.begin ( ); it != basicData().blocks.end ( ); it++ )
					{
						(*it)->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
					}
				}
				sym->resize ( size );
			}
			break;
		case astOp::btSwitch:
			if ( switchData().condition ) switchData().condition->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			for ( auto it = switchData().caseList.begin ( ); it != switchData().caseList.end ( ); it++ )
			{
				if ( (*it)->block ) (*it)->block->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
				if ( (*it)->condition ) (*it)->condition->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			}
			break;
		case astOp::btIf:
			for ( auto it = ifData().ifList.begin ( ); it != ifData().ifList.end ( ); it++ )
			{
				if ( (*it)->block ) (*it)->block->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
				if ( (*it)->condition ) (*it)->condition->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			}
			if ( ifData().elseBlock ) ifData().elseBlock->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::btInline:
			if ( inlineFunc().node ) inlineFunc().node->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::btFor:
			if ( loopData().initializers ) loopData().initializers->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( loopData().condition ) loopData().condition->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( loopData().increase ) loopData().increase->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( loopData().block ) loopData().block->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::btForEach:
			if ( !sym->isDefined ( forEachData().var->symbolValue(), true ) && !sym->isDefined ( forEachData ().var->symbolValue (), false ) )
			{
				localAdd->addFront ( addPt->addSymbolFront ( forEachData().var->symbolValue(), symWeakVariantType, location ) );
			}
			if ( forEachData().in ) forEachData().in->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( forEachData().statement ) forEachData().statement->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			if ( loopData().block ) loopData().block->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( loopData().condition ) loopData().condition->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::btTry:
			if ( !sym->isDefined ( tryCatchData().errSymbol->symbolValue(), true ) && !sym->isDefined ( tryCatchData ().errSymbol->symbolValue (), false ) )
			{
				localAdd->addFront ( addPt->addSymbolFront ( tryCatchData().errSymbol->symbolValue(), symWeakVariantType, location ) );
			}
			if ( tryCatchData().catchBlock ) tryCatchData().catchBlock->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( tryCatchData().finallyBlock ) tryCatchData().finallyBlock->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( tryCatchData().tryBlock ) tryCatchData().tryBlock->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			if ( returnData().node ) returnData().node->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::symbolValue:
			if ( strchr ( symbolValue(), ':' ) )
			{
				// is some sort of globally accessed symbol... 
			} else if ( !sym->isDefined ( symbolValue(), isAccess ) )
			{
				if ( func )
				{
					if ( !closureAdd->isDefined ( symbolValue(), isAccess ) )
					{
						if ( parentSym && parentSym->isDefined ( symbolValue(), true ) && parentSym->getScope ( symbolValue(), isAccess ) == symbolSpaceScope::symLocal )
						{
							closureAdd->add ( symbolValue(), symWeakVariantType );
							// make the parent enclosed value a variant
							parentSym->setType ( symbolValue(), isAccess, symbolTypeClass ( symWeakVariantType ), accessorType(), nullptr );
							parentSym->setClosedOver ( symbolValue() );
							break;
						}

					}
				}
				if ( symbolValue() == sym->file->selfValue && parentSym )
				{
					localAdd->addFront ( addPt->addSymbolFront ( symbolValue(), symbolTypeClass ( symbolType::symObject, parentSym->getClass()->oClass->name ), location ) );
				} else
				{
					localAdd->addFront ( addPt->addSymbolFront ( symbolValue(), symWeakVariantType, location ) );
				}
			}
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = arrayData().nodes.begin ( ); it != arrayData().nodes.end ( ); it++ )
			{
				(*it)->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			}
			break;
		case astOp::codeBlockValue:
			vmCodeBlock	*cb;
			fgxSymbols	*symbols;

			cb = (vmCodeBlock *) bufferBuff ( codeBlock().buff );

			symbols = (fgxSymbols *) ((char *) cb + cb->symOffset);

			for ( loop = cb->nParams; loop < cb->nSymbols; loop++ )
			{
				auto name = sym->file->sCache.get ( symbols[loop].offsetName + (char *) (cb) );
				if ( !sym->isDefined ( name, true ) && !sym->isDefined ( name, false ) )
				{
					localAdd->addFront ( addPt->addSymbolFront ( name, symWeakVariantType, location ) );
				}
			}
			break;
		case astOp::funcCall:
			// we never default a function call symbol to a local
			if ( left->getOp() != astOp::symbolValue )
			{
				left->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			}
			if ( left->getOp() == astOp::symbolValue )
			{
				left->forceOp ( astOp::symbolValue );
			}
			for ( auto it = pList().param.begin ( ); it != pList().param.end ( ); it++ )
			{
				if ( (*it) ) (*it)->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto it = pList().param.begin ( ); it != pList().param.end ( ); it++ )
			{
				(*it)->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			}
			left->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::cSend:
		case astOp::sendMsg:
			left->makeSymbolsCodeBlock( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( right )
			{
				if ( right->getOp() == astOp::stringValue )
				{
					// convert any strings to nameValues
					*right = *astNode ( sym->file->sCache, astOp::nameValue, sym->file->sCache.get ( right->stringValue () ) ).setLocation ( right );
				} else if ( right->getOp() == astOp::atomValue || right->getOp() == astOp::symbolValue )
				{
					right->forceOp ( astOp::nameValue );
				}

				if ( right->getOp() != astOp::nameValue )
				{
					right->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
				}
			}
			break;
		case astOp::conditional:
			left->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			conditionData().trueCond->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			conditionData().falseCond->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		case astOp::workAreaStart:
			if ( left )
			{
				if ( left->getOp() == astOp::symbolValue )
				{
					left->forceOp ( astOp::symbolValue );
				} else
				{
					left->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
				}
			}
			if ( right->getOp() == astOp::symbolValue )
			{
				right->forceOp ( astOp::symbolValue );
			} else
			{
				right->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			}
			break;
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
			if ( left )	left->makeSymbolsCodeBlock( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, false );
			if ( right ) right->makeSymbolsCodeBlock( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
		default:
			// all other operands take their value from the left side
			if ( left )	left->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			if ( right ) right->makeSymbolsCodeBlock ( err, sym, addPt, parentSym, func, comp, localAdd, closureAdd, true );
			break;
	}
}
