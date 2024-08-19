/*
	COM object support... can generically call methods and set/get properties
*/

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <atlbase.h>
#include <tchar.h>
#include <oaidl.h>
#include <ocidl.h>

#include <Windows.h>
#include "comdef.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"
#include "bcVM/vmAllocator.h"

#define COMClassDRIVER_OLENAMELEN 120

// COMClassInfo: private helper class to store type info of a method or a property
class COMClassInfo
{
	friend class COMClassDriver;
	COMClassInfo ( );
	~COMClassInfo ( );
	// dispatch id
	DISPID m_dispID = 0;
	// method or property name
	BSTR m_bstrName = nullptr;
	// invoke flag
	WORD m_wFlag = 0;
	// offset of virtual function
	short m_oVft = -1;
	// calling convention
	CALLCONV m_callconv = CC_STDCALL;
	// output type
	VARTYPE m_vtOutputType = VT_NULL;
	// output data
	VARIANT *m_pOutput = nullptr;
	// number of parameters
	int m_nParamCount = 0;
	// parameter type array
	WORD *m_pParamTypes = nullptr;
	// assignment operator
	COMClassInfo &operator=( const COMClassInfo &src );
};

// helper class to initialize/uninitialize com via RAII
class CoInit
{
public:
	CoInit ( )
	{
		CoInitializeEx ( NULL, COINIT_APARTMENTTHREADED );
	}
	~CoInit ( )
	{
		CoUninitialize ( );
	}
};

// COMClassDriver: the main class
class COMClassDriver
{
	// initialize/uninitialize com
	CoInit m_coInit;
	// pointer to the IDispatch interface
	IDispatch *m_pDisp = nullptr;
	// pointer to the IConnectionPoint interface
	IConnectionPoint *m_pCP = nullptr;
	// used by IConnectionPoint::Advise/Unadvise
	DWORD m_dwCookie = 0;
	// number of methods and properties
	int m_nDispInfoCount = 0;
	// array of type info
	COMClassInfo *m_pDispInfo = nullptr;
	// error return code
	HRESULT m_hRet = S_OK;
	// exception info
	EXCEPINFO *m_pExceptInfo = nullptr;
	// private helper functions/members
	int m_nVarCount = 0;
	int m_nMethodCount = 0;;
	int FindDispInfo ( LPCTSTR strName, const WORD wFlag = DISPATCH_METHOD );
	HRESULT InvokeMethodV ( const int nIndex );
	bool LoadTypeInfo ( );
public:
	COMClassDriver ( );
	virtual ~COMClassDriver ( );
	// clean up
	void Clear ( );
	// copy contructor
	COMClassDriver ( const COMClassDriver &src );
	// assignment operator
	COMClassDriver &operator=( const COMClassDriver &src );
	// create a com object with given prog id
	bool CreateObject ( LPCTSTR strProgID, DWORD dwClsContext = CLSCTX_ALL );
	// create a com object with given class id
	bool CreateObject ( CLSID clsid, DWORD dwClsContext = CLSCTX_ALL );
	// attach a IDispatch pointer to the obejct
	bool Attach ( IDispatch *pDisp );
	// return the IDispatch pointer
	IDispatch *GetDispatch ( )
	{
		return m_pDisp;
	}
// return the pointer to ith COMClassInfo 
	COMClassInfo *GetDispInfo ( const int i )
	{
		return (i >= 0 && i < m_nDispInfoCount)?(m_pDispInfo + i):NULL;
	}
// return the index of a property in the internal storage
	int FindProperty ( LPCTSTR strPropertyName, bool set = true );
	// return the index of a method in the internal storage
	int FindMethod ( LPCTSTR strMethodName );
	// get the type of a property by name
	WORD GetPropertyType ( LPCTSTR strPropertyName );
	// get the type of a property by index
	WORD GetPropertyType ( int nPropertyIndex );
	// get a property value by name
	VARIANT *GetProperty ( LPCTSTR strPropertyName );
	// get a property value by index
	VARIANT *GetProperty ( int nPropertyIndex );
	// get return type of a method by name
	WORD GetReturnType ( LPCTSTR strMethodName );
	// get return type of a method by index
	WORD GetReturnType ( int nMethodIndex );
	// get number of parameters in a method by name
	int GetParamCount ( LPCTSTR strMethodName );
	// get number of parameters in a method by index
	int GetParamCount ( int nMethodIndex );
	// get the type of a parameter in a method by name
	WORD GetParamType ( LPCTSTR strMethodName, const int nParamIndex );
	// get the type of a parameter in a method by index
	WORD GetParamType ( int nMethodIndex, const int nParamIndex );
	// invoke a method by name and pass 0 or 1 VAR param
	VARIANT *PropertyPut ( vmInstance *instance, LPCTSTR strMethodName, VAR *val );
	// invoke a method by name and pass 0 or more VAR parameters
	VARIANT *InvokeMethod ( vmInstance *instance, LPCTSTR strPropertyName, nParamType params );
	// install even handler
	bool Advise ( IUnknown *pUnkSink, REFIID riid );
	// remove event handler
	void Unadvise ( );
	// get the last error code as HRESULT
	HRESULT GetLastError ( )
	{
		return m_hRet;
	}
// get exception info
	EXCEPINFO *GetExceptionInfo ( )
	{
		return m_pExceptInfo;
	}
};

void CharToWChar ( WCHAR *pTarget, const char *pSource, int len )
{
	while ( len-- )
	{
		*pTarget = *pSource;
		pSource++;
		pTarget++;
	}
	*pTarget = 0;
}

void CharToWChar ( WCHAR *pTarget, const char *pSource )
{
	while ( *pSource )
	{
		*pTarget = *pSource;
		pSource++;
		pTarget++;
	}
	*pTarget = 0;
}

void WCharToChar ( WCHAR *pSource, BSTR pTarget, int len )
{
	while ( len-- )
	{
		*pTarget = (char) *pSource;
		pSource++;
		pTarget++;
	}
	*pTarget = 0;
}

void WCharToChar ( WCHAR *pSource, BSTR pTarget )
{
	while ( *pSource )
	{
		*pTarget = (char) *pSource;
		pSource++;
		pTarget++;
	}
	*pTarget = 0;
}

COMClassInfo::COMClassInfo ( )
{
}

COMClassInfo::~COMClassInfo ( )
{
	if ( m_bstrName != NULL )
	{
		::SysFreeString ( m_bstrName );
	}
	if ( m_pOutput != NULL )
	{
		::VariantClear ( m_pOutput );
	}
	delete m_pOutput;
	delete[]m_pParamTypes;
}

COMClassInfo &COMClassInfo::operator=( const COMClassInfo &src )
{
	if ( &src == this ) return *this;
	m_dispID = src.m_dispID;
	if ( m_bstrName ) ::SysFreeString ( m_bstrName );
	if ( src.m_bstrName ) m_bstrName = ::SysAllocString ( src.m_bstrName );
	else m_bstrName = NULL;
	m_wFlag = src.m_wFlag;
	m_oVft = src.m_oVft;
	m_callconv = src.m_callconv;
	m_vtOutputType = src.m_vtOutputType;
	if ( m_pOutput ) delete m_pOutput;
	m_pOutput = new VARIANT;
	::VariantInit ( m_pOutput );
	m_nParamCount = src.m_nParamCount;
	if ( m_pParamTypes ) delete[]m_pParamTypes;
	if ( m_nParamCount > 0 )
	{
		m_pParamTypes = new WORD[m_nParamCount + 1];
		memcpy ( m_pParamTypes, src.m_pParamTypes, (static_cast<size_t>(m_nParamCount) + 1) * sizeof ( WORD ) );
	} else m_pParamTypes = NULL;
	return *this;
}

