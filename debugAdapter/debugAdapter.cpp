// debugAdapter.cpp : Defines the functions for the static library.
//

#include "Utility/settings.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include <fcntl.h>      /* Needed only for _O_RDWR definition */
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <share.h>

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"

#include "Utility\jsonParser.h"
#include "rpcServer.h"
#include "Utility\socketServer.h"
#include "Target/vmTask.h"
#include "debugAdapter.h"
#include "debugAdapterInternal.h"
#include "bcInterpreter/op_variant.h"

struct variableReference
{
	bool		isInspector = false;
	size_t		inspector = 0;
	size_t		stackFrame = 0;
	vmDebugVar *debugVar = nullptr;
};

static const char *const DebuggerClassName = "SlangDebugClass";

static std::mutex						 debuggerLock;
static HWND								 debuggerWindowHandle;
static std::atomic<bool>				 debuggerRunning = false;

static  stringi							 targetDebugFile;
static  bool							 breakOnEntry;
static	winDebugger						*activeDebugger = nullptr;
static	jsonRPCServerBase				*activeDebugServer = nullptr;
static	vmInstance						*lastInstance = nullptr;
static	debuggerFileStore				 fileStore;
int										 writePipeDesc;
int										 readPipeDesc;
BUFFER									 outputBuffer;

// this data must be cleared at each stop
static  std::vector<variableReference>	 varReferences;
static  vmInspectorList					*inspectors = nullptr;
std::vector<vmInspectList *>			 inspectorList;

#define NOTES \
	DEF ( initialized ) \
	DEF ( breakpoint ) \
	DEF ( continued ) \
	DEF ( exited ) \
	DEF ( invalidated ) \
	DEF ( loadedSource ) \
	DEF ( output ) \
	DEF ( process ) \
	DEF ( stopped ) \
	DEF ( terminated ) \
	DEF ( thread )

enum class notifications : size_t
{
#define DEF(a) a,
	NOTES
#undef DEF
};

static stringi fileNameToUri ( stringi const &uri )
{
	stringi result = "file:///";
	result += uri;
	return result;
}

static stringi uriToFileName ( stringi const &uri )
{
	stringi result;
	char const *uriPtr = uri.c_str ();
	if ( !memcmpi ( uriPtr, "file:///", 8 ) )
	{
		uriPtr += 8;
	}
	while ( *uriPtr )
	{
		if ( *uriPtr == '\\' )
		{
			result += '/';
			uriPtr++;
		} else
		{
			result += *(uriPtr++);
		}
	}
	return result;
}

static LRESULT CALLBACK debuggerWindowProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	vmDebugMessage *msg;

	switch ( uMsg )
	{
		case WM_SLANG_DEBUG:
			msg = (vmDebugMessage *)lParam;

			if ( activeDebugServer && (!lastInstance || (msg->instance == lastInstance)) )
			{
				if ( targetDebugFile.size () )
				{
					if ( targetDebugFile != msg->location.fName && targetDebugFile != "<any>")
					{
						debugContinue ( msg );
						break;
					}
				}
				lastInstance = msg->instance;
				lastInstance->outputDesc = writePipeDesc;
				activeDebugger = static_cast<winDebugger *>(lastInstance->debug);

				activeDebugger->lastMsg = *msg;

//				printf ( "msg state %i\r\n", msg->state );
				switch ( msg->state )
				{
					case vmStateType::vmHalt:
					case vmStateType::vmTrap:
					case vmStateType::vmTrace:
					case vmStateType::vmDebugBreak:
						if ( !inspectors )
						{
							inspectors = debugGetInspectors ( lastInstance );
						}
						for ( auto it : inspectorList )
						{
							delete it;
						}
						inspectorList.clear ();
						varReferences.clear ();

						activeDebugServer->sendNotification ( (size_t)notifications::stopped );
						break;
					case vmStateType::vmThreadEnd:
						lastInstance->outputDesc = 0;
						lastInstance = 0;
						activeDebugServer->sendNotification ( (size_t)notifications::exited );
						debugContinue ( msg );
						break;
					case vmStateType::vmLoad:
						activeDebugServer->sendNotification ( (size_t)notifications::initialized );
						if ( breakOnEntry )
						{
							debugHalt ( lastInstance );
						}
						debugContinue ( msg );
						break;
					case vmStateType::vmThreadStart:
						activeDebugServer->sendNotification ( (size_t)notifications::process );
						if ( breakOnEntry )
						{
							debugHalt ( lastInstance );
						}
						debugContinue ( msg );
						break;
					default:
						debugContinue ( msg );
						break;
				}
			} else
			{
				debugContinue ( msg );
			}
			break;
	}
	return (DefWindowProc ( hWnd, uMsg, wParam, lParam ));

	return (1);
}

