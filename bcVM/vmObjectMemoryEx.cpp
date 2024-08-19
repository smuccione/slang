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
//#define MURPHY_GUARD
#define MURPHY_CHECK
#endif

#include <stdlib.h>
#include <math.h>
#include "malloc.h"

#include "bcInterpreter\bcInterpreter.h"
#include "compilerBCGenerator\compilerBCGenerator.h"
#include "bcVM\exeStore.h"
#include "bcVM\fglTypes.h"
#include "bcVM\vmAtom.h"
#include "bcVM\vmInstance.h"
#include "bcVM\bcVM.h"
#include "bcVM\vmObjectMemory.h"

#ifdef MURPHY_GUARD
#define GUARD_EXTRA	(sizeof ( OM_GUARD ))
#else
#define GUARD_EXTRA 0
#endif	

static char *pseudoAlloc ( char **ptr, uint32_t size )
{
	char *ret;
	ret = *ptr;
	(*ptr) += size;

	return ret;
}

void objectMemoryPage::addGuard ()

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

void objectMemoryPage::validateGuard ( void )
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
			throw scINTERNAL;
		}
		og->ctr = guardCounter;

		if ( memcmp ( og->beginPad, PAD_BYTES, NUM_PAD_BYTES ) )
		{
			throw scINTERNAL;
		}
		if ( memcmp ( og->endPad, PAD_BYTES, NUM_PAD_BYTES ) )
		{
			throw scINTERNAL;
		}

		og = og->next;
	}
	if ( guardEnd != ogPrev )
	{
		throw scINTERNAL;
	}
}

objectMemoryPage::objectMemoryPage( uint32_t pageSize )
{
	data		= 0;
	data		= new unsigned char[pageSize];
	if ( !data ) throw scMEMORY;
	size		= pageSize;
	left		= pageSize;
	used		= 0;
	lockCount	= 0;
	freeAble	= false;

#ifdef MURPHY
	memset ( data, 0xFF, sizeof ( *data ) * size );

	guardEnd	= 0;
	guardStart	= 0;

	addGuard();
#endif
}

objectMemoryPage::~objectMemoryPage()
{
	if ( data ) 
		delete[] data;
}

objectMemory::objectMemory ( class atomTable *atoms, uint32_t size, INSTANCE *instance )
{
	this->atoms				= atoms;
	this->instance			= instance;

	defaultPageSize			= size;
	inCollect				= false;
	enableCollection		= true;
	collectionCount			= 0;
	pagesSinceCollection	= 0;
	pagesSinceVarCollection	= 0;
	destructorList			= 0;
#ifdef MURPHY
	murphyPtrRotate			= 0;
#endif
	pages		= new std::list<objectMemoryPage *>;
	lockedPages = new std::list<objectMemoryPage *>;

	pages->push_front ( new objectMemoryPage ( defaultPageSize ) );

	lastPage = pages->front();

	varTables.push_back ( new varTable ( VAR_TABLE_SIZE, collectionCount ) );
}

objectMemory::~objectMemory()
{
	std::list<objectMemoryPage *>::iterator it;

	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		delete *it;
	}

	for ( it = lockedPages->begin(); it != lockedPages->end(); it++ )
	{
		delete *it;
	}

	delete pages;
	delete lockedPages;
}
void objectMemory::checkMemory ( void )

{
	std::list<objectMemoryPage *>::iterator	 it;

	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		(*it)->validateGuard();
	}
}

void objectMemory::reInit ( void )

{
	std::list<objectMemoryPage *>::iterator		 it;
	std::list<VAR *>::iterator					 dlIt;

	while ( pages->size() != 1 )
	{
		delete pages->front();
		pages->pop_front();
	}

	while ( lockedPages->size() != 1 )
	{
		delete lockedPages->front();
		lockedPages->pop_front();
	}

	inCollect				= false;
	enableCollection		= true;
	collectionCount			= 0;
	pagesSinceCollection	= 0;
	destructorList			= 0;

	lastPage = pages->front();
}

unsigned char *objectMemory::detatchPage ( void *ptr )

{
	unsigned char								*data;
	std::list<objectMemoryPage *>::iterator		 it;
	objectMemoryPage							*page;

	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		page = *it;

		if ( ((unsigned char *)ptr >= page->data) && ((unsigned char *)ptr < page->data + page->size) )
		{
			data = page->data;
			page->data = 0;
			delete page;
			pages->remove ( *it );
			return data;
		}
	}
	throw scINTERNAL;
}

void objectMemory::collectionDisable ( void )

{
	enableCollection = true;
}

/* NOTE: this must NOT cause a collection to occure... the caller MUST have valid pointers after this AS LONG
		 as no other OM calls occur
*/

void objectMemory::collectionEnable ( void )

{
	enableCollection = false;
}

