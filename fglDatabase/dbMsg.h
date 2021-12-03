#if !defined(AFX_DBMSG_H__CA6C233E_3221_4920_AC19_208DD9633CC3__INCLUDED_)
#define AFX_DBMSG_H__CA6C233E_3221_4920_AC19_208DD9633CC3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// dbMsg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// dbMsg dialog

class dbMsg : public CDialog
{
// Construction
public:
	dbMsg(CString msg, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(dbMsg)
	enum { IDD = IDD_DB_MSG };
	CString	m_Text;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(dbMsg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(dbMsg)
	afx_msg void OnCancel();
	afx_msg void OnIgnore();
	afx_msg void OnRepair();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DBMSG_H__CA6C233E_3221_4920_AC19_208DD9633CC3__INCLUDED_)