static UINT debugServerOutputThread ()
{
	char buffer[4096];
	size_t nRead;
	while ( 1 )
	{
		nRead = _read ( readPipeDesc, buffer, sizeof ( buffer ) );
		if ( nRead )
		{
			{
				std::lock_guard g1 ( debuggerLock );
				outputBuffer.write ( buffer, nRead );
			}
			activeDebugServer->sendNotification ( (size_t)notifications::output );
		}
	}
}
static UINT debugServerThread ()
{
	MSG					msg;
	HWND				hWnd;
	WNDCLASSEX			debugWindowClass = {};

	debugWindowClass.cbSize = sizeof ( debugWindowClass );
	debugWindowClass.style = 0;
	debugWindowClass.lpfnWndProc = debuggerWindowProc;
	debugWindowClass.cbClsExtra = 0;					/* extra bytes */
	debugWindowClass.cbWndExtra = 0;					/* window extra */
	debugWindowClass.hInstance = (HINSTANCE)GetWindowLongPtr ( GetConsoleWindow (), GWLP_HINSTANCE );
	debugWindowClass.hIcon = 0;
	debugWindowClass.hCursor = 0;
	debugWindowClass.hbrBackground = 0;
	debugWindowClass.lpszMenuName = 0;
	debugWindowClass.lpszClassName = DebuggerClassName;
	debugWindowClass.hIconSm = 0;

	RegisterClassEx ( &debugWindowClass );

	hWnd = CreateWindow ( DebuggerClassName,
						  "Slang Debugger",
						  0,
						  CW_USEDEFAULT,
						  CW_USEDEFAULT,
						  CW_USEDEFAULT,
						  CW_USEDEFAULT,
						  NULL,
						  NULL,
						  (HINSTANCE)GetWindowLongPtr ( GetConsoleWindow (), GWLP_HINSTANCE ),
						  0
	);

	debuggerWindowHandle = hWnd;

	debuggerRunning = true;

	while ( GetMessage ( &msg, (HWND)0, 0, 0 ) )
	{
		if ( msg.message != WM_QUIT )
		{
			TranslateMessage ( &msg );
			DispatchMessage ( &msg );
		} else
		{
			break;
		}
	}

	debuggerRunning = false;
	return 0;
}

void debugServerInit ( class vmInstance *instance )
{
	std::lock_guard g1 ( debuggerLock );

	assert ( debuggerWindowHandle );

	debugRegister ( instance, static_cast<vmDebug *>(new winDebugger ( debuggerWindowHandle )) );
}

static jsonElement initialize ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	jsonElement capabilities;

	printf ( "Debug client %s connected\r\n", req["clientName"]->operator stringi().c_str () );

	activeDebugServer = &server;

	capabilities["supportsConfigurationDoneRequest"] = false;
	capabilities["supportsFunctionBreakpoints"] = true;
	capabilities["supportsConditionalBreakpoints"] = true;
	capabilities["supportsHitConditionalBreakpoints"] = true;
	capabilities["supportsEvaluateForHovers"] = true;
	capabilities["supportsStepBack"] = false;
	capabilities["supportsSetVariable"] = true;
	capabilities["supportsRestartFrame"] = false;
	capabilities["supportsGotoTargetsRequest"] = true;
	capabilities["supportsStepInTargetsRequest"] = true;
	capabilities["supportsCompletionsRequest"] = false;
	capabilities["supportsModulesRequest"] = false;
	capabilities["supportsExceptionOptions"] = false;
	capabilities["supportsValueFormattingOptions"] = true;
	capabilities["supportsExceptionInfoRequest"] = true;
	capabilities["supportTerminateDebuggee"] = true;
	capabilities["supportSuspendDebuggee"] = false;
	capabilities["supportsDelayedStackTraceLoading"] = true;
	capabilities["supportsLoadedSourcesRequest"] = false;
	capabilities["supportsLogPoints"] = true;
	capabilities["supportsTerminateThreadsRequest"] = true;
	capabilities["supportsSetExpression"] = true;
	capabilities["supportsTerminateRequest"] = true;
	capabilities["supportsDataBreakpoints"] = false;
	capabilities["supportsReadMemoryRequest"] = false;
	capabilities["supportsDisassembleRequest"] = false;
	capabilities["supportsCancelRequest"] = true;
	capabilities["supportsBreakpointLocationsRequest"] = false;
	capabilities["supportsClipboardContext"] = false;
	capabilities["supportsSteppingGranularity"] = false;
	capabilities["supportsInstructionBreakpoints"] = false;
	capabilities["supportsExceptionFilterOptions"] = false;
