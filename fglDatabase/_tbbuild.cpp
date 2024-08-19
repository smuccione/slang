#include "odbf.h"
#include "Utility/funcky.h"

#include <filesystem>

static TABLE *_tbInit ( char const *filename, size_t numfields, size_t recsize )
{

	int					 i;
	char				 buff[32];
	TABLE				*dbf;

	std::filesystem::path p ( filename );
	if ( !p.has_extension () ) p.replace_extension ( __tbfExtension );
	/*
	* Check for file name.
	*/
	if ( recsize < 1 )
	{
		recsize = 1;
	}

	if ( numfields < 1 )
	{
		numfields = 1;
	}

	if ( (unsigned int) numfields > recsize )
	{
		numfields = recsize;
	}

	/*
	*  Calculate size of object
	*/
	dbf = new TABLE ( );

	memcpy ( dbf->header.id, DBF_ID_STRING, sizeof ( dbf->header.id ) );
	dbf->header.lastRecord = 0;
	dbf->header.updateCount = 0;
	dbf->header.freeList = 0;
	dbf->header.capabilities = DBF_CAPABILITY_NONE;
	dbf->header.fieldCount = (unsigned long) numfields;
	dbf->header.recDataSize = (unsigned long) recsize;													// does not include record header
	dbf->header.version = DBF_VERSION;
	memset ( &dbf->header.pad[0], 0, sizeof ( dbf->header.pad ) );

	dbf->fields = new DBFFIELDS[numfields];
	memset ( dbf->fields, 0, sizeof ( DBFFIELDS ) * numfields );

	dbf->blob = 0;
	dbf->fcount = (int) numfields;

	strncpy_s ( dbf->alias, sizeof ( dbf->alias ) - 1, p.generic_string ().c_str(), _TRUNCATE );

	/*
	* Calculate the offsets for each field in the record structure
	*/
	for ( i = 0; i <= numfields - 1; i++ )
	{
		strncpy_s ( dbf->fields[i].name, sizeof ( dbf->fields[i].name ), "FIELD", _TRUNCATE );
		_itoa_s ( i + 1, buff, sizeof ( buff ), 10 );
		strncat_s ( dbf->fields[i].name, sizeof ( dbf->fields[i].name ), buff, _TRUNCATE );
		dbf->fields[i].nameLen = (int) strlen ( dbf->fields[i].name ) + 1;
		dbf->fields[i].offset = (long) i;
		dbf->fields[i].type = 'C';
		dbf->fields[i].leninfo.fieldlen = 1;
	}

	return(dbf);
}

static int tbFieldSet_ ( TABLE *dbf, size_t position, char const *fieldname, unsigned char type, size_t length, size_t decimals )
{
	char name[TB_NAMESIZE];

	_upper ( fieldname, name );
	_alltrim ( name, name );

	switch ( type )
	{
		case 'C':
		case 'S':
			break;

		case '1':
			length = 1;
			break;

		case '2':
			length = 2;
			break;

		case '4':
			length = 4;
			break;

		case 'N':
			length = (unsigned int) (length & 0x00FF);
			break;

		case 'L':
			length = TB_LOGSIZE;
			decimals = 0;
			break;

		case 'M':
			length = TB_MEMOSIZE;
			decimals = 0;
			break;

		case 'D':
			length = TB_DATESIZE;
			decimals = 0;
			break;

		case 'V':
			length = TB_VIRTUALSIZE;
			decimals = 0;
			break;

		case 'B':
			length = TB_BLOBSIZE;
			decimals = 0;
			break;

		case '~':
			length = TB_CRC32SIZE;
			decimals = 0;
			break;

		default:
			decimals = 0;
			break;
	}

	if ( position > 0 && position <= dbf->fcount )
	{
		--position;
		strncpy_s ( dbf->fields[position].name, TB_NAMESIZE, name, _TRUNCATE );
		dbf->fields[position].name[TB_NAMESIZE - 1] = 0;
		dbf->fields[position].nameLen = (int)strlen ( dbf->fields[position].name ) + 1;
		dbf->fields[position].type = type;
		if ( type == 'N' )
		{
			dbf->fields[position].leninfo.num.len = (char) length;
			dbf->fields[position].leninfo.num.dec = (char) (decimals & 255);
		} else
		{
			dbf->fields[position].leninfo.fieldlen = (short)length;
		}

		if ( position == 0 )
		{
			dbf->fields[position].offset = 0;
		} else
		{
			dbf->fields[position].offset = dbf->fields[position - 1].offset +
				((dbf->fields[position - 1].type == 'N') ? (unsigned int) dbf->fields[position - 1].leninfo.num.len : dbf->fields[position - 1].leninfo.fieldlen);
		}
		if ( dbf->fields[position].type == 'B' )
		{
			dbf->header.capabilities = DBF_CAPABILITY_BLOB;
		}
	}

	return(0);
}


