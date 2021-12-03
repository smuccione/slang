// slang.cpp : Defines the entry point for the console application.
//

// vmTest.cpp : Defines the class behaviors for the application.
//

#include "Utility/settings.h"

#include "stdafx.h"
#include <process.h>
#include "resource.h"

#include "externals/externals.h"

#include "compilerParser/fileParser.h"
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

#include "Utility/jsonParser.h"

#include "bcDebugger\bcDebugger.h"
#include "compilerPreprocessor\compilerPreprocessor.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <filesystem>

///////////////////////////////////////////////////////////////////////////////
//  include required boost libraries
#include <boost/config.hpp>     //  global configuration information

#include <boost/assert.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/filesystem/config.hpp>

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>    // token class
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp> // lexer class

using namespace boost;

#pragma comment(lib,"Psapi.lib")

static bool isError = false;

fileCache	globalFileCache ( 0, 0 );

uint8_t *getExeResource ( size_t &size )
{
	HGLOBAL     res_handle = NULL;
	HRSRC       res;

	// NOTE: providing g_hInstance is important, NULL might not work
	res = FindResource ( GetModuleHandle ( NULL ), MAKEINTRESOURCE ( SEXE_RESOURCE ), RT_RCDATA );
	if ( !res )
	{
		throw errorNum::scINTERNAL;
	}
	res_handle = LoadResource ( NULL, res );
	if ( !res_handle )
	{
		throw errorNum::scINTERNAL;
	}
	size = SizeofResource ( NULL, res );
	return (uint8_t *) LockResource ( res_handle );
}

