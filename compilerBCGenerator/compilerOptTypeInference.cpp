
#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "compilerOptimizer.h"

#include <condition_variable>
#include <semaphore>

// this returns if the inlined function has a dependency on a load image... we can not do cross-load image includes currently as they require BSS to be correct
static astNode* convertPseudoCallCB( astNode* node, astNode* parent, symbolStack* sym, bool isAccess, bool, opFunction* func, unique_queue<accessorType>* scanQueue, bool isLS )
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
					// pseudo object calls... left of the send is NOT an object, so convert into a function call if possible
					if ( ((node->left->right->getOp() == astOp::nameValue) || (node->left->right->getOp() == astOp::atomValue)) )
					{
						if ( !sym->isDefined ( node->left->right->symbolValue (), true ) )
						{
							errorLocality e ( &sym->file->errHandler, node->left );
							if ( isLS )
							{
								sym->file->warnings.push_back ( std::make_unique<astNode> ( sym->file->sCache, warnNum::scwUNDEFINED_FUNCTION, sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNDEFINED_FUNCTION, node->left->right->symbolValue ().c_str () ) ) );
								sym->file->warnings.back ().get ()->setLocation ( node->left->right );

								// we NEED some function going forward to operate on... or we have to do a lot of extra crap...
								// the solution to allow the other modules to keep working when in LS mode is to simply create a dummy function to act as a place holder for missing functions
								opFunction *newFunc = new opFunction ( sym->file );
								if ( !memcmp ( node->left->right->symbolValue (), "::", 2 ) )
								{
									node->left->right->symbolValue () = sym->file->sCache.get ( node->left->right->symbolValue ().c_str () + 2 );
								}
								newFunc->name = node->left->right->symbolValue ();
								newFunc->isVariantParam = true;					// mark it variant so we don't have extraneous warnings about extra parameters for someting we don't even know about

								sym->file->functionList.insert ( { newFunc->name, newFunc } );
								sym->file->symSearch.insert ( { newFunc->name, newFunc } );
							} else
							{
								sym->file->errHandler.throwWarning ( isLS, warnNum::scwUNDEFINED_FUNCTION, node->left->right->symbolValue ().c_str () );
							}
						}

						node->left->right->forceOp( astOp::atomValue );
						if ( node->left->left->getOp() == astOp::symbolValue && node->left->right->nameValue() == sym->file->newValue )
						{
							throw errorNum::scPSUEDO_OBJ_NEW;
						} else
						{
							if ( isLS ) sym->file->statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOperator, node->left->location ) );

							// convert to a regular call
							auto send = node->left;
							node->pList().param.insert( node->pList().param.begin(), node->left->left );
							node->left = node->left->right;
							node->right = 0;

							send->left = 0;
							send->right = 0;
							delete send;
							if ( scanQueue ) scanQueue->push( func );
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

