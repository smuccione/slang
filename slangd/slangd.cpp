
#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <fcntl.h>
#include <io.h>

#include "externals/externals.h"

#include "LanguageServer/languageServer.h"
#include "Target/vmTask.h"
#include "Target/fileCache.h"

///////////////////////////////////////////////////////////////////////////////
//  include required boost libraries
#include <boost/config.hpp>     //  global configuration information

#include <boost/assert.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/filesystem/config.hpp>

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>    // token class
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp> // lexer class

#pragma comment(lib,"Psapi.lib")

fileCache globalFileCache( 0, 0 );

int main( int argc, char *argvp[] )
{

#ifdef _MSC_VER
	// fix for https://svn.boost.org/trac/boost/ticket/6320
	boost::filesystem::path::imbue( std::locale( "" ) );
#endif

	if ( argc > 1 )
	{
		if ( !strcmp( argvp[1], "-c" ) )
		{
			_setmode( _fileno( stdin ), _O_BINARY );
			_setmode( _fileno( stdout ), _O_BINARY );

			startLanguageServer();
			
			while ( 1 ) Sleep( 10000000 );
		}
	} else
	{
		startLanguageServer( 6996 );

		while ( 1 ) Sleep ( 10000000 );
	}

	return 0;
}
