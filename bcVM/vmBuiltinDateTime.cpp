/*
SLANG support functions

*/


#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"

static int dow ( long lDate )
{
	return( (int) (( lDate + 1 ) % 7 ) + 1 );
}

static char const *cdow ( long lDate )
{
	int iDay = dow( lDate );

	return ( aDays[iDay - 1] );
}

static char const *cmonth ( long lDate )
{
	int dnc;
	int iMonth;

	jtoi( lDate, &dnc, &iMonth, &dnc );

	return ( aMonths[iMonth - 1] );
}

static unsigned int ctod ( char const *szDate )
{
	int  iDay;
	int  iMonth;
	int  iYear;
	char cTmp[ 6 ] = {};

	/* get month */
	cTmp[0] = szDate[0];
	cTmp[1] = szDate[1];
	cTmp[2] = 0;
	iMonth = atoi( cTmp );

	/* get day */
	cTmp[0] = szDate[3];
	cTmp[1] = szDate[4];
	cTmp[2] = 0;
	iDay = atoi( cTmp );

	/* get year */
	if( strlen ( szDate ) > 8 )
	{
		cTmp[0] = szDate[6];
		cTmp[1] = szDate[7];
		cTmp[2] = szDate[8];
		cTmp[3] = szDate[9];
	} else
	{
		cTmp[0] = '1';
		cTmp[1] = '9';
		cTmp[2] = szDate[6];
		cTmp[3] = szDate[7];
	}
	cTmp[4] = 0;

	iYear = atoi( cTmp );

	return( itoj( iYear, iMonth, iDay ));
}

static int day ( long lDate )
{
	int iDay   = 0;
	int iMonth = 0;
	int iYear  = 0;

	jtoi( lDate, &iYear, &iMonth, &iDay );

	return( iDay );
}

static VAR_STR webGMT ( vmInstance *instance, nParamType nParams )