static void convertPseudoCall( symbolStack* sym, opFunction* func, unique_queue<accessorType>* scanQueue, bool isLS )
{
	astNodeWalk( func->codeBlock, sym, convertPseudoCallCB, func, scanQueue, isLS );
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
		if ( it && ((ptrdiff_t)it - 1 >= index) )
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

static astNode* resetInlineRetTypeCB( astNode* node, astNode* parent, symbolStack* sym, bool isAccess, bool )
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
		if ( !cls->isInterface && (cls->oClass->inUse || !inUseOnly) )
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
							elem.elem->symType.makeKnown ();
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
				if ( scanQueue )scanQueue->push( it->second );
			}
			if ( it->second->codeBlock )
			{
				if ( it->second->codeBlock->makeKnown( file ) )
				{
					if ( scanQueue )scanQueue->push( it->second );
				}
			}
			if ( it->second->retType == symUnknownType )
			{
				it->second->retType.makeKnown ();
				if ( !it->second->isProcedure )
				{
					for ( auto&acc : it->second->accessors )
					{
						if ( scanQueue ) scanQueue->push( acc );
					}
				}
			}
		}
	}
	for ( auto it = file->symbols.begin(); it != file->symbols.end(); it++ )
	{
		if ( it->second.type == symUnknownType )
		{
			it->second.type.makeKnown ();
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
			paramFunc.name = file->newValue;	// some dummy for debug comparison
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
				// do NOT type infer unless the function is actually in use... it may be put in the scan queue for inlined usage, but infact may never be called
				if ( func->inUse && func->codeBlock && !func->isIterator )
				{
					//printf ( "%s\r\n", func->name.c_str () );
					std::shared_lock g1 ( file->makeIteratorLock );
					typeInfer ( compDef, file, func, interProcedural, markUsed, scanQueue, isLS );
				} else
				{
					//printf ( "1: %s\r\n", func->name.c_str () );
				}
			} else if ( std::holds_alternative<opForceScan> ( entry ) )
			{
				opFunction *func = std::get<opForceScan> ( entry );
				if ( !func->isIterator )
				{
					// do NOT type infer unless the function is actually in use... it may be put in the scan queue for inlined usage, but infact may never be called
					//printf ( "f::%s\r\n", func->name.c_str () );
					std::shared_lock g1 ( file->makeIteratorLock );
					typeInfer ( compDef, file, func, interProcedural, markUsed, scanQueue, isLS );
				} else
				{
					//printf ( "2 : %s\r\n", func->name.c_str () );
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

				try
				{
					std::lock_guard g1 ( file->makeIteratorLock );

					if ( isLS )
					{
						makeIt->function->isIterator = false;
					} else
					{
						compDef->compMakeIterator ( file, makeIt->function, makeIt->generic, makeIt->acc, scanQueue, isLS );
					}
				} catch ( ... )
				{
					delete makeIt;
					throw;
				}
				delete makeIt;
			} else
			{
				assert ( false );
				throw errorNum::scINTERNAL;
			}
			scanQueue->finish ( entry );
		}
	} catch ( errorNum err )
	{
		file->errHandler.throwFatalError ( false, err );
	}
}

#define MULTIWORKER

template <typename T>
class guarded_queue
{
	std::mutex		guard;
	std::queue<T>	gq;

public:

	void push ( const T &value )
	{
		std::lock_guard lock ( guard );
		gq.push ( value );
	}

	void pop ()
	{
		std::lock_guard lock ( guard );
		gq.pop ();
	}

	auto front ()
	{
		std::lock_guard lock ( guard );
		return gq.front ();
	}

	auto size ()
	{
		std::lock_guard lock ( guard );
		return gq.size ();
	}

	auto empty ()
	{
		std::lock_guard lock ( guard );
		return gq.empty ();
	}
};


template<typename R, typename ...Args>
struct workerThreadControl;

template<typename R, typename ...Args>
struct workerThreadControl  < R ( Args... ) >
{
	std::condition_variable			 cv;
	std::mutex						 cvMutex;

	std::mutex						 queueMutex;
	std::queue<std::tuple<Args...>>	 queue;
	
	std::vector<std::thread>		 threads;

	std::counting_semaphore< _UI32_MAX>	 waitSemaphore;

	R ( *funcPtr )(Args...);
	
	template <typename Tuple, std::size_t... I>
	void process ( Tuple const &tuple, std::index_sequence<I...> ) {
		funcPtr ( std::get<I> ( tuple )... );
	}

	template <typename Tuple>
	void process ( Tuple const &tuple ) {
		process ( tuple, std::make_index_sequence<std::tuple_size<Tuple>::value> () );
	}

	void worker ( size_t n )
	{
		while ( 1 )
		{
			{
				std::unique_lock<std::mutex> lk ( cvMutex );
				cv.wait ( lk, [=, this]() {return !queue.empty (); } );
			}

			std::tuple<Args...>	value;
			{
				std::lock_guard gq ( queueMutex );
				if ( queue.empty () ) continue;
				value = queue.front ();
				queue.pop ();
			}

			//printf ( "worker %zu start\r\n", n );
			process ( value );
			//printf ( "worker %zu stop\r\n", n );
			waitSemaphore.release ();
		}
	}

