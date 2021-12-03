#include "webServer.h"
#include "webServerGlobalTables.h"

#define DEF_HEADER(name, value) { HTTP_##name, value, (sizeof ( value ) - 1) },
const httpHeaderTableDef	httpHeaderTable[] = {
														HTTP_HEADER_TABLE
												};
#undef DEF_HEADER


#define DEF_HEADER(name, value) std::make_pair( statString ( value ), HTTP_##name ),
std::pair<statString, httpHeader> httpHeaderMapData[] = {
		HTTP_HEADER_TABLE
};

std::map<statString, httpHeader> httpHeaderMap;

#define DEF_REQ(name) { req_##name, #name, (sizeof ( #name ) - 1) },
const reqTableDef	reqTable[] = {
											REQ_TABLE
										};
#undef DEF_REQ


#define DEF_REQ(name) std::make_pair( statString ( #name ), req_##name ),
std::pair<statString, reqId> reqMapTable[] = {
		REQ_TABLE
};

std::map<statString, reqId> reqMap;

void webServerGlobalTablesInit ( void )
{
	if ( !reqMap.size() )
	{
		reqMap		  = std::map<statString, reqId> ( reqMapTable, reqMapTable + sizeof reqMapTable / sizeof reqMapTable[0] );
		httpHeaderMap =	std::map<statString, httpHeader> ( httpHeaderMapData, httpHeaderMapData + sizeof httpHeaderMapData / sizeof httpHeaderMapData[0] );
	}
}