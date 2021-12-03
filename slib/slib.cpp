// slang.cpp : Defines the entry point for the console application.
//

// vmTest.cpp : Defines the class behaviors for the application.
//

#include "Utility/settings.h"

#include <process.h>
#include <io.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <filesystem>

#include <sys/stat.h>
#include <fcntl.h>
#include "Utility/funcky.h"

#include "Utility/buffer.h"
#include "slangLib.h"

int main(int argc, char *argv[] )
{
	size_t	loop;
	bool	del = false;

	DWORD dwMode = 0;
	if ( GetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), &dwMode ) )
	{
		SetConsoleMode ( GetStdHandle ( STD_OUTPUT_HANDLE ), dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
	}

	try
	{
		if ( argc < 2 )
		{
			printf ( "usage: slib <library> +/-<obj>..." );
			return 1;
		}
		auto src = argv[1];
		if ( argv[1][0] == '-' )
		{
			del = true;
			src = src + 1;
		}

		std::filesystem::path p ( src );
		if ( !p.has_extension () ) p.replace_extension ( ".slb" );
		libStore	lib ( p.string().c_str(), del );

		for ( loop = 2; loop < argc; loop++ )
		{
			if ( argv[loop][0] == '-' )
			{
				std::filesystem::path p2 ( argv[loop] + 1 );
				if ( !p2.has_extension () ) p.replace_extension ( ".slo" );
				lib.remove ( p2.string().c_str());
			} else if ( argv[loop][0] == '+' )
			{
				std::filesystem::path p2 ( argv[loop] + 1 );
				if ( !p2.has_extension () ) p.replace_extension ( ".slo" );
				lib.add ( p2.string ().c_str () );
			} else
			{
				std::filesystem::path p2 ( argv[loop] );
				if ( !p2.has_extension () ) p.replace_extension ( ".slo" );
				lib.add ( p2.string ().c_str () );
			}
		}
		lib.serialize ( );
	} catch ( ... )
	{
		return (int)GetLastError ( );
	}
	return 0;
}
