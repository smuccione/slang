#include "stdafx.h"
#include "resource.h"

#include "commctrl.h"
#include "wincon.h"

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

static HTREEITEM AddOneTreeItem( HTREEITEM hParent, LPSTR szText, HTREEITEM hInsAfter, HWND hwndTree, void *param, int expandable )
{
	HTREEITEM hItem;
	TV_ITEM tvI{};
	TV_INSERTSTRUCT tvIns{};
	
	// The .pszText, .iImage, and .iSelectedImage are filled in.
	tvI.mask			= TVIF_TEXT /*| TVIF_IMAGE | TVIF_SELECTEDIMAGE */ | TVIF_PARAM | TVIF_CHILDREN | TVIF_STATE; // no images for now

	tvI.pszText			= szText;
	tvI.cchTextMax		= lstrlen(szText);
//	tvI.iImage			= iImage;
//	tvI.iSelectedImage	= iImage;
	tvI.lParam			= (int)(ptrdiff_t) param;

	tvI.stateMask		= 0;
	if ( expandable )
	{
		tvI.cChildren	= 1;
	} else
	{
		tvI.cChildren = 0;
	}
	
	tvIns.item			= tvI;
	tvIns.hInsertAfter	= TVI_SORT;				// NOLINT(performance-no-int-to-ptr)
	tvIns.hParent		= hParent;
	
	// Insert the item into the tree.
	hItem = (HTREEITEM)SendMessage(hwndTree, TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)&tvIns);				// NOLINT(performance-no-int-to-ptr)
	
	return (hItem);	
}

static INT_PTR CALLBACK debuggerFuncListDlgProc ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	char					*lineBuff;
	DWORD					 loop;
	DWORD					 width;
	char					 lopClass[MAX_PATH]{};
	vmFunctionList			*funcList;
	HWND					 hwndTree;
	HTREEITEM				 htreeClass;
	class vmInstance		*instance;

	switch ( uMsg )
	{
	    case WM_INITDIALOG:
			instance = debugInstance;

			funcList = debugGetFunctionList ( instance );

			width = 2048;

			lineBuff	= (char *)malloc ( sizeof ( char ) * (static_cast<size_t>(width) + 1) );

			hwndTree = GetDlgItem ( hDlg, IDC_FUNC_LIST );

			SetForegroundWindow ( hDlg );

			htreeClass	= 0;

			for ( loop = 0; loop < funcList->entry.size(); loop++ )
			{
				if ( funcList->entry[loop].isUsercode )
				{
					if ( funcList->entry[loop].className && strccmp ( funcList->entry[loop].className, lopClass ) )
					{
						/* new class */
						htreeClass = AddOneTreeItem ( (HTREEITEM)NULL, (LPSTR)funcList->entry[loop].className.c_str(), (HTREEITEM)TVI_ROOT, hwndTree, 0, 1 );							// NOLINT (performance-no-int-to-ptr)
						AddOneTreeItem ( (HTREEITEM)htreeClass, (LPSTR) funcList->entry[loop].funcName.c_str(), (HTREEITEM)TVI_ROOT, hwndTree, (void *)(ptrdiff_t)(loop+1), 0 );		// NOLINT (performance-no-int-to-ptr)
						strcpy_s ( lopClass, sizeof ( lopClass ), (LPSTR) funcList->entry[loop].className.c_str() );
					} else
					{
						if ( funcList->entry[loop].className )
						{
							/* must still be in same class */
							AddOneTreeItem ( (HTREEITEM)htreeClass, (LPSTR) funcList->entry[loop].funcName.c_str(), (HTREEITEM)TVI_ROOT, hwndTree, (void *)(ptrdiff_t)(loop+1), 0 );	// NOLINT (performance-no-int-to-ptr)
						} else
						{
							/* not a class method */
							AddOneTreeItem ( (HTREEITEM)NULL, (LPSTR)funcList->entry[loop].funcName.c_str(), (HTREEITEM)TVI_ROOT, hwndTree, (void *)(ptrdiff_t)(loop+1), 0 );			// NOLINT (performance-no-int-to-ptr)
						}
					}
				}
			}

			delete funcList;
			free ( lineBuff );
			break;

		case WM_NOTIFY:
			// Process notification messages.            
			switch (((LPNMHDR) lParam)->code)		// NOLINT (performance-no-int-to-ptr)
			{
				case NM_DBLCLK:
				case NM_RETURN:
					{
						HTREEITEM   hItem;
						TVITEM		item{};

						hItem = TreeView_GetSelection ( ((LPNMHDR)lParam)->hwndFrom );		// NOLINT (performance-no-int-to-ptr)

						item.hItem	= hItem;
						item.mask	= TVIF_PARAM;

						TreeView_GetItem ( ((LPNMHDR)lParam)->hwndFrom, &item );		// NOLINT (performance-no-int-to-ptr)

						if ( item.lParam )
						{
							EndDialog ( hDlg, item.lParam - 1);
						} else
						{
							TreeView_Expand ( ((LPNMHDR)lParam)->hwndFrom, &item, TVE_TOGGLE );	// NOLINT (performance-no-int-to-ptr)
						}
					}
					break;
				default:
					break;
			
			}
			return false;

 		case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
					break;
 
                case IDCANCEL:
					EndDialog ( hDlg, -1 );
                    return false; 
				default:
					return false;
			}
 			break;
		default:
			return false;
	}
	return false;
}

void debuggerFuncListDlg ( class vmInstance *instance, char	const **fileName, size_t *currentLine, HWND parentWindow )
{

	INITCOMMONCONTROLSEX	 init{};
	INT_PTR					 entry;
	vmFunctionList			*funcList;
	static	char			 fName[MAX_PATH + 1];

	debugInstance = instance;

	init.dwSize = sizeof ( init );
	init.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx ( &init );

    if ( (entry = DialogBox ( (HINSTANCE) GetWindowLongPtr ( GetConsoleWindow(), GWLP_HINSTANCE ), MAKEINTRESOURCE ( IDD_FUNC_LIST ), parentWindow, debuggerFuncListDlgProc )) >= 0 )	// NOLINT (performance-no-int-to-ptr)
	{
		funcList = debugGetFunctionList ( instance );

		strcpy_s ( fName, sizeof ( fName ), funcList->entry[entry].fileName );
		*fileName	 = fName;
		*currentLine = funcList->entry[entry].lineNum;

		delete funcList;
	}
}

