
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "compilerOptimizer.h"

// this returns if the inlined function has a dependency on a load image... we can not do cross-load image includes currently as they require BSS to be correct
static astNode* convertPseudoCallCB( astNode* node, astNode* parent, symbolStack* sym, bool isAccess, opFunction* func, unique_queue<accessorType>* scanQueue )
{
	switch ( node->getOp() )
	{
		case astOp::funcCall:
			if ( node->left->getOp() == astOp::sendMsg )
			{
				// method calls
				symbolTypeClass type = node->left->left->getType( sym );
				if ( !type.isAnObject() && type.compType() != symbolType::symUnknown )
				{
					// pseudo object calls
					if ( ((node->left->right->getOp() == astOp::nameValue) || (node->left->right->getOp() == astOp::atomValue)) )
					{
						node->left->right->forceOp( astOp::atomValue );
						if ( node->left->left->getOp() == astOp::symbolValue && node->left->right->nameValue() == sym->file->newValue )
						{
							throw errorNum::scPSUEDO_OBJ_NEW;
						} else
						{
							// convert to a regular call
							auto send = node->left;
							node->pList().param.insert( node->pList().param.begin(), node->left->left );
							node->left = node->left->right;
							node->right = 0;

							send->left = 0;
							send->right = 0;
							delete send;
							scanQueue->push( func );
						}
					}
				}
			}
			break;
		default:
			break;
	}
	return node;
}

static void convertPseudoCall( symbolStack* sym, opFunction* func, unique_queue<accessorType>* scanQueue )
{
	astNodeWalk( func->codeBlock, sym, convertPseudoCallCB, func, scanQueue );
}

static auto deleteClassElement( compClass* cls, ptrdiff_t index )
{
	std::reference_wrapper<uint32_t> specialFuncs[] = { cls->newBaseIndex,
															cls->newIndex,
															cls->cNewIndex,
															cls->defaultMethodIndex,
															cls->defaultAssignIndex,
															cls->defaultAccessIndex,
															cls->releaseIndex,
															cls->releaseBaseIndex,
															cls->cReleaseIndex,
															cls->packIndex,
															cls->unpackIndex
	};

	for ( auto& it : specialFuncs )
	{
		if ( it && (it - 1 >= (size_t) index) )
		{
			(it)--;
		}
	}
	for ( size_t loop = 0; loop < cls->overload.size(); loop++ )
	{
		if ( cls->overload[loop] )
		{
			if ( cls->overload[loop] - 1 >= (size_t) index )
			{
				cls->overload[loop]--;
			}
		}
	}

	return cls->elements.erase( cls->elements.begin() + index );
}

static astNode* resetInlineRetTypeCB( astNode* node, astNode* parent, symbolStack* sym, bool isAccess )
{
	switch ( node->getOp() )
	{
		case astOp::btInline:
			if ( node->inlineFunc().retType == symWeakVariantType )
			{
				node->inlineFunc().retType.resetType();
			}
			break;
		default:
			break;
	}
	return node;
}

void resetInlineRetType( astNode* block, opFile* file )
{
	symbolStack				sym( file );

	astNodeWalk( block, &sym, resetInlineRetTypeCB );
}

void propagateConst( opFunction* func )
{
	func->needScan = false;
	for ( auto& it : func->accessors )
	{
		if ( std::holds_alternative<opFunction*>( it ) )
		{
			auto func = std::get<opFunction*>( it );
			if ( func->isConst )
			{
				// const has changed so propagate that through all callers
				func->isConst = false;
				propagateConst( func );
			}
		}
	}
}

void propagatePure( opFunction* func )
{
	func->needScan = false;
	for ( auto& it : func->accessors )
	{
		if ( std::holds_alternative<opFunction*>( it ) )
		{
			auto func = std::get<opFunction*>( it );
			if ( func->isPure )
			{
				// const has changed so propagate that through all callers
				func->isPure = false;
				propagateConst( func );
			}
		}
	}
}

