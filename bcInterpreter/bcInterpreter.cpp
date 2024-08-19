#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <math.h>
#include <intrin.h>

#include "bcInterpreter.h"
#include "op_variant.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"
#include "bcVM/bcVM.h"
#include "bcVM/vmDebug.h"
#include "bcVM/bcVMObject.h"
#include "bcVM/vmNativeInterface.h"
#include "Utility/sourceFile.h"

// for workarea/databse support
#include "compilerParser/fglErrors.h"

#define UNUSED(x) (void)(x)

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#define DO_THREADED 1
#else
#define DO_THREADED 0
#endif

bcFuncDef::bcFuncDef ()
{
}

bcFuncDef::~bcFuncDef ()
{
}
void createError ( class vmInstance *instance, bcFuncDef *funcDef, fglOp *op )
{
	VAR *stack;

	stack = instance->stack;
	*(stack++) = *classNew ( instance, "systemError", 0 );
}

fglOp *vmInstance::catchHandler ( bcFuncDef *funcDef, fglOp *ops, VAR *basePtr, int64_t waSave, VAR const &exception )
{
	fgxTryCatchEntry *tcEntry;
	size_t				 opOffset;

	opOffset = (ops - funcDef->loadImage->csBase);

	auto breakType = debug->GetExceptionBreakType ( errorNum ( error.errorNum ) );

	if ( !error.valid )
	{
		error.valid = true;								// prevent overwrite during rethrow
		error.funcDef = funcDef;
		error.stack = stack;
		error.opCode = (ptrdiff_t) & (funcDef->loadImage->csBase)[opOffset];
		error.errorNum = exception.type == slangType::eLONG ? exception.dat.l : int64_t ( errorNum::scCUSTOM );
		error.breakMode = breakType;
	}

	for ( tcEntry = funcDef->tcList; tcEntry->tryOffset != 0xFFFFFFFF; tcEntry++ )
	{
		if ( (opOffset >= tcEntry->tryOffset) && (opOffset < tcEntry->catchOffset) )
		{
			if ( opOffset <= (tcEntry + 1)->tryOffset )
			{
				// we're in a try catch..

				if ( debug->enabled )
				{
					if ( breakType == errorLocation::exceptionBreakType::always || breakType == errorLocation::exceptionBreakType::handled )
					{
						// signal it in the debugger
						auto newOp = debugSignal ( this, vmStateType::vmHaltTrapped, ops, funcDef, stack );

						if ( newOp != ops )
						{
							// this was handled in the debugger, so do NOT call the catch.
							stack = basePtr + debug->stackHeight;
							error.valid = false;
							return newOp;
						}
					}
				}

				// set the result
				basePtr[tcEntry->errSym] = exception;

				// restore state
				this->stack = basePtr + tcEntry->stackSize;
				workareaTable->nStack = waSave;

				// caught the error, so this will no longer be a primary error location
				error.valid = false;

				// jump to catch
				return &(funcDef->loadImage->csBase)[tcEntry->catchOffset];
			}
		}
	}

	// walk the stack and see if this will be caught somewhere else.  if not and we have a debugger than make the exception visible to the developer
	if ( debug->enabled )
	{
		for ( auto sPtr = stack - 1; sPtr > eval; sPtr-- )
		{
			if ( sPtr->type == slangType::ePCALL )
			{
				auto parentFunc = sPtr->dat.pCall.func;
				auto parentOp = sPtr->dat.pCall.op;
				opOffset = (parentOp - parentFunc->loadImage->csBase);

				for ( tcEntry = parentFunc->tcList; tcEntry->tryOffset != 0xFFFFFFFF; tcEntry++ )
				{
					if ( (opOffset >= tcEntry->tryOffset) && (opOffset < tcEntry->catchOffset) )
					{
						if ( opOffset <= (tcEntry + 1)->tryOffset )
						{
							if ( breakType == errorLocation::exceptionBreakType::always || breakType == errorLocation::exceptionBreakType::handled )
							{
								// signal it in the debugger
								auto newOp = debugSignal ( this, vmStateType::vmHaltTrapped, ops, funcDef, stack );

								if ( newOp != ops )
								{
									// this was handled in the debugger, so do NOT call the catch.
									stack = basePtr + debug->stackHeight;
									error.valid = false;
									return newOp;
								}
							}
							break;
						}
					}
				}
			}
		}

		if ( breakType == errorLocation::exceptionBreakType::always || breakType == errorLocation::exceptionBreakType::unhandled )
		{
			// signal it in the debugger
			auto newOp = debugSignal ( this, vmStateType::vmHalt, ops, funcDef, stack );

			if ( newOp == ops )
			{
				// did not change location, so we did not do anything to rectify this exception so rethrow it
				throw;
			}
			stack = basePtr + debug->stackHeight;
			error.valid = false;
			return newOp;
		}
	}
	// no debugger, didn't catch it, so rethrow it to next handler
	throw;
}

