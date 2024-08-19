/*
	Remote Debugging interface

*/

/*
	SLANG JIT Debugger header file

*/

#pragma once

#include <stdint.h>
#include <vector>
#include <list>
#include <set>
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmInstance.h"
#include "bcVM/bcVM.h"
#include "bcVM/vmAtom.h"
#include "Target/common.h"
#include "bcVM/Opcodes.h"
#include "Utility/stringi.h"

enum class vmStateType
{
	vmLoad = 1,
	vmThreadStart,
	vmThreadEnd,
	vmTrap,
	vmTrace,
	vmHalt,
	vmHaltTrapped,
	vmDebugBreak,
	vmProfile,
};

enum class vmBPType
{
	vmBPType_Trace,
	vmBPType_TraceContinue,
	vmBPType_Breakpoint,
	vmBPType_BreakStep,
};

struct vmDebugMessage
{
	vmStateType				 state;
	class vmInstance		*instance;
	HANDLE					 continueEvent;

	struct
	{
		char				 fName[MAX_NAME_SZ];
		char				 functionName[MAX_NAME_SZ];
		size_t				 line;
		size_t				 funcId;
		size_t				 instructionAddr;
		size_t				 stackAddr;
		errorNum			 err;
	} location;
};

class vmDebugVar
{
public:
	virtual	const char			*getName ( void ) = 0;
	virtual	size_t				 getNameLen ( void ) = 0;
	virtual	stringi				 getType ( void ) = 0;
	virtual bool				 isStringValue ( void ) = 0;
	virtual const char			*getValue ( void ) = 0;
	virtual size_t				 getValueLen ( void ) = 0;
	virtual	class vmInspectList *getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end ) = 0;
	virtual	bool				 hasChildren ( void ) = 0;
	virtual VAR					*getRawVar ( void )
	{
		return 0;
	}
	virtual void				 rebuildData ( class vmInstance *instance )
	{};
	virtual ~vmDebugVar ( )
	{}
};

class vmDebugLocalVar : public vmDebugVar
{
private:
	char *name;
	enum class debugValueType
	{
		vmVarStaticPtr,
		vmVarDynamicPtr,
		vmVarData,
		vmVarVar,
	}				 valueType;

	VAR				*var;
	size_t			 cLen;
	uint8_t			*cPtr;


	size_t			 inspector;
	bool			 isString;
	bool			 hasChild;

	char			 tmpName[16];
	char			 data[256];

	void buildData ( class vmInstance *instance, VAR *var );

public:
	vmDebugLocalVar ( class vmInstance *instance, char const *name, VAR *var );
	vmDebugLocalVar ( class vmInstance *instance, int64_t num, VAR *var );
	vmDebugLocalVar ( class vmInstance *instance, double num, VAR *var );
	vmDebugLocalVar ( class vmInstance *instance, char const *name, uint8_t *data, size_t	dataLen );
	vmDebugLocalVar ( class vmInstance *instance, char const *name, const uint8_t *data, size_t dataLen );
	vmDebugLocalVar ( const vmDebugLocalVar &old );

	void rebuildData ( class vmInstance *instance ) override
	{
		buildData ( instance, var );
	}

	virtual ~vmDebugLocalVar ( )
	{
		switch ( valueType )
		{
			case debugValueType::vmVarStaticPtr:
				break;
			case debugValueType::vmVarDynamicPtr:
				free ( cPtr );
				break;
			case debugValueType::vmVarData:
				break;
			case debugValueType::vmVarVar:
				break;
		}
		if ( name != tmpName ) free ( name );
	}

	const char			*getName ( void ) override;
	size_t				 getNameLen ( void ) override;
	const char			*getValue ( void ) override;
	size_t				 getValueLen ( void ) override;

	// ONLY valid AFTER calling getValue
	stringi				 getType ( void ) override;

	bool				 isStringValue ( void ) override;
	class vmInspectList *getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end ) override;
	bool				 hasChildren ( void ) override;
	VAR *getRawVar ( void ) override
	{
		if ( valueType == debugValueType::vmVarVar || valueType == debugValueType::vmVarData )
		{
			return var;
		} else
		{
			return nullptr;
		}
	};
};

class vmInspectList
{
public:
	std::vector<vmDebugVar *>	 entry;

	~vmInspectList ( )
	{
		for ( auto it : entry )
		{
			delete it;
		}
	}
};

class vmCallstackEntry
{
public:
	size_t						 funcId;
	stringi						 funcName;
	stringi						 fileName;
	size_t						 line;
	std::vector<vmDebugVar *>	 params;
	bcFuncDef					*funcDef;