//	capabilities["exceptionBreakpointFilters"] = jsonElement ().makeArray ();

//	server.scheduleNotification ( notifications::initialized );
	return capabilities;
}

static jsonElement launch ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	stringi const origName = *req["program"];
	bool  stopOnEntry = *req["stopOnEntry"];

	if ( origName.size() )
	{
		stringi const fixedName = std::filesystem::path ( origName.c_str () ).generic_string ();
		targetDebugFile = fixedName;
	} else
	{
		targetDebugFile.clear ();
	}
	breakOnEntry = stopOnEntry;
	return jsonElement ();
}

static jsonElement attach ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	targetDebugFile.clear ();
	breakOnEntry = false;
	return jsonElement ();
}

static jsonElement loadedSources ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	return jsonElement ();
}

static jsonElement disconnect ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	server.terminate();
	return jsonElement ();
}

static jsonElement setFunctionBreakpoints ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	auto bp = req["breakpoints"];

	for ( auto it = bp->cbeginArray (); it != bp->cendArray (); it++ )
	{
		int64_t const id = *((*it)["id"]);
		int64_t const funcName = *((*it)["name"]);
		int64_t count = 0;
		stringi expr;

		if ( (*it)["hitCondition"] )
		{
			count = _atoi64 ( ((*it)["hitCondition"])->operator stringi().c_str () );
		}
		if ( (*it)["condition"] )
		{
			count = _atoi64 ( ((*it)["condition"])->operator stringi().c_str () );
		}

		fileStore.functionBreakpoints[id] = dbAdapaterBP ( funcName, count, expr );
	}

	jsonElement rsp;
	size_t index = 0;
	if ( activeDebugger )
	{
		for ( auto &it : fileStore.functionBreakpoints )
		{
			auto &funcName = std::get<stringi> ( it.second.location );
			auto atm = lastInstance->atomTable->atomFind ( funcName.c_str () );
			rsp["breakpoints"][index]["id"] = it.first;
			if ( atm != 0 )
			{
				rsp["breakpoints"][index]["verified"] = false;
			} else
			{
				auto func = lastInstance->atomTable->atomGetFunc ( atm );
				rsp["breakpoints"][index]["source"]["path"] = func->loadImage->srcFiles.getName ( lastInstance->debug->debugGetCurrentSourceIndex ( func, func->cs ) );
				rsp["breakpoints"][index]["line"] = lastInstance->debug->debugGetCurrentLine ( func, func->cs );
				rsp["breakpoints"][index]["verified"] = true;
			}
		}
	}
	return rsp;
}

static jsonElement setBreakpoints ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	auto bp = req["breakpoints"];
	stringi const origName = *(*req["source"])["path"];
	stringi const fixedName = std::filesystem::path ( origName.c_str() ).generic_string ();

	auto &file = fileStore.getFile ( fixedName );

	file.bpList.clear ();

	for ( auto it = bp->cbeginArray (); it != bp->cendArray (); it++ )
	{
		int64_t const line = *((*it)["line"]);
		int64_t count = 0;
		stringi expr;

		if ( (*it)["hitCondition"] )
		{
			count = _atoi64 ( ((*it)["hitCondition"])->operator stringi().c_str () );
		}
		if ( (*it)["condition"] )
		{
			count = _atoi64 ( ((*it)["condition"])->operator stringi().c_str () );
		}

		file.bpList.push_back ( dbAdapaterBP ( fixedName, line, count, expr ) );
	}

	if ( activeDebugger )
	{
		for ( auto &it : file.bpList )
		{
			it.location = (int64_t) activeDebugger->debugAdjustBreakpoint ( lastInstance, fixedName, (size_t) std::get<int64_t>(it.location) );
			it.verified = true;
		}
		debugClearAllBreakpoints ( lastInstance, nullptr, 0 );

		for ( auto &it : file.bpList )
		{
			activeDebugger->debugSetBreakpoint ( lastInstance, it.fileName, (size_t)std::get<int64_t> ( it.location ) );
		}
	}


	jsonElement rsp;
	size_t index = 0;
	for ( auto &it : file.bpList )
	{
		rsp["breakpoints"][index]["verified"] = it.verified;
		rsp["breakpoints"][index]["line"] = std::get<int64_t> ( it.location );
		rsp["breakpoints"][index]["source"]["path"] = fixedName;
		index++;
	}
	return rsp;
}

