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

#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <filesystem>

#include <sys/stat.h>
#include <fcntl.h>
#include "Utility/funcky.h"

#include "Utility/buffer.h"
#include "slangLib.h"

std::vector<std::string> parseArgs ( std::string const &line )
{
	std::vector<std::string>	res;

	auto charP = line.c_str ();
	while ( *charP )
	{
		while ( *charP == ' ' ) charP++;
		if ( !*charP ) return res;

		std::string argument;
		bool		inQuotes = false;
		while ( *charP )
		{
			switch ( *charP )
			{
				case ' ':
					if ( !inQuotes && argument.size () )
					{
						res.push_back ( argument );
						argument.clear ();
					}
					break;
				case '"':
					if ( inQuotes )
					{
						if ( charP[1] == '"' )
						{
							charP++;
							argument.push_back ( *charP );
						} else
						{
							inQuotes = false;
						}
					} else
					{
						inQuotes = true;
					}
					break;
				case '\\':
					if ( (charP[1] == '"') || charP[1] == '\\' )
					{
						charP++;
						argument.push_back ( *charP );
					} else
					{
						argument.push_back ( *charP );
					}
					break;
				default:
					argument.push_back ( *charP );
					break;
			}
			charP++;
		}
	}
	return res;
}

int main(int argc, char *argv[] )
{
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
			printf ( "usage: slib <library> +/-<obj>... | @<responseFile>" );
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


		auto parseInput = [&]( auto &&parseInput, std::vector<std::string> const &args ) ->void {
			for ( auto &it : args )
			{
				if ( it[0] == '@' )
				{
					std::ifstream inFile ( it.c_str () + 1 );
					if ( !inFile.is_open () )
					{
						printf ( "cannot open command redirection file: %s\r\n", it.c_str () + 1 );
						throw GetLastError ();
					}
					std::string inLine;
					while ( std::getline ( inFile, inLine ) )
					{
						auto args = parseArgs ( inLine );
						parseInput ( parseInput, args );
					}
				} else if ( it[0] == '-' )
				{
					std::filesystem::path p2 ( it.c_str() + 1 );
					if ( !p2.has_extension () ) p.replace_extension ( ".slo" );
					lib.remove ( p2.string ().c_str () );
				} else if ( it[0] == '+' )
				{
					std::filesystem::path p2 ( it.c_str () + 1 );
					if ( !p2.has_extension () ) p.replace_extension ( ".slo" );
					lib.add ( p2.string ().c_str () );
				} else
				{
					std::filesystem::path p2 ( it );
					if ( !p2.has_extension () ) p.replace_extension ( ".slo" );
					lib.add ( p2.string ().c_str () );
				}
			}
		};

			// parse out any command line parameters
		std::vector<std::string> args;
		for ( int loop = 1; loop < argc; loop++ )
		{
			args.push_back ( argv[loop] );
		}
		parseInput ( parseInput, args );

		lib.serialize ();
	} catch ( ... )
	{
		return (int)GetLastError ( );
	}
	return 0;
}
