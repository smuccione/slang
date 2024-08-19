/*
	WEB Server header file

*/

#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <functional>
#include <cassert>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <memory.h>

#include "Utility/staticString.h"
#include "Utility/stringi.h"
#include "webBuffer.h"
#include "Target/fileCache.h"
#include "Utility/encodingTables.h"
#include "webTransactionLog.h"

class webBuffs
{
	public:
	struct
	{
		size_t		 len = 0;
		uint8_t		*buf = nullptr;
	}			workBuff[6];
	size_t		nWorkBuff = 0;
	size_t		startWorkBuff = 0;
	size_t		totalIo = 0;

	WSABUF		buff[4] = { { 0 } };
	size_t		nBuffs = 0;
	size_t		startBuff = 0;;

	public:
	void addBuff( uint8_t *ptr, size_t len )
	{
		if ( len )
		{
			assert ( ptr );
			if ( nWorkBuff == sizeof ( workBuff ) / sizeof ( workBuff[0] ) )
			{
				printf ( "ERROR: too many add buff calls\r\n" );
				throw 500;
			}
			workBuff[nWorkBuff].buf = ptr;
			workBuff[nWorkBuff].len = len;
			nWorkBuff++;
			totalIo += len;
		}
	}

	// all this is because the WSABUF has a 32-bit length...   because we can mmap a very large file, this may exceed the size we can write from a single WSABUF so we
	//   have to chunk this up into potentially many WSABUF calls.
	void populateWSA( void )
	{
		startBuff = 0;

		nBuffs = 0;
		while ( startWorkBuff < nWorkBuff )
		{
			while ( workBuff[startWorkBuff].len )
			{
				size_t	len = workBuff[startWorkBuff].len;

				// prune it to the max WSABUF size;
				if ( len > 0x00FFFFF0 ) len = 0x00FFFFF0;

				// add it to the WSABUF array
				buff[nBuffs].len = (uint32_t)len;
				buff[nBuffs].buf = (CHAR *)workBuff[startWorkBuff].buf;
				nBuffs++;

				// remove this portion from our output list 
				workBuff[startWorkBuff].len -= len;
				workBuff[startWorkBuff].buf += len;

				// have we maxed out our WSABUF's?   if so get out and well do more later
				if ( nBuffs == sizeof( buff ) / sizeof( buff[0] ) ) return;
			}
			startWorkBuff++;
		}
	}

	uint8_t		*nextBuff( void )
	{
		if ( !nBuffs ) populateWSA();
		return (uint8_t *)buff[startBuff].buf;
	}

	uint32_t	 nextLen( void )
	{
		if ( !nBuffs ) populateWSA();
		return buff[startBuff].len;
	}

	size_t read( uint8_t *ptr, size_t len )
	{
		size_t	nRead = 0;
		size_t	index;

		if ( !totalIo ) return nRead;

		if ( !nBuffs ) populateWSA();

		for ( index = startBuff; (index < nBuffs) && len; index++ )
		{
			if ( len >= buff[index].len )
			{
				// need to read more than just this buffer... copy it to our read pointer and continue
				memcpy( ptr, buff[index].buf, buff[index].len );
				totalIo -= buff[index].len;
				nRead += buff[index].len;
				ptr += buff[index].len;
				len -= buff[index].len;
			} else
			{
				// have enough to finish off the read, copy and break out
				memcpy( ptr, buff[index].buf, len );
				memmove( buff[index].buf, &buff[index].buf[len], buff[index].len - len );
				totalIo -= len;
				nRead += len;
				ptr += len;
				len -= buff[index].len;
				break;
			}
		}
		return nRead;
	}

	size_t write( uint8_t *ptr, size_t len )
	{
		size_t	nWritten = 0;
		size_t	index;

		if ( !nBuffs ) populateWSA();
		for ( index = startBuff; (index < nBuffs) && len; index++ )
		{
			if ( len >= buff[index].len )
			{
				// filled up the buffer, so write as much as we can, and continue to the next buffer
				memcpy( buff[index].buf, ptr, buff[index].len );

				buff[index].buf += len;
				nWritten += buff[index].len;
				ptr += buff[index].len;
				len -= buff[index].len;
				totalIo -= len;
				startBuff++;
			} else
			{
				// everything we have has been written to the buffer... break out when done
				memcpy( buff[index].buf, ptr, len );
				buff[index].buf += len;
				nWritten += len;
				ptr += len;
				totalIo -= len;
				break;
			}
		}
		return nWritten;
	}

	void reset()
	{
		startBuff = 0;
		nBuffs = 0;
		totalIo = 0;
		nWorkBuff = 0;
		startWorkBuff = 0;
	}

