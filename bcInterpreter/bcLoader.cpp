/*
	Byte Code Loader

*/

#include "compilerBCGenerator/compilerBCGenerator.h"
#include "compilerParser/fileParser.h"
#include "bcInterpreter.h"
#include "bcVM/Opcodes.h"
#include "bcVM/exeStore.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"
#include "Utility/sourceFile.h"
#include "Target/fileCache.h"

namespace slangVM
{
	struct instance_
	{
		class vmInstance *instance;
	};
}
void* bcLoadImage::operator new ( size_t size, class vmInstance *instance )
{
	slangVM::instance_	*ptr;
	ptr = (slangVM::instance_ *) instance->malloc ( size + sizeof ( slangVM::instance_ ) );
	ptr->instance = instance;
	return ptr + 1;
}
void bcLoadImage::operator delete ( void *p, class vmInstance *instance )
{
	operator delete ( p );
}
void bcLoadImage::operator delete ( void *p )
{
	vmInstance *instance = ((slangVM::instance_ *)p)[-1].instance;
	instance->free ( &((slangVM::instance_ *)p)[-1] );
}
bcLoadImage::bcLoadImage ( class vmInstance *instance, char const *name ) : instance ( instance )
{
	strcpy_s ( this->name, sizeof ( this->name ), name );
}

bcLoadImage::~bcLoadImage()
{
	if ( virtMemPtr ) VirtualFree ( virtMemPtr, 0, MEM_RELEASE );
}

static inline uint8_t *pseudoAlloc ( uint8_t **ptr, size_t size )
{
	uint8_t *ret;
	ret = *ptr;
	(*ptr) += size;

	return ret;
}