VAR *vmInstance::interpretBCParam ( bcFuncDef *funcDef, bcFuncDef *destFunc, fglOp *ops, uint32_t  nParams, VAR *stack, VAR *basePtr )
{
	VAR *varTmp;
#if defined ( _NDEBUG )
	uint64_t			  profTimeStart;
	uint64_t			  profTimeCallStart;
	uint64_t			  profTimeCalls;
	size_t				  nCallsSave;
#else
	uint64_t			  profTimeStart = 0;
	uint64_t			  profTimeCallStart = 0;
	uint64_t			  profTimeCalls = 0;
	size_t				  nCallsSave = 0;
#endif

#define RETURN(val) return val;
#include "bcInterpreterOps.Inc"
	assert ( false );
	return 0;
}

void vmInstance::interpretBC ( bcFuncDef *funcDef, fglOp *retAddr, uint32_t nParams )
{
	VAR *varTmp;
	auto basePtr = this->stack - funcDef->nParams;
	auto stack = this->stack;
	auto waSave = workareaTable->nStack;

#if defined ( _NDEBUG )
	uint64_t			  profTimeStart;
	uint64_t			  profTimeCallStart;
	uint64_t			  profTimeCalls;
	size_t				  nCallsSave;
#else
	uint64_t			  profTimeStart = 0;
	uint64_t			  profTimeCallStart = 0;
	uint64_t			  profTimeCalls = 0;
	size_t				  nCallsSave = 0;
#endif

	stack->type = slangType::ePCALL;
	stack->dat.pCall.op = retAddr;
	stack->dat.pCall.func = funcDef;
	stack->dat.pCall.nParams = nParams;
	stack++;

	auto ops = funcDef->cs;
	//	printf ( "%s\r\n", funcDef->name );
	for ( ;; )
	{
		try
		{
#define destFunc  funcDef
#undef RETURN
#define RETURN(val) return;
#include "bcInterpreterOps.Inc"
#undef destFunc
			return;
		} catch ( errorNum err )
		{
			if ( err == errorNum::scUSER_TERMINATE ) throw;
#if defined ( _DEBUG )
			if ( err == errorNum::scINTERNAL )
			{
				DebugBreak ();
			}
#endif
			ops = catchHandler ( funcDef, ops, basePtr, waSave, VAR ( (int64_t)err ) );
			stack = this->stack;
		} catch ( VAR *var )
		{
			ops = catchHandler ( funcDef, ops, basePtr, waSave, *var );
			stack = this->stack;
		} catch ( ... )
		{
			ops = catchHandler ( funcDef, ops, basePtr, waSave, VAR ( (int64_t)errorNum::scINTERNAL ) );
			stack = this->stack;
		}
	}
}

void vmInstance::printCallStack ( VAR *stack, fglOp *op )
{
	if ( !stack )
	{
		return;
	}

	vmCallStack *callStack = new vmCallStack();

	auto evalPtr = stack - 1;
	while ( evalPtr >= eval )
	{
		if ( evalPtr->type == slangType::ePCALL )
		{
			callStack->entry.push_back ( vmCallstackEntry ( this, evalPtr->dat.pCall.func, debug->GetCurrentLine ( evalPtr->dat.pCall.func, op ), evalPtr->dat.pCall.nParams, evalPtr ) );
			op = evalPtr->dat.pCall.op;
		}
		evalPtr--;
	}

	for ( auto &it : callStack->entry )
	{
		printf ( "%s(%i) %s ( ", it.fileName.c_str(), (int)it.line, it.funcName.c_str() );

		for ( size_t loop2 = 0; loop2 < (int)it.params.size (); loop2++ )
		{
			if ( it.params[loop2]->isStringValue () )
			{
				printf ( "\"%.32s\"", it.params[loop2]->getValue () );
				if ( it.params[loop2]->getValueLen () > 32 )
				{
					printf ( "..." );
				}
			} else
			{
				printf ( "%.32s", it.params[loop2]->getValue () );
			}
			if ( loop2 != it.params.size () - 1 )

			{
				printf ( ", " );
			}
		}

		printf ( " )\r\n" );
	}
	delete callStack;
}

