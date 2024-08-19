// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

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
#include <atlstr.h>

//template class __declspec(dllimport) CStringT<TCHAR, StrTraitMFC<TCHAR, ChTraitsCRT<TCHAR> > >;
//template class __declspec(dllimport) CSimpleStringT<TCHAR>;

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include "Utility/funcky.h"
#include "compilerParser/fglErrors.h"
#include "Utility/buffer.h"


#endif // _AFX_NO_AFXCMN_SUPPORT