	workerThreadControl ( R ( *funcPtr )(Args...) ) : waitSemaphore ( std::thread::hardware_concurrency () - 1 ), funcPtr ( funcPtr )
	{
#if defined ( MULTIWORKER )
		for ( int64_t workerCount = 0; workerCount < std::thread::hardware_concurrency () - 1; workerCount++ )
		{
			threads.push_back ( std::thread (&workerThreadControl::worker, this, workerCount + 1) );
		}
#endif
	}

	~workerThreadControl ()
	{
		for ( auto &it : threads )
		{
			// TODO: come up with a good way to terminate workers
			it.detach ();
		}
	}

	void send (Args... args )
	{
#if defined ( MULTIWORKER )
		std::lock_guard g1 ( queueMutex );

		for ( int64_t workerCount = 0; workerCount < std::thread::hardware_concurrency () - 1; workerCount++ )
		{
			waitSemaphore.acquire ();
			queue.push ( std::tuple<Args...>(args...) );
		}
		std::unique_lock<std::mutex> lk ( cvMutex );
		cv.notify_all ();
#endif
	}

	void wait ()
	{
#if defined ( MULTIWORKER )
		for ( int64_t workerCount = 0; workerCount < std::thread::hardware_concurrency () - 1; workerCount++ )
		{
			waitSemaphore.acquire ();
		}
		for ( int64_t workerCount = 0; workerCount < std::thread::hardware_concurrency () - 1; workerCount++ )
		{
			waitSemaphore.release ();
		}
#endif
	}
};

static workerThreadControl<decltype(processScanQueue)>  workers ( processScanQueue );

void startProcessScanQueue( compExecutable* compDef, opFile* file, bool markUsed, bool interProcedural, unique_queue<accessorType>* scanQueue, bool isLS )
{
#if 1
	std::vector<std::thread>  threads;

#if 1
	for ( int64_t workerCount = 0; workerCount < std::thread::hardware_concurrency() - 1; workerCount++ )
	{
		threads.push_back( std::thread( processScanQueue, compDef, file, false, true, scanQueue, isLS ) );

#if defined WIN32
		SetThreadPriority ( threads.back ().native_handle (), THREAD_PRIORITY_LOWEST );
#endif
	}
#endif

	processScanQueue( compDef, file, false, true, scanQueue, isLS );

	for ( auto& it : threads )
	{
		it.join();
	}
#else
//	printf ( "%zu\r\n", scanQueue->size () );
	workers.send ( compDef, file, markUsed, interProcedural, scanQueue, isLS );
	processScanQueue ( compDef, file, markUsed, interProcedural, scanQueue, isLS );
	workers.wait ();
#endif
}

void optimizer::makeKnown ( cacheString const &entry, bool isLS )
{
	unique_queue<accessorType> scanQueue;
	symbolStack				sym ( file );
	class opUsingList		useList;
	symbolSpaceNamespace	ns ( file, &file->ns, &useList );
	sym.push ( &ns );

	//printf ( "********************************************* MAKE KNOWN ***********************************************************\r\n" );

	auto systemError = compDef->file->sCache.get ( "systemError" );
	// reset the class inuse information... this will allow us to mark the special functions appropriately.
	for ( auto &cls : compDef->classDef )
	{
		if ( cls->oClass->name.c_str ()[0] != '$' )
		{
			if ( cls->oClass->name != compDef->file->aArrayValue &&
				 cls->oClass->name != systemError )
			{
				cls->oClass->inUse = false;
			}

		}
	}

	makeKnown ( &scanQueue, false );

	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		if ( !it->second->isInterface )
		{
			symbolStack	localSym ( file, it->second );
			convertPseudoCall ( &localSym, it->second, &scanQueue, isLS );
		}
	}

	if ( !isLS && 1 )
	{
		// this is done to mark any in use in our main code that are called by libraries
		for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
		{
			if ( it->second->isInterface )
			{
				scanQueue.push ( opForceScan ( it->second ) );
			}
		}
		startProcessScanQueue ( compDef, file, true, true, &scanQueue, isLS );
	}

