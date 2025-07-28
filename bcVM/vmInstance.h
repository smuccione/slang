/*
	instance structure for vm execution

*/

#pragma once

#include "Utility/settings.h"

#include <stack>
#include <map>
#include <utility>
#include <functional>
#include <tuple>
#include <source_location>

#include <io.h>

#include "bcInterpreter/bcInterpreter.h"
#include "bcVMObject.h"
#include "exeStore.h"
#include "fglTypes.h"
#include "vmAtom.h"
#include "vmObjectMemory.h"
#include "vmWorkarea.h"
#include "vmConversion.h"
#include "Target/output.h"

#ifdef ERROR
#undef ERROR
#endif

#define LOG_LEVELS	\
	LVL ( INFO )		\
	LVL ( WARN )		\
	LVL ( ERROR )

#define LOG_MODULES	\
	MOD ( SYSTEM )	\
	MOD ( VM )   \
	MOD ( PREPROCESSOR )   \
	MOD ( PARSER )   \
	MOD ( OPTIMIZER )   \
	MOD ( CODE_GENERATOR )   \
	MOD ( FFI )   \
	MOD ( WEBSERVER )   \
	MOD ( DATABASE )   \
	MOD ( LANGUAGE_SERVER )   \
	MOD ( DEBUG_ADAPTER )

namespace logger
{
	enum class level
	{
#undef LVL
#define LVL(x) x,
		LOG_LEVELS
	};

	inline char const *lvlNames[] = {
#undef LVL
#define LVL(x) #x,
		LOG_LEVELS
	};

	enum class modules
	{
#undef MOD
#define MOD(x) x,
		LOG_MODULES
	};

	inline char const *modNames[] = {
#undef MOD
#define MOD(x) #x,
		LOG_MODULES
	};

	struct formatLocation
	{
		char const *fmt;
		stringi						 tmpString;
		std::source_location const &loc;
		errorNum					 err = errorNum::ERROR_OK;

		formatLocation( char const *fmt, std::source_location const &loc = std::source_location::current() ) : fmt( fmt ), loc( loc )
		{}
		formatLocation( errorNum err, std::source_location const &loc = std::source_location::current() ) : loc( loc )
		{
			tmpString = scCompErrorAsText( size_t( err ) );
			fmt = tmpString.c_str();
		}
		formatLocation( warnNum warn, std::source_location const &loc = std::source_location::current() ) : loc( loc )
		{
			tmpString = scCompErrorAsText( size_t( warn ) );
			fmt = tmpString.c_str();
		}
	};
}

constexpr size_t TASK_EVAL_STACK_SIZE	= 65536;
constexpr size_t MAX_ATOMS = (65536 * 4);
constexpr size_t MAX_PROFILE_DEPTH = 1024;
constexpr size_t CPU_STACK_SIZE = (1024 * 1024 * 64);

struct taskResource
{
	RES_MODE			  mode;
	void *resource;
	void				(*free) (void *resource);
	struct taskResource *next;
};

struct errorLocation
{
	enum class exceptionBreakType
	{
		never,
		always,
		unhandled,
		handled
	};

	bool							 valid = false;;
	bcFuncDef *funcDef = nullptr;
	VAR *stack = nullptr;			/* pointer to stack */
	ptrdiff_t						 opCode = 0;				/* opcode throwing error, we can look it up if debugging information is present to extract line information */
	size_t							 errorNum = 0;
	exceptionBreakType				 breakMode;
};

class vmCoroutine
{
	public:
	enum class vmCoroutineStatus
	{
		vmCoroutine_init,
		vmCoroutine_running,
		vmCoroutine_complete
	};

	vmCoroutineStatus	status;
	fglOp *ip;
	public:
	virtual	~vmCoroutine() {};