	bool consume( size_t amt )
	{
		size_t	index;

		// short optimization for when we process everything at once (which is almost always)
		if ( amt >= totalIo )
		{
			totalIo = 0;
			nBuffs = 0;
			return true;
		}

		totalIo -= amt;
		for ( index = startBuff; index < nBuffs; index++ )
		{
			if ( amt >= buff[index].len )
			{
				startBuff++;
				amt -= buff[index].len;
			} else
			{
				buff[index].len -= (uint32_t)amt;
				buff[index].buf += amt;
				break;
			}
		}
		if ( startBuff == nBuffs )
		{
			populateWSA();
		}
		return false;
	}
	operator WSABUF *()
	{
		if ( !nBuffs ) populateWSA();
		return &buff[startBuff];
	}
	operator DWORD ()
	{
		if ( !nBuffs ) populateWSA();
		return (DWORD)(nBuffs - startBuff);
	}

	operator FILE_SEGMENT_ELEMENT *()
	{
		return 0;
	}
};

class pathString {
	stringi str;

public:
	pathString ( stringi &str ) : str ( str )
	{}

	pathString ( char const *str, size_t len )
	{
		this->str = stringi ( str, len );
	}

	pathString ( char const *string )
	{
		this->str = stringi ( string );
	}

	bool operator < ( const pathString &old ) const
	{
		size_t cmpLen = str.size() > old.str.size() ? old.str.size() : str.size();
		int32_t r;
		r = memcmpi ( str.c_str(), old.str.c_str(), cmpLen );

		if ( r < 0 )
		{
			return true;
		} else if ( !r )
		{
			if ( str.size() > old.str.size() ) return true;
		} 
		return false;
	}

	inline size_t hash() const
	{
		// your own hash function here
		size_t h = 0;
		char const *p;
		for ( p = str.c_str(); p  < str.c_str() + str.size(); p++ )
		{
			h = 31 * h + caseInsenstiveTable[(uint8_t)(*p)];
		}
		return h;
	}

	inline bool operator == ( const pathString &obj ) const
	{
		if ( str.size() >= obj.str.size() ) return false;
		if ( memcmpi ( str.c_str(), obj.str.c_str(), str.size() ) ) return false;
		return true;
	}
};

namespace std {
	template <>
	class hash<pathString> {
	public:
		size_t operator()(const pathString &obj) const {
			return obj.hash();
		}
	};
}

enum serverIOType {
	needNone,
	needRecv,
	needSend,
	needWrite,
	needRead,
	needClose
};

enum webServerCompression {			// bit field value
	compNone			= 0,
	compGzip			= 1,
	compDeflate			= 2,
	compCompress		= 4,
};

enum webServerTransferEncodings {
	encodeNone		= 0,
	encodeChunked	= 1,
};

enum webServerPageState {
	wsParseURL	= 1,
	wsParseHeaders,
	wsReceiveBody,
	wsParseBody,
	wsProcess,
	wsSendResponse,
	wsReset
};

enum webServerMultipartState {
	wsParseBodyMultipartMemoryStart,
	wsParseBodyMultipartMemoryHeaders,
	wsParseBodyMultipartMemoryContent,
};

struct webServerRequestCookie {
	uint8_t		**basePtr;
	size_t		  nameOffset;
	size_t		  nameLen;
	size_t		  valueOffset;
	size_t		  valueLen;

	webServerRequestCookie ( uint8_t **basePtr, uint8_t *name, size_t nameLen, uint8_t *value, size_t valueLen ) : basePtr ( basePtr ), nameOffset(name - *basePtr), nameLen (nameLen), valueOffset (value - *basePtr), valueLen(valueLen) {};
	webServerRequestCookie ( void )
	{
		nameLen = 0;
		valueLen = 0;
	}

	int operator == ( char const *s )
	{
		if ( !s[nameLen] )
		{
			return memcmpi ( (*basePtr + nameOffset), s, nameLen ) == 0;
		}
		return 0;
	}

	operator size_t ( void ) const
	{
		size_t	decodedLen;

		decodedLen = (size_t)((valueLen + 3)/ 4) * 3;

		if ( (*basePtr + valueOffset)[valueLen - 1] == '=' ) decodedLen--;
		if ( (*basePtr + valueOffset)[valueLen - 2] == '=' ) decodedLen--;

		return decodedLen;
	}

	void getName ( char *name ) const
	{
		memcpy ( name, *basePtr + nameOffset, nameLen );
	}

	size_t getNameLen ( void ) const
	{
		return nameLen;
	}

	void getValueRaw ( uint8_t *out ) const
	{
		memcpy ( out, *basePtr + valueOffset, valueLen );
	}

	size_t getValueLenRaw ( ) const
	{
		return valueLen;
	}

