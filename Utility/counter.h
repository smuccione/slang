#pragma once

#include <stdint.h>
#include "Utility/settings.h"
#include "Utility/stringi.h"
#include <atomic>

static inline std::atomic<uint64_t>	_globalCounter = 0;

inline uint64_t GetUniqueCounter ( )
{
	auto ret = ++_globalCounter;
	return ret;
}

inline stringi MakeUniqueLabel ( stringi const &label, size_t value = 0 )
{
	stringi ret;
	if ( value )
	{
		ret = label + "$" + (uint64_t)value;
	} else
	{
		ret = label + "$" + GetUniqueCounter ( );
	}
	return ret;
}
