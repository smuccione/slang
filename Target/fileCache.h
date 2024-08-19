/*
Caching system header file

*/

#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>

#include <stdint.h>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <string>
#include <cassert>
#include <mutex>

#include "Utility/staticString.h"
#include "Utility/stringi.h"

struct fileCacheDir {
	OVERLAPPED					 ov = {};
	stringi						 directory;
	HANDLE						 dirHandle = (HANDLE)INVALID_HANDLE_VALUE;
	union {
		struct _FILE_NOTIFY_INFORMATION	fi;
		char							data[4096];
	} dat;

	fileCacheDir ( char const *dir, HANDLE handle )
	{
		memset ( &ov, 0, sizeof ( ov ) );
		directory = dir;
		dirHandle = handle;
	}

	fileCacheDir ( fileCacheDir&& old ) noexcept
	{
		*this = std::move ( old );
	}
	fileCacheDir& operator = ( fileCacheDir&& old ) noexcept
	{
		std::swap ( ov, old.ov );
		std::swap ( directory, old.directory );
		std::swap ( dat, old.dat );
		std::swap ( dirHandle, old.dirHandle );
		return *this;
	}
};


class fileCacheUserData {
	fileCacheUserData	*next = 0;
	public:
	fileCacheUserData ()
	{
	}

	virtual ~fileCacheUserData ()
	{
	}
	friend struct fileCacheEntry;
};

struct fileCacheEntry {
	struct fileCacheEntry			*prev = 0;		// lru list prev
	struct fileCacheEntry			*next = 0;		// lru list next && free list next

	stringi							 fName;

	HANDLE							 fileHandle = 0;
	HANDLE							 fileMap = 0;
	uint8_t							*data = 0;

	CRITICAL_SECTION				 userDataProtect;
	std::list<fileCacheUserData *>	 userDataCache;
	fileCacheUserData				*userDataFree = 0;

	size_t							 refCnt = 0;			// no need for volatile as all access's are via interlocked access (inc/dec/icx)
	bool							 pendingFree = false;

	BY_HANDLE_FILE_INFORMATION		fileInfo = {};

	class fileCache					*cache = 0;

	enum class lruListType : size_t
	{
		heap = 0,
		mapped = 1,
		MAX
	};

	lruListType						 type;

	fileCacheEntry ()
	{
		InitializeCriticalSectionAndSpinCount ( &userDataProtect, 10000 );
	}

	~fileCacheEntry ();
	fileCacheEntry ( fileCacheEntry &&old ) = delete;
	void free ();

	uint8_t const *getData ()
	{
		assert ( refCnt );
		return data;
	}

	void addUserData ( fileCacheUserData *data )
	{
		assert ( refCnt );
		assert ( data );
		EnterCriticalSection ( &userDataProtect );
		userDataCache.push_front ( data );
		LeaveCriticalSection ( &userDataProtect );
	}
	void freeUserData ( fileCacheUserData *data )
	{
		assert ( refCnt );
		assert ( data );
		EnterCriticalSection ( &userDataProtect );
		data->next = userDataFree;
		userDataFree = data;
		LeaveCriticalSection ( &userDataProtect );
	}
	fileCacheUserData *getUserData ()
	{
		assert ( refCnt );
		EnterCriticalSection ( &userDataProtect );
		if ( userDataFree )
		{
			auto ret = userDataFree;
			userDataFree = userDataFree->next;
			LeaveCriticalSection ( &userDataProtect );
			return ret;

		}
		LeaveCriticalSection ( &userDataProtect );
		return 0;
	}

	void release ();


	public:
		bool						 isDataCached ( void ) 
		{ 
			assert ( refCnt );
			return data ? 1 : 0;
		};
		stringi						&getFName () 
		{ 
			assert ( refCnt );
			return fName;
		};
		uint64_t					 getDataLen () 
		{ 
			assert ( refCnt );
			return ((uint64_t)(fileInfo.nFileSizeHigh) << 32) + fileInfo.nFileSizeLow;
		};
		time_t						 getModifiedTime ()
		{
			ULARGE_INTEGER ull{};
			ull.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
			ull.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;

			return (time_t)(ull.QuadPart / 10000000ULL - 11644473600ULL);
		}

