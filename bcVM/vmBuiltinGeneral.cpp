/*
		IO Support functions

*/

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"
#include "bcInterpreter/op_variant.h"
#include "Target/fileCache.h"

#include "Target/common.h"
#include "Target/vmConf.h"
#include "Target/vmTask.h"

#include "version/versionLib.h"

#include "bcDebugger\bcDebugger.h"
#include "compilerPreprocessor\compilerPreprocessor.h"


#include <filesystem>
#include <io.h>
#include <fcntl.h>
#include <sstream>
#include <clocale>
#include <locale>
#include <iostream>
#include <iomanip>

static VAR_STR vmType ( vmInstance *instance, VAR *var )

{
	char const *ret;

	switch ( TYPE ( var ) )
	{
		case slangType::eDOUBLE:
		case slangType::eLONG:
			ret = "N";
			break;
		case slangType::eSTRING:
			ret = "C";
			break;
		case slangType::eARRAY_ROOT:
			ret = "A";
			break;
		case slangType::eCODEBLOCK_ROOT:
			ret = "B";
			break;
		case slangType::eREFERENCE:
			ret = "R";
			break;
		case slangType::eNULL:
			ret = "U";
			break;
		case slangType::eOBJECT_ROOT:
			ret = "O";
			break;
		case slangType::eATOM:
			ret = "F";
			break;
		default:
			ret = "E";
			break;
	}
	return VAR_STR ( ret, 1 );
}

