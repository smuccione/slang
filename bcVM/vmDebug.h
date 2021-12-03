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
#include "bcVM/bcVM.h"
#include "bcVM/vmInstance.h"
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
	vmDebugBreak,
	vmProfile,
};

enum class vmBPType
{
	vmBPType_Trace,
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
		return var;
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
	char						*funcName;
	char						*fileName;
	size_t						 line;
	std::vector<vmDebugVar *>	 params;
	bcFuncDef					*funcDef;

	vmCallstackEntry ( class vmInstance *instance, bcFuncDef *funcDef, size_t line, size_t nParams, VAR *param );

	vmCallstackEntry ( const vmCallstackEntry &old )
	{
		funcId = old.funcId;
		line = old.line;
		funcName = _strdup ( old.funcName );
		fileName = _strdup ( old.fileName );
		params = old.params;
		funcDef = old.funcDef;
	}

	~vmCallstackEntry ( )
	{
		free ( fileName );
		free ( funcName );
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
	char		*className = nullptr;
	char		*funcName = nullptr;
	char		*fileName = nullptr;
	size_t		 lineNum = 0;
	uint32_t	 atomNum = 0;
	bool		 isUsercode = false;

	vmFunctionListEntry ( class vmInstance *instance, uint32_t atomNum );
	vmFunctionListEntry ( const vmFunctionListEntry &old )
	{
		if ( old.className )
		{
			className = _strdup ( old.className );
		} else
		{
			className = 0;
		}
		if ( old.funcName )
		{
			funcName = _strdup ( old.funcName );
		} else
		{
			funcName = 0;
		}
		if ( old.fileName )
		{
			fileName = _strdup ( old.fileName );
		} else
		{
			fileName = 0;
		}
		lineNum = old.lineNum;
		atomNum = old.atomNum;
		isUsercode = old.isUsercode;
	}

	~vmFunctionListEntry ( )
	{
		if ( className ) free ( className );
		if ( funcName ) free ( funcName );
		if ( fileName ) free ( fileName );
	}
};

class vmInspector
{
public:
	char *name;

	typedef vmInspectList *(*vmInspectorCb) (class vmInstance *instance, struct bcFuncDef *funcDef, fglOp *op, VAR *stack, vmDebugVar *var);

	vmInspectorCb	cb;

	vmInspector ( char const *name, vmInspectorCb cb )
	{
		this->name = _strdup ( name );
		this->cb = cb;
	}

	vmInspector ( const vmInspector &old )
	{
		name = _strdup ( old.name );
		cb = old.cb;
	}

	~vmInspector ( )
	{
		free ( name );
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
	std::vector<char *>			entry;
};

struct vmBreakpoint
{
	stringi		 fName;
	uint32_t	 lineNum = 0;
	fglOp		*op = nullptr;

	vmBreakpoint ( char const *fName, size_t lineNum, fglOp *op ) : fName ( fName ), lineNum ( (uint32_t) lineNum ), op ( op )
	{
	}
	vmBreakpoint ( vmBreakpoint const &old )
	{
		fName = old.fName;
		lineNum = old.lineNum;
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
	struct	dbTmpBP *next;
	vmBPType			 type;
	fglOp				 op;		// original op;
	fglOp *address;
};


class vmDebug
{
public:
	bool						 enabled;

	bool						 isProfiling;
	size_t						 nCalls;

	int64_t						 time;
	volatile bool				 halt;					/* halt execution */
	volatile bool				 halted;				/* flag indictationg the instance has halted and awaiting debugger action	*/
	volatile bool				 terminate;
	bool						 isResuming;			/* resuming from a breakpoint... stepInto until we're no longer trapping on a breakpoint and then auto-continue */
	bool						 isStepping;			/* has a step operation been requested */
	bool						 stepInto;				/* has a step-into operation been requested */
	struct	dbTmpBP				*tmpBP;					/* temporary break point list */

	struct	fglOp				*IP;					/* current IP */
	struct	bcFuncDef			*funcDef;				/* current function */
	uint32_t					 stackHeight;			/* if the IP changes we may or may not have new or removed block local declarations */
	VAR							*stack;					/* current stack pointer */

	dbTmpBP						*bpFree;

	std::vector<vmInspector>	 inspectors;			/* registered inspectors */
	std::set<vmBreakpoint, vmBreakpoint::less>		 breakPoints;

	HANDLE						 continueSemaphore = 0;

public:

