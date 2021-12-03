#include "Utility/settings.h"

#include "stringi.h"
#include "buffer.h"

stringi::stringi ( char const **expr )
{
	str = *expr;
	*expr += str.size () + 1;
}

void stringi::serialize ( struct BUFFER *buff ) const
{
	buff->write ( str.c_str (), str.size () + 1 );
}