	virtual	void	 yield( VAR *param, uint32_t nParams )
	{
		throw errorNum::scUNSUPPORTED;
	}
	virtual	vmCoroutineStatus getStatus( void )
	{
		throw errorNum::scUNSUPPORTED;
	}
	virtual VAR *getStack( VAR **topOfStack )
	{
		*topOfStack = 0;
		return 0;
	}
	virtual VAR *getRoot( void )
	{
		throw errorNum::scUNSUPPORTED;
	}
	virtual fglOp *getReturnIP( void )
	{
		return ip;
	}
	virtual void	 setReturnIP( fglOp *op )
	{
		ip = op;
	}
};

template<typename T>
constexpr bool dependent_false = false;

class vmInstance
{
	public:
	template<typename T, int = std::is_base_of<VAR_ARRAY, T>::value * 4 + std::is_integral<T>::value * 2 + std::is_floating_point<T>::value * 1>
	struct varConvert;
	template< typename T>
	struct varConvert <T, 0>
	{
		static inline T convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			static_assert(dependent_false<T>, "invalid type");
		}
	};

	template<typename T>
	struct varConvert <T, 1>
	{
		static inline T convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
			switch ( val->type )
			{
				case slangType::eLONG:
					return (T) val->dat.l;
				case slangType::eDOUBLE:
					return (T) val->dat.d;
				case slangType::eSTRING:
					return (T) atof( val->dat.str.c );
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<typename T>
	struct varConvert <T, 2>
	{
		static inline T convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
			switch ( val->type )
			{
				case slangType::eLONG:
					return (T) val->dat.l;
				case slangType::eDOUBLE:
					return (T) val->dat.d;
				case slangType::eSTRING:
					return (T) _atoi64( val->dat.str.c );
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<typename T>
	struct varConvert <T, 4>
	{
		static inline T convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			return *static_cast<VAR_ARRAY *>(val);
		}
	};


	template<>
	struct varConvert <bool>
	{
		static inline bool convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					return (bool) (val->dat.l ? true : false);
				case slangType::eDOUBLE:
					return (bool) (val->dat.d != 0 ? true : false);
				case slangType::eSTRING:
					return (bool) (_atoi64( val->dat.str.c ) ? true : false);
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <char const *>
	{
		static inline char const *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			int32_t dec;
			int32_t sgn;

			(*pVal)--;
			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					_i64toa_s( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
					return tmpStr;
				case slangType::eDOUBLE:
					_ecvt_s( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
					return tmpStr;
				case slangType::eSTRING:
					return val->dat.str.c;
				case slangType::eNULL:
					return "";
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <char *>
	{
		static inline char *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			int32_t dec;
			int32_t sgn;

			(*pVal)--;
			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					_i64toa_s( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
					return tmpStr;
				case slangType::eDOUBLE:
					_ecvt_s( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
					return tmpStr;
				case slangType::eSTRING:
					return instance->om->strDup( val->dat.str.c );
				case slangType::eNULL:
					return instance->om->strDup( "" );
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <std::string>
	{
		static inline std::string convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			int32_t dec;
			int32_t sgn;

			(*pVal)--;
			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					_i64toa_s( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
					return tmpStr;
				case slangType::eDOUBLE:
					_ecvt_s( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
					return tmpStr;
				case slangType::eSTRING:
					return val->dat.str.c;
				case slangType::eNULL:
					return "";
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <stringi>
	{
		static inline stringi convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			int32_t dec;
			int32_t sgn;

			(*pVal)--;
			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					_i64toa_s( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
					return tmpStr;
				case slangType::eDOUBLE:
					_ecvt_s( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
					return tmpStr;
				case slangType::eSTRING:
					return val->dat.str.c;
				case slangType::eNULL:
					return "";
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <VAR_OBJ *>
	{
		static inline VAR_OBJ *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			new (val) VAR_OBJ( val );
			return reinterpret_cast<VAR_OBJ *>(val);
		}
	};

	template<>
	struct varConvert <VAR_OBJ const *>
	{
		static inline VAR_OBJ const *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			new (val) VAR_OBJ( val );
			return reinterpret_cast<VAR_OBJ *>(val);
		}
	};

	template<>
	struct varConvert <VAR_ARRAY *>
	{
		static inline VAR_ARRAY *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			new (val) VAR_ARRAY( val );
			return reinterpret_cast<VAR_ARRAY *>(val);
		}
	};

	template<>
	struct varConvert <VAR_ARRAY const *>
	{
		static inline VAR_ARRAY const *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			new (val) VAR_ARRAY( val );
			return reinterpret_cast<VAR_ARRAY *>(val);
		}
	};

	template<>
	struct varConvert <VAR_REF *>
	{
		static inline VAR_REF *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			(*nParams)--;
			(*pVal)--;
			return reinterpret_cast<VAR_REF *>(*pVal + 1);
		}
	};

	template<>
	struct varConvert <VAR_REF const *>
	{
		static inline VAR_REF const *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			(*nParams)--;
			(*pVal)--;
			return reinterpret_cast<VAR_REF *>(*pVal + 1);
		}
	};

	template<>
	struct varConvert <VAR_STR>
	{
		static inline VAR_STR convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;
			int32_t dec;
			int32_t sgn;

			VAR_STR retVal;

			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					_i64toa_s( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
					retVal.type = slangType::eSTRING;
					retVal.dat.str.c = tmpStr;
					retVal.dat.str.len = strlen( tmpStr );
					return retVal;
				case slangType::eDOUBLE:
					_ecvt_s( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
					retVal.type = slangType::eSTRING;
					retVal.dat.str.c = tmpStr;
					retVal.dat.str.len = strlen( tmpStr );
					return retVal;
				case slangType::eSTRING:
					return *static_cast<VAR_STR *>(val);
				case slangType::eNULL:
					retVal.type = slangType::eSTRING;
					retVal.dat.str.c = "";
					retVal.dat.str.len = 0;
					return *static_cast<VAR_STR *>(val);
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <VAR_STR *>
	{
		static inline VAR_STR *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *retVal = *pVal;
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;

			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					_i64toa_s( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
					retVal->type = slangType::eSTRING;
					retVal->dat.str.c = tmpStr;
					retVal->dat.str.len = strlen( tmpStr );
					return static_cast<VAR_STR *>(retVal);
				case slangType::eDOUBLE:
					{
						int32_t dec;
						int32_t sgn;

						_ecvt_s( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
					}
					retVal->type = slangType::eSTRING;
					retVal->dat.str.c = tmpStr;
					retVal->dat.str.len = strlen( tmpStr );
					return static_cast<VAR_STR *>(retVal);
				case slangType::eSTRING:
					return static_cast<VAR_STR *>(retVal);
				case slangType::eNULL:
					retVal->type = slangType::eSTRING;
					retVal->dat.str.c = "";
					retVal->dat.str.len = 0;
					return static_cast<VAR_STR *>(retVal);
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <VAR_STR const *>
	{
		static inline VAR_STR const *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *retVal = *pVal;
			VAR *val = *pVal;
			(*nParams)--;
			(*pVal)--;

			while ( val->type == slangType::eREFERENCE ) val = static_cast<VAR *>(val->dat.ref.v);
			switch ( val->type )
			{
				case slangType::eLONG:
					_i64toa_s( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
					retVal->type = slangType::eSTRING;
					retVal->dat.str.c = tmpStr;
					retVal->dat.str.len = strlen( tmpStr );
					return static_cast<VAR_STR *>(retVal);
				case slangType::eDOUBLE:
					{
						int32_t dec;
						int32_t sgn;

						_ecvt_s( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
					}
					retVal->type = slangType::eSTRING;
					retVal->dat.str.c = tmpStr;
					retVal->dat.str.len = strlen( tmpStr );
					return static_cast<VAR_STR *>(retVal);
				case slangType::eSTRING:
					return static_cast<VAR_STR *>(retVal);
				case slangType::eNULL:
					retVal->type = slangType::eSTRING;
					retVal->dat.str.c = "";
					retVal->dat.str.len = 0;
					return static_cast<VAR_STR *>(retVal);
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		}
	};

	template<>
	struct varConvert <vmInstance *>
	{
		static inline vmInstance *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			return instance;
		}
	};

	template<>
	struct varConvert <nParamType>
	{
		static inline nParamType convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			nParamType ret( *pVal, *nParams );
			return ret;
		}
	};

	template<>
	struct varConvert <nWorkarea>
	{
		static inline nWorkarea convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			nWorkarea ret = {};
			ret.value = instance->workareaTable->getData();
			return ret;
		}
	};

	template<>
	struct varConvert <void>
	{
		static inline void convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			throw errorNum::scINVALID_PARAMETER;
		}
	};

	template<>
	struct varConvert <VAR *>
	{
		static inline VAR *convert( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
		{
			VAR *val = *pVal;
			while ( val->type == slangType::eREFERENCE )
			{
				val = static_cast<VAR *>(val->dat.ref.v);
			}
			(*nParams)--;
			(*pVal)--;
			return val;
		}
	};

	template<typename T, int = std::is_base_of<VAR_ARRAY, T>::value * 8 + std::is_base_of<VAR_OBJ, T>::value * 4 + std::is_integral<T>::value * 2 + std::is_floating_point<T>::value * 1>
	struct toVar;

	template<typename T>
	struct toVar <T, 0>
	{
		static inline VAR convert( vmInstance *instance, T value )
		{
			static_assert (dependent_false<T>, "unknown conversion to VAR");
		}
	};

	template<typename T>
	struct toVar <T, 1>
	{
		static inline VAR convert( vmInstance *instance, T &&value )
		{
			return VAR( double( value ) );
		}
	};

	template<typename T>
	struct toVar <T, 2>
	{
		static inline VAR convert( vmInstance *instance, T &&value )
		{
			return VAR( int64_t( value ) );
		}
	};

	template<typename T>
	struct toVar <T, 4>
	{
		static inline VAR convert( vmInstance *instance, T  &&value )
		{
			auto ret = static_cast<VAR const *>(& value);
			return *ret;
		}
	};

	template<typename T>
	struct toVar <T, 8>
	{
		static inline VAR convert( vmInstance *instance, T const &&value )
		{
			auto ret = static_cast<VAR const *>(& value);
			return *ret;
		}
	};

	template<>
	struct toVar <char const *>
	{
		static inline VAR convert( vmInstance *instance, char const *value )
		{
			assert( value );
			return VAR_STR( const_cast<char *>(value), strlen( value ) );
		}
	};

	template<>
	struct toVar <char *>
	{
		static inline VAR convert( vmInstance *instance, char *value )
		{
			assert( value );
			return VAR_STR( const_cast<char *>(value), strlen( value ) );
		}
	};

	template<>
	struct toVar <std::string>
	{
		static inline VAR convert( vmInstance *instance, std::string const &&value )
		{
			return VAR_STR( const_cast<char *>(value.c_str()), value.size() );
		}
	};

	template<>
	struct toVar <stringi>
	{
		static inline VAR convert( vmInstance *instance, stringi &&value )
		{
			return VAR_STR( const_cast<char *>(value.c_str()), value.size() );
		}
	};

	template<>
	struct toVar <VAR *>
	{
		static inline VAR convert( vmInstance *instance, VAR *value )
		{
			while ( value->type == slangType::eREFERENCE ) value = static_cast<VAR *>(value->dat.ref.v);
			return *value;
		}
	};

	template<>
	struct toVar <VAR>
	{
		static inline VAR convert( vmInstance *instance, VAR const &&value )
		{
			auto ret = &value;
			while ( ret->type == slangType::eREFERENCE ) ret = static_cast<VAR *>(ret->dat.ref.v);
			return *ret;
		}
	};

	template<>
	struct toVar <VAR_STR>
	{
		static inline VAR convert( vmInstance *instance, VAR_STR const &&value )
		{
			auto ret = (VAR *) &value;
			while ( ret->type == slangType::eREFERENCE ) ret = static_cast<VAR *>(ret->dat.ref.v);
			return *ret;
		}
	};

	template<>
	struct toVar <VAR_STR *>
	{
		static inline VAR convert( vmInstance *instance, VAR_STR *value )
		{
			auto ret = (VAR *) value;
			while ( ret->type == slangType::eREFERENCE )  ret = static_cast<VAR *>(ret->dat.ref.v);
			return *ret;
		}
	};

	template<>
	struct toVar <VAR_REF>
	{
		static inline VAR convert( vmInstance *instance, VAR_REF const &&value )
		{
			return value;
		}
	};

	template<>
	struct toVar <VAR_REF *>
	{
		static inline VAR convert( vmInstance *instance, VAR_REF *value )
		{
			return *value;
		}
	};

	template<>
	struct toVar <VAR_OBJ>
	{
		static inline VAR convert( vmInstance *instance, VAR_OBJ const &&value )
		{
			return value;
		}
	};

	template<>
	struct toVar <VAR_OBJ *>
	{
		static inline VAR convert( vmInstance *instance, VAR_OBJ *value )
		{
			return *value;
		}
	};

	template<>
	struct toVar <VAR_ARRAY>
	{
		static inline VAR convert( vmInstance *instance, VAR_ARRAY const &&value )
		{
			return value;
		}
	};

	template<>
	struct toVar <VAR_ARRAY *>
	{
		static inline VAR convert( vmInstance *instance, VAR_ARRAY *value )
		{
			return *value;
		}
	};

	VAR *eval = nullptr;
	VAR *stack = nullptr;
	size_t						  nStack;

	VAR							  result;

	int							  outputDesc;

	class atomTable				 *atomTable = nullptr;
	class objectMemory			 *om = nullptr;
	class vmWorkareaTable		 *workareaTable = nullptr;

	bool						  initialized = false;;

	taskResource				 *firstResource = nullptr;

	/* debugger stuff */
	std::shared_ptr<class	vmDebug>	debug;

	errorLocation				  error;						/* error location */

	/* profile stuff */
	time_t						  executionStartTime;
	time_t						  maxExecutionTime;
	int							  execStack[MAX_PROFILE_DEPTH];		/* call stack of executing functions for profiling */
	int							  nProfile;

	vmInstance( char const *name = "(SYSTEM)" );
	virtual ~vmInstance();
	virtual void				  init( void );
	virtual void				  allocateAtomTable( void );

	virtual void				  release( void );
	virtual void				  duplicate( class vmInstance *instance );

	virtual void				  allocateEvalStack( size_t numEntries );
	virtual void				  freeEvalStack( void );
	virtual char const			 *getName( void ) = 0;
	virtual void				  setName( char const *name ) = 0;

	virtual	void				  reset( void );

	int resourceRegister( void *resource, void (*proc) (void *resource), RES_MODE mode )
	{
		taskResource *res;

		if ( !(res = (taskResource *) malloc( sizeof( taskResource ) * 1 )) )
		{
			return (0);
		}
		res->resource = resource;
		res->free = proc;
		res->next = firstResource;
		res->mode = mode;

		firstResource = res;

		return (1);
	}

	void resourceFree( void *resource, void (*proc) (void *resource) )
	{
		auto res = firstResource;
		decltype(res) prev = nullptr;

		while ( res )
		{
			if ( (res->resource == resource) && (res->free == proc) )
			{
				if ( prev )
				{
					prev->next = res->next;
				} else
				{
					firstResource = res->next;
				}
				free( res );
				return;
			}
			prev = res;
			res = res->next;
		}
	}

	void _resourceEndAll()
	{
		for ( auto res = firstResource; res; )
		{
			(res->free) (res->resource);

			auto next = res->next;
			free( res );
			res = next;
		}
		firstResource = nullptr;
	}

	void _resourceReset()
	{
		auto res = firstResource;
		decltype(res) prev = nullptr;

		while ( res )
		{
			auto next = res->next;
			if ( res->mode == RES_MODE::RES_REINIT )
			{
				(res->free) (res->resource);
				if ( prev )
				{
					prev->next = res->next;
				} else
				{
					firstResource = res->next;
				}
				free( res );
			}
			res = next;
		}
	}

	virtual void *malloc( size_t len )
	{
		return ::malloc( len );
	}
	virtual void				 free( void *ptr )
	{
		::free( ptr );
	}
	virtual void *calloc( size_t count, size_t size )
	{
		void *r;
		r = malloc( count * size );
		memset( r, 0, count * size );
		return r;
	}
	virtual char *_strdup( char const *str )
	{
		const size_t len = strlen( str ) + 1;
		char *ret = (char *) malloc( len );
		memcpy( ret, str, len );
		return ret;
	}
	virtual void				  validateHeap( void ) {};

	vmCoroutine *currentCoroutine;
	vmCoroutine *mainCoroutine;
	std::list<vmCoroutine *>	  coRoutines;				// for garbage collection... we need to be able to enumerate all the coroutines to collect their stacks
	std::stack<vmCoroutine *>	  coRoutineStack;

	virtual vmCoroutine *createCoroutine( VAR *func, size_t evalStackSize = 8192, size_t cStackSize = 128ull * 256 )
	{
		throw errorNum::scUNSUPPORTED;
	}

	// load code into the vm
	virtual bool	 load( std::shared_ptr<uint8_t> &buff, char const *name, bool isPersistant, bool isLibrary );

	// start executing the code at function "main".   This also does all the preliminaries such as initializing globals, etc.
	virtual void	 run( char const *name, bool halt = false );

	// retrieve a ponter to a global variable
	virtual VAR *getGlobal( char const *nSpace, char const *name ) const;
	virtual std::tuple<VAR *, bcLoadImage *, uint32_t> findGlobal( char const *name, bcLoadImage *ignoreImage ) const;							// searches all load images

	// print the call stack to the console
	virtual void	 printCallStack( VAR *stack, fglOp *op );

	template<typename T>
	void getSourcePaths( const uint8_t *buff, T t )
	{
		fgxFileHeader *header = (fgxFileHeader *) buff;

		auto paths = (fgxName *) (buff + header->offsetBuildDirectories);

		for ( size_t loop = 0; loop < header->numBuildDirectories; loop++ )
		{
			t( paths[loop].name );
		}

	}

	virtual	 uint64_t	getTicks( void )
	{
		return 0;
	}

	fglOp *catchHandler( bcFuncDef *funcDef, fglOp *ops, VAR *basePtr, int64_t waSave, VAR const &exception );

	// call byte code function within vm context
	void		 interpretBC( bcFuncDef *funcDef, fglOp *retAddr, uint32_t nParams );
	VAR *interpretBCParam( bcFuncDef *funcDef, bcFuncDef *destFunc, fglOp *ops, uint32_t nParams, VAR *stack, VAR *basePtr );

	// call a native function within vm context
	void		 funcCall( bcFuncDef *bcFunc, uint32_t nParams );
	void		 funcCall( uint32_t atomName, uint32_t nParams );

	template <typename Head>
	void _stackPush( types<Head>, Head &&head )
	{
		*(stack++) = toVar<Head>::convert( this, head );
	}

	template <typename Head, typename ...Tail>
	void _stackPush( types<Head, Tail...>, Head &&head, Tail &&...tail )
	{
		_stackPush( types<Tail...>{}, std::forward<Tail>( tail )... );
		*(stack++) = toVar<Head>::convert( this, head );
	}

	template <typename ...Params >
	VAR objNew( char const *name, Params  &&... params )
	{
		// push the parameters on the stack in right to left order
		_stackPush( types<Params...>{}, std::forward<Params>( params )... );
		// call classNew
		classNew( this, name, sizeof...(Params) );
		// fix up the statck
		stack -= sizeof... (Params);
		return result;
	}

	template <>
	VAR objNew( char const *name )
	{
		// call classNew
		classNew( this, name, 0 );
		// fix up the statck
		return result;
	}

	template <typename ...Params >
	VAR call( char const *name, Params  &&... params )
	{
		// push the parameters on the stack in right to left order
		_stackPush( types<Params...>{}, std::forward<Params>( params )... );

		auto func = atomTable->atomGetFunc( name );
		switch ( func->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				interpretBC( func, nullptr, sizeof... (Params) );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				funcCall( func, sizeof... (Params) );
				break;
			default:
				throw errorNum::scINTERNAL;
		}
		// fix up the statck
		stack -= sizeof... (Params);
		return result;
	}

	template <>
	VAR call( char const *name )
	{
		auto func = atomTable->atomGetFunc( name );
		switch ( func->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				interpretBC( func, nullptr, 0 );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				funcCall( func, 0 );
				break;
			default:
				throw errorNum::scINTERNAL;
		}
		// fix up the statck
		return result;
	}

	template<typename ...P>
	void log( logger::level level, logger::modules mod, logger::formatLocation const &fmtLoc, P&& ...p )
	{
		char	logString[4096]{};
		size_t	len = 0;
#if 0
		switch ( level )
		{
			case logger::level::ERROR:
				len += (size_t)sprintf_s( logString + len, sizeof( logString ) - len - 1, "error: " );
				break;
			case logger::level::WARN:
				len += (size_t) sprintf_s( logString + len, sizeof( logString ) - len - 1, "warning: " );
				break;
			case logger::level::INFO:
				len += (size_t) sprintf_s( logString + len, sizeof( logString ) - len - 1, "info: " );
				break;
		}

		if ( fmtLoc.loc.function_name()[0] )
		{
			len += (size_t) sprintf_s( logString + len, sizeof( logString ) - len - 1, "%s:%s(%lu): ", fmtLoc.loc.file_name(), fmtLoc.loc.function_name(), fmtLoc.loc.line() );
		} else
		{
			len += (size_t) sprintf_s( logString + len, sizeof( logString ) - len - 1, "%s(%lu): ", fmtLoc.loc.file_name(), fmtLoc.loc.line() );
		}
#endif
		len += sprintf_s( logString + len, sizeof( logString ) - len - 1, fmtLoc.fmt, std::forward<P>( p ) ... );
		logString[len++] = '\n';
		logString[len] = 0;
		if ( outputDesc )
		{
			_write( outputDesc, logString, uint32_t( len ) );
		}
		fprintf( stdout, "%.*s", int( len ), logString );
	}
};
#if 0
template<typename ...P>
void log( logger::level level, logger::modules mod, logger::formatLocation const &fmtLoc, P&& ...p )
{
	char	logString[4096];
	size_t	len = 0;

	switch ( level )
	{
		case logger::level::ERROR:
			len += sprintf_s( logString, sizeof( logString ) - len, "error: " );
			break;
		case logger::level::WARN:
			len += sprintf_s( logString, sizeof( logString ) - len, "warning: " );
			break;
		case logger::level::INFO:
			len += sprintf_s( logString, sizeof( logString ) - len, "info: " );
			break;
	}

	if ( fmtLoc.loc.function_name()[0] )
	{
		len += sprintf_s( logString, sizeof( logString ) - len, "%s:%s(%lu): ", fmtLoc.loc.file_name(), fmtLoc.loc.function_name(), fmtLoc.loc.line() );
	} else
	{
		len += sprintf_s( logString, sizeof( logString ) - len, fmtLoc.loc.file_name(), fmtLoc.loc.line() );
	}
	len += sprintf_s( logString + len, sizeof( logString ) - len, fmtLoc.fmt, std::forward<P>( p ) ... );

	fprintf( stdout, "%.*s", (int) len, logString );
}
#endif