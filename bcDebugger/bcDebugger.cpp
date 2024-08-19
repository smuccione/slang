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
#include "sys/stat.h"
#include "time.h"

#include <list>
#include <vector>

#include "bcVM/bcVM.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmInstance.h"
#include "bcVM/vmDebug.h"
#include "bcDebugger\bcDebugger.h"
#include "bcDebuggerProfiler.h"

#include "Target/vmTask.h"
#include "Target/common.h"

#define DebuggerClassName "SlangDebugClass"

#define MAX_LINE_WIDTH 1500

static HWND		 debuggerWindowHandle;

static time_t FileTimeToTime_t ( FILETIME *ft )
{
	LARGE_INTEGER li{};

	li.HighPart = static_cast<SHORT>(ft->dwHighDateTime);
	li.LowPart	= static_cast<SHORT>(ft->dwLowDateTime);


	li.QuadPart -= 116444736000000000;
	li.QuadPart /= 10000000;	

	return ((time_t) li.LowPart);
}

static int fastStat ( char const *fName, struct _stat *buf )
{
	WIN32_FILE_ATTRIBUTE_DATA	attrData{};

	if ( !GetFileAttributesEx ( fName, GetFileExInfoStandard , &attrData ) )
	{
		return ( -1 );
	}

	buf->st_atime	= FileTimeToTime_t ( &attrData.ftLastAccessTime );
	buf->st_ctime	= FileTimeToTime_t ( &attrData.ftCreationTime  );
	buf->st_mtime	= FileTimeToTime_t ( &attrData.ftLastWriteTime );
	buf->st_size	= (_off_t)attrData.nFileSizeLow;
	buf->st_mode	= 0;

	if ( attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
	{
		buf->st_mode = _S_IFDIR;
	}
	return ( 0 );
}

UINT CALLBACK debuggerOpenHook ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )

{
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			SetForegroundWindow ( hDlg );
			break;
	}
	return 0;
}

static debugSourceCache		 srcCache;

static int debuggerShowFile	(	HANDLE				 screenBuffer, 
								class vmInstance	*instance, 
								char const			*currentFile,
								size_t				 debugErrorLine,
								size_t				 executeLine,
								size_t				*currentLine,
								char const			*msg
							)

