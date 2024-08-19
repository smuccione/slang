/*
	WEB Server header file

*/

#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>
#include <vector> 
#include <map>

#include <memory.h>

#include <Utility/stringi.h>

extern  uint32_t fastIToA( int32_t value, char *str );
extern  uint32_t fastIToA( uint32_t value, char *str );
extern  uint32_t fastIToA( int64_t value, char *str );
extern  uint32_t fastIToA( size_t value, char *str );

extern DWORD vmPageSize;

class   serverBuffer
{
	private:
	uint8_t			*data;
	size_t			 allocSize;
	size_t			 usedSize;
	size_t			 initialSize;

	public:
	serverBuffer	*next;
	class webServer	*server;

	serverBuffer()
	{
		initialSize = 16384;
		allocSize = initialSize;
		data = (uint8_t *)malloc( sizeof( uint8_t ) * allocSize );
		usedSize = 0;
	}
	serverBuffer( size_t	size )
	{
		initialSize = size;
		allocSize = initialSize;
		data = (uint8_t *)malloc( sizeof( uint8_t ) * allocSize );
		usedSize = 0;
	}
	serverBuffer( uint8_t	*buffer, size_t len )
	{
		initialSize = len;
		data = buffer;
		allocSize = len;
		usedSize = 0;
	}
	~serverBuffer()
	{
		if ( data ) free( data );
	}

	void		 removeTail( size_t size )
	{
		if ( size >= usedSize )
		{
			usedSize = 0;
			return;
		}
		usedSize -= size;
	}

	void		 erase( void )
	{
		usedSize = 0;
	}

	void		 remove( size_t	size )
	{
		if ( size >= usedSize )
		{
			usedSize = 0;
			return;
		}
		memmove( data, data + size, usedSize - size );
		usedSize -= size;
	}


	void		 reset( void )
	{
		if ( allocSize != initialSize )
		{
			if ( data ) free( data );
			data = (uint8_t *)malloc( sizeof( uint8_t ) * initialSize );
			allocSize = initialSize;
		}
		usedSize = 0;
	}

	size_t	 printf( char const *fmt, ... )
	{
		va_list		 varArgs = {};

		int len = 0;

		size_t room = 0;
		do
		{
			makeRoom( 128 );
			room += 128;
			va_start( varArgs, fmt );
			len = _vsnprintf_s( (char *) data + usedSize, room, _TRUNCATE, fmt, varArgs );
			va_end( varArgs );
		} while ( len == -1 );
		usedSize += len;
		return usedSize;
	}

	void		 write( uint8_t	 *buff, size_t len )
	{
		makeRoom( len );
		memcpy( data + usedSize, buff, len );
		usedSize += len;
	}
	void		 write( char		*data, size_t len ) { write( (uint8_t *)data, len ); }
	void		 write( char const	*data, size_t len ) { write( (uint8_t *)data, len ); }
	void		 write( char		*data ) { write( (uint8_t *)data, strlen( data ) ); }
	void		 write( char const	*data ) { write( (uint8_t *)data, strlen( (char *)data ) ); }
	void		 write( uint8_t		*data ) { write( data, strlen( (char *)data ) ); }
	void		 write ( stringi const &data ) { write ( data.c_str (), data.size () ); };

	void		 write( uint32_t value )
	{
		makeRoom( 65 );
		_ultoa( value, (char *)(data + usedSize), 10 );
		usedSize += strlen( (char *)(data + usedSize) );
		//		usedSize += fastIToA ( value, (char *)(data + usedSize) );
	}
	void		 write( size_t value )
	{
		makeRoom( 65 );
		_ui64toa( value, (char *)(data + usedSize), 10 );
		usedSize += strlen( (char *)(data + usedSize) );
		//		usedSize += fastIToA ( value, (char *)(data + usedSize) );
	}

	size_t	 getSize( void ) const
	{
		return usedSize;
	}
	size_t	 getFree( void ) const
	{
		return allocSize - usedSize;
	}
	uint8_t		*getFreeBuff( void )
	{
		return data + usedSize;
	}
	uint8_t		*getBuffer( void ) const
	{
		return data;
	}
	uint8_t		**getBufferDataPtr( void )
	{
		return &data;
	}

	uint8_t		*detach( void )
	{
		uint8_t	*ret;
		ret = data;
		data = 0;
		return ret;
	}

	void		 makeRoom( size_t size )
	{
		if ( allocSize - usedSize < size )
		{
			size_t	newSize;

			newSize = usedSize;
			newSize += size < 8192 ? 8192 : size;
			data = (uint8_t *)realloc( data, newSize );
			allocSize = newSize;
		}
	}
	void		 assume( size_t	size )
	{
		usedSize += size;
	}

	void		 writeFront( uint8_t *ptr, size_t size )
	{
		// make it fit
		makeRoom( size );
		if ( usedSize )
		{
			// move existing data down to make room at front
			memmove( data + size, data, usedSize );
		}
		// copy in new data
		memcpy( data, ptr, size );
		// by adding we handle both no and existing data cases
		usedSize += size;
	}

};

class   serverLargeBuffer {
private:
	HANDLE		 fileHandle;
	HANDLE		 mapHandle;
	uint8_t		 staticBuff[65536];
	uint8_t		*data;
	size_t		 allocSize;
	size_t		 usedSize;
public:
	serverLargeBuffer	()
	{
		fileHandle = 0;
		mapHandle = 0;
		data = staticBuff;
		allocSize = sizeof( staticBuff );
		usedSize = 0;
	}

	~serverLargeBuffer()
	{
		if ( fileHandle )
		{
			CloseHandle( mapHandle );
			CloseHandle( fileHandle );
		}
	}

