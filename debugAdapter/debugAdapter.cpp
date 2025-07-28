// debugAdapter.cpp : Defines the functions for the static library.
//

#include "Utility/settings.h"

#ifdef _WIN32
#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <winsock2.h>
#include <windef.h>
#else
#include <sys/socket.h>
#endif

#include <fcntl.h>      /* Needed only for _O_RDWR definition */
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <filesystem>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <type_traits>
#include <algorithm>

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcVM/bcVMBuiltin.h"

#include "Target/fileCache.h"

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
static	winDebugger *activeDebugger = nullptr;
static	dbgJsonRPCServerBase *activeDebugServer = nullptr;
static	vmInstance *lastInstance = nullptr;
static	debuggerFileStore				 fileStore;
int										 writePipeDesc;
int										 readPipeDesc;
BUFFER									 outputBuffer;

// this data must be cleared at each stop
static  std::vector<variableReference>	 varReferences;
static  vmInspectorList *inspectors = nullptr;
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

static void standaloneRunner ( std::unique_ptr<vmTaskInstance> instance, bool doOutputString, bool printOmStats )
{
	try
	{
		instance->run ( "main", true );
	} catch ( ... )
	{
		instance->log ( logger::level::INFO, logger::modules::SYSTEM, "****** error during execution" );
	}

	if ( doOutputString )
	{
		printf ( "%.*s\r\n", (int)classIVarAccess ( instance->getGlobal ( "userCode", "__outputString" ), "size" )->dat.l, classIVarAccess ( instance->getGlobal ( "userCode", "__outputString" ), "buff" )->dat.str.c );
	}

	if ( printOmStats )
	{
		instance->om->printStats ();
	}

	instance->om->processDestructors ();
	instance->reset ();
}


