/*
		CSV reader class
			- opens the file using the global file cache
			- allows for creation of an enumerator

*/

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"
#include "Target/fileCache.h"

typedef std::map<stringi, size_t>	columnMap;
struct csvLine
{
	char		const	*lineStart;
	size_t				 lineLen;

	csvLine ( char const *lineStart, size_t lineLength ) : lineStart ( lineStart), lineLen ( lineLength )
	{
	}
};

struct csvReader
{
	columnMap									nameToColumn;
	std::vector<std::pair<char const *,size_t>>	lineInfo;
	fileCacheEntry								*entry;
	char const									*fileContents;
	size_t										 fileContentsLength;

	csvReader ( bool columnHeaders )
	{}
	csvReader ( csvReader &&right) noexcept
	{
		nameToColumn.swap ( right.nameToColumn );
		lineInfo.swap ( right.lineInfo );
		std::swap ( entry, right.entry );
		std::swap ( fileContents, right.fileContents );
		std::swap ( fileContentsLength, right.fileContentsLength );
	}
	csvReader ( csvReader const &right )
	{
		nameToColumn = right.nameToColumn;
		lineInfo = right.lineInfo;
		entry = right.entry;
		right.entry->refCnt++;
		fileContents = right.fileContents;
		fileContentsLength = right.fileContentsLength;
	}
	virtual ~csvReader ( )
	{
		if ( entry )
		{
			entry->free ( );
		}
	}
};

struct csvReaderEnumerator
{
	int64_t currentLine = 0;

	csvReaderEnumerator ()
	{}
	virtual ~csvReaderEnumerator ( )
	{}
};

static void csvReaderNew ( vmInstance *instance, VAR_OBJ *obj, char const *fileName, bool columnHeaders )
{
	auto reader = obj->makeObj<csvReader> ( instance, columnHeaders );

	reader->entry = globalFileCache.read ( fileName );
	if ( !reader->entry )
	{
		throw GetLastError ( );
	}
	reader->fileContents = reinterpret_cast<char const *>(reader->entry->getData ( ));
	reader->fileContentsLength = reader->entry->getDataLen ( );
	if ( reader->fileContentsLength >= 3 && !memcmp ( reader->fileContents, "\xEF\xBB\xBF", 3 ))
	{
		reader->fileContents += 3;
	}
	auto ptr = reader->fileContents;
	if ( columnHeaders )
	{
		// parse the headers from the first line
		size_t columnNumber = 0;
		while ( (ptr < reader->fileContents + reader->fileContentsLength) && !_iseol ( ptr ) )
		{
			while ( (ptr < reader->fileContents + reader->fileContentsLength) && _isspace ( ptr ) ) ptr++;
			char const *nameStart = ptr;
			char const *nameEnd;
			if ( ptr[0] == '\"' )
			{
				ptr++;
				nameStart = ptr;
				while ( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] != '\"') ) ptr++;
				nameEnd = ptr;
				if ( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] == '\"') ) ptr++;
			} else
			{
				while ( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] != ',') && !_isspace ( ptr ) ) ptr++;
				nameEnd = ptr;
			}
			while ( (ptr < reader->fileContents + reader->fileContentsLength) && _isspace ( ptr ) ) ptr++;
			if ( (ptr[0] == ',') || _iseol ( ptr ) )
			{
				ptr++;
			} else
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			reader->nameToColumn[stringi ( nameStart, nameEnd - nameStart )] = columnNumber++;
		}
	} else
	{
		// no header, so make field<x> names for each of the columns
		size_t columnNumber = 0;
		while ( (ptr < reader->fileContents + reader->fileContentsLength) && !_iseol ( ptr ) )
		{
			while ( (ptr < reader->fileContents + reader->fileContentsLength) && _isspace ( ptr ) ) ptr++;
			if ( ptr[0] == '\"' )
			{
				ptr++;
				while ( (ptr < reader->fileContents + reader->fileContentsLength) )
				{
					if ( (ptr[0] == '\"') && (ptr[1] == '\"') )
					{
						ptr += 2;
					} else if ( ptr[0] == '\"' )
					{
						ptr++;
						break;
					}
					ptr++;
				}
				while ( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] != ',') ) ptr++;
			} else
			{
				while ( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] != ',') ) ptr++;
			}
			if ( (ptr[0] == ',') || _iseol ( ptr ) )
			{
				ptr++;
			} else
			{
				throw errorNum::scINVALID_PARAMETER;
			}
			char tmpName[128];
			sprintf_s ( tmpName, sizeof ( tmpName ), "field%I64u", columnNumber + 1 );
			reader->nameToColumn[stringi ( tmpName )] = columnNumber++;
		}
	}
	if ( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] == '\r') )
	{
		ptr++;
		if( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] == '\n') )
		{
			ptr++;
		}
	} else
	{
		if ( (ptr < reader->fileContents + reader->fileContentsLength) && (ptr[0] == '\n') )
		{
			ptr++;
		}
	}
	auto lineStart = ptr;
	while ( (ptr < reader->fileContents + reader->fileContentsLength) )
	{
		if ( ptr[0] == '\r' )
		{
			ptr++;
			if ( (ptr >= reader->fileContents + reader->fileContentsLength) )
			{
				reader->lineInfo.push_back ( { lineStart, ptr - lineStart } );
			} else if ( ptr[0] == '\n' )
			{
				ptr++;
				reader->lineInfo.push_back ( { lineStart, ptr - lineStart - 1 } );
			}
			lineStart = ptr;
		} else if ( ptr[0] == '\n' )
		{
			ptr++;
			reader->lineInfo.push_back ( { lineStart, ptr - lineStart } );
			lineStart = ptr;
		} else if ( ptr[0] == '\"' )
		{
			ptr++;
			while ( (ptr < reader->fileContents + reader->fileContentsLength) )
			{
				if ( (ptr[0] == '\"') && (ptr[1] == '\"') )
				{
					ptr += 2;
				} else if ( ptr[0] == '\"' )
				{
					ptr++;
					break;
				} else
				{
					ptr++;
				}
			}
		} else
		{
			ptr++;
		}
	}
	if ( ptr - lineStart > 1 )
	{
		reader->lineInfo.push_back ( { lineStart, ptr - lineStart } );
	}
}

