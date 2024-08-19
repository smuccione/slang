
#include "Utility/settings.h"

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>
#include <variant>

#include "Utility/stringCache.h"
#include "Utility/stringi.h"
#include "Utility/buffer.h"
#include "Utility/funcky.h"
#include "jsonParser.h"

jsonElement::jsonElement ( char const **str )
{
	while ( _isspacex ( *str ) ) (*str)++;		// skip spaces and eol characters

	if ( (*str)[0] == '{' )
	{
		// we're a json object
		(*str)++;

		value = jsonElement::objectType ();

		auto &obj = std::get<jsonElement::objectType> ( value );

		bool first = true;
		for ( ;; )
		{
			while ( _isspacex ( *str ) ) (*str)++;		// skip spaces and eol characters

			if ( (*str)[0] == '}' )
			{
				(*str)++;
				break;
			}
			if ( !first )
			{
				if ( (*str)[0] != ',' )
				{
					throw std::exception ( "missing comma" );
				}
				(*str)++;
				while ( _isspacex ( *str ) ) (*str)++;		// skip spaces and eol characters
				if ( (*str)[0] == '}' )
				{
					(*str)++;
					break;
				}
			}
			first = false;

			stringi name;
			if ( (*str)[0] == '"' )
			{
				(*str)++;
				while ( **str && (*str)[0] != '"' )
				{
					name += *((*str)++);
				}
				if ( **str )
				{
					(*str)++;
				} else
				{
					throw std::exception ( "missing \"" );
				}
			} else
			{
				if ( !_issymbol ( (*str) ) )
				{
					throw std::exception ( "invalid json symbol value" );
				}
				while ( _issymbolb ( (*str) ) )
				{
					name += *((*str)++);
				}
			}

			while ( _isspacex ( *str ) ) (*str)++;		// skip spaces and eol characters
			if ( (*str)[0] != ':' )
			{
				throw std::exception ( "missing name/value separator" );
			}
			(*str)++;

			while ( _isspacex ( *str ) ) (*str)++;		// skip spaces and eol characters

			obj[name] = jsonElement ( str );
		}
	} else if ( (*str)[0] == '[' )
	{
		(*str)++;
		value = jsonElement::arrayType ();

		auto &arr = std::get<jsonElement::arrayType> ( value );

		bool first = true;
		for ( ;; )
		{
			while ( _isspacex ( *str ) ) (*str)++;		// skip spaces and eol characters

			if ( (*str)[0] == ']' )
			{
				(*str)++;
				break;
			}
			if ( !first )
			{
				if ( (*str)[0] != ',' )
				{
					throw std::exception ( "missing comma" );
				}
				(*str)++;
				while ( _isspacex ( *str ) ) (*str)++;		// skip spaces and eol characters
			}
			first = false;

			arr.push_back ( jsonElement ( str ) );
		}
	} else if ( (*str)[0] == '"' )
	{
		// we're a string
		(*str)++;

		stringi v;
		while ( **str && (*str)[0] != '"' )
		{
			if ( (*str)[0] == '\\' && (*str)[1] == '"' )
			{
				v += '"';
				(*str) += 2;
			} else if ( (*str)[0] == '\\' && (*str)[1] == 'r' )
			{
				v += '\r';
				(*str) += 2;
			} else if ( (*str)[0] == '\\' && (*str)[1] == 'n' )
			{
				v += '\n';
				(*str) += 2;
			} else if ( (*str)[0] == '\\' && (*str)[1] == 't' )
			{
				v += '\t';
				(*str) += 2;
			} else if ( (*str)[0] == '\\' && (*str)[1] )
			{
				v += (*str)[1];
				(*str) += 2;
			} else
			{
				v += *((*str)++);
			}
		}
		if ( **str )
		{
			(*str)++;
		} else
		{
			throw std::exception ( "missing \"" );
		}
		value = v;
	} else if ( _isnum ( *str ) )
	{
		stringi v;
		bool isFloat = false;
		while ( _isnum ( *str ) )
		{
			if ( **str == '.' ) isFloat = true;
			v += *((*str)++);
		}
		if ( isFloat )
		{
			value = atof ( v.c_str () );
		} else
		{
			value = _atoi64 ( v.c_str () );
		}
	} else if ( !memcmpi ( *str, "true", 4 ) )
	{
		value = true;
		*str += 4;
	} else if ( !memcmpi ( *str, "false", 5 ) )
	{
		value = false;
		*str += 5;
	} else if ( !memcmpi ( *str, "null", 4 ) )
	{
		value = std::monostate();
		*str += 4;
	} else
	{
		throw std::exception ( "missing \"" );
	}
}