	void getValue ( uint8_t *out ) const
	{
		size_t		 loop;
		size_t		 decodedLen;
		uint8_t		*in;
		uint8_t		 dec[4] = {};

		size_t		 outputIndex = 0;

		decodedLen = *this;

		in = (*basePtr + valueOffset);
		for ( loop = 0; loop < valueLen; loop += 4 )
		{
			/* convert them from the alphabet to the binary equialent.. pos in the alphabet */
			dec[0] = webBase64DecodingTable[in[0]];
			dec[1] = webBase64DecodingTable[in[1]];
			dec[2] = webBase64DecodingTable[in[2]];
			dec[3] = webBase64DecodingTable[in[3]];

			if ( outputIndex < decodedLen ) out[outputIndex++] = (dec[0] << 2) | (dec[1] >> 4);
			if ( outputIndex < decodedLen ) out[outputIndex++] = ((dec[1] & 0x0F) << 4) | (dec[2] >> 2);
			if ( outputIndex < decodedLen ) out[outputIndex++] = ((dec[2] & 0x03) << 6) | dec[3];

			/* advance pointers */
			in += 4;
		}
		out[outputIndex] = 0;
	}

	size_t getValueLen ( void ) const
	{
		return ( *this );
	}
};

struct webServerRequestVar {
	uint8_t		**basePtr;
	size_t		  nameOffset;
	size_t		  nameLen;
	size_t		  valueOffset;
	size_t		  valueLen;

	webServerRequestVar( uint8_t **basePtr, uint8_t *name, size_t nameLen, uint8_t *value, size_t valueLen ) : basePtr( basePtr ), nameOffset( name - *basePtr ), nameLen( nameLen ), valueOffset( value - *basePtr ), valueLen( valueLen ) {}
	webServerRequestVar ( void ) {}

	/* does a compare of a url encoded this to a string name */

	bool operator == ( char const *s )
	{
		size_t		loop;
		uint8_t		val;

		assert ( s );
		for ( loop = nameOffset; loop < nameLen + nameOffset; loop ++ )
		{
			switch ( (*basePtr)[loop] )
			{
				case '%':
					val = (hexDecodingTable[(size_t) (*basePtr)[loop + 1]] << 4) + hexDecodingTable[(size_t) (*basePtr)[loop + 2]];
					loop += 2;
					break;
				case '+':
					val = ' ';
					break;
				default:
					val = (*basePtr)[loop];
					break;
			}
			if ( caseInsenstiveTable[(uint8_t)val] != caseInsenstiveTable[(uint8_t)*(s++ )] ) return false;
		}
		if ( !s ) return false;
		return true;
	}

	operator size_t ( void ) const
	{
		size_t	loop;
		size_t	decodedLen = 0;

		for ( loop = 0; loop < valueLen; loop++ )
		{
			if ( (*basePtr)[valueOffset + loop] == '%' )
			{
				loop += 2;
			}
			decodedLen++;
		}

		return decodedLen;
	}

	void getValue ( uint8_t *out ) const
	{
		size_t	 loop;
		size_t	 decodedLen;

		decodedLen = 0;
		for ( loop = valueOffset; loop < valueLen + valueOffset; loop ++ )
		{
			switch ( (*basePtr)[loop] )
			{
				case '%':
					out[decodedLen++] = (hexDecodingTable[(*basePtr)[loop + 1]] << 4) + hexDecodingTable[(*basePtr)[loop + 2]];
					loop += 2;
					break;
				case '+':
					out[decodedLen++] = ' ';
					break;
				default:
					out[decodedLen++] = (*basePtr)[loop];
					break;
			}
		}
	}

	size_t getValueLen ( void )  const
	{
		return (*this); // this->operator size_t();
	}

	void getName ( uint8_t *out ) const
	{
		size_t	 loop;
		size_t	 decodedLen;

		decodedLen = 0;
		for ( loop = nameOffset; loop < nameLen + nameOffset; loop ++ )
		{
			switch ( (*basePtr)[loop] )
			{
				case '%':
					out[decodedLen++] = (hexDecodingTable[(*basePtr)[loop + 1]] << 4) + hexDecodingTable[(*basePtr)[loop + 2]];
					loop += 2;
					break;
				case '+':
					out[decodedLen++] = ' ';
					break;
				default:
					out[decodedLen++] = (*basePtr)[loop];
					break;
			}
		}
		out[decodedLen] = 0;
	}

	size_t getNameLen ( void ) const
	{
		size_t	loop;
		size_t	decodedLen = 0;

		for ( loop = 0; loop < nameLen; loop++ )
		{
			if ( (*basePtr)[loop + nameOffset] == '%' )
			{
				loop += 2;
			}
			decodedLen++;
		}

		return decodedLen;
	}
};

