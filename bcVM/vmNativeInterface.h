/*
instance structure for vm execution

*/

#pragma once

#include "math.h"
#include <stack>
#include <map>
#include <utility>
#include <functional>
#include <filesystem>

#include "bcVM/fglTypes.h"
#include "bcVM/vmWorkarea.h"
#include "bcVM/vmObjectMemory.h"
#include "bcVM/vmDebug.h"
#include "bcVM/exeStore.h"
#include "bcVM/vmAtom.h"
#include "compilerParser/funcParser.h"
#include "vmInstance.h"
#include "Utility/stringi.h"

#define MAX_CONST_CHAR 200

#define MIN(a,b) (a)<(b)?(a):(b)

#undef _P
#define _P(s)\
getChr(s,  0),getChr(s,  1),getChr(s,  2),getChr(s,  3),getChr(s,  4),getChr(s,  5),getChr(s,  6),getChr(s,  7),getChr(s,  8),getChr(s,  9),\
getChr(s, 10),getChr(s, 11),getChr(s, 12),getChr(s, 13),getChr(s, 14),getChr(s, 15),getChr(s, 16),getChr(s, 17),getChr(s, 18),getChr(s, 19),\
getChr(s, 20),getChr(s, 21),getChr(s, 22),getChr(s, 23),getChr(s, 24),getChr(s, 25),getChr(s, 26),getChr(s, 27),getChr(s, 28),getChr(s, 29),\
getChr(s, 30),getChr(s, 31),getChr(s, 32),getChr(s, 33),getChr(s, 34),getChr(s, 35),getChr(s, 36),getChr(s, 37),getChr(s, 38),getChr(s, 39),\
getChr(s, 40),getChr(s, 41),getChr(s, 42),getChr(s, 43),getChr(s, 44),getChr(s, 45),getChr(s, 46),getChr(s, 47),getChr(s, 48),getChr(s, 49),\
getChr(s, 50),getChr(s, 51),getChr(s, 52),getChr(s, 53),getChr(s, 54),getChr(s, 55),getChr(s, 56),getChr(s, 57),getChr(s, 58),getChr(s, 59),\
getChr(s, 60),getChr(s, 61),getChr(s, 62),getChr(s, 63),getChr(s, 64),getChr(s, 65),getChr(s, 66),getChr(s, 67),getChr(s, 68),getChr(s, 69),\
getChr(s, 70),getChr(s, 71),getChr(s, 72),getChr(s, 73),getChr(s, 74),getChr(s, 75),getChr(s, 76),getChr(s, 77),getChr(s, 78),getChr(s, 79),\
getChr(s, 80),getChr(s, 81),getChr(s, 82),getChr(s, 83),getChr(s, 84),getChr(s, 85),getChr(s, 86),getChr(s, 87),getChr(s, 88),getChr(s, 89),\
getChr(s, 90),getChr(s, 91),getChr(s, 92),getChr(s, 93),getChr(s, 94),getChr(s, 95),getChr(s, 96),getChr(s, 97),getChr(s, 98),getChr(s, 99),\
getChr(s,100),getChr(s,101),getChr(s,102),getChr(s,103),getChr(s,104),getChr(s,105),getChr(s,106),getChr(s,107),getChr(s,108),getChr(s,109),\
getChr(s,110),getChr(s,111),getChr(s,112),getChr(s,113),getChr(s,114),getChr(s,115),getChr(s,116),getChr(s,117),getChr(s,118),getChr(s,119),\
getChr(s,120),getChr(s,121),getChr(s,122),getChr(s,123),getChr(s,124),getChr(s,125),getChr(s,126),getChr(s,127),getChr(s,128),getChr(s,129),\
getChr(s,130),getChr(s,131),getChr(s,132),getChr(s,133),getChr(s,134),getChr(s,135),getChr(s,136),getChr(s,137),getChr(s,138),getChr(s,139),\
getChr(s,140),getChr(s,141),getChr(s,142),getChr(s,143),getChr(s,144),getChr(s,145),getChr(s,146),getChr(s,147),getChr(s,148),getChr(s,149),\
getChr(s,150),getChr(s,151),getChr(s,152),getChr(s,153),getChr(s,154),getChr(s,155),getChr(s,156),getChr(s,157),getChr(s,158),getChr(s,159),\
getChr(s,160),getChr(s,161),getChr(s,162),getChr(s,163),getChr(s,164),getChr(s,165),getChr(s,166),getChr(s,167),getChr(s,168),getChr(s,169),\
getChr(s,170),getChr(s,171),getChr(s,172),getChr(s,173),getChr(s,174),getChr(s,175),getChr(s,176),getChr(s,177),getChr(s,178),getChr(s,179),\
getChr(s,180),getChr(s,181),getChr(s,182),getChr(s,183),getChr(s,184),getChr(s,185),getChr(s,186),getChr(s,187),getChr(s,188),getChr(s,189),\
getChr(s,190),getChr(s,191),getChr(s,192),getChr(s,193),getChr(s,194),getChr(s,195),getChr(s,196),getChr(s,197),getChr(s,198),getChr(s,199)

