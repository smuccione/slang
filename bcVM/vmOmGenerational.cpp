/*

      Generational Garbage Collector - Slang

	  Theory of Operation:
			Gen 0 is the ephemral generation, all new  objects are created here
			Gen 0 may FAIL an allocation request, this is the trigger for garbage collection
			Gen 1, 2 may NOT fail an allocation request.   Instead, they will always succeed but will set a flag that can be read indicating that they request collection
			Gen 1 items may NEVER point to Gen 0 items
			Gen 2 items may NEVER point to Gen 0 or gen 1 items
			Gen 0 items MAY point to Gen 0, Gen 1 or Gen 2 items
			Gen 1 items may point to Gen 1 or Gen 2 items
			Gen 2 items may only point to Gen 2 items

			During collection
				Gen 2 is checked to see if it needs collection, if so a FULL collection is done.
					full:
						walk is started from roots
						objects are aged and promoted as necessary
					partial
						card table is walked
							if a reference into gen 0 exists object is promoted into gen 2
							if a reference into gen 1 exists object is promoted into gen 2, gen 1 is scheduled for a full collection
						gen1 is then checked to see if it needs collection:
							full:
								walk is started from roots
								objects are aged and promoted as necessary
								walk is halted at gen 2 objects
							partial:
								card table is walked
									if a reference exists to Gen 0, the element is promoted to Gen 1
								gen 0
									wakk is started from roots
									objects are aged and promoted as necessray
									walk is halted at gen 1 or gen 2 entries
					
			after collection
				destructor list is walked, any objects marked for destruction have their release method called.
*/

#include "bcVM.h"
#include "bcVM/vmOmGenerational.h"

#ifdef MURPHY_GUARD
#define GUARD_EXTRA	(sizeof ( OM_GUARD ))
#else
#define GUARD_EXTRA 0
#endif	

#define OM_BOUNDARY  3

static objectAgeMap defaultMap[16] =	{
											{ 0, 0 },					// 000
											{ 0, 1 },					// 010
											{ 1, 0 },					// 100
											{ 2, 0 },					// 110  
											{ 2, 0 },					// 110
										};

static objectAgeRoundPadMap defaultAgeRoundMap[] =	{
														{ 0, 0, 0 },
														{ 0, 1, 1 },
														{ 1, 0, 2 },
														{ 2, 0, 3 },
													};

omGenGc::omGenGc ( class vmInstance *instance ) : instance ( instance )
{
	memcpy( ageRoundMap, defaultMap, sizeof( ageRoundMap ) );

	generation.push_back( new omGenGcGeneration( false ) );		// no card table needed for gen 0
	generation.push_back( new omGenGcGeneration( true ) );			// gen 1 - use card table
	generation.push_back( new omGenGcGeneration( true ) );			// gen 2 - use card table

	setPadding( 4, defaultAgeRoundMap );
}

omGenGc::~omGenGc()
{
	for ( auto it = generation.begin(); it != generation.end(); it++ )
	{
		delete (*it);
	}
}

void omGenGc::reset ( void )
{
	for ( auto it = generation.begin(); it != generation.end(); it++ )
	{
		(*it)->reset();
	}
	wantCollection			= false;
	disableCollection		= 0;

	destructorList			= 0;
	roots.clear();
}

objectMemoryPage *omGenGc::detachPage ( void *ptr )
{
	auto age = getAge ( (char const *)ptr );
	auto gen = generation[age];
	return gen->detachPage ( ptr );
}

void omGenGc::collectionDisable( void )
{
	disableCollection++;
}

void omGenGc::collectionEnable ( void )
{
#if defined ( MURPHY )
	wantCollection = true;
#endif
	assert ( disableCollection );
	if ( !--disableCollection && wantCollection )
	{
		instance->stack->type = slangType::eDEBUG_MARKER;
		collect ();
	}
}

bool omGenGc::inPage ( void *ptr )
{
	for ( auto it = generation.begin(); it != generation.end(); it++ )
	{
		if ( (*it)->inPage( ptr ) ) return true;
	}
	return false;
}

void omGenGc::registerDestructor ( VAR *obj )
{
	omGenGcDestructorInfo	*dest;

	_ASSERT ( obj->type == slangType::eOBJECT );

	GRIP grip( instance );

	dest = (omGenGcDestructorInfo *)allocGen( sizeof( omGenGcDestructorInfo ), 0 );

	dest->next = destructorList;
	destructorList = dest;

	dest->obj = obj;
	dest->shouldDestroy = false;
}

bool omGenGc::isValidVar ( void *ptr )
{
	ptrdiff_t ptr1;
	ptrdiff_t ptr2;
	
	if ( inPage ( ptr ) )
	{
		return true;
	}
	
	ptr1 = (ptrdiff_t) ptr;

	ptr2 = (ptrdiff_t) this->instance->eval;
	if ( (ptr1 >= ptr2) && (ptr1 < ptr2 + (ptrdiff_t)(sizeof ( VAR ) * TASK_EVAL_STACK_SIZE) ) )
	{
		return true;
	}
	
	if ( ptr == &instance->result )
	{
		return true;
	}
	return false;
}


