/*
AP Server Page Cache

*/

#include "Utility/settings.h"

#include "stdafx.h"
#include "stdlib.h"
#include <stdint.h>
#include <string.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <time.h>
#include <process.h>
#include <unordered_set> 
#include <unordered_map>
#include <filesystem>

#include <fcntl.h>
#include <share.h>

#include "Utility/staticString.h"
#include "compilerParser/fglErrors.h"
#include "Target/fileCache.h"
#include "Utility/encodingTables.h"
#include "Utility/funcky.h"

class exclusiveLocker {
	SRWLOCK	&lock;
	public:
	exclusiveLocker ( SRWLOCK &lock ) : lock ( lock )
	{
		AcquireSRWLockExclusive ( &lock );
	}
	~exclusiveLocker ()
	{
		ReleaseSRWLockExclusive ( &lock );
	}
};

class sharedLocker {
	SRWLOCK	&lock;

	public:
	sharedLocker ( SRWLOCK &lock ) : lock ( lock )
	{
		AcquireSRWLockShared ( &lock );
	}
	~sharedLocker ( )
	{
		ReleaseSRWLockShared ( &lock );
	}
};

static DWORD WINAPI fileCacheChangeThread ( void *param )
{
	BOOL				 result;
	DWORD				 nBytesTransferred;
	ULONG_PTR			 key;
	fileCacheDir		*watch;
	fileCache			*cache = (fileCache *) param;

	while ( 1 )
	{
		result = GetQueuedCompletionStatus ( cache->monitorCP, &nBytesTransferred, &key, (OVERLAPPED **) &watch, INFINITE );

		if ( result )
		{
			// simply flush when anything was changed... this is a RARE occurance and the beneif of just removing one entry from the cache is outweighed by simplicity and downline speed
			cache->flush ();

			memset ( &(watch->ov), 0, sizeof ( watch->ov ) );
			// register for the next change notification
			ReadDirectoryChangesW ( watch->dirHandle,
				watch->dat.data,
				sizeof ( watch->dat.data ),
				TRUE,
				FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE,
				&nBytesTransferred,
				&(watch->ov),
				0
			);
		} else
		{
			return 0;
		}
	}
}

fileCacheEntry ::~fileCacheEntry ()
{
	//assert ( !refCnt );
	if ( fileMap )
	{
		UnmapViewOfFile ( data );
		CloseHandle ( fileMap );
		CloseHandle ( fileHandle );
	} else
	{
		if ( data ) HeapFree ( cache->cacheHeap, 0, data );
	}

	for ( auto &it : userDataCache )
	{
		delete it;
	}
	DeleteCriticalSection ( &userDataProtect );
}

void fileCacheEntry::free ()
{
	cache->free ( this );
}

void fileCacheEntry::release ()
{
	assert ( !refCnt );
	if ( fileMap )
	{
		UnmapViewOfFile ( data );
		CloseHandle ( fileMap );
		CloseHandle ( fileHandle );
	} else
	{
		if ( data ) HeapFree ( cache->cacheHeap, 0, data );
	}
	fName = "INVALID-FREE";
	data = 0;
	for ( auto &it : userDataCache )
	{
		delete it;
	}
	fileHandle = 0;
	fileMap = 0;
	cache = 0;
	userDataFree = nullptr;
	userDataCache.clear ( );
}

fileCache::fileCache ( uint64_t heapSize, uint64_t largeFileSize )
{
	if ( !heapSize ) heapSize = 1024ull * 1024 * 1024; // 1G of cache size by default
	if ( !largeFileSize ) largeFileSize = 65536ull * 1024;	// 64MB as cutoff to memory mapped file

	this->largeFileSize = largeFileSize;
	this->cacheSize = heapSize;

	InitializeSRWLock ( &lock );

	cacheHeap = HeapCreate ( HEAP_NO_SERIALIZE, 0, 0);		// serialization is done via our own locks so no need to duplicate

	monitorCP = CreateIoCompletionPort ( INVALID_HANDLE_VALUE, 0, 0, 1 );	// NOLINT (performance-no-int-to-ptr)

	monitorThread = CreateThread ( 0, 0, fileCacheChangeThread, this, 0, 0 );
}