#define getChr(name, ii) ((MIN(ii,MAX_CONST_CHAR))<sizeof(name)/sizeof(*name)?name[ii]:0)

template <char... Chars_>
class VAR_OBJ_TYPE : public VAR_OBJ {

public:
	static char const *getClass(){
		static std::vector<char> vec = {Chars_...};
		static std::string str (vec.cbegin(),vec.cend());
		return str.c_str ();
	}

	VAR_OBJ_TYPE ( VAR_OBJ const &obj ) : VAR_OBJ ( obj )
	{
	}
	template<typename T>
	operator T&() const {
		return *static_cast<T*>(this);
	}
	template<typename T>
	operator T&(){
		return *static_cast<T*>(this);
	}
};

template <>
class VAR_OBJ_TYPE<> : public VAR_OBJ {

public:
	static char const *getClass()
	{
		return "";
	}

	VAR_OBJ_TYPE ( VAR_OBJ const &obj ) : VAR_OBJ ( obj )
	{
	}
	template<typename T>
	operator T&() const {
		return *static_cast<T*>(this);
	}
	template<typename T>
	operator T&(){
		return *static_cast<T*>(this);
	}
};


template<typename T, int = std::is_base_of<VAR_ARRAY,T>::value * 8 + std::is_base_of<VAR_OBJ,T>::value * 4 + std::is_integral<T>::value * 2 + std::is_floating_point<T>::value * 1>
struct getPType;

template<typename T>
struct getPType <T, 0>
{
	static inline bcFuncPType toPType ()
	{
		static_assert(false, "invalid type");
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<typename T>
struct getPType <T, 1>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Double;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<typename T>
struct getPType <T, 2>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Long_Int;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<typename T>
struct getPType <T, 4>
{
	static inline bcFuncPType toPType ( void )
	{
		return bcFuncPType::bcFuncPType_Object;
	}
	static inline char const *getClass()
	{
		return T::getClass();
	}
};

template<typename T>
struct getPType <T, 8>
{
	static inline bcFuncPType toPType ( void )
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return T::getClass();
	}
};

template<>
struct getPType <void >
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_NULL;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <char *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <char const *> 
{
	static inline bcFuncPType toPType()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <std::string> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <std::string &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <std::string const &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi const>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi const &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR> 
{
	static inline bcFuncPType toPType()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_REF> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_REF *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_REF const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_STR> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_STR *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_STR const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_OBJ> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_OBJ *> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_OBJ const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_ARRAY> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_ARRAY *> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_ARRAY const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <vmInstance *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Instance;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <nParamType> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_NParams;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <nWorkarea> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Workarea;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<typename T, int = std::is_base_of<VAR_ARRAY,T>::value * 4 + std::is_integral<T>::value * 2 + std::is_floating_point<T>::value * 1>
struct varConvert;

template< typename T>
struct varConvert <T, 0>
{
	static inline T convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		static_assert(false, "invalid type");
	}
};

template<typename T>
struct varConvert <T, 1>
{
	static inline T convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
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
				return (T) atof ( val->dat.str.c );
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}
};

template<typename T>
struct varConvert <T, 2>
{
	static inline T convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
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
				return (T) _atoi64 ( val->dat.str.c );
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}
};

template<typename T>
struct varConvert <T, 4>
{
	static inline T convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		return *static_cast<VAR_ARRAY *>(val);
	}
};


template<>
struct varConvert <bool> 
{
	static inline bool convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;
		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				return (bool) (val->dat.l ? 1 : 0);
			case slangType::eDOUBLE:
				return (bool) (val->dat.d ? 1 : 0);
			case slangType::eSTRING:
				return (bool) (_atoi64 ( val->dat.str.c ) ? 1 : 0);
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}
};

