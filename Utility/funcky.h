/*
   Direct Access scripting language/evaluator header

*/

#pragma once

#include "Utility/settings.h"

#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>

#define S_HEX       1
#define S_ALPHA     2
#define S_SYMBOL    4
#define S_SYMBOLB   8
#define S_DIGIT    16
#define S_NUM      32
#define S_SPACE    64
#define S_EOL     128
#define S_URL	  256

#define fglstrccmp(a,b) ( ((((*(char*)(a)) | 0x20) == (*((char*)(b)) | 0x20)) && !strccmp ( (a), (b) )) ? 0 : 1 )
//#define fglstrccmp(a,b) (strccmp ( (a), (b) ))

// the first (char const *) cast it to force conversion operator if n is an object.
// the (unsigned char *) cast is to ensure that the subsequent * does not lose the high order bit
#define _isurl(n)     ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_URL				)
#define _ishex(n)     ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_HEX				)
#define _isnum(n)     ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_NUM				)
#define _issymbol(n)  ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_SYMBOL				)
#define _isalphac(n)  ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_ALPHA				)
#define _issymbolb(n) ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_SYMBOLB			)
#define _isnum(n)     ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_NUM				)
#define _isdigit(n)   ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_DIGIT				)
#define _isspace(n)   ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_SPACE				)
#define _isspacex(n)  ( sType[(size_t)*(unsigned char const *)((char const *)n)] & (S_SPACE | S_EOL)	)
#define _iseol(n)     ( sType[(size_t)*(unsigned char const *)((char const *)n)] & S_EOL				)

extern	unsigned int sType[256];

extern	char const *aShortMonths[];
extern  char const *aShortDays[];
extern	char const *aMonths[];
extern	char const *aDays[];

#define isTERMINATOR(chr)    ( !(chr) || ((chr)==';') || ((chr) == '\r') || ((chr) == '\n') || ((chr) == 26) )
#define isTERMINATOREOL(chr) ( !(chr) || ((chr) == '\r') || ((chr) == '\n') || ((chr) == 26) )

/* funcky headers to get things to compile */
	
