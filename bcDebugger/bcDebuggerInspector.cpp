// bcDebuggerInspector.cpp : implementation file
//

#include "stdafx.h"

#include "resource.h"

#include "bcInterpreter/bcInterpreter.h"
#include "bcInterpreter/op_variant.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"
#include "bcVM/bcVM.h"
#include "bcVM/vmDebug.h"

#include "bcDebugger.h"
#include "bcDebuggerInspector.h"
#include "SimpleBrowser.h"

#include <map>
#include <string>
#include <algorithm>

HTREEITEM AddOneTreeItem ( HTREEITEM hParent, LPSTR szText, HTREEITEM hInsAfter, HWND hWnd, void *param, int expandable )
{
	HTREEITEM hItem;
	TV_ITEM tvI{};
	TV_INSERTSTRUCT tvIns{};
	
	// The .pszText, .iImage, and .iSelectedImage are filled in.
	tvI.mask			= TVIF_TEXT /*| TVIF_IMAGE | TVIF_SELECTEDIMAGE */ | TVIF_PARAM | TVIF_CHILDREN | TVIF_STATE; // no images for now

	tvI.pszText			= szText;
	tvI.cchTextMax		= lstrlen(szText);
	tvI.lParam			= (LPARAM) param;

	tvI.stateMask		= 0;
	if ( expandable )
	{
		tvI.cChildren	= 1;
	} else
	{
		tvI.cChildren = 0;
	}
	
	tvIns.item			= tvI;
	tvIns.hInsertAfter	= TVI_LAST;				// NOLINT(performance-no-int-to-ptr)

	tvIns.hParent	= hParent;
	
	// Insert the item into the tree.
	hItem = (HTREEITEM) ::SendMessage ( hWnd, TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)&tvIns);				// NOLINT(performance-no-int-to-ptr)
	
	return (hItem);	
}

// CInspector dialog

IMPLEMENT_DYNAMIC(CInspector, CDialog)

CInspector::CInspector(CWnd* pParent /*=NULL*/) : CResizableDialog(CInspector::IDD, pParent)
{

}

CInspector::~CInspector()
{
	for ( auto &it : m_inspectorTrees )
	{
		delete it;
	}
	delete m_inspectors;
}

void CInspector::DoDataExchange(CDataExchange* pDX)
{
	CResizableDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INSPECTOR_TOP_TAB, m_topTab);
	DDX_Control(pDX, IDC_INSPECTOR_TAB, m_bottomTab);
}


BEGIN_MESSAGE_MAP(CInspector, CResizableDialog)
	ON_NOTIFY(TCN_SELCHANGE, IDC_INSPECTOR_TAB, &ThisClass::OnTcnSelchangeInspectorTab)
	ON_NOTIFY(TCN_SELCHANGE, IDC_INSPECTOR_TOP_TAB, &ThisClass::OnTcnSelchangeInspectorTopTab)
	ON_WM_SIZE()
	ON_WM_SHOWWINDOW()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CInspector message handlers

BOOL CInspector::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your specialized code here and/or call the base class

	return CResizableDialog::OnCommand(wParam, lParam);
}

void CInspector::SetData ( vmDebugVar *var )
{
	if ( var->isStringValue() )
	{
		m_dumpTab.SetPointerData ( (DWORD)var->getValueLen(), (LPBYTE)var->getValue(), true );

		{
			char *tmp;
			tmp = (char *)malloc ( var->getValueLen() + 1 );
			memcpy ( tmp, var->getValue(), var->getValueLen() );
			tmp[var->getValueLen()] = 0;
			m_stringTab.SetWindowTextA ( tmp );
			free ( tmp );
		}

		// fill in the web browser
		// exception handling is VERY necessary here... it is quite possible to crash the browser control
		// by sending invalid html code to it.
		__try 
		{
			m_browserTab.Stop();
			m_browserTab.Clear();
			m_browserTab.Write ( _T(var->getValue()), var->getValueLen() );
		}
		__except ( EXCEPTION_EXECUTE_HANDLER )
		{
			__try 
			{
				m_browserTab.Clear();
				m_browserTab.Write ( _T(var->getValue()), var->getValueLen() );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER )
			{
			}
		}
	} else
	{
		m_dumpTab.SetPointerData ( 0, (LPBYTE)"", true );
		m_stringTab.SetWindowTextA ( "" );

		// fill in the web browser
		// exception handling is VERY necessary here... it is quite possible to crash the browser control
		// by sending invalid html code to it.
		__try 
		{
			m_browserTab.Stop();
			m_browserTab.Clear();
			m_browserTab.Write ( _T(""), 1 );
		}
		__except ( EXCEPTION_EXECUTE_HANDLER )
		{
			__try 
			{
				m_browserTab.Clear();
				m_browserTab.Write ( _T(""), 1 );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER )
			{
			}
		}
	}
}

