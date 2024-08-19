/*

	Error Handler

*/

#pragma once

#include "Utility/settings.h"

#include <stdint.h>
#include <list>
#include <tuple>
#include <exception>

#include "Target/common.h"
#include "Target/vmConf.h"
#include "compilerParser/fglErrors.h"
#include "Utility/sourceFile.h"
#include "Utility/buffer.h"

class errorHandler {
	bool						 fatalThrown = false;
	bool						 warnThrown = false;
	errorNum					 errNum{};
	class opFile				*file;
	std::vector<std::pair<srcLocation, class astNode *>>	 locality;
	size_t						 nPopNeeded = 0;

public:

	errorHandler ( class opFile *file );

	void showLineCaret ( char const *fileName, srcLocation const &location, class astNode *node, consoleColor color );

	void throwFatalError ( bool isLS, unsigned long errNum )
	{
		throwFatalError ( isLS, (errorNum) errNum );
	}

	char const *getName ( uint32_t sourceIndex );

	void resetFatalError()
	{
		fatalThrown = false;
	}

	void setFatal ()
	{		
		fatalThrown = true;
	}

	void throwFatalError ( bool isLS, errorNum errNum );

#if defined ( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#endif
	template <typename ...Args>
	stringi throwWarning ( bool isLS, warnNum warnNum, Args &&...args )
	{
//		if ( vmConf.warnDisable[warnNum] ) return "";
		if ( !isLS )
		{
			for ( auto it = locality.begin(); it != locality.end(); it++ )
			{
				if ( std::get<0>( *it ).sourceIndex )
				{
					printf( "%s(%u)", getName( std::get<0>( *it ).sourceIndex ), std::get<0> ( *it ).lineNumberStart );
					if ( std::next ( it ) != locality.end () && std::get<0> ( *std::next ( it ) ).sourceIndex )
					{
						printf ( "::" );
					}
				}
			}

			printf( ": %sWarning W%04x - ", windowsToFgColorCode[int(consoleColor::Yellow)], (int)warnNum );
			printf( scCompWarn( size_t( warnNum ) ).c_str(), std::forward<Args>( args )... );
			printf( "%s\r\n", windowsToFgColorCode[int ( consoleColor::DarkGray)] );
			showLineCaret( getName( std::get<0>( locality.back() ).sourceIndex ), std::get<0>( locality.back() ), std::get<1>( locality.back() ), consoleColor::Yellow );
		}
		warnThrown = true;

		BUFFER out;
		out.printf ( scCompWarn ( size_t ( warnNum ) ).c_str (), std::forward<Args> ( args )... );

		return stringi ( out.data<char const *> () );
	}

	uint32_t topSourceIndex ( void ) noexcept
	{
		return std::get<0>(locality.back ( )).sourceIndex;
	}

	void popLocality ()
	{
		assert ( locality.size () > 1 );
		// we don't actually want to resize here... we only resize on a push.
		// that way we maintain our error location incase we throw... if we're throwing we're not going to be pushing any more as we'll just unwind to the top
		if ( !std::uncaught_exceptions () )
		{
			// No exception
			locality.pop_back ();
		} else
		{
			nPopNeeded++;
		}
	}

	void setLine ( class source &src );
	bool isFatal() noexcept
	{
		return fatalThrown;
	}

	errorNum getErrorNum ( ) noexcept
	{
		return errNum;
	}
	friend class errorLocality;
};

class errorLocality
{
	errorHandler							*err;
	bool									doPop = false;

	public:
	errorLocality ( errorHandler *e, source &src );
	errorLocality ( errorHandler *e, srcLocation const &location, bool force = false ) : err ( e )
	{
		if ( force || (err->locality.size () <= 1)  || (location.sourceIndex != std::get<0>(err->locality.back ( )).sourceIndex) )
		{
			err->locality.push_back ( std::pair{ location, nullptr } );
			doPop = true;
		} else
		{
			err->locality.back () = std::pair { location, nullptr };
		}
	}
	errorLocality ( errorHandler *e, class astNode *node, bool force = false );
	~errorLocality ()
	{
		if ( doPop )
		{
			err->popLocality ();
		}
	}
};