static bool standaloneLaunch ( std::vector<stringi> const &fList, bool noInline, bool doListing, bool doProfiling, bool doBraces )
{
	BUFFER				 buff ( 1024ull * 1024 * 4 );
	BUFFER				 objBuff ( 1024ull * 1024 * 4 );
	char *code = nullptr;
	bool				 isAP = false;
	bool				 doOutputString = false;
	bool				 printOmStats = false;
	bool				 doTest = false;
	bool				 noRun = false;

	std::unique_ptr<vmTaskInstance> instance = std::make_unique<vmTaskInstance> ( fList[0].c_str () );

	debugServerInit ( instance.get () );

	// compile the built-in code
	auto [builtIn, builtInSize] = builtinInit ( instance.get (), builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE

	opFile oCombined;

	isAP = false;

	for ( size_t loop = 0; loop < fList.size (); loop++ )
	{
		stringi file = std::filesystem::absolute ( fList[loop].c_str () ).string ();

		instance->log ( logger::level::INFO, logger::modules::SYSTEM, "file: %s", file.c_str () );
		{
			if ( isAP )
			{
				code = compAPPreprocessor ( file.c_str (), doBraces, true );
			} else
			{
				auto entry = globalFileCache.read ( file.c_str () );

				if ( !entry )
				{
					instance->log ( logger::level::ERROR, logger::modules::SYSTEM, "reading file: %s", file.c_str () );
					return 0;
				}
				BUFFER b;

				b.write ( entry->getData (), entry->getDataLen () );
				b.put ( 0 );	// need to do this to ensure null termination
				entry->free ();

				code = compPreprocessor ( file.c_str (), b.data<char *> (), doBraces );
			}
		}

		opFile	oFile;

		oFile.parseFile ( file.c_str (), code, doBraces, false, false, sourceFile::sourceFileType::external );
		if ( oFile.errHandler.isFatal () )
		{
			return false;
		}

		free ( code );

		objBuff.reset ();
		oFile.addBuildSource ( file );
		oFile.serialize ( &objBuff, false );

		if ( !doTest ) instance->log ( logger::level::INFO, logger::modules::SYSTEM, "object file size = %zu", bufferSize ( &objBuff ) );

		// code being built
		auto obj = (uint8_t const *)bufferBuff ( &objBuff );
		oCombined.addStream ( (const char **)&obj, false, false );
	}

	char const *obj = (char const *)builtIn;
	oCombined.addStream ( &obj, false, false );

	if ( oCombined.errHandler.isFatal () )
	{
		return false;
	}

	try
	{
		oCombined.loadLibraries ( false, false );
	} catch ( ... )
	{
		printf ( "****** error loading libraries\r\n" );
		return false;
	}

	compExecutable comp ( &oCombined );
	if ( doListing )
	{
		comp.listing.level = compLister::listLevel::LIGHT;
	}
	if ( doProfiling ) comp.enableProfiling ();
	comp.enableDebugging ();
	try
	{
		comp.opt.setInlining ( false );
		comp.genCode ( noRun ? 0 : "main" );
	} catch ( errorNum err )
	{
		oCombined.errHandler.throwFatalError ( false, err );
	}

	if ( oCombined.errHandler.isFatal () )
	{
		return 0;
	}

	comp.serialize ( &buff, false );

	if ( !doTest ) instance->log ( logger::level::INFO, logger::modules::SYSTEM, "compiled file size = %zu", bufferSize ( &buff ) );
	if ( !doTest ) instance->log ( logger::level::INFO, logger::modules::SYSTEM, "optimization time = %I64u ms", comp.tOptimizer );
	if ( !doTest ) instance->log ( logger::level::INFO, logger::modules::SYSTEM, "back end time     = %I64u ms", comp.tBackEnd );

	if ( !doTest ) comp.listing.printOutput ();

	{
		std::shared_ptr<uint8_t> dup2 ( (uint8_t *)bufferDetach ( &buff ), [=]( auto p ) { free ( p ); } );
		instance->load ( dup2, "usercode", false, false );
	}

	std::thread task ( standaloneRunner, std::move ( instance ), doOutputString, printOmStats );
	task.detach ();
	return 1;
}

static LRESULT CALLBACK debuggerWindowProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	vmDebugMessage *msg;

	switch ( uMsg )
	{
		case WM_SLANG_DEBUG:
			msg = reinterpret_cast<vmDebugMessage *>(lParam);	// NOLINT(performance-no-int-to-ptr)

			if ( activeDebugServer && (!lastInstance || (msg->instance == lastInstance)) )
			{
				if ( targetDebugFile.size () )
				{
					if ( targetDebugFile != msg->location.fName && targetDebugFile != "<any>" )
					{
						debugContinue ( msg );
						break;
					}
				}
				lastInstance = msg->instance;
				lastInstance->outputDesc = writePipeDesc;
				activeDebugger = static_cast<winDebugger *>(lastInstance->debug.get ());

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
						activeDebugServer->sendNotification ( (size_t)notifications::terminated );
						activeDebugServer = nullptr;
						if ( lastInstance ) lastInstance->outputDesc = 0;
						lastInstance = nullptr;
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
						activeDebugServer->sendNotification ( (size_t)notifications::thread );
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
			if ( activeDebugServer )
			{
				activeDebugServer->sendNotification ( (size_t)notifications::output );
			}
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
	debugWindowClass.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr ( GetConsoleWindow (), GWLP_HINSTANCE ));		// NOLINT(performance-no-int-to-ptr)
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
						  (HINSTANCE)GetWindowLongPtr ( GetConsoleWindow (), GWLP_HINSTANCE ),			// NOLINT(performance-no-int-to-ptr)
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
	std::shared_ptr<vmDebug> debugger ( static_cast<vmDebug *>(new winDebugger ( debuggerWindowHandle )) );
	debugRegister ( instance, debugger );
}

static jsonElement initialize ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement capabilities;

	printf ( "Debug client %s connected\r\n", req["clientName"].operator const stringi & ().c_str () );

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
	capabilities["supportsModulesRequest"] = true;
	capabilities["supportsExceptionOptions"] = true;
	capabilities["supportsValueFormattingOptions"] = true;
	capabilities["supportsExceptionInfoRequest"] = true;
	capabilities["supportTerminateDebuggee"] = true;
	capabilities["supportSuspendDebuggee"] = false;
	capabilities["supportsDelayedStackTraceLoading"] = true;
	capabilities["supportsLoadedSourcesRequest"] = true;
	capabilities["supportsLogPoints"] = true;
	capabilities["supportsTerminateThreadsRequest"] = true;
	capabilities["supportsSetExpression"] = true;
	capabilities["supportsTerminateRequest"] = true;
	capabilities["supportsDataBreakpoints"] = false;
	capabilities["supportsReadMemoryRequest"] = false;
	capabilities["supportsDisassembleRequest"] = false;
	capabilities["supportsCancelRequest"] = true;
	capabilities["supportsBreakpointLocationsRequest"] = false;		// TODO
	capabilities["supportsClipboardContext"] = false;
	capabilities["supportsSteppingGranularity"] = false;
	capabilities["supportsInstructionBreakpoints"] = false;
	capabilities["supportsExceptionFilterOptions"] = false;
	capabilities["exceptionBreakpointFilters"][0]["filter"] = "uncaught";
	capabilities["exceptionBreakpointFilters"][0]["label"] = "Uncaught exceptions";
	capabilities["exceptionBreakpointFilters"][0]["description"] = "Breaks only on exceptions that are not handled";
	capabilities["exceptionBreakpointFilters"][1]["filter"] = "caught";
	capabilities["exceptionBreakpointFilters"][1]["label"] = "Caught exceptions";
	capabilities["exceptionBreakpointFilters"][1]["description"] = "Break on exceptions, even if they are handled later";
//	server.scheduleNotification ( notifications::initialized );
	return capabilities;
}