BOOL CInspector::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	NMTREEVIEW				*tv;
	DWORD					 code;

	tv = (LPNMTREEVIEW) lParam;			// NOLINT(performance-no-int-to-ptr)

	if ( tv )
	{
		code = (int) tv->hdr.code;
		switch ( code )
		{
			case TVN_SELCHANGED:
				DWORD loop;

				for ( loop = 0; loop < m_inspectorTrees.size(); loop++ )
				{
					if ( tv->hdr.hwndFrom == m_inspectorTrees[loop]->GetSafeHwnd() )
					{
						SetData ( (vmDebugVar *)tv->itemNew.lParam );			// NOLINT(performance-no-int-to-ptr)
						break;
					}
				}
				break;
			case TVN_ITEMEXPANDINGA:
				if ( tv->itemNew.lParam )
				{
					if ( !(tv->itemNew.state & TVIS_EXPANDEDONCE) )
					{
						vmInspectList	*iList;
						vmDebugVar		*var;

						var = (vmDebugVar *)tv->itemNew.lParam;			// NOLINT(performance-no-int-to-ptr)

						iList = debugInspect ( m_instance, m_stackFrame, var, 0, 0 );
						if ( iList )
						{
							m_inspectorTrees[m_topTab.GetCurSel()]->iList.push_back ( iList );	// save it so we can free it after we'e done
	
							DWORD		loop2;
							HTREEITEM	hTPrev;

							hTPrev = (HTREEITEM) tv->itemNew.hItem;

							if ( iList->entry.size() )
							{
								for ( loop2 = 0; loop2 < iList->entry.size(); loop2++ )
								{
									char	tmpBuff[512];
									uint32_t len;
									len = (uint32_t)iList->entry[loop2]->getValueLen();

									if ( len > 256 ) len = 256;
									
									if ( *(iList->entry[loop2]->getValue()) && *(iList->entry[loop2]->getName()) )
									{
										if ( iList->entry[loop2]->isStringValue ( ) )
										{
											sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "%.*s : \"%.*s\"", (uint32_t) iList->entry[loop2]->getNameLen ( ), iList->entry[loop2]->getName ( ), len, iList->entry[loop2]->getValue ( ) );
										} else
										{
											sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "%.*s : %.*s", (uint32_t) iList->entry[loop2]->getNameLen ( ), iList->entry[loop2]->getName ( ), len, iList->entry[loop2]->getValue ( ) );
										}
									} else if ( *(iList->entry[loop2]->getName()) )
									{
										sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "%.*s", (uint32_t) iList->entry[loop2]->getNameLen ( ), iList->entry[loop2]->getName ( ) );
									} else
									{
										if ( iList->entry[loop2]->isStringValue ( ) )
										{
											sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "\"%.*s\"", len, iList->entry[loop2]->getValue ( ) );
										} else
										{
											sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "%.*s", len, iList->entry[loop2]->getValue ( ) );
										}
									}

									hTPrev = AddOneTreeItem ( tv->itemNew.hItem, tmpBuff, hTPrev, tv->hdr.hwndFrom, (void *)iList->entry[loop2], iList->entry[loop2]->hasChildren() );
								}
							}
						}
					}
				}
				break;
		}
	}
			
	return CResizableDialog::OnNotify(wParam, lParam, pResult);
}

void CInspector::OnTcnSelchangeInspectorTab(NMHDR *pNMHDR, LRESULT *pResult)
{
	if ( m_bottomTab.GetCurSel()!= -1 )
	{
		ShowBottomTab ( m_bottomTab.GetCurSel() );
	}
	*pResult = 0;
}