struct webServerHeader {
	uint8_t		**basePtr;
	bool		  staticName;
	char const	 *name = nullptr;
	ptrdiff_t	  nameOffset = 0;
	uint32_t	  nameLen = 0;
	ptrdiff_t	  valueOffset;
	uint32_t	  valueLen;

	webServerHeader ( uint8_t **basePtr, char const *name, uint32_t nameLen, uint8_t *value, uint32_t valueLen ) : basePtr ( basePtr ), staticName ( true ), name ( name ), nameLen (nameLen), valueOffset (value - *basePtr), valueLen(valueLen)
	{
	};

	webServerHeader( uint8_t **basePtr, ptrdiff_t nameOffset, uint32_t nameLen, uint8_t *value, uint32_t valueLen ) : basePtr( basePtr ), staticName ( false ), nameOffset( nameOffset ), nameLen( nameLen ), valueOffset( value - *basePtr ), valueLen( valueLen )
	{
	};

	webServerHeader ( void )
	{
		nameLen = 0;
		valueLen = 0;
	}

	bool operator == ( char const *s ) const
	{
		if ( staticName )
		{
			if ( !s[nameLen] )
			{
				if ( !memcmpi( name, s, nameLen) )
				{
					return true;
				}
			}
		} else
		{
			if ( !s[nameLen] )
			{
				if ( !memcmpi( (char *)(*basePtr + nameOffset), s, nameLen) )
				{
					return true;
				}
			}
		}
		return false;
	}

	bool operator == ( std::string const &s )  const
	{ 
		return *this == (char const *)s.c_str();
	};
	bool operator == ( stringi &s )  const
	{ 
		return *this == (char const *)s.c_str();
	};
	bool operator != ( char const *s ) const { return !(*this == s); };
	bool operator != ( std::string const &s ) const { return !(*this == (char const *)s.c_str()); };

	uint8_t operator [] ( size_t  index ) const
	{
		return (*basePtr + valueOffset)[index];
	}

	char const * getName ( void ) const
	{
		if ( staticName )
		{
			return name;
		} else
		{
			return (char *)(*basePtr + nameOffset);
		}
	}

	size_t getNameLen ( void ) const
	{
		return nameLen;
	}

	operator uint32_t() const
	{
		return valueLen;
	}

	operator char const *() const
	{
		return (char *)(*basePtr + valueOffset);
	}

	size_t cmpValuei ( char const *s ) const
	{
		int32_t		r = memcmpi ( *basePtr + valueOffset, s, valueLen);
		if ( !r )
		{
			if ( !s[valueLen] )
			{
				return 0;
			} return 1;
		} else return r;
	}

	size_t cmpValue ( char const *s ) const
	{
		int32_t		r = memcmp ( *basePtr + valueOffset, s, valueLen );
		if ( !r )
		{
			if ( !s[valueLen] )
			{
				return 0;
			} return 1;
		} else return r;
	}

};

struct webServerMultipart {
	std::vector<webServerRequestVar>	 vars;
	std::vector<webServerHeader>		 headers;
	size_t								 dataOffset;
	size_t								 dataLen;
	HANDLE								 spoolHandle;

	webServerMultipart()
	{
		headers.reserve( 4 );
		vars.reserve( 32 );
		spoolHandle = 0;
	}
};

struct webServerMethods {
	std::string			name;
	std::string			funcName;
	void			(*handler) ( class vmInstance *instance, class webProtocolH1 *conn  );

	webServerMethods() {};
	webServerMethods ( char const *name, char const *funcName, void (*handler) ( class vmInstance *instance, class webProtocolH1 *conn ) )
	{
		this->name = name;
		this->name += " ";
		this->funcName = funcName;
		this->handler = handler;
	}
	webServerMethods ( const webServerMethods &old )
	{
		name = old.name;
		funcName = old.funcName;
		handler = old.handler;
	}
};

typedef void (* webMimeHandler) ( class vmInstance *instance, class webProtocolH1 *conn );

struct webMimeType {
	webMimeHandler		handler;
	stringi				ext;
	stringi				compExt;
	stringi				contentType;
	stringi				contentDisposition;
	stringi				encoding;
	bool				compressable;

	webMimeType()
	{
		compressable = false;
	};

	webMimeType( webMimeHandler handler, char const *ext, char const*compExt, char const *contentType, char const *contentDisposition, char const *encoding )
	{
		this->handler				= handler;
		this->ext					= ext;
		this->compExt				= compExt;
		this->contentType			= contentType;
		this->contentDisposition	= contentDisposition;
		this->encoding				= encoding;
		this->compressable			= compExt[0] ? true : false;  // has a non-blank compressible file name extension so we mark this as compressible
	}

