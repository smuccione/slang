#include "stdafx.h"
#include "resource.h"

#include "commctrl.h"
#include "wincon.h"

#include "bcDebuggerProfiler.h"
#include "bcVM/vmDebug.h"
#include "bcVM/vmInstance.h"

/////////////////////////////////////////////////////////////////////////////
// ProfileDialog dialog


ProfileDialog::ProfileDialog(CWnd* pParent /*=NULL*/) : CDialog(ProfileDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(ProfileDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void ProfileDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ProfileDialog)
	DDX_Control(pDX, IDC_EXEC_PROFILE, m_Report);
	//}}AFX_DATA_MAP
}

#if defined ( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
BEGIN_MESSAGE_MAP ( ProfileDialog, CDialog )
	//{{AFX_MSG_MAP(ProfileDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
#if defined ( __clang__ )
#pragma clang diagnostic pop
#endif

/////////////////////////////////////////////////////////////////////////////
// ProfileDialog message handlers

BOOL ProfileDialog::OnInitDialog() 
{
	size_t		 loop;
	vmProfileData	*profile;
	char			 tmpC1[64];
	char			 tmpC2[64];
	char			 tmpC3[64];
	char			 tmpC4[64];
	char			 tmpC5[64];

	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	SetForegroundWindow ();

	m_Report.SetHeadings( _T("Function,200;Local Time,80;% of Total,80;Total Time,80;% of Total,80;nCalls,80;File,500") );
	m_Report.LoadColumnInfo();

	profile = debugGetExecutionProfile ( m_Instance );

	for ( loop = 0; loop < (*profile).size(); loop++ )
	{
		sprintf ( tmpC1, "% 12I64i", (*profile)[loop]->numMicroSecsLocal );
		sprintf ( tmpC2, "% 6.2f", (*profile)[loop]->percentLocal );

		sprintf ( tmpC3, "% 12I64i", (*profile)[loop]->numMicroSecsTotal );
		sprintf ( tmpC4, "% 6.2f", (*profile)[loop]->percentTotal );

		sprintf ( tmpC5, "% 5I64i", (int64_t)(*profile)[loop]->numCalls );

		m_Report.AddItem( _T((*profile)[loop]->funcName), _T(tmpC1), _T(tmpC2), _T(tmpC3), _T(tmpC4), _T(tmpC5), _T((*profile)[loop]->fileName) );
	}

	m_Report.Sort ( 3, 1 );

	delete profile;

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void doProfileDialog ( vmInstance *instance )

{
	ProfileDialog	dlg;

	dlg.m_Instance = instance;
	dlg.DoModal();
}
