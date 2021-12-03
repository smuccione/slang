/*

	atomList for the compiler - this is NOT the runtime atom table... that's a different beast entirely

*/

#include "atomList.h"
#include "Utility/funcky.h"

uint32_t atomList::add ( stringi const &name, atomListType type )
{
	atomDef	*atom;

	auto lu = lookup.find ( name );
	if ( lu != lookup.end () )
	{
		auto it = atoms[lu->second];
		if ( it->type == atomListType::atomRef )
		{
			// we're now defining the reference
			it->type			= type;
			it->isInterface	= false;
			return (uint32_t)lu->second;
		} else
		{
			return (uint32_t)lu->second;
		}
	}

	atom = new atomDef;

	atom->type				= type;
	atom->name				= name;
	atom->isInterface		= false;
	atoms.push_back ( atom );

	lookup[name] = atoms.size () - 1;

	return (uint32_t)( atoms.size() - 1 );
}

uint32_t atomList::addProvisional ( stringi const &name, atomListType type )
{
	atomDef	*atom;

	auto lu = lookup.find ( name );
	if ( lu != lookup.end () )
	{
		auto it = atoms[lu->second];
		if ( it->type == atomListType::atomRef )
		{
			// we're now defining the reference
			it->type = type;
			return (uint32_t)lu->second;
		} else
		{
			return (uint32_t)lu->second;
		}
	}

	atom = new atomDef;

	atom->type				= type;
	atom->name				= name;
	atom->isInterface		= true;
	atoms.push_back ( atom );

	lookup[name] = atoms.size () - 1;

	return (uint32_t)( atoms.size() - 1 );
}

char const *atomList::getAtom ( size_t offset )
{
	for ( auto it = fixup.begin(); it != fixup.end(); it++ )
	{
		if ( (*it)->offset == offset )
		{
			return atoms[(*it)->atom]->name.c_str();
		}
	}

	throw errorNum::scINTERNAL;
}

uint32_t atomList::getAtom ( char const *name )
{
	auto lu = lookup.find ( name );
	if ( lu != lookup.end () )
	{
		return (uint32_t)lu->second;
	}
	throw errorNum::scINTERNAL;
}

void atomList::serializeAtoms ( BUFFER *buff )
{
	fgxAtomEntry	 entry = {};
	size_t			 indexOffset;
	uint32_t		*index;
	int32_t			 counter;

	index = new uint32_t[atoms.size()];

	bufferPut32 ( buff, (uint32_t)atoms.size() );

	indexOffset = bufferSize ( buff );
	buff->makeRoom ( sizeof ( uint32_t ) * atoms.size() );
	buff->assume ( sizeof ( uint32_t ) * atoms.size() );

	counter = 0;

	size_t nameOffset = bufferSize ( buff  ) + atoms.size() * sizeof ( entry );

	for ( auto it = atoms.begin(); it != atoms.end(); it++ )
	{
		entry.type = int((*it)->type);
		entry.nameOffset = (uint32_t)nameOffset;
		entry.hashKey	 = _hashKey( (*it)->name );

		nameOffset += (*it)->name.size() + 1;
		index[std::distance(atoms.begin(), it )] = counter++;
		buff->put ( entry );
	}

	for ( auto it = atoms.begin(); it != atoms.end(); it++ )
	{
		buff->write ( (*it)->name.c_str(), (*it)->name.size() + 1);
	}

	memcpy ( bufferBuff ( buff ) + indexOffset, index, sizeof ( uint32_t ) * atoms.size() );

	delete[] index;
}

void atomList::serializeFixups ( BUFFER *buff )
{
	fgxAtomFixup fixupEntry = {};

	bufferPut32 ( buff, (uint32_t)fixup.size() );
	for ( auto it = fixup.begin(); it != fixup.end(); it++ )
	{
		fixupEntry.type = (*it)->type;
		fixupEntry.atom = (*it)->atom;
		fixupEntry.offset = (uint32_t)(*it)->offset;
		bufferWrite ( buff, &fixupEntry, sizeof ( fixupEntry ) );
	}
}

void atomList::addFixup ( uint32_t atom, size_t offset )
{
	atoms[atom]->isInterface = false;
	fixup.push_back ( new atomFixup ( atom, offset ) );
}

void atomList::addFixup ( fgxAtomFixupType type, uint32_t atom, size_t offset )
{
	atoms[atom]->isInterface = false;
	fixup.push_back ( new atomFixup ( type, atom, offset ) );
}
