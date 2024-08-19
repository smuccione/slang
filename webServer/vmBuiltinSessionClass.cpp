#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"

#include "Target/vmTask.h"
#include "Target/fileCache.h"

#include "webServer.h"
#include "webSocket.h"
#include "webProtoSSL.h"
#include "webProtoH1.h"
#include "webProtoH2.h"
#include "webServerISAPIInterface.h"

#include "bcVM/vmNativeInterface.h"			// MUST come after windows... it undefines CONST and reutilizes the #def macro

static void cSessionNew ( vmInstance *instance, VAR *obj )
{
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	if ( !task->getCargo<webProtocolH1 *> ( ) )
	{
		return;
		throw errorNum::scINVALID_PARAMETER;
	}
}

static VAR_STR cRequestPostData ( vmInstance *instance, VAR *obj, size_t part )

{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( !part-- ) throw errorNum::scINVALID_PARAMETER;

	if ( part < conn->req.multiParts.size ( ) )
	{
		return VAR_STR ( instance, (char *)(conn->req.rxBuffer.getBuffer ( ) + conn->req.multiParts[part].dataOffset), conn->req.multiParts[part].dataLen );
	} else
	{
		throw errorNum::scINVALID_PARAMETER;
	}
}

static void cRequestWritePostData ( vmInstance *instance, VAR *obj, char const *fName, size_t part )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( !part-- ) throw errorNum::scINVALID_PARAMETER;

	if ( part < conn->req.multiParts.size ( ) )
	{
		HANDLE fHandle;

		fHandle = CreateFile ( fName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
		if ( fHandle == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
		{
			throw GetLastError ( );
		}

		size_t len = conn->req.multiParts[part].dataLen;
		uint8_t *ptr = conn->req.rxBuffer.getBuffer ( ) + conn->req.multiParts[part].dataOffset;

		while ( len )
		{
			DWORD writeSize = len < 1024ULL * 1024 ? (DWORD) len : 1024UL * 1024;
			DWORD nWritten;

			if ( !WriteFile ( fHandle, ptr, writeSize, &nWritten, 0 ) )
			{
				CloseHandle ( fHandle );
				throw GetLastError ( );
			}
			len -= nWritten;
			ptr += nWritten;
		}
		CloseHandle ( fHandle );
	} else
	{
		throw errorNum::scINVALID_PARAMETER;
	}
}

static VAR_STR cRequestBody ( vmInstance *instance, VAR *obj )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	return VAR_STR ( instance, (char const *)conn->req.rxBuffer.getBuffer ( ), conn->req.rxBuffer.getSize ( ) );
}

static VAR_STR cRequestHeaders ( vmInstance *instance, VAR *obj )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	return VAR_STR ( instance, (char const *) conn->req.headerBuffer, conn->req.headerBufferLen - 2 );	// remove final \r\n (makes appending easier)
}

static int32_t cRequestNumParts ( vmInstance *instance, VAR *obj )

{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	return static_cast<int32_t>(conn->req.multiParts.size ( ));
}

static VAR_STR cRequestVar ( vmInstance *instance, VAR *obj, char const *name, size_t part )
{
	VAR_STR ret;

	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( !part-- ) 
		throw errorNum::scINVALID_PARAMETER;

	if ( part < conn->req.multiParts.size ( ) )
	{
		for ( auto &it : conn->req.multiParts[part].vars )
		{
			if ( it == name )
			{
				char *ret = (char *) instance->om->alloc ( (size_t) it + 1 );
				it.getValue ( (uint8_t *) ret );
				ret[(size_t) it] = 0;
				return VAR_STR ( ret, (size_t) it );
		}
		}
	}
	return VAR_STR ( "", 0 );
}

static VAR_STR cRequestVarX ( vmInstance *instance, VAR *obj, char const *name, size_t part )
{
	VAR_STR ret;

	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( !part-- ) 
		throw errorNum::scINVALID_PARAMETER;

	if ( part < conn->req.multiParts.size ( ) )
	{
		for ( auto &it : conn->req.multiParts[part].vars )
		{
			if ( it == name )
			{
				char *ret = (char *) instance->om->alloc ( (size_t) it + 1 );
				it.getValue ( (uint8_t *) ret );
				ret[(size_t) it] = 0;

				if ( !checkScript ( ret ) )
				{
					// our string is built and this isn't some code injection issue so finally set the result to be a string rather than a null
					return VAR_STR ( ret, (size_t) it );
				}
				// code injection.. break and return a 0 length string;
				break;
			}
		}
	}
	return VAR_STR ( "", 0 );
}

static uint32_t cRequestVarExists ( vmInstance *instance, VAR *obj, char const *name, size_t part )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( !part-- ) throw errorNum::scINVALID_PARAMETER;

	if ( part < conn->req.multiParts.size ( ) )
	{
		for ( auto &it : conn->req.multiParts[part].vars )
		{
			if ( it == name )
			{
				return 1;
			}
		}
	}
	return 0;
}