// ----------------------------------------------- creater functions
jsonElement::operator bool ( )
{
	if ( std::holds_alternative<int64_t> ( value ) )
	{
		value = std::get<int64_t> ( value ) ? true : false;
	} else if ( !std::holds_alternative<bool> ( value ) )
	{
		value = false;
	}
	return std::get<bool> ( value );
}
jsonElement::operator int64_t ()
{
	if ( std::holds_alternative<double> ( value ) )
	{
		value = (int64_t) std::get<double> ( value );
	}
	if ( !std::holds_alternative<int64_t> ( value ) )
	{
		value = (int64_t) 0;
	}
	return std::get<int64_t> ( value );
}
jsonElement::operator double ()
{
	if ( std::holds_alternative<int64_t> ( value ) )
	{
		value = (double) std::get<int64_t> ( value );
	} else if ( !std::holds_alternative<double> ( value ) )
	{
		value = 0.0;
	}
	return std::get<double> ( value );
}
jsonElement::operator stringi ()
{
	if ( std::holds_alternative<int64_t> ( value ) )
	{
		value = (double) std::get<int64_t> ( value );
	} else if ( !std::holds_alternative<stringi> ( value ) )
	{
		value = stringi ();
	}
	return std::get<stringi> ( value );
}

// ------------------------------------- reader functions
jsonElement::operator int64_t const () const
{
	if ( std::holds_alternative<int64_t> ( value ) )
	{
		return std::get<int64_t> ( value );
	}
	throw std::exception ( "invalid json integer value" );
}
jsonElement::operator bool const () const
{
	if ( std::holds_alternative<bool> ( value ) )
	{
		return std::get<bool> ( value );
	} else if ( std::holds_alternative<arrayType> ( value ) )
	{
		return !std::get<arrayType> ( value ).empty ();
	}
	throw std::exception ( "invalid json integer value" );
}
jsonElement::operator double const () const
{
	if ( std::holds_alternative<double> ( value ) )
	{
		return std::get<double> ( value );
	}
	throw std::exception ( "invalid json double value" );
}

jsonElement::operator stringi const & () const
{
	if ( std::holds_alternative<stringi> ( value ) )
	{
		return std::get<stringi> ( value );
	}
	throw std::exception ( "invalid json string value" );
}

size_t jsonElement::size () const
{
	if ( std::holds_alternative<objectType> ( value ) )
	{
		auto &obj = std::get<objectType> ( value );
		return obj.size ();
	} else if ( std::holds_alternative<arrayType> ( value ) )
	{
		auto &arr = std::get<arrayType> ( value );
		return arr.size ();
	} else if ( std::holds_alternative<std::monostate> ( value ) )
	{
		return 0;
	}
	throw std::exception ( "invalid usage" );
}
// ------------------------------- serialization
void jsonElement::serialize ( BUFFER &buff, bool quoteNames ) const
{
	if ( std::holds_alternative<objectType> ( value ) )
	{
		auto &obj = std::get<objectType> ( value );
		buff.put ( '{' );
		bool first = true;
		for ( auto &&[name, v] : obj )
		{
			if ( !first )
			{
				buff.put ( ',' );
			}
			first = false;
			if ( quoteNames ) buff.put ( '\"' );
			buff.write ( name.c_str (), name.size () );
			if ( quoteNames ) buff.put ( '\"' );
			buff.put ( ':' );
			v.serialize ( buff, quoteNames );
		}
		buff.put ( '}' );
	} else if ( std::holds_alternative<arrayType> ( value ) )
	{
		auto &arr = std::get<arrayType> ( value );
		buff.put ( '[' );
		bool first = true;
		for ( auto const &it : arr )
		{
			if ( !first )
			{
				buff.put ( ',' );
			}
			first = false;
			it.serialize ( buff, quoteNames );
		}
		buff.put ( ']' );
	} else if ( std::holds_alternative<int64_t> ( value ) )
	{
		auto v = std::get<int64_t> ( value );
		buff.printf ( "%I64i", v );
	} else if ( std::holds_alternative<double> ( value ) )
	{
		auto v = std::get<double> ( value );
		buff.printf ( "%d", v );
	} else if ( std::holds_alternative<stringi> ( value ) )
	{
		auto const &v = std::get<stringi> ( value );
		buff.makeRoom( v.size() + 2 );
		buff.put( '\"' );
		for ( auto const &it : v )
		{
			switch ( it )
			{
				case '\"':
					buff.write( "\\\"", 2 );
					break;
				case '\\':
					buff.write( "\\\\", 2 );
					break;
				case '\r':
					buff.write( "\\r", 2 );
					break;
				case '\n':
					buff.write( "\\n", 2 );
					break;
				case '\t':
					buff.write( "\\t", 2 );
					break;
				default:
					if ( it < 32 || it > 127 )
					{
						buff.put( '%' );
						buff.put( hexEncodingTable[(it & 0xF0) >> 4] );
						buff.put( hexEncodingTable[(it & 0x0F)] );
					} else
					{
						buff.put( it );
					}
			}
		}
		buff.put( '\"' );
	} else	if ( std::holds_alternative<bool> ( value ) )
	{
		if ( std::get<bool> ( value ) )
		{
			buff.write ( "true", 4 );
		} else
		{
			buff.write ( "false", 5 );
		}
	} else if ( std::holds_alternative<std::monostate> ( value ) )
	{
		buff.write ( "null", 4 );
	}
}

jsonElement jsonParser ( char const *str )
{
	auto tmpStr = &str;
	auto result = jsonElement ( tmpStr );
	while ( _isspacex ( *tmpStr ) ) (*tmpStr)++;		// skip spaces and eol characters
	if ( **tmpStr )
	{
		throw std::exception ( "invalid json" );
	}
	return result;
}
