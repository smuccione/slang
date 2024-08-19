// vmTest.cpp : Defines the class behaviors for the application.
//

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "stdafx.h"
#include <windows.h>
#include <process.h>

#include <filesystem>
#include <thread>
#include <memory>

#include "externals/externals.h"

#include "compilerTest.h"

#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmInstance.h"
#include "bcVM/vmDebug.h"
#include "bcVM/bcVMObject.h"

#include "Target/common.h"
#include "Target/vmConf.h"
#include "Target/vmTask.h"
#include "Target/fileCache.h"

#include "version/version.h"

#include "bcDebugger\bcDebugger.h"
#include "../debugAdapter/debugAdapter.h"
#include "compilerPreprocessor\compilerPreprocessor.h"

///////////////////////////////////////////////////////////////////////////////
//  include required boost libraries
#define BOOST_LIB_DIAGNOSTIC

#include <boost/config.hpp>     //  global configuration information

#include <boost/assert.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/filesystem/config.hpp>

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>    // token class
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp> // lexer class

using namespace boost;

#pragma comment(lib,"Psapi.lib")

char titleString[512];

CvmTestApp::CvmTestApp()
{
}

CvmTestApp theApp;

BEGIN_MESSAGE_MAP(CvmTestApp, CWinApp)
	ON_COMMAND(ID_HELP, &ThisClass::OnHelp)
END_MESSAGE_MAP()

fileCache	globalFileCache ( 0, 0 );

