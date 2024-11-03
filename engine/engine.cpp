#include "Utility/settings.h"

#include "stdafx.h"
#include "engine.h"

#include "compilerBCGenerator/compilerBCGenerator.h"
#include "compilerPreprocessor/compilerPreprocessor.h"

#include <process.h>

#include "Target/vmTask.h"
#include "Target/vmConf.h"
#include "Target/fileCache.h"
#include "webServer/webServer.h"
#include "webServer/webServerIOCP.h"
#include "webServer/webServerTaskControl.h"
#include "fglDatabase/dbIoServer.h"
#include "compilerParser/fileParser.h"

#include "bcInterpreter/bcInterpreter.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmDebug.h"
#include "bcVM/bcVMObject.h"
#include "version/version.h"
#include "bcVM/bcVMBuiltin.h"
#include "LanguageServer/languageServer.h"
#include "debugAdapter/debugAdapter.h"

//#include "bcDebugger/bcDebugger.h"

#include <openssl/sha.h>

///////////////////////////////////////////////////////////////////////////////
//  include required boost libraries
#include <boost/config.hpp>     //  global configuration information

#include <boost/assert.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/filesystem/config.hpp>

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>    // token class
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp> // lexer class

#include "bcVM/vmBuiltinTusClient.h"

using namespace boost;

#pragma comment(lib,"Psapi.lib")

static bool exiting = false;
std::vector<taskControl	*>		 tc;
char							 titleString[512];
fileCache						 globalFileCache ( 0, 0 );

static struct tcCleanup
{
	~tcCleanup ( )
	{
		for ( auto &it : tc )
		{
			delete it;
		}
	}
} tcCleanup;

CSlangServer::CSlangServer ()
{}

CSlangServer theApp;

BEGIN_MESSAGE_MAP ( CSlangServer, CWinApp )
	ON_COMMAND ( ID_HELP, &ThisClass::OnHelp )
END_MESSAGE_MAP ()

class handleTaskControl : public taskControl
{
	HANDLE handle;
	HANDLE shutdown;
	public:
	handleTaskControl ( HANDLE threadHandle, HANDLE shutdown ) : handle ( threadHandle ), shutdown ( shutdown )
	{};
	void end ( void )
	{
		exiting = true;
		if ( shutdown ) CloseHandle ( shutdown );
		WaitForSingleObject ( handle, INFINITE );
	}
};

UINT __cdecl fileWatchChangeThread ( LPVOID param )
{
	HANDLE						 handle = (HANDLE) param;
	BOOL						 result;
	DWORD						 nBytesTransferred;
	ULONG_PTR					 key;
	vmDirectoryWatch			*watch = nullptr;
	vmTaskInstance				 instance ( "testWorker" );

	for ( auto it = vmConf.watch.begin(); it != vmConf.watch.end(); it++ )
	{
		// associate it
		CreateIoCompletionPort ( it->dirHandle, handle, 0, 1 );

		ReadDirectoryChangesW ( it->dirHandle,
								it->dat.data,
								sizeof ( it->dat.data ),
								TRUE,
								FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE,
								&nBytesTransferred,
								&(it->ov),
								0
		);
	}

	while ( 1 )
	{
		result = GetQueuedCompletionStatus ( handle, &nBytesTransferred, &key, (OVERLAPPED **) &watch, INFINITE );

		if ( result )
		{
			// reinitialize for next watch change
			memset ( &(watch->ov), 0, sizeof ( watch->ov ) );
			// register for the next change notification
			ReadDirectoryChangesW ( watch->dirHandle,
									watch->dat.data,
									sizeof ( watch->dat.data ),
									TRUE,
									FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE,
									&nBytesTransferred,
									&(watch->ov),
									0
								  );

			FILE_NOTIFY_INFORMATION *fi = &watch->dat.fi;
			while ( fi )
			{
				char const *action;
				switch ( fi->Action )
				{
					case FILE_ACTION_RENAMED_NEW_NAME:
					case FILE_ACTION_ADDED:
					case FILE_ACTION_MODIFIED:
					case FILE_ACTION_REMOVED:
						{
							std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

							switch ( fi->Action )
							{
								case FILE_ACTION_RENAMED_NEW_NAME:
									action = "RENAME";
									break;
								case FILE_ACTION_ADDED:
									action = "ADDED";
									break;
								case FILE_ACTION_MODIFIED:
									action = "MODIFIED";
									break;
								case FILE_ACTION_REMOVED:
									action = "REMOVED";
									break;
								default:
									action = nullptr;
									break;
							}

							if ( action )
							{
								// execute page
								std::string u = watch->url + "?fName=" + converter.to_bytes ( std::wstring ( fi->FileName, fi->FileNameLength / sizeof ( WCHAR ) ) ) + "&action=" + action;

								auto uri = Uri::Parse ( converter.from_bytes ( u ) );

								std::string domain = converter.to_bytes ( uri.Host );
								std::string url = converter.to_bytes ( uri.Req );

								instance.stack = instance.eval + 1;
								instance.om->reset ();
								webSend ( &instance, VAR_STR ( "GET" ), VAR_STR ( domain.c_str () ), VAR_STR ( url.c_str () ), uri.Port, VAR_STR ( "" ), VAR_STR ( "" ), uri.isSecure );

								if ( fi->NextEntryOffset )
								{
									fi = (FILE_NOTIFY_INFORMATION *)(((char *)fi) + fi->NextEntryOffset);
								} else
								{
									fi = 0;
								}
							}
						}
						break;
					default:
						break;
				}
			}
		} else
		{
			return 0;
		}
	}
}

