/*
	engine config header file

*/
#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <stdint.h>
#include <string>
#include <cassert>
#include <unordered_map>
#include <windows.h>
#include "Utility/util.h"
#include "Utility/funcky.h"
#include "Target/common.h"

struct vmCacheMonitor {
	std::string		directory;

	vmCacheMonitor()  noexcept
	{};
	vmCacheMonitor ( std::string &dir ) : directory ( dir ) {};
	vmCacheMonitor ( char const *dir ) : directory ( dir ) {};
};

struct vmDirectoryWatch {
	OVERLAPPED		 ov;
	std::string		 directory;
	HANDLE			 dirHandle;
	std::string		 url;
	union {
		struct _FILE_NOTIFY_INFORMATION	fi;
		char							data[4096];
	} dat;

	vmDirectoryWatch ( char const *dir, char const *url, HANDLE handle ) : directory ( dir ), dirHandle ( handle ), url ( url )
	{
		memset ( &ov, 0, sizeof ( ov ) );
	}
};

struct vmFglDbList {
	std::string				 tableName;
	uint32_t				 port = 0;
	uint32_t				 ip = 0;
	std::string				 host;
	bool					 used = false;

	vmFglDbList() noexcept
	{};
	vmFglDbList ( uint32_t port, std::string host ) : port ( port ), host ( host ) 
	{
		ip = webAddrFromHost ( host.c_str() );
		used = false;
	};
	vmFglDbList ( uint32_t port, char const *host ) : port ( port ), host ( host )
	{
		ip = webAddrFromHost ( host );
		used = false;
	};
	vmFglDbList ( uint32_t port, std::string host, std::string tableName ) : tableName ( tableName ), port ( port ), host ( host )
	{
		ip = webAddrFromHost ( host.c_str() );
		used = false;
	};
	vmFglDbList ( uint32_t port, char const *host, const char *tableName ) : tableName ( tableName ), port ( port ), host ( host )
	{
		ip = webAddrFromHost ( host );
		used = false;
	};

#if 0
	if ( _isnum ( subValue1 ) )
	{
		ip = _AToIp ( subValue1 );
	} else
	{
		ip = webAddrFromHost ( subValue1 );
	}
#endif
};

struct SLANG_EXTENSION_VERSION {
	uint32_t		version;
	char			name[MAX_PATH + 1];
};

typedef BOOL ( _cdecl * PFN_SLANG_EXTENSION_INIT )	(struct SLANG_EXTENSION_VERSION *pVer);
typedef void ( _cdecl * PFN_SLANG_INSTANCE_END)		();
typedef BOOL ( _cdecl * PFN_SLANG_INSTANCE_INIT )	(class vmInstance *instance);


struct vmModule {
	std::string					 name;
	HINSTANCE					 hInst = 0;
	PFN_SLANG_INSTANCE_INIT		 instanceInit = nullptr;
	PFN_SLANG_INSTANCE_END		 instanceEnd = nullptr;

	vmModule() noexcept
	{};
	vmModule ( std::string name, HINSTANCE hInst, PFN_SLANG_INSTANCE_INIT reg, PFN_SLANG_INSTANCE_END end ) : name ( name ), hInst ( hInst ), instanceInit ( reg  ), instanceEnd ( end )
	{
		load();
	}

	vmModule ( char const *name ) : name ( name )
	{
		load();
	}

	void load ( void ) noexcept
	{
	}
};

struct vmLibrary {
	std::string					name;

	vmLibrary() noexcept
	{};
	vmLibrary ( std::string &name ) : name ( name ) {} ;
	vmLibrary ( char const *name ) : name ( name ) {} ;
};

struct vmAutorun {
	std::string					name;
	std::string					params;

	vmAutorun() noexcept
	{};
	vmAutorun ( std::string &name, std::string &params ) : name ( name ), params ( params ) {} ;
	vmAutorun ( char const *name, char const *params ) : name ( name ), params ( params ) {} ;
};

struct vmScheduler {
	std::string					 name;
	enum class schedType
	{
		exe,
		ap
	}							 type = schedType::ap;
	enum class schedPeriod
	{
		fixed,
		count
	}							 periodType = schedPeriod::fixed;
	union 
	{
		struct	tm				 time = { 0 };
		struct
		{
			time_t				 nextRun;
			uint32_t			 periodSeconds;
		};
	};

