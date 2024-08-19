/*
	Byte code debugger

*/

#pragma once

#include <vector>
#include <list>
#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <Windows.h>
#include <vector>
#include <string>
#include <list>
#include "Utility/stringi.h"
#include "Target/fileCache.h"
#include "bcVM/vmDebug.h"
#include "debugAdapter/debugAdapter.h"
#include "debugAdapter/debugAdapterInternal.h"

struct debuggerBreakPt {	
	size_t		 line;
	size_t		 count;
	stringi		 expr;

	debuggerBreakPt ( size_t line, size_t count, char const *expr ) : line ( line ), count ( count ), expr ( expr )
	{}

	virtual ~debuggerBreakPt	( void );

	debuggerBreakPt ( debuggerBreakPt const &old )
	{
		line = old.line;
		count = old.count;
		expr = old.expr;
	}

	debuggerBreakPt ( debuggerBreakPt &&old )
	{
		*this = std::move ( old );
	}

	debuggerBreakPt &operator = ( debuggerBreakPt const &old )
	{
		return *this = debuggerBreakPt ( old );
	}

	debuggerBreakPt &operator = ( debuggerBreakPt &&old )
	{
		std::swap ( line, old.line );
		std::swap ( count, old.count );
		std::swap ( expr, old.expr );
		return *this;
	}
};


struct debugListerSource {
	fileCacheEntry					*entry = nullptr;
	stringi							 fName;
	std::vector<char const *>		 linePtr;
	std::vector<size_t>				 lineLength;
	size_t							 numLines = 0;
	size_t							 topLine = 0;		/* top of last window into file shown */
	char const						*text = nullptr;
	size_t							 textSize = 0;
	std::list<debuggerBreakPt>		 bpList;

	debugListerSource	( char const *fName );
	~debugListerSource	();

	debugListerSource ( debugListerSource const &old ) = delete;
	debugListerSource &operator = ( debugListerSource const &old ) = delete;

	debugListerSource ( debugListerSource &&old )
	{
		*this = std::move ( old );
	}
	debugListerSource &operator = ( debugListerSource &&old )
	{
		std::swap ( entry, old.entry );
		std::swap ( fName, old.fName );
		std::swap ( linePtr, old.linePtr );
		std::swap ( lineLength, old.lineLength );
		std::swap ( numLines, old.numLines );
		std::swap ( topLine, old.topLine );
		std::swap ( text, old.text );
		std::swap ( textSize, old.textSize );
		std::swap ( bpList, old.bpList );
		return *this;
	}

	bool		 isBreakpoint		( size_t line );
	void		 setBreakpoint		( size_t line );
	void		 clearBreakpoint	( size_t line );
	void		 toggleBreakpoint	( size_t line );
	char const	*getLine			( size_t lineNum );
	size_t		 getLineLen			( size_t lineNum );
	size_t		 getNumLines		( void );
	void		 setTopLine			( size_t lineNum );
	size_t		 getTopLine			( void );
};

class debugSourceCache {
	std::list<debugListerSource>		src;

public:
	debugSourceCache(  );
	virtual ~debugSourceCache();
	bool		 isBreakpoint		( char const *fName, size_t lineNum );
	size_t		 setBreakpoint		( char const *fName, size_t lineNum, class vmInstance *instance = 0 );
	void		 clearBreakpoint	( char const *fName, size_t lineNum, class vmInstance *instance = 0 );
	size_t		 toggleBreakpoint	( char const *fName, size_t lineNum, class vmInstance *instance = 0 );
	char const	*getSourceLine		( char const *fName, size_t lineNum, size_t *lineLen );
	size_t		 getNumLines		( char const *fName );
	size_t		 getTopLine			( char const *fName );
	void		 setTopLine			( char const *fName, size_t lineNum );
	void		 flush				( void );

	void		 setBreakpoints		( class vmInstance *instance );
};

extern	void	debuggerInit				( class vmInstance *instance, bool enable );
extern	void	debuggerCallstackDlg		( class vmInstance *instance, char const ** fileName, size_t *currentLine, size_t *stackFrame, HWND parentWindow );
extern	void	debuggerFuncListDlg			( class vmInstance *instance, char	const **fileName, size_t *currentLine, HWND parentWindow );
extern	void	debuggerShowInspectorDlg	( class vmInstance *instance, size_t stackFrame );