template<>
struct varConvert <char const *>
{
	static inline char const *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		int32_t dec;
		int32_t sgn;

		(*pVal)--;
		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				_i64toa_s ( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
				return tmpStr;
			case slangType::eDOUBLE:
				_ecvt_s ( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
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
	static inline char *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		int32_t dec;
		int32_t sgn;

		(*pVal)--;
		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				_i64toa_s ( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
				return tmpStr;
			case slangType::eDOUBLE:
				_ecvt_s ( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
				return tmpStr;
			case slangType::eSTRING:
				return instance->om->strDup ( val->dat.str.c );
			case slangType::eNULL:
				return instance->om->strDup ( "" );
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}
};

template<>
struct varConvert <std::string>
{
	static inline std::string convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		int32_t dec;
		int32_t sgn;

		(*pVal)--;
		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				_i64toa_s ( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
				return tmpStr;
			case slangType::eDOUBLE:
				_ecvt_s ( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
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
	static inline stringi convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		int32_t dec;
		int32_t sgn;

		(*pVal)--;
		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				_i64toa_s ( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
				return tmpStr;
			case slangType::eDOUBLE:
				_ecvt_s ( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
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
	static inline VAR_OBJ *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;
		new (val) VAR_OBJ ( val );
		return reinterpret_cast<VAR_OBJ *>(val);
	}
};

template<>
struct varConvert <VAR_OBJ const *>
{
	static inline VAR_OBJ const *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;
		new (val) VAR_OBJ ( val );
		return reinterpret_cast<VAR_OBJ *>(val);
	}
};

template<>
struct varConvert <VAR_ARRAY *>
{
	static inline VAR_ARRAY *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;
		new (val) VAR_ARRAY ( val );
		return reinterpret_cast<VAR_ARRAY *>(val);
	}
};

template<>
struct varConvert <VAR_ARRAY const *>
{
	static inline VAR_ARRAY const *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;
		new (val) VAR_ARRAY ( val );
		return reinterpret_cast<VAR_ARRAY *>(val);
	}
};

template<>
struct varConvert <VAR_STR>
{
	static inline VAR_STR convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;
		int32_t dec;
		int32_t sgn;

		VAR_STR retVal;

		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				_i64toa_s ( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
				retVal.type = slangType::eSTRING;
				retVal.dat.str.c = tmpStr;
				retVal.dat.str.len = strlen ( tmpStr );
				return retVal;
			case slangType::eDOUBLE:
				_ecvt_s ( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
				retVal.type = slangType::eSTRING;
				retVal.dat.str.c = tmpStr;
				retVal.dat.str.len = strlen ( tmpStr );
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
	static inline VAR_STR *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *retVal = *pVal;
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;

		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				_i64toa_s ( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
				retVal->type = slangType::eSTRING;
				retVal->dat.str.c = tmpStr;
				retVal->dat.str.len = strlen ( tmpStr );
				return static_cast<VAR_STR *>(retVal);
			case slangType::eDOUBLE:
				{
					int32_t dec;
					int32_t sgn;

					_ecvt_s ( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
				}
				retVal->type = slangType::eSTRING;
				retVal->dat.str.c = tmpStr;
				retVal->dat.str.len = strlen ( tmpStr );
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
	static inline VAR_STR const *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *retVal = *pVal;
		VAR *val = *pVal;
		(*nParams)--;
		(*pVal)--;

		while ( val->type == slangType::eREFERENCE ) val = val->dat.ref.v;
		switch ( val->type )
		{
			case slangType::eLONG:
				_i64toa_s ( val->dat.l, tmpStr, _CVTBUFSIZE, 10 );
				retVal->type = slangType::eSTRING;
				retVal->dat.str.c = tmpStr;
				retVal->dat.str.len = strlen ( tmpStr );
				return static_cast<VAR_STR *>(retVal);
			case slangType::eDOUBLE:
				{
					int32_t dec;
					int32_t sgn;

					_ecvt_s ( tmpStr, _CVTBUFSIZE, val->dat.d, _CVTBUFSIZE - 1, &dec, &sgn );
				}
				retVal->type = slangType::eSTRING;
				retVal->dat.str.c = tmpStr;
				retVal->dat.str.len = strlen ( tmpStr );
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
	static inline vmInstance *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		return instance;
	}
};

template<>
struct varConvert <nParamType>
{
	static inline nParamType convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		nParamType ret ( *pVal, *nParams );
		return ret;
	}
};

template<>
struct varConvert <nWorkarea>
{
	static inline nWorkarea convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		nWorkarea ret = {};
		ret.value = instance->workareaTable->getData ();
		return ret;
	}
};

template<>
struct varConvert <void>
{
	static inline void convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
};

template<>
struct varConvert <VAR *>
{
	static inline VAR *convert ( vmInstance *instance, uint32_t *nParams, VAR **pVal, char *tmpStr )
	{
		VAR *val = *pVal;
		while ( val->type == slangType::eREFERENCE )
		{
			val = val->dat.ref.v;
		}
		(*nParams)--;
		(*pVal)--;
		return val;
	}
};

template<typename T, int = std::is_base_of<VAR_ARRAY,T>::value * 8 + std::is_base_of<VAR_OBJ,T>::value * 4 + std::is_integral<T>::value * 2 + std::is_floating_point<T>::value * 1>
struct toVar;

template<typename T>
struct toVar <T, 0>
{
	static inline VAR convert ( vmInstance *instance, T value )
	{
		static_assert (false, "unknown converstion to VAR");
	}
};

template<typename T>
struct toVar <T, 1>
{
	static inline VAR convert ( vmInstance *instance, T value )
	{
		VAR ret;
		ret.type = slangType::eDOUBLE;
		ret.dat.d = (double) value;
		return ret;
	}
};

template<typename T>
struct toVar <T, 2>
{
	static inline VAR convert ( vmInstance *instance, T value )
	{
		VAR ret;
		ret.type = slangType::eLONG;
		ret.dat.l = (int64_t) value;
		return ret;
	}
};

template<typename T>
struct toVar <T, 4>
{
	static inline VAR convert ( vmInstance *instance, T value )
	{
		auto ret = static_cast<VAR *>(&value);
		return *ret;
	}
};

template<typename T>
struct toVar <T, 8>
{
	static inline VAR convert ( vmInstance *instance, T value )
	{
		auto ret = static_cast<VAR *>(&value);
		return *ret;
	}
};

template<>
struct toVar <char *>
{
	static inline VAR convert ( vmInstance *instance, char *value )
	{
		assert ( value );
		VAR ret;
		ret.type = slangType::eSTRING;
		ret.dat.str.c = value;
		ret.dat.str.len = strlen ( value );
		return ret;
	}
};

template<>
struct toVar <char const *>
{
	static inline VAR convert ( vmInstance *instance, char const *value )
	{
		assert ( value );
		VAR ret;
		ret.type = slangType::eSTRING;
		ret.dat.str.c = const_cast<char *>(value);
		ret.dat.str.len = strlen ( value );
		return ret;
	}
};

template<>
struct toVar <std::string &>
{
	static inline VAR convert ( vmInstance *instance, std::string &value )
	{
		VAR ret;
		ret.type = slangType::eSTRING;
		ret.dat.str.c = const_cast<char *>(value.c_str ());
		ret.dat.str.len = value.size ();
		return ret;
	}
};

template<>
struct toVar <std::string>
{
	static inline VAR convert ( vmInstance *instance, std::string value )
	{
		VAR ret;
		ret.type = slangType::eSTRING;
		ret.dat.str.c = const_cast<char *>(value.c_str ());
		ret.dat.str.len = value.size ();
		return ret;
	}
};

template<>
struct toVar <stringi &>
{
	static inline VAR convert ( vmInstance *instance, stringi &value )
	{
		VAR ret;
		ret.type = slangType::eSTRING;
		ret.dat.str.c = const_cast<char *>(value.c_str ());
		ret.dat.str.len = value.size ();
		return ret;
	}
};

template<>
struct toVar <stringi>
{
	static inline VAR convert ( vmInstance *instance, stringi value )
	{
		VAR ret;
		ret.type = slangType::eSTRING;
		ret.dat.str.c = const_cast<char *>(value.c_str ());
		ret.dat.str.len = value.size ();
		return ret;
	}
};

template<>
struct toVar <VAR *>
{
	static inline VAR convert ( vmInstance *instance, VAR *value )
	{
		while ( value->type == slangType::eREFERENCE ) value = value->dat.ref.v;
		return *value;
	}
};

template<>
struct toVar <VAR>
{
	static inline VAR convert ( vmInstance *instance, VAR value )
	{
		auto ret = &value;
		while ( ret->type == slangType::eREFERENCE ) ret = ret->dat.ref.v;
		return *ret;
	}
};

template<>
struct toVar <VAR &>
{
	static inline VAR convert ( vmInstance *instance, VAR &value )
	{
		auto ret = &value;
		while ( ret->type == slangType::eREFERENCE ) ret = ret->dat.ref.v;
		return *ret;
	}
};

template<>
struct toVar <VAR_STR>
{
	static inline VAR convert ( vmInstance *instance, VAR_STR value )
	{
		auto ret = (VAR *) &value;
		while ( ret->type == slangType::eREFERENCE ) ret = ret->dat.ref.v;
		return *ret;
	}
};

template<>
struct toVar <VAR_STR &>
{
	static inline VAR convert ( vmInstance *instance, VAR_STR &value )
	{
		auto ret = (VAR *) &value;
		while ( ret->type == slangType::eREFERENCE ) ret = ret->dat.ref.v;
		return *ret;
	}
};

template<>
struct toVar <VAR_STR *>
{
	static inline VAR convert ( vmInstance *instance, VAR_STR *value )
	{
		auto ret = (VAR *) value;
		while ( ret->type == slangType::eREFERENCE ) ret = ret->dat.ref.v;
		return *ret;
	}
};

template<>
struct toVar <VAR_REF>
{
	static inline VAR convert ( vmInstance *instance, VAR_REF value )
	{
		return value;
	}
};

template<>
struct toVar <VAR_REF &>
{
	static inline VAR convert ( vmInstance *instance, VAR_REF &value )
	{
		return value;
	}
};

template<>
struct toVar <VAR_REF *>
{
	static inline VAR convert ( vmInstance *instance, VAR_REF *value )
	{
		return *value;
	}
};

template<>
struct toVar <VAR_OBJ>
{
	static inline VAR convert ( vmInstance *instance, VAR_OBJ value )
	{
		return value;
	}
};

template<>
struct toVar <VAR_OBJ &>
{
	static inline VAR convert ( vmInstance *instance, VAR_OBJ &value )
	{
		return value;
	}
};

template<>
struct toVar <VAR_OBJ *>
{
	static inline VAR convert ( vmInstance *instance, VAR_OBJ *value )
	{
		return *value;
	}
};

template<>
struct toVar <VAR_ARRAY>
{
	static inline VAR convert ( vmInstance *instance, VAR_ARRAY value )
	{
		return value;
	}
};

template<>
struct toVar <VAR_ARRAY &>
{
	static inline VAR convert ( vmInstance *instance, VAR_ARRAY &value )
	{
		return value;
	}
};

template<>
struct toVar <VAR_ARRAY *>
{
	static inline VAR convert ( vmInstance *instance, VAR_ARRAY *value )
	{
		return *value;
	}
};


struct vmDispatcher
{
	virtual ~vmDispatcher ()
	{};
	virtual void     operator() ( vmInstance *instance, uint32_t params ) = 0;
	virtual void     getPTypes ( bcFuncPType *pTypes ) = 0;
	virtual uint32_t getNParams () = 0;
	virtual char const *getClassName () = 0;
};

template<class T>
struct nativeDispatch
{};

template<class R, class ... Args> struct nativeDispatch < R ( Args... ) > : vmDispatcher
{
	nativeDispatch<R ( Args... )> ( R ( *func )(Args...) )
	{
		funcPtr = func;
	}
	virtual ~nativeDispatch<R (Args...)> ()
	{
	}

	void operator () ( vmInstance *instance, uint32_t nParams )  override
	{
		GRIP	 g1 ( instance );
		_call ( types<R>{}, instance, nParams, instance->stack - 1, types< Args... >{} );
	}
	void getPTypes ( bcFuncPType *pTypes )  override
	{
		if ( std::is_same<R, void>::value )
		{
			*pTypes = getPType<int>::toPType ();
		} else
		{
			*pTypes = getPType<R>().toPType();
		}
		_getpTypes ( ++pTypes, types<Args...>{}, types<R>{} );
	}

	char const *getClassName () override
	{
		return getPType<R>::getClass ();
	}
	uint32_t getNParams ()  override
	{
		return sizeof... (Args);
	}
	private:
	R ( *funcPtr ) (Args...);

	template<typename RET, class Head, class ... Tail, class ...Vs>
	typename std::enable_if<!(std::is_same<Head, char *>::value || std::is_same<Head, char const *>::value || std::is_same<Head, VAR_STR>::value || std::is_same<Head, VAR_STR *>::value), void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types< Head, Tail ...>, Vs && ...vs )
	{
		_call ( types<RET>{}, instance, nParams, param, types<Tail...>{}, std::forward<Vs> ( vs )..., varConvert<typename std::remove_const<Head>::type>::convert ( instance, &nParams, &param, 0 ) );
	}

	template<typename RET, class Head, class ... Tail, class ...Vs>
	typename std::enable_if<std::is_same<Head, char *>::value || std::is_same<Head, char const *>::value || std::is_same<Head, VAR_STR >::value || std::is_same<Head, VAR_STR *>::value, void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types< Head, Tail ...>, Vs && ...vs )
	{
		char tmpStr[_CVTBUFSIZE];
		_call ( types<RET>{}, instance, nParams, param, types<Tail...>{}, std::forward<Vs> ( vs )..., varConvert<typename std::remove_const<Head>::type>::convert ( instance, &nParams, &param, tmpStr ) );
	}

	template<typename RET, class ...Vs>
	typename std::enable_if<!std::is_same<RET, void>::value, void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types<>, Vs &&...vs )
	{
		instance->result = toVar<RET>::convert ( instance, (funcPtr) (std::forward <Vs> ( vs )...) );
		assert ( instance->result.type != slangType::eSTRING || instance->result.dat.str.c );
	}

	template<typename RET, class ...Vs>
	typename std::enable_if<std::is_same<RET, void>::value, void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types<>, Vs &&...vs )
	{
		funcPtr ( std::forward <Vs> ( vs )... );
	}

	template<class Head, class ... Tail, class RET>
	void _getpTypes ( bcFuncPType *pTypes, types< Head, Tail ...>, types<RET> )
	{
		if ( !std::is_void<Head>::value )
		{
			*pTypes = getPType<Head>::toPType ();
			_getpTypes ( ++pTypes, types<Tail...>{}, types<RET>{} );
		}
	}

	template<class RET>
	void _getpTypes ( bcFuncPType *pTypes, types<>, types<RET> )
	{
		//*pTypes = toPType<Head>();
	}
};

class slangDispatcher
{
	vmInstance *instance = 0;
	bcFuncDef  *callerFDef = 0;