void propagateConstPure( opFile* file )
{
	for ( auto& it : file->functionList )
	{
		if ( !it.second->isInterface )
		{
			it.second->needScan = true;
		}
	}
	for ( auto& it : file->functionList )
	{
		if ( !it.second->isInterface )
		{
			if ( !it.second->isConst )
			{
				propagateConst( it.second );
			}
		}
	}
	for ( auto& it : file->functionList )
	{
		if ( !it.second->isInterface )
		{
			it.second->needScan = true;
		}
	}
	for ( auto& it : file->functionList )
	{
		if ( !it.second->isInterface && it.second->needScan )
		{
			if ( !it.second->isPure )
			{
				propagatePure( it.second );
			}
		}
	}
}

// NOTE: inUseOnly should ONLY be set when we have valid inUse information... this is only set when we're marking for removal
void optimizer::makeKnown( unique_queue<accessorType>* scanQueue, bool inUseOnly )
{
	// do class elements first... these won't be recursive but functions MAY be... so doing these first will mean it's much less likely to need to do a function again based on the element being forced
	for ( auto& cls : compDef->classDef )
	{
		if ( !cls->isInterface && cls->oClass->inUse )
		{
			for ( auto& elem : cls->elements )
			{
				switch ( elem.type )
				{
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
						if ( elem.elem->symType == symUnknownType )
						{
							elem.elem->symType = symWeakVariantType;
							for ( auto& acc : elem.elem->accessors )
							{
								if ( scanQueue ) scanQueue->push( acc );
							}
						}
						break;
					default:
						break;
				}
			}
		}
	}
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		if ( !it->second->isInterface && (it->second->inUse || !inUseOnly) )
		{
			if ( it->second->params.makeKnown() )
			{
				scanQueue->push( it->second );
			}
			if ( it->second->codeBlock )
			{
				if ( it->second->codeBlock->makeKnown( file ) )
				{
					scanQueue->push( it->second );
				}
			}
			if ( it->second->retType == symUnknownType )
			{
				it->second->retType = symWeakVariantType;
				if ( !it->second->isProcedure )
				{
					for ( auto& it : it->second->accessors )
					{
						if ( scanQueue ) scanQueue->push( it );
					}
				}
			}
		}
	}
	for ( auto it = file->symbols.begin(); it != file->symbols.end(); it++ )
	{
		if ( it->second.type == symUnknownType )
		{
			it->second.type = symWeakVariantType;
			for ( auto& acc : it->second.accessors )
			{
				if ( scanQueue ) scanQueue->push( acc );
			}
		}
	}
}