	webMimeType ( const webMimeType &old )
	{
		handler = old.handler;
		ext		= old.ext;
		compExt	= old.compExt;
		contentType = old.contentType;
		contentDisposition = old.contentDisposition;
		encoding = old.encoding;
		compressable = old.compressable;
	}
};

class webServerVar {
public:
	stringi name;
	stringi value;

	webServerVar ( stringi const &name, stringi const &value ) : name ( name ), value ( value ) {};
	webServerVar ( const webServerVar &old )
	{
		name = old.name;
		value = old.value;
	}
};

class webVirtualServer {
public:
	stringi											 serverName;			/* for logging purposes */
	stringi											 name;
	std::vector<stringi>							 alias;

	SRWLOCK											 lock;

	size_t											 refCount;

	bool											 passiveSecurity;

	std::vector<stringi>							 cgiDir;


	std::vector<transLog>							 logs;					/* vector of active logs */

	// engine size_terface stuff
	size_t											 maxExecuteTime;		/* maximum execution time for an ap page		*/

	// compression settings
	size_t											 compressAPMin;			/* minimum size AP file to compress	(maxint to disable */

	// general server information
	std::vector<webServerVar>						 vars;
	std::unordered_map<statString, webMimeType >	 mimeTypes;
	bool											 apDefaultAPS = true;

	size_t											 maxCookieSize;			/* max cookie size to accept from client											*/
	size_t											 maxRxContent;			/* maximum size we will accept from the client										*/
	stringi											 uploadDir;				/* root directory for all PUT method files											*/
	stringi											 rootName;				/* base path for HTML files... NO access below this... must be fully qualified		*/
	stringi											 defaultCache;			/* for items that have not had a specific cache policy set							*/		
	bool											 defaultRedir;			/* to redirect or return direct														*/
	bool											 doAltParamSearch;		/* enable embedded  www.xx.com/yy/param format										*/
	std::map<pathString, std::string>				 pathPageMap;			// to allow http://<domain>/<something>/file  mapping where <something> is an ap	*/
	std::unordered_map<statString, webServerMethods> methodHandlers;
	std::vector<stringi>							 defaultPages;

	// ssl stuff
	stringi											 certificateFile;
	stringi											 sslPassword;
	stringi											 keyFile;
	stringi											 chainFile;
	stringi											 cipherList;
	stringi											 cipherSuite;

	// apHandler stuff	- for remote (old engine) handling of ap to circumvent shit ssl code
	uint32_t										 ipAddress;
	uint16_t										 port;

	// auto-make support
	std::atomic<bool>								 apProjectNeedBuild = false;
	std::mutex										 apProjectBuildGuard;
	stringi											 apProjectBuildCommand;
	HANDLE											 apProjectMonitorHandle = 0;
	std::thread										 apProjectMonitorThread;
	void											 apProjectFileChangeMonitor ( void );
	SRWLOCK											 apProjectDirLock;

	struct apProjectDirEntry
	{
		OVERLAPPED					 ov;
		stringi						 directory;
		HANDLE						 dirHandle;
		union
		{
			struct _FILE_NOTIFY_INFORMATION	fi;
			char							data[4096];
		} dat;
		apProjectDirEntry ( char const *dir, HANDLE handle )
		{
			memset ( &ov, 0, sizeof ( ov ) );
			directory = dir;
			dirHandle = handle;
		}

		apProjectDirEntry ( apProjectDirEntry &&old )
		{
			*this = std::move ( old );
		}
		apProjectDirEntry &operator = ( apProjectDirEntry &&old ) noexcept
		{
			std::swap ( ov, old.ov );
			std::swap ( directory, old.directory );
			std::swap ( dat, old.dat );
			std::swap ( dirHandle, old.dirHandle );
			return *this;
		}
	};
	std::list<webVirtualServer::apProjectDirEntry>		 apProjectDir;		// monitored directories

	friend class webServerRequest;

	webVirtualServer (void)
	{
		InitializeSRWLock ( &lock );
		InitializeSRWLock ( &apProjectDirLock );
		mimeTypes.max_load_factor ( 1 );

		apProjectMonitorHandle = CreateIoCompletionPort ( INVALID_HANDLE_VALUE, 0, 0, 1 );	// NOLINT (performance-no-int-to-ptr)
		apProjectMonitorThread = std::thread ( &webVirtualServer::apProjectFileChangeMonitor, this );
	}