static jsonElement stackTrace ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	int64_t startFrame = 0;
	int64_t levels = MAXINT64;

	if ( req["startFrame"] )
	{
		startFrame = *req["startFrame"];
	}
	if ( req["levels"] )
	{
		levels = *req["levels"];
	}

	jsonElement rsp;
	int64_t	nFramesReturned = 0;

	auto callStack = debugGetCallstack ( lastInstance );

	for ( size_t loop = startFrame; loop < callStack->entry.size () && loop < (size_t)levels; loop++ )
	{
		jsonElement stackFrame;

		stackFrame["id"] = (int64_t)loop;
		stackFrame["name"] = callStack->entry[loop].funcName;
		stackFrame["source"]["path"] = fileNameToUri ( callStack->entry[loop].fileName );
		stackFrame["line"] = callStack->entry[loop].line;
		stackFrame["column"] = 1;

		rsp["stackFrames"][nFramesReturned++] = stackFrame;
	}
	
	rsp["totalFrames"] = callStack->entry.size ();

	return rsp;
}

static jsonElement threads ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;
	rsp["threads"][0]["id"] = 0;
	rsp["threads"][0]["name"] = "<empty>";
	if ( lastInstance )
	{
		rsp["threads"][0]["id"] = (int64_t)lastInstance;
		rsp["threads"][0]["name"] = lastInstance->getName ();
	}
	return rsp;
}

static jsonElement setExceptionBreakpoints ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;
	rsp["breakpoints"] = jsonElement ().makeArray ();
	return rsp;
}

static jsonElement cont ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}

static jsonElement pause ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugHalt ( lastInstance );

	return jsonElement ();
}

static jsonElement terminate ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugTerminate ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}


static jsonElement next ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugStepOver ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}

static jsonElement stepIn ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugStepInto ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}

static jsonElement stepOut ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}

	debugStepOut ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}

static jsonElement scopes ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	int64_t frameId = *req["frameId"];

	if ( !lastInstance )
	{
		return jsonElement ().makeArray();
	}

	jsonElement rsp;

	rsp["scopes"].makeArray ();

	size_t index = 0;
	for ( auto &it : inspectors->entry )
	{
		rsp["scopes"][index]["name"] = it;
		rsp["scopes"][index]["variablesReference"] = varReferences.size() + 1;
		varReferences.push_back ( variableReference{ true, index, (size_t)frameId, nullptr } );		// dummy to indicate that this is a base, unexpanded inspector
		index++;
	}

	return rsp;
}

