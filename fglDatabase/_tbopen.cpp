#include "odbf.h"

#include <filesystem>

TABLE *_tbOpen ( char const *filename, int openmode )
{
	TABLE					*dbf;
	size_t					 i;
	size_t					 toff;
	DBFHEADER				*header;

	/*
	* Check for file name.
	*/
	std::filesystem::path p ( filename );
	p.make_preferred ();
	if ( !p.has_extension () ) p.replace_extension ( __tbfExtension );
	dbf = new TABLE ( );

	if ( (dbf->fileHandle = CreateFile ( p.generic_string().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{	
		delete dbf;
		return nullptr;
	}

	TABLE_CONNECTION tblConn;

	dbf->mapFile ( );

	header = dbf->getHeader ( );

	// save off the last update count
	dbf->updateCount = header->updateCount;

	/*
	* Check for valid database file.
	*/
	if ( /*!header->updateCount || */ memcmp ( header->id, DBF_ID_STRING, sizeof ( header->id )) != 0 )
	{
		delete dbf;
		return nullptr;
	}

	header->updateCount = 0;

	dbf->blob          = 0;
	strncpy_s ( dbf->alias, sizeof(dbf->alias)-1, p.string ().c_str (), _TRUNCATE );

	/*
	*  Read in the fields structure
	*/
	
	dbf->fcount = (int)dbf->getHeader()->fieldCount;

	dbf->fields = new DBFFIELDS[dbf->fcount];

	for ( auto numFields = 0; numFields < dbf->fcount; numFields++ )
	{
		auto field = dbf->getField ( numFields );

		memcpy ( &dbf->fields[numFields], field, sizeof ( DBFFIELDS ) );
	}

	/*
	* Calculate the offsets for each field in the record structure
	*/
	toff = 0;
	for ( i = 0; i < dbf->fcount; i++)
	{
		dbf->fields[i].name[TB_NAMESIZE] = 0;							// protect ourself by guaranteing a null termination at the limit
		dbf->fields[i].nameLen = (int)strlen(dbf->fields[i].name) + 1;

		dbf->fields[i].offset = (unsigned long)toff;
		toff += _tbFieldLen( dbf, &tblConn, i+1 );

		dbf->fieldMap[stringi ( dbf->fields[i].name )] = std::make_pair ( i, &dbf->fields[i] );
	}

	if ( dbf->getHeader ( )->capabilities & DBF_CAPABILITY_BLOB )
	{
		std::filesystem::path p2 ( filename );
		p2.replace_extension ( __BLOBExtension );

		if ( !(dbf->blob = blobInit ( p2.generic_string ().c_str(), dbf->updateCount )) )
		{
			delete dbf;
			return 0;
		}
	}
	return dbf;
}

TABLE *_tbOpenRepair ( char const *filename, int openmode )
{
	TABLE					*dbf;
	size_t					 i;
	size_t					 toff;
	DBFHEADER				*header;

	/*
	* Check for file name.
	*/
	std::filesystem::path p ( filename );
	if ( !p.has_extension () ) p.replace_extension ( __tbfExtension );
	dbf = new TABLE ( );

	if ( (dbf->fileHandle = CreateFile ( p.generic_string ().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{	
		delete dbf;
		return nullptr;
	}

	TABLE_CONNECTION tblConn;

	dbf->mapFile ( );

	header = dbf->getHeader ( );

	// save off the last update count
	dbf->updateCount = dbf->header.updateCount;

	header->updateCount = 0;

	dbf->blob          = 0;
	strncpy_s ( dbf->alias, sizeof(dbf->alias)-1, p.generic_string ().c_str (), _TRUNCATE );

	/*
	*  Read in the fields structure
	*/

	dbf->fcount = (int)dbf->getHeader()->fieldCount;

	dbf->fields = new DBFFIELDS[dbf->fcount];

	for ( auto numFields = 0; numFields < dbf->fcount; numFields++ )
	{
		auto field = dbf->getField ( numFields );

		memcpy ( &dbf->fields[numFields], field, sizeof ( DBFFIELDS ) );
	}

	/*
	* Calculate the offsets for each field in the record structure
	*/
	toff = 0;
	for ( i = 0; i < dbf->fcount; i++)
	{
		dbf->fields[i].name[TB_NAMESIZE] = 0;							// protect ourself by guaranteing a null termination at the limit
		dbf->fields[i].nameLen = (int)strlen(dbf->fields[i].name) + 1;

		dbf->fields[i].offset = (unsigned long)toff;
		toff += _tbFieldLen( dbf, &tblConn, i+1 );

		dbf->fieldMap[stringi ( dbf->fields[i].name )] = std::make_pair ( i, &dbf->fields[i] );
	}

	if ( dbf->getHeader ( )->capabilities & DBF_CAPABILITY_BLOB )
	{
		std::filesystem::path p2 ( filename );
		p2.replace_extension ( __BLOBExtension );

		if ( !(dbf->blob = blobInitRead ( p2.generic_string ().c_str(), dbf->updateCount )) )
		{
			delete dbf;
			throw errorNum::scDATABASE_INTEGRITY;
		}
	}
	return dbf;
}