{
	CONSOLE_SCREEN_BUFFER_INFO	 consoleInfo;
	DWORD						 width;
	DWORD						 height;
	size_t						 lineLen;
	DWORD						 numWritten;
	size_t						 maxLine;
	DWORD						 loop;
	COORD						 coord{};
	DWORD						 cnt;
	size_t						 topLine;

	char						 lineBuff[MAX_LINE_WIDTH + 1];
	char	const				*tmpPtr;

	GetConsoleScreenBufferInfo ( screenBuffer, &consoleInfo );

	width	= consoleInfo.srWindow.Right - consoleInfo.srWindow.Left + 1;
	height  = consoleInfo.srWindow.Bottom - consoleInfo.srWindow.Top;

	maxLine = srcCache.getNumLines ( currentFile );

	if ( !maxLine ) return 0;

	topLine = srcCache.getTopLine ( currentFile );

	if ( width < MAX_LINE_WIDTH )
	{
		width = MAX_LINE_WIDTH;
	}

	coord.X = static_cast<SHORT>(width);
	coord.Y = static_cast<SHORT>(height + 1);

	SetConsoleScreenBufferSize ( screenBuffer, coord );
	GetConsoleScreenBufferInfo ( screenBuffer, &consoleInfo );

	if ( *currentLine > maxLine ) *currentLine = maxLine;

	if ( *currentLine < 1 ) *currentLine = 1;

	if ( *currentLine > topLine + height - 5 )
	{
		topLine = *currentLine - height + 5;
	}
	if ( *currentLine < topLine + 5 )
	{
		if ( *currentLine < 5 )
		{
			topLine = 0;
		} else
		{
			topLine = *currentLine - 5;
		}
	}
	for ( loop = 0; loop <= height; loop++ )
	{
		tmpPtr = srcCache.getSourceLine ( currentFile, loop + topLine, &lineLen );

		memset ( lineBuff, ' ', sizeof ( lineBuff ) );

		cnt = 2;
		while ( lineLen )
		{
			if ( cnt >= MAX_LINE_WIDTH  )
			{
				break;
			}	
			switch ( *tmpPtr )
			{
				case '\t':
					cnt = (cnt + 4) & ~3;
					tmpPtr++;
					lineLen--;
					break;
				default:
					lineBuff[cnt++] = *(tmpPtr++);
					lineLen--;
					break;
			}
		}		
		coord.Y = static_cast<SHORT>(loop - 1);
		coord.X = 2;

		WriteConsoleOutputCharacter ( screenBuffer, lineBuff, MAX_LINE_WIDTH - 1, coord, &numWritten );

		coord.X = 0;

		if ( loop + topLine == debugErrorLine )
		{
			FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, width, coord, &numWritten );
		} else if ( loop + topLine == executeLine )
		{
			FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, width, coord, &numWritten );
		} else if ( loop + topLine == *currentLine )
		{
			FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, width, coord, &numWritten );
		} else
		{
			FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_BLUE | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, width, coord, &numWritten );
		}
		if ( srcCache.isBreakpoint ( currentFile, loop + topLine ) )
		{
			FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_BLUE | FOREGROUND_RED, 1, coord, &numWritten );
			WriteConsoleOutputCharacter ( screenBuffer, "*", 1, coord, &numWritten );
		} else
		{
			WriteConsoleOutputCharacter ( screenBuffer, " ", 1, coord, &numWritten );
		}
	}

	while ( loop <= height + 1 )
	{
		coord.X = 0;
		coord.Y = static_cast<SHORT>(loop - 1);
		FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_BLUE | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, width, coord, &numWritten );
		FillConsoleOutputCharacter ( screenBuffer, ' ', width, coord, &numWritten );
		loop++;
	}

	coord.X = 0;
	coord.Y = static_cast<SHORT>(*currentLine - topLine - 1);
	SetConsoleCursorPosition ( screenBuffer, coord );

	srcCache.setTopLine ( currentFile, topLine );

	_snprintf_s ( lineBuff, MAX_LINE_WIDTH, _TRUNCATE, " Line %I64u   %s", *currentLine, msg );
	SetConsoleTitle ( lineBuff );

	return 1;
}

static char const *helpData[] =	{	"   ALT-F1      Output Screen",
									"   ALT-F5      Stop",
									"   F2          Call Stack",
									"   F3          Variable list",
									"   F4          Function Picker",
									"   F5          Run",
									"   F6          Run to End of Function",
									"   F7          Run to Cursor",
									"   F8          Step Into",
									"   F9          Toggle Breakpoint",
									"   F10         Step Over",
									"   F11         Set Current Execution Line",
									"   F12         Execution profile",
									"",
									"   E           launch Editor",
									0
								};

static void debuggerHelp ( HANDLE screenBuffer )

