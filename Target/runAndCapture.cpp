/*
		IO Support functions

*/

#include "Utility/settings.h"

#include "stdafx.h"
#include "vmConf.h"
#include <stdlib.h>
#include <errno.h>
#include <filesystem>

#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/Opcodes.h"
#include "bcVM/exeStore.h"
#include "bcVM/vmAtom.h"
#include "Utility/funcky.h"
#include "bcVM/fglTypes.h"
#include "bcVM/bcVMObject.h"
#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerOptimizer.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"
#include "compilerBCGenerator/transform.h"

DWORD runAndCapture ( char const *directory, char const *fName, char const *params, char const *stdIn, BUFFER &buffer )
{
	DWORD					 bytesAvail = 0;
	DWORD					 bytesRead = 0;
	DWORD					 bytesWritten;
	DWORD					 exitCode = 0;
	char					 tmpBuff[8192];
	SECURITY_ATTRIBUTES		 sa{};
	SECURITY_DESCRIPTOR		 sd{};
	STARTUPINFO				 startupInfo;
	PROCESS_INFORMATION		 processInfo;
	HANDLE					 newstdin;
	HANDLE					 newstdout;
	HANDLE					 newstderr;
	HANDLE					 read_stdout;
	HANDLE					 read_stdoutTmp;
	HANDLE					 write_stdin;
	HANDLE					 write_stdinTmp;
	int						 r;

	InitializeSecurityDescriptor ( &sd, SECURITY_DESCRIPTOR_REVISION );
	SetSecurityDescriptorDacl ( &sd, TRUE, NULL, FALSE );
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = TRUE;         //allow inheritable handles
	sa.nLength = sizeof ( SECURITY_ATTRIBUTES );

	if ( !CreatePipe ( &newstdin, &write_stdinTmp, &sa, 0 ) )   //create stdin pipe
	{
		return GetLastError ();
	}

	if ( !CreatePipe ( &read_stdoutTmp, &newstdout, &sa, 0 ) )  //create stdout pipe
	{
		CloseHandle ( newstdin );
		CloseHandle ( write_stdinTmp );
		return GetLastError ();
	}

	// Create a duplicate of the output write handle for the std error
	  // write handle. This is necessary in case the child application
	  // closes one of its std output handles.
	if ( !DuplicateHandle ( GetCurrentProcess (), 
							newstdout,
							GetCurrentProcess (), 
							&newstderr,
							0,
							TRUE, DUPLICATE_SAME_ACCESS )
		 )
	{
		CloseHandle ( write_stdinTmp );
		CloseHandle ( read_stdoutTmp );
		CloseHandle ( newstdin );
		CloseHandle ( newstdout );
		return GetLastError ();
	}

	// Create new output read handle and the input write handles. Set
	// the Properties to FALSE. Otherwise, the child inherits the
	// properties and, as a result, non-closeable handles to the pipes
	// are created.
	if ( !DuplicateHandle ( GetCurrentProcess (), 
							write_stdinTmp,
							GetCurrentProcess (),
							&write_stdin, // Address of new handle.
							0, FALSE, // Make it uninheritable.
							DUPLICATE_SAME_ACCESS )
		 )
	{
		CloseHandle ( write_stdinTmp );
		CloseHandle ( read_stdoutTmp );
		CloseHandle ( newstdin );
		CloseHandle ( newstdout );
		CloseHandle ( newstderr );
		return GetLastError ();
	}

	if ( !DuplicateHandle ( GetCurrentProcess (), 
							read_stdoutTmp,
							GetCurrentProcess (),
							&read_stdout, // Address of new handle.
							0, FALSE,	// Make it uninheritable.
							DUPLICATE_SAME_ACCESS )
		 )
	{
		CloseHandle ( write_stdinTmp );
		CloseHandle ( read_stdoutTmp );
		CloseHandle ( newstdin );
		CloseHandle ( newstdout );
		CloseHandle ( newstderr );
		return GetLastError ();
	}

	// Close inheritable copies of the handles you do not want to be
	// inherited.
	CloseHandle ( write_stdinTmp );
	CloseHandle ( read_stdoutTmp );

	GetStartupInfo ( &startupInfo );

	startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_HIDE;
	startupInfo.hStdOutput = newstdout;
	startupInfo.hStdError = newstderr;     //set the new handles for the child process
	startupInfo.hStdInput = newstdin;

	if ( *params )
	{
		strncpy_s ( tmpBuff, sizeof ( tmpBuff ), "\"", _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), directory, _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), "/", _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), fName, _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), "\"", _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), " ", _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), params, _TRUNCATE );
	} else
	{
		strncpy_s ( tmpBuff, sizeof ( tmpBuff ), fName, _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), " ", _TRUNCATE );
		strncat_s ( tmpBuff, sizeof ( tmpBuff ), params, _TRUNCATE );
	}

	if ( vmConf.clientToken )
	{
		r = CreateProcessAsUser ( vmConf.clientToken,
								  nullptr, //progName,
								  tmpBuff,
								  NULL,
								  NULL,
								  TRUE,
								  CREATE_NEW_CONSOLE,
								  0,
								  directory,
								  &startupInfo,
								  &processInfo
		);
	} else
	{
		r = CreateProcess ( nullptr, //progName,
							tmpBuff,
							NULL,
							NULL,
							TRUE,
							CREATE_NEW_CONSOLE,
							0,
							directory,
							&startupInfo,
							&processInfo
		);
	}

	if ( !r )
	{
		CloseHandle ( processInfo.hProcess );
		CloseHandle ( processInfo.hThread );
		return GetLastError ();
	}

	if ( stdIn )
	{
		WriteFile ( write_stdin, stdIn, (DWORD) strlen ( stdIn ), &bytesWritten, NULL );
	}
	CloseHandle ( write_stdin );	// trigger end of file

	/* wait for program to end */
	for ( ;;)      //main program loop
	{
		GetExitCodeProcess ( processInfo.hProcess, (LPDWORD) &exitCode );      //while the process is running
		if ( exitCode != STILL_ACTIVE )
		{
			break;
		}

		PeekNamedPipe ( read_stdout, 0, 0, (LPDWORD) &bytesRead, (LPDWORD) &bytesAvail, NULL );

		//check to see if there is any data to read from stdout
		if ( bytesAvail != 0 )
		{
			bufferMakeRoom ( &buffer, bytesAvail );
			ReadFile ( read_stdout, bufferBuff ( &buffer ) + bufferSize ( &buffer ), (DWORD) bufferFree ( &buffer ), (LPDWORD) &bytesRead, NULL );  //read the stdout pipe
			bufferAssume ( &buffer, bytesRead );
		} else
		{
			/* just relinquish our time slice */
			Sleep ( 10 );
		}
	}

	/* read any remaining data from the pipe */
	do
	{
		PeekNamedPipe ( read_stdout, 0, 0, (LPDWORD) &bytesRead, (LPDWORD) &bytesAvail, NULL );

		//check to see if there is any data to read from stdout
		if ( bytesAvail != 0 )
		{
			bufferMakeRoom ( &buffer, bytesAvail );
			ReadFile ( read_stdout, bufferBuff ( &buffer ) + bufferSize ( &buffer ), (DWORD) bufferFree ( &buffer ), (LPDWORD) &bytesRead, NULL );  //read the stdout pipe
			bufferAssume ( &buffer, bytesRead );
		}
	} while ( bytesAvail );

	bufferPut8 ( &buffer, 0 );

	/* no close all the freaking handles we had to create just to do this... sigh... */
	CloseHandle ( newstdout );
	CloseHandle ( newstderr );
	CloseHandle ( newstdin );
	CloseHandle ( read_stdout );
	CloseHandle ( processInfo.hProcess );
	CloseHandle ( processInfo.hThread );

	return exitCode;
}