COMClassDriver::COMClassDriver ( )
{
}

COMClassDriver::~COMClassDriver ( )
{
	Clear ( );
}

COMClassDriver::COMClassDriver ( const COMClassDriver &src )
{
	*this = src;
}

COMClassDriver &COMClassDriver::operator=( const COMClassDriver &src )
{
	if ( &src == this ) return *this;
	Clear ( );
	m_pDisp = src.m_pDisp;
	if ( m_pDisp )
	{
		m_pDisp->AddRef ( );
		if ( src.m_nDispInfoCount > 0 )
		{
			m_nDispInfoCount = src.m_nDispInfoCount;
			m_pDispInfo = new COMClassInfo[m_nDispInfoCount];
			for ( int i = 0; i < m_nDispInfoCount; i++ ) m_pDispInfo[i] = src.m_pDispInfo[i];
		}
	}
	return *this;
}

void COMClassDriver::Clear ( )
{
	m_hRet = S_OK;
	Unadvise ( );
	if ( m_pExceptInfo != NULL )
	{
		::SysFreeString ( m_pExceptInfo->bstrSource );
		::SysFreeString ( m_pExceptInfo->bstrDescription );
		::SysFreeString ( m_pExceptInfo->bstrHelpFile );
		delete m_pExceptInfo;
		m_pExceptInfo = NULL;
	}
	if ( m_pDisp != NULL )
	{
		m_pDisp->Release ( );
		m_pDisp = NULL;
	}
	if ( m_pDispInfo )
	{
		delete[]m_pDispInfo;
		m_pDispInfo = NULL;
	}
	m_nVarCount = 0;
	m_nMethodCount = 0;
	m_nDispInfoCount = 0;
}

bool COMClassDriver::CreateObject ( LPCTSTR strProgID, DWORD dwClsContext )
{
	Clear ( );
	CLSID clsid;
#ifdef _UNICODE
	WCHAR *pProgID = (WCHAR *) strProgID;
#else
	WCHAR pProgID[COMClassDRIVER_OLENAMELEN + 1];
	CharToWChar ( pProgID, strProgID );
#endif
	m_hRet = ::CLSIDFromProgID ( pProgID, &clsid );
	if ( m_hRet == S_OK )
	{
		return CreateObject ( clsid, dwClsContext );
	} else
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "CLSIDFromProgID failed: %x\n" ), m_hRet );
#endif
		return false;
	}
}

bool COMClassDriver::CreateObject ( CLSID clsid, DWORD dwClsContext )
{
	Clear ( );
	m_hRet = ::CoCreateInstance ( clsid, NULL, dwClsContext, IID_IDispatch, (void **) (&m_pDisp) );
	if ( m_hRet != S_OK )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "CoCreateInstance failed: %x\n" ), m_hRet );
#endif
		return false;
	}

	return LoadTypeInfo ( );
}

bool COMClassDriver::LoadTypeInfo ( )
{
	/*UINT nTypeInfoCount;
	m_hRet = m_pDisp->GetTypeInfoCount(&nTypeInfoCount);
	if(m_hRet!=S_OK||nTypeInfoCount==0)
	{
		#ifdef COMClassDRIVER_DEBUG
		_tprintf(_T("GetTypeInfoCount failed or no type info: %x\n"),m_hRet);
		#endif
	}*/

	ITypeInfo *pTypeInfo;
	m_hRet = m_pDisp->GetTypeInfo ( 0, LOCALE_SYSTEM_DEFAULT, &pTypeInfo );
	if ( m_hRet != S_OK || pTypeInfo == nullptr )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "GetTypeInfo failed: %x\n" ), m_hRet );
#endif
		return false;
	}

	TYPEATTR *pTypeAttr;
	m_hRet = pTypeInfo->GetTypeAttr ( &pTypeAttr );
	if ( m_hRet != S_OK )
	{
		pTypeInfo->Release ( );
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "GetTypeAttr failed: %x\n" ), m_hRet );
#endif
		return false;
	}

	if ( pTypeAttr->typekind != TKIND_DISPATCH && pTypeAttr->typekind != TKIND_COCLASS )
	{
		pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
		pTypeInfo->Release ( );
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Cannot get type info\n" ) );
#endif
		m_hRet = S_FALSE;
		return false;
	}

	if ( pTypeAttr->typekind == TKIND_COCLASS )
	{
		int nFlags;
		HREFTYPE hRefType;
		ITypeInfo *pTempInfo = nullptr;
		TYPEATTR *pTempAttr = nullptr;
		for ( int i = 0; i < pTypeAttr->cImplTypes; i++ )
		{
			if ( pTypeInfo->GetImplTypeFlags ( i, &nFlags ) == S_OK && (nFlags & IMPLTYPEFLAG_FDEFAULT) )
			{
				m_hRet = pTypeInfo->GetRefTypeOfImplType ( i, &hRefType );
				if ( m_hRet == S_OK ) m_hRet = pTypeInfo->GetRefTypeInfo ( hRefType, &pTempInfo );
				if ( m_hRet == S_OK && pTempInfo )
				{
					m_hRet = pTempInfo->GetTypeAttr ( &pTempAttr );
					if ( m_hRet != S_OK )
					{
						pTempInfo->Release ( );
						pTempInfo = NULL;
						break;
					}
				} else break;
			}
		}
		pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
		pTypeInfo->Release ( );
		if ( pTempAttr == nullptr || pTempInfo == nullptr )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "Failed to get reference type info: %x\n" ), m_hRet );
#endif
			if ( m_hRet == S_OK ) m_hRet = S_FALSE;
			return false;
		} else
		{
			pTypeInfo = pTempInfo;
			pTypeAttr = pTempAttr;
		}
	}

	m_nMethodCount = pTypeAttr->cFuncs;
	m_nVarCount = pTypeAttr->cVars;
	m_nDispInfoCount = m_nMethodCount + 2 * m_nVarCount;
#ifdef COMClassDRIVER_DEBUG
	_tprintf ( _T ( "Method and variable count = %d\n" ), m_nMethodCount + m_nVarCount );
#endif
	m_pDispInfo = new COMClassInfo[m_nDispInfoCount];

	for ( int i = 0; i < m_nMethodCount; i++ )
	{
		FUNCDESC *pFuncDesc;
		m_hRet = pTypeInfo->GetFuncDesc ( i, &pFuncDesc );
		if ( m_hRet != S_OK )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "GetFuncDesc failed: %x\n" ), m_hRet );
#endif
			pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
			pTypeInfo->Release ( );
			m_nMethodCount = m_nVarCount = m_nDispInfoCount = 0;
			return false;
		}
		m_pDispInfo[i].m_dispID = pFuncDesc->memid;
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "%d: DispID = %d\n" ), i, m_pDispInfo[i].m_dispID );
#endif

		unsigned int nCount;
		m_hRet = pTypeInfo->GetNames ( m_pDispInfo[i].m_dispID, &m_pDispInfo[i].m_bstrName, 1, &nCount );
		if ( m_hRet != S_OK )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "GetNames failed: %x\n" ), m_hRet );
#endif
			pTypeInfo->ReleaseFuncDesc ( pFuncDesc );
			pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
			pTypeInfo->Release ( );
			m_nMethodCount = m_nVarCount = m_nDispInfoCount = 0;
			return false;
		}
