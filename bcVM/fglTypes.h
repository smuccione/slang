/*
	FGL data types

*/

#pragma once

#include <cassert>
#include <new>
#include <stdint.h>
#include "compilerParser/fglErrors.h"
#include <cinttypes>
#include "Utility/stringi.h"
#include <string.h>                    // for size_t, strlen
#include "type_traits"                 // for forward
#include "xstring"                     // for string

// for template metaprogramming
template <class ... > struct types
{};

enum class slangType
{
	eNULL			= 0x00,
	eLONG,
	eSTRING,
	eDOUBLE,
	eARRAY_SPARSE,
	eARRAY_ELEM,
	eCODEBLOCK,
	eREFERENCE,
	eOBJECT,
	ePCALL,
	eATOM,
	eARRAY_FIXED,
	eOBJECT_ROOT,
	eARRAY_ROOT,
	eCODEBLOCK_ROOT,
	eUNIONN_ROOT,
	eUNIONN,
	eMOVED,							// exists beyond MAXTYPE because it's NOT a valid type to exist in the OM system proper... it's special
	eDEBUG_MARKER,
};

#define eMAXTYPE		slangType::eMOVED

struct DBFIELDCACHE {
	unsigned char	cacheDat[16];
};

#pragma pack ( push, 4 )

struct vmCodeBlock {
	bool		 isStatic;
	uint32_t	 storageSize;
	uint32_t	 nParams;
	uint32_t	 nSymbols;
	uint32_t	 bssSize;
	uint32_t	 csOffset;
	uint32_t	 dsOffset;
	uint32_t	 symOffset;
};

#if 0

Object Model

<derived object>
<most - derived - self>
<ivars>
<base object 1>
<most - derived - self>
<ivars>
<base object 2>
<most - derived - self>
<ivars>
<base object 3>
<most - derived - self>
<ivars>
<virtual base object 1>
<ivars>
<base object 1>
<most - derived - self>
<ivars>
<base object 2>
<most - derived - self>
<ivars>
<virtual base object 1>
<most - derived - self>
<ivars>

each regular base object is just emitted in the order in which it's inherit appears
virtual bases's are noted, but not emitted until a second pass.   any virtual bases inheritted in a virtual base are noted but not emitted.
they are emitted as any other virtual base class
when emitting a virtual base, they are emitted as if they were their own most - derivced class.

durign execution, virtual base classes use the pushContextVirt while non - base classes use pushContext
then a dispatch occurs as with any other method call

in the vtable the objoffset entry is used to offset the self for the virtual base class.during construction a fixup list into the vtable is generated
and when a virtual base is emitted location for that virtual base vtable and ivar offset are stored
after all virtual bases are emitted, the fixup list is traversed and all fixups are done.


#endif

// object memory layout
//
// OBJECT_ROOT
// INSTANCE VARIABLES
// DIRECT (non-virtual) INHERITED CLASSES[]
// VIRTUALLY inherited classes

struct VAR;

// Read-Write guard.   This allows us to create a card table without having access to windows virtual memory 

class RW_GUARD
{
	VAR *ptr;

	public:

	operator VAR *()
	{
		return ptr;
	}

	operator VAR * () const
	{
		return ptr;
	}
	
	VAR *& operator () ()
	{
		return this->ptr;
	}

	VAR * const &operator () () const
	{
		return ptr;
	}

	VAR *operator = ( VAR *val )
	{
		this->ptr = val;
		return this->ptr;
	}

	VAR * operator -> ( void )
	{
		return ptr;
	}
	operator bool ( void ) const
	{
		return this->ptr != nullptr;
	}
	bool operator == ( VAR *value ) const
	{
		return this->ptr == value;
	}
	bool operator != ( VAR *value ) const
	{
		return this->ptr != value;
	}
	ptrdiff_t operator - ( VAR *value ) const
	{
		return (ptrdiff_t)ptr - (ptrdiff_t)value;
	}
	ptrdiff_t operator + ( VAR *value ) const
	{
		return (ptrdiff_t) ptr + (ptrdiff_t)value;
	}
};