fileCacheEntry *fileCache::read ( char const *fName )
{
	return read ( fName, strlen ( fName ) );
}

fileCacheEntry *fileCache::read ( stringi const &fName )
{
	return read ( fName.c_str(), fName.length() );
}

fileCacheEntry *fileCache::read ( char const *fName, size_t fNameLen )
{
	fileCacheEntry	*entry;

	AcquireSRWLockShared ( &lock );

	auto mapIt = lookup.find ( fName );
	if ( mapIt == lookup.end () )
	{
		{
			ReleaseSRWLockShared ( &lock );
			exclusiveLocker l1 ( lock );

			// try it again once more to make sure someone else didn't add it in when we were waiting for exclusive access
			mapIt = lookup.find ( fName );
			if ( mapIt != lookup.end ( ) )
			{
				entry = mapIt->second;
				InterlockedIncrement ( &entry->refCnt );
				return entry;
			}

			if ( freeEntries )
			{
				// grab one from the free list
				entry = freeEntries;
				freeEntries = freeEntries->next;

				assert ( !entry->refCnt );
			} else
			{
				entries.push_back ( new fileCacheEntry () );
				entry = entries.back ();
			}

			entry->fileHandle = CreateFile ( fName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( entry->fileHandle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
			{
				entry->next = freeEntries;
				freeEntries = entry;
				//printf ( "fileCacheRead failed create file   %i   for %s\r\n", GetLastError (), fName );
				return 0;
			}

			GetFileInformationByHandle ( entry->fileHandle, &entry->fileInfo );

			// fill in our entry
			entry->fName = fName;
			entry->pendingFree = false;
			entry->cache = this;

			auto size = ((uint64_t) entry->fileInfo.nFileSizeHigh << 32) + entry->fileInfo.nFileSizeLow;
	//		printf ( "heapMaxSize = %zu    largFileSize = %zu     size=%zu\r\n", this->cacheSize, this->largeFileSize, size );
			if ( size >= this->largeFileSize )
			{
				entry->type = fileCacheEntry::lruListType::mapped;
				if ( inUseSize + size > cacheSize )
				{
					// "TRY" to keep us less than max cache size - if we can't release enough we'll just allocate and pray 
					releaseLRU ( fileCacheEntry::lruListType::mapped, size - (cacheSize - inUseSize) );
				}

				entry->fileMap = CreateFileMapping ( entry->fileHandle, NULL, PAGE_READONLY, 0, 0, NULL );
				if ( entry->fileHandle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
				{
					CloseHandle ( entry->fileHandle );
					entry->next = freeEntries;
					freeEntries = entry;
					//printf ( "fileCacheRead failed file mapping   %i   for %s\r\n", GetLastError (), fName );
					return nullptr;
				}

				entry->data = (uint8_t *) MapViewOfFileEx ( entry->fileMap, FILE_MAP_READ, 0, 0, 0, NULL );
				inUseMappedSize += size;

				if ( !entry->data )
				{
					CloseHandle ( entry->fileMap );
					CloseHandle ( entry->fileHandle );
					entry->next = freeEntries;
					freeEntries = entry;
					//printf ( "fileCacheRead failed file map view of file   %i   for %s\r\n", GetLastError (), fName );
					return nullptr;
				}
				// this entry is NOT cached!, the caching mechanism is simply used to manage the lifetime of the object itself
			} else
			{
				entry->type = fileCacheEntry::lruListType::heap;
				entry->fileMap = 0;
				if ( inUseSize + size > cacheSize )
				{
					// "TRY" to keep us less than max cache size - if we can't release enough we'll just allocate and pray 
					releaseLRU ( fileCacheEntry::lruListType::heap, size - (cacheSize - inUseSize) );
				}

				if ( !(entry->data = (uint8_t *) HeapAlloc ( cacheHeap, 0, size )) )
				{
					//printf ( "fileCacheRead heapAlloc failed for %s\r\n", fName );
					// should never fail as heap is growable... UNLESS we simply run out of OS memory
					CloseHandle ( entry->fileHandle );
					entry->next = freeEntries;
					freeEntries = entry;
					return nullptr;
				}
				inUseSize += size;
				DWORD nRead;

				auto toRead = size;
				while ( toRead )
				{
					if ( size > 1024ull * 1024 * 1024 )
					{
						size = 1024ull * 1024 * 1024;
					}
					if ( !ReadFile ( entry->fileHandle, entry->data, (DWORD)size, &nRead, 0 ) )
					{
						//printf ( "fileCacheRead read file    %i   for %s\r\n", GetLastError (), fName );
						HeapFree ( cacheHeap, 0, entry->data );
						CloseHandle ( entry->fileHandle );
						entry->next = freeEntries;
						freeEntries = entry;
						return nullptr;
					}
					toRead -= nRead;
				}
				CloseHandle ( entry->fileHandle );
				//  only cached entries are added to lookup
//				printf ( "filename: " );
//				printf ( "%s  %i\r\n", entry->fName.c_str ( ), entry->fName.length ( ) );
				lookup[entry->fName] = entry;
			}
			entry->refCnt = 1;
		}

		monitor ( entry->fName.c_str() );
		return entry;
	} else
	{
		entry = mapIt->second;
		// we MUST increment the reference count before releasing shared access... a deleter (flush) needs exclusive and will free unless the ref count is non 0
		InterlockedIncrement ( &entry->refCnt );
		ReleaseSRWLockShared ( &lock );

		makeMRU ( entry, entry->type );

		return entry;
	}
}

fileCacheEntry *fileCache::get ( char const *fName, size_t fNameLen )
{
	fileCacheEntry	*entry;

	AcquireSRWLockShared ( &lock );
	auto mapIt = lookup.find ( fName );
	if ( mapIt == lookup.end () )
	{
		ReleaseSRWLockShared ( &lock );
		return nullptr;
	} else
	{
		entry = mapIt->second;
		// we MUST increment the reference count before releasing shared access... a deleter (flush) needs exclusive and will free unless the ref count is non 0
		InterlockedIncrement ( &entry->refCnt );
		ReleaseSRWLockShared ( &lock );
		return entry;
	}
}

fileCacheEntry *fileCache::get ( char const *fName )
{
	return get ( fName, strlen ( fName ) );
}

fileCacheEntry *fileCache::get ( stringi const &fName )
{
	fileCacheEntry *entry;

	AcquireSRWLockShared ( &lock );
	auto mapIt = lookup.find ( fName );
	if ( mapIt == lookup.end ( ) )
	{
		ReleaseSRWLockShared ( &lock );
		return nullptr;
	} else
	{
		entry = mapIt->second;
		// we MUST increment the reference count before releasing shared access... a deleter (flush) needs exclusive and will free unless the ref count is non 0
		InterlockedIncrement ( &entry->refCnt );
		ReleaseSRWLockShared ( &lock );
		return entry;
	}
}

// attach new fileCacheEntry to the head of the MRU list
void fileCache::addToMRU ( fileCacheEntry *entry, fileCacheEntry::lruListType type )
{
	std::lock_guard g1 ( lruListGuard[(size_t) type] );

	entry->next = lruListMRU[(size_t) type];
	entry->prev = nullptr;
	if ( lruListMRU[(size_t)type] )
	{
		lruListMRU[(size_t) type]->prev = entry;
	}
	lruListMRU[(size_t) type] = entry;
}

// make entry the most recently used item in the list
void fileCache::makeMRU ( fileCacheEntry *entry, fileCacheEntry::lruListType type )
{
	std::lock_guard g1 ( lruListGuard[(size_t) type] );

	if ( entry != lruListMRU[(size_t) type] )
	{
		// detach from the list
		if ( entry->prev )
		{
			entry->prev->next = entry->next;
		}
		if ( entry->next )
		{
			entry->next->prev = entry->prev;
		} else
		{
			lruListLRU[(size_t) type] = entry->prev;
		}

		// reattach back at the head
		entry->next = lruListMRU[(size_t) type];
		entry->prev = nullptr;
		if ( lruListMRU[(size_t) type] )
		{
			lruListMRU[(size_t) type]->prev = entry;
		}
		lruListMRU[(size_t) type] = entry;
	}
}

// this starts from the back of the LRU list and releases as much as can occur until it sizeToRelease is met
void fileCache::releaseLRU ( fileCacheEntry::lruListType type, size_t sizeToRelease )
{
	std::lock_guard g1 ( lruListGuard[(size_t) type] );

	fileCacheEntry *current = lruListLRU[(size_t) type];

	while ( current )
	{
		if ( !current->refCnt )
		{
			// detach from the list
			if ( current->prev )
			{
				current->prev->next = current->next;
			}
			if ( current->next )
			{
				current->next->prev = current->prev;
			}
			if ( current == lruListLRU[(size_t) type] )
			{
				lruListLRU[(size_t) type] = current->prev;
			}
			if ( current == lruListMRU[(size_t) type] )
			{
				lruListMRU[(size_t) type] = current->next;
			}

			auto size = ((uint64_t) (current->fileInfo.nFileSizeHigh) << 32) + current->fileInfo.nFileSizeLow;
			
			if ( sizeToRelease > size )
			{
				sizeToRelease -= size;
			} else
			{
				// we've release enough
				current->release ();
				current->next = freeEntries;
				freeEntries = current;

			}
		}
	}
}

void fileCache::flush ( void )
{
	exclusiveLocker l1 ( lock );
	// just put EVERYTHING that's active (in lookup) into the pending free list and clear the map
	for ( auto &it : lookup )
	{
		if ( it.second->refCnt )
		{
			// mark it as pending free - will be completely free'd when the last holder releases it's reference
			it.second->pendingFree = true;
		} else
		{
			// move it into the free list
			auto &entry = it.second;
			entry->release ( );
			entry->next = freeEntries;
			freeEntries = entry;
		}
	}
	// all lookup entries have been processed so clear it out... they're all either pending free or in the free list
	lookup.clear ();
}

void fileCache::free ( fileCacheEntry *entry )
{
	auto newCnt = InterlockedDecrement ( &entry->refCnt );

	// if newCnt == 0 then we were the final user, it's interlocked so only one thread can ever bring it to 0 - the last owning thread
	if ( !newCnt )
	{
		exclusiveLocker l1 ( lock );

		// see if we're still 0 refcnt AND we're pending free...    we need to check refCnt again to void a race between the interlockedDecrement and the exclusive lock
		if ( !entry->refCnt && entry->pendingFree )
		{
			// free the data
			entry->release ( );

			// add to free list
			entry->next = freeEntries;
			freeEntries = entry;
		}
	}
}

void fileCache::monitor ( char const *file )
{
	HANDLE	handle;
	DWORD	nBytesTransferred;

	auto f = std::filesystem::path ( file );
	auto dir = std::filesystem::path ( f ).remove_filename ().generic_string ();
	{
		sharedLocker l1 ( lock );
		for (auto &it : dirHandles)
		{
			if (!memcmpi ( it.directory.c_str ( ), dir.c_str(), it.directory.size ( ) ))
			{
				// all ready tracking changes (either in current directory or below
				return;
			}
		}

	}
	exclusiveLocker l1 ( lock );


	handle = CreateFile ( dir.c_str(),
						  FILE_LIST_DIRECTORY,
						  FILE_SHARE_READ | FILE_SHARE_WRITE,
						  NULL,
						  OPEN_EXISTING,
						  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
						  NULL
	);

	if ( handle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		return;
	}

	dirHandles.push_back ( fileCacheDir ( dir.c_str(), handle ) );

	// associate it
	CreateIoCompletionPort ( handle, monitorCP, 0, 1 );

	ReadDirectoryChangesW ( handle,
		dirHandles.back().dat.data,
		sizeof ( dirHandles.back().dat.data ),
		TRUE,
		FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE,
		&nBytesTransferred,
		&(dirHandles.back().ov),
		0
	);
}

void fileCache::printCache ( void )
{
	printf ( "--------------------------------------------" );
}