	vmCallstackEntry ( class vmInstance *instance, bcFuncDef *funcDef, size_t line, size_t nParams, VAR *param );

	vmCallstackEntry ( const vmCallstackEntry &old )
	{
		funcId = old.funcId;
		line = old.line;
		funcName = old.funcName;
		fileName = old.fileName;
		params = old.params;
		funcDef = old.funcDef;
	}

	~vmCallstackEntry ( )
	{
	}
};

class vmCallStack
{
public:
	std::vector<vmCallstackEntry>	entry;
};

class vmFunctionListEntry
{
public:
	stringi		className;
	stringi		funcName;
	stringi		fileName;
	size_t		lineNum = 0;
	uint32_t	atomNum = 0;
	bool		isUsercode = false;

	vmFunctionListEntry ( class vmInstance *instance, uint32_t atomNum );
	vmFunctionListEntry ( const vmFunctionListEntry &old )
	{
		className = old.className;
		funcName = old.funcName;
		fileName = old.fileName;
		lineNum = old.lineNum;
		atomNum = old.atomNum;
		isUsercode = old.isUsercode;
	}

	~vmFunctionListEntry ( )
	{
	}
};

class vmInspector
{
public:
	stringi name;

	typedef vmInspectList *(*vmInspectorCb) (class vmInstance *instance, struct bcFuncDef *funcDef, fglOp *op, VAR *stack, vmDebugVar *var);

	vmInspectorCb	cb;

	vmInspector ( char const *name, vmInspectorCb cb )
	{
		this->name = name;
		this->cb = cb;
	}

	vmInspector ( const vmInspector &old )
	{
		name = old.name;
		cb = old.cb;
	}

	vmInspector( vmInspector &&old ) noexcept
	{
		std::swap( name, old.name );
		std::swap( cb, old.cb );
	}
	~vmInspector ( )
	{
	}

	friend class vmDebug;
};

class vmFunctionList
{
public:
	std::vector<vmFunctionListEntry>	entry;

	vmFunctionList ( class vmInstance *instance );
};

class vmInspectorList
{
public:
	std::vector<stringi>			entry;
};

struct vmBreakpoint
{
	stringi		 fName;
	uint32_t	 lineNum = 0;
	fglOp		*op = nullptr;
	stringi		 expression;
	stringi		 count;
	stringi		 logMessage;
	int64_t		 currentCount = 0;  

	vmBreakpoint( stringi const &fName, size_t lineNum, fglOp *op, stringi const &expr, stringi const &count, stringi const &logMessage ) : fName( fName ), lineNum( (uint32_t) lineNum ), op( op ), expression( expr ), count( count ), logMessage ( logMessage )
	{}
	vmBreakpoint ( vmBreakpoint const &old )
	{
		fName = old.fName;
		lineNum = old.lineNum;
		expression = old.expression;
		count = old.count;
		logMessage = old.logMessage;
		currentCount = old.currentCount;
		op = old.op;
	}
	vmBreakpoint ( vmBreakpoint &&old ) noexcept
	{
		*this = old;
	}

	vmBreakpoint &operator = ( vmBreakpoint const &old )
	{
		return *this = vmBreakpoint ( old );
	}

	vmBreakpoint &operator = ( vmBreakpoint &&old ) noexcept
	{
		std::swap ( fName, old.fName );
		std::swap ( lineNum, old.lineNum );
		std::swap ( op, old.op );
		std::swap( expression, old.expression );
		std::swap( count, old.count );
		std::swap( logMessage, old.logMessage );
		std::swap( currentCount, old.currentCount );
		return *this;
	}

	virtual ~vmBreakpoint ( )
	{
	}

	struct less
	{
		bool operator() ( vmBreakpoint const &left, vmBreakpoint const &right ) const
		{
			if ( strccmp ( left.fName.c_str(), right.fName.c_str() ) < 0 ) return true;
			if ( left.lineNum < right.lineNum ) return true;
			return false;
		}
	};
};

struct vmFunctionProfile
{
	char const	*funcName;
	char const	*fileName;
	uint32_t	 lineNum;
	uint64_t	 numCalls;
	uint64_t	 numMicroSecsTotal;
	uint64_t	 numMicroSecsLocal;
	float		 percentLocal;
	float		 percentTotal;

	vmFunctionProfile ( class vmInstance *instance, uint32_t atomNum );
};

class vmProfileData
{
	std::vector<vmFunctionProfile>	func;

public:
	uint64_t	 numMicroSecsTotal;