static void CreateConsole ( char const *appName )
{
	HANDLE				 handle;
	COORD				 coord{};
	DWORD				 dummy;
	char				*VerInfoData;
	int					 VerInfoSize;
	unsigned int		 VerInfoBlockLen;
	VS_FIXEDFILEINFO	*VerFixedFileInfo = nullptr;
	SMALL_RECT DisplayArea = {0, 0, 0, 0};

	TCHAR szExeName[MAX_NAME_SZ];

	::GetModuleFileName(AfxGetInstanceHandle(), szExeName, MAX_NAME_SZ);

	VerInfoSize = GetFileVersionInfoSize ( szExeName, &dummy );
	VerInfoData = (char *)malloc ( sizeof ( char ) * VerInfoSize );
	GetFileVersionInfo ( szExeName, 0, VerInfoSize, VerInfoData );
	VerQueryValue ( VerInfoData, "\\", (LPVOID *) &VerFixedFileInfo, &VerInfoBlockLen );

	if ( !freopen ("CONOUT$", "w", stdout ) )
	{
		/* create a seperate console window for printf (print, println and debug) output */
		AllocConsole();

		/* get the new console's handle */
		handle = GetStdHandle ( STD_OUTPUT_HANDLE );

		/* give it a title */
		sprintf_s ( titleString, sizeof ( titleString ), "Slang VM Test - v%u.%u Build %u", HIWORD ( VerFixedFileInfo->dwFileVersionMS ), LOWORD ( VerFixedFileInfo->dwFileVersionMS ), LOWORD ( VerFixedFileInfo->dwFileVersionLS ) );

		SetConsoleTitle ( titleString );

		coord.X = 500;

		coord.Y = 9999;
//		coord.Y = 256;

		SetConsoleScreenBufferSize ( handle, coord );

		DisplayArea.Right = 180;
		DisplayArea.Bottom = 70;

		SetConsoleWindowInfo (handle, TRUE, &DisplayArea);

		/* re-direct all i/o operations to the new console */
		freopen ("CONOUT$", "w", stderr );
		freopen ("CONOUT$", "w", stdout );

		DWORD dwMode = 0;
		GetConsoleMode ( handle, &dwMode );
		SetConsoleMode ( handle, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
	} else
	{
		freopen ("CONIN$", "r", stdin );
	}

	free ( VerInfoData );
}

UINT __cdecl testCompile( std::vector<char const *> const &fList )
{
	LARGE_INTEGER		 start;
	LARGE_INTEGER		 end;
	LARGE_INTEGER		 frequency;
	BUFFER				 buff ( 1024ull * 1024 * 4 );
	BUFFER				 objBuff ( 1024ull * 1024 * 4 );
	char				*code;
	DWORD				 loop;
	bool				 isAP;
	HANDLE				 pipeHandle;
	bool				 doOutputString = false;
	bool				 doDebug = false;
	bool				 doListing = false;
	bool				 doProfiling = false;
	bool				 doBraces = true;
	bool				 printOmStats = false;
	bool				 doTest = false;
	bool				 noRun = false;

	vmTaskInstance instance ( "test" );

	// parse out any command line parameters
	for ( loop = 0; loop < fList.size(); loop++ )
	{
		if ( fList[loop][0] == '-' )
		{
			if ( !strccmp ( fList[loop], "-debug" ) )
			{
				if ( !doDebug )
				{
					doDebug = true;
					vmConf.genDebug = true;
					vmConf.inlineCode = false;
					debuggerInit ( &instance, true );
				}
			} else if ( !strccmp ( fList[loop], "-list" ) )
			{
				doListing = true;
			} else if ( !strccmp ( fList[loop], "-prof" ) )
			{
				doProfiling = true;
				vmConf.genProfiler = true;
			} else if ( !strccmp( fList[loop], "-fgl" ) )
			{
				doBraces = false;
			} else if ( !strccmp( fList[loop], "-test" ) )
			{
				doTest = true;
			} else if ( !strccmp( fList[loop], "-omStats" ) )
			{
				printOmStats = true;
			} else if ( !strccmp ( fList[loop], "-inline" ) )
			{
				vmConf.inlineCode = true;
			} else if ( !strccmp ( fList[loop], "-noinline" ) )
			{
				vmConf.inlineCode = false;
			} else if ( !strccmp ( fList[loop], "-norun" ) )
			{
				noRun = true;
			}
	 	}
	}

	if ( doDebug )
	{
		doTest = false;
	}

	if ( !doDebug )
	{
		std::shared_ptr<vmDebug> debugger ( static_cast<vmDebug *>(new nullDebugger()) );
		debugRegister ( &instance, debugger );
	}

	if ( doTest )
	{
		doListing = false;
		uint64_t ts = GetTickCount64();
		std::string name = (std::string( "\\\\.\\pipe\\StdOutPipe" ) + std::to_string( ts ));

		pipeHandle = CreateNamedPipe( name.c_str(), PIPE_ACCESS_INBOUND, PIPE_READMODE_BYTE, 2, 1024 * 1024, 1024 * 1024, 0, NULL );

		freopen( name.c_str(), "a", stdout );

		ConnectNamedPipe( pipeHandle, 0 );
	}

	// compile the built-in code
	auto [builtIn, builtInSize] = builtinInit ( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE

	QueryPerformanceFrequency ( &frequency );
	frequency.QuadPart = frequency.QuadPart / 1000;

//	SetPriorityClass ( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
//	SetThreadPriority ( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

	opFile oCombined;

	isAP = false;

	for ( loop = 0; loop < fList.size(); loop++ )
	{
		if ( fList[loop][0] == '-' )
		{
			if ( !strccmp( fList[loop], "-ap" ) )
			{
				isAP = true;
				doOutputString = true;
			} else if ( !strccmp( fList[loop], "-braces" ) )
			{
				doBraces = true;
			}
		}
	}
	for ( loop = 0; loop < fList.size(); loop++ )
	{
		if ( fList[loop][0] != '-' )
		{
			stringi file = std::filesystem::absolute ( fList[loop] ).string();

			if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "file: %s", file.c_str() );

			QueryPerformanceCounter ( &start );
			{
				if ( isAP )
				{
					code = compAPPreprocessor ( file.c_str ( ), doBraces, true );
				} else
				{
					auto entry = globalFileCache.read ( file.c_str ( ) );

					if ( !entry )
					{
						instance.log ( logger::level::ERROR, logger::modules::SYSTEM, "reading file: %s", file.c_str () );
						return 0;
					}
					BUFFER b;

					b.write ( entry->getData ( ), entry->getDataLen ( ) );
					b.put ( 0 );	// need to do this to ensure null termination

					code = compPreprocessor ( file.c_str ( ), b.data<char *>(), doBraces );
				}
			}
			QueryPerformanceCounter ( &end );

			if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "preprocess time = %I64u ms", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

			opFile	oFile;

			QueryPerformanceCounter ( &start );
			{
				oFile.parseFile ( file.c_str ( ), code, doBraces, false );
				if ( oFile.errHandler.isFatal() )
				{
					return 0;
				}
			}
			QueryPerformanceCounter ( &end );

			free ( code );
			
			if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "front end time = %I64u ms", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

			objBuff.reset();
			oFile.addBuildSource ( file );
			oFile.serialize ( &objBuff, false );

			if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "object file size = %zu", bufferSize ( &objBuff ) );

			// code being built
			auto obj = (uint8_t const *) bufferBuff ( &objBuff );
			oCombined.addStream ( (const char **) &obj, false, false );

			isAP = false;
		}
	}

	char const *obj = (char const *)builtIn;
	oCombined.addStream (  &obj, false, false );

	if ( oCombined.errHandler.isFatal() )
	{
		return 0;
	}

	try {
		oCombined.loadLibraries ( false, false );
	} catch ( ... )
	{
		printf ( "****** error loading libraries\r\n" );
		return 0;
	}

	compExecutable comp ( &oCombined );
	if ( doListing )
	{
		comp.listing.level = compLister::listLevel::LIGHT;
	}
	if ( doProfiling ) comp.enableProfiling();
	if ( doDebug ) comp.enableDebugging();
	try {
		comp.genCode( noRun ? 0 : "main" );
	} catch ( errorNum err )
	{
		oCombined.errHandler.throwFatalError ( false, err );
	}

	if ( oCombined.errHandler.isFatal() )
	{
		return 0;
	}

	comp.serialize ( &buff, false );

	if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "compiled file size = %zu", bufferSize ( &buff ) );
	if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "optimization time = %I64u ms", comp.tOptimizer);
	if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "back end time     = %I64u ms", comp.tBackEnd );

	if ( !doTest ) comp.listing.printOutput ( );

	QueryPerformanceCounter ( &start );
	{
		std::shared_ptr<uint8_t> dup2 ( (uint8_t *) bufferDetach ( &buff ), [=]( auto p ) { free ( p ); } );
		instance.load ( dup2, "usercode", false, false );
	}

	QueryPerformanceCounter ( &end );
	
	if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "load time = %I64u ms", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

	QueryPerformanceCounter ( &start );
	{
		try
		{
			if ( !noRun ) instance.run ( "main", doDebug );
		} catch ( ... )
		{
			instance.log ( logger::level::INFO, logger::modules::SYSTEM, "****** error during execution" );
		}
	}
	QueryPerformanceCounter ( &end );

	if ( doOutputString  )
	{
		printf ( "%.*s\r\n", (int)classIVarAccess ( instance.getGlobal ( "userCode", "__outputString" ), "size" )->dat.l, classIVarAccess ( instance.getGlobal ( "userCode", "__outputString" ), "buff" )->dat.str.c );
	}

	if ( !doTest ) instance.log ( logger::level::INFO, logger::modules::SYSTEM, "run time = %I64u ms", (end.QuadPart - start.QuadPart) / frequency.QuadPart );

	if ( printOmStats )
	{
		instance.om->printStats();
	}

	instance.om->processDestructors();
	instance.reset();

	if ( doTest )
	{
		fflush( stdout );
		freopen( "CONOUT$", "w", stdout );

		while ( 1 )
		{
			char inp[1024] = {};
			DWORD nRead;
			ReadFile( pipeHandle, inp, sizeof( inp), &nRead, 0 );
			if ( !nRead )
			{
				break;
			}
			printf( "%.*s", (int)nRead, inp );
		}
		CloseHandle( pipeHandle );
	}

 	return 1;
}