UINT __cdecl testCompile ( LPVOID param )
{
	LARGE_INTEGER		 start;
	LARGE_INTEGER		 end;
	LARGE_INTEGER		 frequency;
	BUFFER				 buff ( 1024ULL * 1024 * 4 );
	BUFFER				 objBuff ( 1024ULL * 1024 * 4 );
	bool				 doOutputString = false;
	bool				 doDebug = false;
	bool				 doProfiling = false;
	std::vector<char*>	 fList;

	vmTaskInstance		 instance ( "testWorker" );

	if ( vmConf.useDebugAdapter )
	{
		debugServerInit ( &instance );
	} else
	{
#if 0
		debuggerInit ( &instance, true );
		if ( !vmConf.debuggerEnabled )
		{
			instance.debug->Disable ();
		}
#else
		std::shared_ptr<vmDebug> debugger ( static_cast<vmDebug *>(new nullDebugger ()) );
		debugRegister ( &instance, debugger );
#endif
	}

	instance.duplicate ( (vmTaskInstance *) param );

	WIN32_FIND_DATA	findData;
	HANDLE	findHandle;

	if ( (findHandle = FindFirstFile ( "*.sl", &findData )) != INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		while ( 1 )
		{
			fList.push_back ( _strdup ( findData.cFileName ) );
			if ( !FindNextFile ( findHandle, &findData ) )
			{
				break;
			}
		}
	}

	//	fList.clear();
	//	fList.push_back( "linq_from_let_select.fgl" );

	Sleep ( 2000 );
	uint32_t ctr = 1000000;
	for ( ; !exiting && --ctr; )
	{
		for ( auto fIt = fList.begin (); fIt != fList.end () && !exiting; fIt++ )
		{
			QueryPerformanceFrequency ( &frequency );
			frequency.QuadPart = frequency.QuadPart / 1000;

			//	SetPriorityClass ( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
			//	SetThreadPriority ( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

			opFile oCombined;

			fileCacheEntry *entry = globalFileCache.read ( *fIt );
			if ( !entry )
			{
				instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "unable to open file: %s", *fIt );
				continue;
			}

			instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "file: %s\r\n", (*fIt) );

			QueryPerformanceCounter ( &start );

			BUFFER b;

			b.write ( entry->getData ( ), entry->getDataLen ( ) );
			b.put ( 0 );	// need to do this to ensure null termination

			auto code = compPreprocessor ( (*fIt), b.data<char *> ( ), true );

			QueryPerformanceCounter ( &end );

			instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "preprocess time = %I64u ms\r\n", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

			opFile	oFile;

			QueryPerformanceCounter ( &start );
			{
				oFile.parseFile ( (*fIt), code, true, false );
				if ( oFile.errHandler.isFatal () )
				{
					return 0;
				}
			}
			QueryPerformanceCounter ( &end );

			free ( code );

			instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "front end time = %I64u ms\r\n", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

			objBuff.reset();
			oFile.serialize ( &objBuff, false );

			instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "object file size = %zu\r\n", bufferSize ( &objBuff ) );

			auto obj = (uint8_t const *) bufferBuff ( &objBuff );

			oCombined.addBuildSource ( *fIt );

			oCombined.addStream ( (const char **) &obj, false, false );

			// load all the preloaded interfaces
			for ( auto &it : ((vmTaskInstance *)param)->preLoadedSource )
			{
				oCombined.loadLibraryDirect ( &*std::get<1> ( it ), true, false );
			}

			if ( oCombined.errHandler.isFatal () )
			{
				return 0;
			}

			compExecutable comp ( &oCombined );
			if ( doProfiling ) comp.enableProfiling ();
			if ( doDebug ) comp.enableDebugging ();
			try
			{
				comp.genCode ( "main" );
			} catch ( errorNum err )
			{
				oCombined.errHandler.throwFatalError ( false, err );
			}

			if ( oCombined.errHandler.isFatal () )
			{
				return 0;
			}

			buff.reset();
			comp.serialize ( &buff, false );

			instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "compiled file size = %zu\r\n", bufferSize ( &buff ) );
			instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "optimization time = %I64u ms\r\n", comp.tOptimizer );
			instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "back end time     = %I64u ms\r\n", comp.tBackEnd );

			if ( !exiting )
			{
				uint8_t *userCode = (uint8_t *)bufferDetach( &buff );

				for ( uint32_t loop = 0; loop < 1; loop++ )
				{
					QueryPerformanceCounter( &start );
					{
						// load the built-in code (if any.. includes any run-time evaluated default parameters
						std::shared_ptr<uint8_t> dup( (uint8_t *)userCode, [=]( auto p ) { free( p ); } );
						instance.load( dup, "userCode", false, false );
					}
					QueryPerformanceCounter( &end );

					instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "load time time = %I64u ms\r\n\r\n", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

					QueryPerformanceCounter( &start );
					{
						instance.run( "main", doDebug );
					}
					QueryPerformanceCounter( &end );


					if ( doOutputString )
					{
						instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "%.*s\r\n", (int)classIVarAccess( instance.getGlobal( "userCode", "__outputString" ), "size" )->dat.l, classIVarAccess( instance.getGlobal( "userCode", "__outputString" ), "buff" )->dat.str.c );
					}

					instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "run time = %I64u ms\r\n", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

					instance.om->processDestructors();
					instance.reset();
				}
			}
		}
	}

	return 1;
}