	webVirtualServer ( webVirtualServer const &old ) noexcept
	{
		try
		{
			InitializeSRWLock ( &lock );
			InitializeSRWLock ( &apProjectDirLock );
		} catch ( ... )
		{
		};

		serverName = old.serverName;
		name = old.name;
		alias = old.alias;
		refCount = 0;
		passiveSecurity = old.passiveSecurity;
		logs = old.logs;
		maxExecuteTime = old.maxExecuteTime;
		compressAPMin = old.compressAPMin;
		vars = old.vars;
		mimeTypes = old.mimeTypes;
		apDefaultAPS = old.apDefaultAPS;
		maxCookieSize = old.maxCookieSize;
		maxRxContent = old.maxRxContent;
		uploadDir = old.uploadDir;
		rootName = old.rootName;
		defaultRedir = old.defaultRedir;
		doAltParamSearch = old.doAltParamSearch;
		defaultCache = old.defaultCache;
		pathPageMap = old.pathPageMap;
		methodHandlers = old.methodHandlers;

		certificateFile = old.certificateFile;
		sslPassword = old.sslPassword;
		keyFile = old.keyFile;
		chainFile = old.chainFile;
		cipherList = old.cipherList;
		cipherSuite = old.cipherSuite;

		ipAddress = old.ipAddress;
		port = old.port;

		apProjectBuildCommand = old.apProjectBuildCommand;
		apProjectNeedBuild.store ( old.apProjectNeedBuild );
		apProjectMonitorHandle = CreateIoCompletionPort ( INVALID_HANDLE_VALUE, 0, 0, 1 );	// NOLINT (performance-no-int-to-ptr)
		apProjectMonitorThread = std::thread ( &webVirtualServer::apProjectFileChangeMonitor, this );
	}

	webVirtualServer ( webVirtualServer &&old ) noexcept
	{
		std::swap ( lock, old.lock );
		std::swap ( serverName, old.serverName );
		std::swap ( name, old.name );
		std::swap ( alias, old.alias );
		std::swap ( refCount, old.refCount );
		std::swap ( passiveSecurity, old.passiveSecurity );
		std::swap ( logs, old.logs );
		std::swap ( maxExecuteTime, old.maxExecuteTime );
		std::swap ( compressAPMin, old.compressAPMin );
		std::swap ( vars, old.vars );
		std::swap ( mimeTypes, old.mimeTypes );
		std::swap ( apDefaultAPS, old.apDefaultAPS );
		std::swap ( maxCookieSize, old.maxCookieSize );
		std::swap ( maxRxContent, old.maxRxContent );
		std::swap ( uploadDir, old.uploadDir );
		std::swap ( rootName, old.rootName );
		std::swap ( defaultRedir, old.defaultRedir );
		std::swap ( doAltParamSearch, old.doAltParamSearch );
		std::swap ( pathPageMap, old.pathPageMap );
		std::swap ( methodHandlers, old.methodHandlers );

		std::swap ( certificateFile, old.certificateFile );
		std::swap ( sslPassword, old.sslPassword );
		std::swap ( keyFile, old.keyFile );
		std::swap ( chainFile, old.chainFile );
		std::swap ( cipherList, old.cipherList );
		std::swap ( cipherSuite, old.cipherSuite );

		std::swap ( ipAddress, old.ipAddress );
		std::swap ( port, old.port );

		std::swap ( apProjectDirLock, old.apProjectDirLock );
		std::swap ( apProjectBuildCommand, old.apProjectBuildCommand );
		std::swap ( apProjectMonitorHandle, old.apProjectMonitorHandle );
		std::swap ( apProjectMonitorThread, old.apProjectMonitorThread );
		apProjectNeedBuild.store ( old.apProjectNeedBuild );
	}

	~webVirtualServer (void)
	{
		CloseHandle ( apProjectMonitorHandle );		// signal file watcher to shut down
		apProjectMonitorThread.join ();				// wait for it to shut down
	}


	class exclusiveLocker
	{
		SRWLOCK &lock;
	public:
		exclusiveLocker ( SRWLOCK &lock ) : lock ( lock )
		{
			AcquireSRWLockExclusive ( &lock );
		}
		~exclusiveLocker ()
		{
			ReleaseSRWLockExclusive ( &lock );
		}
	};

	class sharedLocker
	{
		SRWLOCK &lock;

	public:
		sharedLocker ( SRWLOCK &lock ) : lock ( lock )
		{
			AcquireSRWLockShared ( &lock );
		}
		~sharedLocker ()
		{
			ReleaseSRWLockShared ( &lock );
		}
	};