{
	CONSOLE_SCREEN_BUFFER_INFO	 consoleInfo;
	INPUT_RECORD				*inputBuffer;
	DWORD						 width;
	DWORD						 height;
	DWORD						 numWritten;
	char						*lineBuff;
	COORD						 coord{};
	DWORD						 numRead{};
	DWORD						 loop;
	bool						 cont;

	GetConsoleScreenBufferInfo ( screenBuffer, &consoleInfo );

	width	= consoleInfo.srWindow.Right - consoleInfo.srWindow.Left + 1;
	height  = consoleInfo.srWindow.Bottom - consoleInfo.srWindow.Top;

	coord.X = static_cast<SHORT>(width + 1);
	coord.Y = static_cast<SHORT>(height + 1);

	inputBuffer = (INPUT_RECORD *)malloc ( sizeof ( INPUT_RECORD ) * 1000 ); // per spec

	lineBuff	= (char *)malloc ( sizeof ( char ) * (MAX_LINE_WIDTH + 1) );

	for ( loop = 0; helpData[loop]; loop++ )
	{
		memset ( lineBuff, ' ', MAX_LINE_WIDTH );
		memcpy ( lineBuff, helpData[loop], strlen ( helpData[loop] ) );

		coord.X = 0;
		coord.Y = (SHORT)loop;
		FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_BLUE | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, width, coord, &numWritten );
		WriteConsoleOutputCharacter ( screenBuffer, lineBuff, width, coord, &numWritten );
	}

	memset ( lineBuff, ' ', MAX_LINE_WIDTH );
	while ( loop < height + 1 )
	{
		coord.X = 0;
		coord.Y = (SHORT)loop;
		FillConsoleOutputAttribute ( screenBuffer, BACKGROUND_BLUE | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, width, coord, &numWritten );
		WriteConsoleOutputCharacter ( screenBuffer, lineBuff, width, coord, &numWritten );

		loop++;
	}

	cont = true;

	while ( cont )
	{
		do {
			GetNumberOfConsoleInputEvents ( GetStdHandle ( STD_INPUT_HANDLE ), &numRead );

			{
				MSG	msg;

				while ( PeekMessage (&msg, (HWND) 0, 0, 0, PM_NOREMOVE) )
				{
					GetMessage(&msg, (HWND) 0, 0, 0);
					if ( msg.message != WM_QUIT )
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			}
		} while ( !numRead );

		ReadConsoleInput ( GetStdHandle ( STD_INPUT_HANDLE ), inputBuffer, 1000, &numRead );

		for ( loop = 0; loop < numRead; loop++ )
		{
			switch ( inputBuffer[loop].EventType )
			{
				case KEY_EVENT:
					if ( inputBuffer[loop].Event.KeyEvent.bKeyDown)
					{
						/* keys when in debugger mode */
						switch ( inputBuffer[loop].Event.KeyEvent.wVirtualKeyCode )
						{
							case VK_F1:
							case VK_ESCAPE:
								cont = false;
								break;
						}
					}
			}
		}
	}

	free ( inputBuffer );
	free ( lineBuff );
}

HWND GetConsoleHwnd(void)
{
	return GetConsoleWindow();
}

static void debuggerProcess (	vmDebugMessage	 *dbgMsg,
								char const		**currentFile, 
								time_t			 *currentTime,
								char			**fileData, 
								DWORD			  fileDataLen, 
								DWORD			  debugErrorLine, 
								char const		 *msg, 
								DWORD			  escExit
							) 

