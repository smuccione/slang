/*

      Object Memory Manager - SLANG compiler

*/

/*
   Murphy test is currently fully implemented.  Current implementation is
   much more robust then prior implementations.  Should always be used to check
   any modifications that do VAR structure manipulations.
*/

#if 0
#define MURPHY 1
#define MURPHY_GUARD
#define MURPHY_CHECK
#endif

#include "bcVM.h"
#include "bcVM/vmOmCopying.h"

#ifdef MURPHY_GUARD
#define GUARD_EXTRA	(sizeof ( OM_GUARD ))
#else
#define GUARD_EXTRA 0ull
#endif	

#define OM_BOUNDARY  3

objectPageCache::objectPageCache( size_t defaultPageSize, size_t maxMem ) : defaultPageSize( defaultPageSize )
{
	numPages = maxMem / defaultPageSize;

	heap = HeapCreate( HEAP_NO_SERIALIZE, 0, 0 /*maxMem*/ );

	pages = new omCopyGcMemoryPage[numPages]();

	// build a free list of pages
	freePages = 0;
	for ( size_t loop = 0; loop < numPages; loop++ )
	{
		pages[loop].next = freePages;
		freePages = &pages[loop];
	}

	InitializeCriticalSectionAndSpinCount( &lock, 4000 );
}

objectPageCache::~objectPageCache()
{
	HeapDestroy( heap );
	delete pages;
}

size_t objectPageCache::nFreePages()
{
	size_t cnt = 0;
	for ( auto it = freePages; it; it = it->next )
	{
		cnt++;
	}
	return cnt;
}

omCopyGcMemoryPage	*objectPageCache::getMemoryPage( size_t size )
{
	omCopyGcMemoryPage *page;
	EnterCriticalSection( &lock );
	if ( !freePages )
	{
		LeaveCriticalSection( &lock );
		throw errorNum::scMEMORY;
	}
	page = freePages;
	freePages = freePages->next;


	size = ((size + 1) + (defaultPageSize - 1)) & ~(defaultPageSize - 1);

	if ( page->data )
	{
		if ( size != page->size )
		{
			// need to free it... we need a non-default size;
			HeapFree( heap, HEAP_NO_SERIALIZE, page->data );
			page->data = (uint8_t *)HeapAlloc( heap, 0, size );
		}
	} else
	{
		//printf ( "freePageCount = %i\r\n", nFreePages() );
		//printf ( "allocate memory 1  %i\r\n", ctr++ );
		page->data = (uint8_t *)HeapAlloc( heap, 0, size );
	}

	if ( !page->data )
	{
		// can't complete the request... free all our pages back into our private heap and see if we can complete
		for ( auto it = freePages; it; it = it->next )
		{
			if ( it->data )
			{
				HeapFree( heap, HEAP_NO_SERIALIZE, it->data );
				it->data = 0;
			}
		}
		page->data = (uint8_t *)HeapAlloc( heap, 0, size );
		if ( !page->data )
		{
			// really can't complete :(
			page->next = freePages;
			freePages = page;
			LeaveCriticalSection( &lock );
			throw errorNum::scMEMORY;
		}
	}
	LeaveCriticalSection( &lock );

	page->prev = 0;
	page->next = 0;
	page->used = 0;
	page->lockCount = 0;
	page->freeAble = false;
	page->left = size;
	page->size = size;

#ifdef MURPHY
	memset ( page->data, 0xFF, size );

	page->guardEnd = 0;
	page->guardStart = 0;

	page->addGuard ();
#endif

	return page;
}

void objectPageCache::freeMemoryPage( omCopyGcMemoryPage	*page )
{
	EnterCriticalSection( &lock );
	page->next = freePages;
	freePages = page;

	// only keep pages witht he default size
	if ( page->size != defaultPageSize )
	{
		HeapFree( heap, HEAP_NO_SERIALIZE, page->data );
		page->data = 0;
	}
	LeaveCriticalSection( &lock );
}

class objectPageCache *pageCache;

static size_t total_alloc = 0;