void objectMemory::lockPage ( void *ptr )

{
	std::list<objectMemoryPage *>::iterator		 it;

	#ifdef MURPHY_GUARD
		checkMemory ();
	#endif

	for ( it = lockedPages->begin(); it != lockedPages->end(); it++ )
	{
		if ( ((unsigned char *)ptr >= (*it)->data) && ((unsigned char *)ptr < (*it)->data + (*it)->size) )
		{
			(*it)->lockCount++;
			return;
		}
	}
	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		if ( ((unsigned char *)ptr >= (*it)->data) && ((unsigned char *)ptr < (*it)->data + (*it)->size) )
		{
			(*it)->lockCount++;
			return;
		}
	}
}

void objectMemory::unlockPage ( void *ptr )

{
	std::list<objectMemoryPage *>::iterator		 it;

	#ifdef MURPHY_GUARD
		checkMemory ();
	#endif

	for ( it = lockedPages->begin(); it != lockedPages->end(); it++ )
	{
		if ( ((unsigned char *)ptr >= (*it)->data) && ((unsigned char *)ptr < (*it)->data + (*it)->size) )
		{
			(*it)->unlockPage();
			return;
		}
	}
	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		if ( ((unsigned char *)ptr >= (*it)->data) && ((unsigned char *)ptr < (*it)->data + (*it)->size) )
		{
			(*it)->unlockPage();
			return;
		}
	}
}

bool objectMemory::inPage ( void *ptr )

{
	std::list<objectMemoryPage *>::iterator		 it;

	#ifdef MURPHY_GUARD
		checkMemory ();
	#endif

	for ( it = lockedPages->begin(); it != lockedPages->end(); it++ )
	{
		if ( ((unsigned char *)ptr >= (*it)->data) && ((unsigned char *)ptr < (*it)->data + (*it)->size) )
		{
			return true;
		}
	}
	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		if ( ((unsigned char *)ptr >= (*it)->data) && ((unsigned char *)ptr < (*it)->data + (*it)->size) )
		{
			return true;
		}
	}
	return false;
}

bool objectMemory::inVarPage ( VAR *var )
{
	std::list<varTable *>::iterator		 it;

	#ifdef MURPHY_GUARD
		checkMemory ();
	#endif

	for ( it = varTables.begin(); it != varTables.end(); it++ )
	{
		if ( (var >= (*it)->var) && (var < &(*it)->var[(*it)->count]) )
		{
			return true;
		}
	}
	return false;
}

GRIP objectMemory::getGrip ( VAR **var )
{
	return 0;
}

void objectMemory::freeGrip ( GRIP var )
{
	std::list<objGrip *>::iterator		it;

	for ( it = grips.begin(); it != grips.end(); it++ )
	{
		switch ( (*it)->type )
		{
			case objGrip_array:
				if ( (VAR *)(*it)->dat.array.array == (VAR *)var )
				{
					delete (*it)->dat.array.array;
					delete *it;
					grips.remove ( *it );
					return;
				}
				break;
		}
	}
	return;
}

GRIP *objectMemory::getGripArray ( unsigned int count )
{
	objGrip			*grip;

	grip = new objGrip;

	try {
		/* allocate the array */
		grip->dat.array.array = new VAR *[count];

		/* build the grip */
		grip->type            = objGrip_array;
		grip->dat.array.count = count;
		memset ( grip->dat.array.array, 0, sizeof ( VAR * ) * count );

		grips.push_front ( grip );

		return ( grip->dat.array.array );
	} catch ( ... )
	{
		delete grip;
		throw scMEMORY;
	}
}

char *objectMemory::strDup ( const char *str )

{
	int   len;
	char *res;
	
	lockPage ( (unsigned char *)str );
	
#ifdef MURPHY_GUARD
	checkMemory ();
#endif
	
	len = strlen ( str );

	try {
		res = (char *)alloc ( sizeof ( char ) * ( len + 1 ) );
	} catch ( ... )
	{
		unlockPage ( (unsigned char *)str );
		throw;
	}
	
	memcpy ( res, str, len + 1 );
	
	unlockPage ( (unsigned char *)str );
	
	return ( res );
}

void objectMemory::registerDestructor ( VAR *obj )
{
	VAR			*dest;

	_ASSERT ( obj->type == eOBJECT );

	dest = (VAR *)alloc ( sizeof ( VAR ) );
	dest->type = eDESTRUCTOR;

	if ( destructorList )
	{
		destructorList->dat.destructor.prev = dest;
	}

	dest->dat.destructor.obj	= obj;
	dest->dat.destructor.prev	= 0;
	dest->dat.destructor.next	= destructorList;
	destructorList = dest;

	obj->dat.obj->dest = dest;
}

