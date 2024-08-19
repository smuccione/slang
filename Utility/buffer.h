/*
	buffer defines

*/

#pragma once

#include "Utility/settings.h"

#include <iostream>

#include "Target/common.h"
#include "compilerParser/fglErrors.h"
#include "Target/output.h"

#define SERVER_BUFFER_DEFAULT_SIZE 8192

#define bufferBuff(buffer) ((buffer)->buff + (buffer)->buffStart)
#define bufferSize(buffer) ((buffer)->size())
#define bufferFree(buffer) ((buffer)->avail())
#define bufferPrintf( buffer, fmt, ... ) ((buffer)->printf ( fmt, __VA_ARGS__ ) );

struct BUFFER {
	char		 defaultBuffer[SERVER_BUFFER_DEFAULT_SIZE] = "";
	bool		 usingDefault = true;
	char		*buff = defaultBuffer;
	size_t		 buffSize = sizeof ( defaultBuffer );
	size_t		 buffPos = 0;
	size_t		 buffStart = 0;
	size_t		 increment = SERVER_BUFFER_DEFAULT_SIZE;

	BUFFER ( size_t increment = SERVER_BUFFER_DEFAULT_SIZE ) noexcept : increment ( increment )
	{
#ifdef _DEBUG
		memset ( buff, 0xCA, buffSize );
#endif
	}

	virtual ~BUFFER() noexcept
	{
		if ( !usingDefault )
		{
			free( buff );
		}
	}

	BUFFER ( BUFFER const &old)			// NOLINT ( C26495 )
	{
		increment = old.increment;
		write ( old.data<const char *> ( ), old.size() );
	}

	BUFFER ( BUFFER &&buff ) noexcept		// NOLINT ( C26495 )
	{
		std::swap ( *this, buff );
	}

	BUFFER &operator = ( BUFFER const &buff )
	{
		if ( this != &buff )
		{
			reset ();
			write ( buff.data<const char *> (), buff.size () );
		}
		return *this;
	}

	BUFFER &operator = ( BUFFER &&buff ) noexcept
	{
		std::swap ( *this, buff );
		return *this;
	}

	void reset()
	{
		if ( !usingDefault && buffSize != increment )
		{
			free( buff );
			usingDefault = true;
			buff = (char *)&defaultBuffer;
			buffSize = sizeof ( defaultBuffer );
		}
		buffPos = 0;
		buffStart = 0;
	}

	void clear()
	{
		buffPos = 0;
		buffStart = 0;
	}

	void assume ( size_t len )
	{
		buffPos += len;
	}

	void makeRoom ( size_t len )
	{
		if ( buffPos + len >= buffSize )
		{
			if ( buffStart > len )
			{
				// we can still fit without reallocating... so trim the unused off the front instead
				memmove ( buff, buff + buffStart, buffPos - buffStart );
				buffPos -= buffStart;
				buffStart = 0;
			} else
			{
				if ( usingDefault )
				{
					// allocate bigger buffer and copy trimmed version
					char *newBuff;

					if ( !(newBuff = (char *) malloc ( buffSize + len + increment )) )
					{
						return;
					}
					memcpy ( newBuff, buff + buffStart, buffPos - buffStart );

					buff = newBuff;
					buffPos = buffPos - buffStart;
					buffStart = 0;
					buffSize = buffSize + len + increment;
					usingDefault = false;
				} else if ( buffStart )
				{
					// allocate bigger bufff, copy trimmed version and free old buffer
					auto buff2 = (char *)realloc ( buff, buffSize + len + increment );
					buff = buff2;
					memmove( buff, buff + buffStart, buffPos - buffStart );
					buffPos -= buffStart;
					buffStart = 0;
					buffSize = buffSize + len + increment;
				} else
				{
					// realocate in place
					char *tmpP;
					tmpP = (char *) realloc ( buff, buffSize + len + increment );
					if ( !tmpP ) throw errorNum::scMEMORY;
					buff = tmpP;
					buffSize = buffSize + len + increment;
				}
			}
		}
	}

	void align ( size_t len )
	{
		makeRoom ( len );
		assume ( ((size() + (len - 1)) & (UINTPTR_MAX - (len - 1))) - size() );
	}

//	buff       buffStart                buffPos                   buffSize
//	|          |        size            |           avail         |
//
//	available = buffSize - buffPos
//


	size_t size ( ) const noexcept
	{
		return buffPos - buffStart;
	}

	template <typename T>
	T data ( ) const noexcept
	{
		return (T) (buff + buffStart);
	}

	template <typename T>
	T readPoint ( ) const noexcept
	{
		return (T) (buff + buffPos);
	}

	size_t avail ( ) const noexcept
	{
		return buffSize - buffPos;
	}

	void forceRemove ( size_t len ) noexcept
	{
		if ( len >= buffPos - buffStart )
		{
			buffPos = 0;
			buffStart = 0;
		} else
		{
			memmove ( buff, buff + buffStart + len, (buffPos - buffStart) - len );
			buffPos -= (buffStart + len);
			buffStart = 0;
		}
	}

