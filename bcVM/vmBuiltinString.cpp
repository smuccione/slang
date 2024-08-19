/*
SLANG support functions

*/


#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "vmAllocator.h"

static VAR_STR vmLtrim ( class vmInstance *instance, VAR_STR *val)
{
	auto src = val->dat.str.c;
	while ( (*src == ' ') || (*src == '\t') )
	{
		src++;
	}

	return VAR_STR ( src );
}

static VAR_STR vmRtrim ( class vmInstance *instance, VAR_STR *val )
{
	if ( val->dat.str.len )
	{
		auto src = val->dat.str.c + val->dat.str.len - 1;
		while ( ((*src == ' ') || (*src == '\t')) && (src >= val->dat.str.c) )
		{
			src--;
		}

		if ( (*src == ' ') || (*src == '\t') )
		{
			return VAR_STR ( "" );
		}
		return VAR_STR ( instance, val->dat.str.c, src - val->dat.str.c + 1 );
	} else
	{
		return VAR_STR ( "" );
	}
}

static VAR_STR vmAlltrim ( class vmInstance *instance, VAR_STR *val )
{
	auto start = val->dat.str.c;
	while ( (*start == ' ') || (*start == '\t') )
	{
		start++;
	}

	auto stop = val->dat.str.c + val->dat.str.len - 1;
	while ( (*stop == ' ') || (*stop == '\t') )
	{
		stop--;
	}

	if ( stop < start )
	{
		return VAR_STR ( "" );
	} else
	{
		return VAR_STR ( instance, start, stop - start + 1 );
	}
}

static VAR_STR vmUntrim ( class vmInstance *instance, VAR_STR *val, int len )
{
	char const	*src;
	BUFFER		 buff;
	int			 loop;

	src = val->dat.str.c;

	for ( loop = 0; (loop < len) && *src; loop++ )
	{
		buff.put ( *(src++) );
	}

	for (; loop < len; loop++ )
	{
		buff.put ( ' ' );
	}

	return VAR_STR ( instance, buff );
}

static VAR_STR vmLjust ( class vmInstance *instance, VAR_STR *val )
{
	char const	*src;
	BUFFER		 buff;
	int		     cnt;

	src = val->dat.str.c;

	cnt = 0;

	while ( *src == ' ' )
	{
		src++;
		cnt++;
	}

	while ( *src )
	{
		buff.put ( *(src++) );
	}

	while ( cnt-- )
	{
		buff.put ( ' ' );
	}

	return VAR_STR ( instance, buff );
}

static VAR_STR vmRjust ( class vmInstance *instance, VAR_STR *val )
{
	char const	*src;
	char	*dst;
	char	*ret;
	int		 cnt;

	if ( !val->dat.str.len )
	{
		return ( "" );
	}

	ret = (char *)instance->om->alloc ( val->dat.str.len + 1 );

	src = val->dat.str.c + val->dat.str.len - 1;
	dst = ret + val->dat.str.len;
	*(dst--) = 0;

	cnt = 0;

	while ( (src >= val->dat.str.c) && (*src == ' ') )
	{
		src--;
		cnt++;
	}

	while ( (src >= val->dat.str.c) )
	{
		*(dst--) = *(src--);
	}

	while ( cnt-- )
	{
		*(dst--) = ' ';
	}

	return ret;
}

static VAR_STR vmMid( class vmInstance *instance, VAR_STR *var, size_t i, size_t l )
{
	VAR_STR	ret;

	i = (i > var->dat.str.len ? var->dat.str.len : i);
	i--;		// make it 0 based
	l = (l > var->dat.str.len - i ? var->dat.str.len - i : l);

	return VAR_STR ( instance, var->dat.str.c + i, l );
}

static VAR_STR vmLeft ( class vmInstance *instance, VAR_STR *var, size_t i )
{
	VAR_STR	ret;

	i = (i > var->dat.str.len ? var->dat.str.len : i );

	return VAR_STR ( instance, var->dat.str.c, i );
}

static VAR_STR vmRight ( class vmInstance *instance, VAR_STR *var, size_t i )
{
	VAR_STR	ret;

	i = (i > var->dat.str.len ? var->dat.str.len : i);

	return VAR_STR ( instance, var->dat.str.c + var->dat.str.len - i, i );
}