void objectMemory::unregisterDestructor ( VAR *obj )
{
	VAR			*dest;

	_ASSERT ( obj->type == eOBJECT_ROOT );

	dest = obj->dat.v->dat.obj->dest;

	if ( dest )
	{
		if ( destructorList == dest )
		{
			destructorList = dest->dat.destructor.next;
			if ( destructorList )
			{
				destructorList->dat.destructor.prev = 0;
			}
		} else
		{
			if ( dest->dat.destructor.prev )
			{
				dest->dat.destructor.prev->dat.destructor.next = dest->dat.destructor.next;
			}
			if ( dest->dat.destructor.next )
			{
				dest->dat.destructor.next->dat.destructor.prev = dest->dat.destructor.prev;
			}
		}
	}
}

void moveCB ( class objectMemory *om, VAR *val )

{
	om->_move ( val, 0 );
}

void objectMemory::_move ( VAR *val, uint32_t depth )

{
	void			*newPtr;
	VAR				*obj;
	char			*tmpPtr;
	unsigned int	 loop;

	if ( !val ) return;
	if ( val->collCount == collectionCount ) return;

	val->collCount = collectionCount;
	
	if ( (++depth > 1024) && (TYPE ( val ) != eNULL) )
	{
		/* we must be carefull about massive recursion... 
			if we reach a stack depth that is to great... need to stop recursing...
			to do this we create a linked list of objects that still need to be fixed up
			(that is... we copy the structure, but the elements within the var need to
			be fixed up... we will go back and do this at the end)		   
		*/

		collectionUpdateList.push_back ( val );

		return;
	}
	
	switch ( val->type )
	{
		case eATOM:
//			instance->atoms->markInUse (  val->dat.atom );
			break;
		case eSTRING:
			if ( val->dat.str.c )
			{
				newPtr = (VAR *)alloc ( val->dat.str.len  + 1 );
				memcpy ( (unsigned char *)newPtr, val->dat.str.c, val->dat.str.len + 1 );
				val->dat.str.c = (char *)newPtr;
			}
			break;
		case eARRAY_ROOT:
		case eOBJECT_ROOT:
		case eREFERENCE:
			if ( val->dat.v )
			{
				_move ( val->dat.v, depth );
			}
			break;
		case eOBJECT:
			bcClass	*classDef;
			uint32_t	 loop2;

			classDef = val->dat.obj->classDef;

			obj = val;		// not moving any VAR's

			tmpPtr = (char *)alloc ( classDef->allocSize );
			memcpy ( tmpPtr, obj->dat.obj, classDef->allocSize );

			obj->dat.obj = (vmObject *)pseudoAlloc ( &tmpPtr, sizeof ( vmObject ) );

			obj->dat.obj->context		= (VAR **) pseudoAlloc ( &tmpPtr, sizeof ( VAR * ) * classDef->nContext );
			obj->dat.obj->instSym		= (VAR **) pseudoAlloc ( &tmpPtr, sizeof ( VAR * ) * classDef->nLocals );
			obj->dat.obj->vCtxt			= (VAR **) pseudoAlloc ( &tmpPtr, sizeof ( VAR * ) * classDef->nVTable );

			obj->dat.obj->context[0]->collCount = collectionCount;

			// allocate all the base objects
			for ( loop = 1; loop < classDef->nContext; loop++ )
			{
				if ( !classDef->context[loop].virtDestination )
				{
					obj->dat.obj->context[loop]->dat.v->dat.obj					= (vmObject *)pseudoAlloc ( &tmpPtr, sizeof ( vmObject ) );
					obj->dat.obj->context[loop]->dat.v->dat.obj->context		= &(obj->dat.obj->context[loop]);
					obj->dat.obj->context[loop]->dat.v->dat.obj->instSym		= (VAR **)pseudoAlloc ( &tmpPtr, sizeof ( VAR * ) * obj->dat.obj->context[loop]->dat.v->dat.obj->classDef->nLocals );
					obj->dat.obj->context[loop]->dat.v->dat.obj->vCtxt			= &obj->dat.obj->vCtxt[classDef->context[loop].vTableOffset];
					for ( loop2 = 0; loop2 < obj->dat.obj->context[loop]->dat.v->dat.obj->classDef->nLocals; loop2++ )
					{
						_move ( obj->dat.obj->context[loop]->dat.v->dat.obj->instSym[loop2], depth );
					}
				}
			}

			for ( loop = 0; loop < classDef->nLocals; loop++ )
			{
				_move ( obj->dat.obj->instSym[loop], depth );
			}

			if ( val->dat.obj->cargo )
			{
				obj->dat.obj->cargo = alloc ( sizeof ( char ) * val->dat.obj->cargoLen );
				memcpy ( obj->dat.obj->cargo, val->dat.obj->cargo, val->dat.obj->cargoLen);
			}

			if ( val->dat.obj->classDef->cGarbageCollectionCB && val->dat.obj->cargo )
			{
				(val->dat.obj->classDef->cGarbageCollectionCB) ( instance, this, val, moveCB );
			}
			break;
			
		case eCODEBLOCK:
			if ( val->dat.cb.cb && !val->dat.cb.cb->isStatic )
			{
				newPtr = (char *)alloc ( val->dat.cb.cb->storageSize );
				memcpy ( newPtr, val->dat.cb.cb, val->dat.cb.cb->storageSize );
				val->dat.cb.cb = (vmCodeBlock *)newPtr;
			}
			if ( val->dat.cb.detach )
			{
				newPtr = alloc ( sizeof ( VAR * ) * val->dat.cb.cb->nSymbols );
				memcpy ( newPtr, val->dat.cb.detach, sizeof ( VAR * ) * val->dat.cb.cb->nSymbols );

				for ( loop = 0; loop < val->dat.cb.cb->nSymbols; loop++ )
				{
					if ( val->dat.cb.detach[loop] )
					{
						_move ( val->dat.cb.detach[loop], depth );
					}
				}
				val->dat.cb.detach = (VAR **)newPtr;
			}
			break;
			
		case eCLOSURE:
			if ( val->dat.closure.detach )
			{
				newPtr = alloc ( sizeof ( VAR * ) * val->dat.closure.func->nClosures );
				memcpy ( newPtr, val->dat.closure.detach, sizeof ( VAR * ) * val->dat.closure.func->nClosures );
				
				for ( loop = 0; loop < val->dat.closure.func->nClosures; loop++ )
				{
					if ( val->dat.closure.detach[loop] )
					{
						_move ( val->dat.closure.detach[loop], depth );
					}
				}
				val->dat.closure.detach = (VAR **)newPtr;
			}
			break;
			
		case eARRAY_SPARSE:
			if ( val->dat.aSparse.v )
			{
				_move ( val->dat.aSparse.v, depth );
			}
			if ( val->dat.aSparse.lastAccess )
			{
				_move ( val->dat.aSparse.lastAccess, depth );
			}
			break;
			
		case eARRAY_ELEM:
			while ( val )
			{
				if ( val->dat.aElem.var )
				{
					_move ( val->dat.aElem.var, depth );
				}
				val	= val->dat.aElem.next;
			}
			break;
			
		case eARRAY_FIXED:
			if ( val->dat.arrayFixed.elem )
			{
				newPtr = alloc ( sizeof ( VAR * ) * (unsigned)(val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1) );
				memcpy ( newPtr, val->dat.arrayFixed.elem, sizeof ( VAR * ) * (unsigned)(val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1) );
				val->dat.arrayFixed.elem = (VAR **) newPtr;
				
				for ( loop = 0; loop < (unsigned)(val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1); loop++ )
				{
					// this condition is only necessary when BUILDING an array... once built the array of pointers will always point to an eNULL val
					if ( val->dat.arrayFixed.elem[loop] )
					{
						_move ( val->dat.arrayFixed.elem[loop], depth );
					}
				}
			}
			break;
		case ePCALL:
			// just do atoming for in-use
//			instance->atomList[val->dat.pCall.func->pCall.atom - 1].inUse = 1;
			break;
   }
}