void vmInstance::run ( char const *name, bool halt )
{
	bcFuncDef *funcDef = nullptr;
	bcLoadImage *loadImage = {};
	VAR *stack;

	debug->reset ();
	try
	{
		funcDef = atomTable->atomGetFunc ( name );

		if ( !funcDef )
		{
			throw errorNum::scMISSING_ENTRY_POINT;
		}

		stack = eval + 1;

#if _DEBUG
		memset ( eval, uint8_t ( slangType::eDEBUG_MARKER ), sizeof ( VAR ) * TASK_EVAL_STACK_SIZE );		// don't fill with NULLS, doing so hides some potentential use-before bugs
#endif
		debugSignal ( this, vmStateType::vmLoad, 0, 0, 0 );
		if ( halt ) debug->halt = true;

		// for each load image allocate and reserve OM memory for each of our global symbols (globals, global statics, function statics, class constants, class statics)
		// we initialize the loadImage globals array to point to the allocated OM memory
		atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
							 {
								 loadImage = (bcLoadImage *)atomTable->atomGetLoadImage ( index );
								 auto globals = om->allocVar ( loadImage->nGlobals );

								 for ( size_t loop = 0; loop < loadImage->nGlobals; loop++ )
								 {
									 loadImage->globals[loop] = &(globals[loop]);
									 loadImage->globals[loop]->type = slangType::eNULL;
									 loadImage->globalDefs[loop].globalIndex = loop;
									 loadImage->globalDefs[loop].image = loadImage;
								 }

								 // for each global we point ourselves at the first definition found.   this should be true regardless of load image.
								 // it doesn't matter WHICH definition we use... they will all point to the same one, and that definition will be whatever comes first in the atm
								 // this results in (nLoadImages - 1) * totalGlobalSymbols of wasted om memory... these will always simply be NULL values so should have minimal impact on GC

								 auto nExportable = loadImage->globalsExportRoot;
								 while ( nExportable )
								 {
									 auto global = &loadImage->globalSymbolTable[nExportable - 1];
									 assert ( global->isExportable );
									 auto gbl = findGlobal ( global->name, loadImage );
									 if ( std::get<0> ( gbl ) )
									 {
										 // someone else already has this defined... use their definition
										 loadImage->globals[global->index] = std::get<0> ( gbl );
										 loadImage->globalDefs[nExportable - 1].image = std::get<1> ( gbl );
										 loadImage->globalDefs[nExportable - 1].globalIndex = std::get<2> ( gbl );
									 }
									 nExportable = global->nextExportable;
								 }
								 return false;
							 }
		);

		bcFuncDef	 fDef;

		// call code to intialize the globals for each load image
		atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
							 {
								 loadImage = (bcLoadImage *)atomTable->atomGetLoadImage ( index );
								 if ( loadImage->globalInit )
								 {
									 fDef.cs = loadImage->globalInit;
									 fDef.loadImage = loadImage;
									 fDef.name = "Global Initializers";

									 stack = this->stack;
									 stack->type = slangType::ePCALL;
									 stack->dat.pCall.op = 0;
									 stack->dat.pCall.func = &fDef;
									 stack->dat.pCall.nParams = 0;
									 stack++;

									 this->stack = stack;
									 interpretBCParam ( &fDef, &fDef, loadImage->globalInit, 0, stack, stack );
									 this->stack = stack - 1;
								 }
								 return false;
							 }
		);

		debugSignal ( this, vmStateType::vmThreadStart, nullptr, funcDef, nullptr );
		debug->halt = debug->halt || halt;
		interpretBC ( funcDef, 0, funcDef->nParams );
		debugSignal ( this, vmStateType::vmThreadEnd, nullptr, funcDef, nullptr );
	} catch ( errorNum err )
	{
		if ( err == errorNum::scUSER_TERMINATE )
		{
			if ( error.valid )
			{
				err = (errorNum)error.errorNum;
				log( logger::level::ERROR, logger::modules::VM, "%x - %s\r\n", int32_t ( err ), scCompErrorAsText( size_t( err ) ).c_str() );
				printCallStack ( error.stack, (fglOp *)error.opCode );	// NOLINT(performance-no-int-to-ptr)
			}
		} else
		{
			log ( logger::level::ERROR, logger::modules::VM, "%x - %s\r\n", int32_t ( err ), scCompErrorAsText ( size_t ( err ) ).c_str () );
			printCallStack ( error.stack, (fglOp *)error.opCode );	// NOLINT(performance-no-int-to-ptr)
		}
		debugSignal ( this, vmStateType::vmThreadEnd, nullptr, funcDef, nullptr );
		throw;
	} catch ( unsigned long err )
	{
		log ( logger::level::ERROR, logger::modules::VM, "%x - %s\r\n", int32_t ( err ), scCompErrorAsText ( size_t ( err ) ).c_str () );
		printCallStack ( error.stack, (fglOp *)error.opCode );	// NOLINT(performance-no-int-to-ptr)
		debugSignal ( this, vmStateType::vmThreadEnd, nullptr, funcDef, nullptr );
		throw;
	} catch ( VAR *var )
	{
		UNUSED ( var );
		log ( logger::level::ERROR, logger::modules::VM, "user defined" );
		printCallStack ( error.stack, (fglOp *)error.opCode );	// NOLINT(performance-no-int-to-ptr)
		debugSignal ( this, vmStateType::vmThreadEnd, nullptr, funcDef, nullptr );
		throw;
	} catch ( ... )
	{
		log ( logger::level::ERROR, logger::modules::VM, "internal VM" );
		printCallStack ( error.stack, (fglOp *)error.opCode );	// NOLINT(performance-no-int-to-ptr)
		debugSignal ( this, vmStateType::vmThreadEnd, nullptr, funcDef, nullptr );
		throw;
	}
}