uint64_t omGenGc::memoryInfo ( size_t num )
{
	size_t	size;

	switch ( num )
	{
		case 0:
			collect( 0 );
		// TODO: report accurate numbers!!!
		case 1:			// how much are we using?
		case 2:			// how much memory until next collection (aggregat across all pages)
		case 3:			// how many pages
			size = 0;
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

void omGenGc::moveDestructorList( size_t stopGen )
{
	omGenGcDestructorInfo	*dest;
	omGenGcDestructorInfo	*destNew;

	dest = destructorList;
	
	destructorList = 0;

	while ( dest )
	{
		if ( dest->obj )
		{
			destNew = (omGenGcDestructorInfo	*)allocGen( sizeof( omGenGcDestructorInfo ), 0 );

			if ( dest->obj->type == slangType::eMOVED )
			{
				// have object to be destroyed survive this round
				destNew->obj = dest->obj->dat.ref.v; // location of moved object
				destNew->next = destructorList;
				destructorList = destNew;
			} else
			{
				// has not been collected, so must attach to the pending destruction list

				// test to ensure that the object is still valid... if it hasn't than a destructor
				// was manually fired and we no longer need to keep it around
				if ( dest->obj->type == slangType::eOBJECT )
				{
					size_t age = getAge ( dest->obj );
					if ( age >= stopGen )
					{
						// not collected so no knowledge regarding liveness.  keep until next time
						destNew->obj = dest->obj->dat.ref.v; // location of moved object
						destNew->next = destructorList;
						destructorList = destNew;
					} else
					{
						destNew->obj = dest->obj;
						destNew->next = pendingDestructionList;
						pendingDestructionList = destNew;
					}
				}
			}
		}

		dest = dest->next;
	}
}

void omGenGc::processDestructors ( void )
{
	// save the result onto the stack incase we collect again;
	*instance->stack = instance->result;
	instance->stack++;
	while ( pendingDestructionList )
	{
		auto item = pendingDestructionList;
		pendingDestructionList = pendingDestructionList->next;
		instance->stack = instance->eval;
		try
		{
			classCallRelease ( instance, item->obj );
		} catch ( ... )
		{
		}
	}
	instance->result = *(--instance->stack);
#if defined ( _DEBUG )
	instance->stack->type = slangType::eDEBUG_MARKER;
#endif
}

void omGenGc::freeMemoryPage( objectMemoryPage *pageP )
{
	if ( pageP )
	{
		omGenGCMemoryPage	*page = &static_cast<omGenGCMemoryPage &>(*pageP);
		omGenGcPageCache::free ( page );
	}
}

bool omGenGc::isRoot( VAR *var )
{
	if ( IsThreadAFiber() )
	{
		for ( auto coIt = instance->coRoutines.begin(); coIt != instance->coRoutines.end(); coIt++ )
		{
			VAR			*stack;
			VAR			*topOfStack;

			stack = (*coIt)->getStack( &topOfStack );
			if ( (var >= stack) && (var <= topOfStack) ) return true;
		}
	} else
	{
		if ( (var >= instance->eval) && (var <= instance->stack) ) return true;
	}
	if ( var == &instance->result ) return true;

	return false;
}

#if defined ( MURPHY_CHECK )
void omGenGc::checkAge ( void *ptr, size_t age )
{
	return;
	if ( !generation[age]->inPage ( ptr ) )
	{
		throw errorNum::scINTERNAL;
	}
	if ( generation[age]->inOldPage ( ptr ) )
	{
		throw errorNum::scINTERNAL;
	}
}
void omGenGc::checkAge ( VAR *val, size_t age, size_t round )
{
	return;
	if ( !generation[age]->inPage ( val ) )
	{
		throw errorNum::scINTERNAL;
	}
	if ( generation[age]->inOldPage ( val ) )
	{
		throw errorNum::scINTERNAL;
	}
}

void omGenGc::check ( VAR *val, bool root )
{
	if ( !val ) return;

	size_t age;
	size_t round;

	if ( root )
	{
		age = 0;
		round = 0;
	} else
	{
		age = getAge ( val );
		round = getRound ( val );

		checkAge ( val, age );
	}

	switch ( val->type )
	{
		case slangType::eMOVED:
		case slangType::eDEBUG_MARKER:
			assert ( 0 );
			throw errorNum::scINTERNAL;
		case slangType::eATOM:
			//			instance->atoms->markInUse (  val->dat.atom );
			break;
		case slangType::eSTRING:
			assert ( val->dat.str.c );
			if ( !root )
			{
//				if ( getAge ( val->dat.str.c ) != age ) 
//					throw errorNum::scINTERNAL;
			}
			break;
		case slangType::eCODEBLOCK_ROOT:
			if ( val->dat.ref.v )
			{
				assert ( val->dat.ref.obj->type == val->dat.ref.v->type );
				assert ( val->dat.ref.v->type == slangType::eARRAY_FIXED || val->dat.ref.v->type == slangType::eARRAY_SPARSE );
				if ( isRoot ( val->dat.ref.v ) )
				{
					// don't copy it, it's pointing to a root which can never move.. it will get collected in it's own right, don't double collect
				} else
				{
					if ( getAge ( val->dat.ref.obj ) < age || getAge ( val->dat.ref.v ) < age ) throw errorNum::scINTERNAL;
					check ( val->dat.ref.obj, false );
					//					check ( val->dat.ref.v, false );
				}
			}
			break;
		case slangType::eARRAY_ROOT:
			if ( val->dat.ref.v )
			{
				assert ( val->dat.ref.obj->type == val->dat.ref.v->type );
				assert ( val->dat.ref.v->type == slangType::eARRAY_FIXED || val->dat.ref.v->type == slangType::eARRAY_SPARSE );
				if ( isRoot ( val->dat.ref.v ) )
				{
					// don't copy it, it's pointing to a root which can never move.. it will get collected in it's own right, don't double collect
				} else
				{
					if ( getAge ( val->dat.ref.obj ) < age || getAge ( val->dat.ref.v ) < age )
					{
						throw errorNum::scINTERNAL;
					}
					check ( val->dat.ref.obj, false );
//					check ( val->dat.ref.v, false );
				}
			}
			break;
		case slangType::eOBJECT_ROOT:
			if ( val->dat.ref.v )
			{
				assert ( val->dat.ref.obj->type == val->dat.ref.v->type );
				assert ( val->dat.ref.v->type == slangType::eNULL || val->dat.ref.v->type == slangType::eOBJECT );
				if ( isRoot ( val->dat.ref.v ) )
				{
					// don't copy it, it's pointing to a root which can never move.. it will get collected in it's own right, don't double collect
				} else
				{
					if ( getAge ( val->dat.ref.obj ) < age || getAge ( val->dat.ref.v ) < age ) throw errorNum::scINTERNAL;
					check ( val->dat.ref.obj, false );
//					check ( val->dat.ref.v, false );
				}
			}
			break;
		case slangType::eREFERENCE:
			if ( val->dat.ref.v )
			{
				assert ( val->dat.ref.obj->type == val->dat.ref.v->type );
				if ( isRoot ( val->dat.ref.v ) )
				{
					// don't copy it, it's pointing to a root which can never move.. it will get collected in it's own right, don't double collect
				} else
				{
					if ( getAge ( val->dat.ref.obj ) < age || getAge ( val->dat.ref.v ) < age ) throw errorNum::scINTERNAL;
					check ( val->dat.ref.obj, false );
//					check ( val->dat.ref.v, false );
				}
			}
			break;
		case slangType::eOBJECT:
			{
				bcClass *classDef;

				val = val[1].dat.ref.v;				// always start at the most derived
				classDef = val->dat.obj.classDef;

				// see if we have to move any cargo
				if ( val[0].dat.obj.cargo )
				{
					checkAge ( val[0].dat.obj.cargo, age );
				}
				if ( val[0].dat.obj.classDef->cCheckCB )
				{
	//				val[0].dat.obj.classDef->cCheckCB ( instance, &val[0], checkCB, age );
				}

				// now, any ivars and handle any inherited objects that need gc callbacks or have cargo that need to be dealt with
				for ( uint32_t loop = 1; loop < classDef->numVars; loop++ ) // start with one to skip our own object
				{
					if ( val[loop].type == slangType::eOBJECT )
					{
						// see if we have to move any cargo
						if ( val[loop].dat.obj.cargo )
						{
							checkAge ( val[loop].dat.obj.cargo, age );
						}
						if ( val[loop].dat.obj.classDef->cGarbageCollectionCB )
						{
							// can't do this since the GC callback may allocate non-vars... need specific check code
	//						val[loop].dat.obj.classDef->cGarbageCollectionCB ( instance, &val[loop], moveCB, age );
						}
					} else if ( val[loop].type == slangType::eOBJECT_ROOT && val[loop].dat.ref.v == val )
					{
						// don't follow... we're pointint back to the most derived which IS val and what' we're currently checking
					} else
					{
						check ( &val[loop], false );
					}
				}
			}
			break;
		case slangType::eCODEBLOCK:
			val->type = slangType::eNULL;
			if ( !val->dat.cb.cb->isStatic )
			{
				checkAge ( val->dat.cb.cb, age );
			}
			for ( uint32_t loop = 0; loop < val->dat.cb.cb->nSymbols - val->dat.cb.cb->nParams; loop++ )
			{
				checkAge ( &val[loop + 1], age );
				check ( &val[loop + 1], false );
			}
			val->type = slangType::eCODEBLOCK;
			break;
		case slangType::eARRAY_SPARSE:
			val->type = slangType::eNULL;
			if ( val->dat.aSparse.v )
			{
				if ( getAge ( val->dat.aSparse.v ) < age ) throw errorNum::scINTERNAL;
				check ( val->dat.aSparse.v, false );
			}
			if ( val->dat.aSparse.lastAccess )
			{
				if ( getAge ( val->dat.aSparse.lastAccess ) < age ) throw errorNum::scINTERNAL;
				check ( val->dat.aSparse.lastAccess, false );
			}
			val->type = slangType::eARRAY_SPARSE;
			break;
		case slangType::eARRAY_ELEM:
			age = getAge ( val );
			round = getRound ( val );
			while ( val )
			{
				if ( getAge ( val ) != age ) throw errorNum::scINTERNAL;
				if ( val->dat.aElem.var )
				{
					if ( getAge ( val->dat.aElem.var ) < age ) throw errorNum::scINTERNAL;
					check ( val->dat.aElem.var, false );
				}
				assert ( !val->dat.aElem.next || val->dat.aElem.elemNum < val->dat.aElem.next->dat.aElem.elemNum );
				assert ( val != val->dat.aElem.next );
				val = val->dat.aElem.next;
			}
			break;
		case slangType::eARRAY_FIXED:
			val->type = slangType::eNULL;
			for ( uint32_t loop = 0; loop < (unsigned)(val->dat.arrayFixed.endIndex - val->dat.arrayFixed.startIndex + 1); loop++ )
			{
				check ( &val[loop + 1], false );
			}
			val->type = slangType::eARRAY_FIXED;
			break;
		case slangType::ePCALL:
			// just do atoming for in-use
			//			instance->atomList[val->dat.pCall.func->pCall.atom - 1].inUse = 1;
		case slangType::eLONG:
		case slangType::eDOUBLE:
		case slangType::eNULL:
			break;
		default:
			assert ( 0 );
			throw errorNum::scINTERNAL;
	}
}
#endif

VAR *omGenGc::move ( vmInstance *instance, VAR **srcPtr, VAR *val, bool isRoot, collectionVariables *col )
{
	auto vars = static_cast<genColVars *>(col);
	return move ( srcPtr, val, isRoot, vars->copyObject, vars->age, vars->round );
}

/*
	this routines sole purpose is to move everything into a specific generation.
	val:	the value we've seen and need to keep
	tableWalk: if tablewalk is true, it will conditionally move until age.  if it's false it will force copy values into the new generation
	destGen: the destination generation that we're currently moving to
*/

VAR *omGenGc::move( VAR **srcPtr, VAR *val, bool valRoot, bool doCopy, size_t destGen, size_t destRound )
{
	VAR				*newVal;

	if ( !val ) return 0;

	if ( srcPtr )
	{
		movePatch.push_back ( srcPtr );
	}

	if ( val->type == slangType::eMOVED )
	{
		val = val->dat.ref.v;
		while ( val->type == slangType::eMOVED )
		{
			val = val->dat.ref.v;
		}
		return val;
	}

	if ( valRoot )
	{
		assert ( !srcPtr );
		newVal = val;

		switch ( newVal->type )
		{
			case slangType::eSTRING:
				if ( getAge( (char const *) newVal->dat.str.c ) < destGen )
				{
					auto newPtr = allocGen( newVal->dat.str.len + 1, destGen );
					memcpy( (unsigned char *) newPtr, newVal->dat.str.c, newVal->dat.str.len + 1 );
					newVal->dat.str.c = (char *) newPtr;
				}
				break;
			case slangType::eARRAY_ROOT:
			case slangType::eOBJECT_ROOT:
			case slangType::eCODEBLOCK_ROOT:
			case slangType::eREFERENCE:
				assert ( !(newVal->dat.ref.obj->type == slangType::eNULL && newVal->dat.ref.v->type != slangType::eNULL) );
				if ( newVal->dat.ref.v && (getAge( newVal->dat.ref.v ) < destGen) )
				{
					// it exists in object memory so must be copied
					newVal->dat.ref.obj = move( &newVal->dat.ref.obj(), newVal->dat.ref.obj, false, true, destGen, destRound );
					newVal->dat.ref.v = move( &newVal->dat.ref.v(), newVal->dat.ref.v, false, true, destGen, destRound );
				}
				break;
			case slangType::eARRAY_SPARSE:
				if ( getAge ( newVal->dat.aSparse.v ) < destGen )
				{
					// the FIRST element in the array is not in our generation, move it into our generation and bring any any other
					// new elements until we encounter on that *IS* in our generation
					if ( newVal->dat.aSparse.v->type == slangType::eMOVED )
					{
				 		newVal->dat.aSparse.v = newVal->dat.aSparse.v->dat.ref.v;
					} else
					{
						// allocate a new val in our gen for this element
						auto tmpVal = allocVarGen ( 1, destGen, destRound );
						*tmpVal = *newVal->dat.aSparse.v;

						// mark the old one as moved an point it to our new genereation
						newVal->dat.aSparse.v->type = slangType::eMOVED;
						newVal->dat.aSparse.v->dat.ref.v = tmpVal;

						newVal->dat.aSparse.v = tmpVal;

						// move the value in to our generation.
						if ( newVal->dat.aSparse.v->dat.aElem.var && getAge ( newVal->dat.aSparse.v->dat.aElem.var ) < destGen )
						{
							newVal->dat.aSparse.v->dat.aElem.var = move ( &newVal->dat.aSparse.v->dat.aElem.var(), newVal->dat.aSparse.v->dat.aElem.var, false, true, destGen, destRound );
						}

						// ok we now have a new first element and it has been moved... we need to move any other sparse elements into our generation
						// we can stop when we hit one that's in our generation.   if their children have been changed we will know due to a table walk hit

						// set us to our current element;
						auto prevVal = newVal->dat.aSparse.v;

						auto elemVal = prevVal->dat.aElem.next;
						while ( elemVal && getAge ( elemVal ) < destGen )
						{
							if ( val->type == slangType::eMOVED )
							{
								prevVal->dat.aElem.next = val->dat.ref.v;
								break;
							}

							// allocate a new val in our gen for this element
							tmpVal = allocVarGen ( 1, destGen, destRound );
							*tmpVal = *elemVal;

							// mark the old one as moved an point it to our new genereation
							elemVal->type = slangType::eMOVED;
							elemVal->dat.ref.v = tmpVal;

							//point val to the one that's in our current generation
							elemVal = tmpVal;

							// move the value in to our generation.
							// the value(VAR)'s may be in a higher level, but not aELEM's... after collection they will be in the same level as the aSPARSE  no need to track pointers for updates for elem entries
							if ( elemVal->dat.aElem.var )
							{
								elemVal->dat.aElem.var = move ( &elemVal->dat.aElem.var(), elemVal->dat.aElem.var, false, false, destGen, destRound );
							}

							// point the prior on (in prevVal) to our new value, excange and continue
							prevVal->dat.aElem.next = elemVal;
							prevVal = elemVal;
							elemVal = elemVal->dat.aElem.next;
						}
					}
				}
				if ( newVal->dat.aSparse.lastAccess && (getAge ( newVal->dat.aSparse.lastAccess) < destGen ) )
				{
					newVal->dat.aSparse.lastAccess = move( &newVal->dat.aSparse.lastAccess(), newVal->dat.aSparse.lastAccess, false, false, destGen, destRound );		// this will have been moved, the result is just returned 
				}
				assert ( !newVal->dat.aSparse.lastAccess || newVal->dat.aSparse.lastAccess->type != slangType::eMOVED );
				break;

			case slangType::eARRAY_ELEM:	
				{
					if ( newVal->dat.aElem.var )
					{
						// move our data int our age if it's younger
						if ( getAge ( newVal->dat.aElem.var ) < destGen )
						{
							newVal->dat.aElem.var = move ( &newVal->dat.aElem.var(), newVal->dat.aElem.var, false, false, destGen, destRound );
						}
					}
					// WE (the current) element are already in this age (this is a table walk)... but our NEXT may not be...
					// so we want to potentially move anything we're pointing to (and theyre children) into this generation
					// if we encounter something that's at our age we can stop... further table walks will bring them in;
					auto prevVal = newVal;
					auto elemVal = prevVal->dat.aElem.next;
					while ( elemVal && getAge ( elemVal ) < destGen )
					{
						if ( elemVal->type == slangType::eMOVED )
						{
							prevVal->dat.aElem.next = elemVal->dat.ref.v;
							break;
						}

						// allocate a new val in our gen for this element
						auto tmpVal = allocVarGen ( 1, destGen, destRound );
						*tmpVal = *elemVal;

						// mark the old one as moved an point it to our new genereation
						elemVal->type = slangType::eMOVED;
						elemVal->dat.ref.v = tmpVal;

						//point val to the one that's in our current generation
						elemVal = tmpVal;

						// move the value in to our generation.
						if ( elemVal->dat.aElem.var && getAge ( elemVal->dat.aElem.var ) < destGen )
						{
							elemVal->dat.aElem.var = move ( &elemVal->dat.aElem.var(), elemVal->dat.aElem.var, false, false, destGen, destRound );
						}

						// point the prior on (in prevVal) to our new value, excange and continue
						prevVal->dat.aElem.next = elemVal;
						prevVal = elemVal;
						elemVal = elemVal->dat.aElem.next;
					}
				}
				break;

			case slangType::eOBJECT:
				// NOTE:   any FFI functions that must have a callback during collection MUST touch() the object to ensure that this occurs
				{
					auto classDef = newVal->dat.obj.classDef;
					// age all the elements... objects are not to be moved, they are self or inherited objects that are just fundamental, moving them would cause infinite recursion
					for ( uint32_t loop = 1; loop < classDef->numVars; loop++ ) // start with one to skip our own object
					{
						if ( newVal[loop].type != slangType::eOBJECT )
						{
							move ( nullptr, &newVal[loop], true, false, destGen, destRound );
						}
					}

					// execute any callbacks
					genColVars tmp = genColVars { destGen, destRound,  false, this };
					for ( uint32_t loop = classDef->numVars; loop; loop-- ) // start with one to skip our own object
					{
						if ( newVal[loop - 1].type == slangType::eOBJECT )
						{
							// see if we have to move any cargo
							if ( newVal[loop - 1].dat.obj.cargo && newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB )
							{
								VAR obj = {};
								obj.type = slangType::eOBJECT_ROOT;
								obj.dat.ref.obj = &newVal[loop - 1];
								obj.dat.ref.v = obj.dat.ref.obj;

								auto resultSave = instance->result;
								newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB ( instance, (VAR_OBJ *) &obj, &tmp );
								instance->result = resultSave;
							}
						}
					}
				}
				break;
			case slangType::eCODEBLOCK:
				// nothing to do for codeblock... it's already in memory and any captured values will be processed by normal tablewalk
			default:
				break;
		}
		return newVal;
	} else
	{
		assert ( srcPtr );
	}

	size_t currentAge = getAge( val );

	if ( currentAge > destGen || (currentAge == destGen && !doCopy) )
	{
		return val;
	}

	switch ( val->type )
	{
		case slangType::eOBJECT:
			{
//				assert ( val == val[1].dat.ref.v );

				auto numVars = val->dat.obj.classDef->numVars;
				newVal = allocVarGen ( numVars, destGen, destRound );
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
				newVal = allocVarGen ( numElems, destGen, destRound );	// allocate array_fixed + elements
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
				newVal = allocVarGen ( numElems, destGen, destRound );
				for ( size_t loop = 0; loop < numElems; loop++ )
				{
					newVal[loop] = val[loop];
					val[loop].type = slangType::eMOVED;
					val[loop].dat.ref.v = &newVal[loop];
				}
			}
		default:
			newVal = allocVarGen ( 1, destGen, destRound );

			*newVal = *val;
			val->type = slangType::eMOVED;
			val->dat.ref.v = newVal;
			break;
	}

	switch ( newVal->type )
	{
		case slangType::eATOM:
			//			instance->atoms->markInUse (  newVal->dat.atom );
			break;
		case slangType::eSTRING:
			{
				assert ( newVal->dat.str.c );
				auto newPtr = allocGen ( newVal->dat.str.len + 1, destGen );
				assert ( getAge ( (char const *) newPtr ) == destGen );
				memcpy ( (unsigned char *) newPtr, newVal->dat.str.c, newVal->dat.str.len + 1 );
				newVal->dat.str.c = (char *) newPtr;
			}
			break;
		case slangType::eARRAY_ROOT:
		case slangType::eOBJECT_ROOT:
		case slangType::eCODEBLOCK_ROOT:
		case slangType::eREFERENCE:
			assert ( !(newVal->dat.ref.obj->type == slangType::eNULL && newVal->dat.ref.v->type != slangType::eNULL) );
			if ( newVal->dat.ref.v )
			{
				if ( isRoot ( newVal->dat.ref.v ) )
				{
					// don't copy it, it's pointing to a root which can never move.. it will get collected in it's own right, don't double collect
				} else if ( newVal->dat.ref.v && (getAge ( newVal->dat.ref.v ) < destGen) )
				{
					assert ( !isRoot ( newVal->dat.ref.v ) );
					assert ( !isRoot ( newVal->dat.ref.obj ) );
					newVal->dat.ref.obj = move ( &newVal->dat.ref.obj(), newVal->dat.ref.obj, false, true, destGen, destRound );
					newVal->dat.ref.v = move ( &newVal->dat.ref.v(), newVal->dat.ref.v, false, true, destGen, destRound );
				}
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
						move( nullptr, &newVal[loop], true, false, destGen, destRound );
					}
				}

				// execute any callbacks
				genColVars tmp = genColVars{ destGen, destRound, false, this };
				for ( uint32_t loop = classDef->numVars - 1; loop; loop-- ) // start with one to skip our own object
				{
					if ( newVal[loop - 1].type == slangType::eOBJECT )
					{
						// see if we have to move any cargo
						if ( newVal[loop - 1].dat.obj.cargo && newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB )
						{
							VAR obj = {};
							obj.type = slangType::eOBJECT_ROOT;
							obj.dat.ref.obj = &newVal[loop - 1];
							obj.dat.ref.v = obj.dat.ref.obj;

							newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB ( instance, (VAR_OBJ *) &obj, &tmp );
						}
					}
				}
			}
			break;

		case slangType::eCODEBLOCK:
//			if ( !newVal->dat.cb.cb->isStatic )
			{
				auto newPtr = (char *) allocGen ( newVal->dat.cb.cb->storageSize, destGen );
				memcpy ( newPtr, newVal->dat.cb.cb, newVal->dat.cb.cb->storageSize );
				newVal->dat.cb.cb = (vmCodeBlock *) newPtr;
			}
			for ( uint32_t loop = 0; loop < (unsigned)(newVal->dat.cb.cb->nSymbols - newVal->dat.cb.cb->nParams); loop++ )
			{
				move( nullptr, &newVal[loop + 1], true, true, destGen, destRound );
			}
			break;

		case slangType::eARRAY_SPARSE:
			if ( newVal->dat.aSparse.v )
			{
				newVal->dat.aSparse.v = move( & newVal->dat.aSparse.v(), newVal->dat.aSparse.v, false, false, destGen, destRound );
			}
			if ( newVal->dat.aSparse.lastAccess )
			{
				newVal->dat.aSparse.lastAccess = move( &newVal->dat.aSparse.lastAccess(), newVal->dat.aSparse.lastAccess, false, false, destGen, destRound );		// this will have been moved, the result is just returned 
			}
			break;
		case slangType::eARRAY_ELEM:
			{
				if ( newVal->dat.aElem.var )
				{
					newVal->dat.aElem.var = move ( &newVal->dat.aElem.var(), newVal->dat.aElem.var, false, false, destGen, destRound );
				}

				auto prevVal = newVal;
				auto elemVal = newVal->dat.aElem.next;
				while ( elemVal )
				{
					auto tmpVal = allocVarGen ( 1, destGen, destRound );
					*tmpVal = *elemVal;

					elemVal->type = slangType::eMOVED;
					elemVal->dat.ref.v = tmpVal;
					elemVal = tmpVal;

					prevVal->dat.aElem.next = elemVal;

					if ( elemVal->dat.aElem.var )
					{
						elemVal->dat.aElem.var = move ( &elemVal->dat.aElem.var(), elemVal->dat.aElem.var, false, false, destGen, destRound );
					}

					prevVal = elemVal;
					elemVal = elemVal->dat.aElem.next;
				}
			}
			break;
		case slangType::eARRAY_FIXED:
			for ( uint32_t loop = 0; loop < (unsigned)(newVal->dat.arrayFixed.endIndex - newVal->dat.arrayFixed.startIndex + 1); loop++ )
			{
				move( nullptr, &newVal[loop + 1], true, false, destGen, destRound );
			}
			break;
		case slangType::ePCALL: // just do atoming for in-use
			//			instance->atomList[newVal->dat.pCall.func->pCall.atom - 1].inUse = 1;
		default:
			break;
	}

	return newVal;
}