static jsonElement variables ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	int64_t ref = *req["variablesReference"];
	int64_t start = 0;
	int64_t count = MAXINT64;
	bool	displayAsHex = false;

	if ( !lastInstance )
	{
		return jsonElement ();
	}

	ref--;
	if ( req["start"] )
	{
		start = *req["start"];
	}
	if ( req["count"] )
	{
		count = *req["count"];
		if ( !count ) count = MAXINT64;
	}
	if ( req["format"] )
	{
		if ( *(*req["format"])["hex"] )
		{
			displayAsHex = *(*req["format"])["hex"];
		}
	}
	jsonElement	rsp;

	// we use size_t cast here ase a negative value will be huge and no match and we'll protect against that as well
	if ( !varReferences.size() || (size_t)ref > varReferences.size () )
	{
		rsp["variables"] = jsonElement ().makeArray ();
	} else
	{
		if ( varReferences[ref].isInspector )
		{
			// top level inspector request
			auto iList = debugInspect ( lastInstance, varReferences[ref].inspector, varReferences[ref].stackFrame );
			inspectorList.push_back ( iList );

			std::sort ( iList->entry.begin (), iList->entry.end (), []( vmDebugVar *left, vmDebugVar *right ) -> bool { return strccmp ( left->getName (), right->getName () ) < 0; } );

			size_t index = 0;
			for ( size_t loop2 = start; loop2 < (iList->entry.size () > ( size_t )count ? (size_t)count : iList->entry.size ()); loop2++ )
			{
				rsp["variables"][index]["name"] = stringi ( iList->entry[loop2]->getName (), iList->entry[loop2]->getNameLen () );
				rsp["variables"][index]["value"] = stringi ( iList->entry[loop2]->getValue (), iList->entry[loop2]->getValueLen () );
				rsp["variables"][index]["type"] = iList->entry[loop2]->getType ();
				rsp["variables"][index]["variablesReference"] = 0;
				if ( iList->entry[loop2]->hasChildren () )
				{
					rsp["variables"][index]["variablesReference"] = varReferences.size () + 1;
					varReferences.push_back ( variableReference{ false, (size_t)ref, varReferences[ref].stackFrame, iList->entry[loop2] } );
				}
				index++;
			}
		} else
		{
			vmDebugVar *var = varReferences[ref].debugVar;
			auto iList = debugInspect ( lastInstance, varReferences[ref].stackFrame, varReferences[ref].debugVar, 0, 0 );
			inspectorList.push_back ( iList );

			size_t index = 0;
			for ( size_t loop2 = start; loop2 < (iList->entry.size () > ( size_t )count ? (size_t)count : iList->entry.size ()); loop2++ )
			{
				rsp["variables"][index]["name"] = stringi ( iList->entry[loop2]->getName (), iList->entry[loop2]->getNameLen() );
				rsp["variables"][index]["value"] = stringi ( iList->entry[loop2]->getValue (), iList->entry[loop2]->getValueLen () );
				rsp["variables"][index]["type"] = iList->entry[loop2]->getType ();
				rsp["variables"][index]["variablesReference"] = 0;
				if ( iList->entry[loop2]->hasChildren () )
				{
					rsp["variables"][index]["variablesReference"] = varReferences.size () + 1;
					varReferences.push_back ( variableReference{ false, (size_t)ref, varReferences[ref].stackFrame, iList->entry[loop2] } );
				}
				index++;
			}
		}
	}

	return rsp;
}

static jsonElement setVariable ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	int64_t ref = *req["variablesReference"];
	stringi name = *req["name"];
	stringi value = *req["value"];
	jsonElement	rsp;

	ref--;
	// we use size_t cast here ase a negative value will be huge and no match and we'll protect against that as well
	if ( (size_t)ref <= varReferences.size() && varReferences[ref].isInspector )
	{
		auto iList = debugInspect ( lastInstance, ref, varReferences[ref].stackFrame );

		vmDebugVar *dbgVar = nullptr;

		for ( auto &it : iList->entry )
		{
			if ( name == it->getName () )
			{
				dbgVar = it;
				break;
			}
		}
		if ( dbgVar )
		{
			auto var = dbgVar->getRawVar ();

			switch ( var->type )
			{
				case slangType::eSTRING:
					*var = VAR_STR ( lastInstance, value );
					break;
				case slangType::eDOUBLE:
					var->dat.d = atof ( value.c_str () );
					break;
				case slangType::eLONG:
					var->dat.l = _atoi64 ( value.c_str () );
					break;
				default:
					rsp["name"] = dbgVar->getName ();
					rsp["value"] = dbgVar->getValue ();
					rsp["variablesReference"] = ref;
					break;
			}
			dbgVar->rebuildData ( lastInstance );
			rsp["name"] = dbgVar->getName ();
			rsp["value"] = dbgVar->getValue ();
			rsp["variablesReference"] = 0;
		} else
		{
			rsp["name"] = name;
			rsp["value"] = "<error>";
			rsp["variablesReference"] = 0;
		}
	}
	return rsp;
}

