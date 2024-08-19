// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__EFC14262_1DC0_4861_B05E_25ABBA9FD593__INCLUDED_)
#define AFX_STDAFX_H__EFC14262_1DC0_4861_B05E_25ABBA9FD593__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#include <winsock2.h>
#include <afxsock.h>
//#include <atlstr.h>

//template class __declspec(dllimport) CStringT<TCHAR, StrTraitMFC<TCHAR, ChTraitsCRT<TCHAR> > >;
//template class __declspec(dllimport) CSimpleStringT<TCHAR>;

#endif // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
#endif // !defined(AFX_STDAFX_H__EFC14262_1DC0_4861_B05E_25ABBA9FD593__INCLUDED_)