	void monitor ( char const *file )
	{
		HANDLE	handle;
		DWORD	nBytesTransferred;

		auto f = std::filesystem::path ( file );
		auto dir = std::filesystem::path ( f ).remove_filename ().generic_string ();
		{
			sharedLocker l1 ( apProjectDirLock );
			for ( auto &it : apProjectDir )
			{
				if ( !memcmpi ( it.directory.c_str (), dir.c_str (), it.directory.size () ) )
				{
					// all ready tracking changes (either in current directory or below
					return;
				}
			}

		}
		exclusiveLocker l1 ( apProjectDirLock );

		handle = CreateFile ( dir.c_str (),
							  FILE_LIST_DIRECTORY,
							  FILE_SHARE_READ | FILE_SHARE_WRITE,
							  NULL,
							  OPEN_EXISTING,
							  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
							  NULL
		);

		if ( handle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
		{
			return;
		}

		apProjectDir.push_back ( webVirtualServer::apProjectDirEntry ( dir.c_str (), handle ) );

		// associate it
		CreateIoCompletionPort ( handle, apProjectMonitorHandle, 0, 1 );

		ReadDirectoryChangesW ( handle,
								apProjectDir.back ().dat.data,
								sizeof ( apProjectDir.back ().dat.data ),
								TRUE,
								FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE,
								&nBytesTransferred,
								&(apProjectDir.back ().ov),
								0
		);
	}

public:
//	virtual	void							 lock();				/* disallow updates to the virtual server */
//	virtual void							 unLock();				/*( allow updates to the virtual server */
};

class webServerAuth {
public:
	enum {
		authIndividual,
		authGroup,
		authCallback
	}						type;

	bool					disabled;
	struct {
		struct {
			stringi			name;
			std::string		pw;
			stringi			basic;
		}					individual;
		struct {
			std::vector<class webServerAuth *>	groupList;
		}					group;
		struct {
			stringi			funcName;
		}					callback;
	};

	webServerAuth(){};
	webServerAuth ( const webServerAuth &old )
	{
		type = old.type;
		disabled = old.disabled;
		switch ( type )
		{
			case authIndividual:
				individual.name = old.individual.name;
				individual.pw = old.individual.pw;
				individual.basic = old.individual.basic;
				break;
			case authGroup:
				group.groupList = old.group.groupList;
				break;
			case authCallback:
				callback.funcName = old.callback.funcName;
				break;
		}
	}

	virtual ~webServerAuth () {};

	operator char *()
	{
		return (char *)individual.basic.c_str();
	}

	bool operator == ( webServerHeader *authHeader )
	{
		switch ( type )
		{
			case authIndividual:
				if ( individual.basic.length() == authHeader->valueLen )
				{
					return memcmp ( individual.basic.c_str(), *authHeader->basePtr + authHeader->valueOffset, authHeader->valueLen ) == 0;
				}
				return false;
			default:
				throw 500;
		}
	}
};

class webServerAuthRealm {
public:
	stringi							name;
	stringi							path;
	stringi							p3p;
	bool							disabled;
	std::vector<webServerAuth *>	accessList;

	webServerAuthRealm ( stringi const &name, stringi const &path, stringi const &p3p, bool disabled ) : name ( name ), path ( path ), p3p ( p3p ), disabled ( disabled ) {};

	webServerAuthRealm ( const webServerAuthRealm &old )
	{
		name = old.name;
		path = old.path;
		p3p = old.p3p;
		disabled = old.disabled;
		accessList = old.accessList;
	}
};

struct webChunkedRxBuff
{
	struct webChunkedRxBuff	*next;

	enum
	{
		wsParseBodyChunkedMemoryLen,
		wsParseBodyChunkedMemoryContent,
	}						 teState;

	size_t					 chunkLength;
	size_t					 chunkRcvdLength;
	uint8_t					*chunkedAssemblyBuffer;
	size_t					 chunkedAssemblyBufferSize;

	webChunkedRxBuff( void )
	{
		chunkedAssemblyBufferSize = 65536;
		chunkedAssemblyBuffer = (uint8_t *)malloc( sizeof( uint8_t ) * chunkedAssemblyBufferSize );
	}
	~webChunkedRxBuff ()
	{
		free ( chunkedAssemblyBuffer );
	}
};

class webServer {
public:
	stringi												 serverName;
	stringi												 configFile;

	size_t												 numHeaderPages = 2;
	uint32_t											 listenAddrV4;
	uint32_t											 listenPort;

	uint32_t											 maxConnections;
	uint32_t											 minAcceptsOutstanding;

	SRWLOCK												 virtualServerAccessLock;
	std::vector<webVirtualServer *>						 virtualServers;
	std::unordered_map<statString, webVirtualServer *>	 virtualServerMap;

	webVirtualServer									 virtualServerDefault;					/* for http 0.9 requests (no host field)  */

	std::vector<webServerAuth *>						 authEntries;							/* we store ponters in the structures... this is to minimize duplication and allow easy cleanup */
	std::vector<webServerAuthRealm *>					 realms;
	std::map<pathString, webServerAuthRealm *>			 realmPathMap;

	uint32_t											 keepAliveTimeout;

	time_t												 startTime;

	class vmTaskInstance								*instance;								/* instance to get preloaded stuff from */

	// ssl stuff
	bool												 isSecure;