bool omGenGc::walkCardTable( size_t gen )
{
	bool moved = false;
	for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
	{
		for ( auto vars = generation[gen]->getPages(round); vars; vars = vars->next )
		{
			size_t nPages = vars->fillCardTable();
			for ( size_t page = 0; page < nPages; page++ )
			{
				moved = true;

				size_t nVars = vars->getCardVarCount( page );
				VAR	*tableStart = vars->getCardVar ( page );

				for ( size_t var = 0; var < nVars; var++ )
				{
					// getAge( tableStart[var].dat.ref.v ) >= age
					// this is so that we only move the entity IF and ONLY IF it is younger than us... otherwise there is no reason to move it.
					// we STILL need to process it for collection as it may have other entries that are younger and need to be collected into the older generation
					switch ( tableStart[var].type )
					{
						case slangType::eOBJECT:
							// update any references
							move ( nullptr, &tableStart[var], true, false, gen, 0 );
							var += tableStart[var].dat.obj.classDef->numVars - size_t {1};
							break;
						case slangType::eOBJECT_ROOT:
						case slangType::eARRAY_ROOT:
						case slangType::eCODEBLOCK_ROOT:
						case slangType::eREFERENCE:
							assert ( !isRoot ( tableStart[var].dat.ref.obj ) );
							assert ( !isRoot ( tableStart[var].dat.ref.v ) );
							assert ( !(tableStart[var].dat.ref.obj->type == slangType::eNULL && tableStart[var].dat.ref.v->type != slangType::eNULL) );
							move ( nullptr, &tableStart[var], true, false, gen, 0 );
							break;
						case slangType::eARRAY_SPARSE:
							// you would think that we don't have to do this as elems would all be in the same generation be if we add one after the last collection
							// they could very well be in a younger generation and hence need collecting.  this MAY add additional work for us, as we'll still examine existing
							// array elems but we only have to do that for a maximum of a card table page so there isn't THAT much extra waste.
							if ( tableStart[var].dat.aSparse.v ) move ( nullptr, &tableStart[var], true, false, gen, 0 );
							break;
						case slangType::eARRAY_ELEM:
							assert ( !isRoot ( tableStart[var].dat.aElem.var ) );
							assert ( !isRoot ( tableStart[var].dat.aElem.next ) );
							move ( nullptr, &tableStart[var], true, false, gen, 0 );
							break;
						case slangType::eSTRING:
							if ( getAge( tableStart[var].dat.str.c ) < gen )
							{
								auto save = tableStart[var].dat.str.c;
								tableStart[var].dat.str.c = (char *)allocGen( tableStart[var].dat.str.len + 1, gen );
								memcpy( const_cast<char *>(tableStart[var].dat.str.c), save, tableStart[var].dat.str.len + 1 );
							}
							break;
						case slangType::eCODEBLOCK:
						case slangType::eARRAY_FIXED:
							// nothing to do here... we will collect the detached locals via normal table walk as they aren't referenced by pointer
						default:
							// other types do not have pointers into GC memory and don't need to be collected
							break;
					}
				}
			}
		}
	}
	return moved;
}