#ifdef COMClassDRIVER_DEBUG
		wprintf ( L"MethodName = %s\n", m_pDispInfo[i].m_bstrName );
#endif

		switch ( pFuncDesc->invkind )
		{
			case INVOKE_PROPERTYGET:
				m_pDispInfo[i].m_wFlag = DISPATCH_PROPERTYGET;
#ifdef COMClassDRIVER_DEBUG
				_tprintf ( _T ( "PropertyGet\n" ) );
#endif
				break;
			case INVOKE_PROPERTYPUT:
				m_pDispInfo[i].m_wFlag = DISPATCH_PROPERTYPUT;
#ifdef COMClassDRIVER_DEBUG
				_tprintf ( _T ( "PropertyPut\n" ) );
#endif
				break;
			case INVOKE_PROPERTYPUTREF:
				m_pDispInfo[i].m_wFlag = DISPATCH_PROPERTYPUTREF;
#ifdef COMClassDRIVER_DEBUG
				_tprintf ( _T ( "PropertyPutRef\n" ) );
#endif
				break;
			case INVOKE_FUNC:
#ifdef COMClassDRIVER_DEBUG
				_tprintf ( _T ( "DispatchMethod\n" ) );
#endif
				m_pDispInfo[i].m_wFlag = DISPATCH_METHOD;
				break;
			default:
				break;
		}

		m_pDispInfo[i].m_oVft = pFuncDesc->oVft;
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "VTable offset: %d\n" ), m_pDispInfo[i].m_oVft );
#endif

		m_pDispInfo[i].m_callconv = pFuncDesc->callconv;
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Calling convention: %d\n" ), m_pDispInfo[i].m_callconv );
#endif

		m_pDispInfo[i].m_pOutput = new VARIANT;
		::VariantInit ( m_pDispInfo[i].m_pOutput );
		m_pDispInfo[i].m_vtOutputType = pFuncDesc->elemdescFunc.tdesc.vt;
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Return type = %d\n" ), m_pDispInfo[i].m_vtOutputType );
#endif
		if ( m_pDispInfo[i].m_vtOutputType == VT_VOID || m_pDispInfo[i].m_vtOutputType == VT_NULL )
		{
			m_pDispInfo[i].m_vtOutputType = VT_EMPTY;
		}

		m_pDispInfo[i].m_nParamCount = pFuncDesc->cParams;
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "ParamCount = %d\n" ), m_pDispInfo[i].m_nParamCount );
#endif

		m_pDispInfo[i].m_pParamTypes = new WORD[m_pDispInfo[i].m_nParamCount + 1];
		for ( int j = 0; j < m_pDispInfo[i].m_nParamCount; j++ )
		{
#if 1
			if ( pFuncDesc->lprgelemdescParam[j].tdesc.vt == VT_PTR || pFuncDesc->lprgelemdescParam[j].tdesc.vt == VT_SAFEARRAY )
			{
				m_pDispInfo[i].m_pParamTypes[j] = (pFuncDesc->lprgelemdescParam[j].tdesc.lptdesc->vt) | VT_BYREF;
			} else
			{
				m_pDispInfo[i].m_pParamTypes[j] = pFuncDesc->lprgelemdescParam[j].tdesc.vt;
			}
#else
			TYPEDESC *typeDesc;

			if ( pFuncDesc->lprgelemdescParam[j].tdesc.vt == VT_PTR || pFuncDesc->lprgelemdescParam[j].tdesc.vt == VT_SAFEARRAY )
			{
				typeDesc = pFuncDesc->lprgelemdescParam[j].tdesc.lptdesc;
				while ( typeDesc->vt == VT_PTR || typeDesc->vt == VT_SAFEARRAY )
				{
					typeDesc = typeDesc->lptdesc;
				}
				m_pDispInfo[i].m_pParamTypes[j] = typeDesc->vt;

				if ( m_pDispInfo[i].m_pParamTypes[j] == VT_USERDEFINED )
				{
					ITypeInfo *pTypeInfo2;

					pTypeInfo->GetRefTypeInfo ( pFuncDesc->lprgelemdescParam[0].tdesc.lptdesc->lptdesc->hreftype, &pTypeInfo2 );

					m_hRet = m_pDisp->GetTypeInfo ( 0, LOCALE_SYSTEM_DEFAULT, &pTypeInfo2 );
					if ( m_hRet != S_OK || pTypeInfo2 == NULL )
					{
#ifdef COMClassDRIVER_DEBUG
						_tprintf ( _T ( "GetTypeInfo failed: %x\n" ), m_hRet );
#endif
						return false;
					}

					TYPEATTR *pTypeAttr;
					m_hRet = pTypeInfo2->GetTypeAttr ( &pTypeAttr );
					if ( m_hRet != S_OK )
					{
						pTypeInfo->Release ( );
#ifdef COMClassDRIVER_DEBUG
						_tprintf ( _T ( "GetTypeAttr failed: %x\n" ), m_hRet );
#endif
						return false;
					}

					if ( pTypeAttr->typekind != TKIND_DISPATCH && pTypeAttr->typekind != TKIND_COCLASS )
					{
						pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
						pTypeInfo->Release ( );
#ifdef COMClassDRIVER_DEBUG
						_tprintf ( _T ( "Cannot get type info\n" ) );
#endif
						m_hRet = S_FALSE;
						return false;
					}


				}
			} else
			{
				m_pDispInfo[i].m_pParamTypes[j] = pFuncDesc->lprgelemdescParam[j].tdesc.vt;
			}

#endif
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "Param(%d) type = %d\n" ), j, m_pDispInfo[i].m_pParamTypes[j] );
#endif
		}
		m_pDispInfo[i].m_pParamTypes[m_pDispInfo[i].m_nParamCount] = 0;
		pTypeInfo->ReleaseFuncDesc ( pFuncDesc );
	}

	for ( int i = m_nMethodCount; i < m_nMethodCount + m_nVarCount; i++ )
	{
		VARDESC *pVarDesc;
		m_hRet = pTypeInfo->GetVarDesc ( i - m_nMethodCount, &pVarDesc );
		if ( m_hRet != S_OK )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "GetVarDesc failed: %x\n" ), m_hRet );
#endif
			pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
			pTypeInfo->Release ( );
			m_nMethodCount = m_nVarCount = m_nDispInfoCount = 0;
			return false;
		}
		m_pDispInfo[i].m_dispID = pVarDesc->memid;
		m_pDispInfo[i + m_nVarCount].m_dispID = m_pDispInfo[i].m_dispID;
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "%d: DispID = %d\n" ), i, m_pDispInfo[i].m_dispID );
#endif

		unsigned int nCount;
		m_hRet = pTypeInfo->GetNames ( m_pDispInfo[i].m_dispID, &m_pDispInfo[i].m_bstrName, 1, &nCount );
		if ( m_hRet != S_OK )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "GetNames failed: %x\n" ), m_hRet );
#endif
			pTypeInfo->ReleaseVarDesc ( pVarDesc );
			pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
			pTypeInfo->Release ( );
			m_nMethodCount = m_nVarCount = m_nDispInfoCount = 0;
			return false;
		}
		m_pDispInfo[i + m_nVarCount].m_bstrName = ::SysAllocString ( m_pDispInfo[i].m_bstrName );
#ifdef COMClassDRIVER_DEBUG
		wprintf ( L"VarName = %s\n", m_pDispInfo[i].m_bstrName );
