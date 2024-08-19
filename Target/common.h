/*
	exeStore.h			FGX and LIB file format and stuctures

*/

#pragma once

#include "Utility/settings.h"
#include <string>

#define MAX_NAME_SZ		254
#define ERROR_BASE		(1 << 28)

std::string GetSystemErrorAsString( uint32_t errNum );
std::string GetSystemErrorAsNameString ( uint32_t errNum );
