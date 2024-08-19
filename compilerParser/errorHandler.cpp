/*

	Error Handler

*/

#include "compilerParser/fileParser.h"
#include "Target/common.h"
#include "errorHandler.h"
#include "compilerAST/astNode.h"
#include "compilerParser/fglErrors.h"
#include "compilerAST/astNodeWalk.h"

char const *errorHandler::getName ( uint32_t sourceIndex )
{
	return file->srcFiles.getName ( sourceIndex ).c_str ();
}

errorHandler::errorHandler ( opFile *file ) : file ( file )
{
	fatalThrown = 0;
	warnThrown = 0;
	locality.reserve ( 256 );
	locality.push_back ( std::pair{ srcLocation (), nullptr } );
}

void errorHandler::setLine ( source &src )
{
	while ( nPopNeeded )
	{
		locality.pop_back ();
		nPopNeeded--;
	}
	assert ( locality.size () > 1 );
	if ( src.getSourceIndex ( ) )
	{
		locality.back () = std::pair{ srcLocation ( src ), nullptr };
	}
}

errorLocality::errorLocality ( errorHandler *e, source &src ) : err ( e )
{
	while ( err->nPopNeeded )
	{
		err->locality.pop_back ();
		err->nPopNeeded--;
	}
	if ( (err->locality.size() <= 1) || (src.getSourceIndex ( ) != err->topSourceIndex ( )) )
	{
		err->locality.emplace_back ( std::pair{ srcLocation ( src ), nullptr } );
		doPop = true;
	} else
	{
		assert ( err->locality.size () > 1 );
		err->locality.back () = std::pair{ srcLocation ( src ), nullptr };
	}
}

static astNode *findExtentCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, srcLocation &location )
{
	if ( node->location.sourceIndex == location.sourceIndex )
	{
		if ( node->location.lineNumberStart == location.lineNumberStart && node->location.lineNumberEnd == location.lineNumberEnd )
		{
			if ( node->location.columnNumber < location.columnNumber ) location.columnNumber = node->location.columnNumber;
			if ( node->location.columnNumberEnd > location.columnNumberEnd ) location.columnNumberEnd = node->location.columnNumberEnd;
		}
	}
	return node;
}

void findExtent ( opFile *file, astNode *block, srcLocation &location  )
{
	symbolStack	sym ( file );

	astNodeWalk ( block, &sym, findExtentCB, location );
}

errorLocality::errorLocality ( errorHandler *e, astNode *node, bool force ) : err ( e )
{
	while ( err->nPopNeeded )
	{
		err->locality.pop_back ();
		err->nPopNeeded--;
	}
	if ( force || (err->locality.size () <= 1) || (node->location.sourceIndex != std::get<0>(err->locality.back ( )).sourceIndex) )
	{
		err->locality.emplace_back ( std::pair { node->location, node } );
		doPop = true;
	} else
	{
		assert ( err->locality.size () > 1 );
		err->locality.back () = std::pair{ srcLocation ( node->location ), nullptr };
	}
}


void errorHandler::showLineCaret ( char const *fileName, srcLocation const &location, astNode *srcNode, consoleColor color )
{
	srcLocation extendedLocation = location;
	if ( srcNode )
	{
		findExtent ( file, srcNode, extendedLocation );
	}

	auto &src = file->getSourceListing ( fileName );
	auto len = src.getLineLen ( extendedLocation.lineNumberStart );
	auto line = src.getLine ( extendedLocation.lineNumberStart );

	printf ( "%s", windowsToFgColorCode[int(consoleColor::DarkGray)] );
	printf ( "%.*s\r\n", len, line );
	if ( extendedLocation.columnNumber && extendedLocation.columnNumber <= len )
	{
		for ( uint32_t loop = 1; loop < extendedLocation.columnNumber; loop++ )
		{
			if ( _isspace ( line + loop - 1 ) )
			{
				// handle potential tabs
				printf ( "%c", line[loop - 1] );
			} else
			{
				printf ( " " );
			}
		}
		for ( uint32_t loop = extendedLocation.columnNumber; loop < location.columnNumber; loop++ )
		{
			printf ( "~" );
		}

		printf ( "%s", windowsToFgColorCode[int(color)] );
		for ( uint32_t loop = location.columnNumber; loop < location.columnNumberEnd; loop++ )
		{
			printf ( "^" );
		}
		printf ( "%s", windowsToFgColorCode[int(consoleColor::DarkGray)] );

		for ( uint32_t loop = location.columnNumberEnd; loop < extendedLocation.columnNumberEnd; loop++ )
		{
			printf ( "~" );
		}
		printf ( "\r\n" );
	}
}

void errorHandler::throwFatalError ( bool isLS, errorNum errNum )
{
	if ( !isLS )
	{
		for ( auto it = locality.begin (); it != locality.end (); it++ )
		{
			if ( std::get<0> ( *it ).sourceIndex )
			{
				printf ( "%s(%u)", getName ( std::get<0> ( *it ).sourceIndex ), std::get<0> ( *it ).lineNumberStart );
				if ( std::next ( it ) != locality.end () && std::get<0> ( *std::next ( it ) ).sourceIndex )
				{
					printf ( "::" );
				}
			}
		}
		printf ( ": %sError E%08x - %s\r\n", windowsToFgColorCode[int ( consoleColor::Red )], uint32_t(errNum), scCompErrorAsText ( size_t ( errNum ) ).c_str () );
		printf ( "%s", windowsToFgColorCode[int ( consoleColor::DarkGray )] );
		showLineCaret ( getName ( std::get<0> ( locality.back () ).sourceIndex ), std::get<0> ( locality.back () ), std::get<1> ( locality.back () ), consoleColor::Red );
	}
	if ( !fatalThrown ) this->errNum = errNum;
	fatalThrown = true;
}