static void csvReaderRelease ( vmInstance *instance, VAR_OBJ *obj )
{
	auto reader = obj->getObj<csvReader> ( );

	reader->~csvReader ( );
}


static VAR_OBJ_TYPE<"csvReaderEnumerator">  csvReaderGetEnumerator ( vmInstance *instance, VAR_OBJ *obj )
{
	auto readerEnumerator = *classNew ( instance, "csvReaderEnumerator", 0 );

	*readerEnumerator["reader"] = *obj;

	readerEnumerator.makeObj<csvReaderEnumerator> ( instance );

	return readerEnumerator;
}

static VAR_OBJ_TYPE<"csvReaderEnumerator"> csvReaderEnumeratorGetEnumerator ( vmInstance *instance, VAR_OBJ *obj )
{
	return *obj;
}

static VAR_OBJ_TYPE<"csvLine"> csvReaderEnumeratorCurrent ( vmInstance *instance, VAR_OBJ *obj )
{
	auto enumerator = obj->getObj<csvReaderEnumerator> ( );
	auto readerVar = (*obj)["reader"];
	auto reader = static_cast<VAR_OBJ *>(readerVar)->getObj<csvReader> ( );

	if ( !enumerator->currentLine )
	{
		throw errorNum::scINVALID_INDICE;
	}
	// create new VM line object
	auto lineVar = *classNew ( instance, "csvLine", 0 );

	*lineVar["reader"] = *readerVar;

	VAR_OBJ ( &lineVar ).makeObj<csvLine> ( instance, reader->lineInfo[enumerator->currentLine - 1].first, reader->lineInfo[enumerator->currentLine - 1].second );

	// return the VM line object
	return VAR_OBJ ( lineVar );
}

