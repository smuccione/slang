/*
	FGL Internal error codes
*/

#pragma once

#include <string>
#include <stdint.h>
#include "fglErrorDefinitions.h"
#include "Target/common.h"
#include "Utility/stringi.h"

// instantiate the errorNum enumerations
#define REGISTER_ERROR( errNum, errTxt )	errNum,
enum class errorNum {
	ERROR_OK = 0,
	ERROR_BASE_ERROR = ERROR_BASE,
	ERROR_TABLE
	ERROR_MAX_ERROR
};
#undef REGISTER_ERROR

// instantiate the warnNum enumerations
#define REGISTER_WARN( errNum, errTxt )	errNum,
enum class warnNum {
	WARN_TABLE
	WARN_MAX_WARN
};
#undef REGISTER_WARN	

struct errorTable {
	errorNum		 num;
	char	const	*txt;
};

struct warnTable {
	warnNum			 num;
	char	const	*txt;
};

struct parseError {
	uint32_t	 lineNum;
	char const	*lineStart;
	char const	*errorPos;
	errorNum	 errNum;
};

extern	stringi	scCompWarn			( size_t err );
extern	stringi scCompErrorAsText	( size_t err );