static void CreateConsole ( char const *appName )

{
	COORD				 coord = {};
	DWORD				 dummy;
	char				*VerInfoData;
	int					 VerInfoSize;
	unsigned int		 VerInfoBlockLen;
	VS_FIXEDFILEINFO	*VerFixedFileInfo = nullptr;
	SMALL_RECT DisplayArea = {0, 0, 0, 0};

	TCHAR szExeName[MAX_NAME_SZ];

	::GetModuleFileName ( AfxGetInstanceHandle (), szExeName, MAX_NAME_SZ );

	VerInfoSize = GetFileVersionInfoSize ( szExeName, &dummy );
	VerInfoData = (char *) malloc ( sizeof ( char ) * VerInfoSize );
	GetFileVersionInfo ( szExeName, 0, VerInfoSize, VerInfoData );
	VerQueryValue ( VerInfoData, "\\", (LPVOID *) &VerFixedFileInfo, &VerInfoBlockLen );

	if ( !freopen ( "CONOUT$", "w", stdout ) )
	{
		/* create a seperate console window for logger (print, println and debug) output */
		AllocConsole ();

		/* get the new console's handle */
		auto handle = GetStdHandle ( STD_OUTPUT_HANDLE );

		/* give it a title */
#if _DEBUG
		sprintf_s ( titleString, sizeof ( titleString ), "Slang MultiServer - v%u.%u.%s %s %s - DEBUG", HIWORD ( VerFixedFileInfo->dwFileVersionMS ), LOWORD ( VerFixedFileInfo->dwFileVersionLS ),	VCS_SHORT_HASH, __TIME__, VCS_WC_MODIFIED ? " - NON-COMMITTED" : "" );
#else
		sprintf_s ( titleString, sizeof ( titleString ), "Slang MultiServer - v%u.%u.%u %s %s", HIWORD (VerFixedFileInfo->dwFileVersionMS), LOWORD (VerFixedFileInfo->dwFileVersionLS), VCS_SHORT_HASH, __TIME__, VCS_WC_MODIFIED ? " - NON-COMMITTED" : "");
#endif

		SetConsoleTitle ( titleString );

		coord.X = 500;
		coord.Y = 9999;
		//		coord.Y = 256;

		SetConsoleScreenBufferSize ( handle, coord );

		DisplayArea.Right = 180;
		DisplayArea.Bottom = 70;

		SetConsoleWindowInfo ( handle, TRUE, &DisplayArea );

		/* re-direct all i/o operations to the new console */
		freopen ( "CONOUT$", "w", stderr );
		freopen ( "CONOUT$", "w", stdout );

		SetConsoleMode ( handle, ENABLE_PROCESSED_OUTPUT );
	} else
	{
		freopen ( "CONIN$", "r", stdin );
	}

	DWORD dwMode = 0;
	if ( GetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), &dwMode ) )
	{
		SetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
	}

	free ( VerInfoData );
}

