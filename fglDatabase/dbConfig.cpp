/*
	database configuration information

*/

#include "odbf.h"

#include "Utility/funcky.h"
#include "dbConfig.h"

#include <istream>
#include <fstream>
#include <string>


enum class configSections {
	CONFIG_AUTOSTART,
	CONFIG_CONFIG,
};

struct flagId {
	char const			*id;
	int					 len;
	configSections		 bitPos;
};

static flagId areas[]	=	{	
								{ "AUTOSTART",	9, configSections::CONFIG_AUTOSTART },
								{ "CONFIG",		6, configSections::CONFIG_CONFIG },
								{ 0,			0, configSections::CONFIG_AUTOSTART }
							};
void dbConfigWrite ( DB_CONFIG *dbConfig )

{
	char			 fName[MAX_PATH + 1];

	_fmerge ( "*.DBI", dbConfig->configFileName.c_str(), fName, sizeof ( fName ) );

	std::ofstream oStream ( fName, std::ios_base::out | std::ios_base::trunc );

	oStream << "[CONFIG]" << std::ends;
	oStream << "CHECK=%s" << (dbConfig->integrityCheck ? (dbConfig->integrityCheck == 1 ? "ON" : "REPAIR") : "OFF") << std::ends;
	oStream << std::ends;
	oStream << "[AUTOSTART]" << std::ends;

	for ( auto &it : dbConfig->dbStartList )
	{
		if ( it.alias.size() )
		{
			oStream << it.fName << "=" << it.alias << std::ends;
		} else
		{
			oStream << it.fName << std::ends;
		}
	}
	oStream << std::ends;
}

DB_CONFIG *dbConfigRead ( char const *fileName )

{
	char			 fName[MAX_PATH + 1];
	int				 loop;
	configSections	 area;
	char			*dummy;
	char			 inLine[1024];
	char			 name[1024];
	char			 value[1024];
	DB_CONFIG		*dbConfig;
	SYSTEM_INFO		 systemInfo;

	GetSystemInfo ( &systemInfo );

	dbConfig = new DB_CONFIG ( );

	dbConfig->dbReqTimeout		= 5000;										// 5 seconds
	dbConfig->integrityCheck	= 0;										// no integrity checking by default
	dbConfig->maxConnections	= 64;										// 64 connections max by default
	dbConfig->traceOutput		= 0;										// disable
	dbConfig->port				= 3102;
	dbConfig->configFileName	= fileName;

	std::ifstream iStream ( fileName );
	if ( iStream.bad ( ) )
	{
		dbConfigWrite ( dbConfig );
		return dbConfig;
	}

	GetFullPathName ( "engine.ini", MAX_PATH, fName, &dummy );
	dbConfig->configFileName = fName;

	while ( !iStream.eof ( ) && iStream.good() )
	{
		std::string line;
		std::getline ( iStream, line );

		strcpy_s ( inLine, sizeof ( inLine ), line.c_str ( ) );

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
						case configSections::CONFIG_AUTOSTART:
						case configSections::CONFIG_CONFIG:
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
				case configSections::CONFIG_CONFIG:
					_tokenn ( inLine, "=", 1, name, sizeof ( name ) );
					_tokenn ( inLine, "=", 2, value, sizeof ( value ) );

					_alltrim ( name, name );
					_alltrim ( value, value );

					if ( !strccmp ( name, "CONNECTIONS" )  )
					{
						dbConfig->maxConnections = atoi ( value );
						if ( (dbConfig->maxConnections < 1) || (dbConfig->maxConnections > 32767) )
						{
							printf ( "error: the number of connections must be between 1 and 32767\r\n" );
							dbConfig->maxConnections = 16;
						} 
					}else if ( !strccmp ( name, "PORT" )  )
					{
						dbConfig->port = atoi ( value );
						if ( (dbConfig->port < 1) || (dbConfig->port > 65535) )
						{
							printf ( "error: the port number must be between 1 and 65535\r\n" );
							dbConfig->port = 3102;
						}
					} else 	if ( !strccmp ( name, "DATABASE_TIMEOUT" ) && *value )
					{
						dbConfig->dbReqTimeout = atoi ( value );
						if ( dbConfig->dbReqTimeout < 1 )
						{
							printf ( "error: database timeout must be alteast 1 second \r\n" );
							dbConfig->dbReqTimeout = 1;
						}
						dbConfig->dbReqTimeout *= 1000;
					} else if ( !strccmp ( name, "TRACE" ) )
					{
						if ( !strccmp ( value, "OFF" ) )
						{
							dbConfig->traceOutput = 0;
						} else if ( !strccmp ( value, "ON" ) )
	
						{
							dbConfig->traceOutput = 1;
						} else
						{
							printf ( "error: TRACE must be either ON or OFF\r\n" );
						}
					} else if ( !strccmp ( name, "CHECK" ) )
					{
						if ( !strccmp ( value, "OFF" ) )
						{
							dbConfig->integrityCheck = 0;
						} else if ( !strccmp ( value, "ON" ) )	
						{
							dbConfig->integrityCheck = 1;
						} else if ( !strccmp ( value, "REPAIR" ) )	
						{
							dbConfig->integrityCheck = 2;
						} else
						{
							printf ( "error: CHECK must be either ON, OFF, or REPAIR\r\n" );
						}
					}
					break;

				case configSections::CONFIG_AUTOSTART:
					// <fname>=<alias>
					_tokenn ( inLine, "=", 1, name, sizeof ( name ) );
					_tokenn ( inLine, "=", 2, value, sizeof ( value ) );

					_unQuote ( name, 0 );
					_unQuote ( value, 0 );

					dbConfig->dbStartList.push_back ( DB_START{ name, value }  );
					break;
				default:
					printf ( "error: not in section\r\n" );
					break;
			}
		}
	}

	return dbConfig;
}