{
	class vmInstance			*instance;
	CONSOLE_SCREEN_BUFFER_INFO	 consoleInfo;
	static HANDLE				 screenBuffer = 0;
	HANDLE						 oldBuffer;
	INPUT_RECORD				*inputBuffer;
	HWND						 hWnd;
//	HWND						 hWndSave;
	HANDLE						 waitHandle[1]{};
//	OPENFILENAME				 openFile;
//	char						*newFile;
	DWORD						 numRead{};
	short						 scroll;
	DWORD						 loop;
	DWORD						 cont;
	size_t						 currentLine;
	size_t						 stackFrame;
	DWORD						 horizontalPos;

	if ( !msg )
	{
		msg = "";
	}

	instance = dbgMsg->instance;

	char titleSave[4096];
	GetConsoleTitle ( titleSave, sizeof ( titleSave ) );

	hWnd = GetConsoleHwnd();
	ShowWindow ( hWnd, SW_SHOW );
	DWORD consoleModeSave;
	GetConsoleMode ( hWnd, &consoleModeSave );
	SetConsoleMode ( hWnd, consoleModeSave | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING );

	stackFrame = 0;

	horizontalPos = 0;

	oldBuffer = GetStdHandle ( STD_OUTPUT_HANDLE );

	screenBuffer = CreateConsoleScreenBuffer ( GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL );

	currentLine = debugErrorLine ? debugErrorLine : (uint32_t)dbgMsg->location.line;

	GetConsoleScreenBufferInfo ( screenBuffer, &consoleInfo );

	inputBuffer = (INPUT_RECORD *)malloc ( sizeof ( INPUT_RECORD ) * 1000 ); // per spec

	if ( !debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, dbgMsg->location.line, &currentLine, msg ) )
	{
		debugStepInto ( instance );
		debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, dbgMsg->location.line, &currentLine, msg );
		free ( inputBuffer );
		SetConsoleMode ( hWnd, consoleModeSave );
		return;
	}

	SetConsoleActiveScreenBuffer ( screenBuffer );

	FlushConsoleInputBuffer ( GetStdHandle ( STD_INPUT_HANDLE ) );

	for ( cont = 1; cont; )
	{	
		do {
			waitHandle[0] = GetStdHandle ( STD_INPUT_HANDLE );

			MsgWaitForMultipleObjects ( 1, waitHandle, 0, INFINITE, QS_ALLEVENTS );

			{
				MSG	msg;

				while ( PeekMessage (&msg, (HWND) 0, 0, 0, PM_NOREMOVE) )
				{
					GetMessage(&msg, (HWND) 0, 0, 0);
					if ( msg.message != WM_QUIT )
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			}

			GetNumberOfConsoleInputEvents ( GetStdHandle ( STD_INPUT_HANDLE ), &numRead );
		} while ( !numRead );

		ReadConsoleInput ( GetStdHandle ( STD_INPUT_HANDLE ), inputBuffer, 1000, &numRead );

		for ( loop = 0; loop < numRead; loop++ )
		{
			switch ( inputBuffer[loop].EventType )
			{
				case KEY_EVENT:
					if ( inputBuffer[loop].Event.KeyEvent.bKeyDown)
					{
						/* see if there is anything executing... if not then dont' allow the user to do 
						   "executable" things */
						if ( !escExit )
						{
							/* keys when in debugger mode */
							switch ( inputBuffer[loop].Event.KeyEvent.wVirtualKeyCode )
							{
								case VK_F2:
									hWnd = GetForegroundWindow();
									debuggerCallstackDlg ( instance, currentFile, &currentLine, &stackFrame, debuggerWindowHandle );
									SetForegroundWindow ( hWnd );
									break;
								case VK_F3:
									hWnd = GetForegroundWindow();
									debuggerShowInspectorDlg ( instance, stackFrame );
									SetForegroundWindow ( hWnd );
									break;
								case VK_F4:
									hWnd = GetForegroundWindow();
									debuggerFuncListDlg ( instance, currentFile, &currentLine, debuggerWindowHandle );
									SetForegroundWindow ( hWnd );
									break;
								case VK_F5:							// run
									if ( inputBuffer[loop].Event.KeyEvent.dwControlKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED) )
									{
//										halt ( instance );
									}
									cont = 0;
									break;
								case VK_F6:
									debugStepOut ( instance );
									cont = 0;
									break;
								case VK_F7:
									debugRunToLine ( instance, *currentFile, currentLine );
									cont = 0;
									break;
								case VK_F8:
									debugStepInto ( instance );
									cont = 0;
									break;
								case VK_F10:
									debugStepOver ( instance );
									cont = 0;
									break;
								case VK_F11:
									if ( debugSetIP ( instance, *currentFile, currentLine ) )
									{
										dbgMsg->location.line = currentLine;
										debugErrorLine = 0;
									}
									break;
								case VK_F12:
									doProfileDialog ( instance );
									break;
							}
						} else
						{
							/* keys when in bkpt setting mode */
							switch ( inputBuffer[loop].Event.KeyEvent.wVirtualKeyCode )
							{
								case VK_F4:
									hWnd = GetForegroundWindow();
									debuggerFuncListDlg ( instance, currentFile, &currentLine, debuggerWindowHandle );
									SetForegroundWindow ( hWnd );
									break;
								case VK_TAB:
									cont = 0;
									break;
							}
						}

						if ( cont )
						{
							switch ( inputBuffer[loop].Event.KeyEvent.wVirtualKeyCode )
							{
								case VK_F1:
									if ( inputBuffer[loop].Event.KeyEvent.dwControlKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED) )
									{
										SetConsoleActiveScreenBuffer ( oldBuffer );
										for (;; )
										{
											ReadConsoleInput ( GetStdHandle ( STD_INPUT_HANDLE ), inputBuffer, 1000, &numRead );
											for ( loop = 0; loop < numRead; loop++ )
											{
												if ( inputBuffer[loop].EventType == KEY_EVENT )
												{
													if ( inputBuffer[loop].Event.KeyEvent.bKeyDown )
													{
														break;
													}
												}
											}
											if ( loop != numRead )
											{	
												/* to break out of top loop and re-read a key */
												loop = numRead;
												break;
											}
										}
									} else
									{
										debuggerHelp ( screenBuffer );
									}
									SetConsoleActiveScreenBuffer ( screenBuffer );
									break;
								case VK_F9:
									srcCache.toggleBreakpoint ( *currentFile, currentLine, instance );
									break;
								case 'W':
//									doWatchWindow();
									break;
								case VK_LEFT:
									if ( horizontalPos )
									{
										horizontalPos -= inputBuffer[loop].Event.KeyEvent.wRepeatCount;
									}
									if ( horizontalPos < 0 )
									{
										horizontalPos = 0;
									}
									break;
								case VK_RIGHT:
									horizontalPos += inputBuffer[loop].Event.KeyEvent.wRepeatCount;
									break;
								case VK_UP:
									currentLine -= inputBuffer[loop].Event.KeyEvent.wRepeatCount;
									break;
								case VK_DOWN:
									currentLine += inputBuffer[loop].Event.KeyEvent.wRepeatCount;
									break;
								case VK_NEXT:
									currentLine += ((size_t)consoleInfo.srWindow.Bottom - (size_t)consoleInfo.srWindow.Top) - 1;
									break;
								case VK_PRIOR:
									if ( (int)currentLine > ((size_t)consoleInfo.srWindow.Bottom - (size_t)consoleInfo.srWindow.Top) - 1 )
									{
										currentLine -= ((size_t)consoleInfo.srWindow.Bottom - (size_t)consoleInfo.srWindow.Top) - 1;
									} else
									{
										currentLine = 0;
									}
									break;
								case VK_HOME:
									if ( horizontalPos )
									{
										horizontalPos = 0;
									} else
									{
										currentLine = 0;
									}
									break;
								case VK_END:
									currentLine = 99999999;
									break;
								case VK_TAB:
#if 0
									memset ( &openFile, 0, sizeof ( openFile ) );

									openFile.lStructSize = sizeof ( openFile );
									openFile.lpstrFilter = "FGL Source Files\0*.AP;*.FGL\0All Files\0*.*\0";
									openFile.lpstrFile	 = debuggerFileName;
									openFile.nMaxFile	 = MAX_PATH;
									openFile.lpstrTitle  = "Open";
									openFile.Flags		 = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ENABLEHOOK;
									openFile.lpstrDefExt = ".FGL";
									openFile.lpfnHook	 = debuggerOpenHook;

									hWndSave = GetForegroundWindow ( );
									if ( GetOpenFileName ( &openFile ) )
									{
										newFile = debuggerFileName;
									} else
									{
										newFile = 0;
									}
									SetForegroundWindow ( hWndSave );

									if ( newFile )
									{
										if ( !lastFile || (newFile && strccmp ( newFile, lastFile )) )
										{	
											*currentFile = newFile;
											if ( !debuggerReadFile ( *currentFile, fileData, &fileDataLen ) )
											{
												cont = 0;;
											} else
											{
												strcpy ( lastFile, newFile);
												fastStat ( lastFile, &statBuff );
												*currentTime = statBuff.st_mtime;
											}
											*topLine = 0;
											msg = *currentFile;
										}
									}
#endif
									break;
							}
						}
					}
					SetConsoleActiveScreenBuffer ( screenBuffer );
					debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, (uint32_t)dbgMsg->location.line, &currentLine, msg );
					break;
				case MOUSE_EVENT:
					if ( inputBuffer[loop].Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED )
					{
						currentLine = (size_t)inputBuffer[loop].Event.MouseEvent.dwMousePosition.Y + 1;
						debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, (uint32_t)dbgMsg->location.line, &currentLine, msg );
					}
					if ( inputBuffer[loop].Event.MouseEvent.dwEventFlags & MOUSE_WHEELED )
					{
						scroll = (short)(inputBuffer[loop].Event.MouseEvent.dwButtonState >> 16);
						scroll /= 120;

						if ( scroll < 0 )
						{
							currentLine -= (size_t)scroll - ((size_t)(scroll + 1) * ((size_t)scroll + 1));
						} else
						{
							currentLine -= (size_t)scroll + ((size_t)(scroll - 1) * ((size_t)scroll - 1));
						}
						debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, (uint32_t)dbgMsg->location.line, &currentLine, msg );
					}
					if ( inputBuffer[loop].Event.MouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED )
					{
						if ( srcCache.isBreakpoint ( *currentFile, currentLine ) )
						{
							/* already set so clear it */
							srcCache.clearBreakpoint ( *currentFile, currentLine );
//							debuggerClearBreakpoint ( instance, *currentFile, currentLine );
						} else
						{
							/* not set so set it */
							srcCache.setBreakpoint ( *currentFile, currentLine );
//							debuggerSetBreakpoint ( instance, *currentFile, currentLine );
						}
						debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, (uint32_t)dbgMsg->location.line, &currentLine, msg );
					}
					break;
				case WINDOW_BUFFER_SIZE_EVENT:
					/* nothing to do.. debuggerShowFile will handle automatically */
					GetConsoleScreenBufferInfo ( screenBuffer, &consoleInfo );
					debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, (uint32_t)dbgMsg->location.line, &currentLine, msg );
					break;

			}
		}
	}

	debuggerShowFile ( screenBuffer, instance, *currentFile, debugErrorLine, (uint32_t)dbgMsg->location.line, &currentLine, msg );

	free ( inputBuffer );
	SetConsoleActiveScreenBuffer ( oldBuffer );
	SetConsoleTitle ( titleSave );
	SetConsoleMode ( hWnd, consoleModeSave );

	return;
}