BOOL CtrlHandler ( DWORD fdwCtrlType )
{
	switch ( fdwCtrlType )
	{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			SetEvent ( theApp.shutDown );
			return TRUE;
		default:
			return FALSE;
	}
}

void CSlangServer::appServerStart ( void )
{
	vmTaskInstance		 instance	( "workerThread" );
	vmTaskInstance		 dbInstance ( "dbWorkerThread" );
	BUFFER				 buff ( 1024ULL * 1024 );
	std::string			 engineConf = "engine.ini";

	CreateConsole ( "test" );

	auto [builtIn, builtInSize] = builtinInit ( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE

	/* load the engine's configuration settings */
	vmConfig ( engineConf );

	// initialize the fiel  cache
	globalFileCache.setSize ( vmConf.vmCacheMaxSize, vmConf.vmCacheLargeFileSize );

	// initialize TUS client... must be done AFTER global cache initialization
//	sendMgr.init ( vmConf.tusSendFileStorage );

	std::shared_ptr<uint8_t> builtInShared ( (uint8_t *) builtIn, [=]( auto p ) { free ( p ); } );

	instance.preLoadedSource.push_back ( std::make_tuple ( builtInSize, builtInShared ) );

	instance.load ( builtInShared, "builtIn", false, false );

	// construct the instance for the database... this instance has the builtin functions compiled and loaded, as opposed to the worker thread instance which has the compiled portion
	// built into the individual ap pages to allow inlining, speed, and pruning

	dbInstance.preLoadedSource.push_back ( std::make_tuple ( builtInSize, builtInShared ) );

	opFile oFile;

	for ( auto &it : dbInstance.preLoadedSource )
	{
		auto obj = (char const *) std::get<1> ( it ).get ();

		oFile.addStream ( &obj, false, false );
	}

	compExecutable comp ( &oFile );
	comp.genCode ( nullptr );
	BUFFER compBuff ( 16ULL * 1024 * 1024 );
	comp.serialize ( &compBuff, false );

	size_t len = compBuff.size ();
	uint8_t *ptr = compBuff.detach<uint8_t> ();

	std::shared_ptr<uint8_t> dbDup ( (uint8_t *) malloc ( len ), [&]( auto p ) { free ( p ); } );
	memcpy ( &*dbDup, ptr, len );
	dbInstance.preLoadedCode.push_back ( { len, dbDup } );

	// create the shutdown event... this is used by ctr-c and windows close to signal shutdown requested
	shutDown = CreateEvent ( 0, 0, 0, 0 );

	taskControl *webServerTC = 0;
	taskControl *dbServerTC = 0;

	SSL_library_init ();
	OpenSSL_add_all_algorithms ();
	SSL_load_error_strings ();

	for ( auto &it : vmConf.servers )
	{
		switch ( it.type )
		{
			case vmServers::vmServerType::http:
				if ( !webServerTC )
				{
					// start actually starts the worker threads and createst the async handler
					webServerTC = webServerStart ( vmConf.nServerThreads, &instance );
					tc.push_back ( webServerTC );
				}
				// this is the actually intialization for the http server
				webServerInit ( static_cast<class webServerTaskControl *>(webServerTC), it.iniFile.c_str ( ), &instance );
				break;
			case vmServers::vmServerType::https:
				if ( !webServerTC )
				{
					// start actually starts the worker threads and createst the async handler
					webServerTC = webServerStart ( vmConf.nServerThreads, &instance );
					tc.push_back ( webServerTC );
				}
				// this is the actually intialization for the https server
				webServerSSLInit ( static_cast<class webServerTaskControl *>(webServerTC), it.iniFile.c_str(), &instance );
				break;
			case vmServers::vmServerType::db:
				if ( !dbServerTC )
				{
					dbServerTC = dbServerStart ( vmConf.dbServerThreads, &dbInstance );
					tc.push_back ( dbServerTC );
				}
	
				dbServerInit ( static_cast<taskControl *>(dbServerTC), it.iniFile.c_str ( ), &dbInstance );
				break;
			case vmServers::vmServerType::language:
				if ( !dbServerTC )
				{
					tc.push_back ( startLanguageServer ( atoi ( it.iniFile.c_str () ) ) );
				}
				break;
			case vmServers::vmServerType::debug:
				if ( !dbServerTC )
				{
					tc.push_back ( debugAdapterInit ( atoi ( it.iniFile.c_str() ) ) );
				}
				break;
			default:
				instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "unsupported server type: %i\r\n", it.type );
				break;
		}
	}

#if 0
	if ( vmConf.useDebugAdapter )
	{
		debugServerInit ( &instance );
	} else
	{
		debuggerInit ( &instance, true );
		if ( !vmConf.debuggerEnabled )
		{
			instance.debug->Disable ();
		}
	}
#endif

	if ( vmConf.watch.size ( ) )
	{
		HANDLE handle = CreateIoCompletionPort ( INVALID_HANDLE_VALUE, 0, 0, 1 );	// NOLINT (performance-no-int-to-ptr)
		
		std::thread th ( fileWatchChangeThread, handle );
		tc.push_back ( new handleTaskControl ( handle, th.native_handle() ) );
		th.detach ( );
	}

	instance.log ( logger::level::INFO, logger::modules::SYSTEM, "SLANG server ready\r\n" );

	SetConsoleCtrlHandler ( (PHANDLER_ROUTINE) CtrlHandler, TRUE );

	WaitForSingleObject ( shutDown, INFINITE );

	instance.log ( logger::level::INFO, logger::modules::SYSTEM, "shutting down...\r\n" );
	exiting = true;

	for ( auto it : tc )
	{
		it->end ();
	}
}

