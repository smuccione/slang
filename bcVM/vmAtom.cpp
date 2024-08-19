/*

     SLANG compiler atom managment

*/

#include "bcVM/bcVM.h"

namespace slangVM
{
	struct instance_
	{
		class vmInstance *instance;
	};
}

void* atom::operator new ( size_t size, class vmInstance *instance )
{
	slangVM::instance_	*ptr;
	ptr = (slangVM::instance_ *) instance->malloc ( size + sizeof ( slangVM::instance_ ) );
	ptr->instance = instance;
	return ptr + 1;
}
void atom::operator delete ( void *p, class vmInstance *instance )
{
	operator delete ( p );
}
void atom::operator delete ( void *p )
{
	vmInstance *instance = ((slangVM::instance_ *)p)[-1].instance;
	instance->free ( &((slangVM::instance_ *)p)[-1] );
}
void *atom::operator new[] ( size_t size, class vmInstance *instance )
{
	return operator new ( size, instance );
}
void atom::operator delete[] ( void *p )
{
	return operator delete ( p );
}

atomTable::atomTable ( uint32_t nAtoms, vmInstance *instance ) : instance ( instance )
{
//	nAtoms = 8192;
	assert ( nAtoms > 1 );
#ifdef _DEBUG
	table = (atom **)instance->calloc ( sizeof ( atom * ), nAtoms );	// NOLINT (bugprone-sizeof-expression)
#else
	table = (atom **) instance->malloc ( sizeof ( atom * ) * nAtoms );	// NOLINT (bugprone-sizeof-expression)
#endif
	memset ( table, 0, sizeof ( atom * ) * nAtoms );	// NOLINT (bugprone-sizeof-expression)
	size = nAtoms;
	used = 0;
	this->instance = instance;
	memset ( typeList, 0, sizeof ( typeList ) );
}

atomTable::~atomTable()
{
	atom	*atm;
	atom	*next;

	for ( atm = this->used; atm; )
	{
		next = atm->next;
		delete atm;
		atm = next;
	}
	for ( atm = this->freeList; atm; )
	{
		next = atm->next;
		delete atm;
		atm = next;
	}
	instance->free ( table );
}

atom::~atom()
{
	release ();
}

void atom::release ()
{
	if ( isDeleteable )
	{
		switch ( type )
		{
			case atom::atomType::aLOADIMAGE:
				delete loadImage;
				break;
			case atom::atomType::aCLASSDEF:
				delete classDef;
				break;
			case atom::atomType::aFUNCDEF:
				delete funcDef;
				break;
			default:
				break;
		}
	}
	type = atom::atomType::aNOT_DEFINED;
	localExecuteTime = 0;
	totalExecuteTime = 0;
	nCalls = 0;
	useCount = 0;
}

void atomTable::reset ( void )
{
	atom	*atm;
	atom	*next;
		
	atm		= used;
	used	= 0;
	memset ( typeList, 0, sizeof ( typeList ) );

	while ( atm )
	{
		next = atm->next;

		assert ( table[atm->index] == atm );

		if ( !atm->isPersistant )
		{
			// even non-deleteable are deletable... (the ATM can be deleted... however the atm won't delete the cargo)
			table[atm->index] = 0;
			atm->release ();
			atm->next = freeList;
			freeList = atm;
		} else
		{
			// not deletable... we need to keep it active in our table
			atm->next = used;
			used = atm;

			atm->typeNext = typeList[int(atm->type)];
			typeList[int(atm->type)] = atm;
		}
		atm = next;
	}
}

uint32_t atomTable::make ( char const *name, uint64_t hash, struct bcLoadImage *data, bool isDeleteable, bool isPersistant )
{
	auto ctr = getCtr ( hash );

	do
	{
		if ( table[ctr] )
		{
			if ( table[ctr]->hash == hash && !strccmp ( table[ctr]->name, name ) )
			{
				if ( table[ctr]->type != atom::atomType::aCLASSDEF || ((table[ctr]->loadImage != nullptr) && data) )
				{
					throw errorNum::scDUPLICATE_SYMBOL;
				}
				return (ctr + 1);
			}
		} else
		{
			if ( freeList )
			{
				table[ctr] = freeList;
				freeList = freeList->next;
				table[ctr]->index = ctr;
			} else
			{
				table[ctr] = new (instance) atom ( ctr );
			}
			table[ctr]->hash = hash;
			strcpy_s ( table[ctr]->name, sizeof ( table[ctr]->name ), name );
			table[ctr]->type = atom::atomType::aLOADIMAGE;
			table[ctr]->loadImage = data;
			table[ctr]->isDeleteable = isDeleteable;
			table[ctr]->isPersistant = isPersistant;

			table[ctr]->typeNext = typeList[int(table[ctr]->type)];
			typeList[int(table[ctr]->type)] = table[ctr];

			table[ctr]->next = used;
			used = table[ctr];
			return (ctr + 1);
		}
		ctr++;
		if ( ctr == size ) ctr = 0;
	} while ( hash != ctr );

	// cant find atom and table is full
	throw errorNum::scINTERNAL;
}

