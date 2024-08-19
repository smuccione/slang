#pragma once

#include "Utility/settings.h"

#include <stdlib.h>
#include <string>
#include <unordered_set> 
#include "buffer.h"

class cacheString
{
	cacheString ( stringi const *entry )  noexcept : entry ( entry )
	{
	}

public:
	stringi const *entry;

	static constexpr auto npos = stringi::npos;

	cacheString ()  noexcept : entry ( nullptr )
	{
	}

	void serialize ( BUFFER *buff ) const
	{
		if ( !entry )
		{
			return stringi ().serialize ( buff );
		}
		entry->serialize ( buff );
	}

	operator stringi const &() const noexcept
	{
		return *entry;
	}

	bool operator == ( char const *str ) const noexcept
	{
		return !memcmpi ( entry->c_str (), str, entry->size () + 1 );
	}

	bool operator == ( cacheString const &right ) const noexcept 
	{
		return entry == right.entry;
	}
	bool operator != ( cacheString const &right ) const noexcept
	{
		return entry != right.entry;
	}
	inline bool operator < ( const cacheString &old ) const noexcept
	{
		return memcmpi ( entry->c_str ( ), old.entry->c_str ( ), entry->size ( ) + 1 ) < 0;
	}

	inline operator char const *(void) const  noexcept
	{
		if ( !entry )
		{
			return nullptr;
		}
		return entry->c_str ( );
	}

	size_t length ( ) const noexcept
	{
		if ( !entry )
		{
			return 0;
		}
		return entry->length ( );
	}
	size_t size ( ) const noexcept
	{
		if ( !entry )
		{
			return 0;
		}
		return entry->size ( );
	}
	char const *c_str ( ) const noexcept
	{
		if ( !entry )
		{
			return nullptr;
		}
		return entry->c_str ( );
	}
	size_t hash () const noexcept
	{
		if ( entry )
		{
			return entry->hash();
		} else
		{
			return 0;
		}
	}

	template <typename T, typename std::enable_if<!std::is_same_v<T, stringi>, bool>::type = true >
	auto find ( T t ) const noexcept
	{
		return entry->find ( t );
	}

	template <typename T, typename std::enable_if <std::is_same_v<T, stringi>, bool >::type = true>
	auto find ( T s ) const noexcept
	{
		return find ( (*s.entry) );
	}

	template <typename T>
	auto rfind ( T t ) const
	{
		return entry->rfind ( t );
	}

	template <typename T>
	auto find_last_of ( T t ) const
	{
		return entry->find_last_of ( t );
	}

	template <typename ... T>
	auto substr ( T ...t ) const
	{
		return stringi ( entry->substr ( t... ) );
	}

	auto cbegin ( )  const noexcept ->decltype(entry->cbegin ( ))
	{
		return entry->cbegin ( );
	}
	auto cend ( )  const noexcept ->decltype(entry->cend ( ))
	{
		return entry->cend ( );
	}
	auto crbegin ( )  const noexcept ->decltype(entry->crbegin ( ))
	{
		return entry->crbegin ( );
	}
	auto crend ( )  const noexcept ->decltype(entry->crend ( ))
	{
		return entry->crend ( );
	}

	friend class stringCache;
};

class stringCache
{
	std::unordered_set<stringi>		cache;

public:
	bool has ( stringi const &str )
	{
		auto it = cache.find ( str );
		return it != cache.end ();
	}
	cacheString const get( stringi const &str );

	cacheString const get ( char const **expr )
	{
		stringi tmp ( expr );

		if ( !tmp.size () )
		{
			return cacheString ();
		}
		auto it = cache.insert ( tmp );
		assert ( tmp == (stringi)*it.first );
		return cacheString ( &(*it.first) );
	}

	cacheString const get ( char const *format, ... )
	{
		char		tmpBuff[512];
		va_list		marker;

		va_start ( marker, format );
		vsprintf_s ( tmpBuff, sizeof ( tmpBuff ), format, marker );

		va_end ( marker );

		return get ( stringi ( tmpBuff ) );
	}

	cacheString const get ( char const *format, va_list argList )
	{
		char		tmpBuff[512];
		vsprintf_s ( tmpBuff, sizeof ( tmpBuff ), format, argList );

		return get ( stringi ( tmpBuff ) );
	}
};

namespace std {
	template <>
	class hash<cacheString>
	{
	public:
		size_t operator()( const cacheString &obj ) const noexcept
		{
			return obj.hash ( );
		}
	};
}