	std::vector<vmFunctionProfile>::size_type size ( void )
	{
		return func.size ( );
	}

	vmFunctionProfile *operator [] ( size_t index )
	{
		return &func[index];
	}

	friend class vmDebug;
};

struct dbTmpBP
{
	struct	dbTmpBP		*next;
	vmBPType			 type;
	fglOp				 op;		// original op;
	fglOp				*address = 0;
	int64_t				 count = 0;
	vmBreakpoint const	*vmBp;
};

class vmDebug
{
public:
	struct exceptionInfo
	{
		stringi								 exceptionId;
		stringi								 description;
		VAR									*exceptionObject = nullptr;
		stringi								 exceptionObjectType;
		errorLocation::exceptionBreakType	 breakMode;
	};

private:
#define REGISTER_ERROR( errNum, errTxt )	errorLocation::exceptionBreakType::unhandled,
	std::vector<errorLocation::exceptionBreakType>	exceptionFilters = {
		ERROR_TABLE
	};
#undef REGISTER_ERROR

	errorLocation::exceptionBreakType		 globalBreakMode;

public:
	bool						 enabled = true;
	bool						 isProfiling = false;
	size_t						 nCalls = 0;

	int64_t						 time = 0;
	std::atomic<bool>			 halt = false;			/* halt execution */
	std::atomic<bool>			 halted = false;		/* flag indictationg the instance has halted and awaiting debugger action	*/
	std::atomic<bool>			 terminate = false;
	bool						 isResuming = false;	/* resuming from a breakpoint... stepInto until we're no longer trapping on a breakpoint and then auto-continue */
	bool						 isStepping = false;	/* has a step operation been requested */
	bool						 stepInto = false;		/* has a step-into operation been requested */
	struct	dbTmpBP				*tmpBP = nullptr;		/* temporary break point list */

	struct	fglOp				*IP;					/* current IP */
	struct	bcFuncDef			*funcDef;				/* current function */
	uint32_t					 stackHeight;			/* if the IP changes we may or may not have new or removed block local declarations */
	VAR							*stack;					/* current stack pointer */

	dbTmpBP						*bpFree = nullptr;

	std::vector<vmInspector>	 inspectors;			/* registered inspectors */
	std::set<vmBreakpoint, vmBreakpoint::less>		 breakPoints;

	HANDLE						 continueSemaphore = 0;

public:

	vmDebug ( );
	virtual ~vmDebug ( );
	void				 reset ( );
	void				 Enable ( )
	{
		enabled = true;
	};
	void				 Disable ( )
	{
		enabled = false;
	};
	void				 RegisterInspector ( char const *name, vmInspector::vmInspectorCb cb );
	void				 combine ( class vmDebug *old )
	{
		inspectors.clear ( );

		for ( auto &it : old->inspectors )
		{
			inspectors.push_back ( vmInspector ( it ) );
		}
	}
private:
	void					 RemoveTracePoint ( struct fglOp *op, vmBreakpoint const *bp );
	void					 RemoveTracePoint ( void );
	vmBPType				 getTracePointType ( fglOp *op );
	void					 MakeBreakpoints ( class vmInstance *instance, fglOp *op );

public:
	errorLocation::exceptionBreakType	 GetExceptionBreakType ( errorNum errNum );
	errorLocation::exceptionBreakType	 GetExceptionGlobalBreakType ();
	void								 SetExceptionBreakType ( errorNum errNum, errorLocation::exceptionBreakType breakType );
	void								 SetExceptionGlobalBreakType ( errorLocation::exceptionBreakType breakType );

	void					 SetTracePoint ( vmBPType type, fglOp *op, vmBreakpoint const *bp );
	uint32_t				 GetCurrentLine ( struct bcFuncDef *funcDef, struct fglOp *op );
	uint32_t				 GetCurrentSourceIndex ( struct bcFuncDef *funcDef, struct fglOp *op );

