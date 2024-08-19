#pragma once

#include "Utility/settings.h"

#include "stdlib.h"
#include <string>
#include <type_traits>
#include <unordered_set> 
#include "Utility/encodingTables.h"
#include "util.h"

class stringi final
{
	std::string		str;

public:
	stringi ( ) noexcept
	{};
	~stringi ( ) noexcept
	{
		return;
	}
	template <typename ...T>
	stringi ( T  &&... t )
	{
		str = std::string ( std::forward<T> ( t )... );
	}

	stringi ( double v)
	{
		char tmpBuff[_CVTBUFSIZE];
		sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "%3.1f", v );
		str = tmpBuff;
	}
	stringi ( int64_t v )
	{
		char tmpBuff[65];
		_i64toa_s ( v, tmpBuff, sizeof ( tmpBuff ), 10 );
		str = tmpBuff;
	}
	stringi ( uint64_t v )
	{
		char tmpBuff[65];
		_ui64toa_s ( v, tmpBuff, sizeof ( tmpBuff ), 10 );
		str = tmpBuff;
	}

	stringi ( const std::string &s )
	{
		str = s;
	}
	stringi ( char const *s )
	{
		str = s;
	}
	stringi ( char const *s, size_t len )
	{
		str = std::string ( s, len );
	}
	stringi ( stringi &old )
	{
		str = old.str;
	}
	stringi ( const stringi &old )
	{
		str = old.str;
	}
	stringi ( stringi &&old ) noexcept
	{
		*this = std::move ( old );
	}

	stringi ( char const **expr );
	void serialize ( struct BUFFER *buff ) const;

	stringi &operator = ( const stringi &old )
	{
		hashCacheValid = false;
		str = old.str;
		return *this;
	}
	stringi &operator = ( stringi &&old ) noexcept
	{
		hashCacheValid = false;
		str.swap ( old.str );
		return *this;
	}
	void reserve ( size_t size )
	{
		str.reserve ( size );
	}
	void swap ( stringi &&old )  noexcept
	{
		hashCacheValid = false;
		str.swap ( old.str );
	}
	void clear ( ) noexcept
	{
		hashCacheValid = false;
		str.clear ( );
	}
	stringi &trim ( )
	{
		hashCacheValid = false;
		auto p = str.find_first_not_of ( " \t" );
		str.erase ( 0, p );

		p = str.find_last_not_of ( " \t" );
		if ( std::string::npos != p )
		{
			str.erase ( p + 1 );
		}
		return *this;
	}
	template <typename ... T>
	auto append ( T const &...t )
	{
		hashCacheValid = false;
		str.append ( t... );
		return *this;
	}

	template <typename ... T>
	auto insert ( T const &...t )
	{
		hashCacheValid = false;
		str.insert ( t... );
		return *this;
	}

	template <typename T, typename std::enable_if <!std::is_same_v<T, stringi>, bool >::type = true>
	auto find ( T const &t ) const
	{
		return str.find ( t );
	}

	template <typename T, typename std::enable_if <std::is_same_v<T, stringi>, bool >::type = true>
	auto find ( T const &s ) const
	{
		size_t loop;

		if ( s.size () > str.size () ) return stringi::npos;

		auto max = str.size () - s.size ();
		for ( loop = 0; loop <= max; loop++ )
		{
			if ( ((s.str.c_str ( )[0] | 32) == (str.c_str()[loop] | 32)) && !memcmpi ( s.c_str() + 1, str.c_str () + loop + 1, s.size () - 1 ) )
			{
				return loop;
			}
		}
		return stringi::npos;
	}

	template <typename T>
	auto rfind ( T const &t ) const
	{
		return str.rfind ( t );
	}

	template <typename T>
	auto find_last_of ( T const &t ) const
	{
		return str.find_last_of ( t );
	}

	template <typename ... T>
	auto substr ( T const &...t ) const
	{
		return stringi ( str.substr ( t... ) );
	}

	template <typename ... T>
	auto erase ( T const &...t )
	{
		return stringi ( str.erase ( t... ) );
	}

	auto begin ( ) const noexcept ->decltype(str.begin ( ))
	{
		return str.begin ( );
	}
	auto end ( ) const noexcept ->decltype(str.end ( ))
	{
		return str.end ( );
	}
	auto rbegin ( ) const noexcept ->decltype(str.rbegin ( ))
	{
		return str.rbegin ( );
	}
	auto rend ( ) const noexcept ->decltype(str.rend ( ))
	{
		return str.rend ( );
	}
	auto cbegin ( ) const noexcept ->decltype(str.cbegin ( ))
	{
		return str.cbegin ( );
	}
	auto cend ( )  const noexcept ->decltype(str.cend ( ))
	{
		return str.cend ( );
	}
	auto crbegin ( ) const noexcept ->decltype(str.crbegin ( ))
	{
		return str.crbegin ( );
	}
	auto crend ( )  const noexcept ->decltype(str.crend ( ))
	{
		return str.crend ( );
	}

	stringi findAndReplace ( stringi const &find, stringi const &replace ) const
	{
		stringi result = *this;
		size_t pos = 0;

		assert ( find.size () );
		while ( (pos = result.str.find ( find.str, pos )) != std::string::npos )
		{
			result.str.replace ( pos, find.size ( ), replace.c_str ( ) );
			pos += replace.size ( );
		}
		return result;
	}

	static constexpr auto npos = std::string::npos;

	mutable size_t hashCache = 0;
	mutable bool   hashCacheValid = false;

	inline size_t hash ( ) const noexcept
	{
		if ( hashCacheValid )
		{
			return hashCache;
		}
		size_t h = 5381;

		// dan bernstein's djb2 hash algorithm
		for ( auto const p : str )
		{
			h = ((h << 5) + h) ^ caseInsenstiveTable[(uint8_t) p];		// h(i) = h(i-1) * 33 ^ str[i]
		}
		hashCache = h;
		hashCacheValid = true;
		return h;
	}

	inline stringi operator + ( const char *string ) const
	{
		stringi ret = str;
		ret.str += string;
		return ret;
	}
	inline stringi operator + ( stringi const &string ) const
	{
		stringi ret = str;
		ret.str += string.str;
		return ret;
	}
	inline stringi operator + ( std::string const &string ) const
	{
		stringi ret = str;
		ret.str += string;
		return ret;
	}
	inline stringi operator + ( int iVal ) const
	{
		stringi ret = str;
		char tmpBuff[65];
		_itoa_s ( iVal, tmpBuff, sizeof ( tmpBuff ), 10 );
		ret.str += tmpBuff;
		return ret;
	}
	inline stringi operator + ( uint32_t iVal ) const
	{
		stringi ret = str;
		char tmpBuff[65];
		_i64toa_s ( (int64_t)iVal, tmpBuff, sizeof ( tmpBuff ), 10 );
		ret.str += tmpBuff;
		return ret;
	}
	inline stringi operator + ( int64_t iVal ) const
	{
		stringi ret = str;
		char tmpBuff[65];
		_i64toa_s ( (int64_t) iVal, tmpBuff, sizeof ( tmpBuff ), 10 );
		ret.str += tmpBuff;
		return ret;
	}
	inline stringi operator + ( uint64_t iVal ) const
	{
		stringi ret = str;
		char tmpBuff[65];
		_ui64toa_s ( (uint64_t) iVal, tmpBuff, sizeof ( tmpBuff ), 10 );
		ret.str += tmpBuff;
		return ret;
	}
	inline stringi operator + ( double dVal ) const
	{
		stringi ret = str;
		char tmpBuff[_CVTBUFSIZE];
		sprintf_s( tmpBuff, sizeof( tmpBuff ), "%3.1f", dVal );
		ret.str += tmpBuff;
		return ret;
	}
	inline stringi &operator = ( char const *s )
	{
		hashCacheValid = false;
		str = s;
		return *this;
	}
	inline stringi &operator = ( char *s )
	{
		hashCacheValid = false;
		str = s;
		return *this;
	}
	inline stringi &operator = ( std::string const *s )
	{
		hashCacheValid = false;
		str = *s;
		return *this;
	}
	inline stringi &operator = ( stringi const *s )
	{
		hashCacheValid = false;
		str = s->str;
		return *this;
	}
	inline stringi &operator += ( std::string const *s )
	{
		hashCacheValid = false;
		str += *s;
		return *this;
	}
	inline stringi &operator += ( stringi const *s )
	{
		hashCacheValid = false;
		str += s->str;
		return *this;
	}
	inline stringi &operator += ( stringi &s )
	{
		hashCacheValid = false;
		str += s.str;
		return *this;
	}
	inline stringi &operator += ( stringi const &s )
	{
		hashCacheValid = false;
		str += s.str;
		return *this;
	}
	inline stringi &operator += ( char const c )
	{
		hashCacheValid = false;
		str += c;
		return *this;
	}
	template <typename T>
	inline char &operator [] ( T position ) noexcept
	{
		return str[position];
	}
	template <typename T>
	inline char const &operator [] ( T position ) const noexcept
	{
		return str[position];
	}
	inline operator std::string &()
	{
		return str;
	}
	inline operator std::string const &() const 
	{
		return str;
	}
	inline operator std::string const () const
	{
		return str;
	}
	inline bool operator < ( const stringi &old ) const
	{
		return memcmpi ( str.c_str ( ), old.c_str ( ), str.size ( ) + 1 ) < 0;
	}

	inline bool operator == ( const stringi &obj ) const
	{
		return ((str.c_str ()[0] | 32) == (obj.c_str ()[0] | 32)) && !memcmpi ( str.c_str (), obj.str.c_str (), str.size () + 1 );
	}

	inline bool operator == ( const char *obj ) const
	{
//		return !strccmp ( str.c_str (), obj );
		return ((str.c_str ( )[0] | 32) == (obj[0] | 32)) && !strccmp ( str.c_str ( ), obj );
	}

	inline bool operator == ( const std::string &obj ) const
	{
		return ((str.c_str ( )[0] | 32) == (obj.c_str ( )[0] | 32)) && !strccmp ( str.c_str ( ), obj.c_str ( ) );
	}

	inline bool operator != ( const stringi &obj ) const
	{
		return !(((str.c_str ( )[0] | 32) == (obj.c_str ( )[0] | 32)) && !strccmp ( str.c_str ( ), obj.str.c_str ( ) ));
	}

	inline bool operator != ( const char *obj ) const
	{
		return !(((str.c_str ( )[0] | 32) == (obj[0] | 32)) && !strccmp ( str.c_str ( ), obj ));
	}

	inline bool operator != ( const std::string &obj ) const
	{
		return !(((str.c_str ( )[0] | 32) == (obj.c_str ( )[0] | 32)) && !strccmp ( str.c_str ( ), obj.c_str ( ) ));
	}

	inline operator char const *(void) const  noexcept
	{
		return str.c_str ( );
	}

	size_t length ( ) const noexcept
	{
		return str.length ( );
	}
	size_t size ( ) const noexcept
	{
		return str.size ( );
	}
	char const *c_str ( ) const noexcept
	{
		return str.c_str ( );
	}
};

namespace std {
	template <>
	class hash<stringi>
	{
	public:
		size_t operator()( const stringi &obj ) const noexcept
		{
			return obj.hash ( );
		}
	};
}