static jsonElement eval ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	int64_t frameId = 0;
	stringi context;
		
	if ( req["frameId"] )
	{
		frameId = *req["frameId"];
	}

	if ( req["context"] )
	{
		context = *req["context"];
	}

	stringi expression = *req["expression"];

	jsonElement  rsp;

	auto funcDef = lastInstance->debug->debugFunc ( lastInstance, frameId );
	auto basePtr = lastInstance->debug->debugBasePtr ( lastInstance, frameId );
	if ( funcDef )
	{
		VAR resultSave = lastInstance->result;
		auto stackSave = lastInstance->stack;
		auto opSave = lastInstance->debug->IP;
		auto stateSave = lastInstance->debug->enabled;
		auto errorSave = lastInstance->error;

		lastInstance->debug->enabled = false;
		*lastInstance->stack = VAR_STR ( expression );
		lastInstance->stack++;

		VAR *res = lastInstance->om->allocVar ( 1 );
		try
		{
			if ( context == "repl" )
			{
				op_compile ( lastInstance, funcDef, true );
				op_cbFixupNoPromote ( lastInstance, funcDef, basePtr );
				op_cbCall ( lastInstance, lastInstance->debug->IP, funcDef, basePtr, 0 );
				*res = lastInstance->result;
			} else
			{
				if ( op_compile ( lastInstance, funcDef, false ) )
				{
					op_cbFixupNoPromote ( lastInstance, funcDef, basePtr );
					op_cbCall ( lastInstance, lastInstance->debug->IP, funcDef, basePtr, 0 );
					*res = lastInstance->result;
				} else
				{
					res->type = slangType::eNULL;
				}
			}
		} catch ( ... )
		{
			res->type = slangType::eNULL;
		}
		lastInstance->debug->enabled = stateSave;
		lastInstance->debug->IP = opSave;
		lastInstance->stack = stackSave;
		lastInstance->result = resultSave;
		lastInstance->error = errorSave;

		vmInspectList *iList = new vmInspectList ();
		iList->entry.push_back ( new vmDebugLocalVar ( lastInstance, "eval", res ) );

		rsp["result"] = iList->entry[0]->getValue ();
		rsp["variablesReference"] = 0;
		if ( iList->entry[0]->hasChildren () )
		{
			rsp["variablesReference"] = varReferences.size () + 1;
			varReferences.push_back ( variableReference{ false, 0, (size_t)frameId, iList->entry[0] } );
		}
	}

	return rsp;
}

static jsonElement gotoTargets ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ()["targets"].makeArray ();
	}
	auto funcDef = lastInstance->debug->debugFunc ( lastInstance, 0 );
	auto targets = lastInstance->debug->debugGetGotoTargets ( lastInstance, funcDef );

	jsonElement rsp;

	rsp["targets"].makeArray ();	// in case we don't have any actual targets makeArray() is necessary to have a properly formatted message to return

	size_t index = 0;
	for ( auto &it : targets )
	{
		rsp["targets"][index]["id"] = it;
		rsp["targets"][index]["line"] = it;
		rsp["targets"][index]["label"] = stringi() + it;
		index++;
	}

	return rsp;
}

static jsonElement gotoIp ( jsonElement const &req, jsonRPCServerBase &server, HWND handle )
{
	int64_t lineNum = *req["targetId"];

	auto funcDef = lastInstance->debug->debugFunc ( lastInstance, 0 );

	auto fileName = funcDef->loadImage->srcFiles.getName ( lastInstance->debug->debugGetCurrentSourceIndex ( funcDef, lastInstance->debug->IP ) );
	lastInstance->debug->debugSetIP ( lastInstance, funcDef, fileName, lineNum );

	activeDebugger->lastMsg.location.line = lineNum;
	activeDebugServer->sendNotification ( (size_t)notifications::stopped );

	return jsonElement ();
}