void omCopyGcMemoryPage::addGuard ()
{
	OM_GUARD	*og;

	og = (OM_GUARD*)(data + used);

	memcpy ( og->beginPad, PAD_BYTES, NUM_PAD_BYTES );
	memcpy ( og->endPad, PAD_BYTES, NUM_PAD_BYTES );
	og->next = 0;
	og->ctr = 0;

	if ( !guardStart )
	{
		guardStart = og;
	} else
	{
		guardEnd->next = og;
	}
	guardEnd = og;

	used += sizeof ( OM_GUARD );
	left -= sizeof ( OM_GUARD );
}

void omCopyGcMemoryPage::validateGuard ( void )
{
	OM_GUARD	*og;
	OM_GUARD	*ogPrev;

	guardCounter++;
	og = guardStart;
	ogPrev = 0;

	while ( og )
	{
		ogPrev = og;

		// ensure no loops;
		if ( og->ctr == guardCounter)
		{
			throw errorNum::scINTERNAL;
		}
		og->ctr = guardCounter;

		if ( strncmp ( (char const *)og->beginPad, PAD_BYTES, NUM_PAD_BYTES ) != 0 )
		{
			throw errorNum::scINTERNAL;
		}
		if ( strncmp ( (char const *)og->endPad, PAD_BYTES, NUM_PAD_BYTES ) != 0 )
		{
			throw errorNum::scINTERNAL;
		}

		og = og->next;
	}
	if ( guardEnd != ogPrev )
	{
		throw errorNum::scINTERNAL;
	}
}

omCopyGcMemoryPage::omCopyGcMemoryPage( )
{
	prev = 0;
	next = 0;
	data = 0;
	used		= 0;
	lockCount	= 0;
	freeAble	= false;
	size		= 0;
	left		= 0;
}

omCopyGc::omCopyGc ( class vmInstance *instance ) : instance ( instance )
{
}

omCopyGc::~omCopyGc()
{
	for ( auto it = pages; it; )
	{
		auto itNext = it->next;
		freeMemoryPage ( it );
		it = itNext;
	}
	pages = 0;
	lastPage= 0;

	for ( auto it = lockedPages; it; )
	{
		auto itNext = it->next;
		freeMemoryPage ( it );
		it = itNext;
	}
	lockedPages = 0;
}
void omCopyGc::checkMemory ( void )
{
	for ( auto it = pages; it; it = it->next )
	{
		it->validateGuard ();
	}
}

void omCopyGc::reset ( void )
{
	for ( auto it = pages; it; )
	{
		auto itNext = it->next;
		freeMemoryPage ( it );
		it = itNext;
	}

	for ( auto it = lockedPages; it; )
	{
		auto itNext = it->next;
		freeMemoryPage ( it );
		it = itNext;
	}
	lockedPages = 0;

	pages = getMemoryPage ( 1 );
	lastPage = pages;

	wantCollection			= false;
	disableCollection		= 0;
	collectionCount			= 0;
	pagesSinceCollection	= 0;
	destructorList			= 0;
	lockedPages				= 0;

}

omCopyGcMemoryPage *omCopyGc::detachPage ( void *ptr )
{
	for ( auto it = pages; it; it = it->next )
	{
		if ( ((unsigned char *)ptr >= it->data) && ((unsigned char *)ptr < it->data + it->size) )
		{
			if ( it->prev )
			{
				it->prev->next = it->next;
			} else pages = it->next;
			if ( it->next )
			{
				it->next->prev = it->prev;
			}
			return it;
		}
	}
	return nullptr;
}

void omCopyGc::collectionDisable ( void )
{
	disableCollection++;
}

/* NOTE: this must NOT cause a collection to occur... the caller MUST have valid pointers after this AS LONG
		 as no other OM calls occur
*/

void omCopyGc::collectionEnable ( void )
{
	assert ( disableCollection );
	if ( !--disableCollection && wantCollection )
	{
		collect ();
	}
}

bool omCopyGc::inPage ( void const *ptr )
{
	#ifdef MURPHY_GUARD
		checkMemory ();
	#endif

	for ( auto it = lockedPages; it; it = it->next )
	{
		if ( ((unsigned char *)ptr >= it->data) && ((unsigned char *)ptr < it->data + it->size) )
		{
			return true;
		}
	}
	for ( auto it = pages; it; it = it->next )
	{
		if ( ((unsigned char *)ptr >= it->data) && ((unsigned char *)ptr < it->data + it->size) )
		{
			return true;
		}
	}
	return false;
}

