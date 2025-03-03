#pragma once

#include <source_location>

namespace logger {
	struct formatLocation
	{
		char const *fmt;
		std::source_location const &loc;

		formatLocation( char const *fmt, std::source_location const &loc = std::source_location::current() ) : fmt( fmt ), loc( loc )
		{
		}
	};
}

template<typename ...P>
void logger( logger::formatLocation const &fmtLoc, P&& ...p )
{
#if 0
	if ( fmtLoc.loc.function_name()[0] )
	{
		printf( "%s:%s(%u): ", fmtLoc.loc.file_name(), fmtLoc.loc.function_name(), fmtLoc.loc.line() );
	} else
	{
		printf( "%s(%u): ", fmtLoc.loc.file_name(), fmtLoc.loc.line() );
	}
#endif
	printf( fmtLoc.fmt, std::forward<P>( p ) ... );
}
