#pragma once

#include "Utility/settings.h"
#include "Utility/stringi.h"

class webLog
{
public:
	virtual ~webLog () {};
	virtual size_t writeLog ( class webProtocolH1 *log, uint8_t *buff, size_t buffLen ) = 0;
	virtual void bindCompletionPort ( HANDLE compPort, ULONG_PTR key ) {}
	virtual operator HANDLE() { return 0; }
};
