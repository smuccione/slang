#pragma once

#if defined ( __cplusplus )

#include "SortListCtrl.h"
#include "SortHeaderCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// ProfileDialog dialog

class ProfileDialog : public CDialog
{
// Construction
public:
	ProfileDialog(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(ProfileDialog)
	enum { IDD = IDD_EXEC_PROFILE };
	CSortListCtrl	m_Report;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(ProfileDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	class vmInstance *m_Instance;

protected:

	// Generated message map functions
	//{{AFX_MSG(ProfileDialog)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#else

extern void doProfileDialog ( INSTANCE *instance );

#endif

extern void doProfileDialog ( class vmInstance *instance );
