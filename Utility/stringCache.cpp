#include "Utility/settings.h"

#include "stdlib.h"
#include <string>
#include <type_traits>
#include <unordered_set> 
#include "Utility/encodingTables.h"
#include "buffer.h"
#include "util.h"
#include "stringi.h"
#include "stringCache.h"

#include <cassert>


cacheString const stringCache::get ( stringi const &str )
{
	auto it = cache.insert ( str );
	return cacheString ( &(*it.first) );
}

