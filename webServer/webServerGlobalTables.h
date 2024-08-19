#include "Utility/settings.h"

#include <memory>
#include <vector>
#include <string>

#include "webServer.h"
#include "Target/vmTask.h"

#define REQ_TABLE \
	DEF_REQ ( ALL_HTTP )				\
	DEF_REQ ( ALL_RAW )					\
	DEF_REQ ( APPL_MD_PATH )			\
	DEF_REQ ( APPL_PHYSICAL_PATH )		\
	DEF_REQ ( AUTH_PASSWORD )			\
	DEF_REQ ( AUTH_TYPE_ISAPI )			\
	DEF_REQ ( AUTH_TYPE )				\
	DEF_REQ ( AUTH_USER )				\
	DEF_REQ ( CERT_COOKIE )				\
	DEF_REQ ( CERT_FLAGS )				\
	DEF_REQ ( CERT_ISSUER )				\
	DEF_REQ ( CERT_KEYSIZE )			\
	DEF_REQ ( CERT_SECRETKEYSIZE )		\
	DEF_REQ ( CERT_SERIALNUMBER )		\
	DEF_REQ ( CERT_SERVER_ISSUER )		\
	DEF_REQ ( CERT_SERVER_SUBJECT )		\
	DEF_REQ ( CERT_SUBJECT )			\
	DEF_REQ ( CONTENT_LENGTH )			\
	DEF_REQ ( CONTENT_TYPE )			\
	DEF_REQ ( HTTP_ACCEPT )				\
	DEF_REQ ( HTTP_HOST )				\
	DEF_REQ ( HTTP_FROM )				\
	DEF_REQ ( HTTPS )					\
	DEF_REQ ( HTTPS_KEYSIZE )			\
	DEF_REQ ( HTTPS_SECRETKEYSIZE )		\
	DEF_REQ ( HTTPS_SERVER_ISSUER )		\
	DEF_REQ ( HTTPS_SERVER_SUBJECT )	\
	DEF_REQ ( INSTANCE_ID )				\
	DEF_REQ ( INSTANCE_META_PATH )		\
	DEF_REQ ( PATH_INFO )				\
	DEF_REQ ( PATH_TRANSLATED )			\
	DEF_REQ ( QUERY_STRING )			\
	DEF_REQ ( REMOTE_ADDR )				\
	DEF_REQ ( REMOTE_PORT )				\
	DEF_REQ ( REMOTE_HOST )				\
	DEF_REQ ( REMOTE_USER )				\
	DEF_REQ ( REQUEST_METHOD )			\
	DEF_REQ ( REQUEST_URI )				\
	DEF_REQ ( SCRIPT_NAME )				\
	DEF_REQ ( SERVER_NAME )				\
	DEF_REQ ( SERVER_PORT )				\
	DEF_REQ ( SERVER_PORT_SECURE )		\
	DEF_REQ ( SERVER_PROTOCOL )			\
	DEF_REQ ( SERVER_SOFTWARE )			\
	DEF_REQ ( URL )						\
	DEF_REQ ( HTTP_COOKIE )				\
	DEF_REQ ( REFERER )					\
	DEF_REQ ( X_FORWARD_FOR )			\
	DEF_REQ ( USERAGENT )				

#define DEF_REQ(name) req_##name, 
enum reqId {
	REQ_TABLE
};
#undef DEF_REQ

struct reqTableDef {
	reqId		 id;
	char const	*name;
	size_t		 nameLen;
};


#define HTTP_HEADER_TABLE \
DEF_HEADER( acceptEncoding,			"Accept-Encoding:" )			\
DEF_HEADER( authorization,			"Authorization:" )				\
DEF_HEADER( cacheControl,			"Cache-Control:" )				\
DEF_HEADER( connection,				"Connection:" )					\
DEF_HEADER( cookie,					"Cookie:" )						\
DEF_HEADER( contentLength,			"Content-Length:" )				\
DEF_HEADER( contentType,			"Content-Type:" )				\
DEF_HEADER( expect,					"Expect:" )						\
DEF_HEADER( from,					"From:" )						\
DEF_HEADER( host,					"Host:" )						\
DEF_HEADER( ifMatch,				"If-Match:" )					\
DEF_HEADER( ifModifiedSince,		"If-Modified-Since:" )			\
DEF_HEADER( ifNoneMatch,			"If-None-Match:" )				\
DEF_HEADER( ifRange,				"If-Range:" )					\
DEF_HEADER( ifUnmodifiedSince,		"If-Unmodified-Since:" )		\
DEF_HEADER( keepAlive,				"Keep-Alive:" )					\
DEF_HEADER( pragma,					"Pragma:" )						\
DEF_HEADER( range,					"Range:" )						\
DEF_HEADER( transferEncoding,		"Transfer-Encoding:" )			\
DEF_HEADER( upgrade,				"Upgrade:" )					\
DEF_HEADER( xFormVar,				"X-Form-Var:" )					\
DEF_HEADER( xForwarded_for,			"X-Forwarded_For:" )			\
DEF_HEADER( xForward_for,			"X-Forwarded_For:" )			\
DEF_HEADER( xDebug,					"X-Debug:" )					

#define DEF_HEADER(name, value) HTTP_##name, 
enum httpHeader {
	HTTP_HEADER_TABLE
};
#undef DEF_HEADER

struct httpHeaderTableDef {
	httpHeader	 id;
	char const	*name;
	uint32_t	 nameLen;
};

extern const reqTableDef			reqTable[];
extern const httpHeaderTableDef		httpHeaderTable[];

extern std::map<statString, httpHeader>	httpHeaderMap;
extern std::map<statString, reqId>		reqMap;

extern void webServerGlobalTablesInit ( void );