static VAR cRequestVarArray ( vmInstance *instance, VAR *obj, size_t part )
{
	if ( !part-- ) throw errorNum::scINVALID_PARAMETER;

	auto	task	= static_cast<vmTaskInstance *>(instance);
	auto	conn	= task->getCargo<webProtocolH1 *> ( );
	VAR		arr		= VAR_ARRAY ( instance );

	if ( part < conn->req.multiParts.size ( ) )
	{
		int64_t index = 1;
		
		for ( auto &it : conn->req.multiParts[part].vars )
		{
			char *name = (char *) instance->om->alloc ( it.getNameLen ( ) + 1 );
			it.getName ( (uint8_t *) name );
			name[it.getNameLen ( )] = 0;

			arrayGet ( instance, &arr, index, 1 ) = VAR_STR ( name, it.getNameLen ( ) );

			char *value = (char *) instance->om->alloc ( it.getValueLen ( ) + 1 );
			it.getValue ( (uint8_t *) value );
			value[it.getValueLen ( )] = 0;

			if ( checkScript ( value ) )
			{
				// returned value is executable (script tags)... 
				arrayGet ( instance, &arr, index, 2 ) = VAR_NULL ( );
			} else
			{
				arrayGet ( instance, &arr, index, 2 ) = VAR_STR ( value, it.getValueLen ( ) );
			}
			index++;
		}
	}
	return arr;
}

static VAR cRequestVarSet ( vmInstance *instance, VAR *obj, char *name, char *value, size_t part )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( !part-- ) throw errorNum::scINVALID_PARAMETER;

	if ( part < conn->req.multiParts.size ( ) )
	{
		auto valueLen = strlen ( value );
		auto nameLen = strlen ( name );
		for ( auto it = conn->req.multiParts[part].vars.begin(); it != conn->req.multiParts[part].vars.end(); it++ )
		{
			if ( *it == name )
			{
				auto nameLoc = conn->req.rxBuffer.getBuffer ();
				conn->req.rxBuffer.write ( name, nameLen );
				auto valueLoc = conn->req.rxBuffer.getBuffer ();
				conn->req.rxBuffer.write ( value, valueLen );
				conn->req.multiParts[part].vars.push_back ( webServerRequestVar ( conn->req.rxBuffer.getBufferDataPtr (), nameLoc, nameLen, valueLoc, valueLen ) );
			}
		}
	}
	return false;
}

static bool cRequestVarDelete ( vmInstance *instance, VAR *obj, char const *name, size_t part )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( !part-- ) throw errorNum::scINVALID_PARAMETER;

	if ( part < conn->req.multiParts.size ( ) )
	{
		for ( auto it = conn->req.multiParts[part].vars.begin(); it != conn->req.multiParts[part].vars.end(); it++ )
		{
			if ( *it == name )
			{
				conn->req.multiParts[part].vars.erase ( it );
				return true;
			}
		}
	}
	return false;
}

static VAR_STR cRequestData ( vmInstance *instance, VAR *obj, char const *name )
{
	VAR_STR				 ret;

	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	for ( auto &it : conn->req.virtualServer->vars )
	{
		if ( it.name == name )
		{
			return VAR_STR ( instance, it.value.c_str(), it.value.size() );
		}
	}

	char	 tmpBuff[4096];
	size_t	 len;

	// no.. check for a header variable
	len = sizeof ( tmpBuff );
	if ( GetServerVariable ( task, name, tmpBuff, len ) )
	{
		tmpBuff[sizeof ( tmpBuff ) - 1] = 0;
		return VAR_STR ( instance, tmpBuff, strlen ( tmpBuff ) );
	}
	return VAR_STR ( instance, "", 0 );
}

static VAR_STR cRequestHeader ( vmInstance *instance, VAR *obj, VAR *name )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	for ( auto &it : conn->req.headers )
	{
		if ( it == name->dat.str.c )
		{
			return VAR_STR ( instance, it, it );
		}
	}
	return VAR_STR ( instance, "", 0 );
}

class sessionHeaderValueInspector : public vmDebugVar 
{
	webServerHeader		*header;

	public:
	sessionHeaderValueInspector ( webServerHeader *header )
	{
		this->header = header;
	}

	char const *getName ( void )
	{
		return header->getName ( );
	}
	size_t getNameLen ( void )
	{
		return header->getNameLen ( );
	}
	char const *getValue ( void )
	{
		return *header;
	}
	size_t getValueLen ( void )
	{
		return *header;
	}
	stringi getType ( void )
	{
		return "string";
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
	{
		throw errorNum::scINTERNAL;
	}
	bool hasChildren ( void )
	{
		return false;
	}
	bool isStringValue ( void )
	{
		return true;
	}
};

class sessionHeaderInspector : public vmDebugVar {
	vmTaskInstance					*task;
	std::vector<webServerHeader>	*headers;

	public:
	sessionHeaderInspector ( vmTaskInstance *task, std::vector<webServerHeader>	*headers )
	{
		this->task = task;
		this->headers = headers;
	}