static jsonElement launch ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	stringi const origName = req["program"];
	bool	stopOnEntry = req.has ( "stopOnEntry" ) ? (bool)req["stopOnEntry"] : false;
	bool	noInline = req.has ( "noInline" ) ? (bool)req["noInline"] : false;
	bool	doList = req.has ( "list" ) ? (bool)req["list"] : false;
	bool	doProfile = req.has ( "profile" ) ? (bool)req["profile"] : false;

	std::filesystem::path fName = origName.c_str ();
	if ( stringi ( ".sl" ) == fName.extension ().string () )
	{
		standaloneLaunch ( {origName.c_str ()}, noInline, doList, doProfile, true );
	} else if ( stringi ( ".fgl" ) == fName.extension ().string () )
	{
		standaloneLaunch ( {origName.c_str ()}, noInline, doList, doProfile, false );
	} else
	{
		if ( origName.size () )
		{
			stringi const fixedName = std::filesystem::path ( origName.c_str () ).generic_string ();
			targetDebugFile = fixedName;
		} else
		{
			targetDebugFile.clear ();
		}
	}

//	static bool standaloneLaunch( vmInstance & instance, std::vector<stringi const > const &fList, bool noInline, bool doListing, bool doProfiling, bool doBraces )

	breakOnEntry = stopOnEntry;
	return jsonElement ();
}

static jsonElement attach ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	targetDebugFile.clear ();
	breakOnEntry = false;
	return jsonElement ();
}

static jsonElement disconnect ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	server.terminate ();
	return jsonElement ();
}

static jsonElement setFunctionBreakpoints ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	auto &bp = req["breakpoints"];

	fileStore.functionBreakpoints.clear ();

	for ( auto it = bp.cbeginArray (); it != bp.cendArray (); it++ )
	{
		int64_t const id = (*it)["id"];
		int64_t const funcName = (*it)["name"];
		stringi count = (*it).conditionalGet ( "hitCount", stringi {} );
		stringi condition = (*it).conditionalGet ( "hitCondition", stringi {} );
		stringi logMessage = (*it).conditionalGet ( "logMessage", stringi {} );

		fileStore.functionBreakpoints[id] = dbAdapaterBP ( funcName, condition, count, logMessage );
	}

	jsonElement rsp;
	size_t index = 0;
	if ( activeDebugger && lastInstance )
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
				rsp["breakpoints"][index]["source"]["path"] = func->loadImage->srcFiles.getName ( lastInstance->debug->GetCurrentSourceIndex ( func, func->cs ) );
				rsp["breakpoints"][index]["line"] = lastInstance->debug->GetCurrentLine ( func, func->cs );
				rsp["breakpoints"][index]["verified"] = true;
			}
		}
	}
	return rsp;
}