	void switchToFile( size_t newSize )
	{
		char tmpPath[MAX_PATH] = {};

		allocSize = newSize;

		LARGE_INTEGER time;
		QueryPerformanceCounter( &time );
		sprintf( tmpPath + GetTempPath( sizeof( tmpPath ), tmpPath ), "\\engineTmp%llu", time.QuadPart );

		LARGE_INTEGER	pageSize = {};
		pageSize.QuadPart = (LONGLONG)newSize;
		fileHandle = CreateFile( tmpPath, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0 );
		if ( fileHandle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
		{
			throw 500;
		}
		mapHandle = CreateFileMapping( fileHandle, 0, PAGE_READWRITE, pageSize.HighPart, pageSize.LowPart, 0 );
		if ( !mapHandle )
		{
			fileHandle = 0;
			CloseHandle( fileHandle );
			throw 500;
		}
		data = (uint8_t *)MapViewOfFile( mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, allocSize );
		if ( !data )
		{
			fileHandle = 0;
			CloseHandle( mapHandle );
			CloseHandle( fileHandle );
			throw 500;
		}

		// copy over any data from the static buffer into the file backed mapping
		memcpy( data, staticBuff, usedSize );

		return;
	}

	void		 removeTail		( size_t size )
	{
		if ( size >= usedSize )
		{
			usedSize = 0;
			return;
		}
		usedSize -= size;
	}

	void		 erase			( void )
	{
		usedSize = 0;
	}

	void		 remove			( size_t	size )
	{
		if ( size >= usedSize )
		{
			usedSize = 0;
			return;
		}
		memmove ( data, data + size, usedSize - size );
	}


	void		 reset			( void )
	{
		if ( fileHandle )
		{
			UnmapViewOfFile ( data );
			CloseHandle( mapHandle );
			CloseHandle( fileHandle );
			fileHandle = 0;
		}
		data = staticBuff;
		allocSize = sizeof( staticBuff );
		usedSize = 0;
	}

	size_t	 printf			( char const *fmt, ... )
	{
		va_list		 varArgs = {};

		auto len = -1;

		size_t room = 0;
		while ( len == -1 )
		{
			makeRoom ( 128 );
			room += 128;
			va_start ( varArgs, fmt );
			len += _vsnprintf_s ( (char *)data + usedSize, room, _TRUNCATE, fmt, varArgs );
			va_end ( varArgs );
		}
		usedSize += len;
		return usedSize;
	}
	void		 write			( uint8_t	 *buff, size_t len )
	{
		makeRoom ( len );
		memcpy ( data + usedSize, buff, len );
		usedSize += len;
	}
	void		 write			( char			*data, size_t len ) { write ( (uint8_t *)data, len ); }
	void		 write			( char const	*data, size_t len ) { write ( (uint8_t *)data, len ); }
	void		 write			( char			*data ) { write ( (uint8_t *)data, strlen ( data ) ); }
	void		 write			( char const	*data ) { write ( (uint8_t *)data, strlen ( (char *)data ) ); }
	void		 write			( uint8_t		*data ) { write ( data, strlen ( (char *)data ) ); }

	void		 write			( uint32_t value )
	{
		makeRoom ( 65 );
		_ultoa ( value, (char *)(data + usedSize), 10 );
		usedSize += strlen ( (char *)(data + usedSize) );
//		usedSize += fastIToA ( value, (char *)(data + usedSize) );
	}
	void		 write			( size_t value )
	{
		makeRoom ( 65 );
		_ui64toa ( value, (char *)(data + usedSize), 10 );
		usedSize += strlen ( (char *)(data + usedSize) );
//		usedSize += fastIToA ( value, (char *)(data + usedSize) );
	}

	size_t	 getSize		( void ) const
	{
		return usedSize;
	}
	size_t	 getFree		( void ) const
	{
		return allocSize - usedSize;
	}
	uint8_t		*getFreeBuff	( void ) const
	{
		return data + usedSize;
	}
	uint8_t		*getBuffer		( void ) const
	{
		return data;
	}
	uint8_t		**getBufferDataPtr ( void )
	{
		return &data;
	}

	uint8_t		*detach		( void )
	{
		uint8_t	*ret;
		ret = data;
		data = 0;
		return ret;
	}

	void		 makeRoom		( size_t size )
	{
		size_t	newSize;

		if ( size <= allocSize - usedSize )
		{
			return;
		}

		newSize = usedSize;
		newSize += size < (size_t)vmPageSize ? (size_t)vmPageSize : (size + ((size_t)vmPageSize - 1)) & (~((size_t)vmPageSize - 1));

		if ( !fileHandle )
		{
			switchToFile( newSize );
		} else
		{
			// increase size on disk
			CloseHandle ( mapHandle );

			LARGE_INTEGER	pageSize = {};
			pageSize.QuadPart = (LONGLONG)newSize;
			mapHandle = CreateFileMapping( fileHandle, 0, PAGE_READWRITE, pageSize.HighPart, pageSize.LowPart, 0 );
			data = (uint8_t *)MapViewOfFile( mapHandle, FILE_MAP_ALL_ACCESS, 0, 0, allocSize );

			allocSize = newSize;
		}
	}
	void		 assume			( size_t	size )
	{
		usedSize += size;
	}

	void		 writeFront		( uint8_t *ptr, size_t size )
	{
		// make it fit
		makeRoom ( size );
		if ( usedSize )
		{
			// move existing data down to make room at front
			memmove ( data + size, data, usedSize );
		}
		// copy in new data
		memcpy ( data, ptr, size );
		// by adding we handle both no and existing data cases
		usedSize += size;
	}

};
