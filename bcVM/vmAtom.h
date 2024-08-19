/*
	atom header file

*/
#pragma once

#include "Utility/funcky.h"
#include "Target/common.h"
#include "compilerParser/fglErrors.h"
#include <cassert>

class atom {
public:
	enum class atomType : int
	{
		aNOT_DEFINED = 0,
		aLOADIMAGE,
		aCLASSDEF,
		aFUNCDEF,
		aMAX_TYPE,
	};

	atomType					 type;
	char						 name[MAX_NAME_SZ];
	uint64_t					 hash = 0;

	bool						 isDeleteable = false;	// indicates that the cargo can be deleted.. otherwise this is a pointer into someting else (loadImage data)
	bool						 isPersistant = false;	// indicates that the cargo is persistant and should exist across resets

	union {
		void					*cargo2;
		struct	bcLoadImage		*loadImage;
		struct	bcClass			*classDef;
		struct	bcFuncDef		*funcDef;
	};

	class	atom				*next = 0;				// pointer to next used atom
	class   atom				*typeNext = 0;			// pointer next atom of same type
	uint32_t					 index = 0;				// for when we're accesing as a list and not an array... 

	uint32_t					 useCount = 0;			// for pack

	// profiler stuff
	uint64_t					 localExecuteTime = 0;	// for profiling functions - time for procedure exclusive of calls
	uint64_t					 totalExecuteTime = 0;	// for profiling functions - total of procedure and it's calls
	uint32_t					 nCalls = 0;			// number of calls

	atom ( uint32_t index )
	{
#if _DEBUG
		memset ( name, 0, sizeof ( name ) );
#endif
		type = atom::atomType::aNOT_DEFINED;
		cargo2 = 0;
		this->index = index;
		*name = 0;
	}

	void release ();
	~atom ();

	void		*operator new		( size_t size, class vmInstance *instance );
	void		 operator delete	( void *p, class vmInstance *instance );
	void		 operator delete	( void *p );
	void		*operator new[]		( size_t size, class vmInstance *instance );
	void		 operator delete[]	( void *p );
};

class atomTable	{
	atom				**table;
	uint32_t			  size = 128;
	class vmInstance	 *instance;
	atom				 *freeList = 0;
	atom				 *typeList[int(atom::atomType::aMAX_TYPE)];
	atom				 *used;

private:
	uint64_t getHash ( char const *ptr )
	{
		assert ( size > 1 );
		return _hashKey ( ptr );
	}

	uint32_t getCtr ( uint64_t hash )
	{
		assert ( size > 1 );
		return hash % ((int64_t)size - 1);
	}

	uint32_t find ( char const *name )
	{
		uint64_t hash = getHash ( name );
		uint32_t ctr = getCtr ( hash );

		do {
			if ( table[ctr] )
			{
				if ( table[ctr]->hash == hash && !strccmp ( table[ctr]->name, name ) )
				{
					return ( ctr + 1 );
				}
			} else
			{
				return 0;
			}
			ctr++;
			if ( ctr == size ) ctr = 0;
		} while ( hash != ctr );

		// cant find atom
		throw errorNum::scINTERNAL;
	}

	uint32_t find( char const *name, uint64_t hash )
	{
		auto ctr = getCtr ( hash );

		do
		{
			if ( table[ctr] )
			{
				if ( table[ctr]->hash == hash && !strccmp( table[ctr]->name, name ) )
				{
					return (ctr + 1);
				}
			} else
			{
				return 0;
			}
			ctr++;
			if ( ctr == size ) ctr = 0;
		} while ( hash != ctr );

		// cant find atom
		throw errorNum::scINTERNAL;
	}

	public:
	atomTable ( uint32_t nAtoms, vmInstance *instance ) ;
	~atomTable ( );

	template <typename map>
	void typeMap ( atom::atomType t, map m )
	{
		auto atm = typeList[int(t)];
		while ( atm )
		{
			assert ( atm->type == t );
			if ( m ( atm->index + 1 ) )
			{
				break;
			}
			atm = atm->typeNext;
		}
	}

	uint32_t make ( char const *name, uint64_t hash, struct bcLoadImage *data, bool isDeleteable, bool isPersistant );
	uint32_t make ( char const *name, uint64_t hash, struct bcClass *data, bool isDeleteable, bool isPersistant );
	uint32_t make ( char const *name, uint64_t hash, struct bcFuncDef *data, bool isDeleteable, bool isPersistant );
	uint32_t make ( char const *name, uint64_t hash );

	struct bcClass *atomGetClass ( uint32_t index )
	{
		assert ( index );
		assert ( index <= size );
		assert ( table[index - 1] );
		if ( table[index - 1]->type != atom::atomType::aCLASSDEF )
		{
			throw errorNum::scCLASS_NOT_FOUND;
		}
		return table[index-1]->classDef;
	}

	struct bcFuncDef *atomGetFunc ( uint32_t index )
	{
		assert ( index );
		assert ( index <= size );
		assert ( table[index - 1] );
		if ( table[index - 1]->type != atom::atomType::aFUNCDEF )
		{
			throw errorNum::scUNKNOWN_FUNCTION;
		}
		return table[index-1]->funcDef;
	}

	struct bcLoadImage *atomGetLoadImage ( uint32_t index )
	{
		assert ( index );
		assert ( index <= size );
		assert ( table[index - 1] );
		if ( table[index - 1]->type != atom::atomType::aLOADIMAGE )
		{
			throw errorNum::scNO_LIBRARY;
		}
		return table[index - 1]->loadImage;
	}