bool omCopyGc::inOldPage ( void const *ptr )
{
	#ifdef MURPHY_GUARD
		checkMemory ();
	#endif

	for ( auto it = oldPages; it; it = it->next )
	{
		if ( ((unsigned char *)ptr >= it->data) && ((unsigned char *)ptr < it->data + it->size) )
		{
			return true;
		}
	}

	return false;
}

omCopyGcMemoryPage *omCopyGc::getMemoryPage ( size_t size )
{
	omCopyGcMemoryPage *page= new omCopyGcMemoryPage;
	size = (size + (OM_PAGE_SIZE_DEFAULT - 1)) & ~(OM_PAGE_SIZE_DEFAULT - 1);

	page->data = (uint8_t *)malloc ( size );
	page->left			= size;
	page->size			= size;

	return page;
}

void omCopyGc::freeMemoryPage ( omCopyGcMemoryPage *page )
{
	pageCache->freeMemoryPage ( page );
}

bool omCopyGc::isInGCMemory ( void *ptr )
{
	return inPage ( ptr );
}

void omCopyGc::registerDestructor( VAR *obj )
{
	omCopyGcDestructorInfo	*dest;

	_ASSERT( obj->type == slangType::eOBJECT );

	GRIP grip( instance );

	dest = (omCopyGcDestructorInfo *)alloc( sizeof( omCopyGcDestructorInfo ) );

	dest->next = destructorList;
	destructorList = dest;

	dest->obj = obj;
	dest->shouldDestroy = false;
}

char *omCopyGc::strDup ( char const *str )
{
	assert ( disableCollection );
	size_t   len;
	char	*res;
	
	len = strlen ( str );

	try {
		res = (char *)alloc ( sizeof ( char ) * ( len + 1 ) );
	} catch ( ... )
	{
		throw;
	}
	
	memcpy ( res, str, len + 1 );
	
	return res;
}

VAR *omCopyGc::move ( vmInstance *instance, VAR **srcPtr, VAR *val, bool copy, objectMemory::collectionVariables *col )
{
	return _move ( val, copy, 0 );
}

VAR *omCopyGc::_move ( VAR *val, bool copy, int depth )

