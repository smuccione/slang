#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <atlenc.h>

#include "Target/vmConf.h"
#include "Utility/util.h"
#include "Utility/stringi.h"

#include "webServer.h"
#include "webSocket.h"
#include "webServerIOCP.h"

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmInstance.h"
#include "Target/vmTask.h"

#define HTTP_MIME_TABLE \
DEF_MIME ( webActivePageHandler,	".ap",		"",				"text/html; charset=ISO-8859-1",				"",				"" )	\
DEF_MIME ( webActivePageHandler,	".apf",		"",				"text/html; charset=ISO-8859-1",				"",				"" )	\
DEF_MIME ( webActivePageHandler,	".aps",		"",				"text/html; charset=ISO-8859-1",				"",				"" )	\
DEF_MIME ( webActivePageHandler,	".apx",		"",				"text/html; charset=ISO-8859-1",				"",				"" )	\
DEF_MIME ( webStaticPageHandler,	".htm",		".htm.gzc",		"text/html; charset=ISO-8859-1",				"",				"" )	\
DEF_MIME ( webStaticPageHandler,	".html",	".html.gzc",	"text/html; charset=ISO-8859-1",				"",				"" )	\
DEF_MIME ( webStaticPageHandler,	".264",		"",				"video/h264",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".3GP",		"",				"video/3gpp",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".DIVX",	"",				"video/divx",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".EOT",		"",				"application/vnd.ms-fontobject",				"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".F4V",		"",				"video/mp4",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".FLV",		"",				"video/x-flv",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MAP",		"",				"application/json",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".JSON",	"",				"application/json",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".JSONP",	"",				"application/javascript",						"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".M2TS",	"",				"video/mp2t",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".M3U8",	"",				"application/x-mpegURL",						"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".M4V",		"",				"video/x-m4v",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MKV",		"",				"video/x-matroska",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MPC",		"",				"audio/x-musepack",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MPV",		"",				"video/quicktime",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".RAM",		"",				"audio/x-pn-realaudio",							"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".RMI",		".RMI.GZC",		"audio/mid",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".SND",		".SND.GZC",		"audio/basic",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".WAV",		".WAV.GZC",		"audio/x-wav",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".AIF",		"",				"audio/x-aiff",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".AIFC",	"",				"audio/x-aiff",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".AIFF",	"",				"audio/x-aiff",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".AU",		"",				"audio/basic",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".M3U",		"",				"audio/x-mpegurl",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MID",		".MID.GZC",		"audio/mid",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MP3",		"",				"audio/mpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MP2",		"",				"audio/mpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".RA",		"",				"audio/x-pn-realaudio",							"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".LSF",		"",				"video/x-la-asf",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".LSX",		"",				"video/x-la-asf",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MOV",		"",				"video/quicktime",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MOVIE",	"",				"video/x-sgi-movie",							"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MPA",		"",				"video/mpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MPE",		"",				"video/mpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MPEG",	"",				"video/mpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MPG",		"",				"video/mpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MP4",		"",				"video/mp4",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".MPV2",	"",				"video/mpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".QT",		"",				"video/quicktime",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".ASF",		"",				"video/x-ms-asf",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".ASR",		"",				"video/x-ms-asf",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".ASX",		"",				"video/x-ms-asf",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".AVI",		"",				"video/x-msvideo",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".TIFF",	".TIFF.GZC",	"image/tiff",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".XBM",		"",				"image/x-xbitmap",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".XPM",		"",				"image/x-xpixmap",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".XWD",		"",				"image/x-xwindowdump",							"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".XML",		"",				"text/xml",										"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".BMP",		".BMP.GZC",		"image/bmp",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".CMX",		"",				"image/x-cmx",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".COD",		"",				"image/cis-cod",								"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".GIF",		"",				"image/gif",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".ICO",		"",				"image/x-icon",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".IEF",		"",				"image/ief",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".JFIF",	"",				"image/pipeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".JPE",		"",				"image/jpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".JPEG",	"",				"image/jpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".JPG",		"",				"image/jpeg",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".JS",		".JS.GZC",		"application/javascript",						"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".PNG",		"",				"image/png",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".PBM",		"",				"image/x-portable-bitmap",						"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".PGM",		"",				"image/x-portable-graymap",						"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".PNM",		"",				"image/x-portable-anymap",						"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".PPM",		"",				"image/x-portable-pixmap",						"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".RAS",		"",				"image/x-cmu-raster",							"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".RGB",		"",				"image/x-rgb",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".TIF",		".TIF.GZC",		"image/tiff",									"",				"binary" )     \
DEF_MIME ( webStaticPageHandler,	".CAB",		"",				"application/cab",								"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SWF",		"",				"application/x-shockwave-flash",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".FLV",		"",				"video/x-flv",									"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CDF",		"",				"application/x-cdf",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CER",		"",				"application/x-x509-ca-cert",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CLASS",	".CLASS.GZC",	"application/octet-stream",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CLP",		"",				"application/x-msclip",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CPIO",	"",				"application/x-cpio",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".COM",		".COM.GZC",		"application/x-msdos-program",					"attachment",	"binary" )\
DEF_MIME ( webStaticPageHandler,	".CRD",		".CRD.GZC",		"application/x-mscardfile",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CRL",		"",				"application/pkix-crl",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CRT",		".CRT.GZC",		"application/x-x509-ca-cert",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CSH",		"",				"application/x-csh",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CSS",		".CSS.GZC",		"text/css",										"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DCR",		"",				"application/x-director",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DER",		"",				"application/x-x509-ca-cert",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DIR",		"",				"application/x-director",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DLL",		".DLL.GZC",		"application/x-msdownload",						"attachment",	"binary" )\
DEF_MIME ( webStaticPageHandler,	".DMS",		"",				"application/octet-stream",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DOC",		".DOC.GZC",		"application/msword",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DOT",		".DOT.GZC",		"application/msword",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DVI",		"",				"application/x-dvi",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".DXR",		"",				"application/x-director",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".EPS",		".EPS.GZC",		"application/postscript",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".EVY",		"",				"application/envoy",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".BAT",		".EXE.GZC",		"application/x-msdos-program",					"attachment",	"binary" )\
DEF_MIME ( webStaticPageHandler,	".EXE",		".EXE.GZC",		"application/x-msdos-program",					"attachment",	"binary" )\
DEF_MIME ( webStaticPageHandler,	".FIF",		"",				"application/fractals",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".GTAR",	"",				"application/x-gtar",							"attachment",	"binary" )\
DEF_MIME ( webStaticPageHandler,	".GZ",		"",				"application/x-gzip",							"attachment",	"binary" )\
DEF_MIME ( webStaticPageHandler,	".HDF",		"",				"application/x-hdf",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".HLP",		".HLP.GZC",		"application/winhlp",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".HQX",		"",				"application/mac-binhex40",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".HTA",		"",				"application/hta",								"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".III",		"",				"application/x-iphone",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".INS",		"",				"application/x-internet-signup",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".ISP",		"",				"application/x-internet-signup",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".LATEX",	"",				"application/x-latex",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".LHA",		"",				"application/octet-stream",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".LZH",		"",				"application/octet-stream",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".M13",		"",				"application/x-msmediaview",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".M14",		"",				"application/x-msmediaview",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".MAN",		"",				"application/x-troff-man",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".MDB",		"",				"application/x-msaccess",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".ME",		"",				"application/x-troff-me",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".MNY",		"",				"application/x-msmoney",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".MPP",		"",				"application/vnd.ms-project",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".MS",		"",				"application/x-troff-ms",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".MVB",		"",				"application/x-msmediaview",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".NC",		"",				"application/x-netcdf",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".ODA",		"",				"application/oda",								"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".P10",		"",				"application/pkcs10",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".P12",		"",				"application/x-pkcs12",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".P7B",		"",				"application/x-pkcs7-certificates",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".P7C",		"",				"application/x-pkcs7-mime",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".P7M",		"",				"application/x-pkcs7-mime",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".P7R",		"",				"application/x-pkcs7-certreqresp",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".P7S",		"",				"application/x-pkcs7-signature",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PDF",		"",				"application/pdf",								"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PFX",		"",				"application/x-pkcs12",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PKO",		"",				"application/ynd.ms-pkipko",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PMA",		".PMA.GZC",		"application/x-perfmon",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PMC",		".PMC.GZC",		"application/x-perfmon",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PML",		".PML.GZC",		"application/x-perfmon",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PMR",		".PMR.GZC",		"application/x-perfmon",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PMW",		".PMW.GZC",		"application/x-perfmon",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".POT",		".pot.gzc",		"application/vnd.ms-powerpoint",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PPS",		".pps.gzc",		"application/vnd.ms-powerpoint",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PPT",		".ppt.gzc",		"application/vnd.ms-powerpoint",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PRF",		"",				"application/pics-rules",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PS",		".PS.GZC",		"application/postscript",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".PUB",		"",				"application/x-mspublisher",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".ROFF",	"",				"application/x-troff",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".RTF",		".RTF.GZC",		"application/rtf",								"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SCD",		"",				"application/x-msschedule",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SETPAY",	"",				"application/set-payment-initiation",			"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SETREG",	"",				"application/set-registration-initiation",		"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SH",		"",				"application/x-sh",								"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SHAR",	"",				"application/x-shar",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SIT",		"",				"application/x-stuffit",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SPC",		".SPC.GZC",		"application/x-pkcs7-certificates",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SPL",		"",				"application/futuresplash",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SRC",		"",				"application/x-wais-source",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SST",		"",				"application/vnd.ms-pkicertstore",				"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SV4CPIO",	"",				"application/x-sv4cpio",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".SV4CRC",	"",				"application/x-sv4crc",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".T",		"",				"application/x-troff",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TAR",		".TAR.GZC",		"application/x-tar",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TCL",		".TCL.GZC",		"application/x-tcl",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TEX",		".TEX.GZC",		"application/x-tex",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TEXI",	".TEXI.GZC",	"application/x-texinfo",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TEXINFO",	"",				"application/x-texinfo",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TGZ",		"",				"application/x-compressed",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TR",		"",				"application/x-troff",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TRM",		"",				"application/x-msterminal",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TTF",		"",				"application/x-font-ttf",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".TXT",		"",				"text/plain",									"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".USTAR",	"",				"application/x-ustar",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WCM",		".WCM.GZC",		"application/vnd.ms-works",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WDB",		".WDB.GZC",		"application/vnd.ms-works",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WKS",		".WKS.GZC",		"application/vnd.ms-works",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WMF",		".WMF.GZC",		"application/x-msmetafile",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WOFF2",	"",				"application/font-woff2",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WOFF",	"",				"application/font-woff",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WPI",		"",				"application/x-xpinstall",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WPS",		".WPS.GZC",		"application/vnd.ms-works",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WRI",		".WRI.GZC",		"application/x-mswrite",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".WVX",		"",				"video/x-ms-wvx ",								"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".XLA",		".XLA.GZC",		"application/vnd.ms-excel",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".XLC",		".XLC.GCZ",		"application/vnd.ms-excel",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".XLM",		".XLM.GZC",		"application/vnd.ms-excel",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".XLS",		".XLS.GZC",		"application/vnd.ms-excel",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".XLSX",	".XLSX.GZC",	"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",	"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".XLT",		".XLT.GZC",		"application/vnd.ms-excel",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".XLW",		".XLW.GZC",		"application/vnd.ms-excel",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".Z",		"",				"application/x-compress",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".ZIP",		"",				"application/zip",								"attachment",	"binary" )\
DEF_MIME ( webStaticPageHandler,	".ACX",		"",				"application/internet-property-stream",			"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".AI",		".AI.GZC",		"application/postscript",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".AXS",		"",				"application/olescript",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".BCPIO",	"",				"application/x-bcpio",							"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".BIN",		".BIN.GZC",		"application/octet-stream",						"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".CAT",		"",				"application/vnd.ms-pkiseccat",					"",				"binary" )\
DEF_MIME ( webStaticPageHandler,	".ISO",		"",				"application/octet-stream",						"attachment",	"binary" )\
	
