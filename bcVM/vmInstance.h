/*
	instance structure for vm execution

*/

#pragma once

#include "Utility/settings.h"

#include <stack>
#include <map>
#include <utility>
#include <functional>
#include <tuple>

#include "bcInterpreter/bcInterpreter.h"
#include "bcVMObject.h"
#include "exeStore.h"
#include "fglTypes.h"
#include "vmAtom.h"
#include "vmDebug.h"
#include "vmObjectMemory.h"
#include "vmWorkarea.h"

#define TASK_EVAL_STACK_SIZE	65536
#define MAX_ATOMS				(65536 * 4)
#define MAX_PROFILE_DEPTH		 1024
#define CPU_STACK_SIZE			(1024 * 1024 * 64)


struct TASK_RESOURCE {
		  RES_MODE			  mode;
          void				 *resource;
          void				(*free) ( void *resource );
   struct TASK_RESOURCE		 *next;
};

struct errorLocation {
	bool		 valid = false;;
	bcFuncDef	*funcDef = nullptr;
	VAR			*stack = nullptr;			/* pointer to stack */
	ptrdiff_t	 opCode = 0;				/* opcode throwing error, we can look it up if debugging information is present to extract line information */
	size_t		 errorNum = 0;
};

class vmCoroutine {
public:
	enum class vmCoroutineStatus{
		vmCoroutine_init,
		vmCoroutine_running,
		vmCoroutine_complete
	};

	vmCoroutineStatus	status;
	fglOp				*ip;
public:
	virtual	~vmCoroutine () {};

	virtual	void	 yield		( VAR *param, uint32_t nParams )
	{
		throw errorNum::scUNSUPPORTED;
	}
	virtual	vmCoroutineStatus getStatus ( void )
	{
		throw errorNum::scUNSUPPORTED;
	}
	virtual VAR		*getStack	( VAR **topOfStack )
	{
		*topOfStack = 0;
		return 0;
	}
	virtual VAR		*getRoot( void )
	{
		throw errorNum::scUNSUPPORTED;
	}
	virtual fglOp	*getReturnIP ( void )
	{
		return ip;
	}
	virtual void	 setReturnIP ( fglOp *op )
	{
		ip = op;
	}
};

class vmInstance {
public:
	VAR							 *eval = nullptr;
	VAR							 *stack = nullptr;
	size_t						  nStack;

	VAR							  result;

	int							  outputDesc;

	class atomTable				 *atomTable = nullptr;
	class objectMemory			 *om = nullptr;
	class vmWorkareaTable		 *workareaTable = nullptr;

	bool						  initialized = false;;

	TASK_RESOURCE				 *firstResource = nullptr;

	/* debugger stuff */
	class	vmDebug				 *debug = nullptr;

	errorLocation				  error;						/* error location */

	/* profile stuff */
	time_t						  executionStartTime;
	time_t						  maxExecutionTime;
	int							  execStack[MAX_PROFILE_DEPTH];		/* call stack of executing functions for profiling */
	int							  nProfile;

	vmInstance ( char const *name = "(SYSTEM)" );
	virtual ~vmInstance();
	virtual void				  init						( void );
	virtual void				  allocateAtomTable			( void );

	virtual void				  release					( void );
	virtual void				  duplicate					( class vmInstance *instance );

	virtual void				  allocateEvalStack			( size_t numEntries );
	virtual void				  freeEvalStack				( void );
	virtual char const			 *getName					( void ) = 0;
	virtual void				  setName					( char const *name ) = 0;

	virtual	void				  reset						( void );

	int resourceRegister ( void *resource, void (*proc) (void *resource), RES_MODE mode )
	{
		TASK_RESOURCE *res;

		if ( !(res = (TASK_RESOURCE *) malloc ( sizeof ( TASK_RESOURCE ) * 1 )) )
		{
			return (0);
		}
		res->resource = resource;
		res->free = proc;
		res->next = firstResource;
		res->mode = mode;

		firstResource = res;

		return (1);
	}

	void resourceFree ( void *resource, void (*proc) (void *resource) )
	{
		auto res = firstResource;
		decltype(res) prev = nullptr;

		while ( res )
		{
			if ( (res->resource == resource) && (res->free == proc) )
			{
				if ( prev )
				{
					prev->next = res->next;
				} else
				{
					firstResource = res->next;
				}
				free ( res );
				return;
			}
			prev = res;
			res = res->next;
		}
	}

	void _resourceEndAll ()
	{
		for ( auto res = firstResource; res; )
		{
			(res->free) (res->resource);

			auto next = res->next;
			free ( res );
			res = next;
		}
		firstResource = nullptr;
	}

	void _resourceReset ()
	{
		auto res = firstResource;
		decltype(res) prev = nullptr;

		while ( res )
		{
			auto next = res->next;
			if ( res->mode == RES_MODE::RES_REINIT )
			{
				(res->free) (res->resource);
				if ( prev )
				{
					prev->next = res->next;
				} else
				{
					firstResource = res->next;
				}
				free ( res );
			}
			res = next;
		}
	}