TABLE * _tbBuild( char const *filename, TBFIELD * field, int count)
{
	TABLE * dbf;
	unsigned int recsize;
	size_t x;
	
	recsize = 0;
	for( x = 0; x < count; x++)
	{
		switch( field[x].type )
		{
			case '1':
				field[x].length   = 1;
				field[x].decimals = 0;
				break;
            
			case '2':
				field[x].length   = 2;
				field[x].decimals = 0;
				break;
				
			case '4':
				field[x].length   = 4;
				field[x].decimals = 0;
				break;
				
			case 'D':
			case 'd':
				field[x].length   = TB_DATESIZE;
				field[x].decimals = 0;
				break;
				
			case 'L':
			case 'l':
				field[x].length   = TB_LOGSIZE;
				field[x].decimals = 0;
				break;
				
			case 'B':
			case 'b':
			case 'O':
			case 'o':
				field[x].length   = TB_BLOBSIZE;
				field[x].decimals = 0;
				break;
				
			case '~':
				field[x].length   = TB_CRC32SIZE;
				field[x].decimals = 0;
				break;
			
			case 'C':
			case 'N':
			case 'S':
				break;
			default:
				return ( 0 );
		}
		
		recsize += (unsigned) field[x].length;
	}
	
	dbf = _tbInit( filename, count, recsize);
	if( !dbf )
	{
		return( 0L );
	}
	
	for( x = 0; x < count; x++)
	{
		tbFieldSet_ ( dbf, x + 1, field[x].name, field[x].type, field[x].length, field[x].decimals );
	}

	for ( auto loop = 0; loop < count; loop++ )
	{
		dbf->fieldMap[stringi ( dbf->fields[loop].name )] = std::make_pair ( loop, &dbf->fields[loop] );
	}
	return( dbf );
}

void _tbBuildFromTable ( char const *newFilename, TABLE *tblOld )
{
	char				 buff[16]{};
	DWORD				 nWritten;
	LARGE_INTEGER		 filePointer{};
	BLOB_FILE			*blob;
	HANDLE				 fHandle;
	DBFHEADER			 header;

	if ( (fHandle = CreateFile ( newFilename,
								 GENERIC_READ | GENERIC_WRITE,
								 FILE_SHARE_READ,
								 0,
								 CREATE_NEW,
								 FILE_ATTRIBUTE_ARCHIVE  /*| FILE_FLAG_WRITE_THROUGH */ | FILE_FLAG_RANDOM_ACCESS,
								 0
							)) == INVALID_HANDLE_VALUE	// NOLINT (performance-no-int-to-ptr)
		 )
	{
		throw (errorNum) GetLastError();
	}

	memcpy ( &header, tblOld->getHeader ( ), sizeof ( header ) );
	header.lastRecord = 0;
	header.updateCount = 1;
	header.freeList = 0;
	header.nAllocatedRecords = 0;

	// write out the table header
	WriteFile ( fHandle, &header, sizeof ( header ), &nWritten, 0 );
	WriteFile ( fHandle, tblOld->fields, tblOld->fcount * sizeof ( DBFFIELDS ), &nWritten, 0 );
	WriteFile ( fHandle, buff, sizeof ( char ) * 2, &nWritten, 0 );

	// we're now at the start of the data portion... let's find it
	filePointer.QuadPart = 0;
	SetFilePointerEx ( fHandle, filePointer, &filePointer, FILE_END );
	header.dataStartOffset = filePointer.QuadPart;

	// rewrite the file header with the updated data start entry
	filePointer.QuadPart = 0;
	SetFilePointerEx ( fHandle, filePointer, &filePointer, FILE_BEGIN );
	WriteFile ( fHandle, &header, sizeof ( header ), &nWritten, 0 );

	if ( header.capabilities & DBF_CAPABILITY_BLOB )
	{
		std::filesystem::path p ( newFilename );
		p.replace_extension ( __BLOBExtension );
		if ( !(blob = blobCreate ( p.generic_string ().c_str(), header.updateCount )) )
		{
			CloseHandle ( fHandle );
			throw (errorNum) GetLastError();
		}
		delete blob;
	}
	CloseHandle ( fHandle );
}