#if 1
	// now that we've made everthing known that needs to be known... we need to rescan everyting one last time...
	// this is necessary to ensure that any function that was operating on 
	for ( auto &it : file->symbols )
	{
		if ( it.second.loadTimeInitializable && !it.second.isInterface )
		{
			auto init = it.second.initializer;
			if ( init )
			{
				scanQueue.push ( &it.second );
			}
		}
	}

	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		if ( !it->second->isInterface && it->second->inUse )
		{
			scanQueue.push ( opForceScan ( it->second ) );
		}
	}

	// reset all IVAR's, const and static types to undefined
	for ( auto &cls : compDef->classDef )
	{
		if ( !cls->isInterface )
		{
			for ( auto &elem : cls->elements )
			{
				switch ( elem.type )
				{
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
						scanQueue.push ( elem.elem );
						break;
					default:
						break;
				}
			}
		}
	}
#endif

	while ( !scanQueue.empty () )
	{
		startProcessScanQueue ( compDef, file, true, true, &scanQueue, isLS );
		// repeat make known... we may have now brought in new functions and need to repeat until all call paths are completely infered and settled
		makeKnown ( &scanQueue, false );
	} while ( !scanQueue.empty () );
}

void optimizer::doTypeInference( cacheString const& entry, bool isLS )
{
	auto mainIt = file->functionList.find( entry );
	unique_queue<accessorType> scanQueue;

	symbolStack				sym( file );
	class opUsingList		useList;
	symbolSpaceNamespace	ns( file, &file->ns, &useList );
	sym.push( &ns );

	//printf ( "********************************************* TYPE INFER ***********************************************************\r\n" );

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
		for ( auto &cls : compDef->classDef )
		{
			if ( !cls->isInterface )
			{
				cls->oClass->accessors.clear ();
				for ( auto &elem : cls->elements )
				{
					switch ( elem.type )
					{
						case fgxClassElementType::fgxClassType_iVar:
							elem.elem->symType.forceVariant ();
							elem.elem->typeChanged = false;
							elem.elem->accessors.clear ();
							break;
						case fgxClassElementType::fgxClassType_static:
						case fgxClassElementType::fgxClassType_const:
							elem.elem->symType.forceVariant ();
							elem.elem->typeChanged = false;
							elem.elem->accessors.clear ();
							scanQueue.push ( elem.elem );
							break;
						default:
							break;
					}
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
		startProcessScanQueue( compDef, file, false, false, &scanQueue, isLS );

		if ( !isLS )
		{
			for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
			{
				if ( (*it).second->isIterator && (*it).second->inUse )
				{
					errorLocality e ( &file->errHandler, (*it).second->location );
					compDef->compMakeIterator ( file, (*it).second, true, accessorType (), &scanQueue, isLS );
					scanQueue.push ( opForceScan ( (*it).second ) );
				}
			}

			startProcessScanQueue ( compDef, file, false, false, &scanQueue, isLS );
		}
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

	startProcessScanQueue( compDef, file, false, true, &scanQueue, isLS );

	if ( !isLS )
	{
		if ( file->dependenciesFromExternalSources.size () )
		{
			for ( auto &it : file->dependenciesFromExternalSources )
			{
				auto funcIt = file->functionList.find ( it );
				if ( funcIt != file->functionList.end () )
				{
					if ( !funcIt->second->inUse )
					{
						funcIt->second->inUse = true;
						scanQueue.push ( funcIt->second );
					}
					if ( funcIt->second->classDef )
					{
						markClassInuse ( compDef, accessorType (), funcIt->second->classDef, &sym, &scanQueue, isLS );
					}
				}
			}
			startProcessScanQueue ( compDef, file, false, true, &scanQueue, isLS );
		}
	}

	if ( !isLS )
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
			if ( !(*it).second->inUse && (*it).second->isIterator )
			{
				errorLocality e( &file->errHandler, (*it).second->location );
				scanQueue.push( opForceScan( (*it).second ) );
			}
		}
		startProcessScanQueue( compDef, file, false, true, &scanQueue, isLS );
		for ( auto& it : file->functionList )
		{
			it.second->inUse = it.second->inUseSave;
		}
		propagateConstPure( file );
	}
}

void optimizer::removeUnused ( cacheString const &entry, bool isLS )
{
	unique_queue<accessorType>	scanQueue;
	symbolStack					sym ( file );
	class opUsingList			useList;
	symbolSpaceNamespace		ns ( file, &file->ns, &useList );
	sym.push ( &ns );

	//printf ( "********************************************* REMOVE UNUSED ***********************************************************\r\n" );

	bool unknownClassInstantiation = false;
	bool unknownFunctionCall = false;

	for ( auto &it : file->functionList )
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
		for ( auto &it : file->classList )
		{
			markClassMethodsInuse ( compDef, accessorType (), it.second, &sym, &scanQueue, true, isLS );
		}
	}

	// NOTE: if we don't have a DIRECT link to a function we really don't give a crap about constness/purity.
	//		 the only reason we care about constness/purity is if we have all direct links to functions so that we can potentially optimize out calls
	//		 and or warn about unused returns from const functions.   If we don't have a direct call non of those (nor other optimizations) apply
	propagateConstPure ( file );

	std::unordered_map<cacheString, opFunction *>  funcsToBeDeleted;

	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		if ( (*it).second->isIterator && (*it).second->inUse )
		{
			errorLocality e ( &file->errHandler, (*it).second->location );
			compDef->compMakeIterator ( file, (*it).second, true, accessorType (), &scanQueue, isLS );
			scanQueue.push ( opForceScan ( (*it).second ) );
		}
	}

	// process the scanQueue
	startProcessScanQueue ( compDef, file, true, false, &scanQueue, isLS );

	if ( !unknownFunctionCall )
	{
		// mark unused functions for deletion
		for ( auto funcIt = file->functionList.begin (); funcIt != file->functionList.end (); funcIt++ )
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
						// we need to do this as it's possible that a functor can be inlined.   this will leave no new() sites for the class, although the inline will cause the 
						markClassInuse ( compDef, accessorType (), funcIt->second->classDef, &sym, &scanQueue, isLS );
					}
				}
			} else if ( funcIt->second->inUse )
			{
				if ( funcIt->second->classDef )
				{
					// we need to do this as it's possible that a functor can be inlined.   this will leave no new() sites for the class, although the inline will cause the 
					markClassInuse ( compDef, accessorType (), funcIt->second->classDef, &sym, &scanQueue, isLS );
				}
			}
		}
	} else if ( !unknownClassInstantiation )
	{
		// even thought we can't delete unused functions, we MAY still be able to remove unused class methods/property set/get'ers
		for ( auto funcIt = file->functionList.begin (); funcIt != file->functionList.end (); funcIt++ )
		{
			if ( !funcIt->second->inUse && !funcIt->second->inlineUse && funcIt->second->classDef )
			{
				funcsToBeDeleted[funcIt->first] = funcIt->second;
				funcIt->second->toBeDeleted = true;
			} else if ( funcIt->second->inUse )
			{
				if ( funcIt->second->classDef )
				{
					// we need to do this as it's possible that a functor can be inlined.   this will leave no new() sites for the class, although the inline will cause the 
					markClassInuse ( compDef, accessorType (), funcIt->second->classDef, &sym, &scanQueue, isLS );
				}
			}
		}
	}

	for ( auto it = file->classList.begin (); it != file->classList.end (); )
	{
		if ( !it->second->inUse )
		{
			for ( auto cIt = compDef->classDef.begin (); cIt != compDef->classDef.end (); cIt++ )
			{
				if ( *cIt == it->second->cClass )
				{
					compDef->classDef.erase ( cIt );
					break;
				}
			}
			//printf ( "deleting: %s\r\n", it->second->name.c_str () );
			delete it->second;
			it = file->classList.erase ( it );
		} else
		{
			it++;
		}
	}

	// go through and remove any class methods/props for which we no longer have a valid element;  this will happen when we simply never call that method
	// since inUse is marked within the function definition, if the function is no longer present (has been removed due to non-use) then we use this as
	// the indicator to remove the method.
	for ( auto &cls : compDef->classDef )
	{
		for ( size_t index = 0; index < cls->overload.size (); index++ )
		{
			if ( cls->overload[index] && cls->oClass->opOverload[index] )
			{
				auto funcIt = file->functionList.find ( cls->oClass->opOverload[index]->data.method.func );
				if ( funcIt->second->toBeDeleted )
				{
					delete cls->oClass->opOverload[index];
					deleteClassElement ( cls, (ptrdiff_t) cls->overload[index] - 1 );
					cls->oClass->opOverload[index] = 0;
					cls->overload[index] = 0;
				}
			}
		}
		for ( auto elemIt = cls->elements.begin (); elemIt != cls->elements.end (); )
		{
			switch ( elemIt->elem->type )
			{
				case fgxClassElementType::fgxClassType_method:
					if ( elemIt->elem->data.method.func )
					{
						auto funcIt = file->functionList.find ( elemIt->elem->data.method.func );
						if ( funcIt->second->toBeDeleted )
						{
							for ( auto &oElem : cls->oClass->elems )
							{
								if ( elemIt->elem == oElem )
								{
									oElem->name = cacheString (); // don't erase as we have hard links back so can't cause vector to reallocate, but simply make it unfindable
									oElem->data.method.func = cacheString ();
									break;
								}
							}
							elemIt = deleteClassElement ( cls, std::distance ( cls->elements.begin (), elemIt ) );
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
							auto funcIt = file->functionList.find ( elemIt->elem->data.prop.access );
							if ( funcIt->second->toBeDeleted )
							{
								elemIt->elem->data.prop.access = cacheString ();
								elemIt->methodAccess.func = nullptr;
							}
						}
						if ( elemIt->elem->data.prop.assign )
						{
							auto funcIt = file->functionList.find ( elemIt->elem->data.prop.assign );
							if ( funcIt->second->toBeDeleted )
							{
								elemIt->elem->data.prop.assign = cacheString ();
								elemIt->assign.func = nullptr;
							}
						}

						if ( !elemIt->elem->data.prop.access.size () )
						{
							for ( auto &oElem : cls->oClass->elems )
							{
								if ( elemIt->elem == oElem )
								{
									oElem->data.prop.access = cacheString ();
									break;
								}
							}
						}
						if ( !elemIt->elem->data.prop.assign.size () )
						{
							for ( auto &oElem : cls->oClass->elems )
							{
								if ( elemIt->elem == oElem )
								{
									oElem->data.prop.assign = cacheString ();
									break;
								}
							}
						}
						if ( !elemIt->elem->data.prop.access.size () && !elemIt->elem->data.prop.assign.size () )
						{
							for ( auto &oElem : cls->oClass->elems )
							{
								if ( elemIt->elem == oElem )
								{
									oElem->name = cacheString (); // don't erase as we have hard links back so cant cause vector to reallocate, but simply make it unfindable
									break;
								}
							}
							elemIt = deleteClassElement ( cls, std::distance ( cls->elements.begin (), elemIt ) );
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
	for ( auto const &it : funcsToBeDeleted )
	{
		file->functionList.erase ( file->functionList.find ( it.first ) );
		delete it.second;
	}
}