void omGenGc::scanGeneration ( size_t gen )
{
	assert ( gen );

	for ( size_t round = 0; round < OM_MAX_ROUNDS; round++ )
	{
		for ( auto page = generation[gen]->getPages(round); page; page = page->next )
		{
			auto var = page->getFirstVar ();
			auto numVars = page->getNumVars ();

			while ( numVars )
			{
				switch ( var->type )
				{
					case slangType::eOBJECT_ROOT:
					case slangType::eARRAY_ROOT:
					case slangType::eCODEBLOCK_ROOT:
					case slangType::eREFERENCE:
						if ( var->dat.ref.v && var->dat.ref.v->type == slangType::eMOVED )
						{
							var->dat.ref.obj = var->dat.ref.obj->dat.ref.v;
							var->dat.ref.v = var->dat.ref.v->dat.ref.v;
							assert ( var->dat.ref.obj->type == var->dat.ref.v->type );
						}
						break;
					case slangType::eARRAY_ELEM:
						if ( var->dat.aElem.var->type == slangType::eMOVED )
						{
							var->dat.aElem.var = var->dat.ref.v;
						}
						break;
					case slangType::eOBJECT:
						// we need to call gcCB's to allow them to adjust any pointers to moved VAR's.
						// the most derived object will ALWAYS come first.   when we hit an object we always simply proceses its vars in reverse to allow the roots to move first
						// and then skip over all derived objects that compose the primary var
						{
							auto classDef = var->dat.obj.classDef;
							// execute any callbacks
							genColVars tmp = genColVars { gen, round, false, this };
							auto resultSave = instance->result;
							for ( uint32_t loop = classDef->numVars; loop; loop-- ) // start with one to skip our own object
							{
								if ( var[loop - 1].type == slangType::eOBJECT )
								{
									// see if we have to move any cargo
									if ( var[loop - 1].dat.obj.cargo && var[loop - 1].dat.obj.classDef->cGarbageCollectionCB )
									{
										VAR obj;
										obj.type = slangType::eOBJECT_ROOT;
										obj.dat.ref.obj = &var[loop - 1];
										obj.dat.ref.v = obj.dat.ref.obj;

										var[loop - 1].dat.obj.classDef->cGarbageCollectionCB ( instance, (VAR_OBJ *) &obj, &tmp );
									}
								}
							}
							instance->result = resultSave;
							var += classDef->numVars;
							numVars -= classDef->numVars;
						}
						break;
					default:
						break;

				}
				numVars--;
				var++;
			}
		}
	}
}