static VAR csvReaderEnumeratorIndex ( vmInstance *instance, VAR_OBJ *obj )
{
	auto enumerator = obj->getObj<csvReaderEnumerator> ( );

	if ( !enumerator->currentLine )
	{
		throw errorNum::scINVALID_INDICE;
	}
	return enumerator->currentLine;
}

static VAR csvReaderEnumeratorSkip( vmInstance *instance, VAR_OBJ *obj, int64_t num )
{
	auto enumerator = obj->getObj<csvReaderEnumerator> ( );
	auto readerVar = (*obj)["reader"];
	auto reader = static_cast<VAR_OBJ *>(readerVar)->getObj<csvReader> ( );

	int64_t line = enumerator->currentLine;
	line += num;
	if ( line < 1 || line >= (int64_t) reader->lineInfo.size() )
	{
		return *obj;
	}
	enumerator->currentLine = line;
	return *obj;
}

static bool csvReaderEnumeratorMoveNext ( vmInstance *instance, VAR_OBJ *obj )
{
	auto enumerator = obj->getObj<csvReaderEnumerator> ( );
	auto readerVar = (*obj)["reader"];
	auto reader = static_cast<VAR_OBJ *>(readerVar)->getObj<csvReader> ( );

	enumerator->currentLine++;
	if ( enumerator->currentLine < (int64_t) reader->lineInfo.size() )
	{
		return true;
	}
	return false;
}

static VAR csvLineAccess ( vmInstance *instance, VAR_OBJ *obj, VAR *field )
{
	auto readerVar = (*obj)["reader"];
	auto reader = static_cast<VAR_OBJ *>(readerVar)->getObj<csvReader> ( );

	auto line = obj->getObj<csvLine> ( );

	size_t column;

	switch ( field->type )
	{
		case slangType::eLONG:
			column = field->dat.l;
			break;
		case slangType::eSTRING:
			{
				auto it = reader->nameToColumn.find ( stringi ( field->dat.str.c ) );
				if ( it == reader->nameToColumn.end () )
				{
					throw errorNum::scINVALID_INDICE;
				}
				column = it->second;
			}
			break;
		default:
			throw errorNum::scINVALID_INDICE;
	}

	auto ptr = line->lineStart;
	while ( (ptr < line->lineStart + line->lineLen) )
	{
		if ( !column )
		{
			// skip whitespace
			while ( (ptr < line->lineStart + line->lineLen) && _isspace ( ptr ) ) ptr++;
			char const *valueStart = ptr;
			char const *valueEnd;
			if ( ptr[0] == '\"' )
			{
				ptr++;
				valueStart = ptr;
				while ( (ptr < line->lineStart + line->lineLen) )
				{
					if ( (ptr + 1 < line->lineStart + line->lineLen) && (ptr[0] != '\"') && (ptr[1] != '\"') )
					{
						ptr += 2;
					} else if ( ptr[0] == '\"' )
					{
						break;
					} else
					{
						ptr++;
					}
				}
				valueEnd = ptr;
				ptr++;
				while ( (ptr < line->lineStart + line->lineLen) && (ptr[0] != ',') ) ptr++;
			} else
			{
				while ( (ptr < line->lineStart + line->lineLen) && (ptr[0] != ',') ) ptr++;
				valueEnd = ptr;
				ptr++;
			}
			return VAR_STR ( instance, valueStart, valueEnd - valueStart );
		} else
		{
			// skip whitespace
			while ( (ptr < line->lineStart + line->lineLen) && _isspace ( ptr ) ) ptr++;
			if ( ptr[0] == '\"' )
			{
				ptr++;
				while ( (ptr < line->lineStart + line->lineLen) )
				{
					if ( (ptr + 1 < line->lineStart + line->lineLen) && (ptr[0] != '\"') && (ptr[1] != '\"') )
					{
						ptr += 2;
					} else if ( ptr[0] == '\"' )
					{
						break;
					} else
					{
						ptr++;
					}
				}
				ptr++;
				while ( (ptr < line->lineStart + line->lineLen) && (ptr[0] != ',') ) ptr++;
			} else
			{
				while ( (ptr < line->lineStart + line->lineLen) && (ptr[0] != ',') ) ptr++;
			}
			if ( ptr < line->lineStart + line->lineLen ) ptr++;
			column--;
		}
	}
	throw errorNum::scINVALID_PARAMETER;
}