struct VAR;

struct VAR {
	struct {
		slangType				type		:  8;
		unsigned	int			pad			: 24;
		uint32_t				packIndex;
	};

	union
	{
		int64_t					  l;
		double					  d;
		uint32_t				  atom;

		// object members come directly after the slangType::eOBJECT var
		struct
		{
			struct bcClass *classDef;
			struct bcClassVTableEntry *vPtr;
			void *cargo;
		}						  obj;

		// union members come directly after the slangType::eUNIONN var
		struct
		{
			struct bcUnion		*unionDef;
			uint32_t			 activeTag;			// 0 - non selected, 1 is FIRST tag
		}						 u;

		struct
		{
			RW_GUARD			  v;
			RW_GUARD			  obj;				// enclosing object for garbage collection;
		}						  ref;
		struct
		{
			vmCodeBlock *cb;
		}                         cb;
		struct
		{
			size_t		          len;
			char const *c;
		}                         str;
		struct
		{
			RW_GUARD			  v;				// points to an slangType::eARRAY_ELEM element
			RW_GUARD			  lastAccess;		// speed up sequential array accesses - points to last slangType::eARRAY_ELEM element accessed - cheap global iterator
			int64_t				  maxI;				// maximum indicie
		}						  aSparse;
		struct
		{
			RW_GUARD			  var;
			RW_GUARD			  next;
			int64_t				  elemNum;
		}                         aElem;
		struct
		{
			int64_t				  startIndex;
			int64_t				  endIndex;
		}                         arrayFixed;
		struct
		{
			struct	fglOp		 *op;				// the next instruction after return
			struct	bcFuncDef	 *func;				// pointer to function definition
			uint32_t			  nParams;			// number of parameter passed into function (may be more than is required)
		} pCall;
	} dat;

	VAR ( ) = default;

	VAR ( int64_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = value;
	}

	VAR ( uint64_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = (int64_t)value;
	}

	VAR ( int32_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = value;
	}

	VAR ( uint32_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = value;
	}

	VAR ( int16_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = value;
	}

	VAR ( uint16_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = value;
	}

#if 0
	VAR ( int8_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = static_cast<int64_t>(value);
	}

	VAR ( uint8_t const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.l = value;
	}
#endif
	VAR ( double const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.d = value;
	}

	VAR ( float const value )		// NOLINT ( c26495 )
	{
		type = slangType::eLONG;
		dat.d = value;
	}

	int64_t operator = ( int64_t const value )
	{
		type = slangType::eLONG;
		dat.l = value;
		return value;
	}
	int64_t operator = ( int32_t const value )
	{
		type = slangType::eLONG;
		dat.l = value;
		return value;
	}
	int64_t operator = ( int16_t const value )
	{
		type = slangType::eLONG;
		dat.l = value;
		return value;
	}
	int64_t operator = ( uint16_t const value )
	{
		type = slangType::eLONG;
		dat.l = value;
		return value;
	}
	int64_t operator = ( int8_t const value )
	{
		type = slangType::eLONG;
		dat.l = value; // NOLINT(bugprone-signed-char-misuse)
		return value;
	}
	int64_t operator = ( uint8_t const value )
	{
		type = slangType::eLONG;
		dat.l = value;
		return value;
	}
	double operator = ( double const value )
	{
		type = slangType::eDOUBLE;
		dat.d = value;
		return value;
	}
	double operator = ( float const value )
	{
		type = slangType::eDOUBLE;
		dat.d = value;
		return value;
	}
	char const *operator = ( char const *value )
	{
		assert ( value );
		type = slangType::eSTRING;
		dat.str.c = const_cast<char *>(value);
		dat.str.len = strlen ( value );
		return value;
	}

