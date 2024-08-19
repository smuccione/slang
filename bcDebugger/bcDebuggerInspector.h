#pragma once
#include "afxcmn.h"
#include "resource.h"
#include "SimpleBrowser.h"
#include "PPDumpCtrl.h"
#include "ColumnTreeCtrl.h"
#include "ResizableLib\ResizableDialog.h"

// CInspector dialog

class CInspectorTab : public CTreeCtrl 
{
public:
	std::vector<vmInspectList *>	iList;

	~CInspectorTab()
	{
		for ( auto &it : iList )
		{
			delete it;
		}
	}
};

class CInspector : public CResizableDialog
{
	DECLARE_DYNAMIC(CInspector)

public:
	CInspector(CWnd* pParent = NULL);   // standard constructor
	virtual ~CInspector();

// Dialog Data
	enum { IDD = IDD_INSPECTOR };

public:
	class vmInstance				*m_instance;
	size_t							 m_stackFrame;

	std::vector<CInspectorTab *>	 m_inspectorTrees;

	class vmInspectorList			*m_inspectors;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
private:
	CTabCtrl m_topTab;
	CTabCtrl m_bottomTab;

	void ShowBottomTab	( int number );
	void ShowTopTab		( int number );
	void SetData		( vmDebugVar *var );
public:
	afx_msg void OnTcnSelchangeInspectorTab(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTcnSelchangeInspectorTopTab(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnDestroy();

	SimpleBrowser		m_browserTab;
	CPPDumpCtrl			m_dumpTab;
	CEdit				m_stringTab;
	CListCtrl			m_nameValueTab;


};