void CSlangServer::testServerStart ( void )
{
	vmTaskInstance		 instance ( "workerThread" );
	BUFFER				 buff ( 1024ULL * 1024 * 4 );

	CreateConsole ( "test" );

	if ( vmConf.useDebugAdapter )
	{
		debugServerInit ( &instance );
	} else
	{
#if 0
		debuggerInit ( &instance, true );
		if ( !vmConf.debuggerEnabled )
		{
			instance.debug->Disable ();
		}
#else
		std::shared_ptr<vmDebug> debugger ( static_cast<vmDebug *>(new nullDebugger ()) );
		debugRegister ( &instance, debugger );
#endif
	}

	auto builtIn = builtinInit ( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE

	auto obj = builtIn;
	opFile oFile ( (char const **) &obj );
	compExecutable comp ( &oFile );

	if ( vmConf.genDebug )
	{
		comp.enableDebugging ( );
	}
	if ( vmConf.genProfiler )
	{
		comp.enableProfiling ( );
	}

	// build everything
	comp.genCode ( 0 );
	// serialize the output
	comp.serialize ( &buff, true );
	auto builtInCode = (char *) bufferDetach ( &buff );							// this is the code library

	/* load the engine's configuration settings */
	vmConfig ( engineConf );

	// initialize the file  cache
	globalFileCache.setSize ( vmConf.vmCacheMaxSize, vmConf.vmCacheLargeFileSize );

	// initialize TUS client... must be done AFTER global cache initialization
//	sendMgr.init ( vmConf.tusSendFileStorage );

	std::shared_ptr<uint8_t> dup ( (uint8_t*) builtInCode, [=]( auto p ) { free ( p ); } );
	instance.load ( dup, "builtIn", false, false );

	std::thread th ( testCompile, (LPVOID) &instance );
	tc.push_back ( new handleTaskControl ( th.native_handle(), (HANDLE) 0 ) );
	th.detach ( );

	shutDown = CreateEvent ( 0, 0, 0, 0 );

	taskControl *webServerTC = 0;
	taskControl *dbServerTC = 0;

	SSL_library_init ();
	OpenSSL_add_all_algorithms ();
	SSL_load_error_strings ();

	for ( auto &it : vmConf.servers )
	{
		switch ( it.type )
		{
			case vmServers::vmServerType::http:
				if ( !webServerTC )
				{
					webServerTC = webServerStart ( vmConf.nServerThreads, &instance );
					tc.push_back ( webServerTC );
				}
				webServerInit ( static_cast<class webServerTaskControl *>(webServerTC), it.iniFile.c_str ( ), &instance );
				break;
			case vmServers::vmServerType::https:
				if ( !webServerTC )
				{
					webServerTC = webServerStart ( vmConf.nServerThreads, &instance );
					tc.push_back ( webServerTC );
				}
				webServerSSLInit ( static_cast<class webServerTaskControl *>(webServerTC), it.iniFile.c_str ( ), &instance );
				break;
			case vmServers::vmServerType::db:
				if ( !dbServerTC )
				{
					dbServerTC = dbServerStart ( vmConf.nServerThreads, &instance );
					tc.push_back ( dbServerTC );
				}
				dbServerInit ( static_cast<taskControl *>(dbServerTC), it.iniFile.c_str ( ), &instance );
				break;
			default:
				break;
		}
	}

	instance.log ( logger::level::INFO, logger::modules::SYSTEM, "server ready\r\n" );

	SetConsoleCtrlHandler ( (PHANDLER_ROUTINE) CtrlHandler, TRUE );

	WaitForSingleObject ( shutDown, INFINITE );

	instance.log ( logger::level::INFO, logger::modules::SYSTEM, "shutting down...\r\n" );
	exiting = true;

	for ( auto it : tc )
	{
		it->end ( );
	}
}

BOOL CSlangServer::InitInstance ( )
{
	WSADATA				 wsaData;
	std::vector<char *>	 params;
	size_t				 loop;
	bool				 doTest = false;
	bool				 doExec = true;

//	_CrtSetDbgFlag ( _CRTDBG_CHECK_ALWAYS_DF );

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls = {};
	InitCtrls.dwSize = sizeof ( InitCtrls );
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx ( &InitCtrls );

	CWinApp::InitInstance ( );

//	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	if ( !AfxSocketInit () )
	{
		AfxMessageBox ( IDP_SOCKETS_INIT_FAILED );
		return FALSE;
	}

	AfxEnableControlContainer ();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey ( _T ( "slangVm" ) );

	WSAStartup ( MAKEWORD ( 2, 2 ), &wsaData );

#ifdef _MSC_VER
	// fix for https://svn.boost.org/trac/boost/ticket/6320
	boost::filesystem::path::imbue ( std::locale ( "" ) );
#endif

	for ( int loop = 1; loop < __argc; loop++ )
	{
		params.push_back ( __argv[loop] );
	}

	for ( loop = 0; loop < params.size (); loop++ )
	{
		if ( params[loop][0] == '-' )
		{
			if ( !strccmp ( params[loop], "--test" ) )
			{
				doTest = true;
			} else if ( !strccmp ( params[loop], "-conf" ) )
			{
				loop++;
				if ( loop < params.size () )
				{
					engineConf = params[loop];
				} else
				{
					printf ( "error: -conf <fileName>\r\n" );
					return 0;
				}
			} else if ( !strccmp ( params[loop], "--register" ) )
			{
				RegisterAsService ();
				doExec = false;
			} else if ( !strccmp ( params[loop], "--deregister" ) || !strccmp( params[loop], "--unregister" ) )
			{
				DeRegisterAsService ();
				loop++;
				doExec = false;
			} else if ( !strccmp ( params[loop], "--service" ) )
			{
				SetCurrentDirectory ( params[loop + 1] );
				InstallAPPService ();
				doExec = false;
			}
		}
	}

	if ( doExec )
	{
		if ( doTest )
		{
			testServerStart ();
		} else
		{
			if ( CheckService () )
			{
				appServerStart ( );
			}
		}
	}
	return 0;
}
