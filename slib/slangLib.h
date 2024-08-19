#include <process.h>
#include <io.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include <string>

#include <sys/stat.h>
#include <fcntl.h>
#include "Utility/util.h"
#include "Utility/buffer.h"

struct libEntry {
	stringi		  fName;
	uint8_t		 *data = nullptr;
	size_t		  dataLen = 0;

	libEntry()
	{
	};

	libEntry ( const struct libEntry &old )
	{
		fName = old.fName;
		dataLen = old.dataLen;
		data = (uint8_t *)malloc ( sizeof ( uint8_t ) * dataLen );
		memcpy ( data, old.data, dataLen );
	}

	~libEntry()
	{
		free ( data );
	}

	operator uint8_t *()
	{
		return data;
	}
	operator size_t ( )
	{
		return dataLen;
	}
};

class libStore {
	stringi						fName;
	std::vector<libEntry *>		entries;

public:
	typedef	decltype(entries)::iterator iterator;

	iterator begin ( )
	{
		return entries.begin ( );
	}
	iterator end ( )
	{
		return entries.end ( );
	}

	libStore ( stringi const &fName, bool del = false )
	{
		FILE			*file;
		libEntry		*entry;
		char			 nameStore[MAX_PATH+1];

		this->fName = fName;

		if ( !del )
		{
			if ( !fopen_s ( &file, fName.c_str ( ), "rb" ) )
			{
				while ( !feof ( file ) )
				{
					auto x = fread ( nameStore, sizeof ( char ), sizeof ( nameStore ), file );
					if ( x < sizeof ( nameStore ) * 1 )
					{
						break;
					}
					entry = new libEntry ( );
					entry->fName = nameStore;
					fread ( &entry->dataLen, sizeof ( entry->dataLen ), 1, file );
					entry->data = (uint8_t *) malloc ( sizeof ( uint8_t ) * entry->dataLen );
					fread ( entry->data, sizeof ( uint8_t ), entry->dataLen, file );

					entries.push_back ( entry );
				}
				fclose ( file );
			}
		}
	}

	libStore ( uint8_t const *data, size_t dataLen )
	{
		char			 nameStore[MAX_PATH + 1];
		uint8_t const *dataEnd = data + dataLen;

		while ( data < dataEnd )
		{
			memcpy ( nameStore, data, sizeof ( nameStore ) );
			data += sizeof ( nameStore );

			auto entry = new libEntry ( );
			entry->fName = nameStore;
			entry->dataLen = *(decltype ( entry->dataLen ) *)data;
			data += sizeof ( entry->dataLen );
			entry->data = (uint8_t *) malloc ( sizeof ( uint8_t ) * entry->dataLen );
			memcpy ( entry->data, data, entry->dataLen );
			data += entry->dataLen;
			entries.push_back ( entry );
		}
	}

	~libStore ()
	{
		for ( auto it = entries.begin(); it != entries.end(); it++ )
		{
			delete *it;
		}
	}

	void add ( stringi const &fName )
	{
		FILE		*file;
		libEntry	*entry;
		char	     nameStore[MAX_PATH + 1];
		char		 fullFName[MAX_PATH + 1];

		_fmerge ( "*.slo", fName.c_str ( ), fullFName, sizeof ( fullFName ) );

		_fullpath ( nameStore, fullFName, sizeof ( nameStore ) );

		entry = new libEntry();

		if ( fopen_s ( &file, fullFName, "rb" ) )
		{
			printf ( "error: Input file \"%s\" not found \r\n", fName.c_str() );
			delete entry;
		} else
		{
			entry->fName = nameStore;
			_fseeki64 ( file, 0, SEEK_END );

			entry->dataLen = _ftelli64 ( file );
			_fseeki64 ( file, 0, SEEK_SET );
			entry->data = (uint8_t *)malloc ( sizeof ( uint8_t ) * entry->dataLen );
			fread ( entry->data, sizeof ( uint8_t ), entry->dataLen, file );
			fclose ( file );

			for ( auto it = entries.begin(); it != entries.end(); it++ )
			{
				if ( !strccmp ( (*it)->fName.c_str ( ), entry->fName.c_str ( ) ) )
				{
					delete *it;
					entries.erase ( it );
					break;
				}
			}
			entries.push_back ( entry );
			fclose ( file );
		}
	}

	void remove ( stringi const &fName )
	{
		for ( auto it = entries.begin(); it != entries.end(); it++ )
		{
			if ( !strccmp ( fName.c_str(), (*it)->fName.c_str() ) )
			{
				delete *it;
				entries.erase ( it );
				break;
			}
		}
	}

	void serialize ( void )
	{
		FILE		*file;
		char		 nameStore[MAX_PATH+1];

		if ( fopen_s ( &file, fName.c_str(), "w+b" ) )
		{
			printf ( "error: can not open \"%s\" for writing\r\n", fName.c_str() );
		} else
		{
			for ( auto it : entries )
			{
				strcpy_s ( nameStore, sizeof ( nameStore ), (it)->fName.c_str() );
				fwrite ( nameStore, sizeof ( nameStore ), 1, file );
				fwrite ( &(it)->dataLen, sizeof ( (it)->dataLen ), 1, file );
				fwrite ( (it)->data, sizeof ( uint8_t ), (it)->dataLen, file );
			}
			fclose ( file );
		}
	}

	uint8_t	*operator [] ( size_t index )
	{
		return entries[index]->data;
	}

	size_t size ( void )
	{
		return entries.size();
	}

	stringi *getName ( size_t index )
	{
		return &entries[index]->fName;
	}
};
