/*
    SLANG multitasker.   PROPRIETARY
*/

#include "Utility/settings.h"

#include "stdafx.h"
#include <process.h>
#include <TlHelp32.h>

#include "vmTask.h"
#include "vmConf.h"
#include "bcVM/bcVM.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"
#include "bcVM/vmInstanceDispatch.h"
#include "bcVM/vmOmCopying.h"
#include "bcVM/vmOmGenerational.h"
#include "bcVM/vmWorkarea.h"
#include "bcVM/vmDebug.h"
#include "compilerParser/fglErrors.h"
#include "Utility/buffer.h"

#undef NDEBUG
//#define NDEBUG 1

struct THREADNAME_INFO
{
  size_t dwType; // must be 0x1000
  LPCSTR szName; // pointer to name (in user addr space)
  size_t dwThreadID; // thread ID (-1=caller thread)
  size_t dwFlags; // reserved for future use, must be zero
};

void SetThreadName( size_t dwThreadID, LPCSTR szThreadName)
{
	THREADNAME_INFO info = {};
	{
		info.dwType = 0x1000;
		info.szName = szThreadName;
		info.dwThreadID = dwThreadID;
		info.dwFlags = 0;
	}
	__try
	{
		RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(size_t), (const ULONG_PTR *)&info );
	}
	__except (EXCEPTION_CONTINUE_EXECUTION)
	{
		return;
	}
}

vmTaskInstance::vmTaskInstance( char const *name )
{
	threadId = 0;
	heap = HeapCreate( HEAP_GENERATE_EXCEPTIONS, 0, 0 );

	switch ( vmConf.vmCollector )
	{
		case vmCollectorType::colGenerational:
//			printf ( "Intializing generational garbage collector\r\n" );
			om = new omGenGc ( this );
			break;
		case vmCollectorType::colCopy:
		default:
//			printf ( "Intializing copying garbage collector\r\n" );
			pageCache = new objectPageCache( vmConf.vmObjectDefaultPageSize, vmConf.vmObjectMemoryMax );
			om = new cachedObjectMemory( this );
			break;
	}
	vmInstance::init();
	vmTaskInstance::setName( name );

	for ( auto &it : vmConf.modules )
	{
		it.instanceInit ( this );
	}
}

void vmTaskInstance::duplicate ( class vmInstance *instance )
{
	auto old = dynamic_cast<vmTaskInstance *>(instance);

	vmInstance::duplicate ( instance );
	preLoadedSource = old->preLoadedSource;
	preLoadedCode = old->preLoadedCode;
}


vmTaskInstance::~vmTaskInstance()
{
	for ( auto &it : vmConf.modules )
	{
		it.instanceInit ( this );
		CloseHandle ( it.hInst );
	}

	if ( IsThreadAFiber() )
	{
		ConvertFiberToThread();
		this->mainCoroutine = 0;
	}
	release();
	switch ( vmConf.vmCollector )
	{
		case vmCollectorType::colCopy:
		default:
			delete pageCache;
			break;
	}
	vmTaskInstance::freeEvalStack();
	HeapDestroy( heap );
}


uint64_t vmTaskInstance::getTicks ( void )
{
	LARGE_INTEGER val;

	QueryPerformanceCounter ( &val );

	return val.QuadPart;
}

void vmTaskInstance::setName ( char const *name )
{
	strcpy_s ( id, sizeof ( id ), name );
	SetThreadName ( GetCurrentThreadId(), id );
}

void vmTaskInstance::allocateEvalStack ( size_t nEntries )
{
	size_t			 dwPageSize;
	size_t			 stackSize;
	DWORD			 oldProtect;
    SYSTEM_INFO		 sSysInfo;

	// create the eval stack
    GetSystemInfo(&sSysInfo);
    dwPageSize = sSysInfo.dwPageSize;
	stackSize = (((sizeof ( VAR ) * nEntries) + dwPageSize + (dwPageSize -1)) / dwPageSize) * dwPageSize;
	eval = (VAR *)VirtualAlloc ( 0, stackSize, MEM_COMMIT, PAGE_READWRITE  );
	oldProtect = 0;
	VirtualProtect ( (char *)eval + stackSize - dwPageSize, dwPageSize, PAGE_READWRITE | PAGE_GUARD, &oldProtect );
}

void vmTaskInstance::freeEvalStack ( void )
{
	VirtualFree ( eval, 0, MEM_RELEASE );
}


void *vmTaskInstance::malloc ( size_t len )
{
#if _DEBUG
	return ::malloc ( len );
#else
	return HeapAlloc ( heap, 0, len );
#endif
}

void vmTaskInstance::free ( void *ptr )
{
#if _DEBUG
	::free ( ptr );
	return;
#else
 	HeapFree ( heap, 0, ptr );
#endif
}

