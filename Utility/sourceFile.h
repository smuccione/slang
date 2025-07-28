/*

	block parser definitions

*/

#pragma once

#include "Utility/settings.h"

#include <vector>
#include <filesystem>
#include <tuple>
#include <stdint.h>
#include "Utility/buffer.h"
#include "Utility/stringi.h"
#include "Utility/stringCache.h"

#define BS_ADVANCE(src) src.bsAdvance();
#define ADVANCE_EOL(src) src.advanceEOL();

class sourceFile
{
	public:
	enum class sourceFileType : uint8_t
	{
		none = 0,		// (INTERNAL)
		internal,	// no source file available
		external	// external source file
	};
	private:
	std::vector<std::pair<stringi, sourceFileType>>			 sourceFiles;
	std::vector<size_t>				 mapping;

	public:

	uint32_t						 lastIndex = 0;

	sourceFile () noexcept
	{
		getIndex ( "(INTERNAL)", sourceFileType::none );		// 0th entry is always internal (no source available)
	}

	sourceFile ( char const **expr )
	{
		auto num = *(size_t *)*expr;
		(*expr) += sizeof ( size_t );

		while ( num-- )
		{
			auto fName = stringi ( expr );
			auto fileType = (sourceFileType)(*(uint8_t *)(*expr));
			(*expr) += sizeof ( sourceFileType );
			sourceFiles.emplace_back ( std::make_pair (fName, fileType) );
		}
	}
	sourceFile ( sourceFile const &right ) = delete;
	sourceFile ( sourceFile &&right ) noexcept
	{
		*this = std::move ( right );
	}
	sourceFile &operator = ( sourceFile const &right ) = delete;
	sourceFile &operator = ( sourceFile &&right ) noexcept
	{
		std::swap ( sourceFiles, right.sourceFiles );
		std::swap ( mapping, right.mapping );
		return *this;
	}

	~sourceFile ()
	{}

	auto getFileType ( uint32_t index ) const noexcept
	{
		if ( index >= sourceFiles.size () )
		{
			return sourceFileType::none;
		}
		return sourceFiles[index].second;
	}

	void serializeCompiled ( BUFFER *buffer )
	{
		// number of source files
		buffer->put ( sourceFiles.size () );

		uint32_t		nameSize = 0;
		for ( auto it = sourceFiles.begin (); it != sourceFiles.end (); it++ )
		{
			nameSize += (uint32_t)it->first.size () + 1;
			nameSize += sizeof ( it->second );
		}

		// size of the name region
		bufferPut32 ( buffer, nameSize );

		// name region
		for ( auto it = sourceFiles.begin (); it != sourceFiles.end (); it++ )
		{
			it->first.serialize ( buffer );
			buffer->put<sourceFileType> ( it->second );
		}

		// offset array
		nameSize = 0;
		for ( auto it = sourceFiles.begin (); it != sourceFiles.end (); it++ )
		{
			bufferPut32 ( buffer, nameSize );
			nameSize += (uint32_t)it->first.size () + 1;
			nameSize += sizeof ( it->second );
		}
	}

	void serialize ( BUFFER *buffer )
	{
		buffer->put ( sourceFiles.size () );
		for ( auto it = sourceFiles.begin (); it != sourceFiles.end (); it++ )
		{
			it->first.serialize ( buffer );
			buffer->put<sourceFileType> ( it->second );
		}
	}

	void addMapping ( char const **expr )
	{
		auto num = *((size_t *)*expr);
		(*expr) += sizeof ( size_t );

		while ( num-- )
		{
			auto fName = stringi ( expr );
			auto fileType = (sourceFileType)(*(uint8_t *)(*expr));
			(*expr) += sizeof ( sourceFileType );

			size_t loop;
			for ( loop = 0; loop < sourceFiles.size (); loop++ )
			{
				if ( sourceFiles[loop].first == fName )
				{
					mapping.push_back ( loop );
					break;
				}
			}
			if ( loop >= sourceFiles.size () )
			{
				mapping.push_back ( sourceFiles.size () );
				sourceFiles.push_back ( std::make_pair ( fName, fileType ) );
			}
		}
	}

