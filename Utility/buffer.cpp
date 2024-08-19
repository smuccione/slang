/*
	buffer defines

*/

#include "Utility/settings.h"

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <iostream>

#include "Target/common.h"
#include "compilerParser/fglErrors.h"
#include "Target/output.h"
#include "buffer.h"


std::ostream &operator << ( std::ostream &os, BUFFER const &buff )
{
	os.write( buff.data<char const *>(), static_cast<std::streamsize>(buff.size()) );
	return os;
}
