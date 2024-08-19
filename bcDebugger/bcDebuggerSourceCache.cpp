/*
	SLANG JIT Debugger

*/

#include "stdafx.h"
#include "resource.h"

#include "commctrl.h"
#include "wincon.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>
#include <stdarg.h>

#include <list>
#include <vector>
#include <string>

#include "bcVM/bcVM.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmInstance.h"
#include "bcVM/vmDebug.h"
#include "bcDebugger\bcDebugger.h"

#define DebuggerClassName "SlangDebugClass"

#define MAX_LINE_WIDTH 1500

debuggerBreakPt::~debuggerBreakPt ( void )
{
}

debugListerSource::~debugListerSource ()
{
	if ( entry ) entry->free ( );
}

bool debugListerSource::isBreakpoint ( size_t line )
{
	for ( auto &it : bpList )
	{
		if ( it.line == line )
		{
			return true;
		}
	}
	return false;
}

void debugListerSource::setBreakpoint ( size_t line )
{
	for ( auto &it : bpList )
	{
		if ( it.line == line )
		{
			return;		// already set
		}
	}
	bpList.push_back ( debuggerBreakPt ( line, 1, "" ) );
}

void debugListerSource::clearBreakpoint ( size_t line )
{
	for ( auto it = bpList.begin(); it != bpList.end(); it++ )
	{
		if ( it->line == line )
		{
			bpList.erase ( it );
			return;
		}
	}
}

void debugListerSource::toggleBreakpoint ( size_t line )
{
	for ( auto it = bpList.begin(); it != bpList.end(); it++ )
	{
		if ( it->line == line )
		{
			bpList.erase ( it );
			return;
		}
	}
	// doesn't exist so create it
	bpList.push_back ( debuggerBreakPt ( line, 1, "" ) );
}

debugListerSource::debugListerSource ( char const *fName ) : fName ( fName )
{
	entry = globalFileCache.read ( fName );
	if ( !entry )
	{
		text = "";
		return;
	}
	text = (char const *)entry->getData ( );
	textSize = entry->getDataLen ( );

	auto ptr = text;
	size_t len = 0;
	linePtr.push_back ( text );
	while ( ptr < text + textSize )
	{
		if ( *ptr == '\r' )
		{
			ptr++;
			if ( ptr >= text + textSize ) break;
			lineLength.push_back ( len );
			if ( *ptr == '\n' )
			{
				ptr++;
			}
			linePtr.push_back ( ptr );
			len = 0;
		} else if ( *ptr == '\n' )
		{
			ptr++;
			lineLength.push_back ( len );
			linePtr.push_back ( ptr );
			len = 0;
		} else
		{
			len++;
			ptr++;
		}
	}
	lineLength.push_back ( len );
	numLines = (DWORD)lineLength.size();
	topLine = 0;
}

char const *debugListerSource::getLine ( size_t lineNum )
{
	if ( lineNum && (lineNum <= numLines) )
	{
		return linePtr[lineNum - 1];
	}
	return 0;
}

size_t debugListerSource::getLineLen ( size_t lineNum )
{
	if ( lineNum && (lineNum <= numLines) )
	{
		return ( lineLength[lineNum - 1] );
	}
	return 0;
}

size_t debugListerSource::getNumLines ( void )
{
	return numLines;
}

void debugListerSource::setTopLine ( size_t lineNum)
{
	topLine = lineNum;
}
size_t debugListerSource::getTopLine ( void )
{
	return topLine;
}

debugSourceCache::debugSourceCache(  )
{
}

debugSourceCache::~debugSourceCache()
{
}

bool debugSourceCache::isBreakpoint ( char const *fName, size_t lineNum )
{
	for ( auto &it : src )
	{
		if ( it.fName == fName  )
		{
			return it.isBreakpoint ( lineNum );
		}
	}
	return false;
}

size_t debugSourceCache::setBreakpoint ( char const *fName, size_t lineNum, class vmInstance *instance )
{
	for ( auto &it : src )
	{
		if ( it.fName == fName )
		{
			if ( instance )
			{
				lineNum = debugSetBreakpoint ( instance, fName, lineNum, stringi(), stringi(), stringi() );
			}
			it.setBreakpoint ( lineNum );
			return lineNum;
		}
	}

	if ( instance )
	{
		lineNum = debugSetBreakpoint ( instance, fName, lineNum, stringi(), stringi(), stringi() );
	}
	if ( lineNum )
	{
		src.push_back ( debugListerSource ( fName ) );
		src.back ( ).setBreakpoint ( lineNum );
	}
	return lineNum;
}

void debugSourceCache::clearBreakpoint ( char const *fName, size_t lineNum, class vmInstance *instance )
{
	for ( auto &it : src )
	{
		if ( it.fName == fName )
		{
			if ( instance ) debugClearBreakpoint ( instance, fName, lineNum );
			it.clearBreakpoint ( lineNum );
			return;
		}
	}
	src.push_back ( debugListerSource ( fName ) );
	src.back().clearBreakpoint ( lineNum );
	if ( instance ) debugClearBreakpoint ( instance, fName, lineNum );
}

size_t debugSourceCache::toggleBreakpoint ( char const *fName, size_t lineNum, class vmInstance *instance )
{
	if ( instance )
	{
		lineNum = debugAdjustBreakpoint ( instance, fName, lineNum );
	}
	if ( isBreakpoint ( fName, lineNum ) )
	{
		clearBreakpoint ( fName, lineNum, instance );
		return 0;
	} else
	{
		return setBreakpoint ( fName, lineNum, instance );
	}
}

char const *debugSourceCache::getSourceLine ( char const *fName, size_t lineNum, size_t *lineLen )
{
	for ( auto &it : src )
	{
		if ( it.fName == fName )
		{
			*lineLen = it.getLineLen ( lineNum );
			return it.getLine ( lineNum );
		}
	}
	src.push_back ( debugListerSource ( fName ) );
	return getSourceLine ( fName, lineNum, lineLen );
}

size_t debugSourceCache::getNumLines ( char const *fName )
{
	for ( auto &it : src )
	{
		if ( it.fName == fName )
		{
			return it.getNumLines ( );
		}
	}
	src.push_back ( debugListerSource ( fName ) );
	return src.back ( ).getNumLines ( );
}

void debugSourceCache::flush ( )
{
	src.clear ( );
}

void debugSourceCache::setTopLine ( char const *fName, size_t lineNum )
{
	for ( auto &it : src )
	{
		if ( it.fName == fName )
		{
			return it.setTopLine ( lineNum );
		}
	}
}

size_t debugSourceCache::getTopLine ( char const *fName )
{
	for ( auto &it : src )
	{
		if ( it.fName == fName )
		{
			return it.getTopLine ( );
		}
	}
	return 0;
}

void debugSourceCache::setBreakpoints ( class vmInstance *instance )
{
	// for each source file
	for ( auto &sourceIt : src )
	{
		// for each breakpoint in that file
		for ( auto &bpIt : sourceIt.bpList )
		{
			// set the breakpoint
			debugSetBreakpoint ( instance,sourceIt.fName, bpIt.line, stringi(), stringi(), stringi() );
		}
	}
}