	void freeMapping ( void ) noexcept
	{
		mapping.clear ();
	}

	sourceFileType getFileType ( char const *fName )
	{
		stringi file;

		if ( !strccmp ( fName, "(INTERNAL)" ) )
		{
			file = fName;
		} else
		{
			std::filesystem::path p ( fName );
			auto abs = std::filesystem::absolute ( p );
			file = stringi ( abs.generic_string () );
		}

		uint32_t	loop;

		for ( loop = 0; loop < sourceFiles.size (); loop++ )
		{
			if ( sourceFiles[loop].first == file )
			{
				return sourceFiles[loop].second;
			}
		}
		return sourceFileType::none;
	}

	sourceFileType getFileType (uint32_t index )
	{
		return sourceFiles[index].second;
	}

	uint32_t getIndex ( char const *fName, sourceFileType type )
	{
		stringi file;
		if ( !strccmp ( fName, "(INTERNAL)" ) )
		{
			file = fName;
		} else
		{
			std::filesystem::path p ( fName );
			auto abs = std::filesystem::absolute ( p );
			file = stringi ( abs.generic_string () );
		}

		uint32_t	loop;

		for ( loop = 0; loop < sourceFiles.size (); loop++ )
		{
			if ( sourceFiles[loop].first == file )
			{
				lastIndex = loop;
				return loop;
			}
		}
		sourceFiles.push_back ( {file, type} );
		lastIndex = (uint32_t)sourceFiles.size () - 1;
		return lastIndex;
	}

	uint32_t getStaticIndex ( char const *fName )
	{
		stringi file;
		if ( !strccmp ( fName, "(INTERNAL)" ) )
		{
			file = fName;
		} else
		{
			std::filesystem::path p ( fName );
			auto abs = std::filesystem::absolute ( p );
			file = stringi ( abs.generic_string () );
		}

		for ( uint32_t loop = 0; loop < sourceFiles.size (); loop++ )
		{
			if ( sourceFiles[loop].first == file )
			{
				lastIndex = loop;
				return loop;
			}
		}
		return -1;

	}

	uint32_t getMappedIndex ( uint32_t mapIndex ) noexcept
	{
		return (uint32_t)mapping[mapIndex];
	}

	stringi const &getName ( uint32_t index ) noexcept
	{
		return sourceFiles[index].first;
	}

	size_t getCount ( void ) noexcept
	{
		return sourceFiles.size ();
	}
};

class source
{
	class sourceFile	*srcFile = nullptr;
	char const			*expr = nullptr;
	uint32_t			 lineNumber = 0;
	uint32_t			 columnNumber = 0;
	uint32_t			 prevLineEnd = 0;
	char const			*lineStartPrimary = nullptr;
	char const			*lineStartInclude = nullptr;
	cacheString			 currentFileName;
	uint32_t			 currentSourceIndex = 0;
	bool				 lineStart = true;
	bool				 needDebugEmit = false;
	stringCache			&sCache;

public:
	source ( sourceFile *srcFile, stringCache &sCache, char const *primaryFileName, char const *expr, sourceFile::sourceFileType type, uint32_t lineNumber = 1 );
	virtual ~source ( )
	{}

	source ( const source &old ) noexcept : srcFile ( old.srcFile ), sCache ( old.sCache )
	{
		expr = old.expr;
		lineNumber = old.lineNumber;
		columnNumber = old.columnNumber;
		lineStartPrimary = old.lineStartPrimary;
		lineStartInclude = old.lineStartInclude;
		currentFileName = old.currentFileName;
		currentFileName = old.currentFileName;
		currentSourceIndex = old.currentSourceIndex;
		prevLineEnd = old.prevLineEnd;
	}
	source ( source &&old ) noexcept : srcFile ( old.srcFile ), sCache ( old.sCache )
	{
		*this = std::move ( old );
	}

	source &operator = ( source const &old ) noexcept
	{
		return *this = source ( old );
	}