{
	time_t		 lTime;
	char		 dst[30];
	VAR			*val;

	if ( nParams == 1 )
	{
		val = nParams[0];
		DEREF ( val );

		switch ( TYPE ( val ) )
		{
			case slangType::eLONG:
				lTime = val->dat.l;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	} else
	{
		lTime = time ( 0 );
	}

	_webGMT ( lTime, dst );
	return VAR_STR ( instance, dst );
}

static VAR_STR mailGMT ( vmInstance *instance, nParamType nParams )

{
	time_t		 lTime;
	char		 dst[30];
	VAR			*val;

	lTime = (long)time ( 0 );

	if ( nParams == 1 )
	{
		val = nParams[0];
		DEREF ( val );

		switch ( TYPE ( val ) )
		{
			case slangType::eLONG:
				lTime = val->dat.l;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
			}
	}

	_mailGMT ( lTime, dst );
	return VAR_STR ( instance, dst );
}

static VAR_STR webLocal ( vmInstance *instance, nParamType nParams )

{
	time_t		 lTime;
	char		 dst[30];
	VAR			*val;

	lTime = (long)time ( 0 );

	if ( nParams == 1 )
	{
		val = nParams[0];
		DEREF ( val );

		switch ( TYPE ( val ) )
		{
			case slangType::eLONG:
				lTime = val->dat.l;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
			}
	}

	_webLocal ( lTime, dst );
	return VAR_STR ( instance, dst );
}

static VAR_STR xmlTime ( vmInstance *instance, nParamType nParams )
{
	time_t		 lTime;
	char		 dst[30];
	VAR			*val;

	lTime = (long)time ( 0 );

	if ( nParams == 1 )
	{
		val = nParams[0];
		DEREF ( val );

		switch ( TYPE ( val ) )
		{
			case slangType::eLONG:
				lTime = val->dat.l;
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}
	}

	_xmlTime ( lTime, dst );
	return VAR_STR ( instance, dst );
}

static VAR_STR normalizeDate ( vmInstance *instance, char const *date, nParamType numParams )

{
	VAR		*var;
	char  const *dstSep;
	char	 dst[12];
	long	 julian;
	int		 iDay;
	int		 iMonth;
	int		 iYear;

	julian = datej ( date );

	jtoi( julian, &iYear, &iMonth, &iDay );

	if ( numParams == 1 )
	{
		var = numParams[0];
		DEREF ( var );

		if ( TYPE ( var ) == slangType::eNULL )
		{
			dstSep = "-";
		} else
		{
			if ( TYPE ( var ) != slangType::eSTRING )
			{
				throw errorNum::scINVALID_PARAMETER;
			}

			if ( var->dat.str.len != 1 )
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			dstSep = var->dat.str.c;
		}
	} else
	{
		dstSep = "-";
	}

	_snprintf_s ( dst, 12, _TRUNCATE, "%02i%c%02i%c%04i", iMonth, *dstSep, iDay, *dstSep, iYear );

	return VAR_STR ( instance, dst );
}

static int month ( long lDate )
{
	int iDay   = 0;
	int iMonth = 0;
	int iYear  = 0;

	jtoi( lDate, &iYear, &iMonth, &iDay );

	return( iMonth );	/* jtoi returns 1 based month */
}

static void datef ( struct tm *utcTime, char const *fmt, BUFFER &buff )
{
	if ( !utcTime )
	{
		buff.write ( "<error>" );
		return;
	}

	while ( *fmt )
	{
		if ( !strncmp ( fmt, "MMMM", 4 )  )
		{
			fmt += 4;
			buff.write ( aMonths[utcTime->tm_mon] );
		} else if ( !strncmp ( fmt, "MMM", 3 ) )
		{
			fmt += 3;
			buff.write ( aShortMonths[utcTime->tm_mon] );
		} else if ( !strncmp ( fmt, "MM", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%02i", utcTime->tm_mon + 1);
		} else if ( !strncmp ( fmt, "mm", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%2i", utcTime->tm_mon + 1);
		} else if ( !strncmp ( fmt, "YYYY", 4 ) )
		{
			fmt += 4;
			buff.printf ( "%04i", utcTime->tm_year + 1900 );
		} else if ( !strncmp ( fmt, "YY", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%02i", (utcTime->tm_year + 1900) % 100);
		} else if ( !strncmp ( fmt, "HH", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%02i", utcTime->tm_hour );
		} else if ( !strncmp ( fmt, "hh", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%02i", (utcTime->tm_hour % 12) ? (utcTime->tm_hour % 12) : 12 );
		} else if ( !strncmp ( fmt, "MN", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%02i", utcTime->tm_min );
		} else if ( !strncmp ( fmt, "SS", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%02i", utcTime->tm_sec );
		} else if ( !strncmp ( fmt, "AP", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%2s", utcTime->tm_hour >= 12 ? "PM" : "AM" );
		} else if ( !strncmp ( fmt, "ap", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%2s", utcTime->tm_hour >= 12 ? "pm" : "am");
		} else if ( !strncmp ( fmt, "DDDD", 4 ) )
		{
			fmt += 4;
			buff.write ( aDays[utcTime->tm_wday] );
		} else if ( !strncmp ( fmt, "DDD", 3 ) )
		{
			fmt += 3;
			buff.printf ( "%2i", utcTime->tm_mday );
			switch ( utcTime->tm_mday )
			{
				case 1:
				case 21:
				case 31:
					buff.write ( "st" );
					break;
				case 2:
				case 22:
					buff.write ( "nd" );
					break;
				case 3:
				case 23:
					buff.write ( "rd" );
					break;
				default:
					buff.write ( "th" );
					break;
			}
		} else if ( !strncmp ( fmt, "DD", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%02i", utcTime->tm_mday );
		} else if ( !strncmp ( fmt, "dd", 2 ) )
		{
			fmt += 2;
			buff.printf ( "%2i", utcTime->tm_mday );
		} else 
		{
			buff.put ( *(fmt++) );
		}
	}
}

static VAR_STR date_ ( vmInstance *instance, nParamType nParams )
{
	char  const *fmt;
	VAR			*var;
	time_t		 lTime = time ( 0 );

	switch ( nParams )
	{
		case 2:
			var = nParams[1];
			DEREF ( var );

			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					if ( !var->dat.l )
					{
						lTime = time ( 0 );
					}
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			// intentional fall-through
		case 1:
			var = nParams[0];
			DEREF ( var );

			switch ( TYPE ( var ) )
			{
				case slangType::eSTRING:
					fmt = var->dat.str.c;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case 0:
			fmt = "MM-DD-YYYY";
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}

	struct tm	 utcTime;
	localtime_s ( &utcTime, &lTime );

	BUFFER buff;
	datef ( &utcTime, fmt, buff );
	return VAR_STR ( instance, buff );
}

static VAR_STR dateGMT_ ( vmInstance *instance, nParamType nParams )
{
	char const  *fmt;
	VAR			*var;
	time_t		 lTime;
	struct tm	 utcTime;

	time ( &lTime );

	switch ( nParams )
	{
		case 0:
			fmt = "MM-DD-YYYY";
			break;
		case 2:
			var = nParams[1];
			DEREF ( var );

			switch ( TYPE ( var ) )
			{
				case slangType::eLONG:
					if ( var->dat.l )
					{
						lTime = var->dat.l;
					}
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			// intentional fall-through
		case 1:
			var = nParams[0];
			DEREF ( var );
			switch ( TYPE ( var ) )
			{
				case slangType::eSTRING:
					fmt = var->dat.str.c;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}

	gmtime_s ( &utcTime, &lTime );

	BUFFER buff;
	datef ( &utcTime, fmt, buff );
	return VAR_STR ( instance, buff );
}

static VAR_STR jdatef ( vmInstance *instance, long date, nParamType nParams )
{
	char  const *fmt;
	VAR			*var;
	struct tm	 tm;
	time_t		 lTime;

	if ( nParams )
	{
		if ( nParams == 1 )
		{
			var = nParams[0];
			DEREF ( var );

			switch ( TYPE ( var ) )
			{
				case slangType::eSTRING:
					fmt = var->dat.str.c;
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		} else
		{
			throw errorNum::scINVALID_PARAMETER;
		}
	} else
	{
		fmt = "MM/DD/YYYY";
	}

	lTime = time( 0 );
	localtime_s ( &tm, &lTime );

	jtoi ( date, &tm.tm_year, &tm.tm_mon, &tm.tm_mday );

	tm.tm_wday = (date + 1) % 7;
	tm.tm_year -= 1900; /* for tm struct standard... */
	tm.tm_mon  -= 1;	/* jtoi returns 1 based month... tm struct is 0 based */

	BUFFER buff;
	datef ( &tm, fmt, buff );
	return VAR_STR ( instance, buff );
}

static VAR_STR vmDateFormatJ (  vmInstance *instance, long date, char const *fmt )
{
	struct tm	 tm = {};

	jtoi ( date, &tm.tm_year, &tm.tm_mon, &tm.tm_mday );

	tm.tm_wday = (date + 1) % 7;
	tm.tm_year -= 1900;
	tm.tm_mon  -= 1;	/* jtoi returns 1 based month... tm struct is 0 based */

	BUFFER buff;
	datef ( &tm, fmt, buff );

	return VAR_STR ( instance, buff );
}

static VAR_STR time_ ( vmInstance *instance )
{
	char			buff[9];
	time_t			lTime;
	struct	tm		utcTime;

	lTime = time ( 0 );
	localtime_s ( &utcTime, &lTime );

	sprintf ( buff, "%02i:%02i:%02i", utcTime.tm_hour, utcTime.tm_min, utcTime.tm_sec );

	return VAR_STR ( instance, buff );
}

static VAR_STR timeh ( vmInstance *instance )
{
	char			buff[12];
	SYSTEMTIME		systemTime;
	FILETIME		fileTime;
	ULARGE_INTEGER	uLarge;

	GetSystemTime (	&systemTime );
	SystemTimeToFileTime ( &systemTime, &fileTime );

	uLarge = *((ULARGE_INTEGER *)&fileTime);
	fileTime = *((FILETIME *)&uLarge);

	FileTimeToSystemTime ( &fileTime, &systemTime );

	sprintf ( buff, "%02u:%02u:%02u.%02u", systemTime.wHour, systemTime.wMinute, systemTime.wSecond, (uint32_t)(systemTime.wMilliseconds / 10) );

	return VAR_STR ( instance, buff );
}

static unsigned long vmQueryPerformanceCounter ( VAR_STR )
{
	LARGE_INTEGER		frequency;
	LARGE_INTEGER		pTime;

	QueryPerformanceFrequency	( &frequency );
	QueryPerformanceCounter		( &pTime );

	return ( (unsigned long)(pTime.QuadPart / (frequency.QuadPart / 1000)) );
}

static VAR_STR timetostr(  vmInstance *instance, char const *Time, int Type)
{
	char buf[12];
	int	 i;

	switch ( Type )
	{
		case 0:
			strncpy_s ( buf, 12, Time, _TRUNCATE );
			buf[7] = 0;

			/* 12 hour format */
			i = atoi ( Time );
			buf[5] = ' ';
			if ( i < 12 )
			{
				/* am */
				sprintf ( buf, "%02i", i == 0 ? 12 : i );
				buf[6] = 'a';
			} else
			{
				/* pm */
				sprintf ( buf, "%02i", i == 12 ? 12 : i - 12 );
				buf[6] = 'p';
			}
			buf[2] = ':';
			break;
		case 1:
			strncpy_s ( buf, 5, Time, _TRUNCATE );
			buf[5] = 0;
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	return VAR_STR ( instance, *buf == '0' ? buf + 1 : buf );
}

static VAR_STR secondstostr( vmInstance *instance, time_t time, int Type)
{
	char		buff[12];
	time_t		lTime;
	struct	tm	utcTime;

	lTime = (long)time;
	localtime_s ( &utcTime, &lTime );;

	sprintf ( buff, "%02i:%02i", utcTime.tm_hour, utcTime.tm_min );

	switch ( Type )
	{
		case 0:
			buff[7] = 0;

			/* 12 hour format */
			buff[5] = ' ';
			if ( utcTime.tm_hour < 12 )
			{
				/* am */
				sprintf ( buff, "%02i", utcTime.tm_hour == 0 ? 12 : utcTime.tm_hour );
				buff[6] = 'a';
			} else
			{
				/* pm */
				sprintf ( buff, "%02i", utcTime.tm_hour == 12 ? 12 : utcTime.tm_hour - 12 );
				buff[6] = 'p';
			}
			buff[2] = ':';
			break;
		case 1:
			buff[5] = 0;
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	return VAR_STR ( instance, *buff == '0' ? buff + 1 : buff );
}

int64_t vmSecondsh ( void )
{
	SYSTEMTIME		systemTime;
	FILETIME		fileTime;
	ULARGE_INTEGER	uLarge = {};

	GetSystemTime (	&systemTime );
	SystemTimeToFileTime ( &systemTime, &fileTime );

	uLarge.HighPart = fileTime.dwHighDateTime;
	uLarge.LowPart	= fileTime.dwLowDateTime;

	return int64_t( uLarge.QuadPart / 100000 );
}

static int64_t seconds_ ( void )
{
	return (int64_t)time ( 0 );
}

static VAR_STR vmDtoc ( vmInstance *instance, int date )
{
	char	retStr[12];

	dtoc ( date, retStr );

	return VAR_STR ( instance, retStr );
}

static VAR_STR vmDtos ( vmInstance *instance, int date )
{
	char	retStr[12];

	dtos ( date, retStr );

	return VAR_STR ( instance, retStr );
}

int setTime ( int hour, int min, int sec )

{
	SYSTEMTIME	t;

	GetLocalTime ( &t );
	t.wHour			= hour;
	t.wMinute		= min;
	t.wSecond		= sec;
	t.wMilliseconds	=0;
	SetLocalTime ( &t );

	return ( 1 );
}

int setDate ( int year, int month, int day)

{
	SYSTEMTIME	t;

	GetLocalTime ( &t );
	t.wYear			= year;
	t.wMonth		= month;
	t.wDay			= day;
	t.wMilliseconds	=0;
	SetLocalTime ( &t );

	return ( 1 );
}

int64_t  cUTCGet ( vmInstance *instance, VAR *var )
{
	VAR				*val;
	SYSTEMTIME		 systemTime;
	FILETIME		 fileTime;

	memset ( &systemTime, 0, sizeof ( systemTime ) );

	val = classIVarAccess ( var, "MONTH" );
	if ( TYPE ( val ) != slangType::eLONG )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	systemTime.wMonth = (uint16_t) val->dat.l;

	val = classIVarAccess ( var, "DAY" );
	if ( TYPE ( val ) != slangType::eLONG )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	systemTime.wDay = (uint16_t) val->dat.l;

	val = classIVarAccess ( var, "YEAR" );
	if ( TYPE ( val ) != slangType::eLONG )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	systemTime.wYear = (uint16_t) val->dat.l;

	val = classIVarAccess ( var, "HOUR" );
	if ( TYPE ( val ) != slangType::eLONG )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	systemTime.wHour = (uint16_t) val->dat.l;

	val = classIVarAccess ( var, "MINUTE" );
	if ( TYPE ( val ) != slangType::eLONG )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	systemTime.wMinute = (uint16_t) val->dat.l;

	val = classIVarAccess ( var, "SECOND" );
	if ( TYPE ( val ) != slangType::eLONG )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	systemTime.wSecond = (uint16_t) val->dat.l;

	val = classIVarAccess ( var, "MILLISECONDS" );
	systemTime.wMilliseconds = (uint16_t) val->dat.l;

	SystemTimeToFileTime ( &systemTime, &fileTime );

	return int64_t (((ULARGE_INTEGER *) &fileTime)->QuadPart / 100000);
}

void cUTCTime ( vmInstance *instance, VAR *var, VAR *time )
{
	SYSTEMTIME		 systemTime;
	FILETIME		 fileTime = {};
	char  const		*tmpC;
	size_t			 tmpLen;
	char			 buff[5];

	ULARGE_INTEGER	 uLarge = {};

	switch ( TYPE ( time ) )
	{
		case slangType::eLONG:
			if ( time->dat.l )
			{
				uLarge.QuadPart = (int64_t) time->dat.l * 100000;
				*((ULARGE_INTEGER *) &fileTime) = uLarge;
				FileTimeToSystemTime ( &fileTime, &systemTime );
			} else
			{
				GetSystemTime ( &systemTime );
			}
			break;
		case slangType::eDOUBLE:
			if ( (bool)time->dat.d )
			{
				uLarge.QuadPart = (ULONGLONG) time->dat.d * 100000;
				*((ULARGE_INTEGER *) &fileTime) = uLarge;
				FileTimeToSystemTime ( &fileTime, &systemTime );
			} else
			{
				GetSystemTime ( &systemTime );
			}
			break;
		case slangType::eSTRING:
			memset ( &systemTime, 0, sizeof ( systemTime ) );

			tmpC = time->dat.str.c;
			tmpLen = time->dat.str.len;
			if ( tmpLen < 8 )
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			memcpy ( buff, tmpC, 4 );
			buff[4] = 0;
			systemTime.wYear = atoi ( buff );

			memcpy ( buff, tmpC + 4, 2 );
			buff[2] = 0;
			systemTime.wMonth = atoi ( buff );

			memcpy ( buff, tmpC + 6, 2 );
			buff[2] = 0;
			systemTime.wDay = atoi ( buff );

			tmpLen -= 8;
			tmpC += 8;

			if ( tmpLen >= 6 )
			{
				memcpy ( buff, tmpC, 2 );
				buff[2] = 0;
				systemTime.wHour = atoi ( buff );

				memcpy ( buff, tmpC + 2, 2 );
				buff[2] = 0;
				systemTime.wMinute = atoi ( buff );

				memcpy ( buff, tmpC + 4, 2 );
				buff[2] = 0;
				systemTime.wSecond = atoi ( buff );

				tmpLen -= 6;
				tmpC += 6;

				if ( tmpLen )
				{
					if ( tmpLen != 3 )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					memcpy ( buff, tmpC, 3 );
					buff[3] = 0;
					systemTime.wMilliseconds = atoi ( buff );
				}
			}

			// fill in the other fields.
			SystemTimeToFileTime ( &systemTime, &fileTime );
			FileTimeToSystemTime ( &fileTime, &systemTime );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	*classIVarAccess ( var, "DAY" ) = VAR ( systemTime.wDay );
	*classIVarAccess ( var, "DAYOFWEEK" ) = VAR ( systemTime.wDayOfWeek );
	*classIVarAccess ( var, "HOUR" ) = VAR ( systemTime.wHour );
	*classIVarAccess ( var, "MILLISECONDS" ) = VAR ( systemTime.wMilliseconds );
	*classIVarAccess ( var, "MINUTE" ) = VAR ( systemTime.wMinute );
	*classIVarAccess ( var, "MONTH" ) = VAR ( systemTime.wMonth );
	*classIVarAccess ( var, "SECOND" ) = VAR ( systemTime.wSecond );
	*classIVarAccess ( var, "YEAR" ) = VAR ( systemTime.wYear );
}

void cLOCALTime ( vmInstance *instance, VAR *var, VAR *time )
{
	SYSTEMTIME		 systemTime;
	FILETIME		 fileTime = {};
	char  const		*tmpC;
	size_t			 tmpLen;
	char			 buff[5];

	ULARGE_INTEGER	 uLarge = {};

	switch ( TYPE ( time ) )
	{
		case slangType::eLONG:
			if ( time->dat.l )
			{
				uLarge.QuadPart = (int64_t) time->dat.l * 100000;
				*((ULARGE_INTEGER *) &fileTime) = uLarge;
				FileTimeToSystemTime ( &fileTime, &systemTime );
			} else
			{
				GetLocalTime ( &systemTime );
			}
			break;
		case slangType::eDOUBLE:
			if ( (bool) time->dat.d )
			{
				uLarge.QuadPart = (ULONGLONG) time->dat.d * 100000;
				*((ULARGE_INTEGER *) &fileTime) = uLarge;
				FileTimeToSystemTime ( &fileTime, &systemTime );
			} else
			{
				GetLocalTime ( &systemTime );
			}
			break;
		case slangType::eSTRING:

			memset ( &systemTime, 0, sizeof ( systemTime ) );

			tmpC = time->dat.str.c;
			tmpLen = time->dat.str.len;
			if ( tmpLen < 8 )
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			memcpy ( buff, tmpC, 4 );
			buff[4] = 0;
			systemTime.wYear = atoi ( buff );

			memcpy ( buff, tmpC + 4, 2 );
			buff[2] = 0;
			systemTime.wMonth = atoi ( buff );

			memcpy ( buff, tmpC + 6, 2 );
			buff[2] = 0;
			systemTime.wDay = atoi ( buff );

			tmpLen -= 8;
			tmpC += 8;

			if ( tmpLen >= 6 )
			{
				memcpy ( buff, tmpC, 2 );
				buff[2] = 0;
				systemTime.wHour = atoi ( buff );

				memcpy ( buff, tmpC + 2, 2 );
				buff[2] = 0;
				systemTime.wMinute = atoi ( buff );

				memcpy ( buff, tmpC + 4, 2 );
				buff[2] = 0;
				systemTime.wSecond = atoi ( buff );

				tmpLen -= 6;
				tmpC += 6;

				if ( tmpLen )
				{
					if ( tmpLen != 3 )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					memcpy ( buff, tmpC, 3 );
					buff[3] = 0;
					systemTime.wMilliseconds = atoi ( buff );
				}
			}

			// fill in the other fields.
			SystemTimeToFileTime ( &systemTime, &fileTime );
			FileTimeToSystemTime ( &fileTime, &systemTime );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	*classIVarAccess ( var, "DAY" ) = VAR ( systemTime.wDay );
	*classIVarAccess ( var, "DAYOFWEEK" ) = VAR ( systemTime.wDayOfWeek );
	*classIVarAccess ( var, "HOUR" ) = VAR ( systemTime.wHour );
	*classIVarAccess ( var, "MILLISECONDS" ) = VAR ( systemTime.wMilliseconds );
	*classIVarAccess ( var, "MINUTE" ) = VAR ( systemTime.wMinute );
	*classIVarAccess ( var, "MONTH" ) = VAR ( systemTime.wMonth );
	*classIVarAccess ( var, "SECOND" ) = VAR ( systemTime.wSecond );
	*classIVarAccess ( var, "YEAR" ) = VAR ( systemTime.wYear );
}

const int64_t nano100SecInDay = (int64_t) 100 * 60 * 60 * 24;
const int64_t nano100SecInHour = (int64_t) 100 * 60 * 60;
const int64_t nano100SecInMin = (int64_t) 100 * 60;
const int64_t nano100SecInSec = (int64_t) 100;

void cUTCAdd ( vmInstance *instance, VAR *var, int day, int hour, int min, int sec )
{
	VAR			newTime{};
	int64_t		time;

	time = (int64_t) cUTCGet ( instance, var );

	time += day * nano100SecInDay;
	time += hour * nano100SecInHour;
	time += min * nano100SecInMin;
	time += sec * nano100SecInSec;

	newTime.type = slangType::eDOUBLE;
	newTime.dat.d = (double) time;

	cUTCTime ( instance, var, &newTime );

	instance->result = *var;
}

VAR_STR cUTCFormat ( vmInstance *instance, VAR *var, VAR *fmt )
{
	VAR	*val;
	int	 fmtOffset;

	if ( TYPE ( fmt ) != slangType::eSTRING )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	fmtOffset = 0;

	BUFFER buff;

	while ( *(fmt->dat.str.c + fmtOffset) )
	{
		if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "MMMM", 4 ) )
		{
			fmtOffset += 4;
			val = classIVarAccess ( var, "MONTH" );
			if ( val->dat.l < 1 || val->dat.l > 12 ) throw errorNum::scINVALID_PARAMETER;
			buff.write ( aMonths[val->dat.l - 1] );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "MMM", 3 ) )
		{
			fmtOffset += 3;
			val = classIVarAccess ( var, "MONTH" );
			if ( val->dat.l < 1 || val->dat.l > 12 ) throw errorNum::scINVALID_PARAMETER;
			buff.write ( aShortMonths[val->dat.l - 1] );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "MM", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "MONTH" );
			if ( val->dat.l < 1 || val->dat.l > 12 ) throw errorNum::scINVALID_PARAMETER;
			buff.printf ( "%02zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "mm", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "MONTH" );
			if ( val->dat.l < 1 || val->dat.l > 12 ) throw errorNum::scINVALID_PARAMETER;
			buff.printf ( "%2zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "YYYY", 4 ) )
		{
			fmtOffset += 4;
			val = classIVarAccess ( var, "YEAR" );
			buff.printf ( "%04zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "YY", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "YEAR" );
			buff.printf ( "%02zu", val->dat.l % 100 );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "HH", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "HOUR" );
			buff.printf ( "%02zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "hh", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "HOUR" );
			buff.printf ( "%02zu", (val->dat.l % 12) ? (val->dat.l % 12) : 12 );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "MN", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "MINUTE" );
			buff.printf ( "%02zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "SS", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "SECOND" );
			buff.printf ( "%02zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "MS", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "MILLISECONDS" );
			buff.printf ( "%03zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "AP", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "HOUR" );
			buff.printf ( "%2s", val->dat.l >= 12 ? "PM" : "AM" );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "ap", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "HOUR" );
			buff.printf ( "%2s", val->dat.l >= 12 ? "pm" : "am" );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "DDDD", 4 ) )
		{
			fmtOffset += 4;
			val = classIVarAccess ( var, "DAYOFWEEK" );
			if ( val->dat.l < 0 || val->dat.l > 6) throw errorNum::scINVALID_PARAMETER;
			buff.write ( aDays[val->dat.l] );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "DDD", 3 ) )
		{
			fmtOffset += 3;
			val = classIVarAccess ( var, "DAY" );
			buff.printf ( "%2zu", val->dat.l );
			switch ( val->dat.l )
			{
				case 1:
				case 21:
				case 31:
					buff.write ( "st" );
					break;
				case 2:
				case 22:
					buff.write ( "nd" );
					break;
				case 3:
				case 23:
					buff.write ( "rd" );
					break;
				default:
					buff.write ( "th" );
					break;
			}
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "DD", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "DAY" );
			buff.printf ( "%02zu", val->dat.l );
		} else if ( !strncmp ( (fmt->dat.str.c + fmtOffset), "dd", 2 ) )
		{
			fmtOffset += 2;
			val = classIVarAccess ( var, "DAY" );
			buff.printf ( "%2zu", val->dat.l );
		} else
		{
			buff.put ( *(fmt->dat.str.c + fmtOffset++) );
		}
	}
	return VAR_STR ( instance, buff );
}


void builtinDateTimeInit ( vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		FUNC ( "CDOW", cdow ) CONST PURE;
		FUNC ( "CMONTH", cmonth ) CONST PURE;
		FUNC ( "CTOD", ctod ) CONST PURE;
		FUNC ( "DAY", day ) CONST PURE;
		FUNC ( "DOW", dow ) CONST PURE;
		FUNC ( "DTOC", vmDtoc ) CONST PURE;
		FUNC ( "DTOS", vmDtos ) CONST PURE;
		FUNC ( "MONTH", month ) CONST PURE;
		FUNC ( "DATEJ", datej ) CONST PURE;
		FUNC ( "JDATEF", jdatef ) CONST PURE;
		FUNC ( "NORMALIZEDATE", normalizeDate ) CONST PURE;
		FUNC ( "JDATE", _jdate ) CONST PURE;

		FUNC ( "DATE", date_ ) CONST;
		FUNC ( "DATEFORMATJ", vmDateFormatJ ) CONST PURE;
		FUNC ( "TIME", time_ ) CONST;
		FUNC ( "TIMEH", timeh ) CONST;

		FUNC ( "TIMETOSTR", timetostr );
		FUNC ( "SECONDSTOSTR", secondstostr );
		FUNC ( "SECONDS", seconds_ );
		FUNC ( "SECONDSH", vmSecondsh );
		FUNC ( "HRTIME", vmQueryPerformanceCounter );
		FUNC ( "WEBTOTIME", webGMTToTime ) CONST PURE;

		FUNC ( "TIMESET", setTime ) PURE;
		FUNC ( "DATESET", setDate ) PURE;

		FUNC ( "DATEGMT", dateGMT_ ) CONST;
		FUNC ( "MAILGMT", mailGMT ) CONST;
		FUNC ( "WEBGMT", webGMT ) CONST;
		FUNC ( "WEBLOCAL", webLocal ) CONST;
		FUNC ( "XMLTIME", xmlTime ) CONST;

		CLASS( "localTime" );
			METHOD( "new", cLOCALTime, DEF ( 2, "0" ) );
			METHOD( "get", cUTCGet ) CONST;
			METHOD( "format", cUTCFormat );
			METHOD( "add", cUTCAdd );
			IVAR ( "year" );
			IVAR ( "month" );
			IVAR ( "dayOfWeek" );
			IVAR ( "day" );
			IVAR ( "hour" );
			IVAR ( "minute" );
			IVAR ( "second" );
			IVAR ( "milliSeconds" );
		END;

		CLASS( "utcTime" );
			METHOD( "new", cUTCTime, DEF( 2, "0" ) );
			METHOD( "get", cUTCGet ) CONST;
			METHOD( "format", cUTCFormat );
			METHOD( "add", cUTCAdd );
			IVAR( "year" );
			IVAR( "month" );
			IVAR( "dayOfWeek" );
			IVAR( "day" );
			IVAR( "hour" );
			IVAR( "minute" );
			IVAR( "second" );
			IVAR( "milliSeconds" );
			END;
	END;
}