static void csvReaderEnumeratorGcCb ( class vmInstance *instance, VAR_OBJ *obj, objectMemory::collectionVariables *col )
{
	auto enumerator = obj->getObj<csvReaderEnumerator> ( );
	if ( col->doCopy ( enumerator ) )
	{
		obj->makeObj<csvReaderEnumerator> ( col->getAge ( ), instance, std::move(*enumerator) );
	}
}

static void csvReaderEnumeratorCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	auto enumerator = reinterpret_cast<VAR_OBJ*>(val)->getObj<csvReaderEnumerator> ( );
	reinterpret_cast<VAR_OBJ *>(val)->makeObj<csvReaderEnumerator> ( instance, *enumerator );
}

static void csvLineGcCb ( class vmInstance *instance, VAR_OBJ *obj, objectMemory::collectionVariables *col )
{
	auto line = obj->getObj<csvLine> ( );
	if ( col->doCopy ( line ) )
	{
		obj->makeObj<csvLine> ( col->getAge ( ), instance, *line );
	}
}

static void csvLineCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	auto line = reinterpret_cast<VAR_OBJ *>(val)->getObj<csvLine> ( );
	reinterpret_cast<VAR_OBJ *>(val)->makeObj<csvLine> ( instance, *line );
}

static void csvReaderGcCb ( class vmInstance *instance, VAR_OBJ *obj, objectMemory::collectionVariables *col )
{
	auto reader = obj->getObj<csvReader> ( );
	if ( col->doCopy ( reader ) )
	{
		obj->makeObj<csvReader> ( col->getAge ( ), instance, std::move ( *reader ) );
	}
}

static void csvReaderCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	auto enumerator = reinterpret_cast<VAR_OBJ *>(val)->getObj<csvReader> ( );
	reinterpret_cast<VAR_OBJ *>(val)->makeObj<csvReader> ( instance, *enumerator );
}

struct reader
{
	int				 offset = 0;
	fileCacheEntry	*entry;
	bool			 isError = false;
	char const		*buff;
	size_t			 buffOffset = 0;
	size_t			 nRead = 0;
	bool			 isEof = false;

	reader ( char *fName )
	{
		if ( !(entry = globalFileCache.read ( fName )) )
		{
			printf ( "error: can't open file %s with %lu\r\n", fName, GetLastError());
			isError = true;
			throw GetLastError();
		}

		buff  = reinterpret_cast<char const *>(entry->getData());
		nRead = entry->getDataLen ();

		if ( nRead >= 3 )
		{
			if (!memcmp ( buff, "\xEF\xBB\xBF", 3 ))
			{
				buffOffset += 3;
				offset += 3;
			}
		}
	}
	~reader ()
	{
		entry->free ();
	}

	operator char ()
	{
		return buff[buffOffset];
	}

	reader &operator ++()
	{
		if ( isEof )
		{
			return *this;
		}
		if ( buffOffset == nRead )
		{
			isEof = true;
			return *this;
		}
		buffOffset++;
		offset++;
		return *this;
	}

	int64_t getFileOffset ()
	{
		return offset;
	}
};

