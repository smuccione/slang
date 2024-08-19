// vmTest.cpp : Defines the class behaviors for the application.
//

#include "Utility/settings.h"

#include "externals/externals.h"

#include "stdafx.h"
#include "slangVM.h"

#include "compilerBCGenerator/compilerBCGenerator.h"
#include "compilerPreprocessor/compilerPreprocessor.h"

#include <process.h>

#include "Target/vmTask.h"
#include "Target/vmConf.h"
#include "Target/fileCache.h"
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

	DWORD dwMode = 0;
	if ( GetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), &dwMode ) )
	{
		SetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), dwMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
	}

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
		sprintf_s ( titleString, sizeof ( titleString ), "Slang VM - v%u.%u Build %u", HIWORD ( VerFixedFileInfo->dwFileVersionMS ), LOWORD ( VerFixedFileInfo->dwFileVersionMS ), LOWORD ( VerFixedFileInfo->dwFileVersionLS ) );

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

		SetConsoleMode ( handle, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
	} else
	{
		freopen ("CONIN$", "r", stdin );
	}

	free ( VerInfoData );
}

uint8_t const *getExeResource ( size_t &size )
{
	HGLOBAL     res_handle = NULL;
	HRSRC       res;

	// NOTE: providing g_hInstance is important, NULL might not work
	res = FindResource ( GetModuleHandle ( NULL ), MAKEINTRESOURCE ( SEXE_RESOURCE ), RT_RCDATA );
	if ( !res )
	{
		return (uint8_t const *) "<NONE>";
	}
	res_handle = LoadResource ( NULL, res );
	if ( !res_handle )
	{
		return (uint8_t const *) "<NONE>";
	}
	size = SizeofResource ( NULL, res );
	return (uint8_t const *) LockResource ( res_handle );
}

BOOL __cdecl slangVM( std::vector<char const *> const &fList )
{
	vmTaskInstance instance ( "slangVM" );

	if ( vmConf.useDebugAdapter )
	{
		debugServerInit ( &instance );
	}

	builtinInit ( &instance, builtinFlags::builtin_RegisterOnly );			// this is the INTERFACE

	size_t resSize = 0;
	auto data = getExeResource ( resSize );

	if ( !memcmp ( data, "<NONE>", 6 ) )
	{
		printf ( "internal error\r\n" );
		return FALSE;
	}

	auto dupData = (uint8_t *) malloc ( resSize );
	memcpy ( dupData, data, resSize );
	std::shared_ptr<uint8_t> dup ( dupData, [=]( auto p ) { free ( p ); } );
	instance.load ( dup, __argv[0], true, false );
	instance.run ( "main", false );
	instance.om->processDestructors();
	instance.reset();
 	return TRUE;
}

BOOL CvmTestApp::InitInstance()
{	
#ifdef _MSC_VER
	// fix for https://svn.boost.org/trac/boost/ticket/6320
	boost::filesystem::path::imbue( std::locale( "" ) );
#endif

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls{};
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

//	_CrtSetDbgFlag ( 0 );

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("slangVm"));

	std::vector<char const *>	params;

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

	/* load the engine's configuration settings */
	vmConfig ( std::string("") );

	return slangVM ( params );
}