	char const *getName ( void )
	{
		return "Headers";
	}
	size_t getNameLen ( void )
	{
		return 7;	// lenght of "Headers"		- output of getName()
	}
	char const *getValue ( void )
	{
		return "";
	}
	size_t getValueLen ( void )
	{
		return 0;
	}
	stringi getType ()
	{
		return "string";
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
	{
		DWORD			 loop;
		vmInspectList	*iList;
		char			 name[256];

		iList = new vmInspectList ( );

		for ( loop = 0; loop < headers->size ( ); loop++ )
		{
			sprintf_s ( name, sizeof ( name ), "%.*s", (uint32_t) (*headers)[loop].getNameLen ( ), (*headers)[loop].getName ( ) );
			iList->entry.push_back ( static_cast<vmDebugVar *>(new sessionHeaderValueInspector ( &(*headers)[loop] )) );
		}

		return iList;
	}
	bool hasChildren ( void )
	{
		if ( headers->size ( ) ) return true;
		return false;
	}
	bool isStringValue ( void )
	{
		return false;
	}
};

class sessionDataInspector : public vmDebugVar {
	char const	*name;
	size_t		 nameLen;
	char const	*value;
	size_t		 valueLen;
	bool		 doFree = false;
	public:
	~sessionDataInspector ( )
	{
		if ( doFree )
		{
			free ( (char *) value );
			free ( (char *) name );
		}
	}

	sessionDataInspector ( char const *name, size_t nameLen, char const *value, size_t valueLen, bool doFree = true ) : doFree ( doFree )
	{
		this->name = name;
		this->nameLen = nameLen;
		this->value = value;
		this->valueLen = valueLen;
	}

	char const			*getName ( void )
	{
		return name;
	}
	size_t			 getNameLen ( void )
	{
		return nameLen;
	}
	bool				 isStringValue ( void )
	{
		return true;
	}
	char const			*getValue ( void )
	{
		return value;
	}
	size_t			 getValueLen ( void )
	{
		return valueLen;
	}
	stringi			 getType()
	{
		return "string";
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
	{
		throw errorNum::scINTERNAL;
	}
	bool				 hasChildren ( void )
	{
		return false;
	}
};

class sessionCookieInspector : public vmDebugVar {
	webProtocolH1	*conn;
	vmTaskInstance		*task;

	public:
	sessionCookieInspector ( vmTaskInstance *task, webProtocolH1 *conn )
	{
		this->task = task;
		this->conn = conn;
	}

	char const *getName ( void )
	{
		return "Cookies";
	}
	size_t getNameLen ( void )
	{
		return 7;
	}
	char const *getValue ( void )
	{
		return "";
	}
	size_t getValueLen ( void )
	{
		return 0;
	}
	stringi getType ()
	{
		return "string";
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
	{
		DWORD				 loop;
		vmInspectList		*iList;

		iList = new vmInspectList ( );

		for ( loop = 0; loop < conn->req.cookies.size ( ); loop++ )
		{
			char	*value;
			char	*name;

			name = (char *) malloc ( conn->req.cookies[loop].getNameLen ( ) + 1 );
			value = (char *) malloc ( conn->req.cookies[loop].getValueLen ( ) + 1 );

			conn->req.cookies[loop].getName ( name );
			conn->req.cookies[loop].getValue ( (uint8_t *) value );
			name[conn->req.cookies[loop].getNameLen ( )] = 0;
			value[conn->req.cookies[loop].getValueLen ( )] = 0;

			iList->entry.push_back ( static_cast<vmDebugVar *>(new sessionDataInspector ( name, conn->req.cookies[loop].getNameLen ( ), value, conn->req.cookies[loop].getValueLen ( ), false )) );
		}

		return iList;
	}
	bool hasChildren ( void )
	{
		return conn->req.cookies.size ( ) ? true : false;
	}
	bool isStringValue ( void )
	{
		return false;
	}
};

class sessionMultipartInspector : public vmDebugVar {
	webProtocolH1	*conn;
	vmTaskInstance		*task;
	size_t			 part;
	char				 name[256];

	public:
	sessionMultipartInspector ( vmTaskInstance *task, webProtocolH1 *conn, size_t part )
	{
		this->task = task;
		this->conn = conn;
		this->part = part;

		if ( conn->req.multiPartBoundary )
		{
			sprintf_s ( name, sizeof ( name ), "Part %d", (uint32_t) part + 1 );
		} else if ( conn->req.contentLengthValid && conn->req.contentLength )
		{
			sprintf_s ( name, sizeof ( name ), "Body" );
		} else
		{
			sprintf_s ( name, sizeof ( name ), "URL" );
		}
	}