	vmScheduler ( ) noexcept : type ( vmScheduler::schedType::exe ), periodType ( schedPeriod::fixed ), time ( { 0 } )
	{
	};
	vmScheduler ( std::string const &name, schedType type, uint32_t seconds ) : name ( name ), type ( type )
	{
		periodType		= schedPeriod::count;
		periodSeconds	= seconds;
		nextRun			= 0;
	}
	vmScheduler ( char const *name, schedType type, uint32_t seconds ) : name ( name ), type ( type )
	{
		periodType		= schedPeriod::count;
		periodSeconds	= seconds;
		nextRun			= 0;
	}
	vmScheduler ( std::string const &name, schedType type, uint32_t hour, uint32_t minute ) : name ( name ), type ( type )
	{
		periodType		= schedPeriod::fixed;
		memset ( &time, 0, sizeof ( time ) );
		time.tm_hour	= hour;
		time.tm_min		= minute;
	}
	vmScheduler ( char const *name, schedType type, uint32_t hour, uint32_t minute ) : name ( name ), type ( type )
	{
		periodType		= schedPeriod::fixed;
		memset ( &time, 0, sizeof ( time ) );
		time.tm_hour	= hour;
		time.tm_min		= minute;
	}
};

struct vmServers {
	enum class vmServerType {
		http,
		https,
		ftp,
		db,
		language,
		debug,
	} type;
	std::string					 iniFile;

	vmServers() noexcept : type ( vmServerType::https ) {};
	vmServers ( vmServerType type, std::string &iniFile ) : type ( type ), iniFile ( iniFile ) {};
	vmServers ( vmServerType type, char const *iniFile ) : type ( type ), iniFile ( iniFile ) {};
};

enum class vmCollectorType
{
	colGenerational,
	colCopy,
};
struct VM_CONF {
	/* general configuration information */
	std::string					 configFileName;

	std::vector<vmServers>		 servers;					/* list of servers to startup								*/
	std::vector<vmModule>		 modules;					/* list of extension modules to load						*/
	std::vector<vmAutorun>		 autoRun;					/* list of executables or ap pages to load at init			*/
	std::vector<vmLibrary>		 preloadLibrary;			/* list of libraries to load directly into the server		*/
	std::vector<vmScheduler>	 schedule;					/* scheduled operations */
	std::vector<bool>			 warnDisable;

	/* debugger related */
	bool						 debuggerEnabled = false;
	bool						 profilerEnabled = false;
	bool						 consoleVisible = true;
	std::string					 editor;

	// use debug adapter and specify the port
	bool						 useDebugAdapter = false;
	uint16_t					 debugAdapterPort = 9772;

	/* vm configuration */
	vmCollectorType				 vmCollector = vmCollectorType::colGenerational;
	size_t						 vmObjectDefaultPageSize = 32 * 1024;
	size_t						 vmObjectMemoryMax = 512 * 1024 * 1024;

	/* vmCache related */
	uint64_t					 vmCacheEntry = 32 * 1024;
	uint64_t					 vmCacheMaxSize = 0;				/* use config default */
	uint64_t					 vmCacheLargeFileSize = 0;			/* use config default */

	/* fgl database related */
	uint32_t					 dbMaxConnections = 64;				/* maximum number of connections per database				*/
	struct timeval				 dbReqTimeout = { 500, 0 };				/* database request timeout									*/
	struct timeval				 dbReqBigTimeout = { 6000, 0 };			/* database request timeout for long execution statements	*/
	vmFglDbList					 dbDefaultDb;
	uint32_t					 dbServerThreads = 1;				/* number of server worker threads							*/
	uint32_t					 dbServerConcurrency = 0;			/* worker thread concurrency level							*/
	uint32_t					 dbServerShutdownWaitTime = 0;		/* server shutdown wait time								*/
	std::unordered_map<std::string,vmFglDbList>	dbList;

	/* service related */
	HANDLE						 clientToken =  nullptr;			/* user to impersonate as									*/

	bool						 cleanStart = false;				/* require a clean *no error* startup to continue			*/
	bool						 abortStart = false;				/* set if clean startup requires us to abort				*/	

	/* tus send file */
	std::string					 tusSendFileStorage;				/* tus sendFile disk storage name							*/

	/* directory change monitoring */
	std::vector<vmDirectoryWatch>	watch;

	/* general vm */
	uint32_t					 nServerThreads = 1;				/* number of server worker threads							*/
	uint32_t					 serverConcurrency = 0;				/* worker thread concurrency level							*/
	uint32_t					 serverShutdownWaitTime = 0;		/* server shutdown wait time								*/

    /* compiler & optimizer configuration */
	bool						 inlineCode = false;
	bool						 genProfiler = false;
	bool						 genDebug = true;
	bool						 genListing = false;

	/* to make a singleton */
	VM_CONF() noexcept
	{
		static bool initialized = false;
		assert ( !initialized );
		initialized = true;
	}

	~VM_CONF()
	{
	};
	VM_CONF ( VM_CONF && ) = delete;
	VM_CONF ( VM_CONF const & ) = delete;
	VM_CONF& operator=( VM_CONF const& ) = delete;
	VM_CONF& operator=( VM_CONF && ) = delete;

};

extern	VM_CONF					vmConf;
extern	void					vmConfig			( std::string const &name );