VAR *omGenGc::age( VAR **srcPtr, VAR *val, bool root, size_t destGen, size_t stopGen )
{
	void			*newPtr;
	VAR				*newVal;
	VAR				*tmpVal;

	if ( !val ) return 0;

	if ( srcPtr )
	{
		movePatch.push_back ( srcPtr );
	}

	if ( val->type == slangType::eMOVED )
	{
		assert ( !root );
		val = val->dat.ref.v;
		while ( val->type == slangType::eMOVED )
		{
			val = val->dat.ref.v;
		}
		return val;
	}

	size_t currentAge = getAge( val );
	size_t destAge = destGen;
	size_t destRound = 0;

	if ( root )
	{
		newVal = val;
	} else
	{
		if ( currentAge >= stopGen )
		{
			return val;
		}

		destAge = nextAge ( val );
		destRound = nextRound ( val );

		if ( destAge < destGen )
		{
			destAge = destGen;
			destRound = 0;
		}
		

		// this is part of a full collection, so we do proper ageing

		switch ( val->type )
		{
			case slangType::eOBJECT:
				{
					assert ( val == val[1].dat.ref.v );

					auto numVars = val->dat.obj.classDef->numVars;
					newVal = allocVarGen ( numVars, destAge, destRound );
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
					newVal = allocVarGen ( numElems, destAge, destRound );	// allocate array_fixed + elements
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
					newVal = allocVarGen ( numElems, destGen, destRound );
					for ( size_t loop = 0; loop < numElems; loop++ )
					{
						newVal[loop] = val[loop];
						val[loop].type = slangType::eMOVED;
						val[loop].dat.ref.v = &newVal[loop];
					}
				}
				break;
			default:
				newVal = allocVarGen ( 1, destAge, destRound );

				*newVal = *val;
				val->type = slangType::eMOVED;
				val->dat.ref.v = newVal;
				break;
		}
	}

	switch ( newVal->type )
	{
		case slangType::eATOM:
			//			instance->atoms->markInUse (  newVal->dat.atom );
			break;
		case slangType::eSTRING:
			assert ( newVal->dat.str.c );
			newPtr = allocGen ( newVal->dat.str.len + 1, destAge );
			assert ( getAge ( (char const *) newPtr ) == destAge );
			memcpy ( (unsigned char *) newPtr, newVal->dat.str.c, newVal->dat.str.len + 1 );
			newVal->dat.str.c = (char *) newPtr;
			break;
		case slangType::eARRAY_ROOT:
		case slangType::eOBJECT_ROOT:
		case slangType::eCODEBLOCK_ROOT:
		case slangType::eREFERENCE:
			assert ( !(newVal->dat.ref.obj->type == slangType::eNULL && newVal->dat.ref.v->type != slangType::eNULL) );
			if ( newVal->dat.ref.v )
			{
				assert ( !isRoot ( newVal->dat.ref.v ) );
				assert ( !isRoot ( newVal->dat.ref.obj ) );
				newVal->dat.ref.obj = age ( &newVal->dat.ref.obj(), newVal->dat.ref.obj, false, destAge, stopGen );
				newVal->dat.ref.v = age ( &newVal->dat.ref.v(), newVal->dat.ref.v, false, destAge, stopGen );
			}
			break;
		case slangType::eOBJECT:
			{
				auto classDef = newVal->dat.obj.classDef;
				// age all the elements... objects are not to be moved, they are self or inherited objects that are just fundamental, moving them would cause infinite recursion
				for ( uint32_t loop = 1; loop < classDef->numVars; loop++ ) // start with one to skip our own object
				{
					if ( newVal[loop].type != slangType::eOBJECT )
					{
						age ( nullptr, &newVal[loop], true, destAge, stopGen );
					}
				}

				// execute any callbacks
				genColVars tmp = genColVars{ destAge, destRound, true, this };
				for ( uint32_t loop = classDef->numVars - 1; loop; loop-- ) // start with one to skip our own object
				{
					if ( newVal[loop - 1].type == slangType::eOBJECT )
					{
						// see if we have to age any cargo
						if ( newVal[loop - 1].dat.obj.cargo && newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB )
						{
							VAR obj = {};
							obj.type = slangType::eOBJECT_ROOT;
							obj.dat.ref.obj = &newVal[loop - 1];
							obj.dat.ref.v = obj.dat.ref.obj;

							newVal[loop - 1].dat.obj.classDef->cGarbageCollectionCB ( instance, (VAR_OBJ *) &obj, &tmp );
						}
					}
				}
			}
			break;
		case slangType::eCODEBLOCK:
			{
				auto newPtr = (char *) allocGen ( newVal->dat.cb.cb->storageSize, destGen );
				memcpy ( newPtr, newVal->dat.cb.cb, newVal->dat.cb.cb->storageSize );
				newVal->dat.cb.cb = (vmCodeBlock *) newPtr;
			}
			for ( uint32_t loop = 0; loop < (unsigned)(newVal->dat.cb.cb->nSymbols - newVal->dat.cb.cb->nParams); loop++ )
			{
				age ( nullptr, &newVal[loop + 1], true, destGen, stopGen );
			}
			break;
		case slangType::eARRAY_SPARSE:
			if ( newVal->dat.aSparse.v )
			{
				newVal->dat.aSparse.v = age ( nullptr, newVal->dat.aSparse.v, false, destAge, stopGen );
			}
			if ( newVal->dat.aSparse.lastAccess )
			{
				newVal->dat.aSparse.lastAccess = age ( nullptr, newVal->dat.aSparse.lastAccess, false, destAge, stopGen );		// this will have been moved, the result is just returned 
			}
			break;
		case slangType::eARRAY_ELEM:
			{
				if ( newVal->dat.aElem.var )
				{
					newVal->dat.aElem.var = age ( &newVal->dat.aElem.var(), newVal->dat.aElem.var, false, destAge, stopGen );
				}

				auto prevVal = newVal;
				auto elemVal = newVal->dat.aElem.next;
				while ( elemVal )
				{
					tmpVal = allocVarGen ( 1, destAge, destRound );
					*tmpVal = *elemVal;

					elemVal->type = slangType::eMOVED;
					elemVal->dat.ref.v = tmpVal;
					elemVal = tmpVal;

					prevVal->dat.aElem.next = elemVal;

					if ( elemVal->dat.aElem.var )
					{
						elemVal->dat.aElem.var = age ( &elemVal->dat.aElem.var(), elemVal->dat.aElem.var, false, destAge, stopGen );
					}

				
					prevVal = elemVal;
					elemVal = elemVal->dat.aElem.next;
				}
			}
			break;
		case slangType::eARRAY_FIXED:
			for ( uint32_t loop = 0; loop < (unsigned)(newVal->dat.arrayFixed.endIndex - newVal->dat.arrayFixed.startIndex + 1); loop++ )
			{
				age ( nullptr, &newVal[loop + 1], true, destAge, stopGen );
			}
			break;
		case slangType::ePCALL: // just do atoming for in-use
					 //			instance->atomList[newVal->dat.pCall.func->pCall.atom - 1].inUse = 1;
		default:
			break;
	}

	return newVal;
}

