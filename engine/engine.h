// vmTest.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "externals/externals.h"

#ifndef __AFXWIN_H__
#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include <string>


// CvmTestApp:
// See vmTest.cpp for the implementation of this class
//

class CSlangServer : public CWinApp
{
	public:
	CSlangServer ();

	void testServerStart ( void );
	void appServerStart ( void );

	bool InstallAPPService ( void );
	bool DeRegisterAsService ( void );
	bool RegisterAsService ();
	bool CheckService ( void );

	std::string			 engineConf = "engine.ini";
	HANDLE				 shutDown;

	// Overrides
	public:
	virtual BOOL InitInstance ();

	// Implementation

	DECLARE_MESSAGE_MAP ()
};

extern CSlangServer theApp;