#endif

		switch ( pVarDesc->varkind )
		{
			case VAR_DISPATCH:
				m_pDispInfo[i].m_wFlag = DISPATCH_PROPERTYGET;
				m_pDispInfo[i + m_nVarCount].m_wFlag = DISPATCH_PROPERTYPUT;
#ifdef COMClassDRIVER_DEBUG
				_tprintf ( _T ( "VarKind = VAR_DISPATCH\n" ) );
#endif
				m_pDispInfo[i].m_vtOutputType = pVarDesc->elemdescVar.tdesc.vt;
				m_pDispInfo[i + m_nVarCount].m_vtOutputType = VT_EMPTY;
#ifdef COMClassDRIVER_DEBUG
				_tprintf ( _T ( "VarType = %d\n" ), m_pDispInfo[i].m_vtOutputType );
#endif
				m_pDispInfo[i + m_nVarCount].m_nParamCount = 1;
				m_pDispInfo[i + m_nVarCount].m_pParamTypes = new WORD[2];
				m_pDispInfo[i + m_nVarCount].m_pParamTypes[0] = m_pDispInfo[i].m_vtOutputType;
				m_pDispInfo[i + m_nVarCount].m_pParamTypes[1] = 0;
				break;
			default:
#ifdef COMClassDRIVER_DEBUG
				_tprintf ( _T ( "VarKind = %d\n" ), pVarDesc->varkind );
#endif
				m_pDispInfo[i].m_wFlag = 0;
				m_pDispInfo[i + m_nVarCount].m_wFlag = 0;
				break;
		}
		m_pDispInfo[i].m_pOutput = new VARIANT;
		::VariantInit ( m_pDispInfo[i].m_pOutput );
		m_pDispInfo[i + m_nVarCount].m_pOutput = new VARIANT;
		::VariantInit ( m_pDispInfo[i + m_nVarCount].m_pOutput );
		pTypeInfo->ReleaseVarDesc ( pVarDesc );
	}

	pTypeInfo->ReleaseTypeAttr ( pTypeAttr );
	pTypeInfo->Release ( );
	return true;
}

bool COMClassDriver::Attach ( IDispatch *pDisp )
{
	Clear ( );
	if ( pDisp == NULL )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Invalid dipatch pointer" ), m_hRet );
#endif
		return false;
	}
	m_pDisp = pDisp;
	m_pDisp->AddRef ( );
	return LoadTypeInfo ( );
}

int COMClassDriver::FindProperty ( LPCTSTR strPropertyName, bool set )
{
	return FindDispInfo ( strPropertyName, set?DISPATCH_PROPERTYPUT:DISPATCH_PROPERTYGET );
}

int COMClassDriver::FindMethod ( LPCTSTR strMethodName )
{
	return FindDispInfo ( strMethodName );
}

WORD COMClassDriver::GetPropertyType ( LPCTSTR strPropertyName )
{
	int nPropertyIndex = FindProperty ( strPropertyName );
	return GetPropertyType ( nPropertyIndex );
}

WORD COMClassDriver::GetPropertyType ( int nPropertyIndex )
{
	if ( nPropertyIndex >= 0 ) return m_pDispInfo[nPropertyIndex].m_vtOutputType;
	else return VT_EMPTY;
}

VARIANT *COMClassDriver::GetProperty ( LPCTSTR strPropertyName )
{
	int nPropertyIndex = FindProperty ( strPropertyName, false );
	if ( nPropertyIndex >= 0 )
	{
		return GetProperty ( nPropertyIndex );
	}
	return NULL;
}

VARIANT *COMClassDriver::GetProperty ( int nPropertyIndex  )
{
	if ( m_pDispInfo[nPropertyIndex].m_wFlag == DISPATCH_PROPERTYGET )
	{
		auto m_hRet = InvokeMethodV ( nPropertyIndex );
		return m_hRet == S_OK?m_pDispInfo[nPropertyIndex].m_pOutput:NULL;
	}
	return NULL;
}

WORD COMClassDriver::GetReturnType ( LPCTSTR strMethodName )
{
	int nMethodIndex = FindMethod ( strMethodName );
	return GetReturnType ( nMethodIndex );
}

WORD COMClassDriver::GetReturnType ( int nMethodIndex )
{
	if ( nMethodIndex >= 0 ) return m_pDispInfo[nMethodIndex].m_vtOutputType;
	else return VT_EMPTY;
}

int COMClassDriver::GetParamCount ( LPCTSTR strMethodName )
{
	int nMethodIndex = FindMethod ( strMethodName );
	return GetParamCount ( nMethodIndex );
}

int COMClassDriver::GetParamCount ( int nMethodIndex )
{
	if ( nMethodIndex >= 0 ) return m_pDispInfo[nMethodIndex].m_nParamCount;
	else return -1;
}

WORD COMClassDriver::GetParamType ( LPCTSTR strMethodName, const int nParamIndex )
{
	int nMethodIndex = FindMethod ( strMethodName );
	return GetParamType ( nMethodIndex, nParamIndex );
}

WORD COMClassDriver::GetParamType ( int nMethodIndex, const int nParamIndex )
{
	if ( nMethodIndex >= 0 && nParamIndex >= 0 && nParamIndex < m_pDispInfo[nMethodIndex].m_nParamCount )
	{
		return m_pDispInfo[nMethodIndex].m_pParamTypes[nParamIndex];
	} else return VT_EMPTY;
}

int COMClassDriver::FindDispInfo ( const LPCTSTR strName, const WORD wFlag )
{
#ifdef _UNICODE
	WCHAR *pName = (WCHAR *) strName;
#else
	WCHAR pName[COMClassDRIVER_OLENAMELEN + 1];
	CharToWChar ( pName, strName );
#endif
	int nRet = -1;
	for ( int i = 0; i < m_nDispInfoCount; i++ )
	{
		if ( _wcsicmp ( pName, m_pDispInfo[i].m_bstrName ) == 0 && m_pDispInfo[i].m_wFlag == wFlag )
		{
			nRet = i;
			break;
		}
	}
	return nRet;
}