void omGenGc::moveRoots( size_t stopGen )
{
	auto resultSave = instance->result;
	if ( IsThreadAFiber() )
	{
		// collect any co-routine stacks
		for ( auto it : instance->coRoutines )
		{
			VAR			*stack;
			VAR			*topOfStack;

			stack = it->getStack( &topOfStack );

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
						age ( nullptr, stack, true, 0, stopGen );
						break;
				}
				stack++;
			}
		}
	} else
	{
		instance->stack->type = slangType::eDEBUG_MARKER;
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
				case slangType::eSTRING:
				case slangType::eARRAY_ROOT:
				case slangType::eOBJECT_ROOT:
				case slangType::eCODEBLOCK_ROOT:
					// default would catch them... but like these MUST alwasy be aged so just making it explicit
				default:
					age ( nullptr, var, true, 0, stopGen );
					break;
			}
		}
	}
	instance->result = resultSave;

	age ( nullptr, &instance->result, true, 0, stopGen );

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   for ( size_t loop = 0; loop < image->nGlobals; loop++ )
									   {
										   image->globals[loop] = age ( nullptr, image->globals[loop], false, 0, stopGen );
									   }
									   return false;
								   }
	);
}

#ifdef MURPHY_CHECK
void omGenGc::check ( void )
{
	inCheck = true;
	if ( IsThreadAFiber () )
	{
		// collect any co-routine stacks
		for ( auto it : instance->coRoutines )
		{
			VAR			*stack;
			VAR			*topOfStack;

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
						check ( stack, true );
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
					check ( var, true );
					break;
			}
		}
	}

	check ( &instance->result, true );

	instance->atomTable->typeMap ( atom::aLOADIMAGE, [&]( uint32_t index )
								   {
									   auto image = instance->atomTable->atomGetLoadImage ( index );
									   for ( size_t loop = 0; loop < image->nGlobals; loop++ )
									   {
//										   check ( image->globals[loop], true );
									   }
									   return false;
								   }
	);

	inCheck = false;
}
#endif