BOOL CvmTestApp::InitInstance()
{	
	size_t	loop;
	size_t	numInputFiles;
	bool	doDebug = false;
	
#ifdef _MSC_VER
	// fix for https://svn.boost.org/trac/boost/ticket/6320
	boost::filesystem::path::imbue( std::locale( "" ) );
#endif

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls = {};
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	_CrtSetDbgFlag ( 0 );

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("slangVm"));

	std::vector<char const *>	params;

	if ( !__argc )
	{
		printf ( "compilerTest <[-ap] inputFiles..> -debug -list\r\n" );
		return false;
	}
	numInputFiles = 0;
	for ( int loop = 1; loop < __argc; loop++ )
	{
		if ( __argv[loop][0] != '-' )
		{
			numInputFiles++;
		}
		params.push_back ( __argv[loop] );
	}

	for ( loop = 0; loop < params.size(); loop++ )
	{
		if ( params[loop][0] == '-' )
		{
			if ( !strccmp ( params[loop], "-debug" ) )
			{
				doDebug = true;
				vmConf.genDebug = true;
			}
		}
	}

	if ( !numInputFiles )
	{
		printf ( "slang compiler test utility\r\n" );
		printf ( "%s\r\n", Copyright.c_str () );
		printf ( "usage: %s <inputFiles...> --debug --list\r\n", __argv[0] );
	}

	if ( doDebug )
	{
		CreateConsole ( "test" );
	} else
	{
		if ( !AttachConsole ( ATTACH_PARENT_PROCESS ) )
		{
			CreateConsole ( "test" );
		} else
		{
			/* re-direct all i/o operations to the new console */
			freopen ("CONOUT$", "w", stderr );
			freopen ("CONOUT$", "w", stdout );
			freopen ("CONIN$", "r", stdin );
		}
	}

	/* load the engine's configuration settings */
	vmConfig ( std::string("") );

// 	vmConf.vmCollector = colCopy;

	std::thread ( testCompile, params ).join ( );

	return FALSE;
}