		friend class fileCache;
		friend struct fileCacheEntry;
};

class fileCache {
	SRWLOCK									 lock;

	HANDLE									 monitorCP;			// completion port for monitor events
	HANDLE									 monitorThread;
	std::list<fileCacheDir>					 dirHandles;		// monitored directories

	std::unordered_map<stringi, fileCacheEntry *> lookup;

	std::list<fileCacheEntry *>				 entries;			// file cache handles - all entires cur
	fileCacheEntry							*freeEntries = nullptr;
	fileCacheEntry							*pendingFree = nullptr;

	std::mutex								 lruListGuard[(size_t) fileCacheEntry::lruListType::MAX];
	fileCacheEntry							*lruListMRU[(size_t) fileCacheEntry::lruListType::MAX] = {};
	fileCacheEntry							*lruListLRU[(size_t) fileCacheEntry::lruListType::MAX] = {};


	std::list<fileCacheEntry *>				 handles;			// file cache handles
	fileCacheEntry							*freeHandles = nullptr;

	uint64_t								 largeFileSize;		// switch from allocated data to memorymapped file
	uint64_t								 cacheSize;			// size of cache heap
	size_t									 maxEntries;


	HANDLE									 cacheHeap;
	size_t									 inUseSize = 0;
	size_t									 inUseMappedSize = 0;

	public:
	fileCache ( uint64_t heapSize, uint64_t largeFileSize );

	~fileCache ()
	{
		CloseHandle ( monitorCP );

		WaitForSingleObject ( monitorThread, INFINITE );		// wait for our thread to exit

		for ( auto &it : dirHandles )
		{
			CloseHandle ( it.dirHandle );
		}
		for ( auto &it : entries )
		{
			delete it;
		}
		for ( auto &it : handles )
		{
			delete it;
		}
	}

	void setSize ( uint64_t heapSize, uint64_t largeFileSize )
	{
		if ( !heapSize ) heapSize = 1024ULL * 1024 * 1024; // 1G of cache size by default
		if ( !largeFileSize ) largeFileSize = 65536ULL * 1024;	// 64MB as cutoff to memory mapped file

		this->largeFileSize = largeFileSize;
		this->cacheSize = heapSize;
	}

	// read the file into cache and return it... if it's cached use the cached entry
	fileCacheEntry				*read ( char const *fName );
	fileCacheEntry				*read ( stringi const &fName );
	// read the file into cache and return it... if it's cached use the cached entry
	fileCacheEntry				*read ( char const *fName, size_t nameLen );

	// the GET functions ONLY look for existing cache entries.   we use this primarily for APX files.   Since any change to the storage will cause a flush of the cache
	// the only time we ever need to check for timestamp older between apx and ap file is when the apx is first loaded into the cache.
	// so to optimize things we try a get first, if the apx is there we know it's up to date and can immediately execute it.   otherwise we load it using read
	fileCacheEntry				*get ( char const *fName );
	fileCacheEntry				*get ( stringi const &fName );
	// the GET functions ONLY look for existing cache entries.   we use this primarily for APX files.   Since any change to the storage will cause a flush of the cache
	// the only time we ever need to check for timestamp older between apx and ap file is when the apx is first loaded into the cache.
	// so to optimize things we try a get first, if the apx is there we know it's up to date and can immediately execute it.   otherwise we load it using read
	fileCacheEntry				*get ( char const *fName, size_t nameLen );
	void						 free ( fileCacheEntry *entry );
	void						 monitor ( char const *dName );
	void						 flush ( void );

	void						 addToMRU ( fileCacheEntry *entry, fileCacheEntry::lruListType type );
	void						 makeMRU ( fileCacheEntry *entry, fileCacheEntry::lruListType type );
	void						 releaseLRU ( fileCacheEntry::lruListType type, size_t sizeToRelease );

	void						 printCache ( void );

	friend DWORD WINAPI fileCacheChangeThread ( void *param );
	friend struct fileCacheEntry;

	private:
};

// our actual cache object
extern fileCache	globalFileCache;