	char const *getName ( void )
	{
		return name;
	}
	size_t getNameLen ( void )
	{
		return strlen ( name );
	}
	char const *getValue ( void )
	{
		return "";
	}
	size_t getValueLen ( void )
	{
		return 0;
	}
	stringi getType ()
	{
		return "string";
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
	{
		DWORD			 loop;
		vmInspectList	*iList;

		iList = new vmInspectList ( );

		for ( loop = 0; loop < conn->req.multiParts[part].vars.size ( ); loop++ )
		{
			char	*value;
			char	*name;

			name = (char *) malloc ( conn->req.multiParts[part].vars[loop].getNameLen ( ) + 1 );
			value = (char *) malloc ( conn->req.multiParts[part].vars[loop].getValueLen ( ) + 1 );

			conn->req.multiParts[part].vars[loop].getName ( (uint8_t *) name );
			conn->req.multiParts[part].vars[loop].getValue ( (uint8_t *) value );
			name[conn->req.multiParts[part].vars[loop].getNameLen ( )] = 0;
			value[conn->req.multiParts[part].vars[loop].getValueLen ( )] = 0;

			iList->entry.push_back ( static_cast<vmDebugVar *>(new sessionDataInspector ( name, conn->req.multiParts[part].vars[loop].getNameLen ( ), value, conn->req.multiParts[part].vars[loop].getValueLen ( ), true )) );
		}
		if ( conn->req.multiParts[part].dataLen )
		{
			iList->entry.push_back ( static_cast<vmDebugVar *>(new sessionDataInspector ( "DATA", 4, (char *) conn->req.rxBuffer.getBuffer ( ) + conn->req.multiParts[part].dataOffset, conn->req.multiParts[part].dataLen, false )) );
		}
		if ( conn->req.multiParts[part].headers.size ( ) )
		{
			iList->entry.push_back ( static_cast<vmDebugVar *>(new sessionHeaderInspector ( task, &conn->req.multiParts[part].headers )) );
		}
		return iList;
	}
	bool hasChildren ( void )
	{
		return true;
	}
	bool isStringValue ( void )
	{
		return false;
	}
};

class sessionBodyInspector : public vmDebugVar {
	webProtocolH1	*conn;
	vmTaskInstance		*task;

	public:
	sessionBodyInspector ( vmTaskInstance *task, webProtocolH1 *conn )
	{
		this->task = task;
		this->conn = conn;
	}

	char const *getName ( void )
	{
		return "Data";
	}
	size_t getNameLen ( void )
	{
		return 4;
	}
	char const *getValue ( void )
	{
		return "";
	}
	size_t getValueLen ( void )
	{
		return 0;
	}
	stringi getType ()
	{
		return "string";
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )
	{
		DWORD				 loop;
		vmInspectList		*iList;

		iList = new vmInspectList ( );

		for ( loop = 0; loop < conn->req.multiParts.size ( ); loop++ )
		{
			iList->entry.push_back ( static_cast<vmDebugVar *>(new sessionMultipartInspector ( task, conn, loop )) );
		}

		return iList;
	}
	bool hasChildren ( void )
	{
		return true;
	}
	bool isStringValue ( void )
	{
		return false;
	}
};

static vmInspectList *cSessionInspector ( class vmInstance *instance, bcFuncDef *func, VAR *obj, size_t start, size_t end )

{
	vmInspectList	*vars;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);

	webProtocolH1	*conn;

	conn = task->getCargo<webProtocolH1 *> ( );

	vars = new vmInspectList ( );

	if ( conn )
	{
		// create the individual inspectors for header's, body (multiparts), and cookies
		vars->entry.push_back ( static_cast<vmDebugVar *>(new sessionHeaderInspector ( task, &conn->req.headers )) );
		vars->entry.push_back ( static_cast<vmDebugVar *>(new sessionBodyInspector ( task, conn )) );
		if ( conn->req.cookies.size ( ) ) vars->entry.push_back ( static_cast<vmDebugVar *>(new sessionCookieInspector ( task, conn )) );
		vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, "Request", const_cast<const uint8_t *>(conn->req.headerBuffer), conn->req.headerBufferLen )) );
	}

	return vars;
}

static vmInspectList *sessionTab ( class vmInstance *instance, bcFuncDef *funcDef, fglOp *op, VAR *stack, vmDebugVar *var )
{
	return cSessionInspector ( instance, funcDef, 0, 0, 0 );
}

static VAR_STR cRequestURL ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.url )
	{
		return VAR_STR ( instance, "", 0 );
	}

	return VAR_STR ( instance, conn->req.headers[conn->req.url], conn->req.headers[conn->req.url] );
}

static VAR_STR cRequestMethod( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	return VAR_STR ( instance, conn->req.method->name.c_str(), conn->req.method->name.size() - 1 );		// internally there's a trailing space so remove it here
}

static VAR_STR cRequestHost ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.host )
	{
		return VAR_STR ( instance, "", 0 );
	}

	return VAR_STR ( instance, conn->req.headers[conn->req.host], conn->req.headers[conn->req.host] );
}

static VAR_STR cRequestFrom ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.from )
	{
		return VAR_STR ( instance, "", 0 );
	}

	return VAR_STR ( instance, conn->req.headers[conn->req.from], conn->req.headers[conn->req.from] );
}


static VAR_STR cRequestUserAgent ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.userAgent )
	{
		return VAR_STR ( instance, "", 0 );
	}

	return VAR_STR ( instance, conn->req.headers[conn->req.userAgent], conn->req.headers[conn->req.userAgent] );
}

static VAR_STR cRequestContentType ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.contentType )
	{
		return VAR_STR ( instance, "", 0 );
	}

	return VAR_STR ( instance, conn->req.headers[conn->req.contentType], conn->req.headers[conn->req.contentType] );
}

static VAR_STR cRequestReferer ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.referer )
	{
		return VAR_STR ( instance, "", 0 );
	}

	return VAR_STR ( instance, conn->req.headers[conn->req.referer], conn->req.headers[conn->req.referer] );
}

