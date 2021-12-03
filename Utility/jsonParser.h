#pragma once

#include "Utility/settings.h"

#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <variant>
#include <type_traits>
#include <memory>
#include <utility>
#include <cstdlib>

#include "Utility/stringCache.h"
#include "Utility/stringi.h"
#include "Utility/buffer.h"

class jsonElement
{
public:
	typedef  std::map<stringi, jsonElement> objectType;
	typedef  std::vector <jsonElement> arrayType;

private:
	std::variant < std::monostate, int64_t, double, stringi, objectType, arrayType, bool >	value;

	template <typename, typename>
	struct is_associative_container
	{
		static constexpr bool value = false;
	};

	template <typename C>
	struct is_associative_container <C, typename C::key_type>
	{
		static constexpr bool value = true;
	};

	template <typename, typename, typename>
	struct is_sequence_container
	{
		static constexpr bool value = false;
	};

	template <typename C>
	struct is_sequence_container <C, typename C::value_type, decltype(std::declval<C> ().clear ())>
	{
		static constexpr bool value = true;
	};

public:
	jsonElement ()
	{
	}

	template <class T, typename std::enable_if_t<is_associative_container<T, T>::value> * = nullptr >
	jsonElement(T const & o) : value(objectType(o.cbegin(),  o.cend()))
	{
	}

	// for array... needs to have a vector type of <jsonElement>
	template <class T, typename std::enable_if_t<is_sequence_container<T, T, T>::value> * = nullptr>
	jsonElement(T const & a) : value(arrayType(a.cbegin(), a.cend()))
	{}

	// for array... needs to have a vector type of <jsonElement>
	template <class T, typename std::enable_if_t<!is_sequence_container<T, T, T>::value && !is_associative_container<T, T>::value> * = nullptr>
	jsonElement ( T const &v )
	{
		if constexpr ( std::is_integral_v<T> || std::is_enum_v<T> && !std::is_same_v<bool, T> )
		{
			value = (int64_t) v;
		} else if constexpr ( std::is_floating_point_v<T> )
		{
			value = (double) v;
		} else if constexpr ( std::is_same_v<cacheString, std::remove_cv_t<T>> )
		{
			value = (stringi) v;
		} else
		{
			value = v;
		}
	}

	virtual ~jsonElement()
	{}

	jsonElement ( char const **str );

	jsonElement ( jsonElement &&old ) noexcept
	{
		*this = std::move ( old );
	}

	jsonElement ( jsonElement const &old )
	{
		value = old.value;
	}

	jsonElement &operator = ( jsonElement const &old )
	{
		value = old.value;
		return *this;
	}

	jsonElement &operator = ( jsonElement &&old ) noexcept
	{
		// free it an initialize it to the old type and copy old into us
		std::swap ( value, old.value  );
		return *this;
	}

	template <class T, typename std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T> && !std::is_same_v<jsonElement, typename std::remove_cvref_t<T>::type>> * = nullptr>
	jsonElement &operator = ( T const &v )
	{
		if constexpr ( std::is_same_v<bool, T> )
		{
			value = v;
		} else if constexpr ( std::is_integral_v<T> || std::is_enum_v<T> && !std::is_same_v<bool,T> )
		{
			value = (int64_t) v;
		} else if constexpr ( std::is_floating_point_v<T> )
		{
			value = (double) v;
		} else if constexpr ( std::is_same_v<cacheString, std::remove_cv<T>::type> )
		{
			value = (stringi) v;
		} else
		{
			value = v;
		}
		return *this;
	}

	template <class T, typename std::enable_if_t<!std::is_arithmetic_v<T> && !std::is_enum_v<T> > * = nullptr>
	jsonElement &operator = ( T const &v )
	{
		value = stringi ( v );
		return *this;
	}

	// ----------------------------------------------- writer functions
	template < typename T, typename std::enable_if_t<std::is_same_v<T, stringi>> * = nullptr>
	jsonElement &operator [] ( T const &name )
	{		
		if ( !std::holds_alternative<objectType> ( value ) )
		{
			value = objectType ();
		}
		auto &obj = std::get<objectType> ( value );
		return obj[name];
	}
	template < typename T, typename std::enable_if_t<std::is_same_v<T, cacheString>> * = nullptr>
	jsonElement &operator [] ( T const &name )
	{		
		if ( !std::holds_alternative<objectType> ( value ) )
		{
			value = objectType ();
		}
		auto &obj = std::get<objectType> ( value );
		return obj[(stringi)name];
	}
	template < typename T, typename std::enable_if_t<std::is_same_v<T, const char *>> * = nullptr>
	jsonElement &operator [] ( T name )
	{
		if ( !std::holds_alternative<objectType> ( value ) )
		{
			value = objectType ();
		}
		auto &obj = std::get<objectType> ( value );
		return obj[stringi(name)];
	}
	template < typename T, typename std::enable_if_t<std::is_integral_v<T>> * = nullptr>
	jsonElement &operator [] ( T index )
	{
		if ( !std::holds_alternative<arrayType> ( value ) )
		{
			value = arrayType ();
		}
		auto &arr = std::get<arrayType> ( value );
		if ( (size_t)index == arr.size () )
		{
			if ( arr.capacity () <= (size_t)index + 1 )
			{
				arr.reserve ( index + 256 );
			}
			arr.resize ( (size_t)index + 1 );
		}
		return arr[index];
	}
	template < typename ...T>
	void emplace_back ( T && ...t )
	{
		if ( !std::holds_alternative<arrayType>( value ) )
		{
			value = arrayType();
		}
		auto &arr = std::get<arrayType>( value );
		arr.emplace_back( std::forward<T...>( t... ) );
	}