	source &operator = ( source &&old ) noexcept
	{
		std::swap ( expr, old.expr );
		std::swap ( lineNumber, old.lineNumber );
		std::swap ( columnNumber, old.columnNumber );
		std::swap ( lineStartPrimary, old.lineStartPrimary );
		std::swap ( lineStartInclude, old.lineStartInclude );
		std::swap ( currentFileName, old.currentFileName );
		std::swap ( currentFileName, old.currentFileName );
		std::swap ( currentSourceIndex, old.currentSourceIndex );
		std::swap ( prevLineEnd, old.prevLineEnd );
		std::swap( sCache, old.sCache );
		return *this;
	}

	bool emitDebug ()
	{
		bool ret = needDebugEmit;
		needDebugEmit = false;
		return ret;
	}

	bool isLineStart ()
	{
		return lineStart;
	}
	operator char const *() noexcept;

	char const *operator ++ ( ) noexcept
	{
		columnNumber++;
		prevLineEnd = columnNumber;
		if ( *expr && expr[0] == '\r' && expr[1] == '\n' )
		{
		} else if ( expr[0] == '\r' || expr[0] == '\n' )
		{
			if ( expr[1] )
			{
				lineNumber++;
			}
			columnNumber = 1;
		}
		return ++expr;
	}
	char const *operator ++ ( int dummy ) noexcept
	{
		columnNumber++;
		prevLineEnd = columnNumber;
		lineStart = false;
		if ( *expr && expr[0] == '\r' && expr[1] == '\n' )
		{
		} else if ( expr[0] == '\r' || expr[0] == '\n' )
		{
			if ( expr[1] )
			{
				lineNumber++;
			}
			columnNumber = 1;
			lineStart = true;
		}
		return expr++;
	}

	char const *operator += ( size_t inc ) noexcept
	{
		while ( inc-- )
		{
			(*this)++;
		}
		return expr;
	}

	char const *operator -- ( int dummy ) noexcept
	{
		columnNumber--;
		auto ret = expr--;
		if ( expr[0] == '\r' && expr[1] == '\n' )
		{
		} else if ( expr[0] == '\r' || expr[0] == '\n' )
		{
			if ( expr[1] )
			{
				lineNumber--;
			}
			columnNumber = prevLineEnd;
		}
		return ret;
	}

	char const *operator -- () noexcept
	{
		columnNumber--;
		auto ret = --expr;
		if ( expr[0] == '\r' && expr[1] == '\n' )
		{
		} else if ( expr[0] == '\r' || expr[0] == '\n' )
		{
			if ( expr[1] )
			{
				lineNumber--;
			}
			columnNumber = prevLineEnd;
		}
		return ret;
	}

	char const *operator -= ( size_t inc ) noexcept
	{
		while ( inc-- )
		{
			(*this)--;
		}
		return expr;
	}

	uint32_t getSourceIndex ( ) const noexcept
	{
		return currentSourceIndex;
	}

	uint32_t getLine ( ) const noexcept
	{
		return lineNumber;
	}

	uint32_t getColumn ( ) const noexcept
	{
		return columnNumber;
	}
	char const *getFilename ( ) const noexcept
	{
		return currentFileName;
	}

	void advanceEOL ( )
	{
		// used for error recovery...  this advances us to the start of an EOL marker, skipping over all remainig characters on the line;
		while ( *expr && !_iseol ( expr ) )
		{
			expr++;
			columnNumber++;
		}
	}
	bool advanceGarbage ( char const stop = 0)
	{	
		if ( *expr && !_iseol ( expr ) )
		{
			expr++;
			columnNumber++;
		}
		while ( *expr && !_isspace ( expr ) && !_iseol ( expr ) && (*expr != ';') )
		{
			expr++;
			columnNumber++;
			if ( *expr == stop )
			{
				expr++;
				columnNumber++;
				lineStart = true;
				return true;
			}
		}
		return false;
	}
	bool advanceGarbage ( char const *stop)
	{	
		auto len = strlen ( stop );
		if ( *expr && !_iseol ( expr ) )
		{
			expr++;
			columnNumber++;
		}
		while ( *expr && !_isspace ( expr ) && !_iseol ( expr ) && (*expr != ';') )
		{
			expr++;
			columnNumber++;
			if ( !memcmpi ( expr, stop, len ) )
			{
				expr += len;
				columnNumber += (uint32_t)len;
				lineStart = true;
				return true;
			}
		}
		return false;
	}