void vmTaskInstance::reset ( void )
{
	vmInstance::reset ();
}

void vmTaskInstance::validateHeap ( void )
{
	HeapValidate ( heap, 0, 0 );
}

static void CALLBACK fiberInit ( void *param )
{
	vmFiber					*fiber = (vmFiber *)param;

	fiber->status = vmCoroutine::vmCoroutineStatus::vmCoroutine_init;

	SwitchToFiber ( fiber->creatingFiber->fiber );

	switch ( fiber->funcDef->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			fiber->instance->interpretBC ( fiber->funcDef, fiber->funcDef->cs, fiber->nParams );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			fiber->instance->funcCall ( fiber->funcDef, fiber->nParams );
			break;
		default:
			throw errorNum::scUNSUPPORTED;
	}


	fiber->status = vmCoroutine::vmCoroutineStatus::vmCoroutine_complete;

	vmFiber	*coRoutine;

	if ( fiber->instance->coRoutineStack.size() )
	{
		coRoutine = static_cast<vmFiber *>(fiber->instance->coRoutineStack.top());
		coRoutine->instance->coRoutineStack.pop();

		fiber->instance->currentCoroutine	= coRoutine;
		fiber->instance->eval				= coRoutine->evalStack;
		fiber->instance->stack				= coRoutine->stack;
		fiber->instance->nStack				= coRoutine->nStack;

		if ( coRoutine->getStatus() == vmCoroutine::vmCoroutineStatus::vmCoroutine_complete )
		{
			throw errorNum::scCOROUTINE_CONTINUE;
		}

		SwitchToFiber ( coRoutine->fiber );
	} else
	{
		throw errorNum::scCOROUTINE_NORETURN;
	}
}

