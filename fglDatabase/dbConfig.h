/*
		Database Configuration information

*/

#pragma once

#include "Utility/settings.h"

#include <stdint.h>
#include <string>
#include <vector>

struct DB_START {
	std::string	fName;
	std::string	alias;
};

struct DB_CONFIG {
	std::string				configFileName;
	uint16_t				port;
	size_t					maxConnections;	// maximum number of connections
	bool					traceOutput;		// enable tracing?
	size_t					dbReqTimeout;		// timeout 
	int						integrityCheck;	// integrity check the database upon opening... default = ON
	std::vector<DB_START>	dbStartList;
};

extern DB_CONFIG	*dbConfig;
extern void			 dbConfigWrite		( DB_CONFIG *dbConfig );
extern DB_CONFIG	*dbConfigRead		( char const *fileName );
extern int			 dbServerLog		( struct DB_SERVER_DB *dbs, char *fmt, ... );