void CInspector::OnTcnSelchangeInspectorTopTab(NMHDR *pNMHDR, LRESULT *pResult)
{
	if ( m_topTab.GetCurSel()!= -1 )
	{
		ShowTopTab ( m_topTab.GetCurSel() );
	}
	*pResult = 0;
}

void CInspector::ShowTopTab (int number)
{
	CRect	tabRect;

	// compute the locations for our rect
	m_topTab.GetClientRect( tabRect );
	m_topTab.ClientToScreen( tabRect );
	ScreenToClient( tabRect );
	m_topTab.AdjustRect( FALSE, tabRect );

	DWORD		loop;

	for ( loop = 0; loop < m_inspectorTrees.size(); loop++ )
	{
		if ( number == loop )
		{
			m_inspectorTrees[loop]->SetWindowPos (&wndTop, tabRect.left, tabRect.top, tabRect.Width(), tabRect.Height(), SWP_SHOWWINDOW);
		} else
		{
			m_inspectorTrees[loop]->ShowWindow ( SW_HIDE );
		}
	}

	m_topTab.SetCurSel ( number );
}

void CInspector::ShowBottomTab (int number)
{
	CRect	tabRect;

	// compute the locations for our rect
	m_bottomTab.GetClientRect( tabRect );
	m_bottomTab.ClientToScreen( tabRect );
	ScreenToClient( tabRect );
	m_bottomTab.AdjustRect( FALSE, tabRect );

	m_dumpTab.ShowWindow ( SW_HIDE );
	m_stringTab.ShowWindow ( SW_HIDE );
	m_nameValueTab.ShowWindow ( SW_HIDE );
	::ShowWindow( m_browserTab.GetSafeHwnd(), SW_HIDE );

	switch ( number )
	{
		case 0:
			m_dumpTab.SetWindowPos (&wndTop, tabRect.left, tabRect.top, tabRect.Width(), tabRect.Height(), SWP_SHOWWINDOW);
			break;
		case 1:
			m_stringTab.SetWindowPos (&wndTop, tabRect.left, tabRect.top, tabRect.Width(), tabRect.Height(), SWP_SHOWWINDOW);
			break;
		case 2:
			m_nameValueTab.SetWindowPos (&wndTop, tabRect.left, tabRect.top, tabRect.Width(), tabRect.Height(), SWP_SHOWWINDOW);
			break;
		case 3:
			m_browserTab.SetWindowPos (&wndTop, tabRect.left, tabRect.top, tabRect.Width(), tabRect.Height(), SWP_SHOWWINDOW);
			break;
	}
	m_bottomTab.SetCurSel(number);

	return;
}

