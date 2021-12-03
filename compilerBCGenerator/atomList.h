/*

	atomTable for the compiler - this is NOT the runtime atom table... that's a different beast entirely


*/

#pragma once

#include "Utility/settings.h"

#include <vector>
#include <string.h>
#include "bcVM/exeStore.h"
#include "Utility/buffer.h"
#include "Utility/stringi.h"

enum class atomListType {
	atomClass		= 1,
	atomFuncref,
	atomRef,
};

class atomDef {
public:
	stringi				 name;
	atomListType		 type;
	bool				 isInterface;

	atomDef ( ) : type ( atomListType::atomRef )
	{
		isInterface = false;
	}

	virtual ~atomDef ( void )
	{
	}
};

class atomFixup {
public:
	fgxAtomFixupType	type;
	uint32_t			atom;
	size_t				offset;

	atomFixup( uint32_t atom, size_t offset )
	{
		this->type	= fgxAtomFixupType::fgxAtomFixup_atom;
		this->atom	= atom;
		this->offset = offset;
	}
	atomFixup( fgxAtomFixupType type, uint32_t atom, size_t offset )
	{
		this->type = type;
		this->atom = atom;
		this->offset = offset;
	}
};

class atomList
{
public:
	std::map<stringi, size_t>	 lookup;
	std::vector<atomDef *>		 atoms;
	std::vector<atomFixup *>	 fixup;

	atomList( )
	{
	}

	~atomList()
	{
		for ( auto it = atoms.begin(); it != atoms.end(); it++ )
		{
			delete (*it);
		}
		atoms.clear();

		for ( auto it = fixup.begin(); it != fixup.end(); it++ )
		{
			delete (*it);
		}
		fixup.clear();
	}

	void clear()
	{
		for ( auto it = atoms.begin(); it != atoms.end(); it++ )
		{
			delete (*it);
		}
		atoms.clear();

		for ( auto it = fixup.begin(); it != fixup.end(); it++ )
		{
			delete (*it);
		}
		fixup.clear();
		lookup.clear ();
	}

	uint32_t add ( stringi const &name, atomListType type );
	uint32_t addProvisional ( stringi const &name, atomListType type );
	void  addFixup ( uint32_t tom, size_t offset );
	void  addFixup ( fgxAtomFixupType type, uint32_t atom, size_t offset );

	uint32_t getAtom ( char const *name );
	char const *getAtom ( size_t offset );
	void serializeAtoms ( BUFFER *buff );
	void serializeFixups ( BUFFER *buff );
};