uint32_t atomTable::make ( char const *name, uint64_t hash, struct bcClass *data, bool isDeleteable, bool isPersistant )
{
	auto ctr = getCtr ( hash );

	do
	{
		if ( table[ctr] )
		{
			if ( table[ctr]->hash == hash && !strccmp ( table[ctr]->name, name ) )
			{
				if ( table[ctr]->type != atom::atomType::aCLASSDEF || ((table[ctr]->classDef != nullptr) && data) )
				{
					throw errorNum::scDUPLICATE_SYMBOL;
				}
				return (ctr + 1);
			}
		} else
		{
			if ( freeList )
			{
				table[ctr] = freeList;
				freeList = freeList->next;
				table[ctr]->index = ctr;
			} else
			{
				table[ctr] = new (instance) atom ( ctr );
			}
			table[ctr]->hash = hash;
			strcpy_s ( table[ctr]->name, sizeof ( table[ctr]->name ), name );
			table[ctr]->type = atom::atomType::aCLASSDEF;
			table[ctr]->classDef = data;
			table[ctr]->isDeleteable = isDeleteable;
			table[ctr]->isPersistant = isPersistant;

			table[ctr]->typeNext = typeList[int(table[ctr]->type)];
			typeList[int(table[ctr]->type)] = table[ctr];

			table[ctr]->next = used;
			used = table[ctr];
			return (ctr + 1);
		}
		ctr++;
		if ( ctr == size ) ctr = 0;
	} while ( hash != ctr );

	// cant find atom and table is full
	throw errorNum::scINTERNAL;
}

uint32_t atomTable::make ( char const *name, uint64_t hash, struct bcFuncDef *data, bool isDeleteable, bool isPersistant )
{
	auto ctr = getCtr ( hash );

	do
	{
		if ( table[ctr] )
		{
			if ( table[ctr]->hash == hash && !strccmp ( table[ctr]->name, name ) )
			{
				if ( table[ctr]->type != atom::atomType::aFUNCDEF || ((table[ctr]->funcDef != nullptr) && data) )
				{
					throw errorNum::scDUPLICATE_SYMBOL;
				}
				return (ctr + 1);
			}
		} else
		{
			if ( freeList )
			{
				table[ctr] = freeList;
				freeList = freeList->next;
				table[ctr]->index = ctr;
			} else
			{
				table[ctr] = new (instance) atom ( ctr );
			}
			table[ctr]->hash = hash;
			strcpy_s ( table[ctr]->name, sizeof ( table[ctr]->name ), name );
			table[ctr]->type = atom::atomType::aFUNCDEF;
			table[ctr]->funcDef = data;
			table[ctr]->isDeleteable = isDeleteable;
			table[ctr]->isPersistant = isPersistant;

			table[ctr]->typeNext = typeList[int ( table[ctr]->type )];
			typeList[int(table[ctr]->type)] = table[ctr];

			table[ctr]->next = used;
			used = table[ctr];
			return (ctr + 1);
		}
		ctr++;
		if ( ctr == size ) ctr = 0;
	} while ( hash != ctr );

	// cant find atom and table is full
	throw errorNum::scINTERNAL;
}

uint32_t atomTable::make ( char const *name, uint64_t hash )
{
	auto ctr = getCtr ( hash );

	do
	{
		if ( table[ctr] )
		{
			if ( table[ctr]->hash == hash && !strccmp ( table[ctr]->name, name ) )
			{
				if ( table[ctr]->type != atom::atomType::aFUNCDEF )
				{
					throw errorNum::scDUPLICATE_SYMBOL;
				}
				return (ctr + 1);
			}
		} else
		{
			if ( freeList )
			{
				table[ctr] = freeList;
				freeList = freeList->next;
				table[ctr]->index = ctr;
			} else
			{
				table[ctr] = new (instance) atom ( ctr );
			}
			table[ctr]->hash = hash;
			strcpy_s ( table[ctr]->name, sizeof ( table[ctr]->name ), name );
			table[ctr]->type = atom::atomType::aNOT_DEFINED;
			table[ctr]->isDeleteable = false;
			table[ctr]->isPersistant = false;

			table[ctr]->typeNext = typeList[int ( table[ctr]->type )];
			typeList[int ( table[ctr]->type )] = table[ctr];

			table[ctr]->next = used;
			used = table[ctr];
			return (ctr + 1);
		}
		ctr++;
		if ( ctr == size ) ctr = 0;
	} while ( hash != ctr );

	// cant find atom and table is full
	throw errorNum::scINTERNAL;
}