void typeInfer ( compExecutable *compDef, opFile *file, opFunction *func, bool doInterprocedural, bool markUsed, unique_queue<accessorType> *scanQueue, bool isLS )
{
	func->needScan = false;

	errorHandler	errHandler ( file );
	try
	{

		symbolStack		sym ( file, func, nullptr );
		errorLocality	e ( &errHandler, func->location );

		if ( func->isInterface )
		{
			// don't change types, but DO mark things as being in use for interface functions
			if ( func->params.size () )
			{
				opFunction paramFunc ( file );
				paramFunc.usingList = func->usingList;
				symbolStack paramSym ( file, &paramFunc );
				func->params.markInUse ( compDef, func, &errHandler, &paramSym, doInterprocedural, scanQueue, isLS );
			}

			func->params.forceVariant ();
			if ( func->codeBlock )
			{
				func->codeBlock->markInUse ( compDef, func, &errHandler, &sym, false, scanQueue, isLS );
			}
			return;
		}

		if ( func->params.size () )
		{
			opFunction paramFunc ( file );
			paramFunc.usingList = func->usingList;
			symbolStack paramSym ( file, &paramFunc );
			func->params.typeInfer ( compDef, func, &errHandler, &paramSym, doInterprocedural, markUsed, scanQueue, isLS );
		}

		if ( func->classDef )
		{
			for ( auto &elemIt : func->classDef->elems )
			{
				for ( auto &inIt : func->initializers )
				{
					if ( elemIt->name == inIt->left->symbolValue () )
					{
						switch ( elemIt->type )
						{
							case fgxClassElementType::fgxClassType_inherit:
								{
									astNode newNode = astNode ( *inIt );
									newNode.morphNew ( compDef->file );
									newNode.typeInfer ( compDef, func, &errHandler, &sym, nullptr, doInterprocedural, true, markUsed, scanQueue, isLS );
								}
								break;
							case fgxClassElementType::fgxClassType_iVar:
								{
									astNode newNode = astNode ( *inIt );
									newNode.morphAssignment ( compDef->file );
									newNode.typeInfer ( compDef, func, &errHandler, &sym, nullptr, doInterprocedural, true, markUsed, scanQueue, isLS );
								}
								break;
							default:
								break;
						}
						break;
					}
				}
			}
		}
		//	printf( "%I64u\r\n", sym.size() );
		if ( func->codeBlock )
		{
			func->codeBlock->typeInfer ( compDef, func, &errHandler, &sym, nullptr, doInterprocedural, false, markUsed, scanQueue, isLS );
		}
	} catch ( errorNum err )
	{
		errHandler.throwFatalError ( false, err );
	}
	if ( errHandler.isFatal () )
	{
		file->errHandler.setFatal ();		// propogate all errors up to main file (since we have our own error handler for each worker thread)
	}
}

