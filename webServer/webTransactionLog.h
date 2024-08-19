#pragma once

#include "Utility/settings.h"

#include <memory>
#include <vector>
#include "webLog.h"

struct transLogEntry {
	enum transLogElemType {
		tlText,
		tlRemoteIP,
		tlLocalIp,
		tlLocalPort,
		tlRemotePort,
		tlCanonicalPort,
		tlBytesSent,
		tlBytesSentNoHeaders,
		tlBytesSentCLF,
		tlBytesRcvd,
		tlCookie,
		tlServiceTime,
		tlServiceTimeuSecs,
		tlLoadTimeuSecs,
		tlRunTimeuSecs,
		tlFullName,
		tlRemoteLogname,
		tlConnectionStatus,
		tlHost,
		tlEnvironmentVar,
		tlProtocol,
		tlHeader,
		tlHTTPType,
		tlHeaders,
		tlReferer,
		tlFrom,
		tlUserAgent,
		tlHeaderField,
		tlMethod,
		tlClientPort,
		tlQuery,
		tlRequestLine,
		tlStatus,
		tlTime,
		tlUserName,
		tlPageName,
		tlServerName,
		tlProcessId,
		tlThreadId,
		tlHexTID,
		tlXForwardedFor,
	}	type;
	stringi					text;
	stringi					foo;
	std::vector<bool>		condition;
	bool					origReq;

	transLogEntry ( const transLogEntry &old )
	{
		type = old.type;
		text = old.text;
		condition = old.condition;
		origReq = old.origReq;
	}
	transLogEntry ( transLogEntry &&old )
	{
		std::swap ( type, old.type );
		std::swap ( text, old.text );
		std::swap ( origReq, old.origReq );
		std::swap ( condition, old.condition );
	}
	transLogEntry ( stringi &string )
	{
		type = tlText;
		text = string;
		origReq = true;
	}
	transLogEntry ( enum transLogElemType type ) : type ( type )
	{
	}
	transLogEntry ( enum transLogElemType type, std::vector<bool> &cond, bool origReq ) : type ( type ), origReq ( origReq )
	{
		condition.resize( cond.size() );
		condition.swap ( cond );
	}
	transLogEntry ( enum transLogElemType type, std::vector<bool> &cond, bool origReq, std::string &foo ) : type ( type ), origReq ( origReq )
	{
		this->foo.swap ( foo );
		condition.resize( cond.size() );
		condition.swap ( cond );
	}
	transLogEntry &operator = ( transLogEntry &&lhs )
	{
		std::swap ( text, lhs.text );
		std::swap ( foo, lhs.foo );
		std::swap ( condition, lhs.condition );
		std::swap ( origReq, lhs.origReq );
		return *this;
	}
	transLogEntry &operator = ( transLogEntry const &lhs )
	{
		text = lhs.text;
		foo = lhs.foo;
		condition = lhs.condition;
		origReq = lhs.origReq;
		return *this;
	}
};

class transLog : public webLog
{
	std::vector<transLogEntry>	logElems;
	HANDLE						handle = 0;
	size_t					currentLogOffset;

public:
	transLog ( char const *format, char const *fName, bool console );

	transLog ( transLog && old ) noexcept
	{
		*this = std::move ( old );
	}

	transLog ( transLog const &old ) noexcept
	{
		logElems = old.logElems;
		DuplicateHandle (	GetCurrentProcess (), old.handle,
							GetCurrentProcess (), &handle, 0,
							TRUE, DUPLICATE_SAME_ACCESS
						);
		currentLogOffset = old.currentLogOffset;
	}

	transLog &operator = ( transLog && old ) noexcept
	{
		std::swap ( logElems, old.logElems );
		std::swap ( handle, old.handle );
		std::swap ( currentLogOffset, old.currentLogOffset );
		return *this;
	}

	transLog &operator = ( transLog const &old ) noexcept
	{
		logElems = old.logElems;
		DuplicateHandle (	GetCurrentProcess (), old.handle,
							GetCurrentProcess (), &handle, 0,
							TRUE, DUPLICATE_SAME_ACCESS
						);
		currentLogOffset = old.currentLogOffset;
		return *this;
	}

	~transLog ( )
	{
		if ( handle && handle != GetStdHandle ( STD_OUTPUT_HANDLE ) )
		{
			CloseHandle ( handle );
		}
	}

	size_t writeLog ( class webProtocolH1 *log, uint8_t *buff, size_t buffLen );
	size_t allocateLogEntry ( size_t size );

	void bindCompletionPort ( HANDLE compPort, ULONG_PTR key )
	{
		CreateIoCompletionPort ( handle, compPort, key, 0 );
	}

	operator HANDLE() { return handle; }
};
