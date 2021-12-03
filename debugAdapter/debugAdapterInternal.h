// engine debug adapter

#pragma once

#include "Utility/settings.h"

#include <algorithm>
#include <vector>
#include <unordered_map>
#include <map>
#include <list>

#include "Utility/jsonParser.h"
#include "bcVM/vmDebug.h"
#include "Utility/stringi.h"
#include "Target/vmTask.h"

struct dbAdapaterBP
{
	stringi		 fileName;
	std::variant<std::monostate, int64_t, stringi>	location;		// either a line number or a function name
	int64_t		 count = 0;
	stringi		 expr;

	bool		 verified = false;

	dbAdapaterBP ( stringi fileName, int64_t line, int64_t count, stringi const &expr ) : fileName ( fileName ), location ( line ), count ( count ), expr ( expr )
	{}

	dbAdapaterBP ( stringi funcName, int64_t count, stringi const &expr ) : location ( funcName ), count ( count ), expr ( expr )
	{}

	dbAdapaterBP ()
	{}

	virtual ~dbAdapaterBP ( void ) = default;
};


struct debuggerFile
{
	stringi						fName;
	std::vector<dbAdapaterBP>	bpList;

	debuggerFile ()
	{
	}
};

struct debuggerFileStore
{
	std::map<stringi, debuggerFile>	 files;
	std::map<int64_t, dbAdapaterBP>	 functionBreakpoints;

	debuggerFileStore ()
	{}

	debuggerFile &getFile ( stringi const &name )
	{
		return files[name];
	}
};

enum WM_SLANG_MESSAGES
{
	WM_SLANG_DEBUG = (WM_APP + 1),
	WM_SLANG_DEBUG_CONSOLE_INTERRUPT
};

class winDebugger : public vmDebug
{
	HWND								debuggerWindowHandle;

	public:

		vmDebugMessage					lastMsg{};

	winDebugger ( HWND window )
	{
		debuggerWindowHandle = window;
	}

	winDebugger ()
	{}

	void debugContinue ( vmDebugMessage *msg )
	{
		ReleaseSemaphore ( msg->continueEvent, 1, NULL );
	}

	void debugInterrupt ( vmDebugMessage *msg )
	{
		PostMessage ( debuggerWindowHandle, WM_SLANG_DEBUG, 0, (LPARAM)msg );
		WaitForSingleObject ( msg->continueEvent, INFINITE );
	}
};
extern	void			 debugServerInit ( class vmInstance *instance );
extern  taskControl		*debugAdapterInit ();

