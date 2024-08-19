#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <memory>
#include <vector>
#include <string>

#include "winsock2.h"
#include <windows.h>

extern	BOOL WINAPI GetServerVariable ( class vmInstance *instance, char const *lpszVariableName, void *lpvBuffer, size_t &lpdwSizeofBuffer );