static jsonElement setBreakpoints ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	auto &bp = req["breakpoints"];
	stringi const origName = req["source"]["path"];
	stringi const fixedName = std::filesystem::path ( origName.c_str () ).generic_string ();

	auto &file = fileStore.getFile ( fixedName );

	for ( auto it = file.bpList.begin (); it != file.bpList.end (); )
	{
		if ( it->fileName == fixedName )
		{
			it = file.bpList.erase ( it );
		} else
		{
			it++;
		}
	}

	for ( auto it = bp.cbeginArray (); it != bp.cendArray (); it++ )
	{
		int64_t const line = (*it)["line"];
		stringi count;
		stringi condition;
		stringi logMessage;

		if ( (*it).has ( "hitCondition" ) )
		{
			count = (*it)["hitCondition"];
		}
		if ( (*it).has ( "condition" ) )
		{
			condition = (*it)["condition"];
		}
		if ( (*it).has ( "logMessage" ) )
		{
			logMessage = (*it)["logMessage"];
		}

		file.bpList.push_back ( dbAdapaterBP ( fixedName, line, condition, count, logMessage ) );
	}

	if ( activeDebugger && lastInstance )
	{
		for ( auto &it : file.bpList )
		{
			it.location = (int64_t)activeDebugger->AdjustBreakpoint ( lastInstance, fixedName, (size_t)std::get<int64_t> ( it.location ) );
			it.verified = true;
		}
		debugClearAllBreakpoints ( lastInstance, fixedName );

		for ( auto &it : file.bpList )
		{
			activeDebugger->SetBreakpoint ( lastInstance, it.fileName, (size_t)std::get<int64_t> ( it.location ), it.expr, it.count, it.logMessage );
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

static jsonElement stackTrace ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	int64_t			startFrame = 0;
	int64_t			levels = MAXINT64;
	jsonElement		rsp;
	int64_t			nFramesReturned = 0;

	if ( req.has ( "startFrame" ) )
	{
		startFrame = req["startFrame"];
	}
	if ( req.has ( "levels" ) )
	{
		levels = req["levels"];
	}

	auto callStack = debugGetCallstack ( lastInstance );

	rsp["stackFrames"].makeArray ();

	for ( size_t loop = startFrame; loop < callStack->entry.size () && loop < (size_t)levels; loop++ )
	{
		jsonElement stackFrame;

		stackFrame["id"] = (int64_t)loop;
		stackFrame["name"] = callStack->entry[loop].funcName;
		stackFrame["source"]["name"] = callStack->entry[loop].fileName;
		stackFrame["source"]["path"] = fileNameToUri ( callStack->entry[loop].fileName );
		stackFrame["line"] = callStack->entry[loop].line;
		stackFrame["column"] = 1;

		rsp["stackFrames"][nFramesReturned++] = stackFrame;
	}

	rsp["totalFrames"] = callStack->entry.size ();

	return rsp;
}

static jsonElement exceptionInfo ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;

	auto exceptionInfo = debugGetExceptionInfo ( lastInstance );

	rsp["exceptionId"] = exceptionInfo.exceptionId;
	rsp["description"] = exceptionInfo.description;

	switch ( exceptionInfo.breakMode )
	{
		case errorLocation::exceptionBreakType::always:
			rsp["breakMode"] = "always";
			break;
		case errorLocation::exceptionBreakType::unhandled:
			rsp["breakMode"] = "unhandled";
			break;
		case errorLocation::exceptionBreakType::handled:
			rsp["breakMode"] = "handled";
			break;
		default:
			rsp["breakMode"] = "never";		// like wtf? should never get here but a safe default
			break;
	}

	return rsp;
}

static jsonElement threads ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;
	rsp["threads"][0]["id"] = 0;
	rsp["threads"][0]["name"] = "<empty>";
	if ( lastInstance )
	{
		rsp["threads"][0]["id"] = reinterpret_cast<int64_t>(lastInstance);
		rsp["threads"][0]["name"] = lastInstance->getName ();
	}
	return rsp;
}

static jsonElement setExceptionBreakpoints ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;

	auto &bp = req["filters"];

	if ( bp )
	{
		errorLocation::exceptionBreakType	breakMode = errorLocation::exceptionBreakType::never;

		for ( auto it = bp.cbeginArray (); it != bp.cendArray (); it++ )
		{
			if ( (*it).operator const stringi & () == "caught" )
			{
				if ( breakMode == errorLocation::exceptionBreakType::unhandled )
				{
					breakMode = errorLocation::exceptionBreakType::always;
				} else if ( breakMode == errorLocation::exceptionBreakType::never )
				{
					breakMode = errorLocation::exceptionBreakType::handled;
				}
			} else if ( (*it).operator const stringi & () == "uncaught" )
			{
				if ( breakMode == errorLocation::exceptionBreakType::handled )
				{
					breakMode = errorLocation::exceptionBreakType::always;
				} else if ( breakMode == errorLocation::exceptionBreakType::never )
				{
					breakMode = errorLocation::exceptionBreakType::unhandled;
				}
			}
		}

		debugSetExceptionGlobalBreakType ( lastInstance, breakMode );
	}
	return rsp;
}

