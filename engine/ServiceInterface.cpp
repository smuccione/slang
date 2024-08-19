/*

XP Service Interface functions


*/

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "stdafx.h"
#include <stdint.h>
#include <Windows.h>
#include "engine.h"
#include "lm.h"
#include "Sddl.h"
#include "resource.h"
#include <stdio.h>
#include <process.h>
#include <fstream>
#include <iostream>
#include <thread>

static void WINAPI AppServiceMain ( DWORD dwArgc, LPTSTR* lpszArgv );

SERVICE_TABLE_ENTRY appServiceTable[] = {
											{const_cast<char *>("SlangSvr"), AppServiceMain},
											{0, 0}
										};


static SERVICE_STATUS_HANDLE	 statusHandle = 0;

static LRESULT CALLBACK CheckServiceDlgProc ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			break;
		case WM_COMMAND:
			switch ( LOWORD ( wParam ) )
			{
				case IDC_SVC_START:
				case IDC_SVC_STOP:
				case IDC_SVC_ENGINE:
				case IDC_SVC_EXIT:
					EndDialog ( hDlg, (INT_PTR)wParam );
					return TRUE;

				default:
					return FALSE;
			}
			return 0;
	}

	return FALSE;
}

static int StopService ( char *name )
{
	int				ctr;
	SERVICE_STATUS	status;
	SC_HANDLE		manHandle;
	SC_HANDLE		svcHandle;

	manHandle = OpenSCManager ( 0, 0, SC_MANAGER_ALL_ACCESS );

	if ( (svcHandle = OpenService ( manHandle, name, SERVICE_STOP | SERVICE_QUERY_STATUS )) )
	{
		ControlService ( svcHandle, SERVICE_CONTROL_STOP, &status );

		for ( ctr = 0; ctr < 900; ctr++ )
		{
			Sleep ( 100 );
			QueryServiceStatus ( svcHandle, &status );
			if ( status.dwCurrentState == SERVICE_STOPPED )
			{
				break;
			}
		}
		CloseServiceHandle ( svcHandle );
		CloseServiceHandle ( manHandle );
		return (1);
	}
	CloseServiceHandle ( manHandle );
	return (0);
}

static int StartService ( char *name )
{
	SC_HANDLE		manHandle;
	SC_HANDLE		svcHandle;

	manHandle = OpenSCManager ( 0, 0, SC_MANAGER_ALL_ACCESS );

	if ( (svcHandle = OpenService ( manHandle, name, SERVICE_START )) )
	{
		StartService ( svcHandle, 0, 0 );
		CloseServiceHandle ( svcHandle );
	}
	CloseServiceHandle ( manHandle );
	return (1);
}

static DWORD WINAPI ServiceHandlerEx ( DWORD	dwControl,
									   DWORD	dwEventType,
									   LPVOID	lpEventData,
									   LPVOID	lpContext
)

{
	SERVICE_STATUS			 serviceStatus;
	static int				 shuttingDown = 0;

	memset ( &serviceStatus, 0, sizeof ( serviceStatus ) );

	switch ( dwControl )
	{
		case SERVICE_CONTROL_STOP:
			shuttingDown = 1;

			serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
			serviceStatus.dwCheckPoint = 1;
			serviceStatus.dwWaitHint = 10000;
			SetServiceStatus ( statusHandle, &serviceStatus );

			SetEvent ( theApp.shutDown );
			break;
		case SERVICE_CONTROL_INTERROGATE:
			if ( shuttingDown )
			{
				serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
				serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
				serviceStatus.dwCheckPoint = 1;
				serviceStatus.dwWaitHint = 10000;
			} else
			{
				serviceStatus.dwCurrentState = SERVICE_RUNNING;
				serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
			}
			SetServiceStatus ( statusHandle, &serviceStatus );
			break;
		case SERVICE_CONTROL_SHUTDOWN:
			shuttingDown = 1;
			serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
			serviceStatus.dwCheckPoint = 1;
			serviceStatus.dwWaitHint = 10000;
			SetServiceStatus ( statusHandle, &serviceStatus );

			SetEvent ( theApp.shutDown );
			break;
		case SERVICE_CONTROL_PAUSE:
		case SERVICE_CONTROL_CONTINUE:
			serviceStatus.dwCurrentState = SERVICE_RUNNING;
			serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
			SetServiceStatus ( statusHandle, &serviceStatus );
			break;
	}

	return (NO_ERROR);
}