static VAR parseCSVFile ( vmInstance *instance, char *fName )
{
	reader r ( fName );

	if ( r.isError )
	{
		throw GetLastError ();
	}

	VAR			arr = VAR_ARRAY ( instance );
	int64_t		lineNum = 1;
	int64_t		lineStart = r.getFileOffset ();

	while ( !r.isEof )
	{
		if ( r == '\r' )
		{
			++r;
			if ( r == '\n' )
			{
				++r;
				arrayGet ( instance, arr, lineNum, 1 ) = lineStart;
				arrayGet ( instance, arr, lineNum, 2 ) = r.getFileOffset () - lineStart - 2;
			} else
			{
				arrayGet ( instance, arr, lineNum, 1 ) = lineStart;
				arrayGet ( instance, arr, lineNum, 2 ) = r.getFileOffset () - lineStart - 1;
			}
			lineNum++;
			lineStart = r.getFileOffset();
		} else if ( r == '\n' )
		{
			++r;
			arrayGet ( instance, arr, lineNum, 1 ) = lineStart;
			arrayGet ( instance, arr, lineNum, 2 ) = r.getFileOffset () - lineStart - 2;
			lineStart = r.getFileOffset();
			lineNum++;
		} else if ( r == '\"' )
		{
			++r;
			while ( !r.isEof )
			{
				if ( r == '\"' )
				{
					++r;
					if ( r == '\"' )
					{
						++r;
					} else
					{
						break;
					}
				} else
				{
					++r;
				}
			}
		} else
		{
			++r;
		}
	}
	if ( r.getFileOffset () != lineStart )
	{
		arrayGet ( instance, arr, lineNum, 1 ) = lineStart ;
		arrayGet ( instance, arr, lineNum, 2 ) = r.getFileOffset () - lineStart;
	}
	return arr;
}

static VAR parseTSVFile ( vmInstance *instance, char *fName, int fieldCount )
{
	reader r ( fName );

	if ( r.isError )
	{
		throw GetLastError ();
	}

	VAR			arr = VAR_ARRAY ( instance );
	int64_t		lineNum = 1;
	int64_t		lineStart = r.getFileOffset ();

	auto fieldsSeen = fieldCount - 1;

	while ( !r.isEof )
	{
		if ( r == (char)0x95 )
		{
			++r;
			if ( r == 0x09 )
			{
				++r;
			}
		} else if ( r == 0x09 )
		{
			++r;
			fieldsSeen--;
		} else  if ( !fieldsSeen && r == '\r' )
		{
			fieldsSeen = fieldCount - 1;
			++r;
			if ( r == '\n' )
			{
				++r;
				arrayGet ( instance, arr, lineNum, 1 ) = VAR ( lineStart );
				arrayGet ( instance, arr, lineNum, 2 ) = VAR ( r.getFileOffset () - lineStart - 2 );
			} else
			{
				arrayGet ( instance, arr, lineNum, 1 ) = VAR ( lineStart );
				arrayGet ( instance, arr, lineNum, 2 ) = VAR ( r.getFileOffset () - lineStart - 1 );
			}
			lineNum++;
			lineStart = r.getFileOffset();
		} else if ( !fieldsSeen && r == '\n' )
		{
			fieldsSeen = fieldCount - 1;
			++r;
			arrayGet ( instance, arr, lineNum, 1 ) = VAR ( lineStart );
			arrayGet ( instance, arr, lineNum, 2 ) = VAR ( r.getFileOffset () - lineStart - 2 );
			lineStart = r.getFileOffset();
			lineNum++;
		} else if ( r == '\"' )
		{
			++r;
			while ( !r.isEof )
			{
				if ( r == '\"' )
				{
					++r;
					if ( r == '\"' )
					{
						++r;
					} else
					{
						break;
					}

				} else
				{
					++r;
				}
			}
		} else
		{
			++r;
		}
	}
	if ( r.getFileOffset () != lineStart )
	{
		arrayGet ( instance, arr, lineNum, 1 ) = VAR ( lineStart );
		arrayGet ( instance, arr, lineNum, 2 ) = VAR ( r.getFileOffset () - lineStart );
	}
	return arr;
}

