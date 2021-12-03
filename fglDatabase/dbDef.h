/*
	database definition file header


*/

#pragma once

#include "Utility/settings.h"

#include <vector>
#include <string>

struct DBDEF_FILTER	{
	std::string		fName;
	std::string		alias;
	std::string		expr;
};

struct DBDEF_INDEX {
	std::string		fName;
	std::string		alias;
	std::string		expr;
	std::string		filter;
	bool			isDescending;
};

/* configuration information for an individual database */
struct DBDEF {
	std::string				 fName;
	int						 integrityCheck;		// was the value specified in the config file?
	bool					 traceOutput;			// enable detailied tracing on the database

	std::string				 sqlUser;
	std::string				 sqlPass;

	std::vector<DBDEF_INDEX> indexs;
};

extern	void	 dbDefWrite		( struct DATABASE *db );
extern	DBDEF	*dbDefRead		( struct DB_SERVER *dbServer, char const *fileName );
