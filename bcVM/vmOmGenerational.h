/*

	Generational garbage collector

*/

#pragma once

#if 0
#define MURPHY 1
#define MURPHY_CHECK		//doesn't work... gc callbacks allocate memory so change ages
#define MURPHY_GUARD
#endif
//#define MURPHY 1
//#define MURPHY_CHECK		//doesn't work... gc callbacks allocate memory so change ages

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>
#include <stdint.h>
#include <cassert>
#include <list>
#include <vector>
#include <unordered_map>
#include "vmObjectMemory.h"
#include "bcVM/fglTypes.h"

struct omConfig
{
	size_t	age;					// 0 - 3
	size_t	round;					// 0 - 12
	size_t	varPad;					// 0 - 14 
	size_t	dataPad;				// 0 - 3
};

struct omGenGcDestructorInfo : public omDestructorInfo
{
	bool					 shouldDestroy;
	omGenGcDestructorInfo	*next;
	VAR						*obj;
};

 class omGenGCMemoryPage : public objectMemoryPage
{
public:
	class omGenGCMemoryPage			*next;
	class omGenGCMemoryPage			*prev;

	uint8_t							*data;
	uint8_t							*nextData;
	size_t							 pad;
	size_t							 size;
	size_t							 left;

public:
	omGenGCMemoryPage( size_t pSize, size_t	pPad )
	{
		pad = pPad;
		size = (pSize + (OM_PAGE_SIZE_DEFAULT - 1)) & ~(OM_PAGE_SIZE_DEFAULT - 1);
		data = (uint8_t *)VirtualAlloc( 0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
		nextData = data + pad;
		left = size-pad;
#if _DEBUG
		memset( data, 0xCD, size);
#endif
	}
	~omGenGCMemoryPage( void )
	{
		VirtualFree( data, 0, MEM_RELEASE );
	}
	inline void reset( size_t pSize, size_t pPad )
	{
		pad = pPad;
		if (size != pSize )
		{
			VirtualFree( data, 0, MEM_RELEASE );
			this->size = (pSize + (OM_PAGE_SIZE_DEFAULT - 1)) & ~(OM_PAGE_SIZE_DEFAULT - 1);
			data = (uint8_t *)VirtualAlloc( 0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
		}
		left = size - pad;
#if _DEBUG
		memset( data, 0xEF, size );

#endif
		nextData = data + pad;
	}
	inline bool inPage( void *ptr )
	{
		if ( (ptr >= (void *)data) && (ptr < ((uint8_t *)data + size)) )
		{
			return true;
		}
		return false;
	}
	inline void *alloc( size_t size )
	{
		// round up to pad size, this keeps us aligned properly with the required lower address bits
		size = (size + 0x3) & (~0x3);
		if ( left > size )
		{
			assert ( ((size_t) nextData & 0x03) == pad );
			uint8_t *ret = nextData;
			nextData += size;
			left -= size;
			assert ( ((size_t)nextData & 0x03) == pad );
			return ret;
		}
		return 0;
	}
	inline void clear ( void )
	{
#if _DEBUG
		memset ( data, 0xFD, size );
#endif
	}
 };

struct omGenGcVarPage
{
	public:
	omGenGcVarPage				 *next;

	bool						  doTables;
	VAR							 *nextVar;
	size_t						  left;

	VAR							 *varStart;
	size_t						  numVars;

	DWORD						  pageSize;

	VAR							 *data;
	size_t						  dataSize;
	size_t						  dataPad;

	VAR							**cardTables;
	ULONG						  cardTableSize;
	ULONG						  cardVarCount;

	// murphy protection structures
	size_t						  guardCounter;

	SYSTEM_INFO					  sysInfo;

#if defined ( _DEBUG )
	bool						  inUse = true;
#endif

	public:
	omGenGcVarPage ( size_t count, bool doTables, size_t pad ) : doTables( doTables ), dataPad( pad )
	{
		GetSystemInfo( &sysInfo );

		// we round count up so that we can max out a page.
		dataSize = ((count * sizeof (VAR)) + ((size_t)sysInfo.dwPageSize - 1)) & ~((size_t)sysInfo.dwPageSize - 1);
		count = dataSize / sysInfo.dwPageSize;

		data = (VAR *)VirtualAlloc( 0, dataSize, MEM_COMMIT | MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE );
		cardTableSize = (ULONG)(dataSize / sysInfo.dwPageSize) + 1;
		cardTables = (VAR **)malloc ( cardTableSize * sizeof ( VAR *) );

		left = dataSize / sizeof ( VAR ) - 1;
#if _DEBUG
		memset( data, 0xCD, dataSize );
#endif
		nextVar = (VAR *)((uint8_t *)data + dataPad);

		numVars = left;
		varStart = nextVar;
	}

	~omGenGcVarPage( void )
	{
		free( cardTables );
	}

	inline void reset( size_t count, bool doTables, size_t pad )
	{
		this->dataPad = pad;
		this->doTables = doTables;

		size_t dataSize2 = ((count * sizeof( VAR )) + ((size_t)sysInfo.dwPageSize - 1)) & ~((size_t)sysInfo.dwPageSize - 1);

		if ( dataSize2 != dataSize )
		{
			VirtualFree( data, 0, MEM_RELEASE );
			free( cardTables );

			dataSize = ((count * sizeof( VAR )) + ((size_t)sysInfo.dwPageSize - 1)) & ~((size_t)sysInfo.dwPageSize - 1);
			count = dataSize / sysInfo.dwPageSize;

			data = (VAR *)VirtualAlloc( 0, dataSize, MEM_COMMIT | MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE );
			cardTableSize = (ULONG)(dataSize / sysInfo.dwPageSize) + 1;
			cardTables = (VAR **)malloc( cardTableSize * sizeof( VAR * ) );
		}

#if _DEBUG
		memset( data, 0xEF, dataSize );
#endif
		// reset the card table - new allocation should be 0
		resetCardTable();

		left = dataSize / sizeof( VAR ) - 1;
		nextVar = (VAR *)((uint8_t *)data + dataPad);

		numVars = left;
		varStart = nextVar;
	}

	VAR *getFirstVar ()
	{
		return varStart;
	}

	size_t getNumVars ()
	{
		return numVars;
	}

	inline bool inPage( void *ptr )
	{
		if ( (ptr >= (void *)data) && (ptr < ((uint8_t *)data + dataSize)) )
		{
			return true;
		}
		return false;
	}

	inline size_t  fillCardTable (  void )
	{
		ULONG_PTR tableSize = cardTableSize;
		GetWriteWatch( 0, data, dataSize, (void **)cardTables, &tableSize, &pageSize );
		cardVarCount = pageSize / sizeof( VAR );
		return (size_t)tableSize;
	}

	inline void resetCardTable( void )
	{
		ResetWriteWatch( data, dataSize );
	}

	inline size_t getCardVarCount( size_t page )
	{
		if ( cardTables[page] == data )
		{
			// first page
			if ( nextVar < data + cardVarCount )
			{
				return nextVar - data;
			} else
			{
				return cardVarCount;
			}
		} else if ( cardTables[page] == (VAR *)((uint8_t *)data + dataSize - pageSize) )
		{
			// last page
			return nextVar - cardTables[page];
		} else
		{
			// middle pages
			if ( (nextVar > cardTables[page]) && (nextVar < cardTables[page] + cardVarCount) )
			{
				return nextVar - cardTables[page] + 1;
			} else
			{
				return cardVarCount + 1;
			}
		}
	}

	inline VAR *getCardVar( size_t page )
	{
		if ( cardTables[page] == data )
		{
			// first page
			return (VAR *)(((uint8_t *)cardTables[page]) + dataPad);
		} else 
		{
			// last page and middle pages - start one var back into previous page in case we cought the end of a var that started in the previous page
			return (VAR *)(((uint8_t *)cardTables[page]) + dataPad - sizeof ( VAR ));
		}
	}


	inline VAR *allocVar( size_t count )
	{
		if ( left >= count )
		{
			VAR *ret = nextVar;
			left -= count;
			nextVar += count;
			if ( left )
			{
				nextVar->type = slangType::eNULL;
			}
			return ret;
		}
		return 0;
	}

	inline void clear ( void )
	{
#if _DEBUG
		memset ( data, 0xFD, dataSize );
#endif
	}
};

class omGenGcPageCache
{
	omGenGcVarPage		*varFree = nullptr;
	omGenGCMemoryPage	*pageFree = nullptr;

	CRITICAL_SECTION	 varAccess;
	CRITICAL_SECTION	 pageAccess;

	static omGenGcPageCache *getInstance()
	{
		static omGenGcPageCache  instance;
		return &instance;
	}

	public:
	omGenGcPageCache( omGenGcPageCache const& ) = delete;
	void operator=( omGenGcPageCache const& ) = delete;

	static omGenGcVarPage *getVarPage( size_t count, bool doTables, size_t pad )
	{
		omGenGcVarPage *ret;

		auto cache = getInstance();

		EnterCriticalSection( &cache->varAccess );
		if ( cache->varFree )
		{
			ret = cache->varFree;
			cache->varFree = cache->varFree->next;

#if defined ( _DEBUG )
			assert ( !ret->inUse );
			ret->inUse = true;
#endif
			LeaveCriticalSection( &cache->varAccess );

			ret->reset( count, doTables, pad );
		} else
		{
			LeaveCriticalSection( &cache->varAccess );
			ret = new omGenGcVarPage( count, doTables, pad );
		}
//		WIN32_MEMORY_RANGE_ENTRY entry{ ret->data, ret->dataSize };
//		PrefetchVirtualMemory ( GetCurrentProcess ( ), 1, &entry, 0 );
		return ret;
	}

	static void free( omGenGcVarPage *page )
	{
		auto cache = getInstance();
		page->clear ();
		EnterCriticalSection( &cache->varAccess );

#if defined ( _DEBUG )
		assert ( page->inUse );
		page->inUse = false;
#endif
		page->next = cache->varFree;
		cache->varFree = page;
		LeaveCriticalSection( &cache->varAccess );
	}

	static omGenGCMemoryPage *getPage( size_t size, size_t pad )
	{
		omGenGCMemoryPage *ret;

		auto cache = getInstance();

		EnterCriticalSection( &cache->pageAccess );
		if ( cache->pageFree )
		{
			ret = cache->pageFree;
			cache->pageFree = cache->pageFree->next;
			LeaveCriticalSection( &cache->pageAccess );

			ret->reset( size, pad );
		} else
		{
			LeaveCriticalSection( &cache->pageAccess );
			ret = new omGenGCMemoryPage( size, pad );
		}
//		WIN32_MEMORY_RANGE_ENTRY entry{ ret->data, ret->size };
//		PrefetchVirtualMemory ( GetCurrentProcess ( ), 1, &entry, 0 );
		return ret;
	}

	static void free( omGenGCMemoryPage *page )
	{
		auto cache = getInstance();
		EnterCriticalSection( &cache->pageAccess );
		page->next = cache->pageFree;
		cache->pageFree = page;
		LeaveCriticalSection( &cache->pageAccess );
	}

	omGenGcPageCache()
	{
		InitializeCriticalSectionAndSpinCount( &varAccess, 4000 );
		InitializeCriticalSectionAndSpinCount( &pageAccess, 4000 );
	}

	~omGenGcPageCache()
	{
		for ( auto it = varFree; it; )
		{
			auto itNext = it->next;
			delete it;
			it = itNext;
		}
		for ( auto it = pageFree; it; )
		{
			auto itNext = it->next;
			delete it;
			it = itNext;
		}
		DeleteCriticalSection( &varAccess );
		DeleteCriticalSection( &pageAccess );
	}
};

class omGenGcGeneration
{
	omGenGcVarPage			*vars[OM_MAX_ROUNDS];
	omGenGcVarPage			*oldVars[OM_MAX_ROUNDS];
	size_t					 pad[OM_MAX_ROUNDS];

	omGenGCMemoryPage		*pages = 0;
	omGenGCMemoryPage 		*oldPages = 0;
	omGenGCMemoryPage		*lastPage = 0;
	size_t					 pagePad;

	size_t					 collectionCount = 0;

	bool					 doTables;

	bool					 inCollection = false;
	bool					 wantCollection = false;

	public:
	size_t					 totalVars = 0;
	size_t					 totalMem = 0;

	omGenGcGeneration( bool doTables ) : doTables( doTables )
	{
		memset( vars, 0, sizeof( vars ) );
		memset( pad, 0, sizeof( pad ) );
		memset ( oldVars, 0, sizeof ( oldVars ) );
	}

	~omGenGcGeneration( void )
	{
		for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
		{
			for ( auto it = vars[round]; it; )
			{
				auto itNext = it->next;
				omGenGcPageCache::free( it );
				it = itNext;
			}
		}
		for ( auto it = pages; it; )
		{
			auto itNext = it->next;
			omGenGcPageCache::free( it );
			it = itNext;
		}
	}

	inline omGenGcVarPage *getPages( size_t round )
	{
		return vars[round];
	}

	inline void setPad( size_t round, size_t padding, size_t dataPad )
	{
		pad[round] = padding;
		pagePad = dataPad;
	}

	inline void reset( void )
	{
		for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
		{
			for ( auto it = vars[round]; it; )
			{
				auto itNext = it->next;
				omGenGcPageCache::free( it );
				it = itNext;
			}
		}
		for ( auto it = pages; it; )
		{
			auto itNext = it->next;
			omGenGcPageCache::free( it );
			it = itNext;
		}
		memset( vars, 0, sizeof( vars ) );
		totalVars = 0;
		totalMem = 0;
		collectionCount = 0;
		inCollection = false;
		wantCollection = false;

		lastPage = 0;
		pages = 0;
	}

	inline void beginCollection( void )
	{
		inCollection = true;
		collectionCount++;
		totalVars = 0;
		totalMem = 0;

		memcpy( oldVars, vars, sizeof( oldVars ) );
		memset( vars, 0, sizeof( vars ) );
		oldPages = pages;

		lastPage = 0;
		pages = 0;
	}

	inline void endCollection( void )
	{
		for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
		{
			for ( auto it = oldVars[round]; it; )
			{
				auto itNext = it->next;
				omGenGcPageCache::free( it );
				it = itNext;
			}
			oldVars[round] = 0;
		}
		for ( auto it = oldPages; it; )
		{
			auto itNext = it->next;
			omGenGcPageCache::free( it );
			it = itNext;
		}
		oldPages = 0;

		if ( doTables )
		{
			for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
			{
				for ( auto it = vars[round]; it; it = it->next )
				{
					//  reset the card table;
					it->resetCardTable();
				}
			}
		}

		lastPage = pages;

		inCollection = false;
		wantCollection = false;
		totalVars = 0;
		totalMem = 0;
	}

	inline void resetTables( void )
	{
		if ( doTables )
		{
			for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
			{
				for ( auto it = vars[round]; it; it = it->next )
				{
					//  reset the card table;
					it->resetCardTable();
				}
			}
		}
	}

	inline bool needCollection( void )
	{
		return wantCollection;
	}

	inline bool inPage( void *ptr )
	{
		for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
		{
			for ( auto it = vars[round]; it; it = it->next )
			{
				if ( it->inPage( ptr ) ) return true;
			}
		}
		for ( auto it = pages; it; it = it->next )
		{
			if ( it->inPage( ptr ) ) return true;
		}
		return false;
	}

	inline bool inOldPage( void *ptr )
	{
		for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
		{
			for ( auto it = oldVars[round]; it; it = it->next )
			{
				if ( it->inPage( ptr ) ) return true;
			}
		}
		for ( auto it = oldPages; it; it = it->next )
		{
			if ( it->inPage( ptr ) ) return true;
		}
		return false;
	}

	inline VAR *allocVar( size_t count, size_t round, bool &coll )
	{
		VAR *ret;

		totalVars += count;

		if ( vars[round] )
		{
			ret = vars[round]->allocVar( count );
			if ( ret ) return ret;
		}

		if ( totalVars > OM_GENGC_VARS_BEFORE_COLLECTION )
		{
			wantCollection = true;
			coll |= true;
		}

		omGenGcVarPage *newPage = omGenGcPageCache::getVarPage( count > OM_VAR_SIZE_DEFAULT ? count : OM_VAR_SIZE_DEFAULT, doTables, pad[round] );
		newPage->next = vars[round];
		vars[round] = newPage;

		newPage->resetCardTable ();

		return newPage->allocVar( count );
	}

	inline void *alloc( size_t size, bool &coll )
	{
		void *ret;

		totalMem += size;

		if ( lastPage )
		{
			ret = lastPage->alloc( size );
			if ( ret ) return ret;
		}

		if ( totalMem > OM_PAGE_SIZE_DEFAULT * OM_NEW_PAGES_BEFORE_COLLECTION )
		{
			wantCollection = true;
			coll |= true;
		}
		omGenGCMemoryPage *newPage = omGenGcPageCache::getPage ( size > OM_PAGE_SIZE_DEFAULT ? size : OM_PAGE_SIZE_DEFAULT, pagePad );
		if ( pages )
		{
			pages->prev = newPage;
		}
		newPage->prev = 0;
		newPage->next = pages;
		pages = newPage;
		lastPage = newPage;

		return newPage->alloc( size );
	}

	inline objectMemoryPage *detachPage( void *ptr )
	{
		for ( auto it = pages; it; it = it->next )
		{
			if ( ((unsigned char *)ptr >= it->data) && ((unsigned char *)ptr < it->data + it->size) )
			{
				if ( it->prev )
				{
					it->prev->next = it->next;
				} else
				{
					pages = it->next;
				}
				if ( it->next )
				{
					it->next->prev = it->prev;
				}
				lastPage = pages;
				return it;
			}
		}
		lastPage = pages;
		return nullptr;
	}
};

#pragma pack ( push, 1 )
struct objectAgeMap
{
	uint8_t		age;
	uint8_t		round;
};

struct objectAgeRoundPadMap
{
	size_t	gen;
	size_t	round;
	size_t	pad;
};

#pragma pack ( pop )

class omGenGc : public objectMemory
{
	protected:
		size_t							  disableCollection = 0;
		size_t							  collectionCount = 0;
		size_t							  pagesSinceCollection = 0;
		bool							  wantCollection = false;

		class vmInstance				 *instance = 0;

		std::vector<omGenGcGeneration *>  generation;

		objectAgeMap					  ageRoundMap[16];

		omGenGcDestructorInfo			 *destructorList = 0;
		omGenGcDestructorInfo			 *pendingDestructionList = 0;
		std::list<VAR *>				  collectionUpdateList;			// used to avoid incredibly deep recursion when collection deep objects

		std::vector<std::pair<VAR *, size_t>>		roots;

		std::vector<VAR **>				  movePatch;

		uint64_t						  totalCollectionTime = 0;
		uint64_t						  totalCollections = 0;
		uint64_t						  maxCollectionTime = 0;
		uint64_t						  minCollectionTime = 0;

		void							  collect ();
		void							  collectionDisable ( void ) override;
		void							  collectionEnable ( void ) override;

#if defined ( MURPHY_CHECK )
		bool							  inCheck = false;
		int64_t							  reservedVars = 0;
		int64_t							  reservedMem = 0;
		void							  check ( void );
		void							  check ( VAR *val, bool root );
		void							  checkAge ( void *ptr, size_t age );
		void							  checkAge ( VAR *val, size_t age, size_t round );
#endif
	public:
		omGenGc( class vmInstance *instance );
		virtual ~omGenGc();

		void				 setPadding( size_t count, objectAgeRoundPadMap *map )
		{
			for ( size_t loop = 0; loop < count; loop++ )
			{
				generation[map[loop].gen]->setPad( map[loop].round, map[loop].pad, map[loop].gen );
				// TODO: generate ageMap automatically from the padding configuration
			}
		}

		size_t			getAge ( char const * ptr ) const override
		{
			ptrdiff_t index = (ptrdiff_t)ptr;
			index &= 0x03;
			return (size_t)index;
		}
		size_t			 getAge( VAR const *var ) const override
		{
			ptrdiff_t index = (ptrdiff_t)var;
			index &= 0x03;
			return ageRoundMap[index].age;
		}
		size_t			 getRound( VAR const *var ) const override
		{
			ptrdiff_t index = (ptrdiff_t)var;
			index &= 0x03;
			return ageRoundMap[index].round;
		}
		size_t			 nextAge( VAR const *var ) const
		{
			ptrdiff_t index = (ptrdiff_t)var;
			index &= 0x3;
			assert( index <= 14 );
			return ageRoundMap[index  + 1].age;
		}
		size_t			 nextRound( VAR const *var )const 
		{
			ptrdiff_t index = (ptrdiff_t)var;
			index &= 0x03;
			assert( index <= 14 );
			return ageRoundMap[index + 1].round;
		}

		void				 reset( void ) override;
		void				*alloc ( size_t size ) override;		// allocate memory from a specific generation
		VAR					*allocVar ( size_t count ) override;	// allocat a VAR from a specific generation

		void				*allocGen ( size_t size, size_t gen ) override;		// allocate memory from a specific generation
		VAR					*allocVarGen ( size_t count, size_t gen, size_t round ) override;	// allocat a VAR from a specific generation

		void				 collect ( size_t p ) override;
		char				*strDup ( char const *str ) override;

		bool				 isInGCMemory ( void *ptr ) override;
	//	long				 memoryInfo( size_t  mode );
		size_t				 memoryState ( void ) override
		{
			return collectionCount;
		};

		void				 registerDestructor ( VAR *obj ) override;
		void				 moveDestructorList( size_t stopGen );
		void				 processDestructors( void ) override;

		objectMemoryPage	*detachPage( void *ptr ) override;
		void				 freeMemoryPage( objectMemoryPage	*page ) override;

		protected:
		bool				 inPage( void *ptr );
		bool				 isValidVar( void *ptr );
		bool				 isRoot( VAR *val );

		VAR					*move( VAR **, VAR *val, bool tableWalk, bool doCopy, size_t destGen, size_t destRound );
		VAR					*age( VAR **, VAR *val, bool root, size_t destGen, size_t stopGen );
		void				 moveRoots( size_t stopGen );
		bool				 walkCardTable( size_t generation );
		void				 scanGeneration ( size_t generation );

		uint64_t			 memoryInfo( size_t num );

		void				 printStats( void ) override;


		using genColRecursionBuster = std::unordered_set<VAR *>;

		struct genColVars : collectionVariables
		{
		private:
			bool copyObject;
			size_t age;
			size_t round;
			omGenGc *om;
//			genColRecursionBuster &recBust;

		private:
			genColVars ( size_t a, size_t round, bool copy, omGenGc *om/*, genColRecursionBuster &recBust*/ ) : copyObject ( copy ), age ( a ), round ( round ), om ( om )// , recBust ( recBust )
			{}

		public:
			bool doCopy ( void const *ptr ) override
			{
				if ( copyObject || om->getAge ( (const char *)ptr ) < age )
				{
					return true;
				}
				return false;
			}
			size_t getAge ( ) override
			{
				return age;
			}
			friend class omGenGc;
		};

		// support for garbage collection of vars
		VAR *move ( vmInstance *instance, VAR **newAddr, struct VAR *val, bool copy, collectionVariables *col ) override;
};