LRESULT CALLBACK debuggerWindowProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )

{
	static	char				*fileData = 0;
	static	long				 fileDataLen = 0;
	static	char const			*currentFile = 0;
	static  time_t				 currentTime;
	static	class vmInstance	*lastInstance = 0;
			vmDebugMessage		*msg;
			char				 msgBuff[256];
//			OPENFILENAME		 openFile;
			struct _stat		 statBuff;
			INPUT_RECORD		inpEvent{};
			DWORD				nWritten;

	// pause console read loop to keep from deadlocking
	inpEvent.EventType							= KEY_EVENT;
	inpEvent.Event.KeyEvent.bKeyDown			= 0;
	inpEvent.Event.KeyEvent.dwControlKeyState	= 0xFFFF;

	WriteConsoleInput ( GetStdHandle ( STD_INPUT_HANDLE ), &inpEvent, 1 ,&nWritten );
	Sleep ( 0 );

	switch ( uMsg )
	{
		case WM_SLANG_DEBUG:
			msg = (vmDebugMessage *)lParam;			// NOLINT(performance-no-int-to-ptr)

			if ( !lastInstance || (msg->instance == lastInstance) )
			{
				currentFile = msg->location.fName;
				lastInstance = msg->instance;

				if ( !fastStat ( currentFile, &statBuff ) )
				{
					currentTime = statBuff.st_mtime;
				}
				switch ( msg->state )
				{
					case vmStateType::vmTrap:
						_snprintf_s ( msgBuff, sizeof ( msgBuff ), _TRUNCATE, "%s    %s:%s(%I64u): error E%x: %s", msg->instance->getName ( ), msg->location.fName, msg->location.functionName, msg->location.line, uint32_t(msg->location.err), scCompErrorAsText ( size_t(msg->location.err) ).c_str ( ) );
	//						doWatchWindowUpdate ( msg->instance );
						debuggerProcess ( msg, &currentFile, &currentTime, &fileData, fileDataLen, (uint32_t) msg->location.line, msgBuff, 0 );
						break;
					case vmStateType::vmTrace:
					case vmStateType::vmDebugBreak:
	//						doWatchEnableUpdate ( msg->instance );
	//						doWatchWindowUpdate ( msg->instance );
						_snprintf_s ( msgBuff, sizeof ( msgBuff ), _TRUNCATE, "%s    %s:%s(%I64u)", msg->instance->getName ( ), msg->location.fName, msg->location.functionName, msg->location.line );
						debuggerProcess ( msg, &currentFile, &currentTime, &fileData, fileDataLen, 0, msgBuff, 0 );
	//						doWatchDisableUpdate ( msg->instance );
						break;
					case vmStateType::vmThreadEnd:
						if ( debugIsProfiling ( msg->instance ) )
						{
							char msgBuff[256];
							_snprintf_s ( msgBuff, sizeof ( msgBuff ), _TRUNCATE, "%s Execution Complete", msg->instance->getName ( ) );
							debuggerProcess ( msg, &currentFile, &currentTime, &fileData, fileDataLen, 0, msgBuff, 0 );
						}
						lastInstance = 0;
						currentFile = 0;
	//					doWatchWindowReset ( msg->instance );
						break;
					case vmStateType::vmLoad:
						srcCache.setBreakpoints ( msg->instance );
						lastInstance = 0;
						break;
					case vmStateType::vmThreadStart:
						srcCache.flush ( );		// allow any flushing to occur (if needed)
						lastInstance = 0;
						/* set any Breakpoints that we may have in this instance */
	//					debuggerDoBp ( msg->instance );
						break;
					case vmStateType::vmHalt:
						break;
					default:
						debugContinue ( msg );
						return (DefWindowProc ( hWnd, uMsg, wParam, lParam ));
				}
			}
			debugContinue ( msg );
			break;
		case WM_SLANG_DEBUG_CONSOLE_INTERRUPT:
#if 0
			msg	= (DEBUG_SLANG_MSG *)lParam;

			memset ( &openFile, 0, sizeof ( openFile ) );

			openFile.lStructSize = sizeof ( openFile );
			openFile.lpstrFilter = "FGL Executables\0*.APX;*.EXE\0FGL Source Files\0*.AP;*.FGL\0Active Page Source Files\0*.AP\0Active Page Executables\0*.APX\0FGL Source Files\0*.FGL\0FGL Executables\0*.EXE\0All Files\0*.*\0";
			openFile.lpstrFile	 = debuggerFileName;
			openFile.nMaxFile	 = MAX_PATH;
			openFile.lpstrTitle  = "Open";
			openFile.Flags		 = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ENABLEHOOK;
			openFile.lpstrDefExt = ".APX";
			openFile.lpfnHook	 = debuggerOpenHook;

			hWnd = GetForegroundWindow ( );
			if ( GetOpenFileName ( &openFile ) )
			{
				/*------------ remove any previous images ---------- */
				_execReinit ( msg->instance );
				_varReinit  ( msg->instance );
				_atomReinit ( msg->instance );
				_omReInit	( msg->instance );

				{
					int			 buffLen;
					char		*buff;
					int			 handle;
					int			 pos;
					char		 ext[MAX_PATH];

					_fparsefx ( debuggerFileName, ext );

					if ( (handle = _open ( debuggerFileName, _O_BINARY | _O_RDONLY )) != -1 )
					{
						if ( !strccmp ( ext, "EXE" ) )
						{
							pos = _lseek ( handle, 0, SEEK_END );
							pos = _lseek ( handle, pos - 4, SEEK_SET );
							_read ( handle, &pos, sizeof ( long ) );

							_lseek ( handle, pos, SEEK_SET );

							buffLen = _flen ( handle ) - _tell ( handle );
							buff = malloc ( sizeof ( char ) * buffLen );

							_read ( handle, buff, buffLen );
							_close ( handle );
							
							_atomPut ( msg->instance, _strdup ( "MAIN" ), aFUNCREF, 0, 0 );

							if ( !_libResolve ( buff, buffLen, msg->instance, 1, 1, debuggerFileName, 1 ) )
							{
								printf ( "%s(%-d) error E%-04d: %-s\r\n", msg->instance->id, 0, msg->instance->scERROR, scErrorAsText ( msg->instance->scERROR ) );

								_close ( handle );

								currentFile = 0;
							} else
							{
								msg->instance->currentLine   = debugGetFirstLine ( (char *)((SFUNC *)(_atomGetCargo2 ( msg->instance, _atomFindType ( msg->instance, "MAIN", aFUNCTYPE ) )))->codeBuff );
								msg->instance->currentdebugId = ((SFUNC *)(_atomGetCargo2 ( msg->instance, _atomFindType ( msg->instance, "MAIN", aFUNCTYPE ) )))->id;

								strcpy ( debuggerFileName, msg->instance->currentdebugId );
								currentFile = debuggerFileName;
							}
						} else if ( !strccmp ( ext, "APX" ) )
						{
#if 0
							buffLen = _flen ( handle ) - _tell ( handle );
							buff = malloc ( sizeof ( char ) * buffLen );

							_read ( handle, buff, buffLen );
							_close ( handle );
							
							if ( !execLoadCompile ( msg->instance, (COMPILE *)buff, debuggerFileName ) )
							{
								printf ( "%s(%-d) error E%-04d: %-s\r\n", msg->instance->id, 0, msg->instance->scERROR, scErrorAsText ( msg->instance->scERROR ) );

								_close ( handle );

								currentFile = 0;
							} else
							{
								msg->instance->currentLine   = debugGetFirstLine ( (char *)((SFUNC *)(_atomGetCargo2 ( msg->instance, _atomFindType ( msg->instance, "MAIN", aFUNCTYPE ) )))->codeBuff );
								msg->instance->currentdebugId = ((SFUNC *)(_atomGetCargo2 ( msg->instance, _atomFindType ( msg->instance, "MAIN", aFUNCTYPE ) )))->id;

								strcpy ( debuggerFileName, msg->instance->currentdebugId );
								currentFile = debuggerFileName;
							}
#endif
						} else
						{
							/* not an executable */
							currentFile = debuggerFileName;
							msg->instance->currentLine = 0;
						}
					}
				}
			} else
			{
				currentFile = 0;
			}
			SetForegroundWindow ( hWnd );

			if ( currentFile && *currentFile )
			{
				if ( !debuggerReadFile ( currentFile, &fileData, &fileDataLen ) )
				{
					debugContinue ( msg->instance );
					break;
				}
				msg->instance->currentdebugId = currentFile;

				fastStat ( currentFile, &statBuff );
				currentTime = statBuff.st_mtime;

				debuggerProcess ( msg, &currentFile, &currentTime, &fileData, fileDataLen, 0, msg->instance->currentdebugId, 1 );
			}
			debugContinue ( msg->instance );
#endif
			break;
	}

	return ( 1 );
}

