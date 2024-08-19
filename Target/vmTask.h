/*
	Header file for muili-tasking support for FGL

*/

#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <stdint.h>
#include <memory>
#include <Windows.h>
#include "bcVM/vmAtom.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmInstance.h"
#include "bcVM/exeStore.h"

class vmFiber : public vmCoroutine {
	class vmInstance	*instance;
public:
	VAR					*evalStack;
	size_t				 nStack;
	VAR					*stack;

	VAR					*func;
	bcFuncDef			*funcDef;
	bcClassSearchEntry  *cEntry = nullptr;	// for lambda calls so we can adjust object
	vmFiber				*creatingFiber;
	uint32_t			 nParams;			// per activation - sets the number of parameters passed on the stack;

	void				*fiber;
	bool				 canContinue;

	vmFiber ( class vmInstance *instance );
	vmFiber ( class vmInstance *instance, VAR *func, size_t nEvalStack, size_t nCStack );
	~vmFiber();

	void				 yield ( VAR *param, uint32_t nParams ) override;
	vmCoroutineStatus	 getStatus	( void )  override
	{ 
		return status; 
	};
	VAR					*getStack	( VAR **topOfStack ) override;

	friend class vmStackInstance;
	friend void CALLBACK fiberInit ( void *param );
};

class vmTaskInstance : public vmInstance {
public:
	enum cargoType
	{
		findFileCargo,
		userCargo
	};

	HANDLE				 threadId;
	char				 id[MAX_NAME_SZ];
	vmCoroutine			 mainThread;

	std::vector<void *>	 cargo;

	HANDLE				 heap;

	/* .NET stuff */
	void				*domain;							/* list of the .net domains attached to this instance */

	vmTaskInstance( char const *name );
	~vmTaskInstance();

	void		 allocateEvalStack	( size_t nEntries ) override;
	void		 freeEvalStack		( void ) override;
	void		 setName			( char const *name ) override;
	char const	*getName			( void )  override
	{ 
		return id; 
	}

	template <typename T> T getCargo ( size_t which = 0 )
	{
		if ( cargo.size ( ) > which )
		{
			return static_cast<T>(cargo[which]);
		}
		return 0;
	}
	void		setCargo			( void *cargo, size_t which=0 ) 
	{
		if ( this->cargo.size ( ) < which + 1 )
		{
			this->cargo.resize ( which + 1 );
		}
		this->cargo[which] = cargo;
	};

	void		 duplicate			( class vmInstance *instance ) override;
	void		*malloc				( size_t size ) override;
	void		 free				( void *free ) override;
	void		 validateHeap		( void ) override;

	uint64_t	 getTicks			( void ) override;

	std::vector<std::tuple<size_t, std::shared_ptr<uint8_t const>>>	preLoadedSource;			// global builtin function support and preloaded libraries to be loaded by the interpreter (source code)
	std::vector<std::tuple<size_t, std::shared_ptr<uint8_t const>>>	preLoadedCode;				// global builtin function support and preloaded libraries to be loaded by the interpreter (compiled code)

	vmCoroutine		*createCoroutine	( VAR *func, size_t evalStackSize = 8192, size_t cStackSize = 128 * 256 ) override;

	void		 reset				( void ) override;
};

class taskControl {
public:
	virtual ~taskControl() {};
	virtual void end ( void ) = 0;
};