vmFiber::vmFiber ( class vmInstance *instance, VAR *func, size_t nEvalStack, size_t cStackSize )
{
	size_t			 dwPageSize;
	size_t			 stackSize;
	DWORD			 oldProtect;
    SYSTEM_INFO		 sSysInfo;

	if ( !IsThreadAFiber () )
	{
		instance->mainCoroutine = new vmFiber ( instance );
		instance->currentCoroutine = instance->mainCoroutine;
		instance->coRoutines.push_back( instance->mainCoroutine );			// must be on the list so we will collect... othewise we lose our eval stack
	}
	instance->coRoutines.push_back( this );		// add new coroutine to list so we collect (and find it later on)

	// create the eval stack
    GetSystemInfo(&sSysInfo);
    dwPageSize = sSysInfo.dwPageSize;
	stackSize = (((sizeof ( VAR ) * nEvalStack ) + dwPageSize + (dwPageSize -1)) / dwPageSize) * dwPageSize;
	evalStack = (VAR *)VirtualAlloc ( 0, stackSize, MEM_COMMIT, PAGE_READWRITE  );
	VirtualProtect ( (char *)evalStack + stackSize - dwPageSize, dwPageSize, PAGE_READWRITE | PAGE_GUARD, &oldProtect );

	nStack = stackSize / sizeof ( VAR );	// number that can fit in the pages we allocated... we round up so this may be greater than the number passed in.
	this->instance = instance;
	stack = evalStack;

	this->func = (VAR *)instance->om->allocVar( 1 );
	*this->func = *func;

	status = vmCoroutine::vmCoroutineStatus::vmCoroutine_init;

	switch ( func->type )
	{
		case slangType::eATOM:
			funcDef = instance->atomTable->atomGetFunc ( func->dat.atom );
			break;
		case slangType::eOBJECT_ROOT:
			{
				if ( !func->dat.ref.v )
				{
					throw errorNum::scINVALID_PARAMETER;
				}
				auto callerClass = func->dat.ref.v->dat.obj.classDef;

				if ( !callerClass->ops[int(fgxOvOp::ovFuncCall)] )
				{
					throw errorNum::scINVALID_OPERATOR;
				}

				cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

				if ( cEntry->isVirtual )
				{
					funcDef = instance->atomTable->atomGetFunc ( func->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
				} else
				{
					funcDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
				}
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}

	if ( !funcDef->coRoutineAble )
	{
		throw errorNum::scCOROUTINE_NONVARIANT;
	}

	setReturnIP ( funcDef->cs );

	fiber = CreateFiberEx ( cStackSize, cStackSize, FIBER_FLAG_FLOAT_SWITCH, fiberInit, this ); 

	creatingFiber = static_cast<vmFiber *>(instance->currentCoroutine);

	SwitchToFiber ( fiber );
}

vmFiber::vmFiber ( class vmInstance *instance )
{
	fiber = ConvertThreadToFiberEx ( this, FIBER_FLAG_FLOAT_SWITCH );
	evalStack		= instance->eval;
	nStack			= instance->nStack;
	stack			= instance->stack;
	status			= vmCoroutine::vmCoroutineStatus::vmCoroutine_running;
	this->instance	= instance;
}

vmFiber::~vmFiber ( void )
{
	for ( auto it = instance->coRoutines.begin(); it != instance->coRoutines.end(); it++ )
	{
		if ( *it == this )
		{
			instance->coRoutines.erase( it );
			break;
		}
	}
	DeleteFiber ( fiber );
	VirtualFree ( evalStack, 0, MEM_RELEASE );
}

void vmFiber::yield ( VAR *param, uint32_t nParams )
{
	if ( fiber == GetCurrentFiber() )
	{
		return;
	}

	(static_cast<vmFiber *>(instance->currentCoroutine))->stack = instance->stack;
	if ( instance->debug ) instance->currentCoroutine->setReturnIP ( instance->debug->IP );

	if ( status == vmCoroutine::vmCoroutineStatus::vmCoroutine_running )
	{
		instance->currentCoroutine	= this;
		instance->result			= *param;		// we RETURN the parameter passed
		instance->eval				= evalStack;
		instance->stack				= stack;
		instance->nStack			= nStack;
		SwitchToFiber ( fiber );
	} else if ( status == vmCoroutine::vmCoroutineStatus::vmCoroutine_init )
	{
		status = vmCoroutine::vmCoroutineStatus::vmCoroutine_running;

		// any NULL (not passed parameters... build that now
		for ( auto pIndex = funcDef->nParams; pIndex > nParams; pIndex-- )
		{
			if ( funcDef->defaultParams && funcDef->defaultParams[pIndex - 1] )
			{
				stack = instance->interpretBCParam ( funcDef, funcDef, funcDef->loadImage->csBase + funcDef->defaultParams[pIndex - 1], 0, instance->stack, stack );
			} else
			{
				(stack++)->type = slangType::eNULL;
			}
		}

		// copy the passed parameters
		for ( auto loop = 0; loop < (int)nParams; loop++ )
		{
			(*stack++) = *(instance->stack - nParams + loop - 1);
		}

		if ( funcDef->isMethod )
		{
			// write our object (in this case func with any adjustments) onto the stack
			if ( cEntry->isVirtual )
			{
				// fixup object to point to the virtual context
				(*stack++) = VAR_OBJ ( func, &func->dat.ref.v[func->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
			} else
			{
				(*stack++) = VAR_OBJ ( func, &func->dat.ref.v[cEntry->methodAccess.objectStart] );
			}
		}

		nParams = nParams >= (int)funcDef->nParams ? nParams :  (int)funcDef->nParams;

		this->nParams = nParams;

		// first initialization done... now switch to fiber to have it execute the code
		instance->currentCoroutine = this;
		instance->eval				= evalStack;
		instance->nStack			= nStack;
		instance->stack				= stack;
		SwitchToFiber ( fiber );
	}
}

VAR *vmFiber::getStack ( VAR **topOfStack )
{
	if ( this == this->instance->currentCoroutine )
	{
		*topOfStack = this->instance->stack;
		return this->instance->eval;
	} else
	{
		*topOfStack = this->stack;
		return this->evalStack;
	}
}

vmCoroutine	*vmTaskInstance::createCoroutine ( VAR *func, size_t evalStackSize, size_t cStackSize )
{
	return new vmFiber ( this, func, evalStackSize, cStackSize );
}

std::string GetSystemErrorAsString( uint32_t errNum )
{
	//Get the error message, if any.
	DWORD errorMessageID = errNum;
	if ( errorMessageID == 0 )
		return std::string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;

	size_t size = FormatMessageA ( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorMessageID, MAKELANGID ( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR) &messageBuffer, 0, NULL );
	if ( size )
	{
		std::string message ( messageBuffer, size );

		//Free the buffer.
		LocalFree ( messageBuffer );

		return message;
	}

	HANDLE hModuleSnap = CreateToolhelp32Snapshot ( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId ( ) );

	MODULEENTRY32 me32 = {};

	//  Set the size of the structure before using it. 
	me32.dwSize = sizeof ( MODULEENTRY32 );

	Module32First ( hModuleSnap, &me32 );

	do {
		size_t size = FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, GetModuleHandle ( _T ( me32.szModule) ), errorMessageID, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR)&messageBuffer, 0, NULL );
		if ( size )
		{
			std::string message ( messageBuffer, size );

			//Free the buffer.
			LocalFree ( messageBuffer );

			return message;
		}
	} while ( Module32Next ( hModuleSnap, &me32 ) );

	return "";
}

std::string GetSystemErrorAsNameString ( uint32_t errNum )
{
	BUFFER buff;

	buff.printf ( "0x%08zx", errNum );
	
	return buff.data<char *> ();
}