	HANDLE												 iocpHandle;

	// counters
	int64_t volatile									 totalBytesSent;
	int64_t volatile 									 totalBytesReceived;
	LONG	volatile									 totalConnections;

	// global buffers
	struct webChunkedRxBuff								*chunkFree;
	CRITICAL_SECTION									 chunkFreeCrit;
	serverBuffer										*serverBuffFree;
	CRITICAL_SECTION									 serverBuffFreeCrit;

	// debugging stuff
	bool												 printRequestHeaders = false;
	bool												 printResponseHeaders = false;

	uint8_t												*h2Settings;
	size_t												 h2SettingsLen;

	webServer ( char const *configName, HANDLE iocpHandle );

	virtual ~webServer ( void )
	{
		for ( auto vsIt = virtualServers.begin(); vsIt != virtualServers.end(); vsIt++ )
		{
			delete *vsIt;
		}

		for ( auto aIt = authEntries.begin(); aIt != authEntries.end(); aIt++ )
		{
			delete (*aIt);
		}

		for ( auto rIt = realms.begin(); rIt != realms.end(); rIt++ )
		{
			delete *rIt;
		}

		while ( chunkFree )
		{
			auto cur = chunkFree;
			chunkFree = chunkFree->next;
			delete cur;
		}

		while ( serverBuffFree )
		{
			auto cur = serverBuffFree;
			serverBuffFree = serverBuffFree->next;
			delete cur;
		}

		DeleteCriticalSection( &chunkFreeCrit );
		DeleteCriticalSection( &serverBuffFreeCrit );
	}

	webChunkedRxBuff		*getChunkBuff( void )
	{
		EnterCriticalSection( &chunkFreeCrit );

		if ( chunkFree )
		{
			auto ret = chunkFree;
			chunkFree = chunkFree->next;

			LeaveCriticalSection( &chunkFreeCrit );
			return ret;
		}
		LeaveCriticalSection( &chunkFreeCrit );

		return new webChunkedRxBuff();
	}

	void freeChunkBuff( webChunkedRxBuff  *buff )
	{
		EnterCriticalSection( &chunkFreeCrit );

		buff->next = chunkFree;
		chunkFree = buff;
		LeaveCriticalSection( &chunkFreeCrit );
	}

	webVirtualServer		*getVirtualServerDefault ( )
	{
		AcquireSRWLockShared ( &virtualServerAccessLock );
		AcquireSRWLockShared ( &virtualServerDefault.lock );
		virtualServerDefault.refCount++;
		ReleaseSRWLockShared ( &virtualServerAccessLock );
		return &virtualServerDefault;
	}

	webVirtualServer		*getVirtualServer	( uint8_t *host, size_t len )
	{
		AcquireSRWLockShared ( &virtualServerAccessLock );

		if ( host[len - 1] == '.' )
		{
			len--;
		}

		auto it = virtualServerMap.find ( statString ( (char*)host, len ) );

		if ( it == virtualServerMap.end() )
		{
			AcquireSRWLockShared ( &virtualServerDefault.lock );
			virtualServerDefault.refCount++;
			ReleaseSRWLockShared ( &virtualServerAccessLock );
			return &virtualServerDefault;
		}
		(*it).second->refCount++;
		AcquireSRWLockShared ( &(*it).second->lock );
		ReleaseSRWLockShared ( &virtualServerAccessLock );
		return (*it).second;
	}

	void					 freeVirtualServer	( webVirtualServer *server )
	{
		AcquireSRWLockShared ( &virtualServerAccessLock );
		ReleaseSRWLockShared ( &server->lock );
		server->refCount--;
		ReleaseSRWLockShared ( &virtualServerAccessLock );
	}

	void					 compileAp			( char const *inName, char const *outName, bool isSlang = true );

	void					 execAp				( class vmInstance *instance, webProtocolH1 *conn, char const *fName, fileCacheEntry *entry );
};


extern size_t	serverBuildName				( std::string const &root, char const *name, char *dest );

extern void		webActivePageHandler		( class vmInstance *instance, class webProtocolH1 *conn );
extern void		webStaticPageHandler		( class vmInstance *instance, class webProtocolH1 *conn );
extern void		webGetPostMethodHandler		( class vmInstance *instance, class webProtocolH1 *conn );
extern void		webHeadMethodHandler		( class vmInstance *instance, class webProtocolH1 *conn );
extern void		webCustomMethodHandler		( class vmInstance *instance, class webProtocolH1 *conn );
extern void		webTraceMethodHandler		( class vmInstance *instance, class webProtocolH1 *conn );

extern webMimeType	*apxMimeType;
extern webMimeType	*htmMimeType;
extern webMimeType	*htmlMimeType;

extern char *webContentDisposition[];