static jsonElement cont ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugContinue ( &activeDebugger->lastMsg );

	jsonElement rsp;

	rsp["threadId"] = reinterpret_cast<int64_t>(lastInstance);
	rsp["allThreadsContinued"] = true;
	return rsp;
}

static jsonElement pause ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugHalt ( lastInstance );

	return jsonElement ();
}

static jsonElement terminate ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugTerminate ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	activeDebugServer->sendNotification ( (size_t)notifications::terminated );

	lastInstance = nullptr;
	activeDebugServer = nullptr;

	return jsonElement ();
}


static jsonElement next ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugStepOver ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}

static jsonElement stepIn ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}
	debugStepInto ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}

static jsonElement stepOut ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ();
	}

	debugStepOut ( lastInstance );
	debugContinue ( &activeDebugger->lastMsg );

	return jsonElement ();
}

static jsonElement scopes ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	int64_t frameId = req["frameId"];

	if ( !lastInstance )
	{
		return jsonElement ().makeArray ();
	}

	jsonElement rsp;

	rsp["scopes"].makeArray ();

	size_t index = 0;
	for ( auto &it : inspectors->entry )
	{
		rsp["scopes"][index]["name"] = it;
		rsp["scopes"][index]["variablesReference"] = varReferences.size () + 1;
		varReferences.push_back ( variableReference {true, index, (size_t)frameId, nullptr} );		// dummy to indicate that this is a base, unexpanded inspector
		index++;
	}

	return rsp;
}

static jsonElement variables ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	int64_t ref = req["variablesReference"];
	int64_t start = 0;
	int64_t count = MAXINT64;
	if ( !lastInstance )
	{
		return jsonElement ();
	}

	ref--;
	if ( req.has ( "start" ) )
	{
		start = req["start"];
	}
	if ( req.has ( "count" ) )
	{
		count = req["count"];
		if ( !count ) count = MAXINT64;
	}
#if 0
	bool	displayAsHex = false;

	if ( req["format"] )
	{
		if ( *(*req["format"])["hex"] )
		{
			displayAsHex = *(*req["format"])["hex"];
		}
	}
#endif
	jsonElement	rsp;

	// we use size_t cast here ase a negative value will be huge and no match and we'll protect against that as well
	if ( !varReferences.size () || (size_t)ref > varReferences.size () )
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
					varReferences.push_back ( variableReference {false, (size_t)ref, varReferences[ref].stackFrame, iList->entry[loop2]} );
				}
				index++;
			}
		} else
		{
			auto iList = debugInspect ( lastInstance, varReferences[ref].stackFrame, varReferences[ref].debugVar, 0, 0 );
			inspectorList.push_back ( iList );

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
					varReferences.push_back ( variableReference {false, (size_t)ref, varReferences[ref].stackFrame, iList->entry[loop2]} );
				}
				index++;
			}
		}
	}

	return rsp;
}

static jsonElement setVariable ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	int64_t ref = req["variablesReference"];
	stringi name = req["name"];
	stringi value = req["value"];
	jsonElement	rsp;

	ref--;
	// we use size_t cast here ase a negative value will be huge and no match and we'll protect against that as well
	if ( (size_t)ref <= varReferences.size () && varReferences[ref].isInspector )
	{
		auto iList = debugInspect ( lastInstance, varReferences[ref].inspector, varReferences[ref].stackFrame );

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

			if ( var )
			{
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
					case slangType::eNULL:
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
				rsp["name"] = dbgVar->getName ();
				rsp["value"] = dbgVar->getValue ();
				rsp["variablesReference"] = 0;
			}
		} else
		{
			rsp["name"] = name;
			rsp["value"] = "<error>";
			rsp["variablesReference"] = 0;
		}
	} else if ( (size_t)ref <= varReferences.size () && !varReferences[ref].isInspector )
	{
		auto iList = varReferences[ref].debugVar->getChildren ( lastInstance, nullptr, 0, 0 );

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

			if ( var )
			{
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
					case slangType::eNULL:
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
				rsp["name"] = dbgVar->getName ();
				rsp["value"] = dbgVar->getValue ();
				rsp["variablesReference"] = 0;
			}
		} else
		{
			rsp["name"] = name;
			rsp["value"] = "<error>";
			rsp["variablesReference"] = 0;
		}
	}

	return rsp;
}