void objectMemory::markVar ( VAR *val, uint32_t depth )

{
	VAR				*obj;
	unsigned int	 loop;

	if ( val->collCount == collectionCount ) return;

	val->collCount = collectionCount;
	
	if ( (++depth > 1024) && (TYPE ( val ) != eNULL) )
	{
		/* we must be carefull about massive recursion... 
			if we reach a stack depth that is to great... need to stop recursing...
			to do this we create a linked list of objects that still need to be fixed up
			(that is... we copy the structure, but the elements within the var need to
			be fixed up... we will go back and do this at the end)		   
		*/

		collectionUpdateList.push_back ( val );

		return;
	}

	depth++;
	
	switch ( val->type )
	{
		case eATOM:
//			instance->atoms->markInUse (  val->dat.atom );
			break;
		case eSTRING:
			break;
		case eARRAY_ROOT:
		case eOBJECT_ROOT:
		case eREFERENCE:
			if ( val->dat.v )
			{
				markVar ( val->dat.v, depth );
			}
			break;
		case eOBJECT:
			bcClass	*classDef;
			uint32_t	 loop2;

			classDef = val->dat.obj->classDef;

			obj = val;		// not moving any VAR's
			obj->dat.obj->context[0]->collCount = collectionCount;

			for ( loop = 1; loop < classDef->nContext; loop++ )
			{
				if ( !classDef->context[loop].virtDestination )
				{
					for ( loop2 = 0; loop2 < obj->dat.obj->context[loop]->dat.v->dat.obj->classDef->nLocals; loop2++ )
					{
						markVar ( obj->dat.obj->context[loop]->dat.v->dat.obj->instSym[loop2], depth );
					}
				}
			}

			for ( loop = 0; loop < classDef->nLocals; loop++ )
			{
				markVar ( obj->dat.obj->instSym[loop], depth );
			}
			break;
			
		case eCODEBLOCK:
			if ( val->dat.cb.detach )
			{
				for ( loop = 0; loop < val->dat.cb.cb->nSymbols; loop++ )
				{
					if ( val->dat.cb.detach[loop] )
					{
						markVar ( val->dat.cb.detach[loop], depth );
					}
				}
			}
			break;
			
		case eCLOSURE:
			if ( val->dat.closure.detach )
			{
				for ( loop = 0; loop < val->dat.closure.func->nClosures; loop++ )
				{
					if ( val->dat.closure.detach[loop] )
					{
						markVar ( val->dat.closure.detach[loop], depth );
					}
				}
			}
			break;
			
		case eARRAY_SPARSE:
			if ( val->dat.aSparse.v )
			{
				markVar( val->dat.aSparse.v, depth );
			}
			if ( val->dat.aSparse.lastAccess )
			{
				markVar ( val->dat.aSparse.lastAccess, depth );
			}
			break;
			
		case eARRAY_ELEM:
			while ( val )
			{
				if ( val->dat.aElem.var )
				{
					markVar ( val->dat.aElem.var, depth );
				}
				val	= val->dat.aElem.next;
			}
			break;
			
		case eARRAY_FIXED:
			if ( val->dat.arrayFixed.elem )
			{
				for ( loop = 0; loop < (unsigned)(val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1); loop++ )
				{
					// this condition is only necessary when BUILDING an array... once built the array of pointers will always point to an eNULL val
					if ( val->dat.arrayFixed.elem[loop] )
					{
						markVar ( val->dat.arrayFixed.elem[loop], depth );
					}
				}
			}
			break;
		case ePCALL:
			// just do atoming for in-use
//			instance->atomList[val->dat.pCall.func->pCall.atom - 1].inUse = 1;
			break;
   }
}