static VAR_STR cRequestDomain ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	return VAR_STR ( instance, conn->req.virtualServer->name );
}

static VAR_STR cRequestRootDirectory ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1 *conn;
	vmTaskInstance *task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	return VAR_STR ( conn->req.virtualServer->rootName );
}

static VAR_STR cRequestUser ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.authorization )
	{
		return VAR_STR ( instance, "", 0 );
	}

	auto sep = ( char const *) conn->req.headers[conn->req.authorization];

	while ( *sep && sep[0] == ' ' ) sep++;
	if ( !memcmp ( sep, "Basic ", 6 ) )
	{
		sep += 6;
		while ( *sep && sep[0] == ' ' ) sep++;

		sep = strchr ( sep, ':' );
		if ( !sep )
		{
			return VAR_STR ( instance, "", 0 );
		}
		return VAR_STR ( instance, conn->req.headers[conn->req.authorization], sep - (char const *) conn->req.headers[conn->req.authorization] );

		return VAR_STR ( instance, conn->req.headers[conn->req.authorization], conn->req.headers[conn->req.authorization] );
	} else if ( !memcmp ( sep, "Digest ", 7 ) )
	{
		//TODO support digest authentication
	}
	return VAR_STR ( instance, "", 0 );
}

static VAR_STR cRequestPW ( class vmInstance *instance, VAR *obj )
{
	webProtocolH1	*conn;
	vmTaskInstance	*task;

	task = static_cast<vmTaskInstance *>(instance);
	conn = task->getCargo<webProtocolH1 *> ( );

	VAR_STR ret;

	if ( !conn->req.authorization )
	{
		return VAR_STR ( instance, "", 0 );
	}

	auto sep = ( char const *) conn->req.headers[conn->req.authorization];

	while ( *sep && sep[0] == ' ' ) sep++;
	if ( !memcmp ( sep, "Basic ", 6 ) )
	{
		sep += 6;
		while ( *sep && sep[0] == ' ' ) sep++;

		sep = strchr ( sep, ':' );
		if ( !sep )
		{
			return VAR_STR ( instance, "", 0 );
		}
		return VAR_STR ( instance, sep, strlen ( sep + 1 ) );
	} else if ( !memcmp ( sep, "Digest ", 7 ) )
	{
		//TODO support digest authentication
	}
	return VAR_STR ( instance, "", 0 );
}

static VAR_STR cResponseRedirect ( vmInstance *instance, VAR *obj, VAR_STR *loc )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	std::string name = loc->dat.str.c;

	if ( _ati ( "://", const_cast<char *>(name.c_str()) ) != -1 )
	{
	} else if ( (name.c_str()[0] != '\\') && (name.c_str ( )[0] != '/') )
	{
		name = std::string ( "\\" ) + name;
	}

	VAR_STR var ( instance, name );
	auto ret = static_cast<VAR_STR *>(instance->getGlobal ( task->getName ( ), "__outputString" ));
	*ret = var;

	conn->rsp.rspCode = 302;

	return *ret;
}

static VAR cResponseSetCompleteResponse ( vmInstance *instance, VAR *obj, VAR_STR *body, VAR_STR *headers )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	auto str = body->dat.str.c;

	if ( !instance->om->isInGCMemory ( (void *)body->dat.str.c ) )
	{
		str = (char const *)instance->om->alloc ( body->dat.str.len + 1 );
		memcpy ( (void *)str, body->dat.str.c, body->dat.str.len + 1 );
	}
	/* convert the body into a static buffer to make it involatile */
	PUSH ( VAR_STR ( str, body->dat.str.len ) );
	auto newBuffer = classNew ( instance, "$staticBuffer", 1 );
	POP ( 1 );

	auto ret = static_cast<VAR *>(instance->getGlobal ( task->getName ( ), "__outputString" ));
	*ret = *newBuffer;

	if ( headers->type == slangType::eSTRING && headers->dat.str.len )
	{
		conn->rsp.headers.reset ();
		conn->rsp.headers.write ( headers->dat.str.c, headers->dat.str.len );
		conn->rsp.body.bodyType = webServerResponse::body::btObjectPageComplete;
	} else
	{
		conn->rsp.body.bodyType = webServerResponse::body::btObjectPage;
	}

	return *ret;
}

size_t cSessionServerTotalBytesSent ( class vmInstance *instance, VAR *obj )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	return conn->server->totalBytesSent;
}

size_t cSessionServerTotalBytesReceived ( class vmInstance *instance, VAR *obj )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	return conn->server->totalBytesReceived;
}

size_t cSessionServerTotalConnections ( class vmInstance *instance, VAR *obj )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	return conn->server->totalConnections;
}

VAR_STR cSessionServerName ( class vmInstance *instance, VAR *obj )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	return VAR_STR ( conn->server->serverName.c_str ( ) );
}

VAR_STR cRequestGetCookie ( class vmInstance *instance, VAR *obj, VAR_STR *name )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	for ( auto it = conn->req.cookies.begin ( ); it != conn->req.cookies.end ( ); it++ )
	{
		if ( (*it) == name->dat.str.c )
		{
			auto len = (*it).getValueLen ( );
			uint8_t *value = (uint8_t *) instance->om->alloc ( len + 1 );

			(*it).getValue ( value );
			value[len] = 0;

			return VAR_STR ( (char *) value, len );
		}
	}
	return VAR_STR ( "" );
}