void processScanQueue ( compExecutable *compDef, opFile *file, bool markUsed, bool interProcedural, unique_queue<accessorType> *scanQueue, bool isLS )
{
	try
	{
		while ( !scanQueue->empty () )
		{
			auto entry = scanQueue->pop ();
			if ( std::holds_alternative<opFunction *> ( entry ) )
			{
				opFunction *func = std::get<opFunction *> ( entry );
				//printf ( "%s\r\n", func->name.c_str () );
				// do NOT type infer unless the function is actually in use... it may be put in the scan queue for inlined usage, but infact may never be called
				if ( func->inUse && func->codeBlock && !func->isIterator )
				{
					std::shared_lock g1 ( file->makeIteratorLock );
					typeInfer ( compDef, file, func, interProcedural, markUsed, scanQueue, isLS );
				}
			} else if ( std::holds_alternative<opForceScan> ( entry ) )
			{
				opFunction *func = std::get<opForceScan> ( entry );
				if ( !func->isIterator )
				{
					// do NOT type infer unless the function is actually in use... it may be put in the scan queue for inlined usage, but infact may never be called
					std::shared_lock g1 ( file->makeIteratorLock );
					typeInfer ( compDef, file, func, interProcedural, markUsed, scanQueue, isLS );
				}
			} else if ( std::holds_alternative<opClassElement *> ( entry ) )
			{
				auto elem = std::get<opClassElement *> ( entry );

				// TODO... elements need to be linked back to the class so we can properly handle using list!!!
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_iVar:		// these are initialized in the function wrapper so they're already accounted for
						break;
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
						{
							auto &s = file->symbols.find ( elem->data.iVar.symbolName )->second;
							if ( s.initializer && s.initializer->getOp () == astOp::assign )
							{
								symbolStack				sym ( file );

								errorHandler	errHandler ( file );
								try
								{

									errorLocality	e ( &errHandler, s.initializer->location );
									symbolTypeClass retType;

									class opUsingList		useList;
									symbolSpaceNamespace	ns ( file, &file->ns, &useList );
									sym.push ( &ns );

									std::shared_lock g1 ( file->makeIteratorLock );
									if ( (elem->symType &= s.initializer->right->typeInfer ( compDef, elem, &errHandler, &sym, &retType, interProcedural, true, markUsed, scanQueue, isLS )) )
									{
										for ( auto &it2 : elem->accessors )
										{
											scanQueue->push ( it2 );
										}
									}
									s.type = elem->symType;
								} catch ( errorNum err )
								{
									errHandler.throwFatalError ( false, err );
									throw;
								}
								if ( errHandler.isFatal () )
								{
									file->errHandler.setFatal ();		// propogate all errors up to main file (since we have our own error handler for each worker thread)
								}
							}
						}
						break;
					default:
						break;
				}
			} else if ( std::holds_alternative<opSymbol *> ( entry ) )
			{
				// TODO: add support for global symbol inferencing
				auto elem = std::get<opSymbol *> ( entry );

				// TODO... elements need to be linked back to the class so we can properly handle using list!!!

				if ( elem->loadTimeInitializable )
				{
					auto init = elem->initializer;

					if ( init )
					{
						symbolTypeClass retType = symUnknownType;

						if ( init->getOp () == astOp::assign )
						{
							symbolStack		sym ( file );

							errorHandler	errHandler ( file );
							try
							{
								errorLocality	e ( &errHandler, init->right );
								symbolTypeClass retType;

								class opUsingList		useList;
								symbolSpaceNamespace	ns ( file, &file->ns, &useList );
								sym.push ( &ns );

								std::shared_lock g1 ( file->makeIteratorLock );
								elem->type = init->right->typeInfer ( compDef, accessorType (), &errHandler, &sym, &retType, interProcedural, true, markUsed, scanQueue, isLS );
							} catch ( errorNum err )
							{
								errHandler.throwFatalError ( false, err );
								throw;
							}
							if ( errHandler.isFatal () )
							{
								file->errHandler.setFatal ();		// propogate all errors up to main file (since we have our own error handler for each worker thread)
							}
						}
					}
				}
			} else if ( std::holds_alternative<std::monostate> ( entry ) )
			{
			} else if ( std::holds_alternative<struct makeIteratorType *> ( entry ) )
			{
				auto makeIt = std::get<struct makeIteratorType *> ( entry );

				std::unique_lock g1 ( file->makeIteratorLock );

				if ( isLS )
				{
					makeIt->function->isIterator = false;
				} else
				{
					compDef->compMakeIterator ( file, makeIt->function, makeIt->generic, makeIt->acc, scanQueue, isLS );
				}

			} else
			{
				assert ( false );
				throw errorNum::scINTERNAL;
			}
		}
	} catch ( errorNum err )
	{
		file->errHandler.throwFatalError ( false, err );
	}
}

void startProcessScanQueue( compExecutable* compDef, opFile* file, bool markUsed, bool interProcedural, unique_queue<accessorType>* scanQueue, bool isLS )
{
	std::vector<std::thread>  threads;
//	for ( size_t workerCount = 0; workerCount < 0; workerCount++ )
	for ( size_t workerCount = 0; workerCount < std::thread::hardware_concurrency() - 1; workerCount++ )
	{
		threads.push_back( std::thread( processScanQueue, compDef, file, false, true, scanQueue, isLS ) );
	}

	processScanQueue( compDef, file, false, true, scanQueue, isLS );

	for ( auto& it : threads )
	{
		it.join();
	}
}