bool objectMemory::murphyStatus ( bool enabled )
{
	bool	ret;

	ret = murphyEnabled;
	murphyEnabled = enabled;
	return ret;
}

bool objectMemory::isValidVar ( VAR *var )
{
	if ( inVarPage ( var ) )
	{
		return true;
	}
	
	if ( (var >= instance->__eval__) && (var < (instance->__eval__ + instance->nStack)) )
	{
		return true;
	}
	if ( var == &instance->__result__ )
	{
		return true;
	}
	return false;
}

void objectMemory::_check ( VAR *val )

{
	unsigned int	 loop;

	if ( !val ) return;

	if ( !isValidVar( val ) )
	{
		throw scINTERNAL;
	}
	
	if ( val->type == eMOVED )
	{
		throw scINTERNAL;
	}
	
	if ( TYPE ( val ) > eMAXTYPE )
	{
		throw scINTERNAL;
	}
	
	switch ( TYPE ( val ) )
	{
		case eSTRING:
			if ( !inPage ( val->dat.str.c ) )
			{
				// we have no indicator if this is a static (in dseg) or heap allocated string so we can't test
//				throw scINTERNAL;
				return;
			}
			break;
		case eCLOSURE:
			if ( val->dat.closure.detach )
			{
				for ( loop = 0; loop < val->dat.closure.func->nClosures; loop++ )
				{
					if ( val->dat.closure.detach[loop] )
					{
						_check ( val->dat.closure.detach[loop] );
					}
				}
			}
			break;
		case eCODEBLOCK:
			if ( val->dat.cb.cb && !val->dat.cb.cb->isStatic )
			{
				if ( inPage ( val->dat.cb.cb ) )
				{
					throw scINTERNAL;
				}
			}
			if ( val->dat.cb.cb && val->dat.cb.detach )
			{
				for ( loop = 0; loop < val->dat.cb.cb->nSymbols; loop++ )
				{
					if ( val->dat.cb.detach[loop] )
					{
						_check ( val->dat.cb.detach[loop] );
					}
				}
			}
			break;

		case eARRAY_ROOT:
		case eOBJECT_ROOT:
			_check ( val->dat.v );
			break;
		case eREFERENCE:
			if ( val->dat.v )
			{
				if ( val->type == eREFERENCE )
				{
					if ( TYPE ( val->dat.v ) == eREFERENCE )
					{
						throw scINTERNAL;
					}
					if ( (val->dat.v == &instance->__result__ ) )
					{
						throw scINTERNAL;
					}
				}
				_check ( val->dat.v );
			}
			break;

		case eOBJECT:
			if ( val->dat.obj )
			{
				if ( !inPage ( val->dat.obj ) )
				{
					throw scINTERNAL;
				}
				
				if ( val->dat.obj->instSym )
				{
					if ( !inPage ( val->dat.obj->instSym ) )
					{
						throw scINTERNAL;
					}
					
					for ( loop = 0; loop < val->dat.obj->classDef->nLocals; loop++ )
					{
						_check ( val->dat.obj->instSym[loop] );
					}
				}
			}
			break;
			

		case eARRAY_SPARSE:
			if ( val->dat.aSparse.v)
			{
				val->type = eNULL;
				
				_check ( val->dat.aSparse.v );
				val->type = eARRAY_SPARSE;
			}
			if ( val->dat.aSparse.lastAccess )
			{
				val->type = eNULL;
				
				_check ( val->dat.aSparse.lastAccess );
				val->type = eARRAY_SPARSE;
			}
			break;
		case eARRAY_ELEM:
			while ( val )
			{
				_check ( val->dat.aElem.var );
				val = val->dat.aElem.next;
			}
			break;

		case eARRAY_FIXED:
			if ( val->dat.arrayFixed.elem )
			{
				if ( !inPage ( val->dat.arrayFixed.elem ) )
				{
					throw scINTERNAL;
				}

				val->type = eNULL;
				
				for ( loop = 0; loop < (unsigned int)(val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1); loop++ )
				{
					if ( val->dat.arrayFixed.elem[loop] ) 
					{
						_check ( val->dat.arrayFixed.elem[loop] );
					}
				}
				val->type = eARRAY_FIXED;
			}
			break;
	}
}