	void remove ( size_t len ) noexcept
	{
		if ( len )
		{
			if ( len >= buffPos - buffStart )
			{
				buffPos = 0;
				buffStart = 0;
			} else
			{
				if ( len + buffStart < (SERVER_BUFFER_DEFAULT_SIZE / 2) )
				{
					buffStart += len;
				} else
				{
					memmove ( buff, buff + buffStart + len, (buffPos - buffStart) - len );
					buffPos -= (buffStart + len);
					buffStart = 0;
				}
			}
			if ( !buffPos )
			{
				buffStart = 0;
			}
		}
	}

	void removeAt ( size_t at, size_t len ) noexcept
	{
		if ( buffStart + at > buffPos )
		{
			// start of area to remove is after end of buffer... ignore
		} else
		{
			// start is within the buffer... 
			if ( buffStart + at + len >= buffPos )
			{
				// end point is after buffer end
				buffPos = buffStart + at;
			} else
			{
				// move down what's left after len bytes beyond at position to at position
				memmove ( buff + buffStart + at, buff + buffStart + at + len, (buffPos - buffStart) - (at + len) );
				buffPos -= len;
			}
		}
	}

	char *trim ( ) noexcept
	{
		if ( buffStart )
		{
			memmove ( buff, buff + buffStart, buffPos - buffStart );
			buffPos -= buffStart;
			buffStart = 0;
		}
		return (buff);
	}


	void unPut ( ) noexcept
	{
		if ( buffPos > buffStart )
		{
			buffPos--;
		} else
		{
			buffPos = 0;
			buffStart = 0;
		}
		return;
	}

	void unWrite ( size_t len ) noexcept
	{
		if ( buffPos - len >= buffStart )
		{
			buffPos -= len;
		} else
		{
			buffPos = 0;
			buffStart = 0;
		}
		return;
	}

	void writeAt ( size_t offset, void const *str, size_t len )
	{
		if ( buffStart + offset + len > buffPos )
		{
			// trim off the end
			unWrite (  (buffPos - buffStart) - offset );

			// append it
			write ( str, len );
		} else
		{
			// just overwrite
			memcpy ( buff + buffStart + offset, str, len );
		}
	}

	void write ( void const *str, size_t len )
	{
		if ( len )
		{
			makeRoom ( len );
			memcpy ( buff + buffPos, str, len );
			buffPos += len;
		}
	}

	void write ( int64_t value )
	{
		char tmp[65];
		_i64toa_s ( value, tmp, sizeof ( tmp ), 10 );
		write ( tmp );
	}
	void write ( const char *str )
	{
		size_t len = strlen ( reinterpret_cast<char const *>(str) );
		if ( len )
		{
			makeRoom ( len );
			memcpy ( buff + buffPos, str, len );
			buffPos += len;
		}
	}

	void set ( char val, size_t len )
	{
		if ( len )
		{
			makeRoom ( len );
			memset ( buff + buffPos, val, len );
			buffPos += len;
		}
	}

	template <typename T>
	void put ( T data )
	{
		makeRoom ( sizeof ( T ) );
		*(T *) (buff + buffPos) = data;
		buffPos += sizeof ( T );
	}

	template <typename T>
	void putAt ( size_t offset, T &data )
	{
		writeAt ( offset, &data, sizeof ( data ) );
	}

	void writeln ( void const *str, size_t len )
	{
		makeRoom ( len + 2 );
		memcpy ( buff + buffPos, str, len );
		buff[buffPos + len] = '\r';
		buff[buffPos + len + 1] = '\n';
		buffPos += len + 2;
	}

	BUFFER &operator += ( char const *str )
	{
		size_t	len = strlen ( str );

		makeRoom ( len );
		memcpy ( buff + buffPos, str, len );
		buffPos += len;

		return *this;
	}

	BUFFER operator + ( char const *str )
	{
		BUFFER buff;
		buff.write ( this->data<const char *> ( ), this->size() );
		buff.write ( str );
		return buff;
	}

	template<typename T=void>
	T *detach ( )
	{
		void *ret;

		if ( usingDefault )
		{
			ret = malloc ( sizeof ( char ) * (buffPos - buffStart) + 1);
			if ( !ret ) throw errorNum::scMEMORY;
			memcpy ( ret, buff + buffStart, buffPos - buffStart );
		} else
		{
			ret = trim ();
		}

		buff = defaultBuffer;
		buffStart = 0;
		buffPos = 0;
		buffSize = SERVER_BUFFER_DEFAULT_SIZE;
		usingDefault = true;
		return reinterpret_cast<T *>(ret);
	}

	auto printf ( char const *fmt, ... )
	{
		int			 len;
		va_list		 varArgs = {};

		len = -1;

		size_t room = 0;
		while ( len == -1 )
		{
			makeRoom ( 128 );
			room += 128;
			va_start ( varArgs, fmt );
			len = _vsnprintf_s ( bufferBuff ( this ) + bufferSize ( this ), room, _TRUNCATE, fmt, varArgs );
			va_end ( varArgs );
		}

		buffPos += len;
	}

