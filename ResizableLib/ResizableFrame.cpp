// ResizableFrame.cpp : implementation file
//
/////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2000-2002 by Paolo Messina
// (http://www.geocities.com/ppescher - ppescher@yahoo.com)
//
// The contents of this file are subject to the Artistic License (the "License").
// You may not use this file except in compliance with the License. 
// You may obtain a copy of the License at:
// http://www.opensource.org/licenses/artistic-license.html
//
// If you find this code useful, credits would be nice!
//
/////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ResizableFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CResizableFrame

IMPLEMENT_DYNCREATE(CResizableFrame, CFrameWnd)

CResizableFrame::CResizableFrame()
{
	m_bEnableSaveRestore = FALSE;
}

CResizableFrame::~CResizableFrame()
{
}


BEGIN_MESSAGE_MAP(CResizableFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CResizableFrame)
	ON_WM_GETMINMAXINFO()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CResizableFrame message handlers

void CResizableFrame::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI) 
{
	MinMaxInfo(lpMMI);

	CView* pView = GetActiveView();
	if (pView == NULL)
		return;

	// get the extra size from view to frame
	CRect rectClient, rectWnd;
	GetWindowRect(rectWnd);
	RepositionBars(0, 0xFFFF, AFX_IDW_PANE_FIRST, reposQuery, rectClient);
	CSize sizeExtra = rectWnd.Size() - rectClient.Size();

	// ask the view for track size
	MINMAXINFO mmiView = *lpMMI;
	pView->SendMessage(WM_GETMINMAXINFO, 0, (LPARAM)&mmiView);
	mmiView.ptMaxTrackSize = sizeExtra + mmiView.ptMaxTrackSize;
	mmiView.ptMinTrackSize = sizeExtra + mmiView.ptMinTrackSize;

	// min size is the largest
	lpMMI->ptMinTrackSize.x = __max(lpMMI->ptMinTrackSize.x,
		mmiView.ptMinTrackSize.x);
	lpMMI->ptMinTrackSize.y = __max(lpMMI->ptMinTrackSize.y,
		mmiView.ptMinTrackSize.y);

	// max size is the shortest
	lpMMI->ptMaxTrackSize.x = __min(lpMMI->ptMaxTrackSize.x,
		mmiView.ptMaxTrackSize.x);
	lpMMI->ptMaxTrackSize.y = __min(lpMMI->ptMaxTrackSize.y,
		mmiView.ptMaxTrackSize.y);
}

// NOTE: this must be called after setting the layout
//       to have the view and its controls displayed properly
BOOL CResizableFrame::EnableSaveRestore(LPCTSTR pszSection, BOOL bRectOnly)
{
	m_sSection = pszSection;

	m_bEnableSaveRestore = TRUE;
	m_bRectOnly = bRectOnly;

	// restore immediately
	return LoadWindowRect(pszSection, bRectOnly);
}

void CResizableFrame::OnDestroy() 
{
	if (m_bEnableSaveRestore)
		SaveWindowRect(m_sSection, m_bRectOnly);

	CFrameWnd::OnDestroy();
}