static void _checkRef ( VAR *var )

{
	switch ( TYPE ( var ) )
	{
		case eARRAY_SPARSE:
		case eARRAY_FIXED:
		case eOBJECT:
			// these must NOT exist on the stack... these types must alwasy be in collectable memory.
			throw scINTERNAL;
		default:
			return;
	}
}

void objectMemory::checkAll ( void )

{
	std::list<objGrip *>::iterator	gripIt;
	atom							*pAtm;
	VAR								*var;
	uint32_t						 loop;

#ifdef MURPHY_GUARD
	checkMemory ();
#endif

	for ( var = instance->__eval__; var < instance->stack; var++ )
	{
		_checkRef ( var );
		_check ( var );
	}

	_checkRef ( &instance->__result__ );
	_check ( &instance->__result__ );

	for ( gripIt = grips.begin(); gripIt != grips.end(); gripIt++ )
	{
		switch ( (*gripIt)->type )
		{
			case objGrip_array:
				for ( loop = 0; loop < (*gripIt)->dat.array.count; loop++ )
				{
					_check ( (*gripIt)->dat.array.array[loop] );
				}
				break;
		}
	}
	
	pAtm = atoms->used;
	while ( pAtm )
	{
		if ( pAtm->type == aLOADIMAGE )
		{
			for ( loop = 0; loop < pAtm->loadImage->nGlobals; loop++ )
			{
				_check ( pAtm->loadImage->globals[loop] );
			}
		}
		pAtm = pAtm->next;
	}
}

void objectMemory::collectVar ( void )
{
	std::list<varTable *>::iterator			 varIt;
	std::list<struct VAR *>::iterator			 updateIt;
	std::list<objectMemoryPage *>::iterator		 it;
	std::list<objGrip *>::iterator				 gripIt;
	atom										*pAtm;
	uint32_t									 loop;
	VAR											*var;

	collectionCount++;
	if ( collectionCount > 4 ) collectionCount = 1;

	/* null out the update list*/
	collectionUpdateList.empty();

	for ( var = instance->__eval__; var < instance->stack; var++ )
	{
		markVar ( var, 0 );
	}

	markVar ( &instance->__result__, 0 );


	for ( gripIt = grips.begin(); gripIt != grips.end(); gripIt++ )
	{
		switch ( (*gripIt)->type )
		{
			case objGrip_array:
				for ( loop = 0; loop < (*gripIt)->dat.array.count; loop++ )
				{
					if ( (*gripIt)->dat.array.array[loop] )
					{
						markVar ( (*gripIt)->dat.array.array[loop], 0 );
					}
				}
				break;
		}
	}

	pAtm = atoms->used;
	while ( pAtm )
	{
		if ( pAtm->type == aLOADIMAGE )
		{
			for ( loop = 0; loop < pAtm->loadImage->nGlobals; loop++ )
			{
				markVar ( pAtm->loadImage->globals[loop], 0 );
			}
		}
		pAtm = pAtm->next;
	}

	/* process any stack recursion busters */
	for ( updateIt = collectionUpdateList.begin(); updateIt != collectionUpdateList.end(); updateIt++ )
	{
		markVar ( (*updateIt ), 0 );
	}

	for ( varIt = varTables.begin(); varIt != varTables.end(); varIt++ )
	{
		(*varIt)->genFree ( collectionCount );
	}
}

