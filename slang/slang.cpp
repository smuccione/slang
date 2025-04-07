// slang.cpp : Defines the entry point for the console application.
//

// vmTest.cpp : Defines the class behaviors for the application.
//


#include "Utility/settings.h"

#include "stdafx.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>

#include "externals/externals.h"

#include "process.h"
#include "compilerParser/fileParser.h"
#include "compilerPreprocessor\compilerPreprocessor.h"
#include "Target/fileCache.h"
#include "Utility/funcky.h"

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

using std::string;
using std::getline;
using std::ifstream;
using std::cout;
using std::cerr;
using std::endl;
using std::ostream;

#pragma comment(lib,"Psapi.lib")

static bool isError = false;

struct compParams {
	std::string	 fName;
	bool		 isAp = false;
	bool		 isSlang = false;

	compParams ( char const *fName, bool isAp = false, bool isSlang = false ) : fName ( fName ), isAp ( isAp ), isSlang ( isSlang ) {};
};

fileCache	globalFileCache ( 0, 0 );

void compFile ( void *param )
{
	HANDLE		 		 port;
	DWORD				 nBytes;
	ULONG_PTR			 key;
	compParams			*cp = nullptr;
	char				 outName[MAX_PATH];
	BUFFER				 objBuff ( 1024ull*1024*16 );
	char				*code;
	int					 oHandle;

	port = (HANDLE) param;

	for ( ;; )
	{
		if ( !GetQueuedCompletionStatus ( port, &nBytes, &key, (LPOVERLAPPED *) &cp, 0 ) )
		{
			return;
		}

		printf ( "%s\r\n", cp->fName.c_str ( ) );

		if ( cp->isAp )
		{
			code = compAPPreprocessor ( cp->fName.c_str ( ), cp->isSlang, true );
		} else
		{
			auto entry = globalFileCache.read ( cp->fName.c_str ( ) );
			BUFFER b;

			b.write ( entry->getData ( ), entry->getDataLen ( ) );
			b.put ( 0 );	// need to do this to ensure null termination
			if ( !entry )
			{
				return;
			}
			code = compPreprocessor ( cp->fName.c_str ( ), b.data<char *> ( ), cp->isSlang );
		}

		opFile	oFile;

		char fName[MAX_PATH + 1];
		_fullpath ( fName, cp->fName.c_str ( ), sizeof ( fName ) );

		try
		{
			if ( !cp->isSlang )
			{
				oFile.ns.use ( "fgl" );
			}
			oFile.parseFile ( fName, code, cp->isSlang, cp->isAp, false );
		} catch ( errorNum err )
		{
			printf ( "error: %u - %s\r\n", err, scCompErrorAsText ( int (err) ).c_str ( ) );
		} catch ( ... )
		{
			printf ( "error: %u - %s\r\n", int(errorNum::scINTERNAL), scCompErrorAsText ( int(errorNum::scINTERNAL) ).c_str ( ) );
		}

		free ( code );

		if ( oFile.errHandler.isFatal ( ) )
		{
			isError = true;
			continue;
		}

		oFile.serialize ( &objBuff, false );

		_fmerge ( "*.slo", cp->fName.c_str ( ), outName, sizeof ( outName ) );

		if ( (oHandle = _open ( outName, _O_CREAT | _O_BINARY | _O_TRUNC | O_RDWR, _S_IREAD | _S_IWRITE )) == -1 )
		{
			printf ( "ERROR: unable to create output file \"%s\"\r\n", outName );
			isError = true;
			return;
		}
		_write ( oHandle, bufferBuff ( &objBuff ), (unsigned int) bufferSize ( &objBuff ) );

		_close ( oHandle );

		delete cp;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	try
	{
		size_t				 loop;
		HANDLE				 port;
		SYSTEM_INFO			 info;
		uint32_t			 nFiles = 0;

		DWORD dwMode = 0;
		if ( GetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), &dwMode ) )
		{
			SetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
		}

#ifdef _MSC_VER
		// fix for https://svn.boost.org/trac/boost/ticket/6320
		boost::filesystem::path::imbue ( std::locale ( "" ) );
#endif

		port = CreateIoCompletionPort ( INVALID_HANDLE_VALUE, NULL, 0, 0 );	// NOLINT (performance-no-int-to-ptr)

		for ( int loop = 1; loop < argc; loop++ )
		{
			if ( argv[loop][0] != '-' )
			{
				std::filesystem::path p ( argv[loop] );
				if ( !strccmp ( p.extension ().string ().c_str (), ".fgl" ) )
				{
					nFiles++;
					PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( argv[loop], false, false )) );
				} else 	if ( !strccmp ( p.extension ().string ().c_str (), ".ap" ) || !strccmp ( p.extension ().string ().c_str (), ".apf" ) )
				{
					nFiles++;
					PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( argv[loop], true, false )) );
				} else 	if ( !strccmp ( p.extension ().string ().c_str (), ".aps" ) )
				{
					nFiles++;
					PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( argv[loop], true, true )) );
				} else 	if ( !strccmp ( p.extension ().string ().c_str (), ".sl" ) )
				{
					nFiles++;
					PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( argv[loop], false, true )) );
				} else if ( !*p.extension ().string ().c_str () )
				{
					std::filesystem::path p2 ( argv[loop] );

					p.replace_extension ( ".fgl" );
					if ( _isfile ( p.generic_string ().c_str () ) )
					{
						nFiles++;
						PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( p.generic_string ().c_str (), false, false )) );
						continue;
					}
					p.replace_extension ( ".ap" );
					if ( _isfile ( p.generic_string ().c_str () ) )
					{
						nFiles++;
						PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( p.generic_string ().c_str (), true, true )) );
						continue;
					}
					p.replace_extension ( ".apx" );
					if ( _isfile ( p.generic_string ().c_str () ) )
					{
						nFiles++;
						PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( p.generic_string ().c_str (), true, true )) );
						continue;
					}
					p.replace_extension ( ".apf" );
					if ( _isfile ( p.generic_string ().c_str () ) )
					{
						nFiles++;
						PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( p.generic_string ().c_str (), true, false )) );
						continue;
					}
					p.replace_extension ( ".sl" );
					if ( _isfile ( p.generic_string ().c_str () ) )
					{
						nFiles++;
						PostQueuedCompletionStatus ( port, 0, 0, (LPOVERLAPPED)(new compParams ( p.generic_string ().c_str (), false, true )) );
						continue;
					}
				}
			}
		}

		GetSystemInfo ( &info );

		std::vector<std::thread>	threads;
		for ( loop = 0; loop < (nFiles < (int)info.dwNumberOfProcessors ? nFiles : (int)info.dwNumberOfProcessors); loop++ )
		{
			threads.push_back ( std::thread ( compFile, port ) );
		}
		for ( auto &it : threads )
		{
			it.join ();
		}

		return ((int)isError);
	} catch ( ... )
	{
		return errno;
	}
}

