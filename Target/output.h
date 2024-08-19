#pragma once

#include "Utility/settings.h"

#include <stdint.h>
#include <map>
#include <vector>

#include "Utility/staticString.h"

#define COLOR_DEFS \
DEF_COLOR ( Black, 0, "\033[30m", "\033[40m" )		\
DEF_COLOR ( Blue, 9, "\033[1;34m", "\033[1;44m" )		\
DEF_COLOR ( Cyan, 11, "\033[1;36m", "\033[1;46m" )	\
DEF_COLOR ( White, 15, "\033[1;38m", "\033[1;48m" )	\
DEF_COLOR ( Gray, 7, "\033[1;37m", "\033[1;47m" )	\
DEF_COLOR ( Green, 10, "\033[1;32m", "\033[1;42m" )	\
DEF_COLOR ( Magenta, 13, "\033[1;35m", "\033[1;45m" )	\
DEF_COLOR ( Red, 12, "\033[1;31m", "\033[1;41m" )		\
DEF_COLOR ( Yellow, 14, "\033[22;33m", "\033[22;43m" )		\
DEF_COLOR ( DarkBlue, 1, "\033[22;34m", "\033[22;44m" )		\
DEF_COLOR ( DarkCyan, 3, "\033[22;36m", "\033[22;46m" )	\
DEF_COLOR ( DarkGray, 8, "\033[22;37m", "\033[22;40m" )		\
DEF_COLOR ( DarkGreen, 2, "\033[22;32m", "\033[22;42m" )	\
DEF_COLOR ( DarkMagenta, 5, "\033[22;35m", "\033[22;45m" )	\
DEF_COLOR ( DarkRed, 4, "\033[22;31m", "\033[22;41m" )		\
DEF_COLOR ( DarkYellow, 6, "\033[22;33m", "\033[22;43m" )	\

enum class consoleColor {
#undef DEF_COLOR
#define DEF_COLOR(a,b,c,d) a = b,
COLOR_DEFS
};

extern std::map<staticLargeString, consoleColor> escapeToWindowsColorCode;
extern std::vector<char const *> windowsToFgColorCode;
extern std::vector<char const *> windowsToBgColorCode;

void setConsoleColor ( consoleColor fg = consoleColor::White, consoleColor bg = consoleColor::Black );