VAR_STR cRequestGetCookieRaw ( class vmInstance *instance, VAR *obj, VAR_STR *name )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	for ( auto it = conn->req.cookies.begin ( ); it != conn->req.cookies.end ( ); it++ )
	{
		if ( (*it) == name->dat.str.c )
		{
			auto len = (*it).getValueLenRaw ( );
			uint8_t *value = (uint8_t *) instance->om->alloc ( len + 1 );

			(*it).getValueRaw ( value );
			value[len] = 0;

			return VAR_STR ( (char *) value, len );
		}
	}
	return VAR_STR ( "" );
}

void cResponseSetCookie ( class vmInstance *instance, VAR *obj, VAR_STR *name, VAR_STR *data, VAR_STR *path, VAR_STR *expires, uint32_t secure, uint32_t httpOnly )
{
	char cookieBuff[4096 + 1024] = "";	// 1K for name, path, expires, secure & httpOnly... the rest for data

	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	uint32_t dataLen = (uint32_t) ((data->dat.str.len - 1) / 3 + 1) * 4; /* round len to greatest 3rd and multiply by 4 */

	if ( name->dat.str.len + dataLen + path->dat.str.len + expires->dat.str.len + 64 + 1 > sizeof ( cookieBuff ) )	// 64 is all max defaults and sepearator sizes
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	auto outPtr = cookieBuff;
	outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "%s=", name->dat.str.c );

	auto in = (uint8_t *) data->dat.str.c;

	for ( uint32_t loop = 0; loop < data->dat.str.len; loop += 3 )
	{
		outPtr[0] = static_cast<char>(webBase64EncodingTable[(in[0] >> 2)]);
		outPtr[1] = static_cast<char>(webBase64EncodingTable[((in[0] & 0x03) << 4) | (in[1] >> 4)]);
		outPtr[2] = static_cast<char>(webBase64EncodingTable[((in[1] & 0x0F) << 2) | (in[2] >> 6)]);
		outPtr[3] = static_cast<char>(webBase64EncodingTable[((in[2] & 0x3F))]);
		/* increment our pointers appropriately */
		outPtr += 4;
		in += 3;
	}

	/* fill in "null" character... all 0's in important bits */
	switch ( data->dat.str.len % 3 )
	{
		case 1:
			outPtr[-2] = '=';
			// fall through 
		case 2:
			outPtr[-1] = '=';
			break;
		case 0:
		default:
			break;
	}

	if ( data->dat.str.len )
	{
		if ( expires->dat.str.len )
		{
			outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; expires=%s", expires->dat.str.c );
		}
	} else
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; expires=Tue 01-Jan-1970 00:00:00 GMT" );
	}
	if ( path->dat.str.len )
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; path=%s", path->dat.str.c );
	}
	if ( httpOnly )
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; httponly" );
	}
	if ( secure )
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; secure" );
	}
	outPtr[0] = 0;

	conn->rsp.extraHeaders.push_back ( webExtraHeaders ( "Set-Cookie", cookieBuff ) );
}

void cResponseSetCookieRaw ( class vmInstance *instance, VAR *obj, VAR_STR *name, VAR_STR *data, VAR_STR *path, VAR_STR *expires, uint32_t secure, uint32_t httpOnly )
{
	char cookieBuff[4096 + 1024]{};	// 1K for name, path, expires, secure & httpOnly... the rest for data

	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	size_t dataLen = data->dat.str.len;

	if ( name->dat.str.len + dataLen + path->dat.str.len + expires->dat.str.len + 64 + 1 > sizeof ( cookieBuff ) )	// 64 is all max defaults and sepearator sizes
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	auto outPtr = cookieBuff;
	outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "%s=%s", name->dat.str.c, data->dat.str.c );

	if ( data->dat.str.len )
	{
		if ( expires->dat.str.len )
		{
			outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; expires=%s", expires->dat.str.c );
		}
	} else
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; expires=Tue 01-Jan-1970 00:00:00 GMT" );
	}
	if ( path->dat.str.len )
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; path=%s", path->dat.str.c );
	}
	if ( httpOnly )
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; httponly" );
	}
	if ( secure )
	{
		outPtr += sprintf_s ( (char *) outPtr, sizeof ( cookieBuff ) - (outPtr - cookieBuff), "; secure" );
	}
	outPtr[0] = 0;

	conn->rsp.extraHeaders.push_back ( webExtraHeaders ( "Set-Cookie", cookieBuff ) );
}

void cResponseDeleteCookie ( class vmInstance *instance, VAR *obj, VAR_STR *name )
{
	char cookieBuff[4096];

	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	if ( name->dat.str.len + +64 + 1 > sizeof ( cookieBuff ) )	// 64 is all max defaults and sepearator sizes
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	sprintf_s ( cookieBuff, sizeof ( cookieBuff ), "%s=; expires = Tue 01 - Jan - 1970 00:00 : 00 GMT", name->dat.str.c );

	conn->rsp.extraHeaders.push_back ( webExtraHeaders ( "Set-Cookie", cookieBuff ) );
}