void omGenGc::collect ( size_t p )
{
	wantCollection = true;
}

void omGenGc::collect ()
{
	LARGE_INTEGER			 start;
	LARGE_INTEGER			 end;
	uint64_t				 collectionTime;

	assert ( !disableCollection );

	QueryPerformanceCounter( &start );

	movePatch.clear ();

	if ( generation[2]->needCollection() )
	{
		// full collection generation 0, 1, 2
		generation[2]->beginCollection();
		generation[1]->beginCollection();
		generation[0]->beginCollection();
		moveRoots( 3 );							//  stop generation being 3 means we will walk everyting
		moveDestructorList( 3 );
		for ( auto &it : movePatch )
		{
			while ( (*it)->type == slangType::eMOVED )
			{
				(*it) = (*it)->dat.ref.v;
			}
		}

#if defined ( MURPHY_CHECK )
		check ();
#endif
		generation[0]->endCollection ();
		generation[1]->endCollection ();
		generation[2]->endCollection ();
	} else
	{
		if ( generation[1]->needCollection() )
		{
			generation[1]->beginCollection();
			generation[0]->beginCollection();
			walkCardTable ( 2 );
			moveRoots( 2 );
			moveDestructorList( 2);
			for ( auto &it : movePatch )
			{
				while ( (*it)->type == slangType::eMOVED )
				{
					(*it) = (*it)->dat.ref.v;
				}
			}

#if defined ( MURPHY_CHECK )
			check ();
#endif
			generation[0]->endCollection ();
			generation[1]->endCollection ();
		} else
		{
			// walk generation 0
			generation[0]->beginCollection ();
			if ( walkCardTable ( 2 ) )
			{
				walkCardTable ( 1 );
				scanGeneration ( 1 );
			} else
			{
				walkCardTable ( 1 );
			}
			moveRoots ( 1 );
			moveDestructorList ( 1 );
			// call any collection callbacks
			for ( auto &it : movePatch )
			{
				while ( (*it)->type == slangType::eMOVED )
				{
					(*it) = (*it)->dat.ref.v;
				}
			}

#if defined ( MURPHY_CHECK )
			check ();
#endif
			generation[0]->endCollection ();
		}
	}

	processDestructors ();

	QueryPerformanceCounter ( &end );

	collectionTime = (end.QuadPart - start.QuadPart);

	totalCollectionTime += collectionTime;
	totalCollections++;

	if ( collectionTime < minCollectionTime ) minCollectionTime = collectionTime;
	if ( collectionTime > maxCollectionTime ) maxCollectionTime = collectionTime;

	wantCollection = false;
}

