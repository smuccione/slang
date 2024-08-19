#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>
#include <windns.h>
#include <winhttp.h>
#include <WinDns.h>
#include <IpExport.h>
#include <cassert>
#include <stdint.h>
#include <unordered_map>

#include "encodingTables.h"

__forceinline int strccmp ( char const *a, char const *b )
{
#if 1
	static caseInsenstiveTableInit caseTable;

	for ( ;; a++, b++ ) 
	{

//		int d = (*a & 0xdf) - (*b & 0xdf);
		int d = caseInsenstiveTable[*(uint8_t *)a] - caseInsenstiveTable[*(uint8_t *)b];
		if ( d || !*a )
			return d;
	}
#else
	for ( ;; a++, b++ )
	{
		auto c1 = *a >= 'a' && *a <= 'z' ? *a - 'a' + 'A' : *a;
		auto c2 = *b >= 'a' && *b <= 'z' ? *b - 'a' + 'A' : *b;
		int d = c1 - c2;
		if ( d != 0 || !*a )
			return d;
	}
#endif
	return 0;
} 

__forceinline int memcmpi ( void const *aptr, void const *bptr, size_t len )
{
	static caseInsenstiveTableInit caseTable;

	char const *a = (char const *)aptr;
	char const *b = (char const *)bptr;
	for ( ;len; a++, b++, len-- )
	{
//		int d = (*a & 0xdf) - (*b & 0xdf);
		int d = caseInsenstiveTable[(uint8_t)*a] - caseInsenstiveTable[(uint8_t)*b];
		if ( d )
			return d;
	}
	return 0;
}

class SRRWLOCK
{
	struct LOCK {
		union {
			struct {
				volatile uint32_t	counter;
				volatile DWORD		writeOwner;
			};
			volatile uint64_t		quad;
		};
	}						lock;

	public:
	SRRWLOCK ( ) noexcept
	{
		lock.quad = 0;
	}
	virtual ~SRRWLOCK ( )
	{
		assert ( !lock.quad );
	}

	void readLock ( )
	{
		for ( ;; )
		{
			LOCK origLock;
			origLock.quad = lock.quad;
			if ( !origLock.writeOwner )
			{
				uint64_t const cmpOrig = InterlockedCompareExchange64 ( (LONG64 *) &lock.quad, origLock.quad + 1, origLock.quad );
				if ( cmpOrig == origLock.quad )
				{
					break;
				}
			} else if ( origLock.writeOwner == GetCurrentThreadId() )
			{
				InterlockedIncrement ( &lock.counter );
				break;
			}
		}
	}

	void readUnlock ( ) noexcept
	{
		InterlockedDecrement ( &lock.counter );
	}

	void writeLock ( ) noexcept
	{
		DWORD const id = GetCurrentThreadId ( );
		for ( ;; )
		{
			LOCK origLock;
			origLock.quad = lock.quad;
			if ( !origLock.quad || lock.writeOwner == id )
			{
				LOCK newLock;
				newLock.writeOwner = id;
				newLock.counter = origLock.counter + 1;
				uint64_t cmpOrig = InterlockedCompareExchange64 ( (LONG64 *) &lock.quad, newLock.quad, origLock.quad );
				if ( cmpOrig == origLock.quad )
				{
					break;
				}
			}
		}
	}

	void writeUnlock  ( ) noexcept
	{
		if ( lock.counter == 1 )
		{
			// set not only the counter but the write owner to 0, thus freeing up the lock
			lock.quad = 0;
		} else
		{
			InterlockedDecrement ( &lock.counter);
		}
	}
};

class SRRWLocker {
	SRRWLOCK &lock;
	bool	  write;

	public:
	SRRWLocker ( SRRWLOCK &lock, bool write ) : lock ( lock ), write ( write )
	{
		if ( write )
		{
			lock.writeLock ( );
		} else
		{
			lock.readLock ( );
		}
	}
	virtual ~SRRWLocker ( )
	{
		if ( write )
		{
			lock.writeUnlock ( );
		} else
		{
			lock.readUnlock ( );
		}
	}
};

class Uri
{
	public:
	std::wstring QueryString;
	std::wstring Path = L"/";
	std::wstring Protocol = L"http";
	std::wstring Host;
	std::wstring User;
	std::wstring Password;
	std::wstring Req;

	uint32_t Port = 80;
	bool isSecure = false;

	static Uri Parse ( const std::wstring &uri )
	{

		URL_COMPONENTS	comp;
		memset ( &comp, 0, sizeof ( comp ) );

		comp.dwStructSize = sizeof ( comp );
		comp.dwExtraInfoLength = -1;
		comp.dwHostNameLength = -1;
		comp.dwPasswordLength = -1;
		comp.dwSchemeLength = -1;
		comp.dwUrlPathLength = -1;
		comp.dwUserNameLength = -1;

		WinHttpCrackUrl ( uri.c_str ( ), 0, 0, &comp );

		Uri result;

		result.Port = comp.nPort;
		result.Host = std::wstring ( comp.lpszHostName, comp.dwHostNameLength );
		result.Path = std::wstring ( comp.lpszUrlPath, comp.dwUrlPathLength );
		result.Protocol = std::wstring ( comp.lpszScheme, comp.dwSchemeLength );
		result.QueryString = std::wstring ( comp.lpszExtraInfo, comp.dwExtraInfoLength );
		result.User = std::wstring ( comp.lpszUserName, comp.dwUserNameLength );
		result.Password = std::wstring ( comp.lpszPassword, comp.dwPasswordLength );

		if ( comp.nScheme == INTERNET_SCHEME_HTTPS ) result.isSecure = true;

		result.Req = result.Path;
		if ( result.QueryString.size ( ) )
		{
			result.Req += result.QueryString;
		}

		return result;
	}   // Parse
};  // uri

