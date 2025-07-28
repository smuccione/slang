/*

	astNode

*/

#include "Utility/settings.h"
#include "Utility/sourceFile.h"

source::source ( sourceFile *srcFile, stringCache &sCache, char const *primaryFileName, char const *expr, sourceFile::sourceFileType type, uint32_t lineNumber ) : 
	srcFile ( srcFile ),
	expr ( expr ), 
	lineNumber ( lineNumber ), 
	columnNumber ( 1 ),
	lineStartPrimary ( expr ),
	lineStartInclude ( expr ),
	currentFileName ( sCache.get ( primaryFileName ) ),
	sCache( sCache )
{
	currentSourceIndex = srcFile->getIndex ( primaryFileName, type );
	currentFileName = sCache.get( primaryFileName );
}

source::operator char const *() noexcept
{
	auto tmpExpr = expr;
	while ( (tmpExpr[0] == '[') && !memcmp( tmpExpr, "[[pos:", 6 ) )
	{
		needDebugEmit = true;
		while ( *tmpExpr && !(tmpExpr[0] == ']' && tmpExpr[1] == ']') )
		{
			tmpExpr++;
		}
		if ( *tmpExpr && tmpExpr[1] )
		{
			tmpExpr += 2;
		}
	}
	return tmpExpr;
}

void source::bsAdvance ()
{
	while ( 1 )
	{
		if ( (expr[0] == '[') && !memcmp ( expr, "[[pos:", 6 ) )
		{
			needDebugEmit = true;
			expr += 6;
			while ( _isspace ( expr ) ) expr++;
			columnNumber = atoi ( expr );  // - 1 because we'll increment it again the moment we process OUR eol... the line number in the #line statement is for the line below us
			while ( _isnum ( expr ) ) expr++;
			while ( _isspace ( expr ) ) expr++;
			lineNumber = atoi ( expr );  // - 1 because we'll increment it again the moment we process OUR eol... the line number in the #line statement is for the line below us
			while ( _isnum ( expr ) ) expr++;
			while ( _isspace ( expr ) ) expr++;
			stringi name;
			while ( *expr && !(expr[0] == ']' && expr[1] == ']') )
			{
				name += *expr;
				expr++;
			}
			if ( name.size () )
			{
				currentFileName = sCache.get( name );
				currentSourceIndex = srcFile->getStaticIndex ( currentFileName.c_str() );
			}
			if ( *expr )
			{
				expr += 2;
			}
		} else if ( _isspace ( expr ) )
		{
			expr++;
			columnNumber++;
		} else
		{
			break;
		}
	}
}

void source::bsAdvanceEol ( bool noSemi )
{
	do
	{
		while ( _isspace ( expr ) || (!noSemi && (*expr == ';')) )
		{
			expr++;
			columnNumber++;
		}

		if ( _iseol ( expr ) )
		{
			columnNumber = 1;
			if ( expr[0] == '\r' && expr[1] == '\n' )
			{
				expr++;
			}
			if ( expr[0] == '\r' || expr[0] == '\n' )
			{
				expr++;

//				if ( expr[0] )
				{
					lineNumber++;
				}

				while ( _isspace ( expr ) )
				{
					expr++;
					columnNumber++;
				}
				lineStart = true;
			}
		} else if ( (expr[0] == '[') && !memcmp( expr, "[[pos:", 6 ) )
		{
			needDebugEmit = true;
			expr += 6;
			while ( _isspace ( expr ) ) expr++;
			columnNumber= atoi ( expr );  // - 1 because we'll increment it again the moment we process OUR eol... the line number in the #line statement is for the line below us
			while ( _isnum ( expr ) ) expr++;
			while ( _isspace ( expr ) ) expr++;
			lineNumber = atoi ( expr );  // - 1 because we'll increment it again the moment we process OUR eol... the line number in the #line statement is for the line below us
			while ( _isnum ( expr ) ) expr++;
			while ( _isspace ( expr ) ) expr++;
			stringi name;
			while ( *expr && !(expr[0] == ']' && expr[1] == ']') )
			{
				name += *expr;
				expr++;
			}
			if ( name.size () )
			{
				currentFileName = sCache.get( name );
				currentSourceIndex = srcFile->getStaticIndex ( currentFileName.c_str() );
			}
			if ( *expr )
			{
				expr += 2;
			}
		}
	} while ( (_isspace ( expr ) || _iseol ( expr )) || (expr[0] == '[' && expr[1] == '[') );
}

void srcLocation::set ( source const &src ) noexcept
{
	sourceIndex = src.getSourceIndex ( );
	lineNumberStart = src.getLine();
	lineNumberEnd = src.getLine ( );
	columnNumber = src.getColumn ( );
	columnNumberEnd = src.getColumn ( ) + 1;

	if ( columnNumberEnd < columnNumber ) columnNumberEnd = columnNumber;
}

void srcLocation::setEnd ( source const  &src ) noexcept
{
	columnNumberEnd = src.getColumn ();
	lineNumberEnd = src.getLine( );
	if ( columnNumberEnd < columnNumber && lineNumberEnd == lineNumberStart ) columnNumberEnd = columnNumber;
}

void srcLocation::setEnd ( srcLocation const  &src ) noexcept
{
	columnNumberEnd = src.columnNumberEnd;
	lineNumberEnd = src.lineNumberEnd;
	if ( columnNumberEnd < columnNumber && lineNumberEnd == lineNumberStart ) columnNumberEnd = columnNumber;
}

srcLocation::srcLocation ( class sourceFile *srcFile, char const **expr )
{
	sourceIndex = *(( uint32_t*) * expr);
	(*expr) += sizeof ( uint32_t );
	sourceIndex = srcFile->getMappedIndex ( sourceIndex );

	lineNumberStart = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );
	lineNumberEnd = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );
	columnNumber = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );
	columnNumberEnd = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );
	if ( columnNumberEnd < columnNumber ) columnNumberEnd = columnNumber;
}

void srcLocation::serialize ( BUFFER *buff )
{
	buff->put<uint32_t> ( sourceIndex );
	buff->put<uint32_t> ( lineNumberStart );
	buff->put<uint32_t> ( lineNumberEnd );
	buff->put<uint32_t> ( columnNumber );
	buff->put<uint32_t> ( columnNumberEnd );
}


bool srcLocation::operator < ( srcLocation const &right ) const noexcept
{
	if ( sourceIndex < right.sourceIndex ) return true;
	if ( sourceIndex == right.sourceIndex )
	{
		if ( lineNumberStart < right.lineNumberStart )
		{
			return true;
		}
		if ( lineNumberStart == right.lineNumberStart && columnNumber < right.columnNumber )
		{
			return true;
		}
		if ( lineNumberStart == right.lineNumberStart && columnNumber == right.columnNumber && lineNumberEnd < right.lineNumberEnd )
		{
			return true;
		}
		if ( lineNumberStart == right.lineNumberStart && columnNumber == right.columnNumber && lineNumberEnd == right.lineNumberEnd && columnNumberEnd < right.columnNumberEnd )
		{
			return true;
		}
	}
	return false;
}