static VAR_STR vmTypeX ( vmInstance *instance, VAR *var )
{
	char const *ret;

	switch ( TYPE ( var ) )
	{
		case slangType::eDOUBLE:
			ret = "double";
			break;
		case slangType::eLONG:
			ret = "integer";
			break;
		case slangType::eSTRING:
			ret = "string";
			break;
		case slangType::eARRAY_ROOT:
			switch ( TYPE ( var ) )
			{
				case slangType::eARRAY_SPARSE:
					ret = "dynamic array";
					break;
				case slangType::eARRAY_FIXED:
					ret = "fixed array";
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case slangType::eCODEBLOCK_ROOT:
			ret = "codeblock";
			break;
		case slangType::eNULL:
			ret = "null";
			break;
		case slangType::eOBJECT_ROOT:
			ret = "object";
			break;
		case slangType::eATOM:
			ret = "function";
			break;
		default:
			ret = "(internal)";
			break;
	}

	return VAR_STR ( ret );
}

static VAR_STR vmTypeOf ( vmInstance *instance, VAR *var )
{
	char const *ret;

	switch ( TYPE ( var ) )
	{
		case slangType::eDOUBLE:
		case slangType::eLONG:
			ret = "number";
			break;
		case slangType::eSTRING:
			ret = "string";
			break;
		case slangType::eARRAY_ROOT:
			ret = "array";
			break;
		case slangType::eCODEBLOCK_ROOT:
			ret = "codeblock";
			break;
		case slangType::eREFERENCE:
			ret = "reference";
			break;
		case slangType::eNULL:
			ret = "null";
			break;
		case slangType::eOBJECT_ROOT:
			ret = "object";
			break;
		default:
			ret = "(internal)";
			break;
	}

	return VAR_STR ( ret );
}

static void vmExcept ( VAR *v )
{
	if ( TYPE ( v ) == slangType::eLONG )
	{
		throw v->dat.l;
	} else
	{
		throw v;
	}
}

static void vmRelease ( vmInstance *instance, VAR *v )
{
	if ( v->dat.ref.v->type != slangType::eOBJECT )
	{
		return;
	}
	if ( v->dat.ref.v != v->dat.ref.v[1].dat.ref.v )
	{
		return;
	}
	v->dat.ref.v->type = slangType::eNULL;
	v->type = slangType::eNULL;
}

static void omCollect ( vmInstance *instance )
{
#ifdef _DEBUG 
	instance->validateHeap ( );
#endif
	instance->om->collect ( );
#ifdef _DEBUG 
	instance->validateHeap ( );
#endif
}

static void omMemory ( vmInstance *instance, size_t fwhich )
{
#ifdef _DEBUG 
	instance->validateHeap ( );
#endif
	instance->om->collect ( );
#ifdef _DEBUG 
	instance->validateHeap ( );
#endif
}

static void throwInternal_ ( void )
{
	throw errorNum::scINTERNAL;
}

static void waSelect ( vmInstance *instance, VAR *var )
{
	switch ( TYPE ( var ) )
	{
		case slangType::eLONG:
			instance->workareaTable->select ( var->dat.l );
			break;
		case slangType::eSTRING:
			instance->workareaTable->select ( var->dat.str.c );
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

static uint32_t waUsed ( vmInstance *instance )
{
	return (uint32_t) instance->workareaTable->inUse ( instance->workareaTable->getDefault ( ) );
}

constexpr size_t MAX_LINE_LEN = 2048;

static size_t display ( int outputDesc, char const *s )
{
	size_t len;

	if ( outputDesc )
	{
		len = _write ( outputDesc, s, (unsigned int)strlen ( s ) );
		fprintf ( stdout, "%s", s );
	} else
	{
		len = printf ( "%s", s );
	}
	return len;
}

static size_t vmPrint ( vmInstance *instance, nParamType numParams )
{
	char		 buff[64];
	char		*ptr;
	int			 dec;
	int			 sign;
	size_t		 loop;
	size_t		 loop2;
	size_t		 r;
	VAR			*var;

	for ( r = loop = 0; loop < numParams; loop++ )
	{
		var = numParams[(uint32_t) loop];
		while ( TYPE ( var ) == slangType::eREFERENCE ) var = var->dat.ref.v;

		switch ( TYPE ( var ) )
		{
			case slangType::eSTRING:
				r += display ( instance->outputDesc, var->dat.str.c );
				break;
			case slangType::eLONG:
				r += display ( instance->outputDesc, _i64toa ( var->dat.l, buff, 10 ) );
				break;
			case slangType::eDOUBLE:
				ptr = _ecvt ( var->dat.d, 16, &dec, &sign );
				*buff = 0;

				if ( sign )
				{
					strcat_s ( buff, sizeof ( buff ), "-" );
				}
				if ( dec )
				{
					strncat_s ( buff, sizeof ( buff ), ptr, dec );
				} else
				{
					strcat_s ( buff, sizeof ( buff ), "0" );
				}
				strncat_s ( buff, sizeof ( buff ), ".", sizeof ( buff ) - strlen ( buff ) - 1 );
				strncat_s ( buff, sizeof ( buff ), ptr + dec, sizeof ( buff ) - strlen ( buff ) - 1 );

				loop2 = strlen ( buff ) - 1;
				while ( (loop2 > (uint32_t) dec) && (*(buff + loop2) == '0') )
				{
					loop2--;
				}
				if ( *(buff + loop2) == '.' )
				{
					loop2++;
					*(buff + loop2) = 0;
					strncat_s ( buff, sizeof ( buff ), "0", sizeof ( buff ) - strlen ( buff ) - 1 );
				} else
				{
					loop2++;
					*(buff + loop2) = 0;
				}

				r += display ( instance->outputDesc, buff );
				break;
			case slangType::eOBJECT_ROOT:
				r += display ( instance->outputDesc, "(OBJECT) " );
				display ( instance->outputDesc, var->dat.ref.v->dat.obj.classDef->name );
				break;
			case slangType::eARRAY_ROOT:
				r += display ( instance->outputDesc, "(ARRAY)" );
				break;
			case slangType::eCODEBLOCK_ROOT:
				r += display ( instance->outputDesc, "(BLOCK)" );
				break;
			case slangType::eATOM:
				r += display ( instance->outputDesc, "(FUNCTION)" );
				break;
			case slangType::eNULL:
				r += display ( instance->outputDesc, "(NULL)" );
				break;
			default:
				throw errorNum::scINTERNAL;
		}
	}

	return  r;
}

static size_t vmPrintln ( vmInstance *instance, nParamType num )

{
	size_t r;

	r = vmPrint ( instance, num );

	display ( instance->outputDesc, "\r\n" );

	return (r + 2);
}

static char *padCommas ( char *res, size_t resLen, char const *buff, size_t buffLen )

{
	char		*res2;
	const char	*tmp2;
	const char	*decPt;
	ptrdiff_t	 size;
	int			 ctr;

	decPt = strchr ( buff, '.' );

	if ( decPt )
	{
		/* find number of spaces until decimal pt */
		size = decPt - buff;
		tmp2 = decPt - 1;
	} else
	{
		size = static_cast<ptrdiff_t>(strlen ( buff ));
		tmp2 = buff + strlen ( buff ) - 1;
	}

	res2 = res + size + (size / 3) - (size % 3 == 0) - 1;

	/* tack a null on the end before we lose the spot */
	res2[1] = 0;

	ctr = 0;
	while ( tmp2 >= buff )
	{
		if ( ++ctr > 3 )
		{
			ctr = 1;
			*(res2--) = ',';
		}
		*(res2--) = *(tmp2--);
	}

	if ( decPt )
	{
		/* copy off decimal portion */
		size = static_cast<ptrdiff_t>(strlen ( res ));
		strncpy_s ( res + size, resLen - size - 1, decPt, _TRUNCATE );
	}

	return (res);
}

class commaFacet : public std::numpunct<char>
{
protected:
	virtual char do_thousands_sep () const
	{
		return ',';
	}

	virtual std::string do_grouping () const
	{
		return "\03";
	}
};

static	std::locale	 commaLocale ( std::locale (), new commaFacet () );

static int64_t outputFmt ( vmInstance *instance, BUFFER &buff, char const *fmt, nParamType numParams )
{
	bool				 leftAlign = false;
	bool				 signPrefix = false;
	bool				 zeroPad = false;
	bool				 blankPad = false;
	bool				 commas = false;
	int64_t				 numModify = 0;
	int64_t				 width = 0;
	int64_t				 iValue = 0;
	int64_t				 precision = -1;
	double				 dValue = 0;
	VAR					*var;
	int64_t				 nParam;
	char const			*tmpP;
	char				 numBuff[512];
	char				 prefix[2]{};
	int64_t				 prefixLen;
	int64_t				 len;
	char				 fmtString[16]{};
	int64_t				 fmtStringLen;
	char				 commaBuff[128];

	nParam = 0;

	while ( *fmt )
	{
		if ( *fmt && (*fmt != '%') )
		{
			while ( *fmt && (*fmt != '%') )
			{
				if ( *fmt == '\\' )
				{
					fmt++;
				}
				buff.put ( *(fmt++) );
			}
		}

		if ( *fmt == '%' )
		{
			fmt++;

			std::stringstream 	output;

			while ( 1 )
			{
				switch ( *fmt )
				{
					case ' ':
						blankPad = true;
						fmt++;
						continue;
					case '+':
						signPrefix = true;
						fmt++;
						continue;
					case '-':
						leftAlign = true;
						fmt++;
						continue;
					case '0':
						zeroPad = true;
						fmt++;
						continue;
					case '#':
						numModify = true;
						fmt++;
						continue;
					case ',':
						commas = true;
						fmt++;
						continue;
					default:
						break;
				}
				// we'll only get here if we don't have a format character
				break;
			}

			width = 0;
			precision = 0;

			if ( (*fmt >= '0') && (*fmt <= '9') )
			{
				do {
					width *= 10;
					width += int64_t( *(fmt++) )- '0';
				} while ( (*fmt >= '0') && (*fmt <= '9') );
			} else if ( *fmt == '*' )
			{
				// width passed in as parameter 
				if ( nParam < numParams )
				{
					var = numParams[nParam++];

					while ( TYPE ( var ) == slangType::eREFERENCE )
					{
						var = var->dat.ref.v;
					}

					switch ( TYPE ( var ) )
					{
						case slangType::eLONG:
							width = var->dat.l;
							break;
						case slangType::eDOUBLE:
							width = (long) var->dat.d;
							break;
						case slangType::eSTRING:
							width = atol ( _firstchar ( var->dat.str.c ) );
							break;
						default:
							throw errorNum::scINVALID_PARAMETER;
							break;
					}
				} else
				{
					throw errorNum::scINVALID_PARAMETER;
				}
			}

			if ( *fmt == '.' )
			{
				fmt++;
				precision = 0;
				if ( (*fmt >= '0') && (*fmt <= '9') )
				{
					do
					{
						precision *= 10;
						precision += int64_t( *(fmt++) ) - '0';
					} while ( (*fmt >= '0') && (*fmt <= '9') );
				} else if ( *fmt == '*' )
				{
					// precision passed in as parameter 
					if ( nParam < numParams )
					{
						var = numParams[nParam++];

						while ( TYPE ( var ) == slangType::eREFERENCE )
						{
							var = var->dat.ref.v;
						}

						switch ( TYPE ( var ) )
						{
							case slangType::eLONG:
								precision = var->dat.l;
								break;
							case slangType::eDOUBLE:
								precision = (long) var->dat.d;
								break;
							case slangType::eSTRING:
								precision = atol ( _firstchar ( var->dat.str.c ) );
								break;
							default:
								throw errorNum::scINVALID_PARAMETER;
								break;
						}
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}
				}
			}

			switch ( *fmt )
			{
				case 's':
					if ( nParam < numParams )
					{
						var = numParams[nParam++];

						while ( TYPE ( var ) == slangType::eREFERENCE )
						{
							var = var->dat.ref.v;
						}

						switch ( TYPE ( var ) )
						{
							case slangType::eLONG:
								_i64toa ( var->dat.l, numBuff, 10 );
								tmpP = numBuff;
								len = (uint32_t) strlen ( numBuff );
								break;
							case slangType::eDOUBLE:
								_i64toa ( (unsigned long) var->dat.d, numBuff, 10 );
								tmpP = numBuff;
								len = (uint32_t) strlen ( numBuff );
								break;
							case slangType::eSTRING:
								if ( var->dat.str.c && var->dat.str.len )
								{
									tmpP = var->dat.str.c;
									len = (uint32_t) var->dat.str.len;
								} else
								{
									tmpP = "";
									len = 0;
								}
								break;
							case slangType::eNULL:
								tmpP = "";
								len = 0;
								break;
							default:
								throw errorNum::scINVALID_PARAMETER;
								break;
						}
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					if ( len > width )
					{
						width = len;
					}

					if ( width > precision )
					{
						precision = width;
					}

					if ( (len > precision) && (precision != -1) )
					{
						len = precision;
					}

					if ( leftAlign )
					{
						while ( len )
						{
							buff.put ( *(tmpP++) );
							len--;
							width--;
						}

						// print any trailing spaces
						while ( width )
						{
							buff.put ( ' ' );
							width--;
						}
					} else
					{
						// pad it out first
						while ( len < width )
						{
							buff.put ( ' ' );
							width--;
						}

						while ( len )
						{
							buff.put ( *(tmpP++) );
							len--;
						}
					}
					break;
				case 'c':
					if ( nParam < numParams )
					{
						var = numParams[nParam++];

						while ( TYPE ( var ) == slangType::eREFERENCE )
						{
							var = var->dat.ref.v;
						}

						switch ( TYPE ( var ) )
						{
							case slangType::eLONG:
								buff.put ( (char) var->dat.l );
								break;
							case slangType::eDOUBLE:
								buff.put ( (char) var->dat.d );
								break;
							case slangType::eSTRING:
								if ( var->dat.str.c && var->dat.str.len )
								{
									buff.put ( var->dat.str.c[0] );
								}
								break;
							default:
								throw errorNum::scINVALID_PARAMETER;
								break;
						}
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}
					break;
					// integer output formatting
				case 'u':
				case 'i':
				case '0':
				case 'x':
				case 'X':
				case 'd':
					prefixLen = 0;

					if ( nParam < numParams )
					{
						var = numParams[nParam++];

						while ( TYPE ( var ) == slangType::eREFERENCE )
						{
							var = var->dat.ref.v;
						}

						switch ( TYPE ( var ) )
						{
							case slangType::eLONG:
								iValue = var->dat.l;
								break;
							case slangType::eDOUBLE:
								iValue = (int64_t) var->dat.d;
								break;
							case slangType::eSTRING:
								iValue = _atoi64 ( _firstchar ( var->dat.str.c ) );
								break;
							case slangType::eNULL:
								iValue = 0;
								break;
							default:
								throw errorNum::scINVALID_PARAMETER;
								break;
						}
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					switch ( *fmt )
					{
						case 'd':
						case 'i':
							if ( signPrefix && (iValue >= 0) )
							{
								prefix[prefixLen++] = '+';
							} else if ( blankPad && (iValue >= 0) )
							{
								prefix[prefixLen++] = ' ';
							} else if ( iValue < 0 )
							{
								prefix[prefixLen++] = '-';
								iValue = -iValue;
							}
							_i64toa ( iValue, numBuff, 10 );

							if ( commas )
							{
								strcpy_s ( numBuff, sizeof ( numBuff ), padCommas ( commaBuff, sizeof ( commaBuff ), numBuff, sizeof ( numBuff ) ) );
							}
							break;
						case 'u':
							_i64toa ( iValue, numBuff, 10 );
							if ( commas )
							{
								strcpy_s ( numBuff, sizeof ( numBuff ), padCommas ( commaBuff, sizeof ( commaBuff ), numBuff, sizeof ( numBuff ) ) );
							}
							break;
						case '0':
							if ( signPrefix && (iValue >= 0) )
							{
								prefix[prefixLen++] = '+';
							} else if ( blankPad && (iValue >= 0) )
							{
								prefix[prefixLen++] = ' ';
							}
							_i64toa ( iValue, numBuff, 8 );
							if ( commas )
							{
								strcpy_s ( numBuff, sizeof ( numBuff ), padCommas ( commaBuff, sizeof ( commaBuff ), numBuff, sizeof ( numBuff ) ) );
							}
							break;
						case 'x':
							if ( numModify )
							{
								prefix[prefixLen++] = '0';
								prefix[prefixLen++] = 'x';
							}
							_i64toa ( iValue, numBuff, 16 );
							break;
						case 'X':
							if ( numModify )
							{
								prefix[prefixLen++] = '0';
								prefix[prefixLen++] = 'X';
							}
							_i64toa ( iValue, numBuff, 16 );
							_upper ( numBuff, numBuff );
							break;
					}

					len = (uint32_t) strlen ( numBuff );

					// precision is the desired minimum length of the number to display 
					if ( precision < len )
					{
						precision = len;
					}

					if ( width < precision + prefixLen )
					{
						width = precision + prefixLen;
					}

					if ( leftAlign )
					{
						/* write out prefix */
						tmpP = prefix;
						while ( prefixLen )
						{
							buff.put ( *(tmpP++) );
							prefixLen--;
						}

						/* if we have less then precision digits then emit 0's */
						while ( len < precision )
						{
							buff.put ( '0' );
							width--;
							precision--;
						}

						/* write out the number itself */
						tmpP = numBuff;
						while ( len )
						{
							buff.put ( *(tmpP++) );
							len--;
							width--;
						}

						/* write out any left over padding */
						while ( width )
						{
							buff.put ( ' ' );
							width--;
						}
					} else
					{
						if ( prefixLen )
						{
							if ( zeroPad )
							{
								/* write out prefix */
								tmpP = prefix;
								while ( prefixLen )
								{
									buff.put ( *(tmpP++) );
									prefixLen--;
									width--;
								}
							}

							/* pad it to our maximum width */
							while ( precision < width )
							{
								if ( zeroPad )
								{
									buff.put ( '0' );
								} else
								{
									buff.put ( ' ' );
								}
								width--;
							}

							/* write out prefix */
							tmpP = prefix;
							while ( prefixLen )
							{
								buff.put ( *(tmpP++) );
								prefixLen--;
								width--;
							}
						} else
						{
							/* pad it to our maximum width */
							while ( precision + prefixLen < width )
							{
								if ( zeroPad )
								{
									buff.put ( '0' );
								} else
								{
									buff.put ( ' ' );
								}
								width--;
							}

							/* write out prefix */
							tmpP = prefix;
							while ( prefixLen )
							{
								buff.put ( *(tmpP++) );
								prefixLen--;
								width--;
								precision--;
							}
						}

						/* if we have less then precision digits then emit 0's */
						while ( len < precision )
						{
							buff.put ( '0' );
							width--;
							precision--;
						}

						/* write out the number itself */
						tmpP = numBuff;
						while ( len )
						{
							buff.put ( *(tmpP++) );
							len--;
							width--;
						}
					}
					break;
					// floating point output formatting
				case 'e':
				case 'g':
				case 'E':
				case 'f':
					prefixLen = 0;

					if ( nParam < numParams )
					{
						var = numParams[nParam++];

						while ( TYPE ( var ) == slangType::eREFERENCE )
						{
							var = var->dat.ref.v;
						}

						switch ( TYPE ( var ) )
						{
							case slangType::eLONG:
								dValue = (double) var->dat.l;
								break;
							case slangType::eDOUBLE:
								dValue = var->dat.d;
								break;
							case slangType::eSTRING:
								dValue = atof ( _firstchar ( var->dat.str.c ) );
								break;
							case slangType::eNULL:
								dValue = 0;
								break;
							default:
								throw errorNum::scINVALID_PARAMETER;
								break;
						}
					} else
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					if ( dValue < 0 )
					{
						prefix[prefixLen++] = '-';
						iValue = -iValue;
						dValue = -dValue;
						width--;
					}

					fmtStringLen = 0;

					fmtString[fmtStringLen++] = '%';
					if ( blankPad )
					{
						fmtString[fmtStringLen++] = ' ';
					}
					if ( numModify )
					{
						fmtString[fmtStringLen++] = '#';
					}
					if ( signPrefix )
					{
						fmtString[fmtStringLen++] = '+';
					}
					if ( leftAlign )
					{
						fmtString[fmtStringLen++] = '-';
					}
					if ( zeroPad )
					{
						fmtString[fmtStringLen++] = '0';
					}
					fmtString[fmtStringLen++] = '*';
					if ( precision >= 0 )
					{
						fmtString[fmtStringLen++] = '.';
						fmtString[fmtStringLen++] = '*';
					}
					fmtString[fmtStringLen++] = *fmt;
					fmtString[fmtStringLen++] = 0;

					if ( precision >= 0 )
					{
						_snprintf_s ( numBuff, sizeof ( numBuff ), _TRUNCATE, fmtString, width, precision, dValue );
					} else
					{
						_snprintf_s ( numBuff, sizeof ( numBuff ), _TRUNCATE, fmtString, width, dValue );
					}

					if ( commas )
					{
						width = (uint32_t) strlen ( numBuff );

						auto first = _firstchar ( numBuff );
						auto size = sizeof ( numBuff ) - (first - numBuff);
						strcpy_s ( first, size, padCommas ( commaBuff, sizeof ( commaBuff ), first, size ) );

						width = width - ((uint32_t) strlen ( numBuff ) - width);

						if ( precision >= 0 )
						{
							_snprintf_s ( numBuff, sizeof ( numBuff ), _TRUNCATE, fmtString, width, precision, dValue );
						} else
						{
							_snprintf_s ( numBuff, sizeof ( numBuff ), _TRUNCATE, fmtString, width, dValue );
						}
						first = _firstchar ( numBuff );
						size = sizeof ( numBuff ) - (first - numBuff);

						strcpy_s ( first, size, padCommas ( commaBuff, sizeof ( commaBuff ), first, size ) );
					}

					/* write out prefix */
					tmpP = prefix;
					while ( prefixLen )
					{
						buff.put ( *(tmpP++) );
						prefixLen--;
					}

					/* write out the number itself */
					tmpP = numBuff;
					while ( *tmpP )
					{
						buff.put ( *(tmpP++) );
					}
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}

			/* skip over the type just processed above */
			fmt++;
		}
	}

	buff.put ( (char) 0 );

	return (int64_t)buff.size ();
}

static uint64_t vmPrintf ( vmInstance *instance, char const *fmt, nParamType nParams )

{
	BUFFER	 buff;

	int64_t len;

	if ( (len = outputFmt ( instance, buff, fmt, nParams )) )
	{
		display ( instance->outputDesc, bufferBuff ( &buff ) );
	}

	return len;
}

static VAR_STR vmSprintf ( vmInstance *instance, char const *fmt, nParamType nParams )
{
	BUFFER		 buff;

	if ( outputFmt ( instance, buff, fmt, nParams ) )
	{
		return VAR_STR ( instance, buff );
	} else
	{
		return VAR_STR ( "" );
	}
}

static bool vmEngineIsDebug ( void )
{
#if defined ( _DEBUG )
	return true;
#else
	return false;
#endif
}

static void vmBuildSystemError ( vmInstance *instance, errorNum err )
{
	VAR		*arr;
	VAR		*stack;

	classNew ( instance, "systemError", 0 );

	*classIVarAccess ( &instance->result, "errno" ) = VAR ( int(err) );

	stringi errText = scCompErrorAsText ( int(err) );
	*classIVarAccess ( &instance->result, "description" ) = VAR_STR ( instance, errText.c_str ( ), errText.size ( ) );

	*classIVarAccess ( &instance->result, "line" ) = VAR ( instance->debug->GetCurrentLine ( instance->error.funcDef, (fglOp *) instance->error.opCode ) );	// NOLINT (performance-no-int-to-ptr)

	*classIVarAccess ( &instance->result, "functionName" ) = VAR_STR ( instance->error.funcDef->name );

	*classIVarAccess ( &instance->result, "fileName" ) = VAR_STR ( instance->error.funcDef->loadImage->srcFiles.getName ( instance->debug->GetCurrentSourceIndex ( instance->error.funcDef, (fglOp *) instance->error.opCode ) ) ); // NOLINT (performance-no-int-to-ptr)

	arr = classIVarAccess ( &instance->result, "callStack" );

	*arr = VAR_ARRAY ( instance );

	stack = instance->error.stack;

	vmCallStack callStack = vmCallStack ( );
	fglOp		*op = (fglOp *) instance->error.opCode;	// NOLINT(performance-no-int-to-ptr)

	while ( stack > instance->eval )
	{
		if ( stack->type == slangType::ePCALL )
		{
			callStack.entry.push_back ( vmCallstackEntry ( instance, stack->dat.pCall.func, instance->debug->GetCurrentLine ( stack->dat.pCall.func, op ), stack->dat.pCall.nParams, stack ) );
			op = stack->dat.pCall.op;
		}
		stack--;
	}

	char entry[1024];
	for ( int64_t loop = 0; loop < (int64_t)callStack.entry.size ( ); loop++ )
	{
		size_t len;
		len = (size_t) _snprintf_s ( entry, sizeof ( entry ) - 6, _TRUNCATE, "%s ( ", callStack.entry[loop].funcName.c_str() );

		for ( size_t loop2 = 0; loop2 < (int) callStack.entry[loop].params.size ( ); loop2++ )
		{
			len += (size_t) _snprintf_s ( entry + len, sizeof ( entry ) - 6 - len, _TRUNCATE, "%.32s", callStack.entry[loop].params[loop2]->getValue ( ) );
			if ( loop2 != callStack.entry[loop].params.size ( ) - 1 )
			{
				if ( len < sizeof ( entry ) - 6 )
				{
					len += (size_t) _snprintf_s ( entry + len, sizeof ( entry ) - 6 - len, _TRUNCATE, ", " );
				}
			}
		}

		if ( len < sizeof ( entry ) - 6 )
		{
			strncat_s ( entry, sizeof ( entry ), " )", _TRUNCATE );
		} else
		{
			strncat_s ( entry, sizeof ( entry ), "... )", _TRUNCATE );
		}

		arrayGet ( instance, arr, loop + 1 ) = VAR_STR ( instance, entry );
	}
}

static void fixedArrayEnumeratorNew ( vmInstance *instance, VAR_OBJ *object, VAR *arr )
{
	VAR	*arrVal;
	VAR	*indexVal;

	arrVal = classIVarAccess ( object, "$array" );
	indexVal = classIVarAccess ( object, "$index" );

	while ( TYPE ( arr ) == slangType::eREFERENCE ) arr = arr->dat.ref.v;

	*arrVal = *arr;

	indexVal->type = slangType::eLONG;
	indexVal->dat.l = arr->dat.ref.v->dat.arrayFixed.startIndex - 1;
}

static VAR_OBJ_TYPE<"fixedArrayEnumerator"> fixedArrayEnumeratorGetEnumerator ( vmInstance *instance, VAR_OBJ *object )
{
	return *object;
}

static VAR fixedArrayEnumeratorCurrent ( vmInstance *instance, VAR_OBJ *object )
{
	VAR	*arr;
	VAR	*index;

	arr = classIVarAccess ( object, "$array" );
	index = classIVarAccess ( object, "$index" );

	return arr->dat.ref.v[index->dat.l - arr->dat.ref.v->dat.arrayFixed.startIndex + 1];
}

static VAR fixedArrayEnumeratorIndex ( vmInstance *instance, VAR_OBJ *object )
{
	return *classIVarAccess ( object, "$index" );
}


static uint32_t fixedArrayEnumeratorMoveNext ( vmInstance *instance, VAR_OBJ *object )
{
	VAR	*arr;
	VAR	*index;

	arr = classIVarAccess ( object, "$array" );
	index = classIVarAccess ( object, "$index" );

	index->dat.l++;
	if ( index->dat.l > arr->dat.ref.v->dat.arrayFixed.endIndex )
	{
		return 0;
	}
	return 1;
}

static void sparseArrayEnumeratorNew ( vmInstance *instance, VAR_OBJ *object, VAR *arr )
{
	auto elem = classIVarAccess ( object, "$elem" );
	while ( TYPE ( arr ) == slangType::eREFERENCE ) arr = arr->dat.ref.v;

	*elem = VAR_REF ( object, arr->dat.ref.v );
}

static VAR_OBJ_TYPE<"sparseArrayEnumerator"> sparseArrayEnumeratorGetEnumerator ( vmInstance *instance, VAR_OBJ *object )
{
	return *object;
}

static VAR sparseArrayEnumeratorCurrent ( vmInstance *instance, VAR_OBJ *object )
{
	VAR	*elem;

	elem = classIVarAccess ( object, "$elem" );
	return *elem->dat.ref.v->dat.aElem.var;
}

static VAR sparseArrayEnumeratorIndex ( vmInstance *instance, VAR_OBJ *object )
{
	VAR	*elem;

	elem = classIVarAccess ( object, "$elem" );

	return elem->dat.ref.v->dat.aElem.elemNum;
}

static uint32_t sparseArrayEnumeratorMoveNext ( vmInstance *instance, VAR_OBJ *object )
{
	VAR	*elem;

	elem = classIVarAccess ( object, "$elem" );

	switch ( TYPE ( elem->dat.ref.v ) )
	{
		case slangType::eNULL:
			return 0;
		case slangType::eARRAY_SPARSE:
			if ( elem->dat.ref.v->dat.aSparse.v )
			{
				*elem = VAR_REF ( object, elem->dat.ref.v->dat.aSparse.v );
			} else
			{
				return 0;
			}
			return 1;
		case slangType::eARRAY_ELEM:
			if ( elem->dat.ref.v->dat.aElem.next )
			{
				*elem = *VAR_REF ( object, elem->dat.ref.v->dat.aElem.next );
				return 1;
			} else
			{
				return 0;
			}
			return 1;
		default:
			break;
	}
	return 0;
}

static void stringEnumeratorNew ( vmInstance *instance, VAR_OBJ *object, VAR *string )
{
	while ( TYPE ( string ) == slangType::eREFERENCE ) string = string->dat.ref.v;

	*classIVarAccess ( object, "$string" ) = VAR_REF ( string, string );
	*classIVarAccess ( object, "$offset" ) = VAR ( 0LL );
	*classIVarAccess ( object, "$current" ) = VAR_STR ( "" );
}

static VAR_OBJ_TYPE<"stringEnumerator">  stringEnumeratorGetEnumerator ( vmInstance *instance, VAR_OBJ *object )
{
	return *object;
}

static VAR_STR stringEnumeratorCurrent ( vmInstance *instance, VAR_OBJ *object )
{
	return *classIVarAccess ( object, "$current" );
}

static int64_t stringEnumeratorIndex ( vmInstance *instance, VAR_OBJ *object )
{
	return classIVarAccess ( object, "offset" )->dat.l;
}

static uint32_t stringEnumeratorMoveNext ( vmInstance *instance, VAR_OBJ *object )
{
	auto string = (*object)["$string"];
	auto offset = (*object)["$offset"];
	auto current = (*object)["$current"];

	if ( (size_t) offset->dat.l < string->dat.str.len )
	{
		*current = VAR_STR ( instance, &string->dat.str.c[offset->dat.l++], 1 );
		return 1;
	} else
	{
		return 0;
	}
}

class currency_punct : public std::numpunct<char>
{
	std::string do_grouping ( ) const
	{
		// group by 3's
		return "\03";
	}
	char do_decimal_point ( ) const
	{
		return '.';
	}
	char do_thousands_sep ( ) const
	{
		return ',';
	}
};

static VAR_STR strInterpolate ( vmInstance *instance, VAR *value, int32_t width, char const *fmt )
{
	stringi	result;
	char	fmtChar = '\0';
	int64_t	precision = 0;
	bool	zeroFill = false;
	int32_t multiplier = 1;

	if ( *fmt )
	{
		fmtChar = *(fmt++);
		if ( *fmt )
		{
			if ( *fmt == '0' )
			{
				zeroFill = true;
				fmt++;
			}
			precision = atoi ( fmt );
		}
	}

	while ( TYPE ( value ) == slangType::eREFERENCE ) value = value->dat.ref.v;

	switch ( TYPE ( value ) )
	{
		case slangType::eOBJECT_ROOT:
			PUSH ( VAR_STR ( fmt ) );
			if ( !classCallMethod ( instance, value, "format", 1 ) )
			{
				throw errorNum::scUNKNOWN_METHOD;
			}
			POP ( 1 );
			if ( TYPE ( &instance->result ) != slangType::eSTRING )
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			return VAR_STR ( instance, instance->result.dat.str.c, instance->result.dat.str.len );
		case slangType::eLONG:
		case slangType::eDOUBLE:
			{
				std::stringstream	s;
				currency_punct c;
				if ( width )
				{
					if ( width < 0 )
					{
						s.width ( -width );
						s.setf ( std::ios_base::left );
					} else
					{
						s.width ( width );
						s.setf ( std::ios_base::right );
					}
				}
				s.precision ( precision );
				if ( zeroFill )
				{
					s.fill ( '0' );
				}
				switch ( fmtChar )
				{
					case 'd':
						if ( value->type == slangType::eDOUBLE )
						{
							throw errorNum::scINVALID_PARAMETER;
						}
						break;
					case 'D':
						s.setf ( std::ios_base::uppercase );
						break;
					case 'c':
						{
							std::locale loc;
							s.imbue ( std::locale ( s.getloc (), &c ) );
							s << std::use_facet<std::moneypunct<char, true>> ( loc ).curr_symbol ();
						}
						break;
					case 'C':
						s.imbue ( std::locale ( s.getloc ( ), &c ) );
						s.setf ( std::ios_base::uppercase );
						break;
					case 'e':
						s.setf ( std::ios_base::scientific );
						break;
					case 'E':
						s.setf ( std::ios_base::scientific );
						s.setf ( std::ios_base::uppercase );
						break;
					case 'f':
						s.setf ( std::ios_base::fixed );
						break;
					case 'F':
						s.setf ( std::ios_base::fixed );
						s.setf ( std::ios_base::uppercase );
						break;
					case 'g':
						break;
					case 'G':
						s.setf ( std::ios_base::uppercase );
						break;
					case 'n':
						s.imbue ( std::locale ( s.getloc ( ), &c ) );
						break;
					case 'N':
						s.imbue ( std::locale ( s.getloc ( ), &c ) );
						s.setf ( std::ios_base::uppercase );
						break;
					case 'p':
						multiplier = 100;
						break;
					case 'P':
						s.setf ( std::ios_base::uppercase );
						multiplier = 100;
						break;
					case 'x':
						s.setf ( std::ios_base::hex );
						break;
					case 'X':
						s.setf ( std::ios_base::hex );
						s.setf ( std::ios_base::uppercase );
						break;
				}
				switch ( TYPE ( value ) )
				{
					case slangType::eLONG:
						s << value->dat.l * multiplier;
						break;
					case slangType::eDOUBLE:
						s << value->dat.d * (double)multiplier;
						break;
					default:
						throw errorNum::scINTERNAL;
				}
				switch ( fmtChar )
				{
					case 'p':
					case 'P':
						s << '%';
						break;
				}
				return VAR_STR ( instance, s.str ( ).c_str() );
			}
			break;
		case slangType::eSTRING:
			result = stringi ( value->dat.str.c, value->dat.str.len );
			if ( !width )
			{
				return VAR_STR ( instance, result );
			} else if ( width < 0 )
			{
				// left align
				width = -width;
				if ( width < instance->result.dat.str.len )
				{
					return VAR_STR ( instance, result );
				}
				result.insert ( result.begin ( ), width - (int32_t) result.length ( ), ' ' );
				return VAR_STR ( instance, result );
			} else
			{
				if ( width < instance->result.dat.str.len )
				{
					result.append ( width - (int32_t) result.length ( ), ' ' );
				}
				return VAR_STR ( instance, result );
			}
			return *value;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

static int64_t vmPCount ( vmInstance *instance )
{
	auto x = instance->stack;
	while ( x >= instance->eval )
	{
		if ( TYPE ( x ) == slangType::ePCALL )
		{
			return x->dat.pCall.nParams;
		}
		x--;
	}
	throw errorNum::scINTERNAL;
}

static VAR vmParam ( vmInstance *instance, uint32_t nParam )
{
	if ( !nParam )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	auto x = instance->stack;
	while ( x >= instance->eval )
	{
		if ( x->type == slangType::ePCALL )
		{
			if ( nParam <= x->dat.pCall.nParams )
			{
				return *(x - nParam);
			} else
			{
				throw errorNum::scINVALID_PARAMETER;
			}
		}
		x--;
	}
	throw errorNum::scINTERNAL;
}

static VAR_STR vmClassName ( vmInstance *instance, VAR *obj )
{
	if ( TYPE ( obj ) != slangType::eOBJECT_ROOT ) throw errorNum::scINVALID_PARAMETER;
	VAR_STR ret ( instance, obj->dat.ref.v->dat.obj.classDef->name, obj->dat.ref.v->dat.obj.classDef->nameLen - 1 ); // nameLen includes trailing 0 to allow us to do quick memcmp
	_upper ( ret.dat.str.c, const_cast<char *>(ret.dat.str.c) );	// not violating the non-modify here as ret is still local to this function
	return ret;
}

static VAR vmClassGetElements ( vmInstance *instance, VAR *obj )
{
	enum class iTypes {
		oIVAR = 1,
		oACCESS = 2,
		oDEFAULT = 3,
		oINHERIT = 4,
		oSTATIC = 5,
		oDIRECTED = 6,
		oCONSTANT = 7,
	};

	if ( TYPE ( obj ) != slangType::eOBJECT_ROOT )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	VAR arr = VAR_ARRAY ( instance );

	if ( obj->dat.ref.v->dat.obj.classDef->cInspectorCB )
	{
		auto e = obj->dat.ref.v->dat.obj.classDef->cInspectorCB ( instance, 0, obj, 0, 0 );

		int64_t index = 1;
		for ( auto &it : e->entry )
		{
			arrayGet ( instance, &arr, index, 1 ) = VAR_STR ( instance, it->getName ( ) );
			arrayGet ( instance, &arr, index, 2 ) = VAR ( int(iTypes::oIVAR) );
			index++;
		}
	} else
	{
		int64_t index = 1;
		for ( size_t loop = 0; loop < obj->dat.ref.v->dat.obj.classDef->nElements; loop++ )
		{
			auto e = obj->dat.ref.v->dat.obj.classDef->elements[loop];

			switch ( e.type )
			{
				case fgxClassElementType::fgxClassType_prop:
				case fgxClassElementType::fgxClassType_method:
					arrayGet ( instance, &arr, index, 2 ) = VAR ( int ( iTypes::oACCESS) );
					break;
				case fgxClassElementType::fgxClassType_iVar:
					arrayGet ( instance, &arr, index, 2 ) = VAR ( int ( iTypes::oIVAR) );
					break;
				case fgxClassElementType::fgxClassType_static:
					arrayGet ( instance, &arr, index, 2 ) = VAR ( int ( iTypes::oSTATIC) );
					break;
				case fgxClassElementType::fgxClassType_const:
					arrayGet ( instance, &arr, index, 2 ) = VAR ( int ( iTypes::oCONSTANT) );
					break;
				case fgxClassElementType::fgxClassType_inherit:
					arrayGet ( instance, &arr, index, 2 ) = VAR ( int ( iTypes::oINHERIT) );
					break;
				default:
					continue;
			}
			VAR_STR s ( instance, e.name, e.nameLen );
			_upper ( s.dat.str.c, const_cast<char *>(s.dat.str.c) );

			arrayGet ( instance, &arr, index, 1 ) = s;

			index++;
		}
	}
	return arr;
}

static bool vmIsLibraryLoaded ( vmInstance *instance, char const *fName )
{
	std::filesystem::path p ( fName );

	auto type = instance->atomTable->atomGetType ( p.generic_string ().c_str () );
	if ( type == atom::atomType::aLOADIMAGE )
	{
		// already loaded
		return true;
	}

	p.replace_extension ( ".slb" );
	type = instance->atomTable->atomGetType ( p.generic_string ().c_str () );
	if ( type == atom::atomType::aLOADIMAGE )
	{
		// already loaded
		return true;
	}


	p.replace_extension ( ".sll" );
	type = instance->atomTable->atomGetType ( p.generic_string ().c_str () );
	if ( type == atom::atomType::aLOADIMAGE )
	{
		// already loaded
		return true;
	}
	return false;
}

static bool vmLoadLibrary ( vmInstance *instance, char const *fName )
{
	std::filesystem::path p ( fName );

	auto type = instance->atomTable->atomGetType ( p.generic_string ().c_str() );
	if ( type == atom::atomType::aLOADIMAGE )
	{
		// already loaded
		return true;
	}

	p.replace_extension ( ".slb" );
	type = instance->atomTable->atomGetType ( p.generic_string ().c_str() );
	if ( type == atom::atomType::aLOADIMAGE )
	{
		// already loaded
		return true;
	}
	
	p.replace_extension ( ".sll" );
	type = instance->atomTable->atomGetType ( p.generic_string ().c_str () );
	if ( type == atom::atomType::aLOADIMAGE )
	{
		// already loaded
		return true;
	}
	
	// does the dynamic lib exist
	auto entry = globalFileCache.read ( p.generic_string ().c_str () );
	if ( !entry )
	{
		return false;
	}

	// read it and load it
	uint8_t *data = (uint8_t *) instance->malloc ( entry->getDataLen ( ) );
	memcpy ( data, entry->getData ( ), entry->getDataLen ( ) );
	entry->free ( );

	try
	{
		std::shared_ptr<uint8_t> dup ( (uint8_t *) data, [=]( auto p ) { instance->free ( p ); } );
		instance->load ( dup, p.generic_string ().c_str (), true, true );
	} catch ( ... )
	{
		free ( data );
	}

	// ideally we should change data into a shared_ptr, but this requires a bit more work as the allocation is based on a private heap from instance
	// right now clang-tidy believes this is a memory leak and reports it as such... NOLINT is ont the next line to stop this, however its really 'data' that
	// it believes is leaking
	auto loadImage = instance->atomTable->atomGetLoadImage ( p.generic_string ().c_str () ); // NOLINT

	// initialize all the local statics
	auto globals = instance->om->allocVar ( loadImage->nGlobals );
	for ( uint32_t loop = 0; loop < loadImage->nGlobals; loop++ )
	{
		loadImage->globals[loop] = &(globals[loop]);
		loadImage->globals[loop]->type = slangType::eNULL;
		loadImage->globalDefs[loop].globalIndex = loop;
		loadImage->globalDefs[loop].image = loadImage;
	}

	// we need to overlay our globals onto any existing ones.. so for each of our globals, loop through and look for ones with identical names.. if they're identical than 
	//		we point to the existing global, otherwise we keep it as a null
	auto nExportable = loadImage->globalsExportRoot;
	while ( nExportable )
	{
		auto global = &loadImage->globalSymbolTable[nExportable - 1];
		assert ( global->isExportable );
		auto gbl = instance->findGlobal ( global->name, loadImage );
		if ( std::get<0>(gbl) )
		{
			// someone else already has this defined... use their definition
			loadImage->globals[global->index] = std::get<0>(gbl);
			loadImage->globalDefs[nExportable - 1].image = std::get<1>(gbl);
			loadImage->globalDefs[nExportable - 1].globalIndex = std::get<2> ( gbl );
		}
		nExportable = global->nextExportable;
	}

	if ( loadImage->globalInit )
	{
		bcFuncDef	 fDef;

		fDef.cs = loadImage->globalInit;
		fDef.loadImage = loadImage;
		fDef.name = "Global Initializers";

		auto stack = instance->stack;
		stack->type = slangType::ePCALL;
		stack->dat.pCall.op = 0;
		stack->dat.pCall.func = &fDef;
		stack->dat.pCall.nParams = 0;
		stack++;

		instance->stack = stack;
		// unlike non-sll code, all code built for sll global intializers has a check for NULL... only non-null globals will be evaluated and stored
		instance->interpretBCParam ( &fDef, &fDef, loadImage->globalInit, 0, stack, stack );
		instance->stack = stack - 1;
	}
	return true;
}

static VAR_STR vmCompile ( vmInstance *instance, VAR_STR *src, bool isSlang, bool isAP )
{
	opFile			 file;
	vmTaskInstance	 newInstance ( "compile" );

	// compile the built-in code
	auto [builtIn, builtInSize] = builtinInit ( &newInstance, builtinFlags::builtIn_MakeCompiler );			// this is the INTERFACE

	try
	{
		opFile	oFile;

		oFile.parseFile ( "(internal)", src->dat.str.c, isSlang, false, isAP );
		if ( oFile.errHandler.isFatal () )
		{
			return VAR_STR ( "" );
		}

		char const *obj = (char const *)builtIn;
		oFile.addStream ( &obj, false, false );

		oFile.loadLibraries ( false, false );

		compExecutable comp ( &oFile );

		comp.genCode ( nullptr );

		BUFFER buff;

		comp.serialize ( &buff, false );
		
		free ( const_cast<uint8_t *>(builtIn) );

		return VAR_STR ( instance, buff );
	} catch ( ... )
	{
		free ( const_cast<uint8_t *>( builtIn ) );
	}
	return VAR_STR ( "" );
}

static VAR_STR vmApTranslate ( vmInstance *instance, VAR_STR *src, bool encapsulate, bool isSlang )
{
	std::unique_ptr<char> apCode ( compAPPreprocessBuffer ( "(apTranslate)", src->dat.str.c, isSlang, encapsulate ) );
	std::unique_ptr<char> ppCode ( compPreprocessor ( "(apTranslate)", apCode.get(), isSlang ) );

	return VAR_STR ( instance, ppCode.get () );
}

static size_t vmLen ( class vmInstance *instance, VAR *var )
{
	switch ( TYPE ( var ) )
	{
		case slangType::eLONG:
			return (sizeof ( long ));
		case slangType::eDOUBLE:
			return (sizeof ( double ));
		case slangType::eSTRING:
			return (var->dat.str.len);
		case slangType::eARRAY_ROOT:
			switch ( TYPE ( var->dat.ref.v ) )
			{
				case slangType::eARRAY_FIXED:
					return (var->dat.ref.v->dat.arrayFixed.endIndex - var->dat.ref.v->dat.arrayFixed.startIndex + 1);
					break;
				case slangType::eARRAY_SPARSE:
					return (var->dat.ref.v->dat.aSparse.maxI);
				default:
					throw errorNum::scINTERNAL;
			}
			throw errorNum::scINTERNAL;
		case slangType::eOBJECT_ROOT:
			if ( !fglstrccmp ( var->dat.ref.v->dat.obj.classDef->name, "SERVERBUFFER" ) )
			{
				if ( !(var = classIVarAccess ( var, "SIZE" )) )
				{
					throw errorNum::scINTERNAL;
				}
				return (var->dat.l);
			} else 
			{
				if ( classCallMethod ( instance, var, "size", 0 ) )
				{
					if ( instance->result.type != slangType::eLONG ) throw errorNum::scINVALID_PARAMETER;
					return instance->result.dat.l;
				} else
				{
					if ( var->dat.ref.v->dat.obj.classDef->cInspectorCB )
					{
						auto e = var->dat.ref.v->dat.obj.classDef->cInspectorCB ( instance, 0, var, 0, 0 );
						auto size = e->entry.size ();
						delete e;
						return size;
					} else
					{
						return var->dat.ref.v->dat.obj.classDef->nElements;
					}
				}
			}
			throw errorNum::scINVALID_PARAMETER;
		case slangType::eNULL:
			return (0);
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

static VAR *varCopy ( vmInstance *instance, VAR *dst, VAR *src, std::vector<std::pair<VAR *, VAR *>> *xRef );

static VAR *varCopyCB ( vmInstance *instance, VAR *val, bool copy, objectMemory::copyVariables *xRef )
{
	if ( copy )
	{
		return varCopy ( instance, 0, val, reinterpret_cast<std::vector<std::pair<VAR *, VAR *>> *>(xRef) );
	} else
	{
		*val = *varCopy ( instance, 0, val, reinterpret_cast<std::vector<std::pair<VAR *, VAR *>> *>(xRef) );
		return val;
	}
}

static VAR *varCopy ( vmInstance *instance, VAR *dst, VAR *src, std::vector<std::pair<VAR *, VAR *>> *xRef )
{
	if ( (src->packIndex < xRef->size()) && ((*xRef)[src->packIndex].first == src) )
	{
		return (*xRef)[src->packIndex].second;
	}

	if ( !src ) return nullptr;

	if ( !dst )
	{
		switch ( src->type )
		{
			case slangType::eOBJECT:
				// handle objects' that point to the middle of an object
				if ( src != src[1].dat.ref.v )
				{
					size_t offset = src - static_cast<VAR *>(src[1].dat.ref.v);
					auto newsrc = varCopy ( instance, 0, src[1].dat.ref.v, xRef );
					return  &newsrc[offset];
				}
				// otherwise don't allocate anything, we'll allocate it later
				dst = instance->om->allocVar ( src->dat.obj.classDef->numVars );
				break;
			case slangType::eARRAY_FIXED:
				{
					auto numElems = (size_t)(src->dat.arrayFixed.endIndex - src->dat.arrayFixed.startIndex + 1) + 1;
					dst = instance->om->allocVar ( numElems );	// allocate array_fixed + elements
					memset ( dst, 0, sizeof ( VAR ) * numElems );
					*dst = *src;		// copy over array information... but leave elements null, we'll copy them individually
				}
				break;
			case slangType::eCODEBLOCK_ROOT:
				{
					dst = instance->om->allocVar ( (size_t)src->dat.cb.cb->nSymbols - src->dat.cb.cb->nParams + 1 );	// allocate array_fixed + elements
					*dst = *src;		// copy over array information... but leave elements null, we'll copy them individually
				}
				break;
			default:
				dst = instance->om->allocVar ( 1 );
				*dst = *src;
				break;
		}
	} else
	{
		*dst = *src;
	}

	src->packIndex = (uint32_t)xRef->size ();
	xRef->push_back ( { src, dst } );

	switch ( src->type )
	{
		case slangType::eSTRING:
			*dst = VAR_STR ( instance, src->dat.str.c, src->dat.str.len );
			break;
		case slangType::eREFERENCE:		// TODO: add a vector to allow us to keep track of circlular refernces
		case slangType::eARRAY_ROOT:
		case slangType::eOBJECT_ROOT:
		case slangType::eCODEBLOCK_ROOT:
			dst->dat.ref.v = varCopy ( instance, 0, src->dat.ref.v, xRef );
			dst->dat.ref.obj = static_cast<VAR *>(dst->dat.ref.v) - (static_cast<VAR *>(src->dat.ref.obj) - static_cast<VAR *>(src->dat.ref.v));
			break;
		case slangType::eARRAY_ELEM:
			dst->dat.aElem.var = varCopy ( instance, 0, src->dat.aElem.var, xRef );
			dst->dat.aElem.next = 0;
			break;
		case slangType::eARRAY_SPARSE:
			dst->dat.aSparse.v = 0;
			dst->dat.aSparse.lastAccess = 0;
			if ( src->dat.aSparse.v )
			{
				VAR		*newElem;
				VAR		*prevNewElem = 0;

				for ( auto elem = src->dat.aSparse.v; elem; elem = elem->dat.aElem.next )
				{
					newElem = varCopy ( instance, 0, elem, xRef );
					if ( prevNewElem )
					{
						prevNewElem->dat.aElem.next = newElem;
					} else
					{
						dst->dat.aSparse.v = newElem;
						dst->dat.aSparse.lastAccess = newElem;
					}
					dst->dat.aSparse.maxI = newElem->dat.aElem.elemNum;
					prevNewElem = newElem;
				}
			}
			break;
		case slangType::eARRAY_FIXED:
			for ( size_t loop = 0; loop < (size_t)(src->dat.arrayFixed.endIndex - src->dat.arrayFixed.startIndex + 1); loop++ )
			{
				// now copy over the elements invidually
				 varCopy ( instance, &dst[loop + 1], &src[loop + 1], xRef );
			}
			break;
		case slangType::eCODEBLOCK:
			if ( !dst->dat.cb.cb->isStatic )
			{
				dst->dat.cb.cb = (vmCodeBlock *) instance->om->alloc ( src->dat.cb.cb->storageSize );
			}
			for ( uint32_t loop = 0; loop < dst->dat.cb.cb->nSymbols - dst->dat.cb.cb->nParams; loop++ )
			{
				varCopy ( instance, &dst[loop + 1], &src[loop + 1], xRef );
			}
			break;
		case slangType::eOBJECT:
			{
				bcClass	*classDef = src->dat.obj.classDef;

				// allocate our vars
				memset ( dst, 0, sizeof ( VAR ) * classDef->numVars );

				// initialize the object
				dst->type = slangType::eOBJECT;
				dst->dat.obj.classDef	= classDef;
				dst->dat.obj.vPtr		= classDef->vTable;
				dst->dat.obj.cargo		= src->dat.obj.cargo;	// it's the job of the copy callback to make it's own copy of this... we just pass the existing one to it untouched
				dst[1].type				= slangType::eOBJECT_ROOT;
				dst[1].dat.ref.v			= dst;
				dst[1].dat.ref.obj			= dst;

				// build out any inherited objects
				for ( size_t loop = 0; loop < classDef->nInherit; loop++ )
				{
					dst[classDef->inherit[loop].objOffset].type = slangType::eOBJECT;
					dst[classDef->inherit[loop].objOffset].dat.obj.cargo = 0;
					dst[classDef->inherit[loop].objOffset].dat.obj.classDef = src[classDef->inherit[loop].objOffset].dat.obj.classDef;
					dst[classDef->inherit[loop].objOffset].dat.obj.vPtr = src[classDef->inherit[loop].objOffset].dat.obj.vPtr;
					dst[classDef->inherit[loop].objOffset + 1].type = slangType::eOBJECT_ROOT;
					dst[classDef->inherit[loop].objOffset + 1].dat.ref.v	= dst;
					dst[classDef->inherit[loop].objOffset + 1].dat.ref.obj  = dst;
				}

				// now, any ivars and handle any inherited objects that need gc callbacks or have cargo that need to be dealt with
				for ( uint32_t loop = 2; loop < classDef->numVars; loop++ ) // start with 1 to skip our own object
				{
					if ( dst[loop].type == slangType::eNULL )
					{
						dst[loop] = *varCopy ( instance, 0, &src[loop], xRef );
					}
				}

				// call any copy callbacks
				for ( uint32_t loop = classDef->numVars; loop; loop-- ) // start with 1 to skip our own object
				{
					if ( dst[loop - 1].type == slangType::eOBJECT && dst[loop - 1].dat.obj.classDef->cCopyCB )
					{
						VAR obj{};
						obj.type = slangType::eOBJECT_ROOT;
						obj.dat.ref.v = &dst[loop - 1];
						obj.dat.ref.obj = dst;

						dst[loop - 1].dat.obj.classDef->cCopyCB ( instance, &obj, varCopyCB, reinterpret_cast<objectMemory::copyVariables *>(xRef) );
					}
				}
			}
			break;
		default:
			break;
	}
	return dst;
}

static VAR vmVarCopy ( vmInstance *instance, VAR *src, nParamType nParams )
{
	VAR	*dst;
	VAR  tmp{};
	std::vector<std::pair<VAR *, VAR *>> xRef;

	if ( nParams > 0 )
	{
		dst = nParams[0];
	} else
	{
		dst = &tmp;
	}
	varCopy ( instance, dst, src, &xRef );
	return *dst;
}

static VAR vmEval ( vmInstance *instance, VAR_STR *str )
{
	bcFuncDef					 cbFunc;
	bcLoadImage					 cbLoadImage;
	static fgxTryCatchEntry		 nullTcList[] = { {0xFFFFFFFF, 0xFFFFFFFF, 0}  };

	cbFunc.conv = fgxFuncCallConvention::opFuncType_Bytecode;
	cbFunc.retType = bcFuncPType::bcFuncPType_Variant;
	cbFunc.tcList = nullTcList;
	cbFunc.loadImage = &cbLoadImage;

	cbLoadImage.csBase = 0;
	cbLoadImage.dsBase = 0;

	cbLoadImage.instance = instance;
	strcpy_s ( cbLoadImage.name, sizeof ( cbLoadImage.name), "<eval>" );
	cbLoadImage.fileImage = 0;
	cbLoadImage.globals = 0;
	cbLoadImage.nDebugEntries = 0;
	cbLoadImage.nGlobals = 0;
	cbLoadImage.globalsExportRoot = 0;

	*(instance->stack++) = *str;
	op_compile ( instance, &cbFunc, true );
	op_cbFixup ( instance, &cbFunc , instance->stack );
	op_cbCall ( instance, 0, &cbFunc, instance->stack, 0 );

	return instance->result;
}

void builtinGeneralInit ( vmInstance *instance, opFile *file )
{
	REGISTER ( instance, file );
		FUNC ( "type", vmType ) CONST PURE;
		FUNC ( "typex", vmTypeX ) CONST PURE;
		FUNC ( "typeof", vmTypeOf ) CONST PURE;
		FUNC ( "except", vmExcept );
		FUNC ( "omCollect", omCollect );
		FUNC ( "system$release", vmRelease );
		FUNC ( "$error", throwInternal_ );								// really fake.. necessary just for linking, prototyping and make atom
		FUNC ( "new", throwInternal_ );								// really fake.. necessary just for linking, prototyping and make atom
		FUNC ( "debugBreak", throwInternal_ );							// really fake.. this will be replaced by a break instruction, but the symbol must exist to be resolved - do NOT mark CONST as it will generat a warning for a non-assignment use which is silly 
		FUNC ( "getEnumerator", throwInternal_ );						// really fake.. this will be replaced by a break instruction, but the symbol must exist to be resolved
		FUNC ( "engineIsDebug", vmEngineIsDebug ) CONST PURE;
		FUNC ( "memory", omMemory );
		FUNC ( "len", vmLen ) CONST PURE;

		FUNC ( "className", vmClassName ) CONST PURE;
		FUNC ( "classElements", vmClassGetElements ) CONST PURE;

		FUNC ( "print", vmPrint );
		FUNC ( "printLn", vmPrintln );
		FUNC ( "printF", vmPrintf );
		FUNC ( "sPrintF", vmSprintf );

		FUNC ( "pcount", vmPCount ) CONST PURE;
		FUNC ( "param", vmParam ) CONST PURE;

		FUNC ( "waSelect", waSelect );
		FUNC ( "waUsed", waUsed );

		FUNC ( "apTranslate", vmApTranslate, DEF ( 3, "false" ) ) CONST PURE;
		FUNC ( "compile", vmCompile, DEF ( 2, "false" ), DEF ( 3, "false" ), DEF ( 3, "false" ) ) CONST PURE;
		FUNC ( "isLibraryLoaded", vmIsLibraryLoaded ) CONST;
		FUNC ( "loadLibrary", vmLoadLibrary );

		FUNC ( "varCopy", vmVarCopy ) CONST PURE;

		FUNC ( "eval", vmEval );

		FUNC ( "$strInterpolate", strInterpolate, DEF ( 2, "-1" ), DEF ( 3, "\"\"" ) );

		CLASS ( "systemError" );
			IVAR ( "errNo" );
			IVAR ( "line" );
			IVAR ( "fileName" );
			IVAR ( "functionName" );
			IVAR ( "description" );
			IVAR ( "callStack" );
			CODE ( R"( 
						method new ( errNo ) : errNo ( errNo )
						{
						}
					)" )
		END;

		CLASS ( "fixedArrayEnumerator" );
			INHERIT ( "Queryable" );
			METHOD ( "getEnumerator", fixedArrayEnumeratorGetEnumerator );
			METHOD ( "moveNext", fixedArrayEnumeratorMoveNext );
			ACCESS ( "current", fixedArrayEnumeratorCurrent ) CONST;
			ACCESS ( "index", fixedArrayEnumeratorIndex ) CONST;

			PRIVATE
				METHOD ( "new", fixedArrayEnumeratorNew );

				IVAR ( "$array" );
				IVAR ( "$index" );
		END;

		CLASS ( "sparseArrayEnumerator" );
			INHERIT ( "Queryable" );
			METHOD ( "getEnumerator", sparseArrayEnumeratorGetEnumerator );
			METHOD ( "moveNext", sparseArrayEnumeratorMoveNext );
			ACCESS ( "current", sparseArrayEnumeratorCurrent ) CONST;
			ACCESS ( "index", sparseArrayEnumeratorIndex ) CONST;

			PRIVATE
				METHOD ( "new", sparseArrayEnumeratorNew );
				IVAR ( "$elem" );
		END;

		CLASS ( "stringEnumerator" );
			INHERIT ( "Queryable" );
			METHOD ( "getEnumerator", stringEnumeratorGetEnumerator );
			METHOD ( "moveNext", stringEnumeratorMoveNext );
			ACCESS ( "current", stringEnumeratorCurrent ) CONST;
			ACCESS ( "index", stringEnumeratorIndex ) CONST;

			PRIVATE
				METHOD ( "new", stringEnumeratorNew );
				IVAR ( "$string" );
				IVAR ( "$offset" );
				IVAR ( "$current" );
		END;

		CODE ( R"sl( 
						iterator range ( start, stop )
						{
							local value;
							for ( value = start; value <= stop; value++ )
							{
								yield value;
							}
						}
				)sl" );
	END;
}