	public:
	slangDispatcher ( vmInstance *instance, stringi name ) : instance ( instance )
	{
		callerFDef = instance->atomTable->atomGetFunc ( name.c_str ( ) );
	}

	slangDispatcher ()
	{
	}

	virtual ~slangDispatcher ()
	{};

	template<class ... Args>
	VAR * operator() ( vmInstance *instance, char const *name, Args && ...args )
	{
		this->instance = instance;
		callerFDef = instance->atomTable->atomGetFunc ( name );

		uint32_t nParams = sizeof...(Args);

		// if we don't have enough parameters push nulls
		for ( ; nParams < callerFDef->nParams; nParams++ )
		{
			auto pIndex = callerFDef->nParams - (nParams - sizeof...(Args)) - 1;
			if ( callerFDef->defaultParams[pIndex] )
			{
				instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex], 0, instance->stack, instance->stack );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}

		makeParams ( sizeof... (Args), types< Args... >{}, std::forward<Args> ( args )... );
		switch ( callerFDef->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				instance->interpretBC ( callerFDef, callerFDef->cs, nParams );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				instance->funcCall ( callerFDef, nParams );
				break;
		}
		instance->stack -= nParams;
		return &instance->result;
	}

	template<class ... Args>
	VAR * operator() (Args && ...args)
	{
		uint32_t nParams = sizeof...(Args);

		// if we don't have enough parameters push nulls
		for ( ;nParams < callerFDef->nParams; nParams++ )
		{
			auto pIndex = callerFDef->nParams - (nParams - sizeof...(Args)) - 1;
			if ( callerFDef->defaultParams[pIndex] )
			{
				instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex], 0, instance->stack, instance->stack );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}