	auto vprintf ( char const *fmt, va_list argptr )
	{
		int			 len;

		len = -1;

		size_t room = 0;
		while ( len == -1 )
		{
			makeRoom ( 128 );
			room += 128;
			len = _vsnprintf_s ( bufferBuff ( this ) + bufferSize ( this ), room, _TRUNCATE, fmt, argptr );
		}

		buffPos += len;
	}

	void setColor ( consoleColor fg = consoleColor::Gray, consoleColor bg = consoleColor::Black )
	{
		write ( windowsToFgColorCode[int(fg)] );
//		writeString ( windowsToBgColorCode[bg] );
	}

	friend std::ostream &operator << ( std::ostream &os, BUFFER const &buff );
};

extern	void		 bufferRemove( BUFFER *buffer, size_t len );
extern	char		*bufferTrimmedBuff( BUFFER *buffer );
extern	void		 bufferUnPut( BUFFER *buffer );
extern	void		 bufferMakeRoom( BUFFER *buffer, size_t len );
extern	void		 bufferAssume( BUFFER *buffer, size_t len );
extern	void		 bufferWrite( BUFFER *buffer, void const *str, size_t len );
extern	void		 bufferWriteAt( BUFFER *buffer, size_t offset, void const *str, size_t len );
extern	void		 bufferWriteString( BUFFER *buffer, char const *str );
extern	void		 bufferPut ( BUFFER *buffer, uint8_t chr );
extern	void		 bufferPut8( BUFFER *buffer, uint8_t chr );
extern	void		 bufferPut16( BUFFER *buffer, uint16_t val );
extern	void		 bufferPut32( BUFFER *buffer, uint32_t  val );
extern	void		 bufferPut64( BUFFER *buffer, uint64_t  val );
extern	void		 bufferWriteln( BUFFER *buffer, void const *str, size_t len );
extern	void		 bufferReset( BUFFER *buffer );
extern  void		*bufferDetach( BUFFER *buffer );
extern  void		 bufferUnWrite( BUFFER *buffer, size_t len );
extern  void		 bufferAlign( BUFFER *buffer, size_t len );

//
//		........xxxxxxxxxxxxxxxx+++++++++++++++++++++++++++
//				buffStart
//								buffPos
//													       buffSize
//


inline void bufferForceRemove( BUFFER *buffer, size_t len )
{
	buffer->forceRemove ( len );
}

inline void bufferRemove( BUFFER *buffer, size_t len )
{
	buffer->remove ( len );
}

inline void bufferRemoveAt( BUFFER *buffer, size_t at, size_t len )
{
	buffer->removeAt ( at, len );
}

inline char *bufferTrimmedBuff( BUFFER *buffer )
{
	return buffer->trim ( );
}

inline void bufferUnPut( BUFFER *buffer )
{
	buffer->unPut ( );
}

inline void bufferUnWrite( BUFFER *buffer, size_t len )
{
	buffer->unWrite ( len );
}

inline void bufferMakeRoom( BUFFER *buffer, size_t len )
{
	buffer->makeRoom ( len );
}

inline void bufferAssume( BUFFER *buffer, size_t len )
{
	buffer->assume ( len );
}

inline void bufferAlign ( BUFFER *buffer, size_t len )
{
	buffer->align ( len );
}

inline void bufferWriteAt( BUFFER *buffer, size_t offset, void const *str, size_t len )
{
	buffer->writeAt ( offset, str, len );
}

inline void bufferWrite( BUFFER *buffer, void const *str, size_t len )
{
	buffer->write ( str, len );
}

inline void bufferSet( BUFFER *buffer, char val, size_t len )
{
	buffer->set ( val, len );
}

inline void bufferPut ( BUFFER *buffer, uint8_t val )
{
	buffer->put ( val );
}

inline void bufferPut8 ( BUFFER *buffer, uint8_t val )
{
	buffer->put ( val );
}

inline void bufferPut16( BUFFER *buffer, uint16_t val )
{
	buffer->put ( val );
}

inline void bufferPut32( BUFFER *buffer, uint32_t val )
{
	buffer->put ( val );
}

inline void bufferPut64( BUFFER *buffer, uint64_t val )
{
	buffer->put ( val );
}

inline void bufferPutDouble( BUFFER *buffer, double val )
{
	buffer->put ( val );
}

inline void bufferPutDoubleAt ( BUFFER *buffer, int offset, double val )
{
	buffer->putAt ( offset, val );
}

inline void bufferWriteln( BUFFER *buffer, void const *str, size_t len )
{
	buffer->writeln ( str, len );
}

inline void bufferWriteString( BUFFER *buffer, char const *str )
{
	buffer->write ( str );
}

inline void *bufferDetach( BUFFER *buffer )
{
	return buffer->detach ( );
}

inline void bufferReset( BUFFER *buffer )
{
	buffer->reset();
}