void objectMemory::collect ( void )
{
	std::list<varTable *>::iterator			 varIt;
	std::list<struct VAR *>::iterator			 updateIt;
	std::list<objectMemoryPage *>::iterator		 it;
	std::list<objGrip *>::iterator				 gripIt;
	objectMemoryPage							*page;
	atom										*pAtm;
	uint32_t									 loop;
	VAR											*var;

	if ( !enableCollection ) return;

	collectionCount++;
	if ( collectionCount > 4 ) collectionCount = 1;

	/* null out the update list*/
	collectionUpdateList.empty();

	inCollect = true;

	page = new objectMemoryPage ( defaultPageSize );

#ifdef MURPHY
	checkAll();

	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		(*it)->check();
	}

	/* every pass we increment the starting offset to make sure all pointers actually change */
	murphyPtrRotate = (murphyPtrRotate + 1) & 127;

	oldPages	= pages;
	pages		= new std::list<objectMemoryPage *>;

	page->used	+= murphyPtrRotate;
	page->left	-= murphyPtrRotate;
#else
	oldPages	= pages;
	pages		= new std::list<objectMemoryPage *>;
#endif

	pages->push_front ( page );
	
#ifdef MURPHY_GUARD
	page->addGuard();
#endif

	lastPage = page;

	for ( var = instance->__eval__; var < instance->stack; var++ )
	{
		_move ( var,0 );
	}

	_move ( &instance->__result__, 0 );


	for ( gripIt = grips.begin(); gripIt != grips.end(); gripIt++ )
	{
		switch ( (*gripIt)->type )
		{
			case objGrip_array:
				for ( loop = 0; loop < (*gripIt)->dat.array.count; loop++ )
				{
					if ( (*gripIt)->dat.array.array[loop] )
					{
						_move ( (*gripIt)->dat.array.array[loop], 0 );
					}
				}
				break;
		}
	}

	pAtm = atoms->used;
	while ( pAtm )
	{
		if ( pAtm->type == aLOADIMAGE )
		{
			for ( loop = 0; loop < pAtm->loadImage->nGlobals; loop++ )
			{
				_move ( pAtm->loadImage->globals[loop], 0 );
			}
		}
		pAtm = pAtm->next;
	}

	/* process any stack recursion busters */
	for ( updateIt = collectionUpdateList.begin(); updateIt != collectionUpdateList.end(); updateIt++ )
	{
		_move ( (*updateIt), 0 );
	}


	// process any destructors, move them if still alive

	processDestructors();
#ifdef MURPHY
	checkAll();

	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		(*it)->check();
	}
#endif  

	for ( it = oldPages->begin(); it != oldPages->end(); it++ )
	{
		if ( (*it)->lockCount )
		{
			lockedPages->push_front ( (*it) );
			(*it)->freeAble = true;
		} else
		{
			memset ( (*it)->data, 0xFE, (*it)->size );
			delete ( *it );
		}
	}
	delete oldPages;

	for ( varIt = varTables.begin(); varIt != varTables.end(); varIt++ )
	{
		(*varIt)->genFree ( collectionCount );
	}

	inCollect				= 0;
	pagesSinceCollection	= 0;
}

#define MK_FP(seg,ofs)  ((void )(((unsigned long)(seg) << 16) | (unsigned)(ofs)))

long objectMemory::memoryInfo ( unsigned int num )

{
	std::list<objectMemoryPage *>::iterator		 it;
	uint32_t										 size;

	switch ( num )
	{
		case 0:
			collect();
		case 1:
			// how much are we using?
			size = 0;
			for ( it = pages->begin(); it != pages->end(); it++ )
			{
				size += (*it)->used;
			}
			return ( size );
		case 2:
			// how much are we using?
			size = 0;
			for ( it = pages->begin(); it != pages->end(); it++ )
			{
				size += (*it)->used;
			}
			return ( size );
	}
	return 0;
}

void objectMemory::processDestructors ( void )
{
	VAR				*dest;
	VAR				*destNext;

	dest = destructorList;
	destructorList = 0;

	while ( dest )
	{
		destNext = dest->dat.destructor.next;
		// only var's that have been moved still exist... all others need to be destroyed
		if ( TYPE ( dest->dat.destructor.obj ) == eMOVED )
		{
			// add the moved object into the destructor list.. it's still alive
			if ( (TYPE ( dest->dat.destructor.obj->dat.v ) == eOBJECT) && (dest->dat.destructor.obj->dat.v->dat.obj->dest == dest ) )
			{
				registerDestructor ( dest->dat.destructor.obj->dat.v );
			}
		} else if ( dest->dat.destructor.obj )
		{
			classCallRelease ( instance, dest->dat.destructor.obj );
		}
		dest = destNext;
	}
}

void objectMemory::checkQuick ( void )