		makeParams ( sizeof... (Args), types< Args... >{}, std::forward<Args> ( args )... );
		switch ( callerFDef->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				instance->interpretBC ( callerFDef, callerFDef->cs, nParams );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				instance->funcCall ( callerFDef, nParams );
				break;
		}
		instance->stack -= nParams;
		return &instance->result;
	}

private:
	template<class size, class Head, class ... Tail>
	void makeParams ( size pCount, types< Head, Tail ...>, Head &&head, Tail && ...args )
	{
		makeParams ( pCount, types<Tail...>{}, std::forward<Tail> ( args )... );
		*(instance->stack++) = toVar<head>::convert ( instance, head );
	}

	template<class size, class Head >
	void makeParams ( size pCount, types< Head>, Head &&head )
	{
		*(instance->stack++) = toVar<head>::convert ( instance, head );
	}
	template<class size>
	void makeParams ( size pCount, types<> )
	{
	}
};

class vmNativeInterface
{
	vmInstance					*instance;
	class opFile				*file;
	std::vector<std::string>	 ns;

	public:

	vmNativeInterface ( vmInstance *instance, class opFile *file ) : instance ( instance ), file ( file )
	{}

	std::string getNs ()
	{
		std::string res;
		for ( auto &it : ns )
		{
			res += "::" + it;
		}
		return res;
	}

