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
	stringi		 expr;
	stringi		 count;
	stringi		 logMessage;

	bool		 verified = false;

	dbAdapaterBP ( stringi fileName, int64_t line, stringi const &condition, stringi const &count, stringi const &logMessage ) : fileName ( fileName ), location ( line ), expr( condition ), count( count ), logMessage( logMessage )
	{}

	dbAdapaterBP( stringi funcName, stringi const &condition, stringi const &count, stringi const &logMessage ) : location( funcName ), expr( condition), count( count ), logMessage ( logMessage )
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

	void Continue ( vmDebugMessage *msg )
	{
		ReleaseSemaphore ( msg->continueEvent, 1, NULL );
	}

	void Interrupt ( vmDebugMessage *msg )
	{
		PostMessage ( debuggerWindowHandle, WM_SLANG_DEBUG, 0, (LPARAM)msg );
		WaitForSingleObject ( msg->continueEvent, INFINITE );
	}
};
extern	void			 debugServerInit ( class vmInstance *instance );
extern  taskControl		*debugAdapterInit ();

