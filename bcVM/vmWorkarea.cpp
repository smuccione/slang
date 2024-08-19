/*

     SLANG compiler atom managment

*/

#include "bcVM.h"

vmWorkareaTable::vmWorkareaTable ( class vmInstance *instance, int64_t nEntries, int64_t stackDepth ) : instance ( instance )
{
	entries.resize ( nEntries );
	stack.resize ( stackDepth );
	nStack = 0;
	stack[nStack++] = 0;

	for ( auto index = 1ULL; index < entries.size ( ); index++ )
	{
		freeEntries.insert ( (uint32_t) index );
	}
}

vmWorkareaTable::~vmWorkareaTable ( )
{
	// force close anything still open
	for ( auto &it : entries )
	{
		if ( it.used )
		{
			it.close ( instance, it.data );
		}
	}
}

void vmWorkareaTable::reset ( void )
{
	// force close anything still open
	for ( auto &it : entries )
	{
		if ( it.used )
		{
			it.close ( instance, it.data );
		}
	}
	stack[0] = 0;
	for ( auto index = 1ULL; index < entries.size ( ); index++ )
	{
		freeEntries.insert ( (uint32_t)index );
	}
	nStack = 1;
}

char *vmWorkareaTable::getAlias ( int64_t entry )
{
	if ( entry >= (int64_t)entries.size() )
	{
		throw errorNum::scINVALID_WORKAREA;
	}
	if ( !entries[entry].used )
	{
		throw errorNum::scWORKAREA_NOT_IN_USE;
	}
	return entries[entry].alias;
}

char *vmWorkareaTable::getAlias ( void *data )
{
	auto it = dataMap.find ( data );
	if ( it == dataMap.end() )
	{
		throw errorNum::scINTERNAL;
	}

	return entries[it->second].alias;
}

void vmWorkareaTable::push ( int64_t entry )
{
	if ( nStack >= (int64_t)stack.size() )
	{
		throw errorNum::scWORKAREA_STACK_DEPTH_EXCEEDED;
	}
	if ( entry >= (int64_t) entries.size() )
	{
		throw errorNum::scINVALID_WORKAREA;
	}
	if ( !entries[entry].used )
	{
		throw errorNum::scWORKAREA_NOT_IN_USE;
	}
	stack[nStack++] = entry;
	stack[0] = stack[nStack - 1];
}

void vmWorkareaTable::push ( char const *alias )
{
	if ( nStack >= (int64_t) stack.size() )
	{
		throw errorNum::scWORKAREA_STACK_DEPTH_EXCEEDED;
	}

	auto it = aliasMap.find ( alias );
	if ( it == aliasMap.end ( ) )
	{
		throw errorNum::scALIAS_NOT_FOUND;
	}

	stack[nStack++] = it->second;
	stack[0] = stack[nStack - 1];
}

void vmWorkareaTable::push ( VAR *var )
{
	while ( TYPE ( var ) == slangType::eREFERENCE ) var = var->dat.ref.v;
	switch ( TYPE ( var ) )
	{
		case slangType::eLONG:
			push ( (int64_t)var->dat.l );
			break;
		case slangType::eSTRING:
			push ( var->dat.str.c );
			break;
		default:
			throw errorNum::scINVALID_ALIAS;
	}
	stack[0] = stack[nStack - 1];
}

void vmWorkareaTable::select ( int64_t entry )
{
	if ( entry > (int64_t) entries.size() )
	{
		throw errorNum::scINVALID_WORKAREA;
	}
	stack[0] = entry;
}

bool vmWorkareaTable::inUse ( int64_t entry )
{
	if ( !entry ) return false;
	if ( entry > (int64_t) entries.size ( ) )
	{
		throw errorNum::scINVALID_WORKAREA;
	}
	if ( entries[entry].used ) return true;
	return false;
}

void vmWorkareaTable::select ( char const *alias )
{
	auto it = aliasMap.find ( alias );
	if ( it == aliasMap.end ( ) )
	{
		throw errorNum::scALIAS_NOT_FOUND;
	}
	stack[0] = stack[nStack - 1];
}

void vmWorkareaTable::pop ( int64_t num )
{
	assert ( nStack > num );
	nStack -= num;
	stack[0] = stack[nStack - 1];
}

