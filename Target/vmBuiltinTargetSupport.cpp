/*
		IO Support functions

*/
#include "Utility/settings.h"

#include "stdafx.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"
#include "bcVM/vmAllocator.h"
#include "version/versionLib.h"
#include "runAndCapture.h"
#include "vmConf.h"

#include <errno.h>
#include <filesystem>

static unsigned long engineBuildNumber ( void )
{
	DWORD				 dummy;
	char				*VerInfoData;
	int					 VerInfoSize;
	int					 VerInfoBlockLen{};
	VS_FIXEDFILEINFO	*VerFixedFileInfo{};
	unsigned long		 result;
	char				 appName[MAX_NAME_SZ];

	GetModuleFileName ( 0, appName, MAX_NAME_SZ );
	VerInfoSize = GetFileVersionInfoSize ( appName, &dummy );
	VerInfoData = (char *)malloc ( sizeof ( char ) * VerInfoSize );
	GetFileVersionInfo ( appName, 0, VerInfoSize, VerInfoData );
	VerQueryValue ( VerInfoData, "\\", (LPVOID*)&VerFixedFileInfo, (PUINT)&VerInfoBlockLen );

	result = LOWORD ( VerFixedFileInfo->dwFileVersionLS );

	free ( VerInfoData );

	return result;
}

static VAR_STR engineVersion ( vmInstance *instance )
{
	DWORD				 dummy;
	char				*VerInfoData;
	int					 VerInfoSize;
	unsigned int		 VerInfoBlockLen;
	VS_FIXEDFILEINFO	*VerFixedFileInfo{};
	TCHAR szExeName[MAX_NAME_SZ];

	::GetModuleFileName ( AfxGetInstanceHandle ( ), szExeName, MAX_NAME_SZ );

	VerInfoSize = GetFileVersionInfoSize ( szExeName, &dummy );
	VerInfoData = (char *) malloc ( sizeof ( char ) * VerInfoSize );
	GetFileVersionInfo ( szExeName, 0, VerInfoSize, VerInfoData );
	VerQueryValue ( VerInfoData, "\\", (LPVOID *) &VerFixedFileInfo, &VerInfoBlockLen );

	char version[512];

#if _DEBUG
	sprintf_s (version, sizeof (version), "Slang MultiServer - v%u.%u.%s %s %s - DEBUG", HIWORD (VerFixedFileInfo->dwFileVersionMS), LOWORD (VerFixedFileInfo->dwFileVersionLS), VCS_SHORT_HASH, __TIME__, VCS_WC_MODIFIED ? " - NON-COMMITTED" : "");
#else
	sprintf_s (version, sizeof (version), "Slang MultiServer - v%u.%u.%s %s %s", HIWORD (VerFixedFileInfo->dwFileVersionMS), LOWORD (VerFixedFileInfo->dwFileVersionLS), VCS_SHORT_HASH, __TIME__, VCS_WC_MODIFIED ? " - NON-COMMITTED" : "");
#endif
	return VAR_STR ( instance, version, strlen ( version ) );
}

static VAR engineVersionNumber ( vmInstance *instance )
{
	return VAR_STR ( VCS_SHORT_HASH );
}

static VAR_STR vmRunAndCapture ( class vmInstance *instance, char const *directory, char const *fName, char const *parameters, char const *stdIn )
{
	BUFFER results;

	runAndCapture ( directory, fName, parameters, stdIn, results );

	return VAR_STR ( instance, results );
}

static DWORD vmRunAndDetatch ( class vmInstance *instance, char const *directory, char const *fName, char const *parameters, char const *stdIn )
{
	BUFFER results;

	return runAndDetatch ( directory, fName, parameters, stdIn );
}


static void vmSleep( uint32_t t )
{
	Sleep( t );
}

static VAR_STR vmEngineFileName ( vmInstance *instance )
{
	char appName[MAX_PATH];

	// do application server processing
	GetModuleFileName ( 0, appName, MAX_PATH );

	return VAR_STR ( instance, appName );
}

static VAR_STR vmIniGetStr ( vmInstance *instance, char const *file, char const *section, char const *key, char const *defStr )
{
	char tmp[8193];

	GetPrivateProfileString ( section, key, defStr, tmp, 8193, file );
	tmp[sizeof ( tmp ) - 1] = 0;	// ensure null termination

	return VAR_STR ( instance, tmp );
}

static auto vmIniGetInt ( char *file, char const *section, char const *key, int defVal )
{
	return GetPrivateProfileInt ( section, key, defVal, file );
}

