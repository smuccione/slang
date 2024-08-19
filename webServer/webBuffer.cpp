/*
	WEB Server header file

*/

#include "webBuffer.h"

DWORD vmPageSize = []()
{
	SYSTEM_INFO	sysInfo;
	GetSystemInfo ( &sysInfo );
	return sysInfo.dwPageSize;
}();

static char itaStr [10000][4];
static bool initIToAStr ( void )
{
	for ( int loop = 0; loop < 10000; loop++ )
	{
		char tmp[5];
		sprintf ( tmp, "%04d", loop );
		memcpy ( itaStr[loop], tmp, 4 );
	}
	return true;
}
static bool initIToAStr_ = initIToAStr();

uint32_t fastIToA ( int32_t value, char *str )
{
	if ( value < 0 )
	{
		*(str++) = '-';
		value = -value;
	}

	char		*orig = str;
	int32_t	 part;

	if ( value > 99999999 )
	{
		part = value / 100000000;
		if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 100000000;

		part = value /= 10000;
		memcpy ( str, itaStr[part], 4 );
		str += 4;
		value -= part * 1000000;

		part = value /= 1000;
		memcpy ( str, itaStr[part], 4 );
		str += 4;
		value -= part * 1000;

		memcpy ( str, itaStr[value], 4 );
	} else if ( value > 10000 )
	{
		part = value / 10000;
		if ( part > 999 )
		{
			*(str++) = itaStr[part][0];
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 99 )
		{
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 10000;

		memcpy ( str, itaStr[value], 4 );
	} else if ( value > 999 )
	{
		*(str++) = itaStr[value][0];
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 99 )
	{
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 9 )
	{
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else
	{
		*(str++) = itaStr[value][3];
	}

	if ( orig == str ) *(str++) = '0';		// didn't emit any characters

	return (uint32_t)(str - orig);
}

uint32_t fastIToA( int64_t value, char *str )
{
	if ( value < 0 )
	{
		*(str++) = '-';
		value = -value;
	}

	char		*orig = str;
	int64_t	 part;

	if ( value > 99999999 )
	{
		part = value / 100000000;
		if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 100000000;

		part = value /= 10000;
		memcpy( str, itaStr[part], 4 );
		str += 4;
		value -= part * 1000000;

		part = value /= 1000;
		memcpy( str, itaStr[part], 4 );
		str += 4;
		value -= part * 1000;

		memcpy( str, itaStr[value], 4 );
	} else if ( value > 10000 )
	{
		part = value / 10000;
		if ( part > 999 )
		{
			*(str++) = itaStr[part][0];
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 99 )
		{
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 10000;

		memcpy( str, itaStr[value], 4 );
	} else if ( value > 999 )
	{
		*(str++) = itaStr[value][0];
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 99 )
	{
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 9 )
	{
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else
	{
		*(str++) = itaStr[value][3];
	}

	if ( orig == str ) *(str++) = '0';		// didn't emit any characters

	return (uint32_t)(str - orig);
}

uint32_t fastIToA ( uint32_t value, char *str )
{
	char		*orig = str;
	uint32_t	 part;

	if ( value > 99999999 )
	{
		part = value / 100000000;
		if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 100000000;

		part = value /= 10000;
		memcpy ( str, itaStr[part], 4 );
		str += 4;
		value -= part * 1000000;

		part = value /= 1000;
		memcpy ( str, itaStr[part], 4 );
		str += 4;
		value -= part * 1000;

		memcpy ( str, itaStr[value], 4 );
	} else if ( value > 10000 )
	{
		part = value / 10000;
		if ( part > 999 )
		{
			*(str++) = itaStr[part][0];
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 99 )
		{
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 10000;

		memcpy ( str, itaStr[value], 4 );
	} else if ( value > 999 )
	{
		*(str++) = itaStr[value][0];
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 99 )
	{
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 9 )
	{
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else
	{
		*(str++) = itaStr[value][3];
	}

	if ( orig == str ) *(str++) = '0';		// didn't emit any characters

	return (uint32_t)(str - orig);
}

uint32_t fastIToA ( size_t value, char *str )
{
	char		*orig = str;
	size_t	 part;

	if ( value > 99999999 )
	{
		part = value / 100000000;
		if ( part > 999 )
		{
			*(str++) = itaStr[part][0];
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 99 )
		{
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 100000000;

		part = value / 10000;
		memcpy ( str, itaStr[part], 4 );
		str += 4;
		value -= part * 10000;

		memcpy ( str, itaStr[value], 4 );
	} else if ( value > 10000 )
	{
		part = value / 10000;
		if ( part > 999 )
		{
			*(str++) = itaStr[part][0];
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 99 )
		{
			*(str++) = itaStr[part][1];
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else if ( part > 9 )
		{
			*(str++) = itaStr[part][2];
			*(str++) = itaStr[part][3];
		} else
		{
			*(str++) = itaStr[part][3];
		}
		value -= part * 10000;

		memcpy ( str, itaStr[value], 4 );
	} else if ( value > 999 )
	{
		*(str++) = itaStr[value][0];
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 99 )
	{
		*(str++) = itaStr[value][1];
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else if ( value > 9 )
	{
		*(str++) = itaStr[value][2];
		*(str++) = itaStr[value][3];
	} else
	{
		*(str++) = itaStr[value][3];
	}

	if ( orig == str ) *(str++) = '0';		// didn't emit any characters

	return (uint32_t)(str - orig);
}

