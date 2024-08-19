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

#include "bcVM/bcVM.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmInstance.h"
#include "bcVM/vmDebug.h"
#include "bcDebugger\bcDebugger.h"

static class vmInstance	*debugInstance;

int AddOneListItem( HWND hWnd, LPSTR szText, void *param)
{
	LVITEM			hItem;

	memset ( &hItem, 0, sizeof ( hItem ) );

	hItem.mask			= LVIF_PARAM | LVIF_TEXT;
	hItem.pszText		= szText;
	hItem.lParam		= (int)(ptrdiff_t)param;
	hItem.iItem			= (int)(ptrdiff_t)param;

	return ( ListView_InsertItem ( hWnd, &hItem ) );
}

INT_PTR CALLBACK debuggerCallstackDlgProc ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	char					*lineBuff;
	DWORD					 loop;
	int						 loop2;
	int						 len;
	int						 width;
	vmCallStack				*callStack;
	HWND					 hwndList;
	class vmInstance		*instance;

	switch ( uMsg )
	{
	    case WM_INITDIALOG:
			instance = debugInstance;

			callStack = debugGetCallstack ( instance );

			width = 2048;

			lineBuff	= (char *)malloc ( sizeof ( char ) * (static_cast<size_t>(width + 1)) );

			hwndList = GetDlgItem ( hDlg, IDC_CALLSTACK );

			SetForegroundWindow ( hDlg );

			for ( loop = 0; loop < callStack->entry.size(); loop++ )
			{
				len = _snprintf_s ( lineBuff, static_cast<size_t>(width) - 1, _TRUNCATE, "%s ( ", callStack->entry[loop].funcName.c_str() );
				
				for ( loop2 = 0; loop2 < (int)callStack->entry[loop].params.size(); loop2++ )
				{
					len += _snprintf_s ( lineBuff + len, static_cast<size_t>(width) - len, _TRUNCATE, "%.32s", callStack->entry[loop].params[loop2]->getValue() );
					if ( loop2 != callStack->entry[loop].params.size() - 1 )
					{
						if ( len < width )
						{
							len += _snprintf_s ( lineBuff + len, static_cast<size_t>(width) - len, _TRUNCATE, ", " );
						}
					}
				}

				if ( len < width )
				{
					strncat_s ( lineBuff, static_cast<size_t>(width) - len, " )", static_cast<size_t>(width) - len);
				}

				AddOneListItem ( hwndList, lineBuff, (void *)(ptrdiff_t)loop );		// NOLINT(performance-no-int-to-ptr)
			}

			ListView_SetItemState ( hwndList, 0, LVIS_SELECTED, LVIS_SELECTED  );

			delete callStack;
			free ( lineBuff );
			break;

		case WM_NOTIFY:
			// Process notification messages.            
			switch (((LPNMHDR) lParam)->code) 		// NOLINT(performance-no-int-to-ptr)
			{
				case LVN_ITEMACTIVATE:
				case NM_RETURN:
					//EndDialog ( hDlg, ListView_GetHotItem ( ((LPNMHDR) lParam)->iItem ) );
					EndDialog ( hDlg, ((LPNMITEMACTIVATE) lParam)->iItem );							// NOLINT(performance-no-int-to-ptr)
					break;
				default:
					break;
			
			}
			return 0;

 		case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
					break;
 
                case IDCANCEL:
					EndDialog ( hDlg, -1 );
                    return FALSE; 
				default:
					return FALSE;
			}
 			break;
		default:
			return FALSE;
	}
	return FALSE;
}

void debuggerCallstackDlg (	class vmInstance *instance, char const ** fileName, size_t *currentLine, size_t *stackFrame, HWND parentWindow )
{
	INITCOMMONCONTROLSEX	 init{};
	vmCallStack				*callStack;
	INT_PTR 				 entry;
	static	char			 fName[MAX_PATH + 1];

	debugInstance = instance;

	init.dwSize = sizeof ( init );
	init.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx ( &init );

    if ( (entry = DialogBox ( (HINSTANCE) GetWindowLongPtr ( GetConsoleWindow(), GWLP_HINSTANCE ), MAKEINTRESOURCE ( IDD_CALLSTACK ), parentWindow, debuggerCallstackDlgProc )) >= 0 )			// NOLINT(performance-no-int-to-ptr)
	{
		callStack = debugGetCallstack ( instance );

		strcpy_s (  fName, sizeof ( fName ), callStack->entry[entry].fileName );
		*fileName	 = fName;
		*currentLine = (uint32_t)callStack->entry[entry].line;

		*stackFrame = (uint32_t)entry;

		delete callStack;
	}
}

