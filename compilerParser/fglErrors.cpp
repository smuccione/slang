/*
	FGL Internal error codes
*/

#include "compilerParser/fglErrors.h"
#include "Target/common.h"

#define REGISTER_ERROR(errNum, errTxt) { errorNum::errNum, (errTxt) },
errorTable fglErrors[] = {
	ERROR_TABLE
	{(errorNum)0, ""}
};
#undef REGISTER_ERROR

#define REGISTER_WARN(num, errTxt) { warnNum::num, (errTxt) },
warnTable fglWarnings[] = {
	WARN_TABLE
	{(warnNum)0, ""}
};
#undef REGISTER_WARN

stringi scCompErrorAsText ( size_t err )
{
	if ( (errorNum(err) < errorNum::ERROR_BASE_ERROR) || (errorNum ( err ) > errorNum::ERROR_MAX_ERROR) )
	{
		return GetSystemErrorAsString( (uint32_t) err );
	}
	return fglErrors[err - int(errorNum::ERROR_BASE_ERROR) - 1].txt;
}

stringi scCompWarn ( size_t err )
{
	return fglWarnings[err].txt;
}

