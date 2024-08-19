/*
	database definition parse/write routines

*/

#include "dbIoServer.h"
#include "odbf.h"

#include "io.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "fcntl.h"
#include "direct.h"

#include "Utility/funcky.h"

enum class configSections {
	CONFIG_DATABASE,
	CONFIG_FILTER,
	CONFIG_INDEX
};

struct flagId {
	char const			*id;
	int					 len;
	configSections					 bitPos;
};

static flagId areas[]	=	{	
								{ "DATABASE",	8, configSections::CONFIG_DATABASE },
								{ "FILTER",		6, configSections::CONFIG_FILTER },
								{ "INDEX",		5, configSections::CONFIG_INDEX },
								{ 0,			0, configSections::CONFIG_DATABASE }
							};

static unsigned dbReadLine ( int handle, char *buffer, int len )
{
	char	chr;
	int		retLen;

	len--;	/* for ending null */

	retLen = 0;

	while ( !_eof ( handle ) )
	{
		if ( _read ( handle, &chr, 1 ) != 1 )
		{
			*buffer = 0;
			return ( -1 );
		}
		if ( (chr == '\r') || (chr == '\n') )
		{
			if ( _read ( handle, &chr, 1 ) == 1 )
			{
				if ( chr != '\n' )
				{
					_lseek ( handle, -1, SEEK_CUR );
				}
			}
			*buffer = 0;
			return ( retLen );
		}
		*(buffer++) = chr;
		retLen++;

		if ( !--len )
		{
			*(buffer++) = 0;
			return ( retLen );
		}
	}
	*buffer = 0;
	return ( retLen );
}

