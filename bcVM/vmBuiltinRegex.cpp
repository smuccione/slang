/*
SLANG support functions

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include <regex>
#include <string_view>

static void regexNew ( vmInstance *instance, VAR_OBJ *obj, VAR_STR *str, int64_t flags )
{
	obj->makeObj<std::regex> ( instance, str->dat.str.c, flags );
	*(*obj)["pattern"] = *str;
}

struct regexEnum
{
	std::cmatch		matchInfo;
	int64_t			which = 0;
};

static void regexEnumeratorNew ( vmInstance *instance, VAR_OBJ *obj, VAR_OBJ *regex )
{
	obj->makeObj<regexEnum> ( instance );
	*(*obj)["regex"] = *regex;
}

static VAR_OBJ_TYPE<"regexEnumerator"> regexEnumeratorGetEnumerator ( vmInstance *instance, VAR_OBJ *obj )
{
	return *obj;
}

static bool regexEnumeratorMoveNext ( vmInstance *instance, VAR_OBJ *obj )
{
	auto enumerator = obj->getObj<regexEnum> ( );

	enumerator->which++;
	if ( enumerator->which < (int64_t)enumerator->matchInfo.size ( ) )
	{
		return true;
	}
	return false;
}

static VAR_STR regexEnumeratorCurrent ( vmInstance *instance, VAR_OBJ *obj )
{
	auto enumerator = obj->getObj<regexEnum> ( );

	return VAR_STR ( instance, enumerator->matchInfo[enumerator->which].str().c_str(), enumerator->matchInfo[enumerator->which].str ( ).size() );
}

#if 0
static VAR_OBJ_TYPE<"regexEnumerator">  regexMatch ( vmInstance *instance, VAR_OBJ *obj, VAR_STR *str, int64_t flags )
{
//	auto r = obj->getObj<std::regex> ( );
//	auto ret = instance->objNew ( "$regexEnumerator", obj );
//	auto e = VAR_OBJ ( &ret ).getObj<regexEnum> ( );

//	std::regex_match ( str->dat.str.c, e->matchInfo, *r, *reinterpret_cast<std::regex_constants::match_flag_type *>(&flags) );

	return ret;
}
#endif

static int64_t regexAt ( vmInstance *instance, VAR_OBJ *obj, VAR_STR *str )
{
	auto stringView = std::string_view ( str->dat.str.c, str->dat.str.len );

	auto r = obj->getObj<std::regex>( );

	std::cmatch results;

	if ( !std::regex_match ( str->dat.str.c, results, *r, std::regex_constants::match_default ) )
	{
		return 0;
	}
	return (int64_t) results.position ( 0 );
}
void builtinRegexInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		CLASS ( "regex" );
			CONSTANT ( "icase", std::regex::icase );
			CONSTANT ( "nosubs", std::regex::nosubs );
			CONSTANT ( "optimize", std::regex::optimize );
			CONSTANT ( "collate", std::regex::collate );
			CONSTANT ( "ECMAScript", std::regex::ECMAScript );
			CONSTANT ( "basic", std::regex::basic );
			CONSTANT ( "extended", std::regex::extended );
			CONSTANT ( "awk", std::regex::awk );
			CONSTANT ( "grep", std::regex::grep );
			CONSTANT ( "egrep", std::regex::egrep );

			METHOD ( "new", regexNew, DEF ( 3, "regex::ECMAScript" ) );
//			METHOD ( "match", regexMatch, DEF ( 3, "0" ) );
			METHOD ( "at", regexAt );
			IVAR ( "pattern" );
			GCCB ( cGenericGcCb<std::regex>, cGenericCopyCb<std::regex> );
		END;

		CLASS ( "regexEnumerator" );
			INHERIT ( "Queryable" );
			METHOD ( "new", regexEnumeratorNew );
			METHOD ( "getEnumerator", regexEnumeratorGetEnumerator );
			METHOD ( "moveNext", regexEnumeratorMoveNext );
			ACCESS ( "current", regexEnumeratorCurrent ) CONST;

			PRIVATE
				IVAR ( "regex" );
			GCCB ( cGenericGcCb<std::smatch>, cGenericCopyCb<std::smatch> );
		END
	END;
}