BOOL CInspector::OnInitDialog()
{
	DWORD			 loop;
	CRect			 tabRect;

	CResizableDialog::OnInitDialog();	

	// Set up our top tab pages
	m_topTab.GetWindowRect ( tabRect );
	m_topTab.GetClientRect( tabRect );
	m_topTab.ClientToScreen( tabRect );
	m_topTab.AdjustRect( FALSE, tabRect );

	tabRect.left = tabRect.left;
	tabRect.top = tabRect.top;
	tabRect.right = tabRect.left + tabRect.Width();
	tabRect.bottom = tabRect.top + tabRect.Height();


	// initialize the top tabs from the available debugger inspectors
	m_inspectors = debugGetInspectors ( m_instance );
	for ( loop = 0; loop < m_inspectors->entry.size (); loop++ )
	{
		m_inspectorTrees.push_back ( new CInspectorTab () );
		m_inspectorTrees[loop]->Create ( TVS_LINESATROOT | TVS_HASBUTTONS | TVS_HASLINES | TVS_NOTOOLTIPS, tabRect, this, IDC_INSPECTOR_TOP_TAB_DYN + loop );
		m_inspectorTrees[loop]->SetFont ( this->GetFont () );
		m_topTab.InsertItem ( (int)loop, m_inspectors->entry[loop] );

		DWORD			 loop2;
		vmInspectList	*iList;

		m_inspectorTrees[loop]->iList.push_back ( iList = debugInspect ( m_instance, loop, m_stackFrame ) );

		std::sort ( iList->entry.begin (), iList->entry.end (), []( vmDebugVar *left, vmDebugVar *right ) -> bool { return strccmp ( left->getName (), right->getName () ) < 0; } );

		for ( loop2 = 0; loop2 < (iList->entry.size () > 100 ? 100 : iList->entry.size ()); loop2++ )
		{
			char	tmpBuff[512];
			uint32_t	len;
			len = (uint32_t)iList->entry[loop2]->getValueLen ();

			if ( len > 256 ) len = 256;

			sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "%s : %.*s", iList->entry[loop2]->getName (), len, iList->entry[loop2]->getValue () );
			AddOneTreeItem ( (HTREEITEM)TVI_ROOT, tmpBuff, (HTREEITEM)TVI_SORT, m_inspectorTrees[loop]->GetSafeHwnd (), (void *)iList->entry[loop2], iList->entry[loop2]->hasChildren () );		// NOLINT(performance-no-int-to-ptr)
		}
	}

	ShowTopTab ( 0 );

	m_bottomTab.GetWindowRect ( tabRect );
	m_bottomTab.GetClientRect( tabRect );
	m_bottomTab.ClientToScreen( tabRect );
	ScreenToClient( tabRect );
	m_bottomTab.AdjustRect( FALSE, tabRect );

	// initialize the bottom tabs

	//  dump tab
	m_dumpTab.myCreate ( tabRect, this, IDC_INSPECTOR_DUMP_TAB );
	m_dumpTab.SetDefaultStyles ( false );
	m_bottomTab.InsertItem ( 0, "Dump" );

	// large string viewer

	m_stringTab.Create ( ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY, tabRect, this, IDC_INSPECTOR_STRING_TAB );
	m_bottomTab.InsertItem ( 1, "String" );
	m_stringTab.ModifyStyle ( 0, WS_VSCROLL | WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0 );

	// name/value viewer

	LVCOLUMN	colStruct{};
	m_nameValueTab.Create (LVS_REPORT | LVS_SORTASCENDING | LVS_SINGLESEL | LVS_NOSORTHEADER, tabRect, this, IDC_INSPECTOR_NAME_VALUE_TAB );
	m_nameValueTab.ModifyStyle ( 0, WS_VSCROLL | WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP );
	colStruct.mask = LVCF_FMT  | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	colStruct.fmt = LVCFMT_LEFT;
	colStruct.iOrder = 0;
	colStruct.iSubItem = 0;
	colStruct.pszText = const_cast<char *>("Name");
	colStruct.cchTextMax = 4;
	colStruct.cx = 150;
	m_nameValueTab.InsertColumn ( 0, &colStruct );
	colStruct.iOrder = 1;
	colStruct.iSubItem = 1;
	colStruct.pszText = const_cast<char *>("Value");
	colStruct.cchTextMax = 5;
	colStruct.cx = 700;
	m_nameValueTab.InsertColumn ( 1, &colStruct );
	m_bottomTab.InsertItem	 ( 2, "Name/Value" );

	// browser
	m_browserTab.myCreate		( WS_VISIBLE | WS_TABSTOP, tabRect, this, IDC_INSPECTOR_BROWSER_TAB );
	m_bottomTab.InsertItem	( 3 , "HTML" );

	ShowBottomTab ( 0 );

	// add resizable anchors
	AddAnchor ( IDC_INSPECTOR_TOP_TAB, TOP_LEFT, BOTTOM_RIGHT );
	AddAnchor ( IDC_INSPECTOR_TAB, BOTTOM_LEFT, BOTTOM_RIGHT );

	CResizableDialog::LoadWindowRect ( "Debug\\VarWindow", 0 );

	return TRUE;
}

void CInspector::OnSize(UINT nType, int cx, int cy)
{
	CResizableDialog::OnSize(nType, cx, cy);

	if ( m_topTab.m_hWnd )
	{
		ShowTopTab ( m_topTab.GetCurFocus() );
		ShowBottomTab (m_topTab.GetCurFocus());

		CResizableDialog::SaveWindowRect ( "Debug\\VarWindow", 0 );
	}
}

void CInspector::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CResizableDialog::OnShowWindow(bShow, nStatus);

	// TODO: Add your message handler code here
}

void CInspector::OnDestroy()
{
	CResizableDialog::OnDestroy();

	// TODO: Add your message handler code here
}

void debuggerShowInspectorDlg ( class vmInstance *instance, size_t stackFrame )
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	CInspector dlg;

	dlg.m_instance		= instance;
	dlg.m_stackFrame	= stackFrame;

	dlg.DoModal();
}