int main ( int argc, char *argv[] )
{
	LARGE_INTEGER		 frequency;
	BUFFER				 buff ( 16 * 1024 * 1024 );
	BUFFER				 objBuff ( 16 * 1024 * 1024 );
	uint8_t const		*obj = nullptr;
	uint8_t				*builtInCode = nullptr;
	bool				 doListing = false;
	bool				 dll = false;
	bool				 hasSource = false;
	bool				 exe = false;
	bool				 genDep = false;
	vmTaskInstance		 instance ( "test" );

#ifdef _MSC_VER
	// fix for https://svn.boost.org/trac/boost/ticket/6320
	boost::filesystem::path::imbue ( std::locale ( "" ) );
#endif

	DWORD dwMode = 0;
	if ( GetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), &dwMode ) )
	{
		SetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
	}

	if ( argc < 1 )
	{
		printf ( "slink: <outFile> <inFile...> -list -lib [-prof | -noprof] [-inline | -noinline] [-debug | -nodebug]\r\n" );
		return 0;
	}

	std::filesystem::path p ( argv[1] );
	if ( !p.has_extension () )
	{
		if ( dll )
		{
			p.replace_extension ( ".sll" );
		} else if ( exe )
		{
			p.replace_extension ( ".exe" );
		} else
		{
			p.replace_extension ( ".slx" );
		}
	}

	try
	{
		// parse out any command line parameters
		for ( int loop = 2; loop < argc; loop++ )
		{
			if ( argv[loop][0] == '-' )
			{
				if ( !strccmp ( argv[loop], "-list" ) )
				{
					doListing = true;
				} else if ( !strccmp ( argv[loop], "-prof" ) )
				{
					vmConf.genProfiler = true;
				} else if ( !strccmp ( argv[loop], "-noprof" ) )
				{
					vmConf.genProfiler = false;
				} else if ( !strccmp ( argv[loop], "-inline" ) )
				{
					vmConf.inlineCode = true;
				} else if ( !strccmp ( argv[loop], "-noinline" ) )
				{
					vmConf.inlineCode = false;
				} else if ( !strccmp ( argv[loop], "-debug" ) )
				{
					vmConf.genDebug = true;
					vmConf.inlineCode = false;
				} else if ( !strccmp ( argv[loop], "-nodebug" ) )
				{
					vmConf.genDebug = false;
				} else if ( !strccmp ( argv[loop], "-release" ) )
				{
					vmConf.genDebug = false;
					vmConf.inlineCode = true;
					vmConf.genProfiler = false;
				} else if ( !strccmp ( argv[loop], "-lib" ) )
				{
					dll = true;
				} else if ( !strccmp ( argv[loop], "-exe" ) )
				{
					exe = true;
				} else if ( !strccmp ( argv[loop], "-gendep" ) )
				{
					genDep = true;
				}
			} else
			{
				hasSource = true;
			}
		}

		// compile the built-in code
		{
			auto [builtIn, builtInSize] = builtinInit ( &instance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE

			auto obj = (const char *)builtIn;
			opFile oFile ( &obj );

			oFile.loadLibraries ( false, false );

			compExecutable comp ( &oFile );

			if ( vmConf.genProfiler ) comp.enableProfiling ( );
			if ( vmConf.genDebug ) comp.enableDebugging ( );
			comp.enableDebugging ( );
			comp.listing.level = compLister::listLevel::NONE;
			// build everything
			comp.genCode ( 0 );
			// serialize the output

			comp.serialize ( &buff, true );					// don't include the interface... this is present in builtIn... we handle this differently as it's not a SLL file
			builtInCode = (uint8_t *)buff.detach ();
		}

		QueryPerformanceFrequency ( &frequency );
		frequency.QuadPart = frequency.QuadPart / 1000;

		opFile oCombined;

		obj = builtInCode;
//		oCombined.addStream ( (char const **) &obj, true );
		oCombined.loadLibraryDirect ( (uint8_t const *)obj, dll, false );

		try {
			if ( !hasSource )
			{
				std::filesystem::path libName ( argv[1] );
				libName.replace_extension ( ".slb" );

				auto entry = globalFileCache.read ( libName.string().c_str() );
				if ( !entry ) throw errorNum::scNO_LIBRARY;

				char const *data = (char const *) entry->getData ( );
				oCombined.addStream ( &data, false, false );
				oCombined.addBuildSource(libName.string().c_str());
			}
			std::vector<stringi> sourceFiles;
			for ( int loop = 2; loop < argc; loop++ )
			{
				if ( argv[loop][0] != '-' )
				{
					auto file = std::filesystem::absolute ( argv[loop] );
					oCombined.addBuildSource(file.string().c_str());

					if ( !strccmp ( file.extension().string().c_str(), ".slo" ) )
					{
						auto entry = globalFileCache.read ( argv[loop] );
						if ( !entry ) throw errorNum::scNO_LIBRARY;

						char const *data = (char const *) entry->getData ( );

						oCombined.addStream ( &data, false, false );
						sourceFiles.push_back ( argv[loop] );
					} else if ( !strccmp ( file.extension ().string ().c_str (), ".slb" ) )
					{
						oCombined.loadLibrary ( argv[loop], false, false );
						sourceFiles.push_back ( argv[loop] );
					} else
					{
						printf ( "do not know how to handle input file: %s\r\n", argv[loop] );
						return 1;
					}
				}
			}

			for ( auto &it : instance.preLoadedSource )
			{
				oCombined.loadLibraryDirect ( &*std::get<1> ( it ), true, false );
			}

			for ( auto &it : sourceFiles )
			{
				oCombined.addBuildSource ( it );
			}

			oCombined.loadLibraries ( true, false );
			oCombined.loadModules ( false, false );

			if ( genDep )
			{
				jsonElement	dep;

				printf ( "\"%s\" : [", std::filesystem::absolute ( p ).generic_string ().c_str() );
				bool first = true;
				for ( size_t index = 1; index < oCombined.srcFiles.getCount(); index++ )
				{
					if ( !first )
					{
						printf ( ", " );
					}
					first = false;

					auto file = std::filesystem::absolute ( oCombined.srcFiles.getName ( (uint32_t) index ).c_str () );

					printf ( "\"%s\"", file.generic_string().c_str() );
				}
				printf ( "],\r\n" );
				return 0;
			}

			compExecutable comp ( &oCombined );
			if ( doListing )
			{
				comp.listing.level = compLister::listLevel::LIGHT;
			} else
			{
				comp.listing.level = compLister::listLevel::NONE;
			}
			if ( vmConf.genProfiler ) comp.enableProfiling ( );
			if ( vmConf.genDebug ) comp.enableDebugging ();
			comp.enableDebugging ( );
			try
			{
				comp.genCode ( dll ? 0 : "main" );
			} catch ( errorNum err )
			{
				return int ( err );
			}

			if ( !oCombined.errHandler.isFatal () )
			{
				comp.serialize ( &buff, dll ? true : false );		// emit the interfaces for dll's

				printf ( "compiled file size = %zu\r\n", bufferSize ( &buff ) );
				printf ( "optimization time = %I64u ms\r\n", comp.tOptimizer );
				printf ( "back end time     = %I64u ms\r\n", comp.tBackEnd );

				{
					FILE *outHandle;

					if ( !fopen_s ( &outHandle, p.string ().c_str (), "w+b" ) )
					{
						if ( exe )
						{
							size_t resSize = 0;
							auto data = getExeResource ( resSize );

							fwrite ( data, 1, resSize, outHandle );
							fclose ( outHandle );

							auto handle = BeginUpdateResource ( p.string ().c_str (), false );
							UpdateResourceA ( handle, RT_RCDATA, MAKEINTRESOURCE ( SEXE_RESOURCE ), 0, bufferBuff ( &buff ), (DWORD)bufferSize ( &buff ) );
							EndUpdateResource ( handle, false );
						} else
						{
							fwrite ( bufferBuff ( &buff ), 1, bufferSize ( &buff ), outHandle );
							fclose ( outHandle );
						}
					}

					if ( doListing )
					{
						std::filesystem::path listName = p;
						listName.replace_extension ( ".lst" );
						FILE *lst;
						if ( fopen_s ( &lst, listName.string ().c_str (), "wb" ) )
						{
							printf ( "unable to open list file \"%s\"for writing with error %lu\r\n", listName.string ().c_str (), GetLastError () );
						} else
						{
							comp.listing.printOutput ( lst );
							fclose ( lst );
						}
					}
				}
			} else 
			{
				return 1;
			}
		} catch ( errorNum err )
		{
			oCombined.errHandler.throwFatalError ( false, err );
			return int ( err );
		}
	} catch ( ... )
	{
		return int ( errorNum::scINTERNAL );
	}

	if ( builtInCode ) free ( builtInCode );

	return 0;
}