{
	void			*newPtr;
	VAR				*newVal;
	VAR				*tmpVal;
	size_t			 loop;

	if ( !val ) return 0;

	if ( val->type == slangType::eMOVED )
	{
		while ( val->type == slangType::eMOVED )
		{
			val = val->dat.ref.v;
		}
		return  val;
	}

	if ( copy )
	{
		switch ( val->type )
		{
			case slangType::eOBJECT:
				// handle objects' that point to the middle of an object
				if ( val != val[1].dat.ref.v )
				{
					size_t offset = val[1].dat.ref.v - val;
					newVal = _move ( val[1].dat.ref.v, true, depth );
					return  &newVal[offset];
				}
				{
					auto numVars = val->dat.obj.classDef->numVars;
					newVal = allocVar ( numVars );
					for ( size_t loop = 0; loop < numVars; loop++ )
					{
						newVal[loop] = val[loop];
						val[loop].type = slangType::eMOVED;
						val[loop].dat.ref.v = &newVal[loop];
					}
				}
				break;
			case slangType::eARRAY_FIXED:
				{
					auto numElems = (size_t) (val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1) + 1;
					newVal = allocVar ( numElems );	// allocate array_fixed + elements
					for ( size_t loop = 0; loop < numElems; loop++ )
					{
						newVal[loop] = val[loop];
						val[loop].type = slangType::eMOVED;
						val[loop].dat.ref.v = &newVal[loop];
					}
				}
				break;
			case slangType::eCODEBLOCK:
				{
					auto numElems = val->dat.cb.cb->nSymbols - val->dat.cb.cb->nParams + 1;
					newVal = allocVar ( numElems );
					for ( size_t loop = 0; loop < numElems; loop++ )
					{
						newVal[loop] = val[loop];
						val[loop].type = slangType::eMOVED;
						val[loop].dat.ref.v = &newVal[loop];
					}
				}
				break;
			default:
				newVal = allocVar ( 1 );

				*newVal = *val;
				val->type = slangType::eMOVED;
				val->dat.ref.v = (VAR *) newVal;
				break;
		}
	} else
	{
		newVal = val;							// object itself is not moving, just it's data 
	}
	
	if ( (++depth > 1024) && (TYPE ( newVal ) != slangType::eNULL) )
	{
		/* we must be carefull about massive recursion... 
			if we reach a stack depth that is to great... need to stop recursing...
			to do this we create a linked list of objects that still need to be fixed up
			(that is... we copy the structure, but the elements within the var need to
			be fixed up... we will go back and do this at the end)		   
		*/

		collectionUpdateList.push_back ( newVal );

		return ( newVal );
	}
	
	switch ( newVal->type )
	{
		case slangType::eATOM:
//			instance->atoms->markInUse (  newVal->dat.atom );
			break;
		case slangType::eSTRING:
			if ( newVal->dat.str.c )
			{
				newPtr = alloc ( newVal->dat.str.len  + 1 );
				memcpy ( (unsigned char *)newPtr, newVal->dat.str.c, newVal->dat.str.len + 1 );
				newVal->dat.str.c = (char *)newPtr;
			}
			break;
		case slangType::eARRAY_ROOT:
		case slangType::eOBJECT_ROOT:
		case slangType::eCODEBLOCK_ROOT:
		case slangType::eREFERENCE:
			if ( newVal->dat.ref.v )
			{
				newVal->dat.ref.obj = _move ( newVal->dat.ref.obj, true, depth );
				newVal->dat.ref.obj = _move ( newVal->dat.ref.v, true, depth );
			}
			break;
		case slangType::eOBJECT:
			{
				auto classDef = newVal->dat.obj.classDef;

				// move all the elements... objects are not to be moved, they are self or inherited objects that are just fundamental, moving them would cause infinite recursion
				for ( uint32_t loop = 1; loop < classDef->numVars; loop++ ) // start with one to skip our own object
				{
					if ( newVal[loop].type != slangType::eOBJECT )
					{
						_move ( &newVal[loop], false, depth );
					}
				}

				// execute any callbacks
				for ( uint32_t loop = classDef->numVars; loop; loop-- ) // start with one to skip our own object
				{
					if ( newVal[loop - 1].type == slangType::eOBJECT )
					{
						// see if we have to move any cargo
						if ( newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB )
						{
							VAR obj;
							obj.type = slangType::eOBJECT_ROOT;
							obj.dat.ref.v = &newVal[loop - 1];
							obj.dat.ref.obj = obj.dat.ref.v;

							objectMemory::collectionVariables tmp;

							auto resultSave = instance->result;
							newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB ( instance, (VAR_OBJ *) &obj, &tmp );
							instance->result = resultSave;
						}
					}
				}
			}
			break;
			
		case slangType::eCODEBLOCK:
			newPtr = (char *) alloc ( newVal->dat.cb.cb->storageSize );
			memcpy ( newPtr, newVal->dat.cb.cb, newVal->dat.cb.cb->storageSize );
			newVal->dat.cb.cb = (vmCodeBlock *) newPtr;

			for ( uint32_t loop = 0; loop < (unsigned)(newVal->dat.cb.cb->nSymbols - newVal->dat.cb.cb->nParams); loop++ )
			{
				_move ( &newVal[loop + 1], false, depth );
			}
			break;
			
		case slangType::eARRAY_SPARSE:
			if ( newVal->dat.aSparse.v )
			{
				newVal->dat.aSparse.v = _move ( newVal->dat.aSparse.v, true, depth );
			}
			if ( newVal->dat.aSparse.lastAccess )
			{
				newVal->dat.aSparse.lastAccess = _move ( newVal->dat.aSparse.lastAccess, true, depth );
			}
			break;
			
		case slangType::eARRAY_ELEM:
			val = newVal;
			while ( val )
			{
				if ( val->dat.aElem.var )
				{
					val->dat.aElem.var  = _move ( val->dat.aElem.var, true, depth );
				}
				if ( val->dat.aElem.next )
				{
					tmpVal = allocVar ( 1 );
					*tmpVal	= *(val->dat.aElem.next);
						
					val->dat.aElem.next->type  = slangType::eMOVED;
					val->dat.aElem.next->dat.ref.v = tmpVal;
						
					val->dat.aElem.next        = tmpVal;
				}
				val	= val->dat.aElem.next;
			}
			break;
			
		case slangType::eARRAY_FIXED:
			for ( loop = 0; loop < static_cast<size_t>(newVal->dat.arrayFixed.endIndex - newVal->dat.arrayFixed.startIndex + 1); loop++ )
			{
				_move ( &newVal[loop + 1], false, depth );
			}
			break;
		case slangType::ePCALL: // just do atoming for in-use
//			instance->atomList[newVal->dat.pCall.func->pCall.atom - 1].inUse = 1;
		default:
			break;
   }

   return newVal;
}