	VAR *operator [] ( int64_t index );

	// dispatches against a function object or other function type.   this is NOT used to call methods.
	template<class ... Args>
	VAR *operator() ( class vmInstance *instance, Args && ...args );

	VAR *operator() ( uint32_t nParams, class vmInstance *instance );

	inline bool isTrue()
	{
		auto varTmp = this;
		while ( varTmp->type == slangType::eREFERENCE ) varTmp = static_cast<VAR *>(varTmp->dat.ref.v);
		switch ( varTmp->type )
		{
			case slangType::eLONG:
				if ( !varTmp->dat.l ) return false;
				break;
			case slangType::eDOUBLE:
				if ( !(bool) varTmp->dat.d ) return false;
				break;
			case slangType::eSTRING:
				if ( !varTmp->dat.str.len ) return false;
				break;
			case slangType::eNULL:
				return false;
			case slangType::eARRAY_ROOT:
				switch ( varTmp->dat.ref.v->type )
				{
					case slangType::eARRAY_FIXED:
						if ( !(varTmp->dat.ref.v->dat.arrayFixed.endIndex >= varTmp->dat.ref.v->dat.arrayFixed.startIndex) ) return false;
						break;
					case slangType::eARRAY_SPARSE:
						if ( !varTmp->dat.ref.v->dat.aSparse.v ) return false;
						break;
					default:
						break;
				}
				break;
			case slangType::eOBJECT_ROOT:
				if ( !varTmp->dat.ref.v ) return false;
				break;
			default:
				break;
		}
		return true;
	}

	void touch( )
	{
		pad = 0;
		switch ( type )
		{
			case slangType::eOBJECT_ROOT:
			case slangType::eARRAY_ROOT:
			case slangType::eREFERENCE:
				dat.ref.v->touch ( );
				break;
			default:
				break;
		}
	}

private:
	template<class size, class Head, class ... Tail>
	void makeParams ( class vmInstance *instance, size pCount, types< Head, Tail ...>, Head &&head, Tail && ...args );

	template<class size, class Head >
	void makeParams ( class vmInstance *instance, size pCount, types< Head>, Head &&head );

	template<class size>
	void makeParams ( class vmInstance *instance, size pCount, types<> );
 };

// used to both generate a unique type, and to aid in construction
struct VAR_STR : public VAR
{
	VAR_STR ( VAR const &old )
	{
		if ( old.type != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;
		*this = *((VAR_STR *) &old);
	}
	VAR_STR()
	{
		type = slangType::eSTRING;
		dat.str.len = 0;
		dat.str.c = "";
	}

	VAR_STR( char const *str )
	{
		assert ( str );
		type = slangType::eSTRING;
		dat.str.c = const_cast<char *>(str);
		dat.str.len = strlen( str );
	}
	VAR_STR ( char const *str, size_t len )
	{
		assert ( str );
		type = slangType::eSTRING;
		dat.str.c = const_cast<char *>(str);
		dat.str.len = len;
	}
	VAR_STR ( class vmInstance *instance, char const *str );
	VAR_STR ( class vmInstance *instance, char *str );
	VAR_STR ( class vmInstance *instance, char const *str, size_t len );
	VAR_STR ( class vmInstance *instance, std::string const &str );
	VAR_STR ( class vmInstance *instance, stringi const &str );
	VAR_STR ( class vmInstance *instance, struct BUFFER &buff );

	operator char const* () const
	{
		return dat.str.c;
	}
};

struct VAR_OBJ : public VAR
{
	// creation ensures that we're an OBJECT_ROOT
	VAR_OBJ ( VAR *var );
	VAR_OBJ ( VAR &var );
	VAR_OBJ ( VAR *root, VAR *var );
	VAR_OBJ () 
	{
		type = slangType::eOBJECT_ROOT;
		dat.ref.v = nullptr;
		dat.ref.obj = nullptr;
	};