	public:

	class nativeClass
	{
		vmNativeInterface			*native;
		char const					*nativeClassName;
		fgxClassElementScope		 nativeScope;
		bool						 nativeVirtual = false;
		bool						 nativeStatic = false;
		bool						 nativeExtension = false;

		public:

		nativeClass ( vmNativeInterface *native, char const *className );

		void Public ()
		{
			nativeScope = fgxClassElementScope::fgxClassScope_public;
		};
		void Private ()
		{
			nativeScope = fgxClassElementScope::fgxClassScope_private;
		};
		void Protected ()
		{
			nativeScope = fgxClassElementScope::fgxClassScope_protected;
		};
		void Virtual ()
		{
			nativeVirtual = true;

		};
		void Static ()
		{
			nativeStatic = true;
		};
		void Extension ()
		{
			nativeExtension = true;
		};

		void property ( char const *propName, fgxClassElementType type, char const *initializer = 0 );
		void property ( char const *propName, fgxClassElementType type, int64_t intializer );
		class opFunction *method ( char const *methodName, vmDispatcher *disp, ... );
		class opFunction *methodProp ( char const *methodName, bool isAccess, vmDispatcher *disp, ... );
		class opFunction *op ( char const *opName, vmDispatcher *disp );
		void require( char const* name, ... );
		void registerGcCb ( classGarbageCollectionCB cGarbageCollectionCB, classCopyCB cCopyCb );
		void registerPackCb ( void ( *cPackCB ) (class vmInstance *instance, struct VAR *var, struct BUFFER *buff, void *param, void ( *pack )(struct VAR *val, void *param)), void ( *cUnPackCB ) (class vmInstance *instance, struct VAR *obj, unsigned char ** buff, uint64_t *len, void *param, struct VAR *(*unPack)(unsigned char ** buff, uint64_t *len, void *param)) );
		void registerInspectorCb ( class vmInspectList *(*cInspectorCB) (class vmInstance *instance, struct bcFuncDef *funcDef, struct VAR *var, uint64_t start, uint64_t end) );

