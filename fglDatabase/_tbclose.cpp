#include "odbf.h"
#include "Db.h"
#include "blob.h"

TABLE::~TABLE ( )
{
	auto header = getHeader ( );
	if ( header )
	{
		header->updateCount = updateCount;
	}

	unMapFile ( );
	if ( fileHandle )
	{
		CloseHandle ( fileHandle );
	}
	if ( blob != 0 )
	{
		blob->setUpdateCount ( (int32_t)getUpdateCount () );
		delete blob;
	}
	if ( fields )
	{
		delete fields;
	}}