	operator VAR *()
	{
		return static_cast<VAR *>(this);
	}

	void *getCargo ()
	{
		if ( dat.ref.v->dat.obj.cargo )
		{
			return (uint8_t *) dat.ref.v->dat.obj.cargo + 16;
		} else
		{
			return 0;
		}
	}

	VAR *operator [] ( char const *name );

	size_t getCargoSize ()
	{
		if ( dat.ref.v->dat.obj.cargo )
		{
			return *(size_t *) dat.ref.v->dat.obj.cargo;
		} else
		{
			return 0;
		}
	}

	template <typename C >
	C *makeObj ( class vmInstance *instance  )
	{
		auto *ret = ::new ((C *) allocCargo ( instance, sizeof ( C ) )) C ( );
		return ret;
	}
	template <typename C, typename ...Params>
	C *makeObj ( class vmInstance *instance, Params &&...params )
	{
		auto *ret = ::new ((C *)allocCargo ( instance, sizeof ( C ) )) C ( std::forward<Params> (params)... );
		return ret;
	}

	template <typename C >
	C *makeObj ( size_t age, class vmInstance *instance )
	{
		auto *ret = ::new ((C *) allocCargo ( age, instance, sizeof ( C ) )) C ( );
		return ret;
	}

	template <typename C, typename ...Params>
	C *makeObj ( size_t age, class vmInstance *instance, Params &&...params )
	{
		auto *ret = ::new ((C *) allocCargo ( age, instance, sizeof ( C ) )) C ( std::forward<Params> ( params )... );
		return ret;
	}

	template<typename C>
	C *getObj ( )
	{
		assert ( getCargoSize ( ) == sizeof ( C ));
		return (C *) getCargo ( );
	}

private:
	void *allocCargo ( class vmInstance *instance, size_t len );
	void *allocCargo ( size_t age, class vmInstance *instance, size_t len );
};

struct VAR_REF : public VAR
{
	VAR_REF ( VAR *obj, VAR *v )
	{
		type = slangType::eREFERENCE;
		dat.ref.v = v;
		dat.ref.obj = v;
	}
	operator VAR * ()
	{
		return static_cast<VAR *>(this);
	}
};

struct VAR_ARRAY : public VAR
{
	VAR_ARRAY ( class vmInstance *instance );
	
	VAR_ARRAY ( VAR *v )
	{
		type = slangType::eARRAY_ROOT;
		dat.ref.v = v;
		dat.ref.obj = v;
	}
	operator VAR * ()
	{
		return static_cast<VAR *>(this);
	}
};

struct VAR_NULL : public VAR
{
	VAR_NULL ( )
	{
		type = slangType::eNULL;
	}
	operator VAR * ()
	{
		return static_cast<VAR *>(this);
	}
};


template <size_t Length>
struct fixed_string
{
	char value[Length + 1] = {}; // +1 for null terminator
	constexpr fixed_string ( const char ( &str )[Length] )
	{
		std::copy_n ( str, Length, value );
	}
}; 

/* 
*  this particular object isn't used during execution (sort of)
*  it's purpose is to allow specificatio of an object return type during FFI (native interface) definition
*  this should be used as the RETURN TYPE of any FFI function which wish's to return a known object type
*  this lets the optimizer understand the type and do further optimizations on it
*/
template <fixed_string name = "">
class VAR_OBJ_TYPE : public VAR_OBJ
{

public:
	static char const *getClass () {
		return name.value;
	}

	VAR_OBJ_TYPE ( VAR_OBJ const &obj ) : VAR_OBJ ( obj )
	{
	}
	template<typename T>
	operator T &() const {
		return *static_cast<T *>(this);
	}
	template<typename T>
	operator T &() {
		return *static_cast<T *>(this);
	}
};

#pragma pack ( pop )

enum class RES_MODE {
	RES_END		= 1,
	RES_REINIT	= 2
};