static VAR_STR vmSpace ( class vmInstance *instance, size_t i )
{
	if ( i < 0 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	char *ret  = (char *)instance->om->alloc ( sizeof ( char ) * (i + 1) );
	memset ( ret, ' ', i );
	ret[i] = 0;

	return VAR_STR ( ret, i );
}

static VAR_STR vmLower ( class vmInstance *instance, VAR_STR *val )
{
	char const	*src;
	char		*dst;
	size_t		 loop;
	char *		 ret;

	ret = (char *)instance->om->alloc ( val->dat.str.len + 1 );

	src = val->dat.str.c;
	dst = ret;

	for ( loop = 0; loop < val->dat.str.len; loop++ )
	{
		if ( (*src >= 'A') && (*src <= 'Z') )
		{
			*(dst++) = (char)(*(src++) - 'A' + 'a');
		} else
		{
			*(dst++) = *(src++);
		}
	}
	*dst = 0;

	return VAR_STR ( ret, val->dat.str.len );
}

static VAR_STR vmUpper ( class vmInstance *instance, VAR_STR *val )
{
	char const	*src;
	char		*dst;
	size_t		 loop;
	char *ret;

	ret = (char *)instance->om->alloc( val->dat.str.len + 1 );

	src = val->dat.str.c;
	dst = ret;

	for ( loop = 0; loop < val->dat.str.len; loop++ )
	{
		if ( (*src >= 'a') && (*src <= 'z') )
		{
			*(dst++) = (char)(*(src++) - 'a' + 'A');
		} else
		{
			*(dst++) = *(src++);
		}
	}
	*dst = 0;
	return VAR_STR ( ret, val->dat.str.len );
}

static VAR_STR vmStrCapFirst ( class vmInstance *instance, VAR_STR *val )
{
	auto ret =  (char *)instance->om->alloc ( val->dat.str.len + 1 );

	_strcapfirst ( val->dat.str.c, ret );

	return VAR_STR ( ret, val->dat.str.len );
}

static VAR_STR vmTrimPunct ( class vmInstance *instance, VAR_STR *val )
{
	auto ret = (char *) instance->om->alloc ( val->dat.str.len + 1 );
	trimpunct( val->dat.str.c, ret );

	return VAR_STR ( ret, val->dat.str.len );
}

static VAR_STR vmChrSwap ( class vmInstance *instance, VAR_STR *pStr, VAR_STR *pOldChar, VAR_STR *pNewChar)
{
	auto ret = (char *)instance->om->alloc( pStr->dat.str.len + 1 );
	auto str = pStr->dat.str.c;
	auto len = pStr->dat.str.len;
	char oldChr = *pOldChar->dat.str.c;
	char newChr = *pNewChar->dat.str.c;
	auto dest = ret;

	while ( len )
	{
		if ( *str == oldChr )
		{
			*(dest++) = newChr;
			str++;
		} else
		{
			*(dest++) = *(str++);
		}
		len--;
	}
	*dest = 0;

	return VAR_STR ( ret, pStr->dat.str.len );
}

static VAR_STR vmString( class vmInstance *instance, size_t len, VAR *var )
{
	unsigned char  chr;

	switch ( TYPE ( var ) )
	{
		case slangType::eLONG:
			chr = (unsigned char)var->dat.l;
			break;
		case slangType::eDOUBLE:
			chr = (unsigned char)var->dat.d;
			break;
		case slangType::eSTRING:
			if ( !var->dat.str.len ) throw errorNum::scINVALID_PARAMETER;
			chr = (unsigned char)*(var->dat.str.c);
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	if ( len < 0 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	auto ret = (char *)instance->om->alloc( len + 1 );
	memset ( ret, chr, len );
	ret[len] = 0;

	return VAR_STR ( ret, len );
}

static VAR_STR vmReplicate ( class vmInstance *instance, VAR_STR str, int64_t count )
{
	auto ret = (char *)instance->om->alloc ( str.dat.str.len * count + 1 );

	auto dst = ret;
	while ( count-- )
	{
		memcpy ( dst, str.dat.str.c, str.dat.str.len );
		dst++;
	}
	dst[0] = 0;

	return VAR_STR ( ret, str.dat.str.len * count );
}

static int isalpha_ ( VAR *val )
{
   switch ( TYPE( val ) )
   {
      case slangType::eSTRING:
         return ( _isalpha ( val->dat.str.c, (int)val->dat.str.len ) );
         break;

      default:
         throw errorNum::scINVALID_PARAMETER;
   }
}

static int isdigit_ ( VAR *val )
{
	unsigned      loop;

	switch ( TYPE( val ) )
	{
		case slangType::eSTRING:
			for ( loop = 0; loop < val->dat.str.len; loop++ )
			{
				if ( !_isdigit ( val->dat.str.c + loop ) )
				{
					return ( 0 );
				}
			}
			return ( 1 );
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			return ( 1 );

		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

static int isnum_ ( VAR *val )
{
	unsigned      loop;

	switch ( TYPE( val ) )
	{
		case slangType::eSTRING:
			for ( loop = 0; loop < val->dat.str.len; loop++ )
			{
				if ( !_isnum ( val->dat.str.c + loop ) )
				{
					return ( 0 );
				}
			}
			return ( 1 );
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			return ( 1 );
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

static int islower_ ( VAR *val )
{
   switch ( TYPE( val ) )
   {
      case slangType::eSTRING:
         return ( _islower ( val->dat.str.c, (int)val->dat.str.len ) );
         break;

      default:
         throw errorNum::scINVALID_PARAMETER;
   }
}

static int isupper_ ( VAR *val )
{
   switch ( TYPE( val ) )
   {
      case slangType::eSTRING:
         return ( _isupper ( val->dat.str.c, (int) val->dat.str.len ) );
         break;

      default:
         throw errorNum::scINVALID_PARAMETER;
   }
}

static int strempty ( VAR *val )
{
	char const	*tmpC;
	
	switch ( TYPE( val ) )
	{
		case slangType::eSTRING:
			tmpC = val->dat.str.c;

			if ( tmpC )
			{
				for ( tmpC = val->dat.str.c; *tmpC; tmpC++ )
				{
					if ( *tmpC != ' ' )
					{
						return ( 0 );
					}
				}
			} else
			{
				return ( 0 );
			}
			return ( 1 );
		case slangType::eNULL:
			return ( 1 );
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

static VAR_STR strcommas ( class vmInstance *instance, VAR *val )
{
	char		*res;
	char		*res2;
	char		 tmpStr[256] = "";
	char		*tmp;
	char		*tmp2;
	char		*decPt;
	ptrdiff_t	 size;
	int			 ctr;
	int			 sgn;
	
	tmp = tmpStr;

	switch ( TYPE( val ) )
	{	
		case slangType::eLONG:
			_snprintf_s ( tmp, sizeof ( tmp ), _TRUNCATE, "%zi", val->dat.l );
			break;
		case slangType::eDOUBLE:
			_snprintf_s ( tmp, sizeof ( tmp ), _TRUNCATE, "%f", val->dat.d );
			break;
		case slangType::eSTRING:
			if ( val->dat.str.len > 200 )
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			if ( !val->dat.str.len )
			{
				return "";
			}
			if ( val->dat.str.c[0] == '.' )
			{
				tmp[0] = '0';
				memcpy ( tmp + 1, val->dat.str.c, val->dat.str.len );
				tmp[val->dat.str.len + 1] = 0;
			} else
			{
				memcpy ( tmp, val->dat.str.c, val->dat.str.len );
				tmp[val->dat.str.len] = 0;
			}
			break;
			
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	sgn = 0;
	if ( *tmp == '-' )
	{
		sgn = 1;
		tmp++;
	}

	/* find '.' */
	for ( decPt = tmp + strlen ( tmp ) - 1; (decPt > tmp) && (*decPt != '.'); decPt-- );

	if ( *decPt == '.' )
	{
		/* find number of spaces until decimal pt */
		size = decPt - tmp;
		tmp2 = decPt - 1;
	} else
	{
		size = (ptrdiff_t)strlen ( tmp );
		tmp2 = tmp + strlen ( tmp ) - 1;
	}

	res = (char *)instance->om->alloc ( sizeof ( char ) * ((size + (size / 3) - (size % 3 == 0) + strlen ( decPt ) + sgn + 1)) );

	res2 = res + size + (size / 3) - (size % 3 == 0) - 1 + sgn;

	/* tack a null on the end before we lose the spot */
	res2[1] = 0;

	ctr = 0;
	while ( tmp2 >= tmp )
	{
		if ( ++ctr > 3 )
		{
			ctr = 1;
			*(res2--) = ',';
		}
		*(res2--) = *(tmp2--);
	}

	if ( sgn )
	{
		*(res2--) = '-';
	}

	if ( *decPt == '.' )
	{
		/* copy off decimal portion */
		strcat_s ( res, sizeof ( tmpStr ) , decPt );
	}

	return VAR_STR ( res, strlen ( res ) );
}


static size_t ati ( VAR_STR substr, VAR_STR str, uint32_t offset )
{
	ptrdiff_t searchLen;
	ptrdiff_t matchPos;

	if ( !str.dat.str.len || !substr.dat.str.len || substr.dat.str.len > str.dat.str.len ) return 0;

	if ( offset > str.dat.str.len )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	
	searchLen = (ptrdiff_t)str.dat.str.len - (ptrdiff_t)substr.dat.str.len + 1;

	for ( matchPos = offset; matchPos < searchLen; matchPos++ )
	{
		if ( !_memicmp ( str.dat.str.c + matchPos, substr.dat.str.c, substr.dat.str.len ) )
		{
			return (size_t)( matchPos + 1 );
		}
	}
	return 0;
}

static size_t at ( VAR_STR substr, VAR_STR str, size_t offset )
{
	size_t	searchLen;
	size_t	 matchPos;

	if ( !str.dat.str.len || !substr.dat.str.len || substr.dat.str.len > str.dat.str.len ) return 0;

	if ( offset > str.dat.str.len )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	searchLen = str.dat.str.len - substr.dat.str.len + 1;

	for ( matchPos = offset; matchPos < searchLen; matchPos++ )
	{
		if ( !strncmp ( str + matchPos, substr.dat.str.c, substr.dat.str.len ) )
		{
			return matchPos + 1;
		}
	}
	return 0;
}

static size_t stratnext ( char const *subStr, char const *str, size_t occur, size_t startpos )
{
	size_t len1;
	size_t addr;

	len1 = strlen ( subStr );

	if ( startpos > strlen ( str ) )
	{
		return ( 0 );
	}

	if ( startpos < 1 )
	{
		return 0;
	}

	addr = startpos;
	str += startpos;
	while ( *str )
	{
		if ( !strncmp ( str, subStr, len1 ) )
		{
			if ( !--occur )
			{
				return ( addr );
			}
		}
		str++;
		addr++;
	}
	return 0;
}

static size_t stratnexti ( char const *subStr, char const *str, size_t occur, size_t startpos )
{
	size_t len1;
	size_t addr;

	len1 = strlen ( subStr );

	if ( startpos > strlen ( str ) )
	{
		return ( 0 );
	}

	if ( startpos < 1 )
	{
		return ( 0 );
	}

	addr = startpos;
	str += startpos;
	while ( *str )
	{
		if ( !_memicmp ( str, subStr, len1 ) )
		{
			if ( !--occur )
			{
				return ( addr );
			}
		}
		str++;
		addr++;
	}
	return ( 0 );
}

static size_t stratprev ( char const *subStr, char const *str, int occur, int startpos )
{
	size_t	len;

	len = strlen ( subStr );

	if ( !len )
	{
		return ( 0 );
	}

	while ( --startpos > 0 )
	{
		if ( !strncmp ( str + startpos, subStr, len) )
		{
			if ( !--occur )
			{
				return (static_cast<size_t>(startpos) + 1 );
			}
		}
	}
	return ( 0 );
}

static size_t stratprevi ( char const *subStr, char const *str, int occur, int startpos )
{
	size_t	len;

	len = strlen ( subStr );

	if ( !len )
	{
		return ( 0 );
	}

	while ( --startpos > 0 )
	{
		if ( !_memicmp ( str + startpos, subStr, len) )
		{
			if ( !--occur )
			{
				return (static_cast<size_t>(startpos) + 1);
			}
		}
	}
	return ( 0 );
}

static int atdiff ( char const *str1, char const *const str2 )
{
	int loop;

	for ( loop = 0; str1[loop] || str2[loop]; loop++ )
	{
		if ( str1[loop] != str2[loop] )
		{
			return ( loop + 1 );
		}
	}
	return ( 0 );
}

static int atdiffi ( char const *str1, char const *str2 )
{
	int loop;

	for ( loop = 0; str1[loop] || str2[loop]; loop++ )
	{
		if ( caseInsenstiveTable[static_cast<unsigned char>(str1[loop])] !=  caseInsenstiveTable[static_cast<unsigned char>(str2[loop])] )
		{
			return ( loop + 1 );
		}
	}
	return ( 0 );
}

static int atlast ( char const *substr, char const *str )
{
	int subStrLen;
	int	strLen;
	int searchLen;
	int	matchPos;
	int	foundPos;

	if ( !*str )
	{
		return ( -1 );
	}
	if ( !*substr )
	{
		return ( -1 );
	}
	
	foundPos = -1;
	
	subStrLen = (int)strlen ( substr );
	strLen = (int)strlen ( str );

	searchLen = strLen - subStrLen + 1;

	for ( matchPos = 0; matchPos < searchLen; matchPos++ )
	{
		if ( !strncmp ( str + matchPos, substr, subStrLen ) )
		{
			foundPos = matchPos;
		}
	}

	return ( foundPos );
}

static int atlasti ( char const *substr, char const *str )
{
	int subStrLen;
	int	strLen;
	int searchLen;
	int	matchPos;
	int	foundPos;

	if ( !*str )
	{
		return ( -1 );
	}
	if ( !*substr )
	{
		return ( -1 );
	}
	
	foundPos = -1;
	
	subStrLen = (int)strlen ( substr );
	strLen = (int)strlen ( str );

	searchLen = strLen - subStrLen + 1;

	for ( matchPos = 0; matchPos < searchLen; matchPos++ )
	{
		if ( !_memicmp ( str + matchPos, substr, subStrLen ) )
		{
			foundPos = matchPos;
		}
	}

	return ( foundPos );
}

static int atnext ( char const *substr, char const *str, int occur )
{
	int subStrLen;
	int	strLen;
	int searchLen;
	int	matchPos;

	if ( !*str )
	{
		return ( -1 );
	}
	if ( !*substr )
	{
		return ( -1 );
	}
	
	subStrLen = (int)strlen ( substr );
	strLen = (int)strlen ( str );

	searchLen = strLen - subStrLen + 1;

	for ( matchPos = 0; matchPos < searchLen; matchPos++ )
	{
		if ( !strncmp ( str + matchPos, substr, subStrLen ) )
		{
			if ( !--occur )
			{
				return ( matchPos );
			}
		}
	}

	return ( -1 );
}

static int atnexti ( char const *substr, char const *str, int occur )
{
	int subStrLen;
	int	strLen;
	int searchLen;
	int	matchPos;

	if ( !*str )
	{
		return ( -1 );
	}
	if ( !*substr )
	{
		return ( -1 );
	}
	
	subStrLen = (int)strlen ( substr );
	strLen = (int)strlen ( str );

	searchLen = strLen - subStrLen + 1;

	for ( matchPos = 0; matchPos < searchLen; matchPos++ )
	{
		if ( !_memicmp ( str + matchPos, substr, subStrLen ) )
		{
			if ( !--occur )
			{
				return ( matchPos );
			}
		}
	}

	return ( -1 );
}

static int attoken ( char const *str, char const *token, int num )
{
	int		 count;
	char const	*tokenPtr;
	int		 pos;

	count = 0;

	if ( !*str )
	{
		return ( -1 );
	}
	
	if ( !token )
	{
		token = "\x09\x0C\x1A\x20\x8A\x8D";
	}
	pos = 0;

	/* special case for very first token */
	if ( num == 1 )
	{
		while ( *str )
		{
			tokenPtr = token;
			while ( *tokenPtr )
			{
				if ( *str == *tokenPtr )
				{
					return ( pos );
				}
				tokenPtr++;
			}
			str++;
			pos++;
		}
	}
	num--;

	/* search the string for tokens */
	while ( *str )
	{
		/* check this character */
		tokenPtr = token;
		while ( *tokenPtr )
		{
			if ( *str == *tokenPtr )
			{
				/* found a token */
				count++;

				/* we're on a token...go to next chr */
				str++;
				pos++;

				if ( count == num )
				{
					/* found our token... now copy it */

					/* skip until next non-token */
					while ( *str )
					{
						tokenPtr= token;
						while ( *tokenPtr )
						{
							if ( *tokenPtr == *str )
							{
								break;
							}
							tokenPtr++;
						}
						if ( !*tokenPtr )
						{
							break;
						}
						str++;
						pos++;
					}

					return ( pos );
				}

				/* skip until next non-token */
				while ( *str )
				{
					tokenPtr= token;
					while ( *tokenPtr )
					{
						if ( *tokenPtr == *str )
						{
							break;
						}
						tokenPtr++;
					}
					if ( !*tokenPtr )
					{
						break;
					}
					str++;
					pos++;
				}
				/* we're on the right chr...but we'll increment it
					so we decrement it here so it all works out
				*/
				str--;
				pos--;
				break;
			}
			tokenPtr++;
		}
		str++;
		pos++;
	}
	return ( -1 );
}

static int wildcard ( char const *searchStr, char const *str )
{
	while ( *searchStr && *str )
	{
		switch ( *searchStr )
		{
			case '*':
				searchStr++;
				while ( *str )
				{
					if ( wildcard ( searchStr, str ) )
					{
						return ( 1 );
					}
					str++;
				}
				while ( *searchStr == '*' || *searchStr == '?' )
				{
					searchStr++;
				}
				if ( *searchStr )
				{
					return ( 0 );
				}
				break;
			case '?':
				str++;
				searchStr++;
				break;
			default:
				if ( *searchStr != *str )
				{
					return ( 0 );
				}
				str++;
				searchStr++;
				break;

		}
	}

	while ( *searchStr == '*' || *searchStr == '?' )
	{
		searchStr++;
	}
	if ( !*searchStr && !*str )
	{
		return ( 1 );
	}
	return ( 0 );
}

static int strcount ( char const *str2, char const *str1 )
{
	int len1;
	int	len2;
	int	cnt;

	len1 = (int)strlen ( str1 );
	len2 = (int)strlen ( str2 );

	if ( len2 > len1 )
	{
		return ( 0 );
	}

	len1 = len1 - len2 + 1;
	cnt = 0;

	while ( *str1 && len1 )
	{
		if ( !strncmp ( str1, str2, len2 ) )
		{
			cnt++;
		}
		str1++;
		len1--;
	}

	return ( cnt );
}

static int strcounti ( char const *str2, char const *str1 )
{
	int len1;
	int	len2;
	int	cnt;

	len1 = (int)strlen ( str1 );
	len2 = (int)strlen ( str2 );

	if ( len2 > len1 )
	{
		return ( 0 );
	}

	len1 = len1 - len2 + 1;
	cnt = 0;

	while ( *str1 && len1 )
	{
		if ( !_memicmp ( str1, str2, len2 ) )
		{
			cnt++;
		}
		str1++;
		len1--;
	}

	return ( cnt );
}

static int vmChrcount ( char const *chr, char const *str )
{
	int	loop;

	if ( !*chr )
	{
		return ( 0 );
	}
	if ( *(chr + 1) )
	{
		return ( 0 );
	}

	loop = 0;

	while ( *str )
	{
		if ( *(str++) == *chr )
		{
			loop++;
		}
	}
	return ( loop );

}

static int vmChrcounti ( char const *chr, char const *str )
{
	int		loop;
	uint8_t c2;

	if ( !*chr )
	{
		return ( 0 );
	}
	if ( *(chr + 1) )
	{
		return ( 0 );
	}

	loop = 0;

	c2 = caseInsenstiveTable[static_cast<unsigned char>(*chr)];
	while ( *str )
	{
		if ( caseInsenstiveTable[static_cast<unsigned char>(*str)] == c2 )
		{
			loop++;
		}
		str++;
	}
	return ( loop );

}

static int vmCheckScript ( VAR_STR *p1 )
{
	uint8_t			*str;
	uint8_t			 chr;
	size_t			 state;
	uint8_t	const	*scriptUc = (uint8_t const *)"<SCRIPT";
	uint8_t	const	*scriptLc = (uint8_t const *)"<script";

	str = (uint8_t *)p1->dat.str.c;

	state = 0;
	while ( *str )
	{
		chr = *str;
		if ( chr == '%' )
		{
			chr = static_cast<char>((hexEncodingTable[(size_t) str[1]] << 4) + hexEncodingTable[(size_t) str[2]]);
			str += 3;
		}
		switch ( state )
		{
			case 0:
				if ( chr == '<' ) state = 1;
				break;
			case 1:
				if ( chr == 's' || chr == 'S' )
				{
					state = 2;
				} else if ( !_isspace ( &chr ) ) 
				{
					state = 0;
				}
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
				if ( chr == scriptUc[state] || chr == scriptLc[state] )
				{
					state++;
				} else
				{
					state = 0;
				}
				break;
			case 7:
				return 1;
		}
	}
	if ( state == 7 ) return 1;
	return 0;
}

static VAR_STR vmBase64Encode ( class vmInstance *instance, VAR_STR *p1 )
{
	size_t		 loop;
	size_t		 outLen;
	uint8_t		*in ;
	uint8_t		*out;

	instance->result.type = slangType::eNULL;

	if ( !p1->dat.str.len )
	{
		return "";
	}

	outLen = ( (p1->dat.str.len - 1) / 3 + 1 ) * 4; /* round len to greatest 3rd and multiply by 4 */

	out = (uint8_t *) instance->om->alloc ( sizeof ( char ) * (outLen + 3 + 1) );

	instance->result.type			= slangType::eSTRING;
	instance->result.dat.str.len	= outLen;
	instance->result.dat.str.c		= (char *)out;

	in = (uint8_t *)p1->dat.str.c;

	for ( loop = 0; loop < p1->dat.str.len; loop += 3 )
	{
		unsigned char in2[3]{};
		in2[0] = in[0];
		in2[1] = loop + 1 < p1->dat.str.len ? in[1] : 0;
		in2[2] = loop + 2 < p1->dat.str.len ? in[2] : 0;

		out[0] = webBase64EncodingTable[(in2[0] >> 2)];
		out[1] = webBase64EncodingTable[((in2[0] & 0x03) << 4) | (in2[1] >> 4)];
		out[2] = webBase64EncodingTable[((in2[1] & 0x0F) << 2) | (in2[2] >> 6)];
		out[3] = webBase64EncodingTable[((in2[2] & 0x3F))];
		/* increment our pointers appropriately */
		out += 4;
		in += 3;
	}

	out[0] = 0;

	/* fill in "null" character... all 0's in important bits */
	switch ( p1->dat.str.len % 3 )
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

	return *(VAR_STR *)&instance->result;
}

static VAR_STR vmBase64Decode ( class vmInstance *instance, VAR_STR *p1 )
{
	size_t		 decodedLen;
	size_t		 loop;
	uint8_t		*in ;
	uint8_t		*out;
	uint8_t		 tmp[4] = "";

	in = (uint8_t *)p1->dat.str.c;

	instance->result.type = slangType::eNULL;

	decodedLen = (p1->dat.str.len + 3) / 4 * 3;

	if ( in[p1->dat.str.len - 1] == '=' )
	{
		decodedLen--;
	}
	if ( in[p1->dat.str.len - 2] == '=' )
	{
		decodedLen--;
	}

	auto ret = out = (uint8_t *) instance->om->alloc ( sizeof ( char ) * (decodedLen + 2 + 1) );

	for ( loop = 0; loop < p1->dat.str.len; loop += 4 )
	{
		/* convert them from the alphabet to the binary equialent.. pos in the alphabet */
		tmp[0] = webBase64DecodingTable[in[0]];
		tmp[1] = webBase64DecodingTable[in[1]];
		tmp[2] = webBase64DecodingTable[in[2]];
		tmp[3] = webBase64DecodingTable[in[3]];

		out[0] = (tmp[0] << 2) | (tmp[1] >> 4);
		out[1] = ((tmp[1] & 0x0F) << 4) | (tmp[2] >> 2);
		out[2] = ((tmp[2] & 0x03) << 6) | tmp[3];

		/* advance pointers */
		out += 3;
		in  += 4;
	}
	ret[decodedLen] = 0;
	return VAR_STR ( (char *)ret, decodedLen );
}

static VAR vmVal ( vmInstance *instance, char const *p )
{
   if ( _chrany ( ".", p ) )
   {
      instance->result.type         = slangType::eDOUBLE;
      instance->result.dat.d        = atof ( p );
   } else
   {
      instance->result.type        = slangType::eLONG;
      instance->result.dat.l       = atol ( p );
   }
   return instance->result;
}

static VAR_STR vmStr ( vmInstance *instance, VAR *var )
{
	char  buff[64] = "";
	char const *dest = buff;
	
	switch ( TYPE ( var ) )
	{
		case slangType::eSTRING:
			instance->result = *var;
			return *(VAR_STR *)&instance->result;
		case slangType::eLONG:
			sprintf_s ( buff, sizeof ( buff ), "%zd", var->dat.l );
			break;
		case slangType::eDOUBLE:
			sprintf_s ( buff, sizeof ( buff ), "%e", var->dat.d );
			break;
		case slangType::eARRAY_ROOT:
			dest = "(ARRRAY)";
			break;
		case slangType::eOBJECT_ROOT:
			dest = "(OBJECT)";
			break;
		case slangType::eNULL:
			dest = "(NULL)";
			break;
		case slangType::eCODEBLOCK_ROOT:
			dest = "(CODEBLOCK)";
			break;
		case slangType::eATOM:
			dest = "(FUNCTION)";
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}
	return VAR_STR ( instance, dest );
}

static uint8_t vmAsc ( char const *p )
{
	return *(uint8_t const *)p;
}

static VAR_STR vmChr ( vmInstance *instance, uint8_t val  )
{
	char tmp[2] = "";
	tmp[0] = static_cast<char>(val);
	tmp[1] = 0;
	return VAR_STR ( instance, tmp, 1 );
}

static VAR_STR vmDecodeUrl ( class vmInstance *instance, VAR_STR *var )
{
	uint8_t		*src;
	size_t		 len;
	int			 val;
	BUFFER		 buffer;

	src = (uint8_t *)var->dat.str.c;
	len = var->dat.str.len;

	while ( len-- )
	{
		if ( *src == '%' )
		{
			src++;
			if ( len-- )
			{
				val = hexDecodingTable[(size_t)*(src++)] << 4;
				if ( len-- )
				{
					val |= hexDecodingTable[(size_t) *(src++)];
					bufferPut8 ( &buffer, val );
				} else
				{
					break;
				}
			} else
			{
				break;
			}
		} else if ( *src == '+' )
		{
			bufferPut8 ( &buffer, ' ' );
			src++;
		} else
		{
			bufferPut8 ( &buffer, *(src++) );
		}		
	}

	return VAR_STR ( instance, bufferBuff ( &buffer ), bufferSize ( &buffer ) );
}

static VAR_STR vmEncodeUrl ( class vmInstance *instance, VAR_STR *var )
{
	BUFFER		 buffer;
	uint8_t		*src;
	size_t		 len;

	src = (uint8_t *)var->dat.str.c;
	len = var->dat.str.len;

	while ( len-- )
	{
		if ( *src == ' ' )
		{
			bufferPut8 ( &buffer, '+' );
		} else if ( _isurl ( src ) )
		{
			bufferPrintf ( &buffer, "%%%02X", *src );
		} else
		{
			bufferPut8 (&buffer, (char)*src );
		}
		src++;
	}

	return VAR_STR ( instance, bufferBuff ( &buffer ), bufferSize ( &buffer ) );
}

static VAR_STR vmEncodeDAV ( class vmInstance *instance, VAR_STR *var )
{
	BUFFER		 buffer;
	uint8_t		*src;
	size_t		 len;

	src = (uint8_t *)var->dat.str.c;
	len = var->dat.str.len;

	while ( len-- )
	{
		if ( _isurl ( src ) )
		{
			bufferPrintf ( &buffer, "%%%02X", *src );
		} else
		{
			bufferPut8 (&buffer, (char)*src );
		}
		src++;
	}

	return VAR_STR ( instance, bufferBuff ( &buffer ), bufferSize ( &buffer ) );
}

//                          abcdefghijklmnopqrstuvwxyz
static char sxConvert[] = "01230120022455012623010202";

static VAR_STR vmSoundex ( class vmInstance *instance, VAR *var, size_t length )
{
	char const	*src;
	char		*dest;
	uint8_t		 last;
	uint8_t		 chr;
	size_t		 numOut;

	if ( TYPE ( var ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;

	dest = (char *)instance->om->alloc ( sizeof ( char ) * (length + 1) );

	instance->result.type = slangType::eSTRING;
	instance->result.dat.str.len = length;
	instance->result.dat.str.c = dest;

	src = var->dat.str.c;

	numOut = 0;		// number of output characters
	last = 7;		// indicator for first character

	if ( *src )
	{
		chr = *(src++);

		if ( (chr >= 'a') && (chr <= 'z') )
		{
			chr = chr - 'a' + 'A';
		}
		*(dest++) = static_cast<char>(chr);
		numOut++;
	}

	while ( *src )
	{
		chr = *(src++);

		if ( (chr >= 'a') && (chr <= 'z') )
		{
			chr = chr - 'a';
		} else if ( (chr >= 'A') && (chr <= 'Z') )
		{
			chr = chr - 'A';
		} else
		{
			/* not a character so the heck with it */
			continue;
		}

		/* get the soundex character */
		chr = sxConvert[chr];

		/* is it a repeat... don't do for repeats */
		if ( chr != last )
		{
			// check to see if it's a non 0 or first character
			if ( (last == 7) || (chr != '0') )
			{
				/* only output six characters */
				if ( numOut < length )
				{
					*(dest++) = static_cast<char>(chr);
					numOut++;
				} else
				{
					break;
				}
			}
		}
		last = chr;
	}
	while ( numOut < length )
	{
		*(dest++) = '0';
		numOut++;
	}
	*(dest) = 0;

	return *(VAR_STR *)&instance->result;
}

static VAR vmDescend ( class vmInstance *instance, VAR *val )
{
	char		*pPtr;
	char		*pRet;
	size_t		 ctr;

	switch ( TYPE( val ) )
	{
		case slangType::eSTRING:
			{
				auto ret = (char *) instance->om->alloc ( sizeof ( char ) * (val->dat.str.len + 1) );

				pPtr = (char *) val->dat.str.c;
				pRet = ret;
				ctr = val->dat.str.len;

				/* reverse characters */
				while ( ctr-- )
				{
					*(pRet++) = static_cast<char>((unsigned char) 255 - *(pPtr++));
				}

				/* null terminate string */
				*pRet = 0;
				return VAR_STR ( ret, val->dat.str.len );
			}
			break;

		case slangType::eLONG:
			return VAR ( -val->dat.l );

		case slangType::eDOUBLE:
			return VAR ( -val->dat.d );

		case slangType::eNULL:
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return *(VAR_STR *)&instance->result;
}

static uint32_t vmEmpty ( VAR *val )
{
   uint32_t loop;

   switch ( TYPE( val ) )
   {
      case slangType::eSTRING:
         for ( loop = 0; loop < val->dat.str.len; loop++ )
         {
            if ( val->dat.str.c[loop] != ' ' ) return 0;
         }
		 return 1;

      case slangType::eLONG:
         return val->dat.l == 0;

      case slangType::eDOUBLE:
         return val->dat.d == 0;

      default:
         throw errorNum::scINVALID_PARAMETER;
   }
}

static VAR_STR vmToken ( vmInstance *instance, VAR_STR *str, VAR_STR *token, int64_t num )
{
	const char	*tokenPtr;
	char const	*strPtr = str->dat.str.c;

	if ( !str->dat.str.len )
	{
		return VAR_STR ( "" );
	}

	/* special case for very first token */
	if ( num == 1 )
	{
		while ( *strPtr )
		{
			tokenPtr = token->dat.str.c;
			while ( *tokenPtr )
			{
				if ( *strPtr == *tokenPtr )
				{
					return VAR_STR ( instance, str->dat.str.c, strPtr - str->dat.str.c );
				}
				tokenPtr++;
			}
			strPtr++;
		}
		return *str;
	}
	num--;

	/* search the strPtring for tokens */
	while ( *strPtr )
	{
		/* check this character */
		tokenPtr = token->dat.str.c;
		while ( *tokenPtr )
		{
			if ( *strPtr == *tokenPtr )
			{
				/* found a token */
				if ( !--num )
				{
					// found the one we want
					strPtr++;
					auto start = strPtr;
					/* look for the start of the next token so we can get our length */
					while ( *strPtr )
					{
						tokenPtr = token->dat.str.c;
						while ( *tokenPtr )
						{
							if ( *strPtr == *tokenPtr )
							{
								return VAR_STR ( instance, start, strPtr - start );
							}
							tokenPtr++;
						}
						strPtr++;
					}
					return VAR_STR ( instance, start, strPtr - start );
				}
				break;
			}
			tokenPtr++;
		}
		strPtr++;
	}

	return VAR_STR ( "" );
}

static int64_t vmNumToken ( VAR_STR *str, VAR_STR *token )
{
	int			 count;
	char const	*tokenPtr;
	char const  *strPtr = str->dat.str.c;
	count = 0;

	while ( *strPtr )
	{
		tokenPtr = token->dat.str.c;
		while ( *tokenPtr )
		{
			if ( *strPtr == *tokenPtr )
			{
				/* found a token */
				count++;
				strPtr++;
			}
			tokenPtr++;
		}
		if ( *strPtr ) strPtr++;
	}

	/* +1 for trailiing token */
	return static_cast<int64_t>(count) + 1;
}

VAR_STR vmStrSwap ( class vmInstance *instance, VAR_STR *str, VAR_STR *toSwap, VAR_STR *swapWith )
{
	BUFFER	 res;

	if ( !toSwap->dat.str.len ) throw errorNum::scINVALID_PARAMETER;

	size_t search;
	for ( search = 0; search < str->dat.str.len; search++ )
	{
		size_t left = str->dat.str.len - search;
		if ( left < toSwap->dat.str.len ) break;

		if ( (left >= toSwap->dat.str.len) && !memcmp ( str->dat.str.c + search, toSwap->dat.str.c, toSwap->dat.str.len ) )
		{
			res.write ( swapWith->dat.str.c, swapWith->dat.str.len );
			search += toSwap->dat.str.len - 1;
		} else
		{
			res.put ( *(str->dat.str.c + search) );
		}
	}
	if ( search < str->dat.str.len )
	{
		res.write ( str->dat.str.c + search, str->dat.str.len - search );
	}
	return VAR_STR ( instance, res );
}

VAR_STR vmStrSwapI ( class vmInstance *instance, VAR_STR *str, VAR_STR *toSwap, VAR_STR *swapWith )
{
	BUFFER	 res;

	if ( !toSwap->dat.str.len ) throw errorNum::scINVALID_PARAMETER;

	size_t search;
	for ( search = 0; search < str->dat.str.len; search++ )
	{
		size_t left = str->dat.str.len - search;
		if ( left < toSwap->dat.str.len ) break;

		if ( (left >= toSwap->dat.str.len) && !_memicmp ( str->dat.str.c + search, toSwap->dat.str.c, toSwap->dat.str.len ) )
		{
			res.write ( swapWith->dat.str.c, swapWith->dat.str.len );
			search += toSwap->dat.str.len - 1;
		} else
		{
			res.put ( *(str->dat.str.c + search) );
		}
	}
	if ( search < str->dat.str.len )
	{
		res.write ( str->dat.str.c + search, str->dat.str.len - search );
	}
	return VAR_STR ( instance, res );
}

struct outVector {
	uint32_t data[8];

	outVector ( )
	{
		memset ( data, 0, sizeof ( data ) );
	}
	bool operator [] ( uint32_t index )
	{
		return data[index >> 5] & (1 << (index & 31)) ? 1 : 0;
	}
	outVector &operator |= ( outVector &v2 )
	{
		for ( size_t loop = 0; loop < sizeof ( data ) / sizeof ( *data ); loop++ )
		{
			data[loop] |= v2.data[loop];
		}
		return *this;
	}
	void set ( uint32_t index )
	{
		data[index >> 5] |= (1 << (index & 31));
		data[255 >> 5] |= (1 << (255 & 31));
	}
	bool isEmpty ( )
	{
		return (*this)[255] ? 0 : 1;
	}
};

struct matchingEngine {
	uint32_t	 MAXS = 1;
	uint32_t	 nKeywords;

	unsigned char lowestChar = ' ';
	unsigned char highestChar = 'Z';

	uint32_t	 MAXC;

	unsigned char	*str;
	bool			 doDelete = false;

	outVector		*out;			// output state
	int				*f;				// failure function
	int				*g;				// Goto function, or -1 if fail.

	private:

	void init ( const std::vector<std::string> &words )
	{
		for ( auto &i : words )
		{
			MAXS += (uint32_t)i.size ( );
		}

		nKeywords = (uint32_t)words.size ( );
		size_t	size;
		size = sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords );
		size += MAXS * sizeof ( *out );				// out
		size += MAXS * sizeof ( *f );				// f
		size += static_cast<size_t>(MAXC) * MAXS * sizeof ( *g );		// g

		str = new unsigned char[size];
		doDelete = true;

		memset ( str, 0, size );

		out = (outVector *) (str + sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords ));
		for ( size_t loop = 0; loop < MAXS; loop++ )
		{
			out[loop] = outVector ();
		}
		f = (int *) (str + sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords ) + sizeof ( *out ) * MAXS);
		g = (int *) (str + sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords ) + sizeof ( *out ) * MAXS + sizeof ( *f ) * MAXS);

		memset ( g, -1, static_cast<size_t>(MAXC) * MAXS * sizeof ( *g ) );

		size_t states = 1; // Initially, we just have the 0 state
		for ( size_t i = 0; i < words.size ( ); ++i )
		{
			const std::string &keyword = words[i];
			size_t currentState = 0;
			for ( size_t j = 0; j < keyword.size ( ); ++j )
			{
				int c = keyword[j] - lowestChar;
				if ( g[static_cast<size_t>(c) * MAXS + currentState] == -1 )
				{ // Allocate a new node
					g[static_cast<size_t>(c) * MAXS + currentState] = static_cast<int>(states++);
				}
				currentState = static_cast<size_t>(g[static_cast<size_t>(c) * MAXS + currentState]);
			}
			out[currentState].set ( (uint32_t)i ); // There's a match of keywords[i] at node currentState.
		}
		// State 0 should have an outgoing edge for all characters.
		for ( size_t c = 0; c < MAXC; ++c )
		{
			if ( g[c * MAXS + 0] == -1 )
			{
				g[c * MAXS + 0] = 0;
			}
		}

		// Now, let's build the failure function
		std::queue<int> q;
		for ( unsigned char c = 0; c <= highestChar - lowestChar; ++c )
		{
			// Iterate over every possible input
			// All nodes s of depth 1 have f[s] = 0
			if ( g[c * MAXS + 0] != -1 && g[c * MAXS + 0] != 0 )
			{
				f[g[c * MAXS + 0]] = 0;
				q.push ( g[c * MAXS + 0] );
			}
		}
		while ( q.size ( ) )
		{
			int state = q.front ( );
			q.pop ( );
			for ( int c = 0; c <= highestChar - lowestChar; ++c )
			{
				if ( g[c * MAXS + state] != -1 )
				{
					int failure = f[state];
					while ( g[c * MAXS + failure] == -1 )
					{
						failure = f[failure];
					}
					failure = g[c * MAXS + failure];
					f[g[c * MAXS + state]] = failure;
					out[g[c * MAXS + state]] |= out[failure]; // Merge out values
					q.push ( g[c * MAXS + state] );
				}
			}
		}
	}

	public:
	virtual ~matchingEngine ( )
	{
		if ( doDelete )
		{
			delete str;
		}
	}

	matchingEngine ( const std::vector<std::string> &words, unsigned char lowestChar, unsigned char highestChar ) : lowestChar ( lowestChar ), highestChar ( highestChar ), MAXC ( highestChar - lowestChar + 1 )
	{
		init ( words );
	}

	matchingEngine ( const std::vector<std::string> &words ) : MAXC ( highestChar - lowestChar + 1 )
	{
		init ( words );
	}

	matchingEngine ( unsigned char *strP )
	{
		doDelete = false;
		str = strP;

		MAXS = ((int *) str)[0];
		MAXC = ((int *) str)[1];
		nKeywords = ((int *) str)[2];
		out = ((outVector *) (str + sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords )));
		f = (int *) (str + sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords ) + sizeof ( *out ) * MAXS);
		g = (int *) (str + sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords ) + sizeof ( *out ) * MAXS + sizeof ( *f ) * MAXS);
	}

	VAR_STR serialize ( vmInstance *instance )
	{
		size_t	size;
		size = sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords );
		size += MAXS * sizeof ( *out );				// out
		size += MAXS * sizeof ( *f );					// f
		size += static_cast<size_t>(MAXC) * MAXS * sizeof ( *g );			// g

		VAR_STR	res;

		auto ret = (char *) instance->om->alloc ( size + 1 );

		((int*) ret)[0] = static_cast<int>(MAXS);
		((int*) ret)[1] = static_cast<int>(MAXC);
		((int*) ret)[2] = static_cast<int>(nKeywords);
		memcpy ( ret + sizeof ( MAXS ) + sizeof ( MAXC ) + sizeof ( nKeywords ), out, sizeof ( *out ) * MAXS );
		memcpy ( ret + sizeof ( MAXS ) + sizeof ( MAXS ) + sizeof ( nKeywords ) + sizeof ( *out ) * MAXS, f, sizeof ( *f ) * MAXS );
		memcpy ( ret + sizeof ( MAXS ) + sizeof ( MAXS ) + sizeof ( nKeywords ) + sizeof ( *out ) * MAXS + sizeof ( *f ) * MAXS, g, sizeof ( *g ) * MAXS *MAXC );

		ret[size] = 0;
		return VAR_STR ( ret, size );
	}

	uint32_t findNextState ( uint32_t currentState, unsigned char nextInput )
	{
		uint32_t answer = currentState;
		uint8_t c = nextInput - lowestChar;
		if ( c >= MAXC ) throw errorNum::scINVALID_PARAMETER;
		while ( g[c * MAXS + answer] == -1 )
		{
			answer = f[answer];
		}
		return g[c * MAXS + answer];
	}
};

static VAR_STR multiPatternBuild ( vmInstance *instance, VAR *arr )
{
	switch ( TYPE ( arr ) )
	{
		case slangType::eARRAY_SPARSE:
		case slangType::eARRAY_FIXED:
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	std::vector<std::string> keywords;

	int index = 1;
	while ( 1 )
	{
		auto var = (*arr)[index];
		if ( !var )
		{
			break;
		}

		keywords.push_back ( var->dat.str.c );
		index++;
	}

	if ( keywords.size ( ) > 255 ) throw errorNum::scINVALID_PARAMETER;

	matchingEngine matcher ( keywords );

	return matcher.serialize ( instance );
}

static int multiPatternFind ( VAR_STR *strP, VAR_STR *pattern, uint32_t maxMatch )
{
	matchingEngine matcher ( (unsigned char *)pattern->dat.str.c );

	std::vector<bool>	matches ( matcher.nKeywords );

	if ( !maxMatch ) maxMatch = -1;

	int currentState = 0;
	uint32_t nMatches = 0;
	auto str = strP->dat.str.c;
	for ( int i = 0; str[0]; str++, i++ )
	{
		currentState = static_cast<int>(matcher.findNextState ( currentState, *str ));
		if ( matcher.out[currentState].isEmpty ( ) )
			continue; // Nothing new, let's move on to the next character.
		for ( uint32_t j = 0; j < matcher.nKeywords; ++j )
		{
			if ( matcher.out[currentState][j] )
			{
				// Matched keywords[j]
				//				cout << "Keyword " << j << " appears at  " << i << endl;
				if ( !matches[j] )
				{
					nMatches++;
					if ( nMatches >= maxMatch )
					{
						return static_cast<int>(nMatches);
					}
				}
				matches[j] = true;
			}
		}
	}
	return static_cast<int>(nMatches);
}

static VAR_STR multiPatternNormalize ( vmInstance *instance, VAR_STR *strP )
{
	std::string dest;
	auto str = strP->dat.str.c;

	dest.reserve ( strlen ( str ) );

	bool spacing = true;
	while ( *str )
	{
		auto chr = *str;

		if ( chr >= 'a' && chr <= 'z' )
		{
			chr = static_cast<char>(static_cast<int>(chr) - 'a' + 'A');
		}
		if ( !((chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9')) )
		{
			chr = ' ';
		}
		if ( spacing )
		{
			if ( chr != ' ' )
			{
				dest += chr;
				spacing = false;
			}
		} else
		{
			if ( chr == ' ' )
			{
				spacing = true;
			}
			dest += chr;
		}
		str++;
	}
	return VAR_STR ( instance, dest );
}
static inline VAR *DEREF2 ( VAR *x ) { 
			while ( TYPE ( x ) == slangType::eREFERENCE ) x = x->dat.ref.v; 
			return x; 
}

VAR_STR strTemplate ( vmInstance *instance, VAR_STR *str, VAR *arr )
{
	std::vector<std::string> keywords;
	std::vector<std::string> replacement;

	switch ( TYPE ( arr ) )
	{
		case slangType::eARRAY_ROOT:
		case slangType::eOBJECT:
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	int64_t index = 1;
	while ( 1 )
	{
		VAR *a = (*arr)[index];

		if ( !a ) break;
		keywords.push_back ( DEREF2 ( (*a)[1] )->dat.str.c );
		replacement.push_back ( DEREF2  ( (*a)[2] )->dat.str.c );
		index++;
	}

	matchingEngine matcher ( keywords, 1, 255 );

	std::vector<bool> matches ( matcher.nKeywords );

	int currentState = 0;
	BUFFER buff;

	unsigned char *s = (unsigned char *) str->dat.str.c;
	for ( int i = 0; s[0]; s++, i++ )
	{
		currentState = static_cast<int>(matcher.findNextState ( currentState, *s ));
		if ( matcher.out[currentState].isEmpty ( ) )
		{
			// no match, emit character
			buff.put ( *s );
			continue;
		}
		for ( uint32_t j = 0; j < matcher.nKeywords; ++j )
		{
			if ( matcher.out[currentState][j] )
			{
				bufferUnWrite ( &buff, keywords[j].size ( ) - 1 );
				buff.write ( replacement[j].c_str ( ), replacement[j].size ( ) );
			}
		}
	}
	return VAR_STR ( instance, bufferBuff ( &buff ), bufferSize ( &buff ) );
}

VAR vmSplitString2 ( vmInstance *instance, VAR_STR *string, VAR_STR *sep, VAR_STR *sep2 )
{
	VAR arr = VAR_ARRAY ( instance );

	int64_t index = 0;
	size_t offset = 0;
	while ( string->dat.str.c[offset] )
	{
		// name
		auto offsetStart = offset;
		while ( string->dat.str.c[offset]  && !strchr ( sep->dat.str.c, string->dat.str.c[offset] ) && !strchr ( sep2->dat.str.c, string->dat.str.c[offset] ) )
		{
			offset++;
		}

		arrayGet ( instance, arr, index, 1 ) = VAR_STR ( instance, &string->dat.str.c[offsetStart], offset - offsetStart );

		// skip over seperator
		if ( string->dat.str.c[offset] && strchr ( sep->dat.str.c, string->dat.str.c[offset] ) )
		{
			offset++;

			// value
			auto offsetStart = offset;
			while ( string->dat.str.c[offset] && !strchr ( sep2->dat.str.c, string->dat.str.c[offset] ) && !strchr ( sep->dat.str.c, string->dat.str.c[offset] ) )
			{
				offset++;
			}

			arrayGet ( instance, arr, index, 2 ) = VAR_STR ( instance, &string->dat.str.c[offsetStart], offset - offsetStart );
		}

		index++;

		// skip over seperator
		if ( string->dat.str.c[offset] )
		{
			offset++;
		}
	}

	return arr;
}

VAR vmSplitString ( vmInstance *instance, VAR_STR *string, VAR_STR *sep )
{
	VAR arr = VAR_ARRAY ( instance );

	int64_t index = 1;
	size_t offset = 0;
	while ( string->dat.str.c[offset] )
	{
		// name
		auto offsetStart = offset;
		while ( string->dat.str.c[offset] && !strchr ( sep->dat.str.c, string->dat.str.c[offset] ) )
		{
			offset++;
		}

		arrayGet ( instance, arr, index ) = VAR_STR ( instance, &string->dat.str.c[offsetStart], offset - offsetStart );

		index++;

		// skip over seperator
		if ( string->dat.str.c[offset] )
		{
			offset++;
		}
	}
	return arr;
}

static VAR_STR vmSubstr ( vmInstance *instance, VAR_STR *src, int64_t start, nParamType n )
{
	size_t			cnt;

	if ( n == 1 )
	{
		auto val = n[0];
		if ( TYPE ( val ) != slangType::eLONG ) throw errorNum::scINVALID_PARAMETER;
		cnt = val->dat.l;
	} else
	{
		cnt = 0;
	}

	if ( (start < 0) || (cnt < 0) )
	{
		return VAR_STR ( "" );
	}

	if ( !cnt )
	{
		cnt = src->dat.str.len;
	}

	return vmMid ( instance, src, start, cnt );
}

VAR_STR vmStrextract ( vmInstance *instance, VAR_STR *strParam, VAR_STR *delim, int64_t occur )
{
	char		 leftDelim;
	char		 rightDelim;
	const char	*str;
	const char	*start;
	int			 startFound;
	BUFFER		 dest;

	if ( (leftDelim = delim->dat.str.c[0]) )
	{
		if ( (rightDelim = delim->dat.str.c[1]) )
		{
			if ( delim->dat.str.len > 2 )
			{
				VAR_STR ( "", 1 );
			}
		} else
		{
			rightDelim = leftDelim;
			if ( occur > 0 )
			{
				occur--;
			}
		}
	} else
	{
		return VAR_STR ( "", 1 );
	}

	if ( !strParam->dat.str.len )
	{
		return VAR_STR ( "", 1 );
	}


	if ( occur >= 0 )
	{
		str = strParam->dat.str.c;
		start = str;

		startFound = 1;

		while ( *str )
		{
			if ( *str == rightDelim )
			{
				if ( !startFound )
				{
					start = str + 1;
				}
				if ( startFound )
				{
					if ( !occur )
					{
						str = start;
						while ( *str && (*str != leftDelim) )
						{
							dest.put ( *(str++) );
						}
						return VAR_STR ( instance, dest );
					}
				}
				startFound = 0;
			}
			if ( *str == leftDelim )
			{
				start = str + 1;
				occur--;
				startFound = 1;
			}
			str++;
		}
		if ( !*str )
		{
			if ( startFound )
			{
				if ( !occur )
				{
					str = start;
					while ( *str && (*str != leftDelim) )
					{
						dest.put ( *(str++) );
					}
					return VAR_STR ( instance, dest );
				}
			}
		}
	} else
	{
		str = strParam->dat.str.c + strParam->dat.str.len - 1;

		while ( str >= strParam->dat.str.c )
		{
			if ( *str == leftDelim )
			{
				if ( !++occur )
				{
					str++;
					while ( *str && (*str != rightDelim) )
					{
						dest.put ( *(str++) );
					}
					return VAR_STR ( instance, dest );
				}
			}
			str--;
		}
		if ( !++occur )
		{
			str = strParam->dat.str.c;
			while ( *str && (*str != rightDelim) )
			{
				dest.put ( *(str++) );
			}
			return VAR_STR ( instance, dest );
		}
	}
	return VAR_STR ( "", 0 );
}

VAR_STR vmSubToken ( vmInstance *instance, VAR_STR *strParam, VAR_STR *token, VAR_STR *delim, VAR_STR *value )
{
	char const
		*tmpC;
	char const	*str = strParam->dat.str.c;

	if ( delim->dat.str.len != 1 || token->dat.str.len != 1 ) throw errorNum::scINVALID_PARAMETER;

	while ( *str )
	{
		/* does our search value match? */
		if ( (*str == value->dat.str.c[0]) && !memcmp ( str, value->dat.str.c, value->dat.str.len ) )
		{
			str += value->dat.str.len;
			if ( *str == delim->dat.str.c[0] )
			{
				str++;

				for ( tmpC = str; *tmpC && *tmpC != token->dat.str.c[0]; tmpC++ );

				return VAR_STR ( instance, str, tmpC - str );
			}
		}
		/* skip to next token */
		while ( *str && *str != token->dat.str.c[0] )
		{
			str++;
		}
		/* skip over token */
		if ( *str )
		{
			str++;
		}
	}
	return VAR_STR ( "", 0 );
}

VAR_STR vmSubTokenI ( vmInstance *instance, VAR_STR *strParam, VAR_STR *token, VAR_STR *delim, VAR_STR *value )
{
	char const	*tmpC;
	char const	*str = strParam->dat.str.c;

	if ( delim->dat.str.len != 1 || token->dat.str.len != 1 ) throw errorNum::scINVALID_PARAMETER;

	while ( *str )
	{
		/* does our search value match? */
		if ( (*str == value->dat.str.c[0]) && !_memicmp ( str, value->dat.str.c, value->dat.str.len ) )
		{
			str += value->dat.str.len;
			if ( *str == delim->dat.str.c[0] )
			{
				str++;

				for ( tmpC = str; *tmpC && *tmpC != token->dat.str.c[0]; tmpC++ );

				return VAR_STR ( instance, str, tmpC - str );
			}
		}
		/* skip to next token */
		while ( *str && *str != token->dat.str.c[0] )
		{
			str++;
		}
		/* skip over token */
		if ( *str )
		{
			str++;
		}
	}
	return VAR_STR ( "", 0 );
}

static VAR_STR vmWebHeader ( void )
{
	return VAR_STR ( "<html><body>" );
}

static VAR_STR vmWebFooter ( void )
{
	return VAR_STR ( "</body></html>" );
}

static VAR_STR vmDec2hex ( vmInstance *instance, int64_t num )
{
	char str[265];

	_snprintf_s ( str, sizeof ( str ), _TRUNCATE, "%I64X", num );

	return VAR_STR ( instance, str );
}

static VAR_STR vmConvertFilterString ( vmInstance *instance, char const *str, VAR *arr )
{
	switch ( TYPE ( arr ) )
	{
		case slangType::eARRAY_SPARSE:
		case slangType::eARRAY_FIXED:
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	std::vector<std::string> keywords;

	int index = 1;
	while ( 1 )
	{
		auto var = (*arr)[index];
		if ( !var )
		{
			break;
		}

		while ( TYPE ( var ) == slangType::eREFERENCE ) var = var->dat.ref.v;

		if ( TYPE ( var ) != slangType::eSTRING )
		{
			throw errorNum::scINVALID_PARAMETER;
		}

		keywords.push_back ( var->dat.str.c );

		index++;
	}
	BUFFER buff;

	buff.write ( "{|arr|", 6 );
	for ( ; *str; )
	{
		if ( _issymbol ( str ) )
		{
			auto start = str;
			while ( _issymbolb ( str ) )
			{
				str++;
			}

			bool found = false;
			for ( auto &it : keywords )
			{
				if ( it.length ( ) == str - start )
				{
					if ( !_memicmp ( it.c_str ( ), start, it.length ( ) ) )
					{
						buff.write ( "arr[\"", 5 );
						buff.write ( it.c_str ( ), it.length ( ) );
						buff.write ( "\"]", 2 );
						found = true;

						while ( *str == ' ' ) buff.put ( *(str++) );
						while ( *str == '.' )
						{
							bool found = false;
							instance->atomTable->typeMap ( atom::atomType::aFUNCDEF, [&]( uint32_t index )
														   {
															   auto name = instance->atomTable->atomGetName ( index );
															   if ( !_memicmp ( name, str + 1, strlen ( name ) ) )
															   {
																   if ( !_issymbolb ( str + strlen ( name ) ) )
																   {
																	   while ( *str != '(' )
																	   {
																		   buff.put ( *(str++) );
																	   }
																	   buff.put ( *(str++) );
																	   found = true;
																	   return true;
																   }
															   }
															   return false;
														   }
							);
							if ( !found )
							{
								str++;
								buff.write ( "[\"", 2 );
								while ( _issymbolb ( str ) )
								{
									buff.put ( *(str++) );
								}
								buff.write ( "\"]", 2 );
								while ( *str == ' ' ) buff.put ( *(str++) );
							}
						}
						break;
					}
				}
			}
			if ( !found )
			{
				str = start;
				while ( _issymbolb ( str ) )
				{
					buff.put ( *(str++) );
				}
			}
		} else if ( *str == '"' )
		{
			buff.put ( *(str++) );
			while ( *str && *str != '"' )
			{
				buff.put ( *(str++) );
			}
			buff.put ( *(str++) );
		} else
		{
			buff.put ( *(str++) );
		}
	}
	buff.put ( '}' );
	return VAR_STR ( instance, buff );
}

VAR_STR vmGUID ( vmInstance *instance )
{
	GUID guid;
	CoCreateGuid ( &guid );

	BYTE * str;
	UuidToString ( (UUID*) &guid, &str );

	VAR_STR ret ( instance, (char *)str );

	RpcStringFree ( &str );

	return ret;
}

VAR_STR vmChrswapall ( vmInstance *instance, char* str, char* search, char* repl )
{
	BUFFER buff;

	if ( !*str )
	{
		return "";
	}

	while ( *str )
	{
		char* s = search;
		while ( *s )
		{
			if ( *str == *s )
			{
				break;
			}
			s++;
		}
		if ( *s )
		{
			bufferWriteString ( &buff, repl );
		} else
		{
			bufferPut8 ( &buff, *str );
		}
		str++;
	}

	return VAR_STR ( instance, buff );
}

VAR_STR vmChrswapalli ( vmInstance* instance, char* str, char* search, char* repl )
{
	BUFFER buff;

	if ( !*str )
	{
		return "";
	}

	while ( *str )
	{
		char* s = search;
		while ( *s )
		{
			char s1 = *s;
			char s2 = *str;

			s1 = s1 >= 'a' && s1 <= 'z' ? (char)((int)s1 - 'a' + 'A') : s1;
			s2 = s2 >= 'a' && s2 <= 'z' ? (char)((int)s2 - 'a' + 'A') : s2;
			if ( s1 == s2 )
			{
				break;
			}
			s++;
		}
		if ( *s )
		{
			bufferWriteString ( &buff, repl );
		} else
		{
			bufferPut8 ( &buff, *str );
		}
		str++;
	}

	return VAR_STR ( instance, buff );
}


void builtinStringInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		FUNC ( "ltrim", vmLtrim ) CONST PURE;
		FUNC ( "rtrim", vmRtrim ) CONST PURE;
		FUNC ( "alltrim", vmAlltrim ) CONST PURE;
		FUNC ( "untrim", vmUntrim ) CONST PURE;
		FUNC ( "rjust", vmRjust ) CONST PURE;
		FUNC ( "ljust", vmLjust ) CONST PURE;

		FUNC ( "left", vmLeft ) CONST PURE;
		FUNC ( "right", vmRight ) CONST PURE;
		FUNC ( "mid", vmMid ) CONST PURE;
		FUNC ( "space", vmSpace ) CONST PURE;
		FUNC ( "string", vmString ) CONST PURE;
		FUNC ( "replicate", vmReplicate ) CONST PURE;

		FUNC ( "isAlpha", isalpha_ ) CONST PURE;
		FUNC ( "isNum", isnum_ ) CONST PURE;
		FUNC ( "isDigit", isdigit_ ) CONST PURE;
		FUNC ( "isLower", islower_ ) CONST PURE;
		FUNC ( "isUpper", isupper_ ) CONST PURE;
		FUNC ( "strEmpty", strempty ) CONST PURE;
		FUNC ( "chrCount", vmChrcount ) CONST PURE;
		FUNC ( "chrCountI", vmChrcounti ) CONST PURE;
		FUNC ( "wildCard", wildcard ) CONST PURE;
		FUNC ( "strCount", strcount ) CONST PURE;
		FUNC ( "strCountI", strcounti ) CONST PURE;
		FUNC ( "strccmp", strccmp ) CONST PURE;
		FUNC ( "strcmpI", strccmp ) CONST PURE;
		FUNC ( "strCmp", strcmp ) CONST PURE;
		FUNC ( "strCommas", strcommas ) CONST PURE;

		FUNC ( "lower", vmLower ) CONST PURE;
		FUNC ( "upper", vmUpper ) CONST PURE;
		FUNC ( "strCapFirst", vmStrCapFirst ) CONST PURE;
		FUNC ( "chrSwap", vmChrSwap ) CONST PURE;
		FUNC ( "trimPunct", vmTrimPunct ) CONST PURE;

		FUNC ( "checkScript", vmCheckScript ) CONST PURE;
		FUNC ( "base64Encode", vmBase64Encode ) CONST PURE;
		FUNC ( "base64Decode", vmBase64Decode ) CONST PURE;

		FUNC ( "webDecodeUrl", vmDecodeUrl ) CONST PURE;
		FUNC ( "webEncodeUrl", vmEncodeUrl ) CONST PURE;
		FUNC ( "webEncodeDav", vmEncodeDAV ) CONST PURE;

		FUNC ( "val", vmVal ) CONST PURE;
		FUNC ( "str", vmStr ) CONST PURE;
		FUNC ( "asc", vmAsc ) CONST PURE;
		FUNC ( "chr", vmChr ) CONST PURE;

		FUNC ( "soundEx", vmSoundex, DEF ( 2, "5" ) ) CONST PURE;

		FUNC ( "descend", vmDescend ) CONST PURE;
		FUNC ( "empty", vmEmpty ) CONST PURE;
		
		FUNC ( "token", vmToken ) CONST PURE;
		FUNC ( "numToken", vmNumToken, DEF ( 2, "' \t='" ) ) CONST PURE;

		FUNC ( "strSwap", vmStrSwap ) CONST PURE;
		FUNC ( "strSwapI", vmStrSwapI ) CONST PURE;

		FUNC ( "split", vmSplitString ) CONST PURE;
		FUNC ( "split2", vmSplitString2 ) CONST PURE;
		FUNC ( "substr", vmSubstr ) CONST PURE;

		FUNC ( "strextract", vmStrextract ) CONST PURE;
		FUNC ( "subToken", vmSubToken ) CONST PURE;
		FUNC ( "subTokenI", vmSubTokenI ) CONST PURE;

		FUNC ( "at", at, DEF ( 3, "1" ) ) CONST PURE;
		FUNC ( "atI", ati, DEF ( 3, "1" ) ) CONST PURE;
		FUNC ( "atDiff", atdiff ) CONST PURE;
		FUNC ( "atDiffI", atdiffi ) CONST PURE;
		FUNC ( "atLast", atlast ) CONST PURE;
		FUNC ( "atLastI", atlasti ) CONST PURE;
		FUNC ( "atNext", atnext ) CONST PURE;
		FUNC ( "atNextI", atnexti ) CONST PURE;
		FUNC ( "attoken", attoken ) CONST PURE;
		DOC ( R"(
					@func at returns the offset of the first occurence of substring in string beginning at [offset=1]
						@param param1(substr) string to search for
						@param param2(str) specifies the string to search
						@param param3(offset) starting offset to search
					@func atI returns the case insensitive offset of the first occurence of substring in string beginning at [offset=1]
						@param param1(substr) string to search for
						@param param2(str) specifies the string to search
						@param param3(offset) starting offset to search
					@func atDiff returns the offset of the first difference between string1 and string1
						@param param1(str1)
						@param param2(str2)
					@func atDiffI returns the case insensitive offset of the first difference between string1 and string1
						@param param1(str1)
						@param param2(str2)
					@func atLast returns the offset of the last occurence of substr in string
						@param param1(str)
						@param param2(substr)
					@func atLastI returns the case insensitive offset of the last occurence of substr in string
						@param param1(str)
						@param param2(substr)
					@func atNext returns the offset of the index'th occurence of substr in str
						@param param1(str)
						@param param2(substr)
						@param param3(index)
					@func atNextI returns the case insensitive offset of the index'th occurence of substr in str
						@param param1(str)
						@param param2(substr)
						@param param3(index)
					@func atToken returns the offset of the index'th token in string 
						@param param1(str)
						@param param2(token)
						@param param3(index)
				)" );

		FUNC ( "strAt", at, DEF( 3, "1" ) ) CONST PURE;
		FUNC ( "strAtI", ati, DEF( 3, "1" ) ) CONST PURE;
		FUNC ( "strAtNext", stratnext, DEF( 3, "1" ), DEF( 4, "1" ) ) CONST PURE;
		FUNC ( "strAtNextI", stratnexti, DEF( 3, "1" ), DEF( 4, "1" ) ) CONST PURE;
		FUNC ( "strAtPrev", stratprev, DEF( 3, "1" ), DEF( 4, "1" ) ) CONST PURE;
		FUNC ( "strAtPrevI", stratprevi, DEF( 3, "1" ), DEF( 4, "1" ) ) CONST PURE;

		FUNC ( "strMultiPatternBuild", multiPatternBuild ) CONST PURE;
		FUNC ( "strMultiPatternFind", multiPatternFind, DEF ( 3, "-1" ) ) CONST PURE;
		FUNC ( "strMultiPatternNormalize", multiPatternNormalize ) CONST PURE;
		FUNC ( "strTemplate", strTemplate ) CONST PURE;

		FUNC ( "convertFilterString", vmConvertFilterString );

		FUNC ( "webHeader", vmWebHeader ) CONST PURE;
		FUNC ( "webFooter", vmWebFooter ) CONST PURE;

		FUNC ( "dec2hex", vmDec2hex ) CONST PURE;

		FUNC ( "genGuid", vmGUID );

		FUNC ( "_chrSwapAll", vmChrswapall ) CONST PURE;
		FUNC ( "_chrSwapAllI", vmChrswapalli ) CONST PURE;
	END;
}