DWORD runAndDetatch ( char const *directory, char const *fName, char const *params, char const *stdIn )
{
	char					 tmpBuff[8192];
	SECURITY_ATTRIBUTES		 sa{};
	SECURITY_DESCRIPTOR		 sd{};
	STARTUPINFO				 startupInfo;
	PROCESS_INFORMATION		 processInfo;
	HANDLE					 newstdin;
	HANDLE					 write_stdin;
	HANDLE					 write_stdinTmp;
	DWORD					 bytesWritten;

	InitializeSecurityDescriptor ( &sd, SECURITY_DESCRIPTOR_REVISION );
	SetSecurityDescriptorDacl ( &sd, TRUE, NULL, FALSE );
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = TRUE;         //allow inheritable handles
	sa.nLength = sizeof ( SECURITY_ATTRIBUTES );

	GetStartupInfo ( &startupInfo );

	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_SHOW; // SW_HIDE;

	if ( !CreatePipe ( &newstdin, &write_stdinTmp, &sa, 0 ) )   //create stdin pipe
	{
		return (0);
	}

	// Create new output read handle and the input write handles. Set
	// the Properties to FALSE. Otherwise, the child inherits the
	// properties and, as a result, non-closeable handles to the pipes
	// are created.
	if ( !DuplicateHandle (	GetCurrentProcess (), 
							write_stdinTmp,
							GetCurrentProcess (),
							&write_stdin, // Address of new handle.
							0, 
							FALSE, // Make it uninheritable.
							DUPLICATE_SAME_ACCESS )
						)
	{
		CloseHandle ( write_stdinTmp );
		CloseHandle ( newstdin );
		return (0);
	}
	CloseHandle ( write_stdinTmp );

	strncpy_s ( tmpBuff, sizeof ( tmpBuff ), fName, _TRUNCATE );
	strncat_s ( tmpBuff, sizeof ( tmpBuff ), " ", _TRUNCATE );
	strncat_s ( tmpBuff, sizeof ( tmpBuff ), params, _TRUNCATE );

	if ( vmConf.clientToken )
	{
		if ( CreateProcessAsUser ( vmConf.clientToken,
								   nullptr, //progName,
								   tmpBuff,
								   NULL,
								   NULL,
								   FALSE,
								   CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
								   0,
								   directory,
								   &startupInfo,
								   &processInfo
		) )
		{
			{
				if ( stdIn )
				{
					WriteFile ( write_stdin, stdIn, (DWORD)strlen ( stdIn ), &bytesWritten, NULL );
				}
				CloseHandle ( write_stdin );	// trigger end of file
				CloseHandle ( processInfo.hProcess );
				CloseHandle ( processInfo.hThread );
				CloseHandle ( newstdin );
				return ERROR_SUCCESS;
			}
		}
	} else
	{
		if ( CreateProcess ( nullptr, //progName,
							 tmpBuff,
							 NULL,
							 NULL,
							 FALSE,
							 CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
							 0,
							 directory,
							 &startupInfo,
							 &processInfo
		) )
		{
			{
				if ( stdIn )
				{
					WriteFile ( write_stdin, stdIn, (DWORD)strlen ( stdIn ), &bytesWritten, NULL );
				}
				CloseHandle ( write_stdin );	// trigger end of file
				CloseHandle ( processInfo.hProcess );
				CloseHandle ( processInfo.hThread );
				CloseHandle ( newstdin );
				return ERROR_SUCCESS;
			}
		}
	}
	return GetLastError ();
}

