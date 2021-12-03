/*
		IO Support functions

*/

#pragma once

#define _CRT_RAND_S

DWORD runAndCapture ( char const *directory, char const *fName, char const *params, char const *stdIn, BUFFER &buffer );
DWORD runAndDetatch ( char const *directory, char const *fName, char const *params, char const *stdIn );