	operator bool &();
	operator int64_t &();
	operator double &();
	operator stringi &();

	void clear ()
	{
		value = std::monostate();
	}
	jsonElement &makeArray ()
	{
		if ( std::holds_alternative<arrayType> ( value ) )
		{
		} else if ( std::holds_alternative<std::monostate> ( value ) )
		{
			value = arrayType ();
		} else
		{
			throw std::exception ( "cannot be made an array" );
		}
		return *this;
	}
	jsonElement &makeObject ()
	{
		if ( std::holds_alternative<objectType> ( value ) )
		{
		} if ( std::holds_alternative<std::monostate> ( value ) )
		{
			value = objectType ();
		} else
		{
			throw std::exception ( "cannot be made an array" );
		}
		return *this;
	}
	// ------------------------------------- reader functions

	template<typename T, typename std::enable_if_t<std::is_integral_v<T>> * = nullptr>
	jsonElement const *operator [] ( T index ) const
	{		
		if ( std::holds_alternative<arrayType> ( value ) )
		{
			auto &arr = std::get<arrayType> ( value );
			if ( (size_t)index < arr.size () )
			{
				return &arr[(size_t)index];
			}
			return nullptr;
		}
		return nullptr;
	}

	template<typename T, typename std::enable_if_t<std::is_same_v<T, stringi>> * = nullptr>
	jsonElement const *operator [] ( T const &name ) const
	{		
		if ( std::holds_alternative<objectType> ( value ) )
		{
			auto &obj = std::get<objectType> ( value );
			auto it = obj.find ( name );
			if ( it != obj.end () )
			{
				return &it->second;
			}
			return nullptr;
		}
		return nullptr;
	}
	template<typename T, typename std::enable_if_t<std::is_same_v<T, char const *>> * = nullptr>
	jsonElement const *operator [] ( T name ) const
	{		
		return (*this)[stringi ( name )];
	}

	void push_back ( jsonElement const &elem )
	{
		makeArray ();
		auto &arr = std::get<arrayType> ( value );
		arr.push_back ( elem );
	}

	jsonElement &reserve ( size_t size )
	{
		if ( std::holds_alternative<arrayType> ( value ) )
		{
		} else if ( std::holds_alternative<std::monostate> ( value ) )
		{
			value = arrayType ();
		} else
		{
			throw std::exception ( "cannot be made an array" );
		}
		auto &arr = std::get<arrayType> ( value );
		arr.reserve ( size);

		return *this;
	}

	auto cbeginObject () const
	{		
		if ( std::holds_alternative<objectType> ( value ) )
		{
			auto &obj = std::get<objectType> ( value );
			return obj.cbegin ();
		}
		throw std::exception ( "json iterating over not object" );
	}

	auto cendObject () const
	{		
		if ( std::holds_alternative<objectType> ( value ) )
		{
			auto &obj = std::get<objectType> ( value );
			return obj.cend ();
		}
		throw std::exception ( "json iterating over not object" );
	}

	auto cbeginArray () const
	{		
		if ( std::holds_alternative<arrayType> ( value ) )
		{
			auto &arr = std::get<arrayType> ( value );
			return arr.cbegin ();
		}
		throw std::exception ( "json iterating over non array" );
	}
	auto cendArray () const
	{		
		if ( std::holds_alternative<arrayType> ( value ) )
		{
			auto &arr = std::get<arrayType> ( value );
			return arr.cend ();
		}
		throw std::exception ( "json iterating over non array" );
	}

	operator bool const () const;
	operator int64_t const () const;
	operator double const () const;
	operator stringi () const;

	size_t size () const;
	bool isNull() const
	{
		if ( std::holds_alternative<std::monostate> ( value ) )
		{
			return true;
		} else
		{
			return false;
		}
	}
	bool isArray() const
	{
		if ( std::holds_alternative<arrayType> ( value ) )
		{
			return true;
		} else
		{
			return false;
		}
	}
	bool isObject() const
	{
		if ( std::holds_alternative<objectType> ( value ) )
		{
			return true;
		} else
		{
			return false;
		}
	}
	bool isInteger() const
	{
		if ( std::holds_alternative<int64_t> ( value ) )
		{
			return true;
		} else
		{
			return false;
		}
	}
	bool isDouble() const
	{
		if ( std::holds_alternative<double> ( value ) )
		{
			return true;
		} else
		{
			return false;
		}
	}
	bool isString() const
	{
		if ( std::holds_alternative<stringi> ( value ) )
		{
			return true;
		} else
		{
			return false;
		}
	}
	bool isBool () const
	{
		if ( std::holds_alternative<bool> ( value ) )
		{
			return true;
		} else
		{
			return false;
		}
	}	// ------------------------------- serialization
	void serialize ( BUFFER &buff, bool quoteNames = false );
};

extern jsonElement jsonParser ( char const *str );

