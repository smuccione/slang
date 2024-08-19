/*
	Engine configuration parser

*/

#include "Utility/settings.h"

#include "stdafx.h"

#include <stdlib.h>
#include <share.h>
#include <istream>
#include <fstream>
#include <string>
#include <Utility/stringi.h>

#include <process.h>
#include <time.h>

#include "bcVM/vmBuiltinTusClient.h"
#include "Target/vmTask.h"
#include "bcVM/bcVM.h"
#include "Utility/funcky.h"
#include "Target/vmConf.h"
#include "fileCache.h"
#include "vmConf.h"


/* returns a human-readable error message or 0 on success */


enum class configSections {
	CONFIG_VM,
	CONFIG_CONFIG,
	CONFIG_SERVER,
	CONFIG_MODULE,
	CONFIG_RUN,
	CONFIG_DATABASE,
	CONFIG_SCHEDULED
};

struct flagId
{
	char const			*id;
	int					 len;
	configSections		 bitPos;
};

static flagId areas[]	=	{	
								{ "CONFIG",		6, configSections::CONFIG_CONFIG },
								{ "VM",			6, configSections::CONFIG_VM },
								{ "SERVER",		6, configSections::CONFIG_SERVER },
								{ "DATABASE",	8, configSections::CONFIG_DATABASE },
								{ "MODULE",		6, configSections::CONFIG_MODULE },
								{ "RUN",		6, configSections::CONFIG_RUN },
								{ "SCHEDULED",	9, configSections::CONFIG_SCHEDULED },
								{ 0,			0, configSections::CONFIG_CONFIG }
							};

VM_CONF		vmConf;