int64_t vmWorkareaTable::alloc ( char const *alias, void *data, vmWorkareaCB read, vmWorkareaCB write, vmWorkareaCloseCB close )
{
	if ( !freeEntries.size() )
	{
		throw errorNum::scNOFREE_WORKAREAS;
	}

	auto entryNum = *freeEntries.begin ( );
	freeEntries.erase ( entryNum );

	aliasMap[alias] = entryNum;
	dataMap[data] = entryNum;

//	printf ( "alloc: %p   %i\r\n", data, entryNum );

	strncpy ( entries[entryNum].alias, alias, sizeof ( entries[entryNum].alias ) );
	entries[entryNum].data		= data;
	entries[entryNum].used		= true;
	entries[entryNum].read		= read;
	entries[entryNum].write	= write;
	entries[entryNum].close	= close;

	stack[0] = entryNum;

	return entryNum;
}

void vmWorkareaTable::free ( int64_t nEntry )
{
	dataMap.erase ( entries[nEntry].data );
	aliasMap.erase ( entries[nEntry].alias );
	freeEntries.insert ( nEntry );
	if ( stack[0] == nEntry )
	{
		stack[0] = 0;
	}
	entries[nEntry].used = false;
}

void vmWorkareaTable::free ( void *data )
{
	assert ( dataMap.find ( data ) != dataMap.end ( ) );
	auto nEntry = dataMap.find ( data )->second;
	dataMap.erase ( entries[nEntry].data );
	aliasMap.erase ( entries[nEntry].alias );
	freeEntries.insert ( nEntry );
	if ( stack[0] == nEntry )
	{
		stack[0] = 0;
	}
	entries[nEntry].used = false;
}

void vmWorkareaTable::read ( char const *fieldName, VAR *var )
{
	if ( !stack[0] ) throw errorNum::scWORKAREA_NOT_IN_USE;
	auto entry = &entries[stack[0]];
	if ( !entry->used ) throw errorNum::scWORKAREA_NOT_IN_USE;
	entry->read ( instance, entry->data, fieldName, var );
}

void vmWorkareaTable::read ( VAR *fieldName, VAR *var )
{
	while ( TYPE ( fieldName ) == slangType::eREFERENCE ) fieldName = fieldName->dat.ref.v;
	if ( TYPE ( fieldName ) != slangType::eSTRING ) throw errorNum::scINVALID_FIELD;

	if ( !stack[0] ) throw errorNum::scWORKAREA_NOT_IN_USE;
	auto entry = &entries[stack[0]];
	if ( !entry->used ) throw errorNum::scWORKAREA_NOT_IN_USE;
	entry->read ( instance, entry->data, fieldName->dat.str.c, var );
}

void vmWorkareaTable::write ( char const *fieldName, VAR *var )
{
	if ( !stack[0] ) throw errorNum::scWORKAREA_NOT_IN_USE;
	auto entry = &entries[stack[0]];
	if ( !entry->used ) throw errorNum::scWORKAREA_NOT_IN_USE;
	entry->write ( instance, entry->data, fieldName, var );
}

void vmWorkareaTable::write ( VAR *fieldName, VAR *var )
{
	while ( TYPE ( fieldName ) == slangType::eREFERENCE ) fieldName = fieldName->dat.ref.v;
	if ( TYPE ( fieldName ) != slangType::eSTRING ) throw errorNum::scINVALID_FIELD;

	if ( !stack[0] ) throw errorNum::scWORKAREA_NOT_IN_USE;
	auto entry = &entries[stack[0]];
	if ( !entry->used ) throw errorNum::scWORKAREA_NOT_IN_USE;
	entry->write ( instance, entry->data, fieldName->dat.str.c, var );
}

void *vmWorkareaTable::getData ( void )
{
	if ( !stack[nStack - 1] ) return 0;
	auto entry = &entries[stack[nStack - 1]];
	if ( !entry->used ) throw errorNum::scWORKAREA_NOT_IN_USE;

	return entry->data;
}

void vmWorkareaTable::setData ( void *data )
{
	if ( !stack[nStack - 1] ) return;
	auto entry = &entries[stack[nStack - 1]];
	if ( !entry->used ) throw errorNum::scWORKAREA_NOT_IN_USE;

	entry->data = data;
}

void *vmWorkareaTable::getData ( int64_t index )
{
	return entries[index].data;
}

void vmWorkareaTable::setData ( int64_t index, void *data )
{
	entries[index].data = data;
}
