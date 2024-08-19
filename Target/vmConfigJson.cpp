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
#include "Utility/jsonParser.h"


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

	GetFullPathName ( fName.c_str (), MAX_NAME_SZ, fileName, &dummy );

	std::ifstream input ( fName.c_str() );

	// copies all data into buffer
	std::string buffer ( std::istreambuf_iterator<char> ( input ), {} );


	auto json = jsonElement ( buffer );

	// vm section
	if ( json.has ( "vm" ) )
	{
		auto vm = json["vm"];

		vmConf.genDebug = (bool) vm.conditionalGet ( "genDebug", false );
		vmConf.genProfiler = (bool) vm.conditionalGet ( "genProfile", false );
		vmConf.genListing = (bool) vm.conditionalGet ( "genListing", false );
		vmConf.inlineCode = (bool) vm.conditionalGet ( "inlineCode", false );
		vmConf.inlineCode = (bool) vm.conditionalGet ( "profiler", false );
	}
	
	// config section
	if ( json.has ( "config" ) )
	{
		auto conf = json["config"];
		if ( conf.has ( "debugger" ) )
		{
			auto debugger = conf["debugger"];
			vmConf.useDebugAdapter = (bool) debugger.conditionalGet ( "enable", false );
			if ( vmConf.useDebugAdapter )
			{
				int64_t port = debugger.conditionalGet ( "port", 9772 );
				vmConf.debuggerEnabled = true;
				vmConf.genDebug = true;
				vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::debug, stringi ( port ).c_str () ) );
			}
		}
		if ( conf.has ( "language_server" ) )
		{
			auto ls = conf["language_server"];
			if ( (bool) ls.conditionalGet ( "enable", false ); )
			{
				int64_t port = ls.conditionalGet ( "port", 6996 );
				vmConf.servers.push_back ( vmServers ( vmServers::vmServerType::debug, stringi ( port ).c_str () ) );
			}
		}
		if ( conf.has ( "console" ) )
		{
			if ( (stringi) conf["console"] == "visible" )
			{
				vmConf.consoleVisible = true;
			} else if ( (stringi) conf["console"] == "hidden" )
			{
				vmConf.consoleVisible = false;
			} else
			{
				printf ( "error: console must be either VISIBLE or HIDDEN\r\n" ); \
			}
		}
		if ( conf.has ( "collector" ) )
		{
			if ( (stringi) conf["collector"] == "generational" )
			{
				vmConf.vmCollector = vmCollectorType::colGenerational;
			} else if ( (stringi) conf["collector"] == "vmCollectorType" )
			{
				vmConf.vmCollector = vmCollectorType::colCopy;
			} else
			{
				printf ( "error: collector must be either GENERATIONAL or COPY\r\n" );
			}
		}
		vmConf.cleanStart = conf.conditionalGet ( "clean_start", true );
		if ( conf.has ( "warn_disable" ) )
		{
			for ( auto it = conf["warn_disable"].cbeginArray (); it != conf["warn_disable"].cendArray (); it++ )
			{
				int64_t warnNum = *it;
				if ( warnNum < vmConf.warnDisable.size () )
				{
					vmConf.warnDisable[warnNum] = true;
				} else
				{
					printf ( "error: maximum warning number is: %I64u\r\n", vmConf.warnDisable.size () );
				}

			}
		}
		if ( conf.has ( "map_drive" ) )
		{
			auto mapping = conf["map_drive"];
			stringi drive = mapping.conditionalGet ( "drive", stringi () );
			stringi uncPath = mapping.conditionalGet ( "location", stringi () );
			stringi userName = mapping.conditionalGet ( "userName", stringi () );
			stringi password = mapping.conditionalGet ( "password", stringi () );

			memset ( &nr, 0, sizeof ( nr ) );
			nr.dwScope = RESOURCE_GLOBALNET;
			nr.dwType = RESOURCETYPE_DISK;
			nr.lpRemoteName = (LPSTR) uncPath.c_str ();
			nr.lpLocalName = (LPSTR) drive.c_str ();

			if ( (lRet = WNetAddConnection2 ( &nr, (LPCSTR) userName.c_str (), (LPCSTR) password.c_str (), CONNECT_REDIRECT )) != NO_ERROR )
			{
				switch ( lRet )
				{
					case ERROR_ALREADY_ASSIGNED:
						{
							char inUseName[MAX_PATH];
							lRet = sizeof ( inUseName );
							if ( WNetGetConnection ( drive, inUseName, (LPDWORD) &lRet ) != NO_ERROR )
							{
								printf ( "error: unable to map network connection %s to %s with error %s\r\n", uncPath.c_str (), drive.c_str (), scCompErrorAsText ( GetLastError () ) );
							} else
							{
								if ( strccmp ( subValue3, subValue2 ) )
								{
									printf ( "error: network connection %s is mapped to %s which differs from %s\r\n", drive.c_str (), inUseName, uncPath.c_str () );
								}
							}
						}
						break;
					default:
						printf ( "error: unable to map network connection %s to %s with error %s\r\n", uncPath.c_str (), drive.c_str (), scCompErrorAsText ( GetLastError () ) );
						break;
				}
			}
		}
		if ( conf.has ( "run_as" ) )
		{
			auto credentials = conf["run_as"];

			stringi userName = credentials.conditionalGet ( "userName", stringi () );
			stringi password = credentials.conditionalGet ( "password", stringi () );

			if ( !LogonUser ( userName.c_str (), ".", password.c_str (), LOGON32_LOGON_NETWORK_CLEARTEXT, LOGON32_PROVIDER_WINNT50, &localToken ) )
			{
				printf ( "error: unable to impersonate local account %s\r\n", subValue1 );
			}

			ImpersonateLoggedOnUser ( localToken );

			vmConf.clientToken = localToken;
		}
		vmConf.vmCacheEntry = conf.conditionalGet ( "cache_entries", 256 );
		vmConf.vmCacheMaxSize = (int64_t)conf.conditionalGet ( "cache_size", 1 ) * 1024 * 1024;
		vmConf.vmCacheLargeFileSize = (int64_t)conf.conditionalGet ( "cache_large_file_size", 1 ) * 1024 * 1024;
		vmConf.tusSendFileStorage = (std::string) (conf.conditionalGet ( "tus_manager_data", std::string() );
		if ( conf.has ( "watch_directory" ) )
		{
			for ( auto it = conf["watch_directory"].cbeginArray (); it != conf["watch_directory"].cendArray (); it++ )
			{
				stringi directory = (*it)["directory"];
				stringi url = (*it)["url"];

				auto handle = CreateFile ( directory.c_str(),
										FILE_LIST_DIRECTORY,
										FILE_SHARE_READ | FILE_SHARE_WRITE,
										NULL,
										OPEN_EXISTING,
										FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
										NULL
				);

				if ( handle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
				{
					printf ( "error: unable to watch directory \"%s\" - error: %s\r\n", directory.c_str(), scCompErrorAsText( GetLastError() ) );
				} else
				{
					vmConf.watch.push_back ( vmDirectoryWatch ( directory, url, handle ) );
				}

			}
		}
	}

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