	struct bcLoadImage *atomGetLoadImage ( char const *name )
	{
		uint32_t index;

		index = find ( name );
		if ( !index )
		{
			throw errorNum::scNO_LIBRARY;
		}
		assert ( index <= size  );
		if ( table[index - 1]->type != atom::atomType::aLOADIMAGE )
		{
			throw errorNum::scNO_LIBRARY;
		}
		return table[index-1]->loadImage;
	}

	struct bcClass *atomGetClass ( char const *name )
	{
		uint32_t index;

		index = find ( name );
		if ( !index )
		{
			throw errorNum::scCLASS_NOT_FOUND;
		}
		assert ( index <= size  );
		if ( table[index - 1]->type != atom::atomType::aCLASSDEF )
		{
			throw errorNum::scCLASS_NOT_FOUND;
		}
		return table[index-1]->classDef;
	}

	struct bcFuncDef *atomGetFunc ( char const *name )
	{
		uint32_t index;

		index = find ( name );
		if ( !index )
		{
			throw errorNum::scUNKNOWN_FUNCTION;
		}
		assert ( index <= size  );
		if ( table[index - 1]->type != atom::atomType::aFUNCDEF )
		{
			throw errorNum::scUNKNOWN_FUNCTION;
		}
		return table[index-1]->funcDef;
	}

	template <typename T>
	uint32_t atomPut ( char const *name, T data, bool isDeleteable, bool isPersistant )
	{
		auto hash = getHash ( name );
		return make ( name, hash, data, isDeleteable, isPersistant );
	}

	void atomUpdate ( uint32_t index, struct bcClass *data )
	{
		assert ( index && index <= size  );
		assert ( table[index - 1] );
		assert ( table[index - 1]->type == atom::atomType::aCLASSDEF );

		table[index - 1]->classDef = data;
	}

	void atomUpdate ( uint32_t index, struct bcClass *data, bool isDeleteable, bool isPersistant )
	{
		assert ( index && index <= size  );
		assert ( table[index - 1] );
		assert ( table[index - 1]->type == atom::atomType::aCLASSDEF );

		table[index - 1]->isDeleteable	= isDeleteable;
		table[index - 1]->isPersistant = isPersistant;
		table[index - 1]->classDef = data;
	}

	void atomUpdate ( uint32_t index, struct bcLoadImage *data )
	{
		assert ( index && index <= size  );
		assert ( table[index - 1] );
		assert ( table[index - 1]->type == atom::atomType::aLOADIMAGE );

		table[index - 1]->loadImage = data;
	}

	void atomUpdate ( uint32_t index, struct bcLoadImage *data, bool isDeleteable, bool isPersistant )
	{
		assert ( index && index <= size  );
		assert ( table[index - 1] );
		assert ( table[index - 1]->type == atom::atomType::aLOADIMAGE );

		table[index - 1]->isDeleteable	= isDeleteable;
		table[index - 1]->isPersistant = isPersistant;
		table[index - 1]->loadImage = data;
	}

	void atomUpdate ( uint32_t index, struct bcFuncDef *data )
	{
		assert ( index && index <= size  );
		assert ( table[index - 1] );
		assert ( table[index - 1]->type == atom::atomType::aFUNCDEF );

		table[index - 1]->funcDef = data;
	}

	void atomUpdate ( uint32_t index, struct bcFuncDef *data, bool isDeleteable, bool isPersistant )
	{
		assert ( index && index <= size  );
		assert ( table[index - 1] );
		assert ( table[index - 1]->type == atom::atomType::aFUNCDEF );

		table[index - 1]->isDeleteable = isDeleteable;
		table[index - 1]->isPersistant = isPersistant;
		table[index - 1]->funcDef = data;
	}

	atom::atomType atomGetType ( uint32_t index )
	{
		if ( index && table[index - 1] )
		{
			return table[index - 1]->type;
		} else
		{
			return atom::atomType::aNOT_DEFINED;
		}
	}

	atom::atomType atomGetType ( char const *name )
	{
		uint64_t hash = getHash ( name );
		uint32_t ctr = getCtr ( hash );

		do
		{
			if ( table[ctr] )
			{
				if ( table[ctr]->hash == hash && !strccmp ( table[ctr]->name, name ) )
				{
					return table[ctr]->type;
				}
			} else
			{
				return atom::atomType::aNOT_DEFINED;
			}
			ctr++;
			if ( ctr == size ) ctr = 0;
		} while ( hash != ctr );

		return atom::atomType::aNOT_DEFINED;
	}


	uint32_t atomFind ( char const *name )
	{
		return find ( name );
	}

	uint32_t atomFind( char const *name, uint64_t hash )
	{
		return find( name, hash );
	}

	uint32_t getInUse ( uint32_t index )
	{
		assert ( index && table[index - 1] );
		return table[index - 1]->useCount;
	}

	void markInUse( uint32_t index )
	{
		assert ( index && table[index - 1] );
		table[index -1]->useCount++;
	}

	void clearUseCount ( uint32_t index )
	{
		assert ( index && table[index - 1] );
		table[index - 1]->useCount = 0;
	}

	char *atomGetName ( uint32_t index )
	{
		assert ( index && table[index - 1] );
		return table[index - 1]->name;
	}

	void reset ( void );
};