static jsonElement eval ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	int64_t frameId = req.conditionalGet ( "frameId", (int64_t)0 );
	stringi context = req.conditionalGet ( "context", stringi () );
	stringi expression = req["expression"];

	jsonElement  rsp;

	auto funcDef = lastInstance->debug->Func ( lastInstance, frameId );
	auto basePtr = lastInstance->debug->BasePtr ( lastInstance, frameId );
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
			if ( op_compile ( lastInstance, funcDef, context == "repl" ) )
			{
				op_cbFixupNoPromote ( lastInstance, funcDef, basePtr );
				op_cbCall ( lastInstance, lastInstance->debug->IP, funcDef, basePtr, 0 );
				*res = lastInstance->result;
			} else
			{
				res->type = slangType::eNULL;
			}
		} catch ( errorNum err )
		{
			*res = VAR_STR ( lastInstance, stringi ( "(" ) + scCompErrorAsText ( size_t ( err ) ) + ")" );
		} catch ( ... )
		{
			*res = VAR_STR ( lastInstance, "(evaluation error)" );
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
			varReferences.push_back ( variableReference {false, 0, (size_t)frameId, iList->entry[0]} );
		}
	}

	return rsp;
}

static jsonElement gotoTargets ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	if ( !lastInstance )
	{
		return jsonElement ()["targets"].makeArray ();
	}
	auto funcDef = lastInstance->debug->Func ( lastInstance, 0 );
	auto targets = lastInstance->debug->GetGotoTargets ( lastInstance, funcDef );

	jsonElement rsp;

	rsp["targets"].makeArray ();	// in case we don't have any actual targets makeArray() is necessary to have a properly formatted message to return

	size_t index = 0;
	for ( auto &it : targets )
	{
		rsp["targets"][index]["id"] = it;
		rsp["targets"][index]["line"] = it;
		rsp["targets"][index]["label"] = stringi () + it;
		index++;
	}

	return rsp;
}

static jsonElement gotoIp ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	int64_t lineNum = req["targetId"];

	auto funcDef = lastInstance->debug->Func ( lastInstance, 0 );

	auto fileName = funcDef->loadImage->srcFiles.getName ( lastInstance->debug->GetCurrentSourceIndex ( funcDef, lastInstance->debug->IP ) );
	lastInstance->debug->SetIP ( lastInstance, funcDef, fileName, lineNum );

	activeDebugger->lastMsg.location.line = lineNum;
	activeDebugServer->sendNotification ( (size_t)notifications::stopped );

	return jsonElement ();
}

static jsonElement modules ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement ret;

	ret["modules"].makeArray ();
	if ( lastInstance )
	{
		lastInstance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t atomIndex )
										   {
											   auto name = lastInstance->atomTable->atomGetName ( atomIndex );
											   bcLoadImage *image = lastInstance->atomTable->atomGetLoadImage ( atomIndex );

											   jsonElement sourceModule;
											   sourceModule["id"] = (int64_t)image;
											   sourceModule["name"] = image->name;
											   sourceModule["path"] = name;
											   sourceModule["isOptimized"] = true;
											   sourceModule["isUserCode"] = image->nDebugEntries;
											   sourceModule["symbolStatus"] = image->nDebugEntries ? "Symbols Loaded" : "Symbols Not Found";

											   ret["modules"].push_back ( sourceModule );
											   return false;
										   }
		);
	}
	return ret;
}