static auto vmIniSetStr ( char *file, char const *section, char const *key, char const *value )
{
	return WritePrivateProfileString ( section, key, value, file );
}

static auto vmIniSetInt ( char *file, char const *section, char const *key, char const *value )
{
	return WritePrivateProfileString ( section, key, value, file );
}

static auto vmIniDeleteSection ( char *file, char const *section )
{
	return WritePrivateProfileString ( section, NULL, NULL, file );
}

static VAR_STR vmIniGetKeys ( vmInstance *instance, char const *file, char const *section )
{
	char	*ptr;
	int		 len;
	BUFFER	 buff;

	for ( ;; )
	{
		len = GetPrivateProfileString ( section, NULL, "", bufferBuff ( &buff ), (DWORD)bufferFree ( &buff ), file );
		if ( len < bufferFree ( &buff ) - 2 )
		{
			break;
		}
		buff.makeRoom ( 16384 );
	}
	if ( !len )
	{
		return VAR_STR ( "", 0 );
	}
	buff.assume ( len );

	ptr = bufferBuff ( &buff );

	/* convert the embedded 0's to ';' characters */
	while ( *ptr || *(ptr + 1) )
	{
		if ( !*ptr )
		{
			*ptr = ';';
		}
		ptr++;
	}
	buff.put ( (char) 0 );

	return VAR_STR ( instance, bufferBuff ( &buff ), bufferSize ( &buff ) );
}

auto static vmIniDeleteKey ( char *file, char const *section, char const *key )
{
	return WritePrivateProfileString ( section, key, NULL, file );
}

auto static vmIniCommit ( char *file )
{
	return WritePrivateProfileString ( NULL, NULL, NULL, file );
}

bool static vmRegistryWrite ( vmInstance *instance, char const *key, char const *subKey, char const *value, VAR *val )
{
	HKEY			hKey;
	unsigned long	disposition;
	unsigned long	tmpWord;

	if ( !fglstrccmp ( key, "HKEY_CLASSES_ROOT" ) )
	{
		hKey = HKEY_CLASSES_ROOT;		// NOLINT(performance-no-int-to-ptr)
	} else 	if ( !fglstrccmp ( key, "HKEY_CURRENT_CONFIG" ) )
	{
		hKey = HKEY_CURRENT_CONFIG;		// NOLINT(performance-no-int-to-ptr)
	} else 	if ( !fglstrccmp ( key, "HKEY_CURRENT_USER" ) )
	{
		hKey = HKEY_CURRENT_USER;		// NOLINT(performance-no-int-to-ptr)
	} else 	if ( !fglstrccmp ( key, "HKEY_LOCAL_MACHINE" ) )
	{
		hKey = HKEY_LOCAL_MACHINE;		// NOLINT(performance-no-int-to-ptr)
	} else 	if ( !fglstrccmp ( key, "HKEY_USERS" ) )
	{
		hKey = HKEY_USERS;				// NOLINT(performance-no-int-to-ptr)
	} else
	{
		return false;
	}

	disposition = 0;

	if ( RegCreateKeyEx ( hKey,
						  subKey,
						  0,
						  0,
						  REG_OPTION_VOLATILE,
						  KEY_WRITE,
						  0,
						  &hKey,
						  &disposition
	) != ERROR_SUCCESS
		 )
	{
		return false;
	}


	switch ( TYPE ( val ) )
	{
		case slangType::eSTRING:
			if ( RegSetValueEx ( hKey, value, 0, REG_SZ, (const unsigned char *) (val->dat.str.c), (DWORD)val->dat.str.len + 1 ) != ERROR_SUCCESS )
			{
				RegCloseKey ( hKey );
				return false;
			}
			break;
		case slangType::eLONG:
			if ( RegSetValueEx ( hKey, value, 0, REG_DWORD, (const unsigned char *) (&(val->dat.l)), (DWORD)sizeof ( val->dat.l ) ) != ERROR_SUCCESS )
			{
				RegCloseKey ( hKey );
				return false;
			}
			break;
		case slangType::eDOUBLE:
			tmpWord = (unsigned long) val->dat.d;

			if ( RegSetValueEx ( hKey, value, 0, REG_DWORD, (const unsigned char *) (&tmpWord), (DWORD)sizeof ( tmpWord ) ) != ERROR_SUCCESS )

			{
				RegCloseKey ( hKey );
				return false;
			}
			break;
		default:
			break;
	}

	RegCloseKey ( hKey );

	return true;
}