VAR *omGenGc::allocVar( size_t count )
{
#if defined ( MURPHY )
	assert ( !inCheck );
#endif
	return generation[0]->allocVar ( count, 0, wantCollection );
}

void *omGenGc::alloc( size_t size )
{
#if defined ( MURPHY )
	assert ( !inCheck );
#endif
	return generation[0]->alloc( size, wantCollection );
}

VAR *omGenGc::allocVarGen ( size_t nVars, size_t gen, size_t round )
{
#if defined ( MURPHY )
	assert ( !inCheck );
#endif
	return generation[gen]->allocVar ( nVars, round, wantCollection );
}

void *omGenGc::allocGen ( size_t len, size_t gen )
{
#if defined ( MURPHY )
	assert ( !inCheck );
#endif
	return generation[gen]->alloc ( len, wantCollection );
}

char *omGenGc::strDup( char const *str )
{
	assert ( disableCollection );

	size_t  len;
	char *res;

	len = strlen( str );

	res = (char *)alloc( sizeof( char ) * (len + 1) );

	memcpy( res, str, len + 1 );

	return res;
}

bool omGenGc::isInGCMemory( void *ptr )
{
	return inPage( ptr );
}

void omGenGc::printStats( void )
{
	LARGE_INTEGER	frequency;

	QueryPerformanceFrequency( &frequency );
	frequency.QuadPart = frequency.QuadPart / 1000;

	printf( "\r\nGenerational Garbage Collector\r\n" );
	printf( "   Total Collections: %I64u\r\n", totalCollections );
	printf( "   Total Collection time: %I64u\r\n", totalCollectionTime / frequency.QuadPart );
	printf( "   MAX   Collection time: %I64u\r\n", maxCollectionTime / frequency.QuadPart );
	printf( "   MIN   Collection time: %I64u\r\n", minCollectionTime / frequency.QuadPart );
}