{
	unsigned int	 loop;
	atom			*pAtm;
	VAR				*var;

	for ( var = instance->__eval__; var < instance->stack; var++ )
	{
		_checkRef ( var );
	}

	_checkRef ( &instance->__result__ );
	
	pAtm = atoms->used;
	while ( pAtm )
	{
		if ( pAtm->type == aLOADIMAGE )
		{
			for ( loop = 0; loop < pAtm->loadImage->nGlobals; loop++ )
			{
				_checkRef ( pAtm->loadImage->globals[loop] );
			}
		}
	}
}

VAR *objectMemory::var ( uint32_t count )
{
	std::list<varTable *>::iterator	 it;
	VAR									*var;
	uint32_t							 loop;

	for ( it = varTables.begin(); it != varTables.end(); it++ )
	{
		if ( (*it)->free )
		{
			var = (*it)->free;
			while ( var )
			{
				if ( var->dat.free.count >= count )
				{
					// allocate off the end so we don't have to 
					var->dat.free.count -= count;
					if ( !var->dat.free.count ) (*it)->free = var->dat.free.next;
					var += var->dat.free.count;
					for ( loop = 0; loop < count; loop++ )
					{
						var[loop].collCount = collectionCount;
					}
					return var;
				}
				var = var->dat.free.next;
			}
		}
	}

	if ( pagesSinceVarCollection > 3 )
	{
		collectVar();
		pagesSinceVarCollection = 0;
		return this->var ( count );
	} 
	pagesSinceVarCollection++;

	// no free entries... allocate a new one
	varTables.push_front ( new varTable ( VAR_TABLE_SIZE + count, collectionCount ) );
	var = varTables.front()->free;
	var->dat.free.count -= count;
	var += var->dat.free.count;
	for ( loop = 0; loop < count; loop++ )
	{
		var[loop].collCount = collectionCount;
	}
	return var;
}

void *objectMemory::alloc ( unsigned int len )

{
	std::list<objectMemoryPage *>::iterator		 it;
	unsigned char								*ptr;
	unsigned int								 boundary;

#ifdef MURPHY
	if ( enableCollection )
	{
		if ( !inCollect )
		{
			if ( murphyEnabled )
			{
				collect();
			}
		}
	}
#endif

	// allocate everyting on 64-bit boundaries for speed... VAR structures are most important and they are all int based.
	boundary = 3;

	if ( (len + GUARD_EXTRA * 2 + boundary + 1) < lastPage->left )
	{
		ptr = (unsigned char *)((int)(lastPage->data + lastPage->used + boundary) & ~boundary);
		lastPage->left -= len + GUARD_EXTRA * 2 + boundary;
		lastPage->used += len + GUARD_EXTRA * 2 + boundary;

#ifdef MURPHY_GUARD
		lastPage->addGuard();
#endif

#ifdef MURPHY
		for ( it = pages->begin(); it != pages->end(); it++ )
		{
			if ( (*it)->used + (*it)->left != (*it)->size ) throw scINTERNAL;
		}
		memset ( ptr, 0xFF, len );
#endif
		return ( ptr );
	}

	for ( it = pages->begin(); it != pages->end(); it++ )
	{
		if ( (len + GUARD_EXTRA + boundary + 1) < (*it)->left )
		{
			lastPage = (*it);

			ptr = (unsigned char *)((int)(lastPage->data + lastPage->used + boundary) & ~boundary);
			lastPage->left -= len + GUARD_EXTRA * 2 + boundary;
			lastPage->used += len + GUARD_EXTRA * 2 + boundary;

#ifdef MURPHY_GUARD
			lastPage->addGuard();
#endif

#ifdef MURPHY
			for ( it = pages->begin(); it != pages->end(); it++ )
			{
				if ( (*it)->used + (*it)->left != (*it)->size ) throw scINTERNAL;
			}
			memset ( ptr, 0xFF, len );
#endif
			return ( ptr );
		}
	}

	// should we do a garbage collection?
	if ( (pagesSinceCollection < 16) || !enableCollection || inCollect )
	{
		pagesSinceCollection++;

		lastPage = new objectMemoryPage ( len > defaultPageSize ? len + defaultPageSize : defaultPageSize );
		pages->push_front ( lastPage );

		ptr = (unsigned char *)((int)(lastPage->data + lastPage->used + boundary) & ~boundary);
		lastPage->left -= len + GUARD_EXTRA * 2 + boundary;
		lastPage->used += len + GUARD_EXTRA * 2 + boundary;

#ifdef MURPHY_GUARD
		lastPage->addGuard();
#endif
		return ( ptr );
	}

	collect();

	inCollect = true;

	ptr = (unsigned char *)alloc ( len );

	inCollect = false;

	if ( !ptr ) throw scMEMORY;

	return ( ptr );
}