#define DEF_MIME(cb, ext, compExt, contentType, contentDisposition, encoding )	webMimeType ( cb, ext, compExt, contentType, contentDisposition, encoding ),
webMimeType globalMimeTypes[] =	{
									HTTP_MIME_TABLE
								};
#undef DEF_MIME

#define HTTP_METH_TABLE \
DEF_METH ( GET,		webGetPostMethodHandler )	\
DEF_METH ( POST,	webGetPostMethodHandler )	\
DEF_METH ( HEAD,	webHeadMethodHandler )	\
DEF_METH ( TRACE,	webTraceMethodHandler )	\

#undef DEF_METH
#define DEF_METH(meth,handler) webServerMethods ( #meth, "", handler ),
webServerMethods globalServerMethods[] =	{
													HTTP_METH_TABLE
											};
enum configSections {
	CONFIG_UNKNOWN,
	CONFIG_CONFIG,
	CONFIG_REALMS,
	CONFIG_GROUPS,
	CONFIG_USER_REALM,
	CONFIG_USER_GROUP,
	CONFIG_VIRTUAL_SERVERS,
	CONFIG_FASTCGI,
};

static struct {
	statString name;
	configSections		section;
}		areas[]	=	{	{ "REALMS",				CONFIG_REALMS			},
						{ "GROUPS",				CONFIG_GROUPS			},
						{ "CONFIG",				CONFIG_CONFIG			},
						{ "FASTCGI",			CONFIG_FASTCGI			},
						{ "VIRTUAL SERVERS",	CONFIG_VIRTUAL_SERVERS	},
					};