//  ********************** EVENT HANDLERS *************************** */
static std::pair<stringi, jsonElement> initialized ( jsonRPCServerBase &server, HWND handle )
{
	return { "initialized", jsonElement () };
}
static std::pair<stringi, jsonElement> breakpoint ( jsonRPCServerBase &server, HWND handle )
{
	return { "breakpoint", jsonElement () };
}
static std::pair<stringi, jsonElement> continued ( jsonRPCServerBase &server, HWND handle )
{
	return { "continued", jsonElement () };
}
static std::pair<stringi, jsonElement> exited ( jsonRPCServerBase &server, HWND handle )
{
	jsonElement	rsp;
	rsp["terminated"] = 0;
	lastInstance->error.errorNum;
	return { "terminated", rsp };
}
static std::pair<stringi, jsonElement> invalidated ( jsonRPCServerBase &server, HWND handle )
{
	return { "invalidated", jsonElement () };
}
static std::pair<stringi, jsonElement> loadedSource ( jsonRPCServerBase &server, HWND handle )
{
	return { "loadedSource", jsonElement () };
}
static std::pair<stringi, jsonElement> output ( jsonRPCServerBase &server, HWND handle )
{
	std::lock_guard  g1 ( debuggerLock );

	jsonElement rsp;
	rsp["category"] = "stdout";
	rsp["output"] = stringi ( outputBuffer.data<char *>(), outputBuffer.size() );
	outputBuffer.reset ();
	return { "output", rsp };
}
static std::pair<stringi, jsonElement> process ( jsonRPCServerBase &server, HWND handle )
{
	return { "process", jsonElement () };
}
static std::pair<stringi, jsonElement> stopped ( jsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;

	switch ( activeDebugger->lastMsg.state )
	{
		case vmStateType::vmHalt:
			{
				char				 msgBuff[256];

				_snprintf_s ( msgBuff, sizeof ( msgBuff ), _TRUNCATE, "%s    %s:%s(%I64u): error E%x: %s", activeDebugger->lastMsg.instance->getName (), activeDebugger->lastMsg.location.fName, activeDebugger->lastMsg.location.functionName, activeDebugger->lastMsg.location.line, activeDebugger->lastMsg.location.err, scCompErrorAsText ( int(activeDebugger->lastMsg.location.err) ).c_str () );
				rsp["reason"] = "exception";		// uncaught exception
				rsp["description"] = "uncaught exception";
				rsp["text"] = stringi ( msgBuff );
			}
			break;
		case vmStateType::vmTrap:
			rsp["reason"] = "exception";		// caught exception but showing for debugging purposes
			rsp["description"] = "exception";
			break;
		case vmStateType::vmTrace:
			rsp["reason"] = "step";
			break;
		case vmStateType::vmDebugBreak:
			rsp["reason"] = "breakpoint";
			rsp["hitBreakpointIds"][0] = 0;		// TODO - map to real breakpoint
			break;
		case vmStateType::vmThreadStart:
			rsp["reason"] = "entry";
			rsp["description"] = "start of execution";
			break;
		default:
			return { "", rsp };
			break;
	}
	rsp["threadId"] = reinterpret_cast<int64_t>(lastInstance);

	return { "stopped", rsp };
}
static std::pair<stringi, jsonElement> terminated ( jsonRPCServerBase &server, HWND handle )
{
	return { "terminated", jsonElement () };
}
static std::pair<stringi, jsonElement> thread ( jsonRPCServerBase &server, HWND handle )
{
	return { "thread", jsonElement () };
}

taskControl *debugAdapterInit ( uint16_t port )
{
	if ( !debuggerRunning )
	{
		std::thread t1 ( debugServerThread );
		t1.detach ();

		HANDLE readHandle;
		HANDLE writeHandle;

		CreatePipe ( &readHandle, &writeHandle, 0, 0 );

		writePipeDesc = _open_osfhandle ( (intptr_t)writeHandle, _O_APPEND | _O_WRONLY );
		readPipeDesc = _open_osfhandle ( (intptr_t)readHandle, _O_RDONLY );

		std::thread t2 ( debugServerOutputThread );
		t2.detach ();
	}

	while ( !debuggerRunning )
	{
		Sleep ( 10 );
	}

	auto svr = new socketServer<HWND> ( port, std::move ( debuggerWindowHandle ) );

	svr->setMethods (
						{
							{ "initialize", initialize },
							{ "disconnect", disconnect },
							{ "loadedSources", loadedSources },
							{ "launch", launch },
							{ "attach", attach },
							{ "setBreakpoints", setBreakpoints },
							{ "setFunctionBreakpoints", setFunctionBreakpoints },
							{ "setExceptionBreakpoints", setExceptionBreakpoints },
							{ "stacktrace", stackTrace },
							{ "threads", threads },
							{ "continue", cont },
							{ "scopes", scopes },
							{ "variables", variables },
							{ "next", next },
							{ "stepIn", stepIn },
							{ "stepOut", stepOut },
							{ "setVariable", setVariable },
							{ "evaluate", eval },
							{ "pause", pause },
							{ "terminate", terminate },
							{ "gotoTargets", gotoTargets },
							{ "goto", gotoIp },
//							{ "initialize", initialize },
//							{ "initialize", initialize },
//							{ "initialize", initialize },
						}
					);

	svr->setNotificationHandler (
									{
#undef DEF
#define DEF(a) a,
									NOTES
									}
								);

	auto tc = new socketServerTaskControl<HWND> ( HWND ( 0 ) );
	svr->startServer ( tc );
	printf ( "debugProtocolAdapter started on port %i\r\n", port );
	return tc;
}