void optimizer::doTypeInference( cacheString const& entry, bool doMakeKnown, bool removeUnused, bool isLS )
{
	auto mainIt = file->functionList.find( entry );
	unique_queue<accessorType> scanQueue;

	symbolStack				sym( file );
	class opUsingList		useList;
	symbolSpaceNamespace	ns( file, &file->ns, &useList );
	sym.push( &ns );

	//	printf ( "--------------------------------------------\r\n" );

	if ( doMakeKnown )
	{
		// reset the class inuse information... this will allow us to mark the special functions appropriately.
		for ( auto& cls : compDef->classDef )
		{
			cls->oClass->inUse = false;
		}

		// we don't need to do any additional inferencing here... the last inference call and the remaining passes did not change anything
		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( !it->second->isInterface && !it->second->inUse )
			{
				scanQueue.push( opForceScan( it->second ) );
			}
		}
		startProcessScanQueue( compDef, file, false, true, &scanQueue, isLS );

		makeKnown( &scanQueue, false );

		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( !it->second->isInterface && !it->second->inUse )
			{
				symbolStack	localSym( file, it->second );
				convertPseudoCall( &localSym, it->second, &scanQueue );
			}
		}

		while ( !scanQueue.empty() )
		{
			startProcessScanQueue( compDef, file, false, true, &scanQueue, isLS );
			// repeat make known... we may have now brought in new functions and need to repeat until all call paths are completely infered and settled
			makeKnown( &scanQueue, false );
		} while ( !scanQueue.empty() );
		return;
	}

	if ( !doInterprocedurealTypeInference || (mainIt == file->functionList.end()) )
	{
		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( (*it).second->isIterator && (*it).second->inUse && !(*it).second->isInterface )
			{
				errorLocality e( &file->errHandler, (*it).second->location );
				compDef->compMakeIterator( file, (*it).second, true, accessorType(), &scanQueue, isLS );
			}
		}

		// clear the callers list
		for ( auto& it : file->functionList )
		{
			errorLocality e( &file->errHandler, it.second->location );
			it.second->inUse = false;
			if ( !it.second->isInterface )
			{
				compDef->resetSymbols( it.second );		// resets everyting to unknown...
				it.second->retType.resetType();
				it.second->params.forceVariant();
				it.second->accessors.clear();
				scanQueue.push( it.second );
				it.second->inUse = true;
			}
			it.second->inlineUse = false;
			it.second->isForcedVariant = false;
		}

		// reset all IVAR's, const and static types to undefined
		for ( auto& cls : compDef->classDef )
		{
			cls->oClass->accessors.clear ();
			for ( auto& elem : cls->elements )
			{
				switch ( elem.type )
				{
					case fgxClassElementType::fgxClassType_iVar:
						if ( !cls->isInterface )
						{
							elem.elem->symType.forceVariant();
						}
						elem.elem->typeChanged = false;
						elem.elem->accessors.clear();
						break;
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
						if ( !cls->isInterface )
						{
							elem.elem->symType.forceVariant();
						}
						elem.elem->typeChanged = false;
						elem.elem->accessors.clear();
						scanQueue.push( elem.elem );
						break;
					default:
						break;
				}
			}
			cls->oClass->inUse = false;
		}

		// mark all the globals
		for ( auto& it : file->symbols )
		{
			if ( it.second.loadTimeInitializable )
			{
				it.second.accessors.clear ();
				auto init = it.second.initializer;
				if ( init )
				{
					scanQueue.push( &it.second );
				}
			}
		}

#if 0
		if ( file->functionList.find( compDef->file->sCache.get( "cCadTransExternalize.processTrans_Caliber.method" ) ) != file->functionList.end() )
		{
			unique_queue<accessorType> scanQueue2;
			scanQueue2.push( file->functionList.find( compDef->file->sCache.get( "cCadTransExternalize.processTrans_Caliber.method" ) )->second );
			startProcessScanQueue( compDef, file, removeUnused, false, &scanQueue2, isLS );
		}
