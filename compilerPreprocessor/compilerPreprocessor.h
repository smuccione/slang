/*

	preprocessor definitions

*/
#pragma once

#include "Utility/settings.h"
#include "Utility/sourceFile.h"

struct languageRegion
{
	enum class languageId
	{
		html = 0,
		slang = 1,
		fgl = 2,
	};

	languageId	language = languageId::fgl;

	srcLocation	location;

	languageRegion ( )
	{}

	languageRegion ( languageId language, size_t sourceIndex, size_t col, size_t line ) : language ( language )
	{
		location.columnNumber = (uint32_t) col;
		location.lineNumberStart= (uint32_t)line;
	}

	void end ( size_t col, size_t line )
	{
		location.columnNumberEnd = (uint32_t)col;
		location.lineNumberEnd = (uint32_t)line ;
	}

	operator srcLocation & ()
	{
		return location;
	}
};

extern char		*compAPPreprocessor		( char const *fName, bool isSlang, bool encapsulate );
extern char		*compAPPreprocessBuffer	( char const *fName, char const *src, bool isSlang, bool encapsulate );
extern char		*compPreprocessor		( char const *fName, char const *src, bool isSlang );