	virtual void				 *malloc					( size_t len )
	{
		return ::malloc ( len );
	}
	virtual void				 free						( void *ptr )
	{
		::free ( ptr );	
	}
	virtual void				 *calloc					( size_t count, size_t size )
	{
		void *r;
		r = malloc ( count * size );
		memset ( r, 0, count * size );
		return r;
	}
	virtual char				 *_strdup					( char const *str )
	{
		const size_t len = strlen ( str ) + 1;
		char *ret = (char *)malloc ( len );
		memcpy ( ret, str, len );
		return ret;
	}
	virtual void				  validateHeap				( void ) {};

			vmCoroutine			 *currentCoroutine;
			vmCoroutine			 *mainCoroutine;
	std::list<vmCoroutine *>	  coRoutines;				// for garbage collection... we need to be able to enumerate all the coroutines to collect their stacks
	std::stack<vmCoroutine *>	  coRoutineStack;

	virtual vmCoroutine			 *createCoroutine			( VAR *func, size_t evalStackSize = 8192, size_t cStackSize = 128 * 256 )
	{
		throw errorNum::scUNSUPPORTED;
	}

	// load code into the vm
	virtual bool	 load					( std::shared_ptr<uint8_t> &buff, char const *name, bool isPersistant, bool isLibrary );

	// start executing the code at function "main".   This also does all the preliminaries such as initializing globals, etc.
	virtual void	 run					( char const *name, bool halt = false );

	// retrieve a ponter to a global variable
	virtual VAR		*getGlobal				( char const *nSpace, char const *name ) const;
	virtual std::tuple<VAR *, bcLoadImage *, uint32_t> findGlobal				( char const *name, bcLoadImage *ignoreImage ) const;							// searches all load images

	// print the call stack to the console
	virtual void	 printCallStack			( VAR *stack, fglOp *op );

	template<typename T>
	void getSourcePaths( const uint8_t *buff, T t)
	{
		fgxFileHeader* header = (fgxFileHeader *) buff;
		
		auto paths = (fgxName*)(buff + header->offsetBuildDirectories);

		for (size_t loop = 0; loop < header->numBuildDirectories; loop++)
		{
			t(paths[loop].name);
		}

	}

	virtual	 uint64_t	getTicks	( void )
	{
		return 0;
	}

	fglOp		*catchHandler ( bcFuncDef *funcDef, fglOp *ops, VAR *basePtr, int64_t waSave, VAR const &exception );

	// call byte code function within vm context
	void		 interpretBC		( bcFuncDef *funcDef, fglOp *retAddr, uint32_t nParams );
	VAR			*interpretBCParam	( bcFuncDef *funcDef, bcFuncDef *destFunc, fglOp *ops, uint32_t nParams, VAR *stack, VAR *basePtr );

	// call a native function within vm context
	void		 funcCall			( bcFuncDef *bcFunc, uint32_t nParams );
	void		 funcCall			( uint32_t atomName, uint32_t nParams );

	template <typename Head>
	void _stackPush ( types<Head>, Head &&head)
	{
		*(stack++) = toVar<Head>::convert ( this, head );
	}

	template <typename Head, typename ...Tail>
	void _stackPush ( types<Head, Tail...>, Head  &&head, Tail &&...tail )
	{
		_stackPush ( types<Tail...>{}, std::forward<Tail> ( tail )... );
		*(stack++) = toVar<Head>::convert ( this, head );
	}

	template <typename ...Params >
	VAR objNew ( char const *name, Params  &&... params )
	{
		// push the parameters on the stack in right to left order
		_stackPush ( types<Params...>{}, std::forward<Params> ( params )... );
		// call classNew
		classNew ( this, name, sizeof...(Params) );
		// fix up the statck
		stack -= sizeof... ( Params );
		return result;
	}

	template <>
	VAR objNew ( char const *name )
	{
		// call classNew
		classNew ( this, name, 0 );
		// fix up the statck
		return result;
	}

	template <typename ...Params >
	VAR call ( char const *name, Params  &&... params )
	{
		// push the parameters on the stack in right to left order
		_stackPush ( types<Params...>{}, std::forward<Params> ( params )... );

		auto func = atomTable->atomGetFunc ( name );
		switch ( func->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				interpretBC ( func, nullptr, sizeof... (Params) );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				funcCall ( func, sizeof... (Params) );
				break;
			default:
				throw errorNum::scINTERNAL;
		}
		// fix up the statck
		stack -= sizeof... (Params);
		return result;
	}

	template <>
	VAR call ( char const *name )
	{
		auto func = atomTable->atomGetFunc ( name );
		switch ( func->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				interpretBC ( func, nullptr, 0 );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				funcCall ( func, 0 );
				break;
			default:
				throw errorNum::scINTERNAL;
		}
		// fix up the statck
		return result;
	}
};