void dbDefWrite ( DATABASE *db )
{
	HANDLE	 handle;
	BUFFER	 buff;
	char	*cPtr;

	if ( (handle = CreateFile ( db->dbDef->fName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		throw (errorNum) GetLastError();
	}

	buff.printf ( "[DATABASE]\r\n" );
	buff.printf ( "TRACE=%s\r\n",			db->dbDef->traceOutput ? "ON" : "OFF");
	if ( db->dbDef->integrityCheck )  
	{
		buff.printf ( "CHECK=%s\r\n",		db->dbDef->integrityCheck ? (db->dbDef->integrityCheck == 1 ? "ON" : "REPAIR") : "OFF" );
	}
	buff.printf ( "\r\n" );
	buff.printf ( "[INDEX]\r\n" );

	for ( auto &it : db->ftx )
	{
		buff.printf ( "\"%s\"=", it.fName.c_str() );
		buff.printf ( "\"%s\",", it.fName.c_str ( ) );
		buff.printf ( "%s,", it.getHeader()->isDescending ? "DESCENDING" : "ASCENDING" );

		buff.put ( '"' );
		cPtr = it.getHeader ( )->keyExpr;
		while ( *cPtr )
		{
			if (*cPtr == '"' )
			{
				buff.write ( "\\\"", 2 );
			} else
			{
				buff.put ( *cPtr );
			}
			cPtr++;
		}
		buff.write ( "\"", 1 );

		if ( it.getHeader()->isFilter )
		{
			buff.write ( ",\"", 2 );
			cPtr = it.getHeader()->filterExpr;
			while ( *cPtr )
			{
				if (*cPtr == '"' )
				{
					buff.write ( "\\\"", 2);
				} else
				{
					buff.put ( *cPtr );
				}
				cPtr++;
			}
			buff.write ( "\"", 1 );
		}
		buff.printf ( "\r\n" );
	}
	buff.printf ( "\r\n" );

	DWORD nWritten;
	WriteFile ( handle, buff.data<char *> ( ), (DWORD)buff.size ( ), &nWritten, 0 );

	CloseHandle ( handle );
}

DBDEF *dbDefRead ( DB_SERVER *dbServer, char const *fileName )

{
	char			 fName[MAX_PATH + 1];
	int				 handle;
	int				 loop;
	unsigned int	 len;
	configSections	 area;
	char			*cPtr;
	char			*valuePtr;
	char			 inLine[1024];
	char			 name[1024];
	char			 value[1024];
	char			 subValue1[1024];
	char			 subValue2[1024];
	char			 subValue3[1024];
	DBDEF			*dbDef;
	DBDEF_FILTER	 filter;
	DBDEF_INDEX		 index;

	dbDef = new DBDEF ( );

	_fmerge ( "*.DBI", fileName, fName, sizeof ( fName ) );
	dbDef->fName = fName;

	// default to the global setting
	dbDef->integrityCheck = dbServer->dbConfig->integrityCheck;

	if ( _sopen_s ( &handle, fName, _O_RDONLY, _SH_DENYWR, _S_IREAD ) )
	{
		return ( dbDef );
	}

	_close ( handle );
	return ( dbDef );

	while ( !_eof ( handle ) )
	{
		if ( (len = dbReadLine ( handle, inLine, 256 )) == -1 )
		{
			break;
		}
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
			for ( loop = 0; areas[loop].id; loop++ )
			{
				if ( !strccmp ( areas[loop].id, inLine ) )
				{
					switch ( areas[loop].bitPos )
					{
						case configSections::CONFIG_DATABASE:
						case configSections::CONFIG_FILTER:
						case configSections::CONFIG_INDEX:
							area = (configSections)areas[loop].bitPos;
							break;
					}
					break;
				}
			}
		} else if ( *inLine )
		{
			switch ( area )
			{
				case configSections::CONFIG_DATABASE:
					_tokenn ( inLine, "=", 1, name, sizeof ( name ) );
					strncpy_s ( value, sizeof ( value ), inLine + _attoken ( inLine, "=", 1 ) + 1, _TRUNCATE );

					_tokenn ( value, ",", 1, subValue1, sizeof ( subValue1 ) );
					_tokenn ( value, ",", 2, subValue2, sizeof ( subValue2 ) );
					_tokenn ( value, ",", 3, subValue3, sizeof ( subValue3 ) );

					_alltrim ( name, name );
					_alltrim ( value, value );
					_alltrim ( subValue1, subValue1 );
					_alltrim ( subValue2, subValue2 );
					_alltrim ( subValue3, subValue3 );

					if ( !strccmp ( name, "CHECK" ) )
					{
						dbDef->integrityCheck = 1;
						if ( !strccmp ( value, "OFF" ) )
						{
							dbDef->integrityCheck = 0;
						} else if ( !strccmp ( value, "ON" ) )	
						{
							dbDef->integrityCheck = 1;
						} else if ( !strccmp ( value, "REPAIR" ) )	
						{
							dbDef->integrityCheck = 2;
						} else
						{
							printf ( "error: CHECK must be either ON, OFF, or REPAIR\r\n" );
						}
					} else if ( !strccmp ( name, "TRACE" ) )
					{
						if ( !strccmp ( value, "OFF" ) )
						{
							dbDef->traceOutput = 0;
						} else if ( !strccmp ( value, "ON" ) )	
						{
							dbDef->traceOutput = 1;
						} else
						{
							printf ( "error: TRACE must be either ON or OFF\r\n" );
						}
					} else
					{
						printf ( "error: unknown database configuration setting: %s\r\n", inLine );
					}
					break;
				case configSections::CONFIG_FILTER:
					break;
				case configSections::CONFIG_INDEX:
					// <fname>=<alias>,<DESECENDING> | <ASCENDING>,<expr>
					_tokenn ( inLine, "=", 1, name, sizeof ( name ) );
					_tokenn ( inLine, "=", 2, value, sizeof ( value ) );

					_tokenn ( value, ",", 1, subValue1, sizeof ( subValue1 ) );
					_tokenn ( value, ",", 2, subValue2, sizeof ( subValue2 ) );
				
					_unQuote ( name, 0 );
					index.fName = name;
					index.alias = subValue1;

					if ( !strccmp ( subValue2, "DESCENDING" ) )
					{
						index.isDescending = 1;
					} else
					{
						index.isDescending = 0;
					}

					// start of key expression
					cPtr = value + _attoken ( value, ",", 3 );
					while ( _isspace ( cPtr ) ) cPtr++;
					if ( *cPtr == '"' )
					{
						cPtr++;
						valuePtr = subValue1;
						while ( *cPtr )
						{
							if ( (*cPtr == '\\') && (*(cPtr + 1) == '"') )
							{
								cPtr += 2;
								*(valuePtr++) = '"';
							} else if ( *cPtr == '"' )
							{
								cPtr++;
								break;
							} else
							{
								*(valuePtr++) = *(cPtr++);
							}

						}
						*(valuePtr++) = 0;
						index.expr = subValue1;

						while ( _isspace ( cPtr ) ) cPtr++;
						if ( *cPtr == ',' )
						{
							cPtr++;
							while ( _isspace ( cPtr ) ) cPtr++;
							if ( *cPtr == '"' )
							{
								cPtr++;
								valuePtr = subValue1;
								while ( *cPtr )
								{
									if ( (*cPtr == '\\') && (*(cPtr + 1) == '"') )
									{
										cPtr += 2;
										*(valuePtr++) = '"';
									} else if ( *cPtr == '"' )
									{
										cPtr++;
										break;
									} else
									{
										*(valuePtr++) = *(cPtr++);
									}

								}
								*(valuePtr++) = 0;
								index.filter = subValue1;
							}
						}
					} else
					{
						// old style compatability
						index.expr = value + _attoken ( value, ",", 3 );
					}
					dbDef->indexs.push_back ( index );
					break;
				default:
					printf ( "error: not in section\r\n" );
					break;
			}
		}
	}

	dbDef->traceOutput	  |= dbServer->dbConfig->traceOutput;	

	_close ( handle );
	return dbDef;
}