	vmDebug ( );
	virtual ~vmDebug ( );
	void				 reset ( );
	void				 debugEnable ( )
	{
		enabled = true;
	};
	void				 debugDisable ( )
	{
		enabled = false;
	};
	void				 debugRegisterInspector ( char const *name, vmInspector::vmInspectorCb cb );
	void				 combine ( class vmDebug *old )
	{
		inspectors.clear ( );

		for ( auto &it : old->inspectors )
		{
			inspectors.push_back ( vmInspector ( it ) );
		}
	}
private:
	void					 debugRemoveTracePoint ( struct fglOp *op );
	void					 debugRemoveTracePoint ( void );
	vmBPType				 getTracePointType ( fglOp *op );
	void					 debugMakeBreakpoints ( class vmInstance *instance, fglOp *op );

public:
	void					 debugSetTracePoint ( vmBPType type, fglOp *op );
	uint32_t				 debugGetCurrentLine ( struct bcFuncDef *funcDef, struct fglOp *op );
	uint32_t				 debugGetCurrentSourceIndex ( struct bcFuncDef *funcDef, struct fglOp *op );

	void					 debugStepOp ( vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack, bool stepInto, vmBPType type );
	void					 debugStepLine ( class vmInstance *instance, struct bcFuncDef *funcDef, struct fglOp *op, struct VAR *stack );
	void					 debugRunToLine ( class vmInstance *instance, char const *fileName, size_t line );
	bool					 debugSetIP ( class vmInstance *instance, struct bcFuncDef *funcDef, char const *fileName, size_t line );
	std::vector<size_t>		 debugGetGotoTargets ( vmInstance *instance, bcFuncDef *funcDef );
	void					 debugStepOut ( class vmInstance *instance, struct bcFuncDef *funcDef, struct fglOp *op, struct VAR *stack );
	void					 debugSetProfiling ( class vmInstance *instance );
	vmCallStack				*debugGetCallstack ( class vmInstance *instance );
	vmFunctionList			*debugGetFunctionList ( class vmInstance *instance );
	vmInspectorList			*debugGetInspectors ( class vmInstance *instance );
	vmInspectList			*debugInspect ( class vmInstance *instance, size_t num, size_t param );
	vmInspectList			*debugInspect ( class vmInstance *instance, size_t stackFrame, vmDebugVar *var, uint32_t start, uint32_t end );
	vmProfileData			*debugGetExecutionProfile ( class vmInstance *instance );
	bcFuncDef				*debugFunc ( vmInstance *instance, size_t stackFrame );
	VAR						*debugBasePtr ( vmInstance *instance, size_t stackFrame );
	size_t					 debugSetBreakpoint ( class vmInstance *instance, char const *fileName, size_t lineNum );
	void					 debugClearBreakpoint ( class vmInstance *instance, char const *fileName, size_t lineNum );
	void					 debugClearAllBreakpoints ( class vmInstance *instance, char const *fileName, size_t lineNum );
	size_t					 debugAdjustBreakpoint ( class vmInstance *instance, stringi const &fileName, size_t line );

	friend fglOp			*debugSignal ( class vmInstance *instance, vmStateType state, fglOp *op, bcFuncDef *funcDef, VAR *stackPtr );

	virtual void			 debugInterrupt ( vmDebugMessage *msg ) = 0;
	virtual void			 debugContinue ( vmDebugMessage *msg) = 0;
};

/* initialization */
extern	void				 debugRegister ( class vmInstance *instance, vmDebug *debug );

/* internal use from the VM to generate debug events */
extern	fglOp *debugSignal ( class vmInstance *instance, vmStateType state, fglOp *op, bcFuncDef *funcDef, VAR *stackPtr );

/* external interfaces */
extern	void					 debugContinue ( struct vmDebugMessage *msg );
extern	void					 debugTerminate ( vmInstance *instance );
extern	void					 debugStepOver ( class vmInstance *instance );
extern	void					 debugStepInto ( class vmInstance *instance );
extern	void					 debugStepOut ( class vmInstance *instance );
extern	void					 debugHalt ( class vmInstance *instance );
extern	void					 debugRunToLine ( class vmInstance *instance, char const *fileName, size_t line );
extern	bool					 debugIsProfiling ( class vmInstance *instance );
extern	bool					 debugSetIP ( class vmInstance *instance, char const *fileName, size_t line );
extern	class vmCallStack		*debugGetCallstack ( class vmInstance *instance );
extern	class vmFunctionList	*debugGetFunctionList ( class vmInstance *instance );
extern	class vmInspectorList	*debugGetInspectors ( class vmInstance *instance );
extern	class vmInspectList		*debugInspect ( class vmInstance *instance, size_t num, size_t stackFrame );
extern	class vmInspectList		*debugInspect ( class vmInstance *instance, size_t stackFrame, class vmDebugVar *varId, uint32_t start, uint32_t end );
extern	class vmProfileData		*debugGetExecutionProfile ( class vmInstance *instance );
extern	size_t					 debugSetBreakpoint ( class vmInstance *instance, char const *fileName, size_t lineNum );
extern	void					 debugClearBreakpoint ( class vmInstance *instance, char const *fileName, size_t lineNum );
extern	void					 debugClearAllBreakpoints ( class vmInstance *instance, char const *fileName, size_t lineNum );
extern	size_t					 debugAdjustBreakpoint ( class vmInstance *instance, char const *fileName, size_t lineNum );