extern			char const	*_firstchar		( char const *str );
extern			char	*_firstchar			( char  *str );
extern			char	*_alltrim			( char const *strIn, char *strOut );
extern			int		 _chrany			( char const *token, char const *str );
extern			int		 _isalpha			( char const *str, int num );
extern			int		 _isupper			( char const *str, int num );
extern			int		 _islower			( char const *str, int num );
extern			int		 _numtoken			( char const *str, char const *token );
extern			char	*_token				( char const *str, char const *token, int num, char *buff );
extern			char	*_tokenn			( char const *str, char const *token, int num, char *buff, int buffMaxLen );
extern			const char	*_arg			( int );
extern			int		 _argc				( void );
extern unsigned int		 _crc32				( unsigned char *buff, unsigned int start, int len );
extern uint64_t			 _flen				( int handle );
extern unsigned int		 jtoi				( int date, int *day, int *month, int *year );
extern unsigned	int		 itoj				( int year, int month, int day );
extern			char	*_ltrim				( char const *src, char *dst );
extern			char	*_rtrim				( char const *src, char *dst );
extern			char	*_fwebname			( char const *src, char *dest );
extern			char	*_fmerge			( char const *tmpl, char const *fname, char *dest, size_t destSize );
extern			char	*_fmake				( char const *drive, char const *path, char const *name, char const *ext, char *dest, size_t destSize );
extern			unsigned long _isfile		( char const *fname );
extern			unsigned long _isDirectory	( char const *fName );
extern			char	*_getenv			( char const *varName, char *dest );
extern			char	*_strtran			( char const *str, char const *oldSeq, char const *newSeq, int startOccur, int count, char *destStr );
extern			char	*strnmerge			( char *dest, char const *src, int len );
extern			char	*_upper				( char const *src, char *dest );
extern			char	*_uppern			( char *dest, char const *src, int len );
extern			int		 _dosgetdate		( int *year, int *month, int *day );
extern			char	*memcpyset			( char *dest, char const *src, size_t totalLen, size_t srcLen, char setChr );
extern			char	*trimpunct			( char const *src, char *dst );
extern			int		 _at				( char const *str, char const *subStr );
extern			int		 _ati				( char const *substr, char const *str);
extern			int		 _atdiff			( char const *str1, char const *str2 );
extern			int		 _atdiffi			( char const *str1, char const *str2 );
extern			int		 _atlast			( char const *substr, char const *str );
extern			int		 _atlasti			( char const *substr, char const *str );
extern			int		 _atnext			( char const *substr, char const *str, int occur );
extern			int		 _atnexti			( char const *substr, char const *str, int occur );
extern			int		 _attoken			( char const *str, char const *token, int num );
extern			char	*_rjust				( char *str, char *dest );
extern			char	*_ljust				( char *str, char *dest );
extern			char	*_untrim			( char *str, int len, char *dest );
extern			long	 _jdate				( void );
extern			char	*_webGMT			( time_t lTime, char *buffer );
extern			char	*_webLocal			( time_t lTime, char *buffer );
extern			char	*_xmlTime			( time_t lTime, char *buffer );
extern			char	*_ipToA				( unsigned long ip, char *buff );
extern			char	*_strextract		( char const *str, char const *delim, int occur, char *dest );
extern			char	*_strextractq		( char const *strParam, char const *delim, int occur, char *dest );
extern			char	*_strswap			( char const *strParam, char const *delim, char *swap );
extern			int		 _chrcount			( char const *chr, char const *str );
extern			int		 _chrcounti			( char const *chr, char const *str );
extern			char	*_chrswap			( char const *str, char const *oldChr, char const *newChr, char *dest );
extern			long	 datej				( char const *date );
extern			int		 _stratnext			( char const *subStr, char const *str, int occur, int startpos );
extern			int		 _stratnexti		( char const *subStr, char const *str, int occur, int startpos );
extern			int		 _stratprev			( char const *subStr, char const *str, int occur, int startpos );
extern			int		 _stratprevi		( char const *subStr, char const *str, int occur, int startpos );
extern			int		 _strcount			( char const *str1, char const *str2 );
extern			int		 _strcounti			( char const *str1, char const *str2 );
extern			char	*_strcapfirst		( char const *str, char *dst );
extern			char	*_encHexExtended	( char const *src, int len );
extern unsigned long	 encHexExtendedX	( char const *src, int len, char *dest );
extern			int		 _convHexExtendedx	( unsigned char *src, int srcLen, char *dst, int dstLen );
extern			int		  encHexExtended_s	( char const *src, int srcLen, char *dst, int dstLen );
extern			int		 _rand				( void );
extern			int		 freadline			( int iHandle, char *buff, int buffLen  );
extern			char	*_unQuote			( char *buff, char *dest );
extern			char	*_unDecimate		( char *str, char *dest );
extern			char	*_subToken			( char const *str, char *token, char *delim, char *dest );
extern unsigned long	 _AToIp				( char const *buff );
extern			long	 webGMTToTime		( char const *str );
extern			char	*_soundEx			( char const *str, char *dest, int resolution );
extern			char	*_subTokeni			( char const *str, char *token, char *delim, char *value );
extern			char	*_strswapi			( char const *str, char *toSwap, char *swapWith );
extern			char	*_mailGMT			( time_t lTime, char *buffer );
extern			char	*_parseUserid		( char const *url, char *dest );
extern			char	*_parsePassword		( char const *url, char *dest );
extern			char	*_parseDomain		( char const *url, char *dest );
extern			char	*_parseURI			( char const *url, char *uri );
extern unsigned int		 webAddrFromHost	( char const *host );

extern			char	*dtoc				( long lDate, char *szDest );
extern			char	*dtos				( long lDate, char *szDest );
extern			int		 checkScript		( char const *value, size_t len = 0 );
extern uint64_t			_hashKey			( char const *ptr );

extern std::string		base64Decode		( std::string const &p1 );
extern std::string		base64Encode		( std::string const &p1 );

extern bool				isSameFile			( char const *file1, char const *file2 );
extern bool				wildcardMatch		( char const *str, char const *match );