#pragma once

#include <string>
#include "versionLib.h"

inline static char BuildYear[] = { __DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], 0 };

inline static std::string Copyright = std::string ( "Copyright 2010-" ) + __DATE__[7] + " Stephen Muccione Jr.";
inline static std::string AuthorNotice = "Stephen Muccione Jr";