	void bsAdvance ();
	void bsAdvanceEol ( bool noSemi );
};

struct srcLocation
{
	uint32_t				 sourceIndex = 0;			// index into list of source names
	uint32_t				 columnNumber = 1;
	uint32_t				 columnNumberEnd = 1;
	uint32_t				 lineNumberStart = 1;		// line number for start of block;
	uint32_t				 lineNumberEnd = 1;			// line number for start of block;

	srcLocation ( class sourceFile *srcFile, char const **expr );
	srcLocation ()
	{}

	srcLocation ( uint32_t sourceIndex, uint32_t columnNumber, uint32_t lineNumberStart, uint32_t lineNumberEnd ) : sourceIndex ( sourceIndex ), 
																													columnNumber ( columnNumber),
																													columnNumberEnd ( columnNumber ),
																													lineNumberStart ( lineNumberStart ), 
																													lineNumberEnd ( lineNumberEnd )
	{}
	srcLocation ( uint32_t sourceIndex, uint32_t columnNumber, uint32_t lineNumberStart, uint32_t columnNumberEnd, uint32_t lineNumberEnd ) :	sourceIndex ( sourceIndex ),
																																				columnNumber ( columnNumber),
																																				columnNumberEnd ( columnNumberEnd ),
																																				lineNumberStart ( lineNumberStart ), 
																																				lineNumberEnd ( lineNumberEnd )
	{}
	srcLocation ( source &src )
	{
		set ( src );
	}
	srcLocation( srcLocation const& old ) noexcept
	{
		sourceIndex = old.sourceIndex;
		columnNumber = old.columnNumber;
		columnNumberEnd = old.columnNumberEnd;
		lineNumberStart = old.lineNumberStart;
		lineNumberEnd = old.lineNumberEnd;
	}
	srcLocation ( srcLocation &&old ) noexcept
	{
		*this = std::move ( old );
	}
	srcLocation &operator = ( srcLocation &&old ) noexcept
	{
		std::swap ( sourceIndex, old.sourceIndex );
		std::swap( columnNumber, old.columnNumber );
		std::swap( columnNumberEnd, old.columnNumberEnd );
		std::swap( lineNumberStart, old.lineNumberStart);
		std::swap( lineNumberEnd, old.lineNumberEnd);
		return *this;
	}
	bool operator == ( srcLocation const &right ) const noexcept
	{
		if ( sourceIndex != right.sourceIndex ) return false;
		if ( columnNumber != right.columnNumber ) return false;
		if ( columnNumberEnd != right.columnNumberEnd ) return false;
		if ( lineNumberStart != right.lineNumberStart ) return false;
		if ( lineNumberEnd != right.lineNumberEnd ) return false;
		return true;
	}

	bool operator < ( srcLocation const &right ) const noexcept;

	srcLocation &operator = ( srcLocation const &right ) noexcept
	{
		return *this = srcLocation ( right );
	}
	void serialize ( BUFFER *buff );
	void set ( source const &src ) noexcept;
	void setEnd ( source const &src ) noexcept;
	void setEnd( srcLocation const &src ) noexcept;

	bool empty () const noexcept
	{
		return lineNumberStart == lineNumberEnd && columnNumber == columnNumberEnd;
	}

	bool encloses( int64_t column, int64_t line ) const
	{
		if ( line < lineNumberStart ) return false;
		if ( line == lineNumberStart )
		{
			if ( column < columnNumber ) return false;
		}
		if ( line > lineNumberEnd ) return false;
		if ( line == lineNumberEnd )
		{
			if ( column > columnNumberEnd ) return false;
		}
		return true;
	}
	bool encloses ( srcLocation const &target ) const
	{
		if ( sourceIndex != target.sourceIndex ) return false;
		if ( target.lineNumberEnd < lineNumberStart ) return false;
		if ( target.lineNumberStart > lineNumberEnd ) return false;

		if ( target.lineNumberStart == lineNumberStart )
		{
			if ( target.columnNumber < columnNumber ) return false;
		}
		if ( target.lineNumberEnd == lineNumberEnd )
		{
			if ( target.columnNumberEnd > columnNumberEnd ) return false;
		}
		return true;
	}
};

