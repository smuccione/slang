#include <Windows.h>
#include <vector>
#include <map>

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "Utility/staticString.h"
#include "output.h"

static struct
{
	char const *fg;
	char const *bg;
	consoleColor		windowsColor;
} colors[] = {
#undef DEF_COLOR
#define DEF_COLOR(a,b,c,d) { c, d, (consoleColor)(b) },
COLOR_DEFS
};

std::map<staticLargeString, consoleColor> escapeToWindowsColorCode;
std::vector<char const *> windowsToFgColorCode;
std::vector<char const *> windowsToBgColorCode;

bool initConsoleCodes ( )
{
	windowsToFgColorCode.resize ( sizeof ( colors ) / sizeof ( colors[0] ) );
	windowsToBgColorCode.resize ( sizeof ( colors ) / sizeof ( colors[0] ) << 16);

	for ( auto &it : colors )
	{
		escapeToWindowsColorCode[staticLargeString ( it.fg, strlen ( it.fg ) )] = it.windowsColor;
		escapeToWindowsColorCode[staticLargeString ( it.bg, strlen ( it.bg ) )] = (consoleColor)(DWORD(it.windowsColor) << 16);

		windowsToFgColorCode[size_t (it.windowsColor)] = it.fg;
		windowsToBgColorCode[size_t(it.windowsColor) << 16] = it.bg;
	}
	return true;
}

static bool init = initConsoleCodes();

void setConsoleColor ( consoleColor fg, consoleColor bg )
{
	SetConsoleTextAttribute ( GetStdHandle ( STD_OUTPUT_HANDLE ), WORD (fg) + (WORD ( bg ) << 16) );
}