bool omCopyGc::murphyStatus ( bool enabled )
{
	bool	ret;

	ret = murphyEnabled;
	murphyEnabled = enabled;
	return ret;
}

bool omCopyGc::isValidVar ( void const *ptr )
{
	ptrdiff_t ptr1;
	ptrdiff_t ptr2;
	
	if ( inPage ( ptr ) )
	{
		return true;
	}
	
	ptr1 = (ptrdiff_t) ptr;

	if ( (ptr1 >= (ptrdiff_t) this->instance->eval) && (ptr1 < (ptrdiff_t)this->instance->stack ) )
	{
		return true;
	}

	// check any co-routine stacks
	for ( auto coIt = instance->coRoutines.begin (); coIt != instance->coRoutines.end (); coIt++ )
	{
		VAR			*topOfStack;
		ptr2 = (ptrdiff_t) (*coIt)->getStack ( &topOfStack );

		if ( (ptr1 >= ptr2) && (ptr1 < (ptrdiff_t)topOfStack) )
		{
			return true;
		}
	}


	if ( ptr == &instance->result )
	{
		return true;
	}
	return false;
}

void omCopyGc::_check ( VAR *val )

{
	unsigned int	 loop;

	if ( !val ) return;

	if ( !isValidVar( val ) )
	{
		throw errorNum::scINTERNAL;
	}
	
	if ( val->type == slangType::eMOVED )
	{
		throw errorNum::scINTERNAL;
	}
	
	if ( TYPE ( val ) > eMAXTYPE )
	{
		throw errorNum::scINTERNAL;
	}
	
	switch ( TYPE ( val ) )
	{
		case slangType::eSTRING:
			if ( !inPage ( val->dat.str.c ) )
			{
				// we have no indicator if this is a static (in dseg) or heap allocated string so we can't test
//				throw errorNum::scINTERNAL;
				return;
			}
			break;
		case slangType::eCODEBLOCK:
			for ( loop = 0; loop < val->dat.cb.cb->nSymbols - val->dat.cb.cb->nParams; loop++ )
			{
				_check ( &val[loop + 1] );
			}
			break;

		case slangType::eARRAY_ROOT:
		case slangType::eOBJECT_ROOT:
		case slangType::eCODEBLOCK_ROOT:
			_check ( val->dat.ref.v );
			break;
		case slangType::eREFERENCE:
			if ( val->dat.ref.v )
			{
				if ( val->type == slangType::eREFERENCE )
				{
					if ( TYPE ( val->dat.ref.v ) == slangType::eREFERENCE )
					{
						throw errorNum::scINTERNAL;
					}
					if ( val->dat.ref.v == &instance->result  )
					{
						return;
					}
				}
				_check ( val->dat.ref.v );
			}
			break;

		case slangType::eOBJECT:
			for ( loop = 2; loop < val->dat.obj.classDef->numVars; loop++ )
			{
				switch ( val[loop].type )
				{
					case slangType::eOBJECT:
						loop++;
						break;
					default:
						_check ( &val[loop] );
						break;
				}
			}
			break;
			

		case slangType::eARRAY_SPARSE:
			if ( val->dat.aSparse.v)
			{
				val->type = slangType::eNULL;
				
				_check ( val->dat.aSparse.v );
				val->type = slangType::eARRAY_SPARSE;
			}
			if ( val->dat.aSparse.lastAccess )
			{
				val->type = slangType::eNULL;
				
				_check ( val->dat.aSparse.lastAccess );
				val->type = slangType::eARRAY_SPARSE;
			}
			break;
		case slangType::eARRAY_ELEM:
			while ( val )
			{
				_check ( val->dat.aElem.var );
				val = val->dat.aElem.next;
			}
			break;

		case slangType::eARRAY_FIXED:
			if ( !inPage ( val ) )
			{
				throw errorNum::scINTERNAL;
			}
			// this is to prevent circular recursion
			val->type = slangType::eNULL;
			for ( loop = 0; loop < (unsigned int)(val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1); loop++ )
			{
				_check ( &val[loop + 1] );
			}
			val->type = slangType::eARRAY_FIXED;
			break;
		default:
			break;
	}
}

static void checkRef ( VAR *var )
{
	switch ( TYPE ( var ) )
	{
		case slangType::eARRAY_SPARSE:
		case slangType::eARRAY_FIXED:
		case slangType::eOBJECT:
		case slangType::eMOVED:
			// these must NOT exist on the stack... these types must always be in collectable memory.
			throw errorNum::scINTERNAL;
		default:
			return;
	}
}

void omCopyGc::checkAll ( void )
{
#ifdef MURPHY_GUARD
	checkMemory ();
#endif
	if ( IsThreadAFiber ( ) )
	{
		// collect any co-routine stacks
		for ( auto it : instance->coRoutines )
		{
			VAR *stack;
			VAR *topOfStack;

			stack = it->getStack ( &topOfStack );

			while ( stack < topOfStack )
			{
				switch ( stack->type )
				{
					case slangType::eNULL:
					case slangType::eLONG:
					case slangType::eDOUBLE:
					case slangType::eATOM:
					case slangType::ePCALL:
						break;
					default:
						checkRef ( stack );
						_check ( stack );
						break;
				}
				stack++;
			}
		}
	} else
	{
		for ( auto var = instance->eval + 1; var < instance->stack; var++ )	// first is just a buffer... never collect it
		{
			switch ( var->type )
			{
				case slangType::eNULL:
				case slangType::eLONG:
				case slangType::eDOUBLE:
				case slangType::eATOM:
				case slangType::ePCALL:
					break;
				default:
					_check ( var );
					break;
			}
		}
	}

	checkRef ( &instance->result );
	_check ( &instance->result );

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   for ( size_t loop = 0; loop < image->nGlobals; loop++ )
									   {
										   _check ( image->globals[loop] );
									   }
									   return false;
								   }
	);
}

void omCopyGc::collect ( size_t P )
{
	wantCollection = true;
}

void omCopyGc::collect ( )
{
	assert ( !disableCollection );
	total_alloc = 0;

	disableCollection++;

	/* null out the update list*/
	collectionUpdateList.clear();

	auto page = getMemoryPage ( 1 );

	if ( !page ) throw errorNum::scMEMORY;

	oldPages	= pages;

#ifdef MURPHY
	checkAll();

	for ( auto it = pages; it; it = it->next )
	{
		it->check();
	}

	/* every pass we increment the starting offset to make sure all pointers actually change */
	murphyPtrRotate = (murphyPtrRotate + 1) & 127;

	oldPages	= pages;

	page->used	+= murphyPtrRotate;
	page->left	-= murphyPtrRotate;
#else
	oldPages	= pages;
#endif

	
#ifdef MURPHY_GUARD
	page->addGuard();
#endif

	pages = page;
	lastPage = page;

	for ( auto var = instance->eval; var < instance->stack; var++ )
	{
		_move ( var, false, 0 );
	}

	// collect any co-routine stacks
	for ( auto coIt = instance->coRoutines.begin(); coIt != instance->coRoutines.end(); coIt++ )
	{
		VAR			*stack;
		VAR			*topOfStack;

		stack = (*coIt)->getStack ( &topOfStack );
		while ( stack < topOfStack )
		{
			_move ( stack, false, 0 );
			stack++;
		}
	}

	_move ( &instance->result, false, 0 );

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   for ( size_t loop = 0; loop < image->nGlobals; loop++ )
									   {
										   image->globals[loop] = _move ( image->globals[loop], true, 0 );
									   }
									   return false;
								   }
	);

	/* process any stack recursion busters */
	for ( auto updateIt = collectionUpdateList.begin(); updateIt != collectionUpdateList.end(); updateIt++ )
	{
		_move ( (*updateIt), true, 0 );
	}

	// process any destructors, move them if still alive
	moveDestructorList();
	processDestructors();
#ifdef MURPHY
	checkAll();

	for ( auto it = pages; it; it = it->next )
	{
		it->check();
	}
#endif
	for ( auto it = oldPages; it; )
	{
		auto itNext = it->next;
		if ( it->lockCount )
		{
			if ( lockedPages )
			{
				lockedPages->prev = it;
			}
			it->next = lockedPages;
			it->prev = 0;
			lockedPages = it;

			it->freeAble = true;
		} else
		{
			freeMemoryPage( it );
		}
		it = itNext;
	}

	wantCollection = false;
	disableCollection++;
	pagesSinceCollection	= 0;
}

#define MK_FP(seg,ofs)  ((void )(((unsigned long)(seg) << 16) | (unsigned)(ofs)))

size_t omCopyGc::memoryInfo ( size_t num )
{
	size_t	size;

	switch ( num )
	{
		case 0:
			collect ( 0 );
		case 1:
			// how much are we using?
			size = 0;
			for ( auto it = pages; it; it = it->next )
			{
				size += it->used;
			}
			return ( size );
		case 2:
			// how much memory until next collection (aggregat across all pages)
			size = 0;
			for ( auto it = pages; it; it = it->next )
			{
				size += it->left;
			}
			return ( size );
		case 3:
			// how many pages
			size = 0;
			for ( auto it = pages; it; it = it->next )
			{
				size++;
			}
			return ( size );
	}
	return 0;
}

/*
This method itereates through the destructor list.
1.	if an object pointed to by a list entry has been moved, the destructor is still valid so add it back to a newly created list
2.  if it has not been moved, the object is no longer alive.
a.	if it is STILL an object than a manual destructor has not been called and so must be added to the pending desturctor list
b.  if it is no longer an object than a manual destructor has already been issued so no more work needs be done
*/

void omCopyGc::moveDestructorList ( void )
{
	omCopyGcDestructorInfo	*dest;
	omCopyGcDestructorInfo	*destNew;

	dest = destructorList;

	destructorList = 0;

	while ( dest )
	{
		if ( dest->obj )
		{
			destNew = (omCopyGcDestructorInfo	*) alloc ( sizeof ( omCopyGcDestructorInfo ) );

			if ( dest->obj->type == slangType::eMOVED )
			{
				// have object to be destroyed survive this round
				destNew->obj = dest->obj->dat.ref.v; // location of moved object
				while ( destNew->obj->type == slangType::eMOVED ) destNew->obj = destNew->obj->dat.ref.v;
				destNew->next = destructorList;
				destructorList = destNew;
			} else
			{
				// has not been collected, so must attach to the pending destruction list

				// test to ensure that the object is still valid... if it hasn't than a destructor
				// was manually fired and we no longer need to keep it around
				if ( dest->obj->type == slangType::eOBJECT )
				{
					destNew->obj = dest->obj;
					destNew->next = pendingDestructionList;
					pendingDestructionList = destNew;
				}
			}
		}

		dest = dest->next;
	}
}

void omCopyGc::processDestructors ( void )
{
	// save the result onto the stack incase we collect again;
	*instance->stack = instance->result;
	instance->stack++;
	while ( pendingDestructionList )
	{
		auto item = pendingDestructionList;
		pendingDestructionList = pendingDestructionList->next;
		classCallRelease ( instance, item->obj );
	}
	instance->result = *(--instance->stack);
}

void omCopyGc::checkQuick ( void )
{
	VAR				*var;

	for ( var = instance->eval; var < instance->stack; var++ )
	{
		checkRef ( var );
	}

	checkRef ( &instance->result );
	
	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   for ( size_t loop = 0; loop < image->nGlobals; loop++ )
									   {
										   checkRef ( image->globals[loop] );
									   }
									   return false;
								   }
	);
}