void VarToVariant ( vmInstance *instance, VAR *val, VARIANT *pArg, WORD pb, void *byRefPtr )
{
	int64_t			 iVal;
	double			 dVal;
	_variant_t		 var;
	BSTR *tmpBstr;

	while ( TYPE ( val ) == slangType::eREFERENCE )
	{
		val = val->dat.ref.v;
	}

	switch ( pArg->vt )
	{
		case VT_UI1:
		case VT_UI2:
		case VT_UI4:
		case VT_UI8:
		case VT_I1:
		case VT_I2:
		case VT_I4:
		case VT_I8:
		case VT_BOOL:
			switch ( TYPE ( val ) )
			{
				case slangType::eSTRING:
					iVal = atoll ( val->dat.str.c );
					break;
				case slangType::eLONG:
					iVal = val->dat.l;
					break;
				case slangType::eDOUBLE:
					iVal = (int64_t) val->dat.d;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			switch ( pArg->vt )
			{
				case VT_UI1:
					pArg->bVal = (BYTE) iVal;
					break;
				case VT_UI2:
					pArg->uiVal = (USHORT) iVal;
					break;
				case VT_UI4:
					pArg->ulVal = (ULONG) iVal;
					break;
				case VT_UI8:
					pArg->ullVal = (ULONGLONG)iVal;
					break;
				case VT_I1:
					pArg->cVal= (CHAR) iVal;
					break;
				case VT_I2:
					pArg->iVal = (SHORT) iVal;
					break;
				case VT_I4:
					pArg->lVal = (LONG)iVal;;
					break;
				case VT_I8:
					pArg->llVal = (LONGLONG)iVal;;
					break;
				case VT_BOOL:
					pArg->boolVal = iVal?-1:0;
					break;
			}
			break;
		case VT_R4:
		case VT_R8:
			switch ( TYPE ( val ) )
			{
				case slangType::eSTRING:
					dVal = atof ( val->dat.str.c );
					break;
				case slangType::eLONG:
					dVal = (double) val->dat.l;
					break;
				case slangType::eDOUBLE:
					dVal = val->dat.d;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			switch ( pArg->vt )
			{
				case VT_R4:
					pArg->fltVal = (float) dVal;
					break;
				case VT_R8:
					pArg->dblVal = (double) dVal;
					break;
			}
			break;
		case VT_VARIANT:
			switch ( TYPE ( val ) )
			{
				case slangType::eSTRING:
					WCHAR *pData;
					pData = (WCHAR *) _alloca ( sizeof ( WCHAR ) * (val->dat.str.len + 1) );
					CharToWChar ( pData, val->dat.str.c, (int)val->dat.str.len );
					pArg->vt = VT_BSTR;
					pArg->bstrVal = ::SysAllocString ( pData );
					break;
				case slangType::eLONG:
					pArg->vt = VT_I8;
					pArg->llVal = val->dat.l;
					break;
				case slangType::eDOUBLE:
					pArg->vt = VT_R8;
					pArg->dblVal = val->dat.d;
					break;
				default:
					break;
			}
			break;
		case VT_I1 | VT_BYREF:
		case VT_I2 | VT_BYREF:
		case VT_I4 | VT_BYREF:
		case VT_I8 | VT_BYREF:
		case VT_UI1 | VT_BYREF:
		case VT_UI2 | VT_BYREF:
		case VT_UI4 | VT_BYREF:
		case VT_UI8 | VT_BYREF:
		case VT_BOOL | VT_BYREF:
			switch ( TYPE ( val ) )
			{
				case slangType::eSTRING:
					iVal = atoll ( val->dat.str.c );
					break;
				case slangType::eLONG:
					iVal = val->dat.l;
					break;
				case slangType::eDOUBLE:
					iVal = (unsigned long) val->dat.d;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}

			switch ( pArg->vt )
			{
				case VT_I1 | VT_BYREF:
					pArg->pbVal = (BYTE *) byRefPtr;
					*pArg->pbVal = (BYTE) iVal;
					break;
				case VT_I2 | VT_BYREF:
					pArg->piVal = (SHORT *) byRefPtr;
					*pArg->piVal = (SHORT) iVal;
					break;
				case VT_I4 | VT_BYREF:
					pArg->plVal = (LONG *) byRefPtr;
					*pArg->plVal = (LONG) iVal;;
					break;
				case VT_I8 | VT_BYREF:
					pArg->pllVal = (LONGLONG *) byRefPtr;
					*pArg->pllVal = (LONGLONG) iVal;;
					break;
				case VT_UI1 | VT_BYREF:
					pArg->pcVal = (CHAR *) byRefPtr;
					*pArg->pcVal = (CHAR) iVal;
					break;
				case VT_UI2 | VT_BYREF:
					pArg->puiVal = (USHORT *) byRefPtr;
					*pArg->puiVal = (USHORT) iVal;
					break;
				case VT_UI4 | VT_BYREF:
					pArg->pulVal = (ULONG *) byRefPtr;
					*pArg->pulVal = (ULONG) iVal;;
					break;
				case VT_UI8 | VT_BYREF:
					pArg->pullVal = (ULONGLONG *) byRefPtr;
					*pArg->pullVal = (ULONGLONG) iVal;;
					break;
				case VT_BOOL | VT_BYREF:
					pArg->pboolVal = (VARIANT_BOOL *) byRefPtr;
					*pArg->pboolVal = iVal?-1:0;
					break;
			}
			break;
		case VT_R4 | VT_BYREF:
		case VT_R8 | VT_BYREF:
			switch ( TYPE ( val ) )
			{
				case slangType::eSTRING:
					dVal = atof ( val->dat.str.c );
					break;
				case slangType::eLONG:
					dVal = (double) val->dat.l;
					break;
				case slangType::eDOUBLE:
					dVal = val->dat.d;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			switch ( pArg->vt )
			{
				case VT_R4 | VT_BYREF:
					pArg->pfltVal = (float *) byRefPtr;
					*(float *) pArg->pfltVal = (float) dVal;
					break;
				case VT_R8 | VT_BYREF:
					pArg->pdblVal = (double *) byRefPtr;
					*(double *) pArg->pdblVal = (double) dVal;
					break;
			}
			break;
		case VT_VARIANT | VT_BYREF:
			pArg->pvarVal = (VARIANT *) byRefPtr;

			VariantInit ( pArg->pvarVal );
			switch ( TYPE ( val ) )
			{
				case slangType::eSTRING:
					WCHAR *pData;
					pData = (WCHAR *) _alloca ( sizeof ( WCHAR ) * (val->dat.str.len + 1) );
					CharToWChar ( pData, val->dat.str.c, (int)val->dat.str.len );
					pArg->pvarVal->vt = VT_BSTR;
					pArg->pvarVal->bstrVal = ::SysAllocString ( pData );
					break;
				case slangType::eLONG:
					pArg->pvarVal->vt = VT_I8;
					pArg->pvarVal->llVal = val->dat.l;
					break;
				case slangType::eDOUBLE:
					pArg->pvarVal->vt = VT_R8;
					pArg->pvarVal->dblVal = val->dat.d;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case VT_BSTR:
			{
				switch ( TYPE ( val ) )
				{
					case slangType::eSTRING:
						WCHAR *pData;
						pData = (WCHAR *) _alloca ( sizeof ( WCHAR ) * (val->dat.str.len + 1) );
						CharToWChar ( pData, val->dat.str.c, (int) val->dat.str.len );
						pArg->bstrVal = ::SysAllocString ( pData );
						break;
					default:
						throw errorNum::scINVALID_PARAMETER;
				}
			}
			break;
		case VT_BSTR | VT_BYREF:
			{
				switch ( TYPE ( val ) )
				{
					case slangType::eSTRING:
						var.Attach ( *pArg );
						var = val->dat.str.c;

						tmpBstr = (BSTR *) byRefPtr;
						*tmpBstr = var.bstrVal;

						WCHAR *pData;
						pData = (WCHAR *) _alloca ( sizeof ( WCHAR ) * (val->dat.str.len + 1) );
						CharToWChar ( pData, val->dat.str.c, (int) val->dat.str.len );
						*tmpBstr = ::SysAllocString ( pData );

						var.Detach ( );
						pArg->vt = pb; // set the variant type
						pArg->pbstrVal = tmpBstr;
						break;
					default:
						throw errorNum::scINVALID_PARAMETER;
				}
			}
			break;
		case VT_DISPATCH:
		case VT_USERDEFINED:
			if ( TYPE ( val ) == slangType::eOBJECT )
			{
				if ( !strccmp ( val->dat.obj.classDef->name, "COM" ) )
				{
					// this is a com object... hump... we can pass this as a dispatch type!
					VAR *var = classIVarAccess ( val, "OBJ" );
					auto cDriver = (COMClassDriver *) var->dat.str.c;
					pArg->pdispVal = cDriver->GetDispatch ( );
				} else
				{
					throw errorNum::scINVALID_PARAMETER;
				}
			} else
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case VT_DISPATCH | VT_BYREF:
		case VT_USERDEFINED | VT_BYREF:
			if ( TYPE ( val ) == slangType::eOBJECT )
			{
				if ( !strccmp ( val->dat.obj.classDef->name, "COM" ) )
				{
					// this is a com object... hump... we can pass this as a dispatch type!
					VAR *var = classIVarAccess ( val, "OBJ" );
					auto cDriver = (COMClassDriver *) var->dat.str.c;

					pArg->vt = VT_DISPATCH;
					pArg->ppdispVal = (IDispatch **) byRefPtr;
					*(pArg->ppdispVal) = cDriver->GetDispatch ( );
				} else
				{
					throw errorNum::scINVALID_PARAMETER;
				}
			} else
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}


HRESULT COMClassDriver::InvokeMethodV ( int nIndex )
{
	m_hRet = S_OK;

	if ( m_pDispInfo[nIndex].m_wFlag == 0 )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Invalid invokation flag\n" ) );
#endif
		m_hRet = S_FALSE;
		return m_hRet;
	}

	DISPPARAMS dispparams;
	memset ( &dispparams, 0, sizeof dispparams );
	dispparams.cArgs = m_pDispInfo[nIndex].m_nParamCount;

	DISPID dispidNamed = DISPID_PROPERTYPUT;
	if ( m_pDispInfo[nIndex].m_wFlag & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF) )
	{
		dispparams.cNamedArgs = 1;
		dispparams.rgdispidNamedArgs = &dispidNamed;
	}

	if ( dispparams.cArgs != 0 )
	{
		throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
	}

	// initialize return value
	VARIANT *pvarResult = m_pDispInfo[nIndex].m_pOutput;
	::VariantClear ( pvarResult );

	// initialize EXCEPINFO struct
	if ( m_pExceptInfo != NULL )
	{
		::SysFreeString ( m_pExceptInfo->bstrSource );
		::SysFreeString ( m_pExceptInfo->bstrDescription );
		::SysFreeString ( m_pExceptInfo->bstrHelpFile );
		delete m_pExceptInfo;
	}
	m_pExceptInfo = new EXCEPINFO;
	memset ( m_pExceptInfo, 0, sizeof ( *m_pExceptInfo ) );

	UINT nArgErr = (UINT) -1;  // initialize to invalid arg

	// make the call
	m_hRet = m_pDisp->Invoke ( m_pDispInfo[nIndex].m_dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT, m_pDispInfo[nIndex].m_wFlag, &dispparams, pvarResult, m_pExceptInfo, &nArgErr );

	// cleanup any arguments that need cleanup
	if ( dispparams.cArgs != 0 )
	{
		VARIANT *pArg = dispparams.rgvarg + dispparams.cArgs - 1;
		const WORD *pb = m_pDispInfo[nIndex].m_pParamTypes;
		while ( *pb != 0 )
		{
			switch ( *pb )
			{
				case VT_BSTR:
					::SysFreeString ( pArg->bstrVal );
					break;
			}
			--pArg;
			++pb;
		}
	}
	delete[] dispparams.rgvarg;

	// throw exception on failure
	if ( m_hRet < 0 )
	{
		::VariantClear ( m_pDispInfo[nIndex].m_pOutput );
		if ( m_hRet != DISP_E_EXCEPTION )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "Invoke failed, no exception: %x\n" ), m_hRet );
#endif
			delete m_pExceptInfo;
			m_pExceptInfo = NULL;
			return m_hRet;
		}

		// make sure excepInfo is filled in
		if ( m_pExceptInfo->pfnDeferredFillIn != NULL )
			m_pExceptInfo->pfnDeferredFillIn ( m_pExceptInfo );

#ifdef COMClassDRIVER_DEBUG
		wprintf ( L"Exception source: %s\n", m_pExceptInfo->bstrSource );
		wprintf ( L"Exception description: %s\n", m_pExceptInfo->bstrDescription );
		wprintf ( L"Exception help file: %s\n", m_pExceptInfo->bstrHelpFile );
#endif
		return m_hRet;
	}

	if ( m_pDispInfo[nIndex].m_vtOutputType != VT_EMPTY )
	{
//		m_pDispInfo[nIndex].m_pOutput->vt = m_pDispInfo[nIndex].m_vtOutputType;
	}

	delete m_pExceptInfo;
	m_pExceptInfo = NULL;
	return m_hRet;
}