static jsonElement loadedSources ( jsonElement const &req, dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement ret;

	ret["sources"].makeArray ();
	if ( lastInstance )
	{
		lastInstance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t atomIndex )
										   {

											   auto name = lastInstance->atomTable->atomGetName ( atomIndex );
											   bcLoadImage *image = lastInstance->atomTable->atomGetLoadImage ( atomIndex );

											   jsonElement sourceModule;
											   sourceModule["name"] = image->name;
											   sourceModule["path"] = name;

											   jsonElement sourceFiles;

											   for ( int32_t index = 0; index < image->srcFiles.getNumSources (); index++ )
											   {
												   auto name = image->srcFiles.getName ( index );
												   if ( strccmp ( name, "(internal)" ) )
												   {
													   sourceFiles.push_back ( {{ "name", image->srcFiles.getName ( index ) }, { "path", image->srcFiles.getName ( index ) }} );
												   }
											   }

											   sourceModule["sources"] = sourceFiles;

											   ret["sources"].push_back ( sourceModule );
											   return false;
										   }
		);
	}

	return ret;
}
//  ********************** EVENT HANDLERS *************************** */
static std::pair<stringi, jsonElement> initialized ( dbgJsonRPCServerBase &server, HWND handle )
{
	return {"initialized", jsonElement ()};
}
static std::pair<stringi, jsonElement> breakpoint ( dbgJsonRPCServerBase &server, HWND handle )
{
	return {"breakpoint", jsonElement ()};
}
static std::pair<stringi, jsonElement> continued ( dbgJsonRPCServerBase &server, HWND handle )
{
	return {"continued", jsonElement ()};
}
static std::pair<stringi, jsonElement> exited ( dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement	rsp;
	rsp["exited"] = 0;
	rsp["exitCode"] = lastInstance->error.errorNum;
	return {"exited", rsp};
}
static std::pair<stringi, jsonElement> invalidated ( dbgJsonRPCServerBase &server, HWND handle )
{
	return {"invalidated", jsonElement ()};
}
static std::pair<stringi, jsonElement> loadedSource ( dbgJsonRPCServerBase &server, HWND handle )
{
	return {"loadedSource", jsonElement ()};
}
static std::pair<stringi, jsonElement> output ( dbgJsonRPCServerBase &server, HWND handle )
{
	std::lock_guard  g1 ( debuggerLock );

	jsonElement rsp;
	rsp["category"] = "stdout";
	rsp["output"] = stringi ( outputBuffer.data<char *> (), outputBuffer.size () );
	outputBuffer.reset ();
	return {"output", rsp};
}
static std::pair<stringi, jsonElement> process ( dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;

	rsp["name"] = activeDebugger->lastMsg.location.fName;
	rsp["startMethod"] = "launch";
	return {"process", rsp};
}
static std::pair<stringi, jsonElement> thread ( dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;

	rsp["reason"] = activeDebugger->lastMsg.location.fName;
	rsp["threadId"] = reinterpret_cast<int64_t>(lastInstance);
	return {"thread", rsp};
}
static std::pair<stringi, jsonElement> stopped ( dbgJsonRPCServerBase &server, HWND handle )
{
	jsonElement rsp;

	switch ( activeDebugger->lastMsg.state )
	{
		case vmStateType::vmHalt:
			{
				char				 msgBuff[256];

				_snprintf_s ( msgBuff, sizeof ( msgBuff ), _TRUNCATE, "%s    %s:%s(%I64u): error E%x: %s", activeDebugger->lastMsg.instance->getName (), activeDebugger->lastMsg.location.fName, activeDebugger->lastMsg.location.functionName, activeDebugger->lastMsg.location.line, uint32_t ( activeDebugger->lastMsg.location.err ), scCompErrorAsText ( int ( activeDebugger->lastMsg.location.err ) ).c_str () );
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
			rsp["reason"] = "pause";
			break;
		case vmStateType::vmDebugBreak:
			rsp["reason"] = "breakpoint";
			rsp["hitBreakpointIds"].makeArray ();
			break;
		case vmStateType::vmThreadStart:
			rsp["reason"] = "entry";
			rsp["description"] = "start of execution";
			break;
		default:
			return {"", rsp};
			break;
	}

	rsp["allThreadsStopped"] = true;

	stringi const fixedName = std::filesystem::path ( activeDebugger->lastMsg.location.fName ).generic_string ();

	auto &file = fileStore.getFile ( fixedName );
	int64_t num = 0;
	int64_t index = 0;
	for ( auto &it : file.bpList )
	{
		if ( it.fileName == activeDebugger->lastMsg.location.fName && std::get<int64_t> ( it.location ) == activeDebugger->lastMsg.location.line )
		{
			rsp["hitBreakpointIds"][index] = num + 1;
			index++;
			break;
		}
		num++;
	}

	rsp["threadId"] = reinterpret_cast<int64_t>(lastInstance);

	return {"stopped", rsp};
}
static std::pair<stringi, jsonElement> terminated ( dbgJsonRPCServerBase &server, HWND handle )
{
	return {"terminated", jsonElement ()};
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

	auto svr = new socketServer<dbgJsonRpcServer < socketServer, decltype (debuggerWindowHandle)>, HWND> ( port, std::move ( debuggerWindowHandle ) );		// NOLINT(performance-move-const-arg)  - this is an incorrect warning, we MUST do the move as were doing an lvalue to forwarding ref so need the move

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
			{ "exceptionInfo", exceptionInfo },
			{ "modules", modules },
			{ "loadedSources", loadedSources },
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