VAR static vmRegistryRead ( vmInstance *instance, char const *key, char const *subKey, char const *value )
{
	HKEY			hKey;
	unsigned long	disposition;
	unsigned long	size;
	unsigned long	type;

	if ( !fglstrccmp ( key, "HKEY_CLASSES_ROOT" ) )
	{
		hKey = HKEY_CLASSES_ROOT;		// NOLINT(performance-no-int-to-ptr)
	} else 	if ( !fglstrccmp ( key, "HKEY_CURRENT_CONFIG" ) )
	{
		hKey = HKEY_CURRENT_CONFIG;		// NOLINT(performance-no-int-to-ptr)
	} else 	if ( !fglstrccmp ( key, "HKEY_CURRENT_USER" ) )
	{
		hKey = HKEY_CURRENT_USER;		// NOLINT(performance-no-int-to-ptr)
	} else 	if ( !fglstrccmp ( key, "HKEY_LOCAL_MACHINE" ) )
	{
		hKey = HKEY_LOCAL_MACHINE;		// NOLINT(performance-no-int-to-ptr)	
	} else 	if ( !fglstrccmp ( key, "HKEY_USERS" ) )
	{
		hKey = HKEY_USERS;				// NOLINT(performance-no-int-to-ptr)
	} else
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	disposition = 0;

	if ( RegCreateKeyEx ( hKey,
						  subKey,
						  0,
						  0,
						  REG_OPTION_VOLATILE,
						  KEY_READ,
						  0,
						  &hKey,
						  &disposition
	) != ERROR_SUCCESS
		 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	size = 0;

	if ( RegQueryValueEx ( hKey, value, 0, &type, 0, &size ) != ERROR_SUCCESS )
	{
		RegCloseKey ( hKey );
		throw errorNum::scINVALID_PARAMETER;
	}

	switch ( type )
	{
		case REG_BINARY:
			{
				BUFFER buff;

				buff.makeRoom ( size );

				RegQueryValueEx ( hKey, value, 0, &type, (LPBYTE) bufferBuff ( &buff ), &size );
				RegCloseKey ( hKey );

				return VAR_STR ( instance, bufferBuff ( &buff ), size );
			}
			break;
		case REG_SZ:
			{
				BUFFER buff;

				buff.makeRoom ( size );

				RegQueryValueEx ( hKey, value, 0, &type, (LPBYTE) bufferBuff ( &buff ), &size );
				RegCloseKey ( hKey );

				return VAR_STR ( instance, bufferBuff ( &buff ), size - 1);		// size includes null
			}
			break;
		case REG_DWORD:
			{
				DWORD dword{};
				RegQueryValueEx ( hKey, value, 0, &type, (unsigned char *) &dword, &size );
				RegCloseKey ( hKey );
				return VAR ( (uint32_t) dword );
			}
			break;
		default:
			RegCloseKey ( hKey );
			throw errorNum::scINVALID_PARAMETER;
	}
	RegCloseKey ( hKey );
}

static bool vmIsSameFile ( VAR_STR file1, VAR_STR file2 )
{
	return isSameFile ( ( char const*) file1, ( char const*) file2 );
}

static BOOL CALLBACK SmartRunWindowEnumerator ( HWND hwnd, LPARAM lParam )
{
	DWORD	pId;
	char	tmp[256];
	int		lExStyle;
	int		bNoOwner;
	DWORD	lForeThreadID;
	DWORD	lThisThreadID;

	GetWindowThreadProcessId ( hwnd, &pId );

	if ( pId == (DWORD) lParam )
	{
		if ( IsWindowVisible ( hwnd ) )
		{
			if ( !GetParent ( hwnd ) )
			{
				bNoOwner = (GetWindow ( hwnd, GW_OWNER ) == 0);
				lExStyle = GetWindowLong ( hwnd, GWL_EXSTYLE );

				if ( (((lExStyle & WS_EX_TOOLWINDOW) == 0) && bNoOwner) ||
					((lExStyle & WS_EX_APPWINDOW) && !bNoOwner) )
				{
					if ( GetWindowText ( hwnd, tmp, sizeof ( tmp ) ) )
					{
						lForeThreadID = GetWindowThreadProcessId ( GetForegroundWindow ( ), 0 );
						lThisThreadID = GetWindowThreadProcessId ( hwnd, 0 );

						AttachThreadInput ( lForeThreadID, lThisThreadID, TRUE );
						SetWindowPos ( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );

						AttachThreadInput ( lForeThreadID, lThisThreadID, FALSE );

						if ( IsIconic ( hwnd ) )
						{
							ShowWindow ( hwnd, SW_RESTORE );
						} else
						{
							ShowWindow ( hwnd, SW_SHOW );
						}
						return (FALSE);
					}
				}
			}
		}
	}
	return (TRUE);
}

auto static vmSmartRun ( vmInstance *instance, char const *fName, int flags, nParamType params )
{
	VAR						*var;
	BUFFER					 buffer;
	SECURITY_ATTRIBUTES		 sa{};
	SECURITY_DESCRIPTOR		 sd{};
	STARTUPINFO				 startupInfo;
	PROCESS_INFORMATION		 processInfo;

	std::filesystem::path p ( fName );
	auto dir = std::filesystem::path ( p ).remove_filename ().generic_string ();

	bufferPrintf ( &buffer, "%s", fName );

	for ( uint32_t loop = 0; loop < params; loop++ )
	{
		var = params[loop];

		while ( TYPE ( var ) == slangType::eREFERENCE )
		{
			var = var->dat.ref.v;
		}

		buffer.put ( ' ' );

		switch ( TYPE ( var ) )
		{
			case slangType::eSTRING:
				bufferPrintf ( &buffer, "%s", var->dat.str.c );
				break;
			case slangType::eDOUBLE:
				bufferPrintf ( &buffer, "%f", var->dat.d );
				break;
			case slangType::eLONG:
				bufferPrintf ( &buffer, "%l", var->dat.l );
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}
	buffer.put<char> ( 0 );

	InitializeSecurityDescriptor ( &sd, SECURITY_DESCRIPTOR_REVISION );
	SetSecurityDescriptorDacl ( &sd, TRUE, NULL, FALSE );
	sa.lpSecurityDescriptor = &sd;
	sa.nLength = sizeof ( SECURITY_ATTRIBUTES );
	sa.bInheritHandle = TRUE;         //allow inheritable handles

	GetStartupInfo ( &startupInfo );

	startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = flags;
	startupInfo.hStdOutput = 0;
	startupInfo.hStdError = 0;     //set the new handles for the child process
	startupInfo.hStdInput = 0;

	if ( !CreateProcess ( NULL, //progName,
						bufferBuff ( &buffer ),
						NULL,
						NULL,
						FALSE,
						CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
						NULL,
						dir.c_str(),
						&startupInfo,
						&processInfo
		) )
	{
		throw GetLastError ( );
	}

	if ( flags == SW_SHOW )
	{
		if ( EnumWindows ( SmartRunWindowEnumerator, processInfo.dwProcessId ) )
		{
			for ( uint32_t loop = 0; loop < 10; loop++ )
			{
				Sleep ( 100 );
				if ( !EnumWindows ( SmartRunWindowEnumerator, processInfo.dwProcessId ) )
				{
					break;
				}
			}
		}
	}

	CloseHandle ( processInfo.hThread );
	CloseHandle ( processInfo.hProcess );

	return (size_t)processInfo.hProcess;
}

void builtinTargetInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		FUNC ( "engineVersion", engineVersion );
		FUNC ( "engineVersionNumber", engineVersionNumber );
		FUNC ( "engineBuildNumber", engineBuildNumber );
		FUNC ( "engineFileName", vmEngineFileName );

		FUNC ( "GetLastError", GetLastError );

		FUNC ( "Sleep", vmSleep );

		FUNC ( "runAndCapture", vmRunAndCapture );
		FUNC ( "runAndDetatch", vmRunAndDetatch );

		FUNC ( "iniCommit", vmIniCommit );
		FUNC ( "iniDeleteKey", vmIniDeleteKey );
		FUNC ( "iniDeleteSection", vmIniDeleteSection );
		FUNC ( "iniGetInt", vmIniGetInt );
		FUNC ( "iniGetKeys", vmIniGetKeys );
		FUNC ( "iniGetStr", vmIniGetStr );
		FUNC ( "iniGetString", vmIniGetStr );
		FUNC ( "iniSetInt", vmIniSetInt );
		FUNC ( "iniSetStr", vmIniSetStr );
		FUNC ( "iniSetString", vmIniSetStr );

		FUNC ( "registryRead", vmRegistryRead );
		FUNC ( "registryWrite", vmRegistryWrite );

		FUNC ( "isSameFile", vmIsSameFile );

		FUNC ( "smartRun", vmSmartRun );
	END
}