/* note DEST must be sized and padded correctly */

char *base64Encode ( unsigned char *in, uint32_t len)

{
	uint32_t		 loop;
	uint32_t		 outLen;
	unsigned char	*out;
	unsigned char	*ret;

	outLen = ( (len - 1) / 3 + 1 ) * 4; /* round len to greatest 3rd and multiply by 4 */

	out = (unsigned char *)malloc ( sizeof ( unsigned char ) * (outLen + 3 + 1) );  // just add extra unused to make routine faster

	ret = out;

	for ( loop = 0; loop < len; loop += 3 )
	{
		out[0] = webBase64EncodingTable[(in[0] >> 2)];
		out[1] = webBase64EncodingTable[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		out[2] = webBase64EncodingTable[((in[1] & 0x0F) << 2) | (in[2] >> 6)];
		out[3] = webBase64EncodingTable[((in[2] & 0x3F))];
		/* increment our pointers appropriately */
		out += 4;
		in  += 3;
	}

	out[0] = 0;

	/* fill in "null" character... all 0's in important bits */
	switch ( len % 3 )
	{
		case 1:
			out[-2] = '=';
			// fall through 
		case 2:
			out[-1] = '=';
			break;
		case 0:
		default:
			break;
	}
	return (char *)ret;
}

class webAuthGroup {
public:
	stringi							 name;
	webServerAuth					*auth;
};

#define DEF_MIME(cb, ext, compExt, contentType, contentDisposition, encoding )	new webMimeType ( cb, ext, compExt, contentType, contentDisposition, encoding )
webMimeType	*apxMimeType	= DEF_MIME ( webActivePageHandler,	".apx",		"",				"text/html; charset=ISO-8859-1",				"",				"" );
webMimeType	*htmMimeType	= DEF_MIME ( webStaticPageHandler,	".htm",		".htm.gzc",		"text/html; charset=ISO-8859-1",				"",				"" );
webMimeType	*htmlMimeType	= DEF_MIME ( webStaticPageHandler,	".html",	".html.gzc",	"text/html; charset=ISO-8859-1",				"",				"" );

void webVirtualServer::apProjectFileChangeMonitor ( void )
{
	BOOL				 result;
	DWORD				 nBytesTransferred;
	ULONG_PTR			 key;
	fileCacheDir *watch = nullptr;;

	while ( 1 )
	{
		result = GetQueuedCompletionStatus ( apProjectMonitorHandle, &nBytesTransferred, &key, (OVERLAPPED **) &watch, INFINITE );

		if ( result )
		{
			// simply flush when anything was changed... this is a RARE occurance and the benefit of just removing one entry from the cache is outweighed by simplicity and downline speed
			apProjectNeedBuild = true;

			memset ( &(watch->ov), 0, sizeof ( watch->ov ) );
			// register for the next change notification
			ReadDirectoryChangesW ( watch->dirHandle,
									watch->dat.data,
									sizeof ( watch->dat.data ),
									TRUE,
									FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_LAST_WRITE,
									&nBytesTransferred,
									&(watch->ov),
									0
			);
		} else
		{
			return;
		}
	}
}

webServer::webServer ( char const *configName, HANDLE iocpHandle ) : iocpHandle ( iocpHandle )

{
	char										 inLine[8192];
	char										 name[256];
	char										 value[256];
	char										 subValue1[256];
	char										 subValue2[256];
	char										 disabled[256];
	char										 configFileName[MAX_PATH];
	char										*dummy;
	configSections								 area = CONFIG_CONFIG;
	webServerAuthRealm							*realm;
	webServerAuth								*auth;
	webAuthGroup								*group;
	std::vector<webAuthGroup *>					 groups;
	uint32_t									 loop;
	webVirtualServer							*virtServer;
	std::map<stringi, stringi>					 logFormats;


	virtualServerMap.max_load_factor ( 1 );

	InitializeSRWLock ( &virtualServerAccessLock );

	InitializeCriticalSectionAndSpinCount( &chunkFreeCrit, 4000 );
	chunkFree = 0;
	InitializeCriticalSectionAndSpinCount( &serverBuffFreeCrit, 4000 );
	serverBuffFree = 0;

	virtualServerDefault.refCount = 0;
	virtualServerDefault.compressAPMin		= 8192;
	virtualServerDefault.defaultRedir		= true;
	virtualServerDefault.doAltParamSearch	= false;
	virtualServerDefault.maxExecuteTime		= 60ULL*5;			// default to 5 minutes;
	virtualServerDefault.maxRxContent		= 1024ULL*1024*128;
	virtualServerDefault.rootName			= "root\\";
	virtualServerDefault.uploadDir			= "";
	virtualServerDefault.maxCookieSize		= 4096;
	virtualServerDefault.passiveSecurity	= false;
	virtualServerDefault.defaultCache		= "\r\nCache-Control: public, max-age=3600";		// default to caching for 1 hour
	
	for ( loop = 0; loop < sizeof ( globalServerMethods ) / sizeof ( globalServerMethods[0] ); loop++ )
	{
		virtualServerDefault.methodHandlers[statString(globalServerMethods[loop].name.c_str())] = globalServerMethods[loop];
	}

	for ( loop = 0; loop < sizeof ( globalMimeTypes ) / sizeof ( globalMimeTypes[0] ); loop++ )
	{
		virtualServerDefault.mimeTypes[statString ( (char *)globalMimeTypes[loop].ext.c_str(), globalMimeTypes[loop].ext.length() )] =  globalMimeTypes[loop];
	}

	if ( virtualServerDefault.rootName[virtualServerDefault.rootName.size() - 1] != '\\' )
	{
		virtualServerDefault.rootName += '\\';
	}

	keepAliveTimeout		= 5;				/* cconnection keep alive time.. default to 5 seconds */
	maxConnections			= 5000;
	minAcceptsOutstanding	= 100; 
	listenPort				= 80;
	listenAddrV4			= INADDR_ANY;
	isSecure				= false;
	virtServer				= &virtualServerDefault;
	serverName				= "undefined";

	// now process the configuration file
	if ( configName && *configName )
	{
		std::ifstream iStream ( configName );
		if ( iStream.bad() )
		{
			printf ( "Unable to open configuration file... starting server with defaults only\r\n" );
			return;
		}

		GetFullPathName ( configName, sizeof ( configFileName ), configFileName, &dummy );
		configFile = configFileName;
		
		while ( !iStream.eof() && iStream.good() )
		{
			std::string line;
			std::getline ( iStream, line );

			strcpy_s ( inLine, sizeof( inLine ), line.c_str ( ) );

			_alltrim ( inLine, inLine );

			if ( *inLine == '#' )
			{
				continue;
			}

			if ( *inLine == '['  )
			{
				trimpunct ( inLine, inLine );
				_alltrim ( inLine, inLine );

				/* look for a pre-declared section name */
				for ( loop = 0; loop < sizeof ( areas ) / sizeof ( areas[0] ); loop++ )
				{
					if ( !strccmp ( areas[loop].name.c_str(), inLine ) )
					{
						switch ( areas[loop].section )
						{
							case CONFIG_REALMS:
							case CONFIG_CONFIG:
							case CONFIG_GROUPS:
							case CONFIG_VIRTUAL_SERVERS:
								area = areas[loop].section;
								break;
							default:
								area = CONFIG_UNKNOWN;
								break;
						}
						break;
					}
				}
				if ( loop >=  sizeof ( areas ) / sizeof ( areas[0] ) )
				{
					/* didn't find one so see if it's a user-declared area */
					area = CONFIG_UNKNOWN;

					for ( auto webVirtIt : virtualServers )
					{
						if ( webVirtIt->name == inLine )
						{
							area = CONFIG_CONFIG;
							virtServer = webVirtIt;
							break;
						}
					}

					if ( area == CONFIG_UNKNOWN )
					{
						for ( auto realmIt : realms )
						{
							if ( realmIt->name == inLine )
							{
								/* found a realm */
								area = CONFIG_USER_REALM;
								realm = realmIt;
								break;
							}
						}
					}

					if ( area == CONFIG_UNKNOWN )
					{
						/* didn't find a user realm so see if it's a group */

						for ( auto groupIt : groups )
						{
							if ( groupIt->name == inLine )
							{
								/* found a group */
								area = CONFIG_USER_GROUP;
								group = groupIt;
								break;
							}
						}
					}
					if ( area == CONFIG_UNKNOWN )
					{
						printf ( "Error parsing configuration file... unknown section: %s\r\n", inLine );
					}
				}
			} else if ( *inLine )
			{
				switch ( area )
				{
					case CONFIG_CONFIG:
						_token ( inLine, "= \t", 1, name );
						strcpy_s ( value, sizeof ( value ), inLine + _attoken ( inLine, " =\t", 1 ) + 1 );

						_alltrim ( name, name );
						_alltrim ( value, value );

						if ( !strccmp ( name, "LISTEN" ) )
						{
							_token ( value, ":", 1, subValue2 );
							_token ( value, ":", 2, subValue1 );
							if ( *subValue1 )
							{
								listenAddrV4 = inet_addr ( subValue1 );
							}
							if ( *subValue2 )
							{
								listenPort = atoi ( subValue2 );
							}
						} else if ( !strccmp ( name, "SERVER_ROOT" ) && *value )
						{
							virtServer->rootName = value;
							if ( virtServer->rootName[virtServer->rootName.length() - 1] != '\\' )
							{
								virtServer->rootName += '\\';
							}
						} else if ( !strccmp ( name, "UPLOAD_DIR" ) && *value )
						{
							virtServer->uploadDir = value;
							if ( virtServer->uploadDir[virtServer->uploadDir.length() - 1] != '\\' )
							{
								virtServer->uploadDir += '\\';
							}
						} else if ( !strccmp ( name, "SERVER_NAME" ) && *value )
						{
							if ( virtServer == &virtualServerDefault )
							{
								serverName = value;
							}
							virtServer->serverName = value;
						} else if ( !strccmp ( name, "CGI_DIR" ) && *value )
						{
							virtServer->cgiDir.push_back ( value );
							if ( virtServer->cgiDir.back ()[virtServer->cgiDir.back ().size () - 1] != '\\' )
							{
								virtServer->cgiDir.back () += "\\";
							}
						} else if ( !strccmp ( name, "DEFAULT_PAGE" ) )
						{
							virtServer->cgiDir.push_back ( value );
						} else if ( !strccmp ( name, "DEFAULT_DIRECT" ) )
						{
							if ( !strccmp ( value, "OFF" ) )
							{
								virtServer->defaultRedir=1;
							} else if ( !strccmp ( value, "ON" ) )
							{
								virtServer->defaultRedir=0;
							} else
							{
								printf ( "error: default_direct must be either ON or OFF\r\n" );
							}
						} else if ( !strccmp ( name, "PRINT_PARSER_OUTPUT" ) )
						{
//							displayParsedCode = 1;
						} else if ( !strccmp ( name, "PRINT_PAGE_OUTPUT" ) )
						{
							printResponseHeaders = true;
						} else if ( !strccmp ( name, "PRINT_REQUEST" ) )
						{
							printRequestHeaders = true;
						} else if ( !strccmp ( name, "MINIMUM_ACCEPT" ) )
						{
							minAcceptsOutstanding = atol ( value );
							if ( minAcceptsOutstanding < 2 )
							{
								minAcceptsOutstanding = 2;
								printf ( "error: minimum_accept must be >= 2\r\n" );
							}
						} else if ( !strccmp ( name, "MAX_CONNECTIONS" ) )
						{
							maxConnections = atol ( value );
							if ( maxConnections < 2 )
							{
								maxConnections = 2;
								printf ( "error: max_connections must be >= 2\r\n" );
							}
						} else if ( !strccmp ( name, "MAX_COOKIE_SIZE" ) )
						{
							virtServer->maxCookieSize = atol ( value );
							if ( virtServer->maxCookieSize < 256 )
							{
								printf ( "error: maximum cookie size can not be less then 256 bytes\r\n" );
								virtServer->maxCookieSize = 256;
							}
						} else if ( !strccmp ( name, "AP_COMPRESSION" ) )
						{
							virtServer->compressAPMin = atol ( value );
						} else if ( !strccmp ( name, "MAX_REQUEST_SIZE" ) )
						{
							virtServer->maxRxContent = atol ( value );			// maximum request body length... both memory and dsk
						} else if ( !strccmp ( name, "IOCP_SERVER" ) )
						{
							printf ( "warning: iocp_server flag is depricated\r\n" );
						} else if ( !strccmp ( name, "ALTERNATE_PARAMETER_METHOD" ) )
						{
							if ( !strccmp ( value, "OFF" ) )
							{
								virtServer->doAltParamSearch = 0;
							} else if ( !strccmp ( value, "ON" ) )
							{
								virtServer->doAltParamSearch = 1;
							} else
							{
								printf ( "error: alternate_parameter_method must be either ON or OFF\r\n" );
							}
						} else if ( !strccmp ( name, "SECURITY" ) )
						{
							if ( !strccmp ( value, "ACTIVE" ) )
							{
								virtServer->passiveSecurity = false;
							} else if ( !strccmp ( value, "PASSIVE" ) )
							{
								virtServer->passiveSecurity = true;
							} else
							{
								printf ( "error: security configuration type must be either ACTIVE or PASSIVE\r\n" );
							}
						} else if ( !strccmp ( name, "METHOD_HANDLER" ) )
						{
							_token ( value, "=", 1, subValue1 );
							_token ( value, "=", 2, subValue2 );

							_alltrim ( subValue1, subValue1 );
							_upper ( subValue1, subValue1 );

							_alltrim ( subValue2, subValue2 );

							auto handler = webServerMethods ( subValue1, subValue2, webCustomMethodHandler );
							strcat_s ( subValue1, sizeof ( subValue1 ), " " );

							virtServer->methodHandlers[statString ( _strdup ( subValue1 ) )] = handler;
						} else if ( !strccmp ( name, "MIME" ) )
						{
							strcpy_s ( inLine, sizeof ( inLine ), value );

							webMimeType mime;

							_token ( inLine, ";", 1, name );
							_alltrim ( name, name );
							_upper ( name, name );
							mime.ext = name;


							_token ( inLine, ";", 2, name );
							_alltrim ( name, name );
							mime.contentType = name;

							_token ( inLine, ";", 3, name );
							_alltrim ( name, name );
							if ( *name )
							{
								mime.contentDisposition = name;
							}
							mime.handler = webStaticPageHandler;
							// TODO: fix memory leak
							// small memory leak for mime->ext in order to keep it valid since we're using statString here which doesn't have it's own buffer (hence STATIC)
							virtServer->mimeTypes[statString ( _strdup ( (char *)mime.ext.c_str() ), mime.ext.length() )] = mime;
						} else if ( !strccmp ( name, "AP_HANDLER" ) )
						{
							if ( !strccmp ( value, "aps" ) )
							{
								virtServer->apDefaultAPS = true;
							} else if ( !strccmp ( value, "apf" ) )
							{
								virtServer->apDefaultAPS = false;
							} else
							{
								printf ( "error: ap_handler must ben either apf or aps\r\n" );
							}
						} else if ( !strccmp ( name, "MAX_EXECUTE_TIME" ) )
						{
							virtServer->maxExecuteTime = atoi ( value );
						} else if ( !strccmp ( name, "KEEP_ALIVE_TIME" ) )
						{
							keepAliveTimeout = atoi ( value );
						} else if ( !strccmp ( name, "SSL" ) )
						{
							if ( !strccmp ( value, "ENABLE" ) )
							{
								isSecure = true;
							} else if ( !strccmp ( value, "DISABLE" ) )
							{
								isSecure = false;
							} else
							{
								printf ( "Error: SSL setting must be either Enable or Disable\r\n" );
							}
						} else if ( !strccmp ( name, "CERTIFICATE" ) )
						{
							_token ( value, "@", 1, subValue1 );
							_token ( value, "@", 2, subValue2 );

							virtServer->certificateFile = subValue1;
							virtServer->sslPassword = subValue2;
						} else if ( !strccmp ( name, "KEY" ) )
						{
							virtServer->keyFile = value;
						} else if ( !strccmp ( name, "CHAIN" ) )
						{
							virtServer->chainFile = value;
						} else if ( !strccmp ( name, "CIPHER_LIST" ) )
						{
							virtServer->cipherList = value;
						} else if ( !strccmp ( name, "CIPHER_SUITE" ) )
						{
							virtServer->cipherSuite = value;
						} else if ( !strccmp ( name, "MAKE" ) )
						{
							virtServer->apProjectBuildCommand = value;
						} else if ( !strccmp ( name, "logFormat" ) )
						{
							if ( logFormats.find ( name ) != logFormats.end() )
							{
								printf ( "Error: duplicate log format definition \"%s\".\r\n", name );
							} else
							{
								_token ( inLine, " \t", 2, name );
								_alltrim ( name, name );
								_upper ( name, name );
								strcat_s ( name, sizeof ( name ), " " );

								strcpy_s ( value, sizeof ( value ), inLine + _attoken ( inLine, " \t", 3 ) );
								_alltrim ( value, value );

								logFormats[name] = value;
							}
						} else if ( !strccmp ( name, "customLog" ) )
						{
							_token ( inLine, " \t", 2, name );
							_alltrim ( name, name );
							_upper ( name, name );
							strcat_s ( name, sizeof ( name ), " " );

							strcpy_s ( value, sizeof ( value ), inLine + _attoken ( inLine, " \t", 3 ) );
							_alltrim ( value, value );

							if ( logFormats.find ( name ) == logFormats.end() )
							{
								printf ( "Error: unknown log format \"%s\"\r\n", name );
							} else
							{
								virtServer->logs.push_back ( transLog ( logFormats.find ( name )->second.c_str(), value, true ) );
							}
						} else if ( !strccmp ( name, "path_page" ) )
						{
							_token ( value, "=", 1, subValue1 );
							_token ( value, "=", 2, subValue2 );

							stringi path = subValue1;
							if ( path[path.size ( ) - 1] != '/' )
							{
								path += "/";
							}

							// build our file name (without extension)
							virtServer->pathPageMap[pathString ( path )] = subValue2;
						} else
						{
							if ( *value && *name )
							{
								if ( value[strlen ( value ) - 1] == '/' )
								{
									value[strlen ( value ) - 1] = 0;
								}
								if ( value[strlen ( value ) - 1] == '\\' )
								{
									value[strlen ( value ) - 1] = 0;
								}

								virtServer->vars.push_back ( webServerVar (  stringi ( name ), stringi ( value ) ) );
							} else
							{
								printf ( "Error: unknown server configuration command \"%s\".\r\n", inLine );
							}
						}
						break;
					case CONFIG_REALMS:
						_token ( inLine, "=", 1, name );
						_token ( inLine, "=", 2, value );
						_token ( inLine, "=", 3, disabled );

						_alltrim ( name, name );
						_alltrim ( value, value );
						_alltrim ( disabled, disabled );

						{
							auto realmIt = realms.begin ();
							while ( realmIt != realms.end () )
							{
								if ( !strccmp ( name, (*realmIt)->name.c_str () ) )
								{
									printf ( "Error: realm \"%s\" declared more than once\r\n", inLine );
									break;
								}
								realmIt++;
							}

							if ( realmIt == realms.end () )
							{
								realms.push_back ( new webServerAuthRealm ( stringi ( name ), value, "", *disabled ? true : false ) );
								realm = realms.back ();
								realmPathMap[pathString ( (char*)realm->path.c_str (), realm->path.length () )] = realm;
							}
						}
						break;

					case CONFIG_GROUPS:
						{
							auto groupIt = groups.begin ();
							while ( groupIt != groups.end () )
							{
								if ( !strccmp ( inLine, (*groupIt)->name.c_str () ) )
								{
									printf ( "Error: group %s declared more than once\r\n", inLine );
									break;
								}
								groupIt++;
							}
							if ( groupIt == groups.end () )
							{
								authEntries.push_back ( new webServerAuth () );
								auth = authEntries[authEntries.size () - 1];

								auth->type = webServerAuth::authGroup;
								auth->disabled = !strccmp ( disabled, "disabled" ) ? true : false;

								group = new webAuthGroup ();
								group->name = inLine;
								group->auth = auth;

								groups.push_back ( group );
							}
						}
						break;

					case CONFIG_USER_REALM:
						_token ( inLine, ":=", 1, name );
						_token ( inLine, ":=", 2, value );
						_token ( inLine, ":=", 3, disabled );

						_alltrim ( name, name );
						_alltrim ( value, value );
						_alltrim ( disabled, disabled );

						/* here dir is actually the password */

						if ( !strccmp ( name, "p3p" ) )
						{
							/* this is actually the privacy policy entry for the realm */
							if ( realm->p3p.size() )
							{
								printf ( "error: only 1 compact privacy policy per realm allowed\r\n" );
							} else 
							{
								if ( realm )
								{
									realm->p3p = value;
								} else
								{
									printf ( "error: p3p only allowed during real configuration\r\n" );
								}
							}
						} else if ( !strccmp ( name, "software" ) )
						{
							authEntries.push_back ( new webServerAuth() );
							auth = authEntries[authEntries.size() - 1];

							auth->type					= webServerAuth::authCallback;
							auth->disabled				= !strccmp ( disabled, "disabled" ) ? true : false;
							auth->callback.funcName		= value;
							realm->accessList.push_back ( auth );
						} else if ( *value )
						{
							authEntries.push_back ( new webServerAuth() );
							auth = authEntries[authEntries.size() - 1];

							auth->type					= webServerAuth::authIndividual;
							auth->disabled				= !strccmp ( disabled, "disabled" ) ? true : false;
							auth->individual.name		= name;
							auth->individual.pw			= value;
							auth->individual.basic		= auth->individual.name + ":" + auth->individual.pw;
							{
								char *tmp;
								tmp = base64Encode ( (unsigned char *)auth->individual.basic.c_str(), (uint32_t)auth->individual.basic.length() );
								auth->individual.basic = stringi ( "Basic " ) + tmp;
								free ( tmp );
							}
							realm->accessList.push_back ( auth );
						} else
						{
							auto groupIt = groups.begin ();
							while ( groupIt != groups.end() )
							{
								if ( !strccmp ( inLine, (*groupIt)->name.c_str() ) )
								{
									break;
								}
								groupIt++;
							}
							if ( groupIt == groups.end() )
							{
								printf ( "Error: unknown group \"%s\".\r\n", name );
							} else
							{
								realm->accessList.push_back ( (*groupIt)->auth );
							}
						}
						break;
					case CONFIG_USER_GROUP:
						_token ( inLine, ":", 1, name );
						_token ( inLine, ":=", 2, value );
						_token ( inLine, "=", 2, disabled );

						_alltrim ( name, name );
						_alltrim ( value, value );
						_alltrim ( disabled, disabled );

						/* here dir is actually the password */
						if ( !strccmp ( name, "software" ) )
						{
							authEntries.push_back ( new webServerAuth() );
							auth = authEntries[authEntries.size() - 1];

							auth->type					= webServerAuth::authCallback;
							auth->disabled				= !strccmp ( disabled, "disabled" ) ? true : false;
							auth->callback.funcName		= value;

							if ( !group )
							{
								group->auth->group.groupList.push_back ( auth );
							} else
							{
								printf ( "error: illegal group definition\r\n" );
							}
						} else if ( *value )
						{
							authEntries.push_back ( new webServerAuth() );
							auth = authEntries[authEntries.size() - 1];

							auth->type					= webServerAuth::authIndividual;
							auth->disabled				= !strccmp ( disabled, "disabled" ) ? true : false;
							auth->individual.name		= name;
							auth->individual.pw			= value;
							auth->individual.basic		= auth->individual.name + ":" + auth->individual.pw;
							{
								char *tmp;
								tmp = base64Encode ( (unsigned char *)auth->individual.basic.c_str(), (uint32_t)auth->individual.basic.length() );
								auth->individual.basic = stringi ( "Basic " ) + tmp;
								free ( tmp );
							}
							group->auth->group.groupList.push_back ( auth );
						} else
						{
							auto groupIt = groups.begin ();
							while ( groupIt != groups.end() )
							{
								if ( !strccmp ( inLine, (*groupIt)->name.c_str() ) )
								{
									break;
								}
								groupIt++;
							}
							if ( groupIt == groups.end() )
							{
								printf ( "Error: unknown group \"%s\".\r\n", name );
							} else
							{
								group->auth->group.groupList.push_back ( (*groupIt)->auth );
							}
						}
						break;
					case CONFIG_VIRTUAL_SERVERS:
						virtualServers.push_back ( virtServer = new webVirtualServer ( virtualServerDefault ) );

						size_t num;
						num = _numtoken ( inLine, "," );
						for ( loop = 0; loop < num; loop++ )
						{
							_token ( inLine, ",", (int)loop + 1, name );
							_alltrim ( name, name );

							virtServer->alias.push_back ( name );
						}
						virtServer->name = virtServer->alias[0];
						virtualServerMap[statString ( (char *)virtServer->name.c_str(), virtServer->name.length() )] = virtServer;
						break;
					default:
						break;
				}
			}
		}
	}

	for ( auto it : virtualServers )
	{
		if ( !it->name.size ( ) )
		{
			it->serverName = it->alias[0];
		}
		if ( !it->defaultPages.size () )
		{
			it->defaultPages.push_back ( "index" );
		}
	}

	for ( auto groupIt = groups.begin(); groupIt != groups.end(); groupIt++ )
	{
		delete *groupIt;
	}

	if ( this->isSecure )
	{
		for ( auto webVirtIt = virtualServers.begin(); webVirtIt != virtualServers.end(); webVirtIt++ )
		{
			char *certificateFile;
			char *sslPassword;
			char *keyFile;
			char *chainFile;
			char *cipherList;
			char *cipherSuite;

			certificateFile	= (char *)((*webVirtIt)->certificateFile.length() ? (*webVirtIt)->certificateFile.c_str() : 0);
			sslPassword		= (char *)((*webVirtIt)->sslPassword.length() ? (*webVirtIt)->sslPassword.c_str() : 0);
			keyFile			= (char *)((*webVirtIt)->keyFile.length() ? (*webVirtIt)->keyFile.c_str() : 0);
			chainFile		= (char *)((*webVirtIt)->chainFile.length() ? (*webVirtIt)->chainFile.c_str() : 0);
			cipherList		= (char *) ((*webVirtIt)->cipherList.length ( ) ? (*webVirtIt)->cipherList.c_str ( ) : 0);
			cipherSuite		= (char *)((*webVirtIt)->cipherSuite.length() ? (*webVirtIt)->cipherSuite.c_str() : 0);

			if ( !webServerSLLAddContext ( *webVirtIt, certificateFile, keyFile, chainFile, cipherList, cipherSuite, sslPassword ) )
			{
				printf ( "Error: unable to initialize ssl context\r\n" );
				throw errorNum::scINTERNAL;
			}
		}
	}

	h2Settings = (uint8_t *)malloc( 512 );
	uint8_t *next = h2Settings + sprintf( (char *)h2Settings, "%s", "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n" );
	auto setting = new (next) H2Settings( h2SETTINGS_MAX_CONCURRENT_STREAMS, 100 );
	setting++;
	new (next) H2Settings( h2SETTINGS_ENABLE_PUSH, 0 );
	setting++;
	new (next) H2Settings( h2SETTINGS_HEADER_TABLE_SIZE, 4096 );
	setting++;
	new (next) H2Settings( h2SETTINGS_INITIAL_WINDOW_SIZE, 65536 );
	setting++;
	new (next) H2Settings( h2SETTINGS_MAX_FRAME_SIZE, 65536 - sizeof( H2Frame ) );
	setting++;
	new (next) H2Settings( h2SETTINGS_MAX_HEADER_LIST_SIZE, 16384 );
	setting++;
	h2SettingsLen = (uint8_t *)setting - h2Settings;
}

