#pragma once

#include "Utility/settings.h"

#include "stdlib.h"
#include "encodingTables.h"
#include <unordered_set> 
#include "Utility/funcky.h"
#include "Utility/util.h"

class statString {
	char const	*string;
	size_t		 len;

public:
	statString () noexcept
	{
		string = "";
		len = 0;
	}
	statString ( char *string, size_t len ) noexcept
	{
		this->string = string;
		this->len = len;
	}

	statString ( char const *string, size_t len ) noexcept
	{
		this->string = (char *)string;
		this->len = len;
	}

	statString ( char const *string ) noexcept
	{
		this->string = (char *)string;
		this->len = strlen ( string );
	}

	inline size_t hash() const noexcept
	{
		size_t h = 5381;

		// dan bernstein's djb2 hash algorithm
		for ( auto p = string; p  < string + len; p++ )
		{
			h = ((h << 5) + h) ^ caseInsenstiveTable[(uint8_t) *p];		// h(i) = h(i-1) * 33 ^ str[i]
		}
		return h;
	}

	inline bool operator < ( const statString &old ) const
	{
		size_t const cmpLen = len > old.len ? len : old.len;
		return ( memcmpi ( string, old.string, cmpLen ) < 0 );
	}

	inline bool operator == ( const statString &obj ) const
	{
		if ( len != obj.len ) return false;
		if ( memcmpi ( string, obj.string, len ) ) return false;
		return true;
	}
	
	char const *c_str ( void ) const
	{
		return string;
	}

	size_t length ( void ) const
	{
		return len;
	}
};

namespace std {
	template <>
	class hash<statString> {
	public:
		size_t operator()(const statString &obj) const  noexcept
		{
			return obj.hash();
		}
	};
}

class statementString {
	char	*string;
	size_t	 len;

	public:

	statementString ( char *string, size_t len ) noexcept
	{
		this->string = string;
		this->len = len;
	}

	statementString ( char const *string, size_t len ) noexcept
	{
		this->string = (char *) string;
		this->len = len;
	}

	statementString ( char const *string ) noexcept
	{
		this->string = (char *) string;
		this->len = -1;
	}

	inline size_t hash ( ) const  noexcept
	{
		size_t h = 5381;

		// dan bernstein's djb2 hash algorithm
		for ( auto p = string; p  < string + len; p++ )
		{
			h = ((h << 5) + h) ^ caseInsenstiveTable[(uint8_t) *p];		// h(i) = h(i-1) * 33 ^ str[i]
		}
		return h;
	}

	inline bool operator < ( const statementString &old ) const 
	{
		if ( len < old.len )
		{
			auto const r = memcmpi ( string, old.string, len );
			if ( !r )
			{
				if ( !_issymbol ( string ) ) return false;
				if ( !old.c_str ( )[len] ) return false;
				if ( _issymbol ( &old.c_str ( )[len] ) )
				{
					return true;
				}
				return false;
			} else if ( r < 0 )
			{
				return true;
			}
			return false;
		} else
		{
			return memcmpi ( string, old.string, old.len ) < 0;
		}
	}

	inline bool operator == ( const statementString &old ) const
	{
		auto const r = memcmpi ( string, old.string, len );
		if ( !r )
		{
			if ( !old.c_str ( )[len] ) return true;
			if ( !_issymbol ( &old.c_str ( )[len] ) ) return true;
			return false;
		}
		return false;
	}

	char const *c_str ( void ) const  noexcept
	{
		return string;
	}

	size_t length ( void ) const  noexcept
	{
		return len;
	}
	size_t size ( void ) const  noexcept
	{
		return len;
	}
};

namespace std
{
	template <>
	class hash<statementString> {
		public:
		size_t operator()( const statementString &obj ) const  noexcept
		{
			return obj.hash ( );
		}
	};
}

class staticLargeString {
	char	*string;
	size_t	 len;

	public:

	staticLargeString ( char *string, size_t len )  noexcept
	{
		this->string = string;
		this->len = len;
	}

	staticLargeString ( char const *string, size_t len )  noexcept
	{
		this->string = (char *) string;
		this->len = len;
	}

	staticLargeString ( char const *string ) noexcept
	{
		this->string = (char *) string;
		this->len = -1;
	}

	inline bool operator < ( const staticLargeString &old ) const
	{
		if ( len < old.len )
		{
			auto const r = memcmpi ( string, old.string, len );
			if ( !r )
			{
				return false;
			} else if ( r < 0 )
			{
				return true;
			}
			return false;
		} else
		{
			return memcmpi ( string, old.string, old.len ) < 0;
		}
	}

	inline bool operator == ( const staticLargeString &old ) const
	{
		auto const r = memcmpi ( string, old.string, len );
		if ( !r )
		{
			if ( !old.c_str ( )[len] ) return true;
			if ( !_issymbol ( &old.c_str ( )[len] ) ) return true;
			return false;
		}
		return false;
	}

	char const *c_str ( void ) const noexcept
	{
		return string;
	}

	size_t length ( void ) const noexcept
	{
		return len;
	}
};