		friend class vmNativeInterface;
	};
	class nativeNamespace
	{
		vmNativeInterface  *native;

		public:
		nativeNamespace ( vmNativeInterface *native, std::string const &ns );
		~nativeNamespace ();

		friend class vmNativeInterface;
	};
	bcFuncDef		   *functionBuild ( char const *funcName, vmDispatcher*disp, bool makeFunc );
	class opFunction   *function ( char const *funcName, bool makeFunc, vmDispatcher*disp, ... );
	void				compile ( int cls, size_t lineNum, char const *fileName, char const *code );
	void				compile ( vmNativeInterface::nativeClass &cls, size_t lineNum, char const *fileName, char const *code );
	void				document ( char const *documentation );
};

// if NO_CALL is defined, it allows us to use all the registrations as interfaces only... the routines will NOT be referenced and
// subsequently not be callable.  However, when we're building the LINKER this is exactly what we want... it gives us all the
// function signatures, but no references to them.  If we build with function linking enabled we get a MUCH smaller executable
#if defined ( NO_CALL )
#define CALLBLE(func) 0
#else
#define CALLABLE(func) (func)
#endif

template<typename T, typename std::enable_if_t<std::is_move_constructible<T>::value>* = nullptr>
static void cGenericGcCb ( class vmInstance *instance, VAR_OBJ *obj, objectMemory::collectionVariables *col )
{
	auto image = obj->getObj<T> ( );
	if ( col->doCopy ( image ) )
	{
		obj->makeObj<T> ( col->getAge ( ), instance, std::move ( *image ) );
	}
}