VAR *omCopyGc::allocVar ( size_t count )
{
	return (VAR *)alloc ( sizeof( VAR ) * count );
}

void *omCopyGc::alloc ( size_t len )

{
	uint8_t	*ptr;

	total_alloc += len;
#ifdef MURPHY
	if ( enableCollection )
	{
		if ( !inCollect )
		{
			if ( murphyEnabled )
			{
				collect ( 0 );
			}
		}
	}
#endif

	if ( !pages )
	{
		pages = getMemoryPage ( len );
		lastPage = pages;
	}

	// allocate everyting on 64-bit boundaries for speed... VAR structures are most important and they are all int based.
	if ( (len + GUARD_EXTRA * 2 + OM_BOUNDARY + 1) < lastPage->left )
	{
		ptr = (unsigned char *)((ptrdiff_t)(lastPage->data + lastPage->used + OM_BOUNDARY) & ~OM_BOUNDARY);		// NOLINT(performance-no-int-to-ptr)
		lastPage->left -= len + GUARD_EXTRA * 2 + OM_BOUNDARY;
		lastPage->used += len + GUARD_EXTRA * 2 + OM_BOUNDARY;

#ifdef MURPHY_GUARD
		lastPage->addGuard();
#endif

#ifdef MURPHY
		for ( auto it = pages; it; it = it->next )
		{
			if ( it->used + it->left != it->size ) throw errorNum::scINTERNAL;
		}
		memset ( ptr, 0xFF, len );
#endif
		return ( ptr );
	}

	for ( auto it = pages; it; it = it->next )
	{
		if ( (len + GUARD_EXTRA + OM_BOUNDARY + 1) < it->left )
		{
			lastPage = it;

			ptr = (unsigned char *)((ptrdiff_t)(lastPage->data + lastPage->used + OM_BOUNDARY) & ~OM_BOUNDARY);		// NOLINT(performance-no-int-to-ptr)
			lastPage->left -= len + GUARD_EXTRA * 2 + OM_BOUNDARY;
			lastPage->used += len + GUARD_EXTRA * 2 + OM_BOUNDARY;

#ifdef MURPHY_GUARD
			lastPage->addGuard();
#endif

#ifdef MURPHY
			for ( auto it2 = pages; it2; it2++ )
			{
				if ( it2->used + it2->left != it2->size ) throw errorNum::scINTERNAL;
			}
			memset ( ptr, 0xFF, len );
#endif
			return ( ptr );
		}
	}

	// should we do a garbage collection?
	if ( pagesSinceCollection > OM_NEW_PAGES_BEFORE_COLLECTION)
	{
		wantCollection = true;
	}
	pagesSinceCollection++;

	lastPage = getMemoryPage ( len );

	if ( pages )
	{
		pages->prev = lastPage;
	}
	lastPage->next = pages;
	pages = lastPage;

	ptr = (unsigned char *)((ptrdiff_t)(lastPage->data + lastPage->used + OM_BOUNDARY) & ~OM_BOUNDARY);		// NOLINT(performance-no-int-to-ptr)
	lastPage->left -= len + GUARD_EXTRA * 2 + OM_BOUNDARY;
	lastPage->used += len + GUARD_EXTRA * 2 + OM_BOUNDARY;

#ifdef MURPHY_GUARD
	lastPage->addGuard();
#endif
	return ( ptr );
}

cachedObjectMemory::~cachedObjectMemory()
{
	for ( auto it = pages; it; )
	{
		auto itNext = it->next;
		delete it;
		it = itNext;
	}
	pages = 0;
	lastPage = 0;

	for ( auto it = lockedPages; it; )
	{
		auto itNext = it->next;
		delete it;
		it = itNext;
	}
	lockedPages = 0;
}

omCopyGcMemoryPage	*cachedObjectMemory::getMemoryPage( size_t size )
{
	return pageCache->getMemoryPage( size );
}

void cachedObjectMemory::freeMemoryPage( objectMemoryPage *pageP )
{
	if ( pageP )
	{
		omCopyGcMemoryPage *page = &static_cast<omCopyGcMemoryPage&>(*pageP);
		pageCache->freeMemoryPage ( page );
	}
}