void vmConfig ( std::string const &fName )
{
	char						 fileName[MAX_PATH + 1];
	size_t						 lRet;
	char						*dummy;
	int							 loop;
	configSections				 area;
	char						 inLine[256];
	char						 name[256];
	char						 value[256];
	char						 subValue1[256];
	char						 subValue2[256];
	char						 subValue3[256];
	char						 subValue4[256];
	NETRESOURCE					 nr;
	HANDLE						 localToken;
	uint32_t					 port;
	SYSTEM_INFO		 systemInfo;

	GetSystemInfo ( &systemInfo );

	bool avxSupported = false;

#if !defined ( __clang__ )
	// If Visual Studio 2010 SP1 or later
#if (_MSC_FULL_VER >= 160040219)
	// Checking for AVX requires 3 things:
	// 1) CPUID indicates that the OS uses XSAVE and XRSTORE
	//     instructions (allowing saving YMM registers on context
	//     switch)
	// 2) CPUID indicates support for AVX
	// 3) XGETBV indicates the AVX registers will be saved and
	//     restored on context switch
	//
	// Note that XGETBV is only available on 686 or later CPUs, so
	// the instruction needs to be conditionally run.
	int cpuInfo[4];
	__cpuid ( cpuInfo, 1 );

	bool osUsesXSAVE_XRSTORE = cpuInfo[2] & (1 << 27) || false;
	bool cpuAVXSuport = cpuInfo[2] & (1 << 28) || false;

	if ( osUsesXSAVE_XRSTORE && cpuAVXSuport )
	{
		// Check if the OS will save the YMM registers
		unsigned long long xcrFeatureMask = _xgetbv ( _XCR_XFEATURE_ENABLED_MASK );
		avxSupported = (xcrFeatureMask & 0x6) || false;
	}
#endif
#else
	avxSupported = true;
#endif

	if ( !avxSupported )
	{
		printf ( "AVX is NOT supported, this application can not run on this machine\n" );
		Sleep ( 10000 );
		exit ( 0 );
	}

	vmConf.warnDisable.resize ( int( warnNum::WARN_MAX_WARN) );

	vmConf.vmCacheEntry				= 32ull * 1024;		// defaults to 32K cache entries
	vmConf.vmCacheLargeFileSize		= 0;			// use cache default (16M)
	vmConf.vmCacheMaxSize			= 0;			// use cache default (1G)

	vmConf.useDebugAdapter			= true;
	vmConf.debuggerEnabled			= true;
	vmConf.consoleVisible			= true;			
	vmConf.profilerEnabled			= false;

	vmConf.nServerThreads			= systemInfo.dwNumberOfProcessors * 4;			// defaults to 1 iocp server worker thread
	vmConf.serverConcurrency		= 0;											// defaults to all processors
	vmConf.serverShutdownWaitTime	= 10;											// defaults to 10 seconds

	vmConf.vmCollector				= vmCollectorType::colGenerational;
	vmConf.vmObjectDefaultPageSize	= 32ull *1024;
	vmConf.vmObjectMemoryMax		= 512ull * 1024 * 1024;

	vmConf.dbServerThreads			= systemInfo.dwNumberOfProcessors * 4;		// number of worker threads, default to 4 x num procs to allow for waiting
	vmConf.dbDefaultDb	= vmFglDbList ( 3102, "127.0.0.1" );

	vmConf.inlineCode	= true;
	vmConf.genDebug		= true;
	vmConf.genProfiler	= false;
	vmConf.genListing	= false;

	vmConf.clientToken	= 0;
	
	std::ifstream iStream ( fName.c_str() );
	if ( iStream.bad ( ) )
	{
		printf ( "Unable to open configuration file... starting server with defaults only\r\n" );
		return;
	}

	GetFullPathName ( fName.c_str ( ), MAX_NAME_SZ, fileName, &dummy );

	while ( !iStream.eof ( ) && iStream.good() )
	{
		std::string line;
		std::getline ( iStream, line );

		strcpy_s ( inLine, sizeof (inLine), line.c_str ( ) );

		_alltrim ( inLine, inLine );

		if ( *inLine == '#' )
		{
			continue;
		}

		if ( *inLine == '['  )
		{
			trimpunct ( inLine, inLine );
			_alltrim ( inLine, inLine );

			/* look for a pre-declared section name */
			for ( loop = 0; areas[loop].id; loop++ )
			{
				if ( !strccmp ( areas[loop].id, inLine ) )
				{
					switch ( areas[loop].bitPos )
					{
						case configSections::CONFIG_MODULE:
						case configSections::CONFIG_CONFIG:
						case configSections::CONFIG_SERVER:
						case configSections::CONFIG_DATABASE:
						case configSections::CONFIG_RUN:
						case configSections::CONFIG_VM:
						case configSections::CONFIG_SCHEDULED:
							area = (configSections)areas[loop].bitPos;
							break;
					}
					break;
				}
			}
			if ( !areas[loop].id )
			{
				printf ( "error: unknown section name %s\r\n", inLine );
			}
		} else if ( *inLine )
		{
			switch ( area )
			{
				case configSections::CONFIG_VM:
					_token ( inLine, " =", 1, name );
					strcpy_s ( value, sizeof ( value ), inLine + _attoken ( inLine, " =", 1 ) + 1 );

					_alltrim ( name, name );
					_alltrim ( value, value );

					if ( !strccmp ( name, "genDebug" ) )
					{
						if ( !strccmp ( value, "on" ) )
						{
							vmConf.genDebug = true;
						} else if ( !strccmp ( value, "off" ) )
						{
							vmConf.genDebug = false;
						} else
						{
							printf ( "error: genDebug must be either ON or OFF\r\n" );
						}
					} else if ( !strccmp ( name, "genProfile" ) )
					{
						if ( !strccmp ( value, "on" ) )
						{
							vmConf.genProfiler = true;
						} else if ( !strccmp ( value, "off" ) )
						{
							vmConf.genProfiler = false;
						} else
						{
							printf ( "error: genProfile must be either on or off\r\n" );
						}
					} else if ( !strccmp ( name, "genListing" ) )
					{
						if ( !strccmp ( value, "on" ) )
						{
							vmConf.genListing = true;
						} else if ( !strccmp ( value, "off" ) )
						{
							vmConf.genListing = false;
						} else
						{
							printf ( "error: genListing must be either on or off\r\n" );
						}
					} else if ( !strccmp ( name, "inlineCode" ) )
					{
						if ( !strccmp ( value, "on" ) )
						{
							vmConf.inlineCode = true;
						} else if ( !strccmp ( value, "off" ) )
						{
							vmConf.inlineCode = false;
						} else
						{
							printf ( "error: inlineCode must be either ON or OFF\r\n" );
						}
					}
					break;
				case configSections::CONFIG_CONFIG:
					_token ( inLine, " =", 1, name );
					strcpy_s ( value, sizeof ( value ), inLine + _attoken ( inLine, " =", 1 ) + 1 );

					_alltrim ( name, name );
					_alltrim ( value, value );

					if ( !strccmp ( name, "debugger" )  )
					{
						_token ( value, ",", 1, subValue1 );		// drive letter	
						_token ( value, ",", 2, subValue2 );		// network location	(unc path)

						if ( !strccmp ( subValue1, "on" ) )
						{
							vmConf.useDebugAdapter = false;
							vmConf.debuggerEnabled = true;
							vmConf.genDebug = true;
						} else if ( !strccmp ( subValue1, "off" ) )
						{
							vmConf.useDebugAdapter = false;
							vmConf.debuggerEnabled = false;
						} else if ( !strccmp ( subValue1, "adapter" ) )
						{
							vmConf.useDebugAdapter = true;
							vmConf.genDebug = true;
							if ( *subValue2 )
							{
								vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::debug, subValue2 ) );
							} else
							{
								vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::debug, "9772" ) );
							}
						} else
						{
							printf ( "error: debugger must be either ON, OFF or ADAPTER\r\n" );
						}
					} else if ( !strccmp ( name, "language_server" ) )
					{
						_token ( value, ",", 1, subValue1 );		// drive letter	
						_token ( value, ",", 2, subValue2 );		// network location	(unc path)

						if ( !strccmp ( subValue1, "off" ) )
						{
							vmConf.debuggerEnabled = false;
						} else if ( !strccmp ( subValue1, "on" ) )
						{
							vmConf.debuggerEnabled = true;
							vmConf.useDebugAdapter = false;
							vmConf.genDebug = true;
						} else if ( !strccmp ( subValue1, "adapter" ) )
						{
							vmConf.useDebugAdapter = true;
							vmConf.genDebug = true;
							if ( *subValue2 )
							{
								vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::language, subValue2 ) );
							} else
							{
								vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::language, "6996" ) );
							}
						} else
						{
							printf ( "error: debugger must be either ON, OFF or ADAPTER\r\n" );
						}
					} else if ( !strccmp ( name, "console" ) )
					{
						if ( !strccmp ( value, "visible" ) )
						{
							vmConf.consoleVisible = true;
						} else if ( !strccmp ( value, "hidden" ) )
						{
							vmConf.consoleVisible = false;
						} else
						{
							printf ( "error: console must be either VISIBLE or HIDDEN\r\n" );
						}
					} else if ( !strccmp( name, "collector" ) )
					{
						if ( !strccmp( value, "GEN" ) )
						{
							vmConf.vmCollector = vmCollectorType::colGenerational;
						} else if ( !strccmp( value, "COPY" ) )
						{
							vmConf.vmCollector = vmCollectorType::colCopy;
						} else
						{
							printf( "error: console must be either VISIBLE or HIDDEN\r\n" );
						}
					} else if ( !strccmp ( name, "profiler" ) )
					{
						if ( !strccmp ( value, "on" ) )
						{
							vmConf.profilerEnabled = true;
							vmConf.genProfiler = true;
						} else if ( !strccmp ( value, "off" ) )
						{
							vmConf.profilerEnabled = false;
						} else
						{
							printf ( "error: profiler must be either ON or OFF\r\n" );
						}
					} else if ( !strccmp ( name, "clean_start" ) )
					{
						if ( !strccmp ( value, "on" ) )
						{
							vmConf.cleanStart = true;
						} else if ( !strccmp ( value, "off" ) )
						{
							vmConf.cleanStart = false;
						} else
						{
							printf ( "error: clean_start must be either ON or OFF\r\n" );
						}
					} else if ( !strccmp ( name, "warn_disable" ) )
					{
						auto warnNum = atoi ( value );
						if ( warnNum < vmConf.warnDisable.size ( ) )
						{
							vmConf.warnDisable[warnNum] = true;
						} else
						{
							printf ( "error: maximum warning number is: %I64u\r\n", vmConf.warnDisable.size ( ) );
						}
					} else if ( !strccmp ( name, "editor" ) )
					{
						vmConf.editor = value;
					} else if ( !strccmp ( name, "MAP_DRIVE" ) && *value )
					{
						_token ( value, ",", 1, subValue1 );		// drive letter	
						_token ( value, ",", 2, subValue2 );		// network location	(unc path)
						_token ( value, ",", 3, subValue3 );		// remote account name
						_token ( value, ",", 4, subValue4 );		// remote account password

						_alltrim ( subValue1, subValue1 );
						_alltrim ( subValue2, subValue2 );
						_alltrim ( subValue3, subValue3 );
						_alltrim ( subValue4, subValue4 );

						if ( !vmConf.clientToken )
						{
							if ( LogonUser ( subValue3, ".", subValue4, LOGON32_LOGON_NETWORK_CLEARTEXT, LOGON32_PROVIDER_WINNT50, &localToken ) )
							{
								// logged on our service user... now impersonate his credentials

								ImpersonateLoggedOnUser(localToken );

								// set it so all our future tasks will impersonate automatically
								vmConf.clientToken = localToken;
							}
						}

						memset ( &nr, 0, sizeof ( nr ) );
						nr.dwScope			= RESOURCE_GLOBALNET;
						nr.dwType			= RESOURCETYPE_DISK;
						nr.lpRemoteName		= subValue2;
						nr.lpLocalName		= subValue1;

						if ( (lRet = WNetAddConnection2 ( & nr, subValue4, subValue3, CONNECT_REDIRECT )) != NO_ERROR )
						{
							switch ( lRet )
							{
								case ERROR_ALREADY_ASSIGNED:
									lRet = sizeof ( subValue3 );
									if ( WNetGetConnection ( subValue1, subValue3, (LPDWORD)&lRet) != NO_ERROR )
									{
										printf ( "error: unable to map network connection %s to %s with error %lu\r\n", subValue2, subValue1, GetLastError() );
									} else
									{
										if ( strccmp ( subValue3, subValue2 ) )
										{
											printf ( "error: network connection %s is mapped to %s which differs from %s\r\n", subValue1, subValue3, subValue2 );
										}
									}
									break;
								default:
									printf ( "error: unable to map network connection %s to %s with error %lu\r\n", subValue2, subValue1, GetLastError() );
									break;
							}
						}
					} else if ( !strccmp ( name, "RUN_AS" ) && *value )
					{
						_token ( value, ",", 1, subValue1 );		// local account name
						_token ( value, ",", 2, subValue2 );		// local account password

						_alltrim ( subValue1, subValue1 );
						_alltrim ( subValue2, subValue2 );

						if ( !LogonUser ( subValue1, ".", subValue2, LOGON32_LOGON_NETWORK_CLEARTEXT, LOGON32_PROVIDER_WINNT50, &localToken ) )
						{
							printf ( "error: unable to impersonate local account %s\r\n", subValue1 );
							break;
						}

						ImpersonateLoggedOnUser(localToken );

						vmConf.clientToken = localToken;
					} else if ( !strccmp ( name, "CACHE_ENTRIES" ) && *value )
					{
						vmConf.vmCacheEntry = atoi ( value );
						if ( vmConf.vmCacheEntry < 16 )
						{
							printf ( "error: cache entries must be atleast 16\r\n" );
							vmConf.vmCacheEntry = 16;
						}
					} else if ( !strccmp ( name, "CACHE_SIZE" ) && *value )
					{
						vmConf.vmCacheMaxSize = _atoi64 ( value ) * 1024 * 1024;
						if ( vmConf.vmCacheMaxSize < 1 )
						{
							printf ( "error: cache entries must be atleast 1\r\n" );
							vmConf.vmCacheEntry = 1;
						}
					} else if ( !strccmp ( name, "CACHE_MAX_ENTRY_SIZE" ) && *value )
					{
						vmConf.vmCacheLargeFileSize = _atoi64 ( value ) * 1024 * 1024;
					} else if ( !strccmp ( name, "TUS_MANAGER_DATA" ) && *value )
					{
						vmConf.tusSendFileStorage = value;
					} else if ( !strccmp ( name, "WATCH" ) && *value )
					{
						_token ( value, ",", 1, subValue1 );		// directory
						_token ( value, ",", 2, subValue2 );		// url
						_alltrim ( subValue1, subValue1 );
						_alltrim ( subValue2, subValue2 );

						auto handle = CreateFile ( subValue1,
											  FILE_LIST_DIRECTORY,
											  FILE_SHARE_READ | FILE_SHARE_WRITE,
											  NULL,
											  OPEN_EXISTING,
											  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
											  NULL
						);

						if ( handle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
						{
							printf ( "error: unable to watch directory \"%s\" - error: %lu\r\n", subValue1, GetLastError ( ) );
						} else
						{
							vmConf.watch.push_back ( vmDirectoryWatch ( subValue1, subValue2, handle ) );
						}
					} else if ( !strccmp ( name, "LIBRARY" ) )
					{
						vmConf.preloadLibrary.push_back ( vmLibrary ( value ) );
					}
					break;
				case configSections::CONFIG_SERVER:
					_token ( inLine, "= ", 1, name );
					_token ( inLine, "= ", 2, value );

					_alltrim ( name, name );
					_alltrim ( value, value );

					if ( !strccmp ( name, "SERVER_CONCURRENCY" ) )
					{
						vmConf.serverConcurrency = atol ( value );
					} else if ( !strccmp ( name, "SERVER_THREADS" ) )
					{
						vmConf.nServerThreads = atol ( value );
					} else if ( !strccmp ( name, "SHUTDOWN_WAIT_TIME" ) )
					{
						vmConf.serverShutdownWaitTime = atol ( value ) * 1000;
					} else if ( !strccmp ( name, "SERVER" ) )
					{
						_token ( inLine, "= ", 3, subValue1 );

						if ( !*subValue1  )
						{
							printf ( "error: server <http|https|db> <iniFile>\r\n" );
						} else
						{
							if ( !strccmp ( value, "http" ) )
							{
								vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::http, subValue1 ) );
							} else if ( !strccmp ( value, "https" ) )
							{
								vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::https, subValue1 ) );
							} else if ( !strccmp ( value, "db" ) )
							{
								vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::db, subValue1 ) );
							}
						}
					}
					break;
				case configSections::CONFIG_RUN:
					_token ( inLine, "=", 1, name );

					_alltrim ( name, name );
					_alltrim ( inLine + strlen ( name ), inLine + strlen ( name ) );

					vmConf.autoRun.push_back ( vmAutorun ( name, inLine + strlen ( name ) ) );
					break;
				case configSections::CONFIG_MODULE:
					_alltrim ( inLine, inLine );

					try {
						HINSTANCE hInst;
						PFN_SLANG_EXTENSION_INIT extensionInit;
						PFN_SLANG_INSTANCE_END	 extensionThreadEnd;
						PFN_SLANG_INSTANCE_INIT extensionThreadInit;

						if ( (hInst = LoadLibrary ( name )) == NULL )
						{
							printf ( "Error: Unable to load dll %s - error number %lu\r\n", name, GetLastError ( ) );
							break;
						}

						if ( !(extensionInit = (PFN_SLANG_EXTENSION_INIT) GetProcAddress ( hInst, "slangExtensionInit" )) )
						{
							printf ( "Error: Invalid SLANG Extension DLL %s\r\n", name );
							break;
						}

						SLANG_EXTENSION_VERSION ver;

						extensionInit ( &ver );

						if ( !(extensionThreadInit = (PFN_SLANG_INSTANCE_INIT) GetProcAddress ( hInst, "slangInstanceInit" )) )
						{
							printf ( "Error: Invalid SLANG Extension DLL %s\r\n", name );
							break;
						}
						if ( !(extensionThreadEnd = (PFN_SLANG_INSTANCE_END) GetProcAddress ( hInst, "slangInstancEnd" )) )
						{
							printf ( "Error: Invalid SLANG Extension DLL %s\r\n", name );
							break;
						}

						vmConf.modules.push_back ( vmModule ( inLine, hInst, extensionThreadInit, extensionThreadEnd ) );

						printf ( "Loading SLANG Extension DLL: %s  - version %i\r\n", ver.name, ver.version );
					} catch ( ... )
					{
						printf ( "error: error loading module %s\r\n", inLine );
					}
					break;
				case configSections::CONFIG_DATABASE:
					_token ( inLine, "=", 1, name );
					_token ( inLine, "=", 2, value );

					_alltrim ( name, name );
					_alltrim ( value, value );

					if ( !strccmp ( name, "MAX_CONNECTIONS" ) && *value )
					{
						vmConf.dbMaxConnections = atoi ( value );
						if ( vmConf.dbMaxConnections < 1 )
						{
							printf ( "error: must have atleast 1 connection \r\n" );
							vmConf.dbMaxConnections = 1;
						}
					} else if ( !strccmp ( name, "CONCURRENCY" ) )
					{
						vmConf.dbServerConcurrency = atol ( value );
					} else if ( !strccmp ( name, "THREADS" ) )
					{
						vmConf.dbServerThreads = atol ( value );
					} else if ( !strccmp ( name, "SHUTDOWN_WAIT_TIME" ) )
					{
						vmConf.dbServerShutdownWaitTime = atol ( value ) * 1000;
					} else if ( !strccmp ( name, "TIMEOUT" ) )
					{
						vmConf.dbReqTimeout.tv_usec = 0;
						vmConf.dbReqTimeout.tv_sec = atol ( value );
					} else 	if ( !strccmp ( name, "DEFAULT_IP" ) && *value )
					{
						_token ( value, ":", 1, subValue1 );
						_token ( value, ":", 2, subValue2 );

						_alltrim ( subValue1, subValue1 );
						_alltrim ( subValue2, subValue2 );

						if ( !*subValue2 )
						{
							port = 3102;
						} else
						{
							port = atoi ( subValue2 );
						}

						vmConf.dbDefaultDb = vmFglDbList ( port, subValue1 );
					} else 
					{

						_token ( value, ":", 1, subValue1 );
						_token ( value, ":", 2, subValue2 );

						_alltrim ( subValue1, subValue1 );
						_alltrim ( subValue2, subValue2 );

						if ( !*subValue2 )
						{
							port = 3102;
						} else
						{
							port = atoi ( subValue2 );
						}

						vmConf.dbList[name] = vmFglDbList ( port, subValue1, name );
					}
					break;
				case configSections::CONFIG_SCHEDULED:
					_token ( inLine, "=", 1, name );
					strcpy_s ( value, sizeof ( value ), inLine + _attoken ( inLine, "=", 1 ) + 1 );

					_alltrim ( name, name );
					_upper ( name, name );

					_token ( value, ";", 1, subValue2 );
					strcpy_s ( subValue1, sizeof ( subValue1 ), value + _attoken ( value, ";", 1 ) + 1 );

					_alltrim ( subValue1, subValue1 );
					_alltrim ( subValue2, subValue2 );

					vmScheduler::schedType	type;
					if ( !strccmp ( name, "EXE" ) && *value )
					{
						type = vmScheduler::schedType::exe;
					} else 	if ( !strccmp ( name, "AP" ) && *value )
					{
						type = vmScheduler::schedType::ap;
					} else
					{
						printf ( "error: <exe|ap> <name> <every|at> <hour>:<min>\r\n" );
						break;
					}

					if ( !memcmpi ( subValue2, "every ", 6 ) )
					{
						memmove ( subValue2, subValue2 + 6, strlen ( subValue2 + 6 ) );
						_alltrim ( subValue2, subValue2 );

						uint32_t hour, min;
						sscanf_s ( subValue2, "%02i:%02i", &hour, &min );

						vmConf.schedule.push_back ( vmScheduler  ( subValue1, type, hour * 3600 + min * 60 ) );
					} else if ( !memcmpi ( subValue2, "at ", 3 ) )
					{
						memmove ( subValue2, subValue2 + 3, strlen ( subValue2 + 3 ) );
						_alltrim ( subValue2, subValue2 );

						uint32_t hour, min;
						sscanf_s ( subValue2, "%02i:%02i", &hour, &min );
						
						vmConf.schedule.push_back ( vmScheduler  ( subValue1, type, hour, min ) );
					} else
					{
						printf ( "error: <exe|ap> <name> <every|at> <hour>:<min>\r\n" );
						break;
					}
					break;
				default:
					printf ( "error: not in section\r\n" );
					break;
			}
		}
	}
}