bool COMClassDriver::Advise ( IUnknown __RPC_FAR *pUnkSink, REFIID riid )
{
	if ( m_pDisp == NULL )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Null dipatch pointer\n" ) );
#endif
		m_hRet = S_FALSE;
		return false;
	}
	Unadvise ( );
	IConnectionPointContainer *pCPContainer = NULL;
	m_hRet = m_pDisp->QueryInterface ( IID_IConnectionPointContainer, (void **) &pCPContainer );
	if ( m_hRet != S_OK )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Failed to call QueryInterface to get the IConnectionPointContainer interface: %x\n" ), m_hRet );
#endif
		return false;
	}
	m_hRet = pCPContainer->FindConnectionPoint ( riid, &m_pCP );
	pCPContainer->Release ( );
	if ( m_hRet != S_OK )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Failed to call FindConnectionPoint to get the IConnectionPoint interface: %x\n" ), m_hRet );
#endif
		return false;
	}
	m_hRet = m_pCP->Advise ( pUnkSink, &m_dwCookie );
	if ( m_hRet != S_OK )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Failed to call Advise: %x\n" ), m_hRet );
#endif
		m_pCP->Release ( );
		m_pCP = NULL;
		return false;
	}
	return true;
}

void COMClassDriver::Unadvise ( )
{
	if ( m_pCP != NULL )
	{
		m_hRet = m_pCP->Unadvise ( m_dwCookie );
		if ( m_hRet != S_OK )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "Failed to call Unadvise: %x\n" ), m_hRet );
#endif
		}
		m_pCP->Release ( );
		m_pCP = NULL;
		m_dwCookie = 0;
	}
}