void cResponseContentDisposition ( vmInstance *instance, VAR *obj, VAR_STR *cd )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	conn->rsp.extraHeaders.push_back ( webExtraHeaders ( "Content-Disposition", cd->dat.str.c ) );
	conn->rsp.extraDisposition = true;
}

void cResponseCache ( vmInstance *instance, VAR *obj, VAR_STR *userCache )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	assert ( conn->rsp.cachePolicy.user );
	assert ( userCache->dat.str.c );

	if ( userCache->dat.str.len + 1 > sizeof ( conn->rsp.cachePolicy.user ) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	memcpy ( conn->rsp.cachePolicy.user, userCache->dat.str.c, userCache->dat.str.len + 1 );
	conn->rsp.cachePolicy.policyType = webCachePolicy_User;
}

void cResponseAddHeader ( vmInstance *instance, VAR *obj, VAR_STR *name, VAR_STR *value )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	conn->rsp.extraHeaders.push_back ( webExtraHeaders ( name->dat.str.c, value->dat.str.c ) );
	conn->rsp.extraDisposition = true;
}

void cResponseMimeType ( vmInstance *instance, VAR *obj, VAR_STR *mimeType )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	auto it = conn->req.virtualServer->mimeTypes.find ( statString ( mimeType->dat.str.c ) );

	if ( it == conn->req.virtualServer->mimeTypes.end ( ) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	conn->rsp.mimeType = &it->second;
}

void cResponseStatus ( vmInstance *instance, VAR *obj, uint32_t status )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	conn->rsp.rspCode = status;
}

void cResponseKeepAlive ( vmInstance *instance, VAR *obj, uint32_t state )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	conn->rsp.keepAlive = state ? 1 : 0;
}

void cResponseHTTPStatus ( vmInstance *instance, VAR *obj, uint32_t status, nParamType nParams )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	conn->rsp.rspCode = status;

	if ( nParams )
	{
		VAR_STR *ret;

		ret = static_cast<VAR_STR *>(instance->getGlobal ( task->getName ( ), "__outputString" ));
		*ret = *(static_cast<VAR_STR *>(nParams[1]));
	}
}

uint32_t cResponseSendFile ( vmInstance *instance, VAR *obj, VAR_STR *fileName )
{
	if ( !_isfile ( fileName->dat.str.c ) )
	{
		throw GetLastError ( );
	}

	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	conn->rsp.body.bodyType = webServerResponse::body::btFileCacheEntry;

	PUSH ( *fileName );
	classNew ( instance, "$staticBuffer", 1 );
	POP ( 1 );

	auto var = instance->getGlobal ( task->getName ( ), "__outputString" );
	*var = instance->result;

	return 0;
}