static VAR parseTSVLine ( vmInstance *instance, VAR_STR *lineP, bool alwasyString, VAR_STR *sep )
{
	VAR		arr = VAR_ARRAY ( instance );
	int		arrayIndex = 1;
	auto	ptr = lineP->dat.str.c;
	char const *valueStart;
	auto	valueEnd = ptr;

	if ( sep->dat.str.len != 1 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	while ( *ptr )
	{
		// skip whitespace
		bool isString = alwasyString;
		bool isDecimal = false;
		bool isNull = false;

		valueStart = ptr;
		if ( ptr[0] == '\"' )
		{
			ptr++;
			valueStart = ptr;
			valueEnd = ptr;
			while ( *ptr )
			{
				if ( (ptr[0] == '\"') && (ptr[1] == '\"') )
				{
					ptr += 2;
				} else if ( ptr[0] == '\"' )
				{
					valueEnd = ptr - 1;
					ptr++;
					break;
				} else
				{
					ptr++;
				}
				valueEnd = ptr;
			}
			isString = true;
			while ( *ptr && (ptr[0] != sep->dat.str.c[0]) ) ptr++;
			if ( *ptr ) ptr++;
		} else if ( *ptr == sep->dat.str.c[0] )
		{
			isNull = true;
			ptr++;
		} else
		{
			while ( *ptr && (ptr[0] != sep->dat.str.c[0]) )
			{
				if ( (!_isnum ( ptr ) && (valueStart == ptr)) || _isdigit(ptr) )
				{
					isString = true;
				}
				if ( *ptr == '.' )
				{
					isDecimal = true;
				}
				valueEnd = ptr;
				ptr++;
			}
			if ( *ptr ) ptr++;
		}

		if ( isNull )
		{
			arrayGet ( instance, arr, arrayIndex++ ) = VAR_STR ( "", 0 );
		} else if ( isString )
		{
			arrayGet ( instance, arr, arrayIndex++ ) = VAR_STR ( instance, valueStart, valueEnd - valueStart + 1);
		} else if ( isDecimal )
		{
			arrayGet ( instance, arr, arrayIndex++ ) = VAR ( std::stod ( std::string ( valueStart, valueEnd - valueStart + 1 ) ) );
		} else
		{
			arrayGet ( instance, arr, arrayIndex++ ) = VAR ( std::stoi ( std::string ( valueStart, valueEnd - valueStart + 1 ) ) );
		}
	}
	return arr;
}

void builtinCSV ( class vmInstance *instance, opFile *file )
{
	// NOTE: MUST use ; to end expressions as \r\n will be stripped out
	// NOTE: because of the above, method initializers will not work as there is no way to determine EOL
	
	REGISTER( instance, file );
		CLASS ( "csvReader" );
			METHOD ( "new", csvReaderNew );
			METHOD ( "release", csvReaderRelease );
			METHOD ( "getEnumerator", csvReaderGetEnumerator );
			GCCB ( csvReaderGcCb, csvReaderCopyCb );
		END;

		CLASS ( "csvReaderEnumerator" );
			INHERIT ( "Queryable" );
			METHOD ( "getEnumerator", csvReaderEnumeratorGetEnumerator );
			METHOD ( "moveNext", csvReaderEnumeratorMoveNext );
			ACCESS ( "current", csvReaderEnumeratorCurrent ) CONST;
			ACCESS ( "index", csvReaderEnumeratorIndex ) CONST;

			// overrides of Queryable methods
			METHOD ( "skip", csvReaderEnumeratorSkip );
			PRIVATE
				IVAR ( "reader" );
			GCCB ( csvReaderEnumeratorGcCb, csvReaderEnumeratorCopyCb );
		END;

		CLASS ( "csvLine" );
			ACCESS( "default", csvLineAccess );
			OP ( "[", csvLineAccess );
			PRIVATE
				IVAR ( "reader" );
			GCCB ( csvLineGcCb, csvLineCopyCb );
		END;

		FUNC ( "parseCSVFile", parseCSVFile );
		FUNC ( "parseTSVFile", parseTSVFile );

		FUNC ( "parseCSVLine", parseTSVLine, DEF ( 2, "false" ), DEF ( 3, "\",\"" ) );
		FUNC ( "parseSSVLine", parseTSVLine, DEF ( 2, "false" ), DEF ( 3, "\";\"" ) );
		FUNC ( "parseTSVLine", parseTSVLine, DEF ( 2, "false" ), DEF ( 3, "\"\t\"" ) );

	END;
}