VARIANT *COMClassDriver::PropertyPut ( vmInstance *instance, LPCTSTR strPropertyName, VAR *val )
{
	_variant_t		 var;

	int nIndex = FindDispInfo ( strPropertyName, DISPATCH_PROPERTYPUT );

	if ( nIndex <= 0 )
	{
		nIndex = FindDispInfo ( strPropertyName, DISPATCH_PROPERTYPUTREF );
		if ( nIndex <= 0 )
		{
			throw errorNum::scUNKNOWN_FUNCTION;
		}
	}

	m_hRet = S_OK;

	if ( m_pDispInfo[nIndex].m_wFlag == 0 )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Invalid invokation flag\n" ) );
#endif
		m_hRet = S_FALSE;
		return 0;
	}

	DISPPARAMS dispparams;
	memset ( &dispparams, 0, sizeof dispparams );
	dispparams.cArgs = m_pDispInfo[nIndex].m_nParamCount;

	DISPID dispidNamed = DISPID_PROPERTYPUT;
	if ( m_pDispInfo[nIndex].m_wFlag & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF) )
	{
		dispparams.cNamedArgs = 1;
		dispparams.rgdispidNamedArgs = &dispidNamed;
	}

	if ( dispparams.cArgs == 1 )
	{
		// allocate memory for all VARIANT parameters
		VARIANT *pArg = new VARIANT[dispparams.cArgs];
		dispparams.rgvarg = pArg;
		memset ( pArg, 0, sizeof ( VARIANT ) * dispparams.cArgs );

		// get ready to walk vararg list
		const WORD *pb = m_pDispInfo[nIndex].m_pParamTypes;
		pArg += dispparams.cArgs - 1;   // params go in opposite order

		while ( *pb != 0 )
		{
			pArg->vt = *pb; // set the variant type
			VarToVariant ( instance, val, pArg, *pb, _alloca ( sizeof ( VARIANT ) ) );
			--pArg; // get ready to fill next argument
			++pb;
		}
	} else
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	// initialize return value
	VARIANT *pvarResult = m_pDispInfo[nIndex].m_pOutput;
	::VariantClear ( pvarResult );

	// initialize EXCEPINFO struct
	if ( m_pExceptInfo != NULL )
	{
		::SysFreeString ( m_pExceptInfo->bstrSource );
		::SysFreeString ( m_pExceptInfo->bstrDescription );
		::SysFreeString ( m_pExceptInfo->bstrHelpFile );
		delete m_pExceptInfo;
	}
	m_pExceptInfo = new EXCEPINFO;
	memset ( m_pExceptInfo, 0, sizeof ( *m_pExceptInfo ) );

	UINT nArgErr = (UINT) -1;  // initialize to invalid arg

	// make the call
	m_hRet = m_pDisp->Invoke ( m_pDispInfo[nIndex].m_dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT, m_pDispInfo[nIndex].m_wFlag, &dispparams, pvarResult, m_pExceptInfo, &nArgErr );

	// cleanup any arguments that need cleanup
	if ( dispparams.cArgs != 0 )
	{
		VARIANT *pArg = dispparams.rgvarg + dispparams.cArgs - 1;
		const WORD *pb = m_pDispInfo[nIndex].m_pParamTypes;
		while ( *pb != 0 )
		{
			switch ( *pb )
			{
				case VT_BSTR:
					::SysFreeString ( pArg->bstrVal );
					break;
				case VT_VARIANT:
					if ( pArg->vt == VT_BSTR )
					{
						::SysFreeString ( pArg->bstrVal );
					}
					break;
			}
			--pArg;
			++pb;
		}
	}
	delete[] dispparams.rgvarg;

	// throw exception on failure
	if ( m_hRet < 0 )
	{
		::VariantClear ( m_pDispInfo[nIndex].m_pOutput );
		if ( m_hRet != DISP_E_EXCEPTION )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "Invoke failed, no exception: %x\n" ), m_hRet );
#endif
			delete m_pExceptInfo;
			m_pExceptInfo = NULL;
			return 0;
		}

		// make sure excepInfo is filled in
		if ( m_pExceptInfo->pfnDeferredFillIn != NULL )
			m_pExceptInfo->pfnDeferredFillIn ( m_pExceptInfo );

#ifdef COMClassDRIVER_DEBUG
		wprintf ( L"Exception source: %s\n", m_pExceptInfo->bstrSource );
		wprintf ( L"Exception description: %s\n", m_pExceptInfo->bstrDescription );
		wprintf ( L"Exception help file: %s\n", m_pExceptInfo->bstrHelpFile );
#endif
		return 0;
	}

#if 0
	if ( m_pDispInfo[nIndex].m_vtOutputType != VT_EMPTY )
	{
		m_pDispInfo[nIndex].m_pOutput->vt = m_pDispInfo[nIndex].m_vtOutputType;
	}
#endif

	delete m_pExceptInfo;
	m_pExceptInfo = NULL;

	return (m_pDispInfo[nIndex].m_pOutput);
}

VARIANT *COMClassDriver::InvokeMethod ( vmInstance *instance, LPCTSTR strMethodName, nParamType params )
{
	VAR *val;
	_variant_t		 var;

	int nIndex = FindDispInfo ( strMethodName );

	if ( nIndex <= 0 )
	{
		throw errorNum::scUNKNOWN_FUNCTION;
	}

	m_hRet = S_OK;

	if ( m_pDispInfo[nIndex].m_wFlag == 0 )
	{
#ifdef COMClassDRIVER_DEBUG
		_tprintf ( _T ( "Invalid invokation flag\n" ) );
#endif
		m_hRet = S_FALSE;
		return 0;
	}

	DISPPARAMS dispparams;
	memset ( &dispparams, 0, sizeof dispparams );
	dispparams.cArgs = m_pDispInfo[nIndex].m_nParamCount;

	DISPID dispidNamed = DISPID_PROPERTYPUT;
	if ( m_pDispInfo[nIndex].m_wFlag & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF) )
	{
		dispparams.cNamedArgs = 1;
		dispparams.rgdispidNamedArgs = &dispidNamed;
	}

	if ( dispparams.cArgs != 0 )
	{
		if ( dispparams.cArgs != (uint32_t) params )
		{
			throw errorNum::scINCORRECT_NUMBER_OF_PARAMETERS;
		}
		// allocate memory for all VARIANT parameters
		VARIANT *pArg = new VARIANT[2ull * dispparams.cArgs];
		dispparams.rgvarg = pArg;
		memset ( pArg, 0, sizeof ( VARIANT ) * dispparams.cArgs * 2 );

		// get ready to walk vararg list
		const WORD *pb = m_pDispInfo[nIndex].m_pParamTypes;
		pArg += dispparams.cArgs - 1;   // params go in opposite order

		for ( size_t loop = 0; loop < params; loop++ )
		{
			auto val = params[loop];
			pArg->vt = *pb; // set the variant type
			VarToVariant ( instance, val, pArg, *pb, _alloca ( sizeof ( VARIANT ) ) );
			--pArg; // get ready to fill next argument
			++pb;
		}
	}

	// initialize return value
	VARIANT *pvarResult = m_pDispInfo[nIndex].m_pOutput;
	::VariantClear ( pvarResult );

	// initialize EXCEPINFO struct
	if ( m_pExceptInfo != NULL )
	{
		::SysFreeString ( m_pExceptInfo->bstrSource );
		::SysFreeString ( m_pExceptInfo->bstrDescription );
		::SysFreeString ( m_pExceptInfo->bstrHelpFile );
		delete m_pExceptInfo;
	}
	m_pExceptInfo = new EXCEPINFO;
	memset ( m_pExceptInfo, 0, sizeof ( *m_pExceptInfo ) );

	UINT nArgErr = (UINT) -1;  // initialize to invalid arg

	// make the call
	try
	{
		m_hRet = m_pDisp->Invoke ( m_pDispInfo[nIndex].m_dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT, m_pDispInfo[nIndex].m_wFlag, &dispparams, pvarResult, m_pExceptInfo, &nArgErr );
	} catch ( ... )
	{
		delete[] dispparams.rgvarg;
		delete m_pExceptInfo;
		m_pExceptInfo = NULL;
		return 0;
	}

	// cleanup any arguments that need cleanup
	if ( dispparams.cArgs != 0 )
	{
		VARIANT *pArg = dispparams.rgvarg + dispparams.cArgs - 1;
		const WORD *pb = m_pDispInfo[nIndex].m_pParamTypes;

		uint32_t param = params;

		while ( *pb != 0 )
		{
			switch ( *pb )
			{
				case VT_BSTR:
					::SysFreeString ( pArg->bstrVal );
					break;
				case VT_VARIANT:
					if ( pArg->vt == VT_BSTR )
					{
						::SysFreeString ( pArg->bstrVal );
					}
					break;
				case VT_VARIANT | VT_BYREF:
					val = params[param];

					if ( val->type == slangType::eREFERENCE )
					{
						switch ( pArg->pvarVal->vt )
						{
							case VT_UI1:
								val->dat.ref.v->type = slangType::eLONG;
								val->dat.ref.v->dat.l = pArg->bVal;
								break;
							case VT_I2:
								val->dat.ref.v->type = slangType::eLONG;
								val->dat.ref.v->dat.l = pArg->iVal;
								break;
							case VT_I4:
								val->dat.ref.v->type = slangType::eLONG;
								val->dat.ref.v->dat.l = pArg->lVal;
								break;
							case VT_R4:
								val->dat.ref.v->type = slangType::eDOUBLE;
								val->dat.ref.v->dat.d = pArg->fltVal;
								break;
							case VT_R8:
								val->dat.ref.v->type = slangType::eDOUBLE;
								val->dat.ref.v->dat.d = pArg->dblVal;
								break;
							case VT_BOOL:
								val->dat.ref.v->type = slangType::eLONG;
								val->dat.ref.v->dat.l = pArg->boolVal;
								break;
							case VT_BSTR:
								val->dat.ref.v->type = slangType::eSTRING;
								val->dat.ref.v->dat.str.len = wcslen ( pArg->pvarVal->bstrVal );
								val->dat.ref.v->dat.str.c = (char *)instance->om->alloc ( val->dat.ref.v->dat.str.len + 1 );
								WCharToChar ( (WCHAR *) val->dat.ref.v->dat.str.c, pArg->pvarVal->bstrVal );
								break;
						}
					}
					break;
			}
			param++;
			--pArg;
			++pb;
		}
	}
	delete[] dispparams.rgvarg;

	// throw exception on failu re
	if ( m_hRet < 0 )
	{
		::VariantClear ( m_pDispInfo[nIndex].m_pOutput );
		if ( m_hRet != DISP_E_EXCEPTION )
		{
#ifdef COMClassDRIVER_DEBUG
			_tprintf ( _T ( "Invoke failed, no exception: %x\n" ), m_hRet );
#endif
			delete m_pExceptInfo;
			m_pExceptInfo = NULL;
			return 0;
		}

		// make sure excepInfo is filled in
		if ( m_pExceptInfo->pfnDeferredFillIn != NULL )
			m_pExceptInfo->pfnDeferredFillIn ( m_pExceptInfo );

#ifdef COMClassDRIVER_DEBUG
		wprintf ( L"Exception source: %s\n", m_pExceptInfo->bstrSource );
		wprintf ( L"Exception description: %s\n", m_pExceptInfo->bstrDescription );
		wprintf ( L"Exception help file: %s\n", m_pExceptInfo->bstrHelpFile );
#endif
		return 0;
	}

