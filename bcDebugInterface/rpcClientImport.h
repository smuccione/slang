#pragma once

#include <stdint.h>

namespace rpcImport
{
#include "bcVM\vmDebugClient.h"

// add each of your functions you want to call from the server to this import list
#define IMPORT_LIST		\
	IMPORT ( debugContinue ) \
	IMPORT ( debugStepOver ) \
	IMPORT ( debugStepInto ) \
	IMPORT ( debugStepOut ) \
	IMPORT ( halt ) \
	IMPORT ( debugRunToLine ) \
	IMPORT ( debugIsProfiling ) \
	IMPORT ( debugSetIP ) \
	IMPORT ( debugGetCallstack ) \
	IMPORT ( debugGetFunctionList ) \
	IMPORT ( debugGetInspectors ) \
	IMPORT ( debugGetExecutionProfile ) \
	IMPORT ( debugSetBreakpoint ) \
	IMPORT ( debugClearBreakpoint ) \
	IMPORT ( debugClearAllBreakpoints ) \
	IMPORT ( debugAdjustBreakpoint )

//	IMPORT ( debugInspect ) \
//	IMPORT ( debugInspect ) \

};