bool cResponseSendZipFile ( vmInstance *instance, VAR *obj, VAR *arr )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	int64_t indicie = 1;
	for ( ;; )
	{
		auto var = (*arr)[indicie];
		if ( !var ) break;

		while ( TYPE ( var ) == slangType::eREFERENCE ) var = var->dat.ref.v;

		switch ( TYPE ( var ) )
		{
			case slangType::eARRAY_ROOT:
				{
					auto fName = (*var)[1];
					auto zipFName = (*var)[2];
					auto comp = (*var)[3];
					if ( TYPE ( fName ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;
					if ( TYPE ( zipFName ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;
					if ( comp )
					{
						if ( TYPE ( comp ) != slangType::eLONG ) throw errorNum::scINVALID_PARAMETER;
						if ( comp->dat.l < 0 || comp->dat.l > 9 ) throw errorNum::scINVALID_PARAMETER;
					}
					if ( !_isfile ( fName->dat.str.c ) )
					{
						throw GetLastError ( );
					}
				}
				break;
			case slangType::eSTRING:
				if ( !_isfile ( var->dat.str.c ) )
				{
					throw GetLastError ( );
				}
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
		indicie++;
	}
	*instance->getGlobal ( task->getName ( ), "__outputString" ) = *arr;

	conn->rsp.body.bodyType = webServerResponse::body::btFileCacheEntry;

	return true;
}

static void vmChainPage ( vmInstance *instance, VAR *obj, VAR_STR *loc )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ( );

	std::string name = loc->dat.str.c;

	if ( _ati ( "://", const_cast<char *>(name.c_str ( )) ) != -1 )
	{
	} else if ( (name.c_str ( )[0] != '\\') && (name.c_str ( )[0] != '/') )
	{
		name = std::string ( "\\" ) + name;
	}

	VAR_STR var ( instance, name );
	*static_cast<VAR_STR *>(instance->getGlobal ( task->getName ( ), "__outputString" )) = var;

	conn->rsp.rspCode = 308;
}

static int64_t cElapsedTime ( vmInstance *instance, VAR *obj )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ();

	LARGE_INTEGER		 endTime{};
	LARGE_INTEGER		 serviceTime{};

	LARGE_INTEGER	frequency;

	QueryPerformanceFrequency ( &frequency );
	frequency.QuadPart = frequency.QuadPart / 1000;

	QueryPerformanceCounter ( &endTime );
	serviceTime.QuadPart = (endTime.QuadPart - conn->req.reqStartTime.QuadPart) / frequency.QuadPart;

	return serviceTime.QuadPart;
}

VAR_STR vmWebServerFile ( vmInstance *instance, VAR_STR *fileName )
{
	auto task = static_cast<vmTaskInstance *>(instance);
	auto conn = task->getCargo<webProtocolH1 *> ();

	char dest[MAX_PATH + 1];

	serverBuildName ( conn->req.virtualServer->rootName, fileName->dat.str.c, dest );

	return VAR_STR ( instance, dest );
}

void builtinSessionClassInit ( class vmInstance *instance, opFile *file )
{
	instance->debug->RegisterInspector ( "Web Server", sessionTab );

	REGISTER ( instance, file );
		CLASS ( "webRequest" );
			METHOD ( "new", cSessionNew );
			METHOD ( "numParts", cRequestNumParts );
			METHOD ( "var", cRequestVar, DEF ( 3, "1" ) );
			METHOD ( "varX", cRequestVarX, DEF ( 3, "1" ) );
			METHOD ( "varExists", cRequestVarExists, DEF ( 3, "1" ) );
			METHOD ( "varArray", cRequestVarArray, DEF ( 2, "1" ) );
			METHOD ( "varSet", cRequestVarSet, DEF ( 2, "1" ) );
			METHOD ( "varDelete", cRequestVarDelete, DEF ( 2, "1" ) );
			METHOD ( "data", cRequestData );
			METHOD ( "postData", cRequestPostData, DEF ( 2, "1" ) );
			METHOD ( "postDataWrite", cRequestWritePostData, DEF ( 3, "1" ) );
			METHOD ( "header", cRequestHeader );

			ACCESS ( "method", cRequestMethod );
			ACCESS ( "url", cRequestURL );
			ACCESS ( "host", cRequestHost );
			ACCESS ( "from", cRequestFrom );
			ACCESS ( "userAgent", cRequestUserAgent );
			ACCESS ( "contentType", cRequestContentType );
			ACCESS ( "referer", cRequestReferer );
			ACCESS ( "user", cRequestUser );
			ACCESS ( "pw", cRequestPW );
			ACCESS ( "domain", cRequestDomain );
			ACCESS ( "body", cRequestBody );
			ACCESS ( "headers", cRequestHeaders );
			ACCESS ( "rootDirectory", cRequestRootDirectory );
			ACCESS ( "serverRoot", cRequestRootDirectory);

			METHOD ( "cookieGet", cRequestGetCookie );
			METHOD ( "cookieGetRaw", cRequestGetCookieRaw );

			ACCESS ( "elapsedTime", cElapsedTime );
		END;

		CLASS ( "webResponse" );
			METHOD ( "new", cSessionNew );

			METHOD ( "cookieSet", cResponseSetCookie, DEF ( 4, "\"\"" ), DEF ( 5, "\"\"" ), DEF ( 6, "0" ), DEF ( 7, "0" ) );
			METHOD ( "cookieDelete", cResponseDeleteCookie );
			METHOD ( "addHeader", cResponseAddHeader );

			ASSIGN ( "cache", cResponseCache );
			ASSIGN ( "contentDisposition", cResponseContentDisposition );
			ASSIGN ( "status", cResponseStatus );
			ASSIGN ( "keepAlive", cResponseKeepAlive );
			ASSIGN ( "mimeType", cResponseMimeType );
			METHOD ( "response", cResponseSetCompleteResponse );
			METHOD ( "sendFile", cResponseSendFile );
			METHOD ( "sendZipFile", cResponseSendZipFile );
			METHOD ( "redir", cResponseRedirect );
			METHOD ( "redirect", cResponseRedirect );

			// TODO: depracate
			METHOD ( "httpStatus", cResponseHTTPStatus );
		END;

		CLASS ( "webServer" );
			METHOD ( "new", cSessionNew );

			// access to things like total bytes sent, etc.
			ACCESS ( "totalBytesSent", cSessionServerTotalBytesSent );
			ACCESS ( "totalBytesReceived", cSessionServerTotalBytesReceived );
			ACCESS ( "totalBytesConnections", cSessionServerTotalConnections );
			ACCESS ( "serverName", cSessionServerName );
			//			ACCESS ( "serverStartTime", cSessionServerName );
		END;

		CLASS ( "webSession" );
			METHOD ( "new", cSessionNew );
			INHERIT ( "webRequest" );
			INHERIT ( "webResponse" );
			INHERIT ( "webServer" );
			INSPECTORCB ( cSessionInspector );
		END;

		FUNC ( "chainPage", vmChainPage );
		FUNC ( "webServerFile", vmWebServerFile );

		NAMESPACE ( "fgl" )
			CLASS ( "COOKIE" );
				METHOD ( "new", cSessionNew );
				METHOD ( "get", cRequestGetCookie );
				METHOD ( "getRaw", cRequestGetCookieRaw );
				METHOD ( "set", cResponseSetCookie, DEF (4, "\"\""), DEF (5, "\"\""), DEF (6, "0"), DEF (7, "0") );
				METHOD ( "delete", cResponseDeleteCookie) ;
			END;
		END
	END;
}