bool vmInstance::load ( std::shared_ptr<uint8_t> &buff, char const *name, bool isPersistant, bool isLibrary )
{
	fgxFileHeader			*header;
	int32_t					*atomIndex;
	uint32_t				*atomXref;
	fgxAtomEntry			*atoms;
	uint32_t				 nAtoms;
	fgxAtomFixup			*atomFixup;
	uint32_t				 nAtomFixup;
	fgxFuncEntry			*func;
	uint32_t				 nFuncs;
	fgxClassEntry			*fClass;
	uint32_t				 nClass;
	uint8_t					*tmpAlloc;
	const uint8_t			*cs;
	const uint8_t			*ds;
	uint32_t				 loop;
	uint32_t				 loop2;
	uint32_t				 loop3;
	bcFuncDef				*funcDef;
	bcClass					*classDef;
	bcLoadImage				*loadImage;
	fgxClassElement			*fElem;
	fgxClassVtableEntry		*fVtab;
	uint32_t				*fOp;
	fgxClassSetupEntry		*fSetup;
	fgxTryCatchEntry		*tcList;

	header = (fgxFileHeader*)&*buff;
	if ( strccmp ( header->sig, sxSig ) )
	{
		return false;
	}
	if ( header->version < sxFileVersion )
	{
		return false;
	}

	if ( atomTable->atomFind ( name ) )
	{
		return true;
	}

	loadImage = new (this) bcLoadImage ( this, name );

	loadImage->isLibrary = isLibrary;

	// get all our pointers and counts
	nAtoms		= *(uint32_t *)(&*buff + header->offsetAtomTable);
	atomIndex	= (int32_t *)(&*buff + header->offsetAtomTable + sizeof ( uint32_t ) );
	atoms		= (fgxAtomEntry *)(&*buff + header->offsetAtomTable + sizeof ( uint32_t ) + (sizeof ( uint32_t ) * nAtoms) );

	nAtomFixup = *(uint32_t *)(&*buff + header->offsetAtomFixups);
	atomFixup = (fgxAtomFixup *)(&*buff + header->offsetAtomFixups + sizeof ( uint32_t ));
	
	tcList = (fgxTryCatchEntry *)(&*buff + header->offsetTCList);
	cs  = &*buff + header->offsetCodeSegment;
	ds  = &*buff + header->offsetDataSegment;

	// TODO: eliminate the funcs that are registered... wasting memory

	auto allocSize =	(sizeof ( uint32_t ) * nAtoms ) +
						(sizeof ( fgxSymbols ) * header->numSymbols ) +
						(sizeof ( bcFuncDef ) * header->numFuncDefs) + 
						(sizeof ( bcClassSearchEntry ) * header->numElements) +
						(sizeof ( bcClassVtableSetup ) * header->numContextEntries) +
						(sizeof ( bcClassVTableEntry ) * header->numVtableEntries) +
						(sizeof ( bcClass ) * header->numClass ) +
						(sizeof ( bcLoadImage  ) * header->numModules ) +
						(sizeof ( uint8_t ) * header->bssSize) +
						(sizeof ( bcGlobalDef ) * header->numGlobals ) + 
						0;

	// CALLOC is much slower than VirtualAlloc in testing for very large memory amounts.   i suspect we're doing double duty with the memory manager both calling virtual alloc and then memset
	loadImage->virtMemPtr = (uint8_t *)VirtualAlloc ( 0, allocSize, MEM_COMMIT, PAGE_READWRITE );

	tmpAlloc = loadImage->virtMemPtr;

	atomXref = (uint32_t *)pseudoAlloc ( &tmpAlloc, nAtoms * sizeof ( uint32_t ));

	nFuncs = *(uint32_t *)(&*buff + header->offsetFuncList);
	func = (fgxFuncEntry *)(&*buff + header->offsetFuncList + sizeof ( uint32_t ));

	nClass = *(uint32_t *)(&*buff + header->offsetClassList);
	fClass = (fgxClassEntry *)(&*buff + header->offsetClassList + sizeof ( uint32_t ));

	loadImage->nDebugEntries			= *(uint32_t *)(&*buff + header->offsetDebug);
	loadImage->debugLineInfo			= (fgxDebugLineInfo *)(&*buff + header->offsetDebug + sizeof ( uint32_t ));
	loadImage->csBase					= (fglOp *)cs;
	loadImage->dsBase					= ds;
	loadImage->fileImage				= buff;

	loadImage->globalInit				= header->offsetGlobalInitializers ? &loadImage->csBase[header->offsetGlobalInitializers] : 0;
	loadImage->nGlobals					= header->numGlobals;
	loadImage->globalSymbolTable		= (fgxGlobalEntry *)(&*buff + header->offsetGlobalSymbolTable);
	loadImage->globalSymbolTableRoot	= header->globalRoot;
	loadImage->globalsExportRoot		= header->globalExportRoot;

	loadImage->bssSize					= header->bssSize;
	loadImage->nSymbols					= header->numSymbols;
	loadImage->symbols					= (fgxSymbols *)pseudoAlloc ( &tmpAlloc, header->numSymbols * sizeof ( fgxSymbols ) );
	loadImage->bssBase					= (uint8_t *)pseudoAlloc ( &tmpAlloc, header->bssSize * sizeof ( uint8_t ) );
	loadImage->globals					= (VAR **)((uint8_t *)loadImage->bssBase + header->offsetBSSGlobals );
	loadImage->nLibraries				= header->numLibraries;
	loadImage->libraries				= (fgxName *)(&*buff + header->offsetLibraries);
	loadImage->nModules					= header->numModules;
	loadImage->modules					= (fgxName *) (&*buff + header->offsetModules);

	// no need to set to 0... virtualAlloc handles this for us
	loadImage->globalDefs				= (bcGlobalDef *)pseudoAlloc ( &tmpAlloc, header->numGlobals * sizeof ( bcGlobalDef ) );

	auto tmpBuff = &*buff + header->offsetSourceFiles;
	loadImage->srcFiles = bcSourceFile ( (uint8_t const **)&tmpBuff );

	auto sym = (fgxSymbols *) (&*buff + header->offsetSymbolTable);
	for ( loop = 0; loop < header->numSymbols; loop++ )
	{
		loadImage->symbols[loop] = sym[loop];
		loadImage->symbols[loop].name = (char const *)(&*buff + sym[loop].offsetName);
	}
	for ( loop = 0; loop < header->numModules; loop++ )
	{
		if ( !atomTable->atomFind ( loadImage->modules[loop].name ) )
		{
			// generate atom entries to indicate that this has been statically linked in
			auto image = (bcLoadImage *) pseudoAlloc ( &tmpAlloc, sizeof ( bcLoadImage ) );
			::new (image) bcLoadImage ( this, loadImage->modules[loop].name );
			atomTable->atomPut ( image->name, image, false, isPersistant );
		}
	}

	// now start processing the file

	// first let's process our atoms;
	for ( loop = 0; loop < nAtoms; loop++ )
	{
		if ( atomIndex[loop] != (uint32_t) -1 )
		{
			char const *atomName = (char const *)(&*buff + atoms[atomIndex[loop]].nameOffset );
			switch ( atoms[atomIndex[loop]].type )
			{
				case int (atomListType::atomClass ):
					atomXref[atomIndex[loop]] = atomTable->make ( atomName, atoms[atomIndex[loop]].hashKey, (bcClass *)0, false, isPersistant );
					break;
				case int ( atomListType::atomFuncref ):
					atomXref[atomIndex[loop]] = atomTable->make ( atomName, atoms[atomIndex[loop]].hashKey, (bcFuncDef *) 0, false, isPersistant );
					break;
				case int (atomListType::atomRef ):
					if ( !(atomXref[atomIndex[loop]] = atomTable->atomFind ( atomName, atoms[atomIndex[loop]].hashKey )) )
					{
						atomXref[atomIndex[loop]] = atomTable->make ( atomName, atoms[atomIndex[loop]].hashKey, (bcFuncDef *) 0, false, isPersistant );
					} else
					{
						break;
						atomXref[atomIndex[loop]] = atomTable->make ( atomName, atoms[atomIndex[loop]].hashKey, (bcFuncDef *) 0, false, isPersistant );
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
		}
	}

	// process the functions
	for ( loop = 0; loop < nFuncs; loop++ )
	{
		switch ( func->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				if ( !(funcDef = atomTable->atomGetFunc ( atomXref[atomIndex[func->atom]] )) )
				{
					funcDef = new ((bcFuncDef*)pseudoAlloc (&tmpAlloc, sizeof (bcFuncDef))) bcFuncDef ();
					funcDef->name = atomTable->atomGetName(atomXref[atomIndex[func->atom]]);
				} else
				{
#if _DEBUG
					pseudoAlloc ( &tmpAlloc, sizeof ( bcFuncDef ) );
#endif
				}
				funcDef->isMethod		= func->isMethod;
				funcDef->isExtension	= func->isExtension;
				funcDef->isMethod		= func->isMethod;
 				funcDef->conv			= func->conv;
				funcDef->isConst		= func->isConst;
				funcDef->loadImage		= loadImage;
				funcDef->cs				= &((fglOp *)(cs))[func->codeOffset];
				funcDef->lastCs			= &((fglOp *)(cs))[func->lastCodeOffset];
				funcDef->nParams		= func->nParams;
				funcDef->coRoutineAble	= func->coRoutineAble ? true : false;
				funcDef->symbols		= &loadImage->symbols[func->symbolsOffset];
				funcDef->nSymbols		= func->nSymbols;
				funcDef->tcList			= &tcList[func->tcOffset];
				funcDef->atom			= atomXref[atomIndex[func->atom]];
				funcDef->defaultParams  = (uint32_t *)(ds + func->defaultParamsOffset);
				atomTable->atomUpdate ( atomXref[atomIndex[func->atom]] , funcDef );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				if ( !(funcDef = atomTable->atomGetFunc ( atomXref[atomIndex[func->atom]] )) )
				{
					funcDef = new ((bcFuncDef*)pseudoAlloc (&tmpAlloc, sizeof (bcFuncDef))) bcFuncDef ();
					funcDef->name = atomTable->atomGetName(atomXref[atomIndex[func->atom]]);
				} else
				{
#if _DEBUG
					pseudoAlloc ( &tmpAlloc, sizeof ( bcFuncDef ) );
#endif
				}
				funcDef->conv			= func->conv;
				funcDef->loadImage		= loadImage;
				funcDef->nParams		= func->nParams;
				funcDef->symbols		= (fgxSymbols *)(&*buff + func->symbolsOffset);
				funcDef->tcList			= 0;
				funcDef->atom			= atomXref[atomIndex[func->atom]];
				funcDef->defaultParams	= (uint32_t *)(ds + func->defaultParamsOffset);
				atomTable->atomUpdate ( atomXref[atomIndex[func->atom]], funcDef );
				break;

			default:
				throw errorNum::scINTERNAL;
		}
		func = (fgxFuncEntry *)(&*buff + func->nextFuncOffset);
	}

	size_t nSearchEntry = 0;
	size_t nContext = 0;
	size_t nVtableEntries = 0;
	for ( loop = 0; loop < nClass; loop++ )
	{
		if ( !(classDef = atomTable->atomGetClass ( atomXref[atomIndex[fClass->atom]] )) )
		{
			classDef = new ((bcClass *) pseudoAlloc ( &tmpAlloc, sizeof ( bcClass ) )) bcClass ();
			classDef->name = atomTable->atomGetName ( atomXref[atomIndex[fClass->atom]] );
			classDef->nameLen = (uint32_t) strlen ( classDef->name ) + 1;
		} else
		{
#if _DEBUG
			pseudoAlloc ( &tmpAlloc, sizeof ( bcClass ) );
#endif
		}
		fElem = (fgxClassElement *) (&*buff + fClass->offsetElements);
		fSetup = (fgxClassSetupEntry *) (&*buff + fClass->offsetInherit);
		fVtab = (fgxClassVtableEntry *) (&*buff + fClass->offsetVTable);
		fOp = (uint32_t *) (&*buff + fClass->offsetOverload);

		classDef->loadImage = loadImage;
		classDef->numVars = fClass->nInstanceVar;
		classDef->globalsBase = loadImage->globals;
		classDef->nElements = fClass->nElements;
		classDef->elements = (bcClassSearchEntry *) pseudoAlloc ( &tmpAlloc, sizeof ( bcClassSearchEntry ) * fClass->nElements );
		classDef->ops = fOp;

		nSearchEntry += fClass->nElements;

		for ( loop2 = 0; loop2 < int( fgxClassElemSearchType::fgxClassElemMaxSearchType ); loop2++ )
		{
			if ( fClass->searchRootIndex[loop2] ) classDef->searchTree[loop2] = &classDef->elements[fClass->searchRootIndex[loop2] - 1];
			if ( fClass->nsSearchRootIndex[loop2] ) classDef->nsSearchTree[loop2] = &classDef->elements[fClass->nsSearchRootIndex[loop2] - 1];
		}

		if ( fClass->newElem )			classDef->newEntry = &classDef->elements[fClass->newElem - 1];
		if ( fClass->releaseElem )		classDef->releaseEntry = &classDef->elements[fClass->releaseElem - 1];
		if ( fClass->packElem )			classDef->packEntry = &classDef->elements[fClass->packElem - 1];
		if ( fClass->unpackElem )		classDef->unpackEntry = &classDef->elements[fClass->unpackElem - 1];
		if ( fClass->defaultMethodElem )	classDef->defaultMethodEntry = &classDef->elements[fClass->defaultMethodElem - 1];
		if ( fClass->defaultAssignElem )	classDef->defaultAssignEntry = &classDef->elements[fClass->defaultAssignElem - 1];
		if ( fClass->defaultAccessElem )	classDef->defaultAccessEntry = &classDef->elements[fClass->defaultAccessElem - 1];

		// we need all this stuff because we allow runtime variant dispatch.  If we can deteremine type, none of this gets used as it is compiletime resolved
		for ( loop2 = 0; loop2 < classDef->nElements; loop2++ )
		{
			classDef->elements[loop2].name = (char *) (&*buff + fElem[loop2].offsetName);
			classDef->elements[loop2].nameLen = (uint32_t) strlen ( classDef->elements[loop2].name ) + 1;
			classDef->elements[loop2].fullName = (char *)(&*buff + fElem[loop2].offsetFullName);
			classDef->elements[loop2].fullNameLen = (uint32_t)strlen( classDef->elements[loop2].fullName) + 1;
			classDef->elements[loop2].type = fElem[loop2].type;

			classDef->elements[loop2].methodAccess.objectStart = (int16_t)(int)fElem[loop2].objectDeltaMethodAccess;
			classDef->elements[loop2].methodAccess.data = fElem[loop2].dataMethodAccess;

			classDef->elements[loop2].assign.objectStart = (int16_t)(int)fElem[loop2].objectDeltaAssign;
			classDef->elements[loop2].assign.data = fElem[loop2].dataAssign;

			for ( loop3 = 0; loop3 < int ( fgxClassElemSearchType::fgxClassElemMaxSearchType ); loop3++ )
			{
				if ( fElem[loop2].leftSearchIndex[loop3] )		classDef->elements[loop2].left[loop3] = &classDef->elements[fElem[loop2].leftSearchIndex[loop3] - 1];
				if ( fElem[loop2].rightSearchIndex[loop3] )		classDef->elements[loop2].right[loop3] = &classDef->elements[fElem[loop2].rightSearchIndex[loop3] - 1];
				if ( fElem[loop2].leftNSSearchIndex[loop3] )	classDef->elements[loop2].leftNS[loop3] = &classDef->elements[fElem[loop2].leftNSSearchIndex[loop3] - 1];
				if ( fElem[loop2].rightNSSearchIndex[loop3] )	classDef->elements[loop2].rightNS[loop3] = &classDef->elements[fElem[loop2].rightNSSearchIndex[loop3] - 1];
			}

			classDef->elements[loop2].isVirtual = fElem[loop2].isVirtual;
			classDef->elements[loop2].isStatic = fElem[loop2].isStatic;
			switch ( classDef->elements[loop2].type )
			{
				case fgxClassElementType::fgxClassType_method:
					classDef->elements[loop2].methodAccess.atom = atomXref[atomIndex[fElem[loop2].atomMethodAccess]];
					break;
				case fgxClassElementType::fgxClassType_prop:
					if ( fElem[loop2].atomMethodAccess != -1 ) classDef->elements[loop2].methodAccess.atom = atomXref[atomIndex[fElem[loop2].atomMethodAccess]];
					if ( fElem[loop2].atomAssign != -1 ) classDef->elements[loop2].assign.atom = atomXref[atomIndex[fElem[loop2].atomAssign]];
					break;
				default:
					break;
			}
		}

		classDef->ops = (uint32_t *) (&*buff + fClass->offsetOverload);

		nVtableEntries += fClass->nVtableEntries; // DEBUG

		// setup the vtable
		classDef->vTable = (bcClassVTableEntry *) pseudoAlloc ( &tmpAlloc, sizeof ( bcClassVTableEntry ) * fClass->nVtableEntries );
		classDef->nVTable = fClass->nVtableEntries;
		for ( loop2 = 0; loop2 < classDef->nVTable; loop2++ )
		{
			classDef->vTable[loop2].atom = atomXref[atomIndex[fVtab[loop2].atom]];			// TODO: must NOT do this except for atoms
			classDef->vTable[loop2].delta = (int16_t)(int)fVtab[loop2].delta;
		}

		nContext += fClass->nInherit; // DEBUG

		classDef->inherit = (bcClassVtableSetup *) pseudoAlloc ( &tmpAlloc, sizeof ( bcClassVtableSetup ) * fClass->nInherit );
		classDef->nInherit = fClass->nInherit;
		for ( loop2 = 0; loop2 < classDef->nInherit; loop2++ )
		{
			classDef->inherit[loop2].atom		= atomXref[atomIndex[fSetup[loop2].atom]];
			classDef->inherit[loop2].classPtr	= atomTable->atomGetClass ( classDef->inherit[loop2].atom );
			classDef->inherit[loop2].vTablePtr	= classDef->vTable + fSetup[loop2].vTableOffset;
			classDef->inherit[loop2].objOffset	= fSetup[loop2].objectOffset;
		}

		classDef->atom = atomXref[atomIndex[fClass->atom]];

		atomTable->atomUpdate ( atomXref[atomIndex[fClass->atom]], classDef );
		fClass = (fgxClassEntry *)(&*buff + fClass->nextClassOffset);
	}

	// fixup the code segment to point to the left atoms
	for ( loop = 0; loop < nAtomFixup; loop++ )
	{
		switch ( atomFixup[loop].type )
		{
			case fgxAtomFixupType::fgxAtomFixup_bcFunc:
				//*(bcFuncDef **)(&(cs[atomFixup[loop].offset])) = atomTable->atomGetFunc ( atomXref[atomIndex[atomFixup[loop].atom]] );
			case fgxAtomFixupType::fgxAtomFixup_atom:
				*(uint32_t *)(&(cs[atomFixup[loop].offset])) = atomXref[atomIndex[atomFixup[loop].atom]];
				break;
		}
	}

	assert ( nVtableEntries == header->numVtableEntries );
	assert ( nContext == header->numContextEntries );
	assert ( nSearchEntry == header->numElements );
	assert ( tmpAlloc == loadImage->virtMemPtr + allocSize );		// requires all pseudoallocs to be executed in _DEBUG... don't need to do many at runtime but this assert ensures we have calculated our maximum memory usage correctly

	for ( loop = 0; loop < header->numLibraries; loop++ )
	{
		auto atm = atomTable->atomFind ( loadImage->libraries[loop].name );

		if ( atm )
		{
			auto type = atomTable->atomGetType ( atm );
			if ( type != atom::atomType::aLOADIMAGE )
			{
				// load failed
				return false;
			}
		} else
		{
			auto entry = globalFileCache.read ( loadImage->libraries[loop].name );
			if ( !entry )
			{
				throw GetLastError ( );
			}
			std::shared_ptr<uint8_t> dup ( (uint8_t *)this->malloc ( entry->getDataLen () ), [=, this]( auto p ) { this->free ( p ); } );
			memcpy ( &*dup, entry->getData ( ), entry->getDataLen ( ) );
			load ( dup, loadImage->libraries[loop].name, isPersistant, true );
		}
	}

	loadImage->atom = atomTable->atomPut ( name, loadImage, true, isPersistant );

	return true;
}