#if 0
	if ( m_pDispInfo[nIndex].m_vtOutputType == VT_EMPTY )
	{
		m_pDispInfo[nIndex].m_pOutput->vt = m_pDispInfo[nIndex].m_vtOutputType;
	}
#endif

	delete m_pExceptInfo;
	m_pExceptInfo = NULL;

	return (m_pDispInfo[nIndex].m_pOutput);
}

static void cCOMNew ( vmInstance *instance, VAR_OBJ *obj, char const *classId )
{
	auto cDriver = obj->makeObj<COMClassDriver> ( instance );

	if ( classId )
	{
		if ( !cDriver->CreateObject ( classId, CLSCTX_LOCAL_SERVER ) )
		{
			if ( !cDriver->CreateObject ( classId, CLSCTX_INPROC_SERVER ) )
			{
				delete cDriver;
				throw GetLastError ( );
			}
		}
	}
}

static void VariantToVAR ( vmInstance *instance, VAR *var, VARIANT *out )
{
out_loop:
	switch ( out->vt )
	{
		case VT_BSTR:
			*var = VAR_STR ( instance, (char *)_bstr_t ( out->bstrVal ) );
			break;
		case VT_UI1:
		case VT_I2:
		case VT_I4:
		case VT_ERROR:
		case VT_BOOL:
			var->type = slangType::eLONG;

			switch ( out->vt )
			{
				case VT_UI1:
					var->dat.l = out->bVal;
					break;
				case VT_I2:
					var->dat.l = out->iVal;
					break;
				case VT_I4:
					var->dat.l = out->lVal;
					break;
				case VT_ERROR:
					var->dat.l = out->scode;
					break;
				case VT_BOOL:
					var->dat.l = out->boolVal?1:0;
					break;
			}
			break;
		case VT_R4:
		case VT_R8:
			var->type = slangType::eDOUBLE;

			switch ( out->vt )
			{
				case VT_R4:
					var->dat.d = out->fltVal;
					break;
				case VT_R8:
					var->dat.d = (long) out->dblVal;
					break;
			}
			break;
		case VT_NULL:
			var->type = slangType::eNULL;
			break;
		case VT_PTR:
		case VT_DISPATCH:
			{
				auto obj = instance->objNew ( "COM" );

				auto cDriver = (COMClassDriver *)classGetCargo ( &obj );
				cDriver->Attach ( out->pdispVal );

				*var = obj;
			}
			break;
		case VT_ARRAY | VT_UI1:
			{
				long lstart, lend;

				SAFEARRAY *sa;
				char *cTmp = nullptr;

				sa = out->parray;
				SafeArrayGetLBound ( sa, 1, &lstart );
				SafeArrayGetUBound ( sa, 1, &lend );
				SafeArrayAccessData ( sa, (void **)&cTmp );

				*var = VAR_STR ( instance, cTmp, lend - lstart );
			}
			break;
		case VT_VARIANT:
			out = out->pvarVal;
			goto out_loop;
		case VT_EMPTY:;
			var->type = slangType::eNULL;
			break;
		default:
			throw errorNum::scUNSUPPORTED;
	}
}

static VAR cCOMDefAccess ( vmInstance *instance, VAR_OBJ *obj, char const *propName )
{
	auto cDriver = (COMClassDriver *) classGetCargo ( obj );
	auto out = cDriver->GetProperty ( propName );
	if ( !out ) throw errorNum::scINVALID_ACCESS;
	
	VAR ret;
	VariantToVAR ( instance, &ret, out );
	return ret;
}

static void cCOMDefAssign ( vmInstance *instance, VAR_OBJ *obj, char const *propName, VAR *val )
{
	auto cDriver = (COMClassDriver *) classGetCargo ( obj );
	cDriver->PropertyPut ( instance, _bstr_t ( propName ), val );
}

static VAR cCOMDefMethod ( vmInstance *instance, VAR_OBJ *obj, char const *methName, nParamType params )
{
	auto cDriver = (COMClassDriver *) classGetCargo ( obj );
	auto out = cDriver->InvokeMethod ( instance, _bstr_t ( methName ), params );

	if ( !out )
	{
		throw errorNum::scINVALID_ASSIGN;
	}

	VAR ret;
	VariantToVAR ( instance, &ret, out );
	return ret;
}

static void cCOMRelease ( vmInstance *instance, VAR_OBJ *obj )
{
	auto cDriver = (COMClassDriver *) classGetCargo ( obj );
	cDriver->~COMClassDriver ( );
}

void builtinCom ( vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		CLASS ( "COM" );
			METHOD( "new", cCOMNew );
			METHOD ( "release", cCOMRelease );

			ACCESS ( "default", cCOMDefAccess );
			ASSIGN ( "default", cCOMDefAssign );
			METHOD ( "default", cCOMDefMethod );
		END;
	END;
}
