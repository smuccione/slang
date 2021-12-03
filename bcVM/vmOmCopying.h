/*

	object memory header

*/

#pragma once

#include <cassert>
#include <list>
#include <vector>
#include "vmObjectMemory.h"
#include "bcVM/fglTypes.h"

#define PAD_BYTES		"pRoTeCtMeFrOmBuG"
#define NUM_PAD_BYTES	sizeof ( PAD_BYTES )

class omCopyGCCb : public omCallback
{
	class omCopyGCCb *next;
	class omCopyGCCb *prev;

	friend class omCopyGc;
};

struct omCopyGcDestructorInfo : public omDestructorInfo
{
	bool					 shouldDestroy;
	omCopyGcDestructorInfo	*next;
	VAR						*obj;
};

struct OM_GUARD {
	uint8_t				 beginPad[NUM_PAD_BYTES];
	struct	 OM_GUARD	*next;
	size_t				 ctr;
	uint8_t				 endPad[NUM_PAD_BYTES];
};

class omCopyGcMemoryPage : public objectMemoryPage
{
public:
	class omCopyGcMemoryPage	*next;
	class omCopyGcMemoryPage	*prev;

	size_t				 size;			// not needed... just for inspection
	uint8_t				*data;
	size_t				 used;
	size_t				 left;

	size_t				 lockCount;
	bool				 freeAble;

	// murphy protection structures
	OM_GUARD			*guardStart;
	OM_GUARD			*guardEnd;
	size_t				 guardCounter;

public:
	omCopyGcMemoryPage();

	omCopyGcMemoryPage( const class omCopyGcMemoryPage &old )
	{
		throw 500;
	}
	void addGuard		( void );
	void validateGuard	( void );
	void freePage ( void )
	{
		freeAble = true;
		if ( !lockCount )
		{
			delete this;				// replace with global free list
		}
	}
	void check ( void )
	{
		assert ( used + left == size );
	}
};


class omCopyGc: public objectMemory
{
protected:
	size_t							  pagesSinceCollection = 0;
	size_t							  disableCollection = 0;
	size_t							  collectionCount = 0;
	bool							  wantCollection = false;

	class vmInstance				 *instance;

	omCopyGcMemoryPage 				 *pages = 0;
	omCopyGcMemoryPage 				 *oldPages = 0;
	omCopyGcMemoryPage 				 *lockedPages = 0;
	omCopyGcMemoryPage				 *lastPage = 0;

	omCopyGcDestructorInfo			 *destructorList = 0;
	omCopyGcDestructorInfo			 *pendingDestructionList = 0;
	std::list<struct VAR *>			  collectionUpdateList;			// used to avoid incredibly deep recursion when collection deep objects

	std::list<omCopyGcMemoryPage *>	  murphyPages;
	bool							  murphyEnabled = false;
	size_t							  murphyPtrRotate = 0;
	void							  collect ();
	void							  collectionDisable ( void );
	void							  collectionEnable ( void );
public:
	omCopyGc( class vmInstance *instance );
	virtual ~omCopyGc();

	void				 reset					( void );
	void				*alloc					( size_t size );
	VAR					*allocVar				( size_t count );
																								// should ONLY ever be called when in a collection (e.g. from a GC callback)
	void				*allocGen				( size_t len, size_t gen ) override { return alloc ( len ); }
	VAR					*allocVarGen			( size_t count, size_t gen, size_t round ) override { return allocVar ( count ); };	// allocat a VAR from a specific generation

	void				 collect				( size_t P );
	char				*strDup					( char const     *str		);

	bool				 isInGCMemory			( void			 *ptr		);
	size_t				 memoryInfo				( size_t        mode		);
	bool				 murphyStatus			( bool			  enable	);

	omCopyGcMemoryPage	*detachPage				( void *ptr );
	void				 checkMemory			( void );
	void				 checkAll				( void );
	void				 checkQuick				( void );
	size_t				 memoryState			( void ) { return collectionCount; };

	void				 processDestructors( void );
	void				 registerDestructor( VAR *obj );
	void				 moveDestructorList( void );

	// support for garbage collection of vars
	VAR					*move ( vmInstance *instance, VAR **srcPtr, struct VAR *val, bool copy, objectMemory::collectionVariables *col ) override;

	virtual	void		 freeMemoryPage			( omCopyGcMemoryPage *page );

protected:
	bool				 inPage					( void const *ptr );
	bool				 inOldPage				( void const *ptr );
	bool				 isValidVar				( void const *ptr );
	VAR					*_move					( VAR *val, bool copy, int depth );
	void				 _check					( VAR *val );

	virtual	omCopyGcMemoryPage	*getMemoryPage	( size_t			 size );
};

class cachedObjectMemory : public omCopyGc
{
	public:
	cachedObjectMemory( vmInstance *instance ) : omCopyGc( instance )
	{
	}

	~cachedObjectMemory();
	omCopyGcMemoryPage	*getMemoryPage( size_t size );
	void freeMemoryPage( objectMemoryPage *pageP );
};

class objectPageCache
{
	size_t				 numPages;
	HANDLE				 heap;
	CRITICAL_SECTION	 lock;
	size_t				 defaultPageSize;

	omCopyGcMemoryPage	*pages;
	omCopyGcMemoryPage	*freePages;

	public:
	objectPageCache( size_t defaultPageSize, size_t maxMem );
	~objectPageCache();
	size_t nFreePages();
	omCopyGcMemoryPage	*getMemoryPage( size_t size );
	void freeMemoryPage( omCopyGcMemoryPage	*page );
 };

extern class objectPageCache *pageCache;