#endif
		startProcessScanQueue( compDef, file, removeUnused, false, &scanQueue, isLS );

		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( (*it).second->isIterator && (*it).second->inUse )
			{
				errorLocality e( &file->errHandler, (*it).second->location );
				compDef->compMakeIterator( file, (*it).second, true, accessorType(), &scanQueue, isLS );
				scanQueue.push( opForceScan( (*it).second ) );
			}
		}

		startProcessScanQueue( compDef, file, removeUnused, false, &scanQueue, isLS );

		// propogate any non-const, non-pure to their respective callers
		propagateConstPure( file );
		return;
	}

	// reset all the weak types to unknown in all the functions
	for ( auto& it : file->functionList )
	{
		it.second->toBeDeleted = false;
		it.second->inUse = false;
		it.second->inlineUse = false;
		if ( it.second->codeBlock && !it.second->isInterface )
		{
			errorLocality e( &file->errHandler, it.second->location );
			compDef->resetSymbols( it.second );
			it.second->retType.resetType();
			resetInlineRetType( it.second->codeBlock, file );
			// these will be discovered... we start off assuming they're all const/pure and when we find otherwise we then set them to non.
			// we'll propagate function calls at the end
			it.second->isPure = true;
			it.second->isConst = true;
		}
		it.second->isForcedVariant = false;
		it.second->accessors.clear();
	}

	// reset all IVAR's, const and static types to undefined
	for ( auto& cls : compDef->classDef )
	{
		cls->oClass->inUse = false;
		cls->oClass->accessors.clear();
		if ( !cls->isInterface )
		{
			for ( auto& elem : cls->elements )
			{
				elem.elem->symType.resetType();
				elem.elem->typeChanged = false;
				elem.elem->accessors.clear();
			}
		}
	}

	// mark all the globals for scanning
	for ( auto& it : file->symbols )
	{
		if ( it.second.loadTimeInitializable && !it.second.isInterface )
		{
			it.second.type.resetType();
			it.second.accessors.clear();
			auto init = it.second.initializer;
			if ( init )
			{
				scanQueue.push( &it.second );
			}
		}
	}

	// mark as in-use all the classes mandated by the runtime.
	for ( auto& cls : file->classList )
	{
		if ( cls.second->name.c_str()[0] == '$' )
		{
			markClassMethodsInuse( compDef, accessorType(), cls.second, &sym, &scanQueue, false, isLS );
		}
	}

	markClassInuse( compDef, accessorType(), file->classList.find ( compDef->file->aArrayValue )->second, &sym, &scanQueue, isLS );
	file->functionList.find( compDef->file->exceptValue )->second->setAccessed( accessorType(), &scanQueue );
	markClassMethodsInuse( compDef, accessorType(), file->classList.find( compDef->file->sCache.get( "systemError" ) )->second, &sym, &scanQueue, false, isLS );

	scanQueue.push( file->functionList.find( compDef->file->sCache.get( "main" ) )->second );
	file->functionList.find( compDef->file->sCache.get( "main" ) )->second->setAccessed( accessorType(), &scanQueue );

	startProcessScanQueue( compDef, file, removeUnused, true, &scanQueue, isLS );

	if ( file->dependenciesFromExternalSources.size() )
	{
		for ( auto& it : file->dependenciesFromExternalSources )
		{
			auto funcIt = file->functionList.find( it );
			if ( funcIt != file->functionList.end() )
			{
				if ( !funcIt->second->inUse )
				{
					funcIt->second->inUse = true;
					scanQueue.push( funcIt->second );
				}
				if ( funcIt->second->classDef )
				{
					markClassInuse( compDef, accessorType(), funcIt->second->classDef, &sym, &scanQueue, isLS );
				}
			}
		}
		startProcessScanQueue( compDef, file, removeUnused, true, &scanQueue, isLS );
	}

	if ( removeUnused )
	{
		bool unknownClassInstantiation = false;
		bool unknownFunctionCall = false;

		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( !it->second->isInterface && !it->second->inUse )
			{
				symbolStack	localSym( file, it->second );
				convertPseudoCall( &localSym, it->second, &scanQueue );
			}
		}

		// we MAY still have functions that have never been called but still have unknown return types. (this is almost always a bug in the source code), but we need to 
		// mark them weakVariant and run one additional pass to allow types to settle
		makeKnown( &scanQueue, true );

		while ( !scanQueue.empty() )
		{
			startProcessScanQueue( compDef, file, removeUnused, true, &scanQueue, isLS );
			// repeat make known... we may have now brought in new functions and need to repeat until all call paths are completely infered and settled
			makeKnown( &scanQueue, true );
		}

		for ( auto& it : file->functionList )
		{
			if ( it.second->inUse )
			{
				unknownClassInstantiation |= it.second->unknownClassInstantiation;
				unknownFunctionCall |= it.second->unknownFunctionCall;
			}
		}

		if ( unknownClassInstantiation )
		{
			// which classes will be instantiated is indeterminate
			for ( auto& it : file->classList )
			{
				markClassMethodsInuse( compDef, accessorType(), it.second, &sym, &scanQueue, true, isLS );
			}
		}

		// NOTE: if we don't have a DIRECT link to a function we really don't give a crap about constness/purity.
		//		 the only reason we care about constness/purity is if we have all direct links to functions so that we can potentially optimize out calls
		//		 and or warn about unused returns from const functions.   If we don't have a direct call non of those (nor other optimizations) apply
		propagateConstPure( file );

		std::unordered_map<cacheString, opFunction*>  funcsToBeDeleted;

		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( (*it).second->isIterator && (*it).second->inUse )
			{
				errorLocality e( &file->errHandler, (*it).second->location );
				compDef->compMakeIterator( file, (*it).second, true, accessorType(), &scanQueue, isLS );
				scanQueue.push( opForceScan( (*it).second ) );
			}
		}

		// process the scanQueue
		startProcessScanQueue( compDef, file, removeUnused, true, &scanQueue, isLS );

		if ( !unknownFunctionCall )
		{
			// mark unused functions for deletion
			for ( auto funcIt = file->functionList.begin(); funcIt != file->functionList.end(); funcIt++ )
			{
				if ( !funcIt->second->inUse && !funcIt->second->inlineUse )
				{
					if ( !funcIt->second->isInterface )
					{
						funcsToBeDeleted[funcIt->first] = funcIt->second;
						funcIt->second->toBeDeleted = true;
					} else
					{
						if ( funcIt->second->classDef )
						{
							markClassMethodsInuse( compDef, accessorType(), funcIt->second->classDef, &sym, &scanQueue, true, isLS );
						}
					}
				}
			}
		} else if ( !unknownClassInstantiation )
		{
			// even thought we can't delete unused functions, we MAY still be able to remove unused class methods/property set/get'ers
			for ( auto funcIt = file->functionList.begin(); funcIt != file->functionList.end(); funcIt++ )
			{
				if ( !funcIt->second->inUse && !funcIt->second->inlineUse && funcIt->second->classDef )
				{
					funcsToBeDeleted[funcIt->first] = funcIt->second;
					funcIt->second->toBeDeleted = true;
				}
			}
		}

		for ( auto it = file->classList.begin(); it != file->classList.end(); )
		{
			if ( !it->second->inUse )
			{
				for ( auto cIt = compDef->classDef.begin(); cIt != compDef->classDef.end(); cIt++ )
				{
					if ( *cIt == it->second->cClass )
					{
						compDef->classDef.erase( cIt );
						break;
					}
				}
				//printf ( "deleting: %s\r\n", it->second->name.c_str () );
				delete it->second;
				it = file->classList.erase( it );
			} else
			{
				it++;
			}
		}

		// go through and remove any class methods/props for which we no longer have a valid element;  this will happen when we simply never call that method
		// since inUse is marked within the function definition, if the function is no longer present (has been removed due to non-use) then we use this as
		// the indicator to remove the method.
		for ( auto& cls : compDef->classDef )
		{
			for ( size_t index = 0; index < cls->overload.size(); index++ )
			{
				if ( cls->overload[index] && cls->oClass->opOverload[index] )
				{
					auto funcIt = file->functionList.find( cls->oClass->opOverload[index]->data.method.func );
					if ( funcIt->second->toBeDeleted )
					{
						delete cls->oClass->opOverload[index];
						deleteClassElement( cls, cls->overload[index] - 1 );
						cls->oClass->opOverload[index] = 0;
						cls->overload[index] = 0;
					}
				}
			}
			for ( auto elemIt = cls->elements.begin(); elemIt != cls->elements.end(); )
			{
				switch ( elemIt->elem->type )
				{
					case fgxClassElementType::fgxClassType_method:
						if ( elemIt->elem->data.method.func )
						{
							auto funcIt = file->functionList.find( elemIt->elem->data.method.func );
							if ( funcIt->second->toBeDeleted )
							{
								for ( auto& oElem : cls->oClass->elems )
								{
									if ( elemIt->elem == oElem )
									{
										oElem->name = cacheString(); // don't erase as we have hard links back so can't cause vector to reallocate, but simply make it unfindable
										oElem->data.method.func = cacheString();
										break;
									}
								}
								elemIt = deleteClassElement( cls, std::distance( cls->elements.begin(), elemIt ) );
							} else
							{
								elemIt++;
							}
						} else
						{
							elemIt++;
						}
						break;
					case fgxClassElementType::fgxClassType_prop:
						{
							if ( elemIt->elem->data.prop.access )
							{
								auto funcIt = file->functionList.find( elemIt->elem->data.prop.access );
								if ( funcIt->second->toBeDeleted )
								{
									elemIt->elem->data.prop.access = cacheString();
									elemIt->methodAccess.func = nullptr;
								}
							}
							if ( elemIt->elem->data.prop.assign )
							{
								auto funcIt = file->functionList.find( elemIt->elem->data.prop.assign );
								if ( funcIt->second->toBeDeleted )
								{
									elemIt->elem->data.prop.assign = cacheString();
									elemIt->assign.func = nullptr;
								}
							}

							if ( !elemIt->elem->data.prop.access.size() )
							{
								for ( auto& oElem : cls->oClass->elems )
								{
									if ( elemIt->elem == oElem )
									{
										oElem->data.prop.access = cacheString();
										break;
									}
								}
							}
							if ( !elemIt->elem->data.prop.assign.size() )
							{
								for ( auto& oElem : cls->oClass->elems )
								{
									if ( elemIt->elem == oElem )
									{
										oElem->data.prop.assign = cacheString();
										break;
									}
								}
							}
							if ( !elemIt->elem->data.prop.access.size() && !elemIt->elem->data.prop.assign.size() )
							{
								for ( auto& oElem : cls->oClass->elems )
								{
									if ( elemIt->elem == oElem )
									{
										oElem->name = cacheString(); // don't erase as we have hard links back so cant cause vector to reallocate, but simply make it unfindable
										break;
									}
								}
								elemIt = deleteClassElement( cls, std::distance( cls->elements.begin(), elemIt ) );
							} else
							{
								elemIt++;
							}
							break;
						}
					default:
						elemIt++;
						break;

				}
			}
		}

		// delete all the functions we have marked for deletion
		for ( auto const& it : funcsToBeDeleted )
		{
			file->functionList.erase( file->functionList.find( it.first ) );
			delete it.second;
		}
	} else
	{
		// we do this for the simple reason that we don't want unused iterators to drag crap in... we STILL want to process them so that we can generate warnings, but we don't
		// want there dependencies to be dragged in.  We do that by saving the inUse information, type inferring them and then putting the inUse information back.
		// they will be if/when we do a removeUnused step.   We don't need to worry all that much about type information yet... we really don't care about types so much until after
		// we removed unused functions.
		for ( auto& it : file->functionList )
		{
			it.second->inUseSave = it.second->inUse;
		}
		for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
		{
			if ( !(*it).second->inUse || (*it).second->isIterator )
			{
				errorLocality e( &file->errHandler, (*it).second->location );
				scanQueue.push( opForceScan( (*it).second ) );
			}
		}
		startProcessScanQueue( compDef, file, false, false, &scanQueue, isLS );
		for ( auto& it : file->functionList )
		{
			it.second->inUse = it.second->inUseSave;
		}
		propagateConstPure( file );
	}
}