template<typename T, typename std::enable_if_t<!std::is_move_constructible<T>::value> * = nullptr>
static void cGenericGcCb ( class vmInstance *instance, VAR_OBJ *obj, objectMemory::collectionVariables *col )
{
	auto image = obj->getObj<T> ( );
	if ( col->doCopy ( image ) )
	{
		classAllocateCargo ( instance, obj, sizeof ( T ) );
		memcpy ( obj->getCargo ( ), image, sizeof ( T ) );
	}
}

template<typename T, typename std::enable_if<std::is_copy_constructible<T>::value, int>::type * = nullptr>
static void cGenericCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	auto image = VAR_OBJ ( val ).getObj<T> ( );
	reinterpret_cast<VAR_OBJ *>(val)->makeObj<T> ( instance, *image );
}

template<typename T, typename std::enable_if<!std::is_copy_constructible<T>::value, int>::type * = nullptr>
static void cGenericCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	// TODO: fixe defaults that are causing this!!!
//	static_assert (false, "unsupported generic copyCB" );
}

#define REGISTER(instance, file) { vmNativeInterface vm(instance, file); auto &vm2 = vm; volatile int cls = 1;
#define END vm;vm2;cls;}
#define CLASS(name) { vmNativeInterface::nativeClass cls(&vm, name); volatile int vm=1;	/* defining vm here is to prevent non-class (FUNC) definitions from working */
#define NAMESPACE(name) { vmNativeInterface::nativeNamespace ns(&vm, name);
#define METHOD(name,func,...) cls.method ( name, new nativeDispatch<decltype(func)>(CALLABLE(func)), ##__VA_ARGS__, -1 )
#define REQUIRE(name,...) cls.require ( name, ##__VA_ARGS__, nullptr )
#define ACCESS(name, func) cls.methodProp ( name, true, new nativeDispatch<decltype(func)>(CALLABLE(func)), -1 )
#define ASSIGN(name, func) cls.methodProp ( name, false, new nativeDispatch<decltype(func)>(CALLABLE(func)), -1 )
#define OP(name, func) cls.op ( name, new nativeDispatch<decltype(func)>(CALLABLE(func)) )
#define IVAR(name,...) cls.property ( name, fgxClassElementType::fgxClassType_iVar, ##__VA_ARGS__ )
#define INHERIT(name,...) cls.property ( name, fgxClassElementType::fgxClassType_inherit, ##__VA_ARGS__ )
#define CONSTANT(name,...) cls.property ( name, fgxClassElementType::fgxClassType_const, ##__VA_ARGS__ )
#define PUBLIC cls.Public();
#define PRIVATE cls.Private();
#define PROTECTED cls.Protected();
#define VIRTUAL cls.Virtual();
#define STATIC cls.Static();
#define EXTENSION cls.Extension();
#define DEF(num,value) num, value
#if defined ( PURE )
#undef PURE
#endif
#define PURE ->setPure()
#if defined ( CONST )
#undef CONST
#endif
#define CONST ->setConst()

#define GCCB(cb, cb2) cls.registerGcCb ( cb, cb2 );
#define PACKCB(cbPack, cbUnpack) cls.registerPackCb ( cbPack, cbUnpack );
#define INSPECTORCB(cb) cls.registerInspectorCb ( cb );

#define FUNC(name, func,...)	 vm.function ( name, true, new nativeDispatch<decltype(func)>(CALLABLE(func)), ##__VA_ARGS__, -1 )
#define FUNC_RPC(name, func,...) vm.function ( name, true, new nativeDispatch<decltype(func::operator())>(CALLABLE(func)), ##__VA_ARGS__, -1 )

#define DOC(doc) vm.document ( doc );

/* note: for this macro to work correctly the opening ( MUST be on the same line as the string defining the code and the closing ) MUST be on the last line of the string */
//#define CODE(code) vm2.compile ( cls, __LINE__, std::filesystem::absolute ( __FILE__ ).string ( ).c_str(), code );
#define CODE(code) vm2.compile ( cls, __LINE__, __FILE__, code );