static volatile bool debuggerReady = false;

static UINT debuggerMain ( class vmInstance *instance )

{
	MSG					msg;
	HWND				hWnd;
	WNDCLASSEX			debugWindowClass{};

	debugWindowClass.cbSize			= sizeof ( debugWindowClass );
	debugWindowClass.style			= 0;
	debugWindowClass.lpfnWndProc	= debuggerWindowProc;
	debugWindowClass.cbClsExtra		= 0;					/* extra bytes */
	debugWindowClass.cbWndExtra		= 0;					/* window extra */
	debugWindowClass.hInstance		= (HINSTANCE) GetWindowLongPtr ( GetConsoleWindow(), GWLP_HINSTANCE );			// NOLINT(performance-no-int-to-ptr)
	debugWindowClass.hIcon			= 0;
	debugWindowClass.hCursor		= 0;
	debugWindowClass.hbrBackground	= 0;
	debugWindowClass.lpszMenuName	= 0;
	debugWindowClass.lpszClassName	= DebuggerClassName;
	debugWindowClass.hIconSm		= 0;

	RegisterClassEx( &debugWindowClass );

	hWnd = CreateWindow (	DebuggerClassName,
							"Slang Debugger",
							0,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							NULL,
							NULL,
							(HINSTANCE) GetWindowLongPtr ( GetConsoleWindow(), GWLP_HINSTANCE ),		// NOLINT(performance-no-int-to-ptr)
							0
						 );

	debuggerWindowHandle = hWnd;

	debuggerReady = true;

	while (GetMessage(&msg, (HWND) 0, 0, 0)) 
	{
		if ( msg.message != WM_QUIT )
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else
		{
			break;
		}
	}
	return 0;
}

void debuggerInit ( class vmInstance *instance, bool enable )
{
	if ( !debuggerReady )
	{
		// spawn debugger thread
		AfxBeginThread( (AFX_THREADPROC)debuggerMain, instance, THREAD_PRIORITY_NORMAL, 0, 0 );

		while ( !debuggerReady )
		{
			Sleep ( 10 );
		}
	}

	std::shared_ptr<vmDebug> debugger ( static_cast<vmDebug *>(new winDebugger ( debuggerWindowHandle )) );
	debugRegister ( instance, debugger );
}