static void WINAPI AppServiceMain ( DWORD dwArgc, LPTSTR* lpszArgv )
{
	SERVICE_STATUS	 serviceStatus;
	char			 appName[MAX_PATH];
	char			*cmdLine;

	statusHandle = RegisterServiceCtrlHandlerEx ( appServiceTable[0].lpServiceName, ServiceHandlerEx, 0 );

	memset ( &serviceStatus, 0, sizeof ( serviceStatus ) );

	// mark us as pending start
	serviceStatus.dwServiceType = SERVICE_WIN32;
	serviceStatus.dwCurrentState = SERVICE_START_PENDING;
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	serviceStatus.dwWaitHint = 2000;					// should be less then 2 seconds to start
	SetServiceStatus ( statusHandle, &serviceStatus );

	// do application server processing
	GetModuleFileName ( 0, appName, MAX_PATH );

	cmdLine = GetCommandLine ();
	while ( *cmdLine && (*cmdLine != ' ') )
	{
		cmdLine++;
	}
	while ( *cmdLine == ' ' )
	{
		cmdLine++;
	}

	serviceStatus.dwCurrentState = SERVICE_RUNNING;
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	serviceStatus.dwWaitHint = 0;
	SetServiceStatus ( statusHandle, &serviceStatus );

	theApp.appServerStart ();

	serviceStatus.dwCurrentState = SERVICE_STOPPED;
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwCheckPoint = 0;
	serviceStatus.dwWaitHint = 0;
	SetServiceStatus ( statusHandle, &serviceStatus );
	return;
}

bool CSlangServer::CheckService ( void )
{
	uint32_t		startChoice;
	SC_HANDLE		manHandle = nullptr;
	SC_HANDLE		svcHandle = nullptr;

	if ( !(manHandle = OpenSCManager ( 0, 0, SC_MANAGER_ALL_ACCESS )) )
	{
		return true;
	}

	if ( (svcHandle = OpenService ( manHandle, appServiceTable[0].lpServiceName, SERVICE_START | SERVICE_STOP )) )
	{
		CloseServiceHandle ( manHandle );
		CloseServiceHandle ( svcHandle );

		startChoice = (uint32_t) DialogBox ( theApp.m_hInstance, MAKEINTRESOURCE ( IDD_SERVICES_DIALOG ), 0, (DLGPROC) CheckServiceDlgProc );

		switch ( startChoice )
		{
			case IDC_SVC_START:
				StartService ( appServiceTable[0].lpServiceName );
				return false;
			case IDC_SVC_STOP:
				StopService ( appServiceTable[0].lpServiceName );
				return false;
			case IDC_SVC_ENGINE:
				return true;
			case IDC_SVC_EXIT:
				return false;
			default:
				return true;
		}
	}
	CloseServiceHandle ( manHandle );
	return true;
}

bool CSlangServer::RegisterAsService ()

{
	SC_HANDLE					 manHandle;
	SC_HANDLE					 svcHandle;
	SERVICE_DESCRIPTION			 svcDesc;
	SERVICE_FAILURE_ACTIONS		 svcFailActions;
	SC_ACTION					 svcActions[4];
	char						 serviceString[1024];
	char						 directory[MAX_PATH + 1];
	char						 appName[MAX_PATH + 1];

	GetModuleFileName ( 0, appName, MAX_PATH );

	manHandle = OpenSCManager ( 0, 0, SC_MANAGER_ALL_ACCESS );

	GetCurrentDirectory ( sizeof ( directory ), directory );

	_snprintf_s ( serviceString, sizeof ( serviceString ), _TRUNCATE, "%s --service %s", appName, directory );

	if ( !(svcHandle = CreateService ( manHandle,
									   appServiceTable[0].lpServiceName,
									   "Slang App Server",
									   SC_MANAGER_ALL_ACCESS,
									   SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
									   SERVICE_DEMAND_START,
									   SERVICE_ERROR_NORMAL,
									   serviceString,
									   NULL,
									   NULL,
									   "\0",
									   NULL,
									   NULL
	)) )
	{
		// error installing service
		printf ( "Error installing service %lu\r\n", GetLastError () );
	} else
	{
		svcDesc.lpDescription = const_cast<char *>("Slang Application Server");
		ChangeServiceConfig2 ( svcHandle, SERVICE_CONFIG_DESCRIPTION, &svcDesc );

		svcFailActions.cActions = 1;
		svcFailActions.lpRebootMsg = 0;
		svcFailActions.lpCommand = 0;
		svcFailActions.dwResetPeriod = INFINITE;
		svcFailActions.lpsaActions = svcActions;

		svcActions[0].Type = SC_ACTION_RESTART;
		svcActions[0].Delay = 0;

		ChangeServiceConfig2 ( svcHandle, SERVICE_CONFIG_FAILURE_ACTIONS, &svcFailActions );

		CloseServiceHandle ( svcHandle );
	}

	CloseServiceHandle ( manHandle );

	return true;
}

bool CSlangServer::DeRegisterAsService ( void )

{
	SC_HANDLE	manHandle;
	SC_HANDLE	svcHandle;

	manHandle = OpenSCManager ( 0, 0, SC_MANAGER_ALL_ACCESS );

	if ( (svcHandle = OpenService ( manHandle, appServiceTable[0].lpServiceName, SERVICE_ALL_ACCESS )) )
	{
		if ( DeleteService ( svcHandle ) )
		{
			// service deleted
		} else
		{
			// unable to delete service
			printf( "error deregistering service: %lu\r\n", GetLastError() );
		}
		CloseServiceHandle ( svcHandle );
	} else
	{
		printf ( "error deregistering service: %lu\r\n", GetLastError () );
	}

	CloseServiceHandle ( manHandle );

	return true;
}

bool CSlangServer::InstallAPPService ( void )
{
	return StartServiceCtrlDispatcher ( appServiceTable ) ? true : false;
}