	void					 StepOp ( vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack, bool stepInto, vmBPType type );
	void					 StepLine ( class vmInstance *instance, struct bcFuncDef *funcDef, struct fglOp *op, struct VAR *stack );
	void					 RunToLine ( class vmInstance *instance, char const *fileName, size_t line );
	bool					 SetIP ( class vmInstance *instance, struct bcFuncDef *funcDef, char const *fileName, size_t line );
	std::vector<size_t>		 GetGotoTargets ( vmInstance *instance, bcFuncDef *funcDef );
	void					 StepOut ( class vmInstance *instance, struct bcFuncDef *funcDef, struct fglOp *op, struct VAR *stack );
	void					 SetProfiling ( class vmInstance *instance );
	vmCallStack				*GetCallstack ( class vmInstance *instance );
	vmFunctionList			*GetFunctionList ( class vmInstance *instance );
	vmInspectorList			*GetInspectors ( class vmInstance *instance );
	vmInspectList			*Inspect ( class vmInstance *instance, size_t num, size_t param );
	vmInspectList			*Inspect ( class vmInstance *instance, size_t stackFrame, vmDebugVar *var, uint32_t start, uint32_t end );
	vmProfileData			*GetExecutionProfile ( class vmInstance *instance );
	bcFuncDef				*Func ( vmInstance *instance, size_t stackFrame );
	VAR						*BasePtr ( vmInstance *instance, size_t stackFrame );
	size_t					 SetBreakpoint ( class vmInstance *instance, stringi const &fileName, size_t lineNum, stringi const &expression, stringi const &count, stringi const &logMessage );
	void					 ClearBreakpoint ( class vmInstance *instance, stringi const &fileName, size_t lineNum );
	void					 ClearAllBreakpoints ( class vmInstance *instance, stringi const &fileName );
	size_t					 AdjustBreakpoint ( class vmInstance *instance, stringi const &fileName, size_t line );
	exceptionInfo			 GetExceptionInfo ( class vmInstance *instance );

	friend fglOp			*debugSignal ( class vmInstance *instance, vmStateType state, fglOp *op, bcFuncDef *funcDef, VAR *stackPtr );

	virtual void			 Interrupt ( vmDebugMessage *msg ) = 0;
	virtual void			 Continue ( vmDebugMessage *msg) = 0;
};

class nullDebugger : public vmDebug
{
public:
	void Continue ( vmDebugMessage *instance ) override
	{
	}

	void Interrupt ( vmDebugMessage *msg ) override
	{
	}
};

/* initialization */
extern	void				 debugRegister ( class vmInstance *instance, std::shared_ptr<vmDebug> &debug );

/* internal use from the VM to generate debug events */
extern	fglOp *debugSignal ( class vmInstance *instance, vmStateType state, fglOp *op, bcFuncDef *funcDef, VAR *stackPtr );

/* external interfaces */
extern	errorLocation::exceptionBreakType	 debugGetExceptionBreakType ( vmInstance *instance, errorNum errNum );
extern	errorLocation::exceptionBreakType	 debugGetExceptionGlobalBreakType ( vmInstance *instance );
extern	void								 debugSetExceptionBreakType ( vmInstance *instance, errorNum errNum, errorLocation::exceptionBreakType breakType );
extern	void								 debugSetExceptionGlobalBreakType ( vmInstance *instance, errorLocation::exceptionBreakType breakType );

extern	void						 debugContinue ( struct vmDebugMessage *msg );
extern	void						 debugTerminate ( vmInstance *instance );
extern	void						 debugStepOver ( class vmInstance *instance );
extern	void						 debugStepInto ( class vmInstance *instance );
extern	void						 debugStepOut ( class vmInstance *instance );
extern	void						 debugHalt ( class vmInstance *instance );
extern	void						 debugRunToLine ( class vmInstance *instance, char const *fileName, size_t line );
extern	bool						 debugIsProfiling ( class vmInstance *instance );
extern	bool						 debugSetIP ( class vmInstance *instance, char const *fileName, size_t line );
extern	class vmCallStack			*debugGetCallstack ( class vmInstance *instance );
extern	class vmFunctionList		*debugGetFunctionList ( class vmInstance *instance );
extern	class vmInspectorList		*debugGetInspectors ( class vmInstance *instance );
extern	class vmInspectList			*debugInspect ( class vmInstance *instance, size_t num, size_t stackFrame );
extern	class vmInspectList			*debugInspect ( class vmInstance *instance, size_t stackFrame, class vmDebugVar *varId, uint32_t start, uint32_t end );
extern	class vmProfileData			*debugGetExecutionProfile ( class vmInstance *instance );
extern	size_t						 debugSetBreakpoint ( class vmInstance *instance, stringi const &fileName, size_t lineNum, stringi const &condition, stringi const &count, stringi const &logMessage );
extern	void						 debugClearBreakpoint ( class vmInstance *instance, stringi const &fileName, size_t lineNum );
extern	void						 debugClearAllBreakpoints ( class vmInstance *instance, stringi const &fileName );
extern	size_t						 debugAdjustBreakpoint ( class vmInstance *instance, stringi const &fileName, size_t lineNum );
extern  vmDebug::exceptionInfo		 debugGetExceptionInfo ( class vmInstance *instance );
