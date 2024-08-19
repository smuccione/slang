// bcInterpreter.h

#pragma once

#include "Utility/settings.h"

#include <memory>
#include <set>

#include "bcVM/fglTypes.h"
#include "bcVM/Opcodes.h"
#include "bcVM/exeStore.h"
#include "bcVM/vmObjectMemory.h"
#include "compilerParser/fglErrors.h"

#pragma pack ( push, 1 ) 

class bcSourceFile
{
	int32_t					  numSourceFiles;
	int32_t					  sizeNameData;
	const uint32_t			 *nameIndex;
	const char				 *nameData;

	public:
	bcSourceFile ()
	{
		static const uint32_t defaultNameIndex = {0};

		numSourceFiles = 1;
		nameIndex = &defaultNameIndex;
		nameData = "(INTERNAL)";
		sizeNameData = 0;
	}

	bcSourceFile ( const uint8_t **expr )
	{
		numSourceFiles = *(uint32_t *) *expr;
		(*expr) += sizeof ( uint32_t );

		sizeNameData = *(uint32_t *) *expr;
		(*expr) += sizeof ( uint32_t );

		nameData = (const char *) *expr;
		(*expr) += sizeNameData;

		nameIndex = (uint32_t *) *expr;
		(*expr) += sizeof ( uint32_t * ) * numSourceFiles;
	}

	int32_t getStaticIndex ( char const *fName )
	{
		for ( int32_t loop = 0; loop < numSourceFiles; loop++ )
		{
			if ( !strccmp ( fName, nameData + nameIndex[loop] ) )
			{
				return loop;
			}
		}
		return -1;
	}
	char const *getName ( uint32_t index )
	{
		return nameData + nameIndex[index];
	}
	int32_t getNumSources( void )
	{
		return numSourceFiles;
	}
};

class nParamType
{
	public:
	uint32_t  count = 0;
	VAR *firstVarParam = nullptr;

	nParamType ()
	{
	};
	nParamType ( VAR *param, uint32_t count ) : count ( count ), firstVarParam ( param )
	{};

	VAR * operator [] ( size_t which )
	{
		if ( which < count )
		{
			return firstVarParam - which;
		}
		throw errorNum::scINTERNAL;
	}
	operator uint32_t ()
	{
		return count;
	}
};

struct nWorkarea
{
	void *value;
	operator void *()
	{
		return value;
	}
};

enum class bcFuncPType
{
	bcFuncPType_NULL,
	bcFuncPType_Short_Int,
	bcFuncPType_Long_Int,
	bcFuncPType_String,
	bcFuncPType_Double,
	bcFuncPType_Variant,
	bcFuncPType_Array,
	bcFuncPType_Object,
	bcFuncPType_NParams,			// not passed in by caller directly... inserted by api routine
	bcFuncPType_Instance,			// not passed in by caller directly... inserted by api routine
	bcFuncPType_Workarea,			// not passed in by caller directly... inserted by api routine
	bcFuncPType_Var_String,
};

struct vmDispatcher;

struct bcFuncDef
{
	uint32_t						  atom = 0;
	fgxFuncCallConvention			  conv = fgxFuncCallConvention::opFuncType_Bytecode;
	char const						 *name = nullptr;
	fglOp							 *cs = nullptr;
	fglOp							 *lastCs = nullptr;
	bool							  coRoutineAble = false;
	bool							  isMethod = false;
	bool							  isExtension = false;
	bool							  isConst = false;
	uint32_t						  nParams = 0;
	uint32_t						  nSymbols = 0;
	fgxSymbols						 *symbols = nullptr;
	struct bcLoadImage				 *loadImage = nullptr;
	std::shared_ptr<vmDispatcher>	  dispatcher;
	fgxTryCatchEntry				 *tcList = nullptr;
	uint32_t						 *defaultParams = nullptr;		// offset into CS of code to compute the default parameter
	// c call interface elements
	long ( *func )(void) = nullptr;
	bcFuncPType						  retType = bcFuncPType::bcFuncPType_NULL;
	bcFuncPType						  pTypes[32]{};
	uint32_t						  nCParams = 0;
//	uint32_t						  stackSize = 0;				// space to hold parameters on the call stack
	uint64_t						  execTimeLocal = 0;			// time spent within the function only
	uint64_t						  execTimeTotal = 0;			// time spent within the function and all calls
	uint64_t						  execNCalls = 0;				// number of times function called

	bcFuncDef ( );
	~bcFuncDef ( );
};

//	need to:
//		1.	quickly find methods, properties, regardless of inheritance depth
//		2.  needs to load quickly (no mallocs)
//		3.	absolute minimal processing during loading`
//
//

struct bcClassVTableEntry
{
	uint32_t					 atom;
	int32_t					     delta;
};

struct bcClassVtableSetup
{
	struct bcClass				*classPtr;
	bcClassVTableEntry			*vTablePtr;			// offset from the start of current objects vtable to the start of the new objects vtable
	uint32_t					 atom;				// atom of function
	uint32_t					 objOffset;			// offset from start of object to the new object (object instantiation method)
};

struct bcClassSearchEntry
{
	char const					 *name;
	uint32_t					  nameLen;

	char const					 *fullName;
	uint32_t					  fullNameLen;

	fgxClassElementType			  type;
	bool						  isVirtual;
	bool						  isStatic;

	struct
	{
		uint32_t					  atom;

		union
		{
			uint32_t				  vTableEntry;
			uint32_t				  globalEntry;
			uint32_t				  data;
			uint32_t				  iVarIndex;
		};
		int16_t						  objectStart;
	} methodAccess;
	struct
	{
		uint32_t					  atom;

		union
		{
			uint32_t				  vTableEntry;
			uint32_t				  globalEntry;
			uint32_t				  data;
			uint32_t				  iVarIndex;
		};
		int16_t						  objectStart;
	} assign;
	struct bcClassSearchEntry	 *left[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	struct bcClassSearchEntry	 *right[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	struct bcClassSearchEntry	 *leftNS[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	struct bcClassSearchEntry	 *rightNS[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];
};

typedef VAR *(*vmCopyCB)(vmInstance *instance, struct VAR *val, bool copy, objectMemory::copyVariables *copyVar);
typedef void (*classCopyCB)(class vmInstance *instance, struct VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar);
typedef void (*classGarbageCollectionCB) (class vmInstance *instance, VAR_OBJ *val, objectMemory::collectionVariables *col);

struct bcClass
{
	char const					  *name;
	uint32_t					   nameLen;

	uint32_t					   atom;					// to allow fast access to our atom table entry to assist pack
	struct bcLoadImage		  	  *loadImage;				// used to get the bss, data, etc. for this struct

	uint32_t					   nVTable;					// number of vTable entries
	struct bcClassVTableEntry	  *vTable;					// array of vTable entries
	
	uint32_t					   nInherit;				// number of inherited classes
	bcClassVtableSetup			  *inherit;					// array of offsets from most derived to vtable starts, etc. to allow fast object construction

	VAR							 **globalsBase;				// base of globals array... this is load image wide

	uint32_t					   nElements;

	uint32_t					  *ops;						// overloaded operators

	uint32_t					   numVars;					// computed at copmile time... makes objConstruct faster by only doing a single allocation

	struct bcClassSearchEntry	  *elements;										// array of all elements for this class, INCLUDING inherited class's
	struct bcClassSearchEntry	  *searchTree[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];			// index into the tree roots for all the elements for searching (already balanced)
	struct bcClassSearchEntry	  *nsSearchTree[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];			// index into the tree roots for all the elements for searching (already balanced)
	struct bcClassSearchEntry	  *defaultMethodEntry;
	struct bcClassSearchEntry	  *defaultAssignEntry;
	struct bcClassSearchEntry	  *defaultAccessEntry;
	struct bcClassSearchEntry	  *packEntry;
	struct bcClassSearchEntry	  *unpackEntry;

	struct bcClassSearchEntry	  *newEntry;
	struct bcClassSearchEntry	  *releaseEntry;

	uint32_t					   nPack;

	classCopyCB					   cCopyCB;
	classGarbageCollectionCB	   cGarbageCollectionCB;
	class vmInspectList			*(*cInspectorCB)			(class vmInstance *instance, struct bcFuncDef *funcDef, struct VAR *var, uint64_t start, uint64_t end);
	void						 ( *cPackCB )				(class vmInstance *instance, struct VAR *var, struct BUFFER *buff, void *param, void ( *pack )(struct VAR *val, void *param));
	void						 ( *cUnPackCB )				(class vmInstance *instance, struct VAR *obj, unsigned char ** buff, uint64_t *len, void *param, struct VAR *(*unPack)(unsigned char ** buff, uint64_t *len, void *param));
	void						 ( *cCheckCB )				(class vmInstance *instance, struct VAR *val, VAR *(*checkVar)(vmInstance *instance, struct VAR *val ), VAR *(*checkMem)(vmInstance *instance, void *val));
};

struct bcUnionEntry
{
	char		name[256];
	uint32_t	nameLen;
	uint32_t	varOffset;
	uint32_t	left;
	uint32_t	right;
};

struct bcUnionTag
{
	uint32_t	 tagNum;							// really just an index into the tags array... this does let us quickly ensure that we have the proper tag number.
	char		 name[256];
	uint32_t	 nameLen;
	uint32_t	 left;
	uint32_t	 right;
	uint32_t	 entryRoot;
};

struct bcUnion
{
	char const				*name;
	uint32_t				 nameLen;

	uint32_t				 atom;					// to allow fast access to our atom table entry to assist pack
	struct bcLoadImage		*loadImage;				// used to get the bss, data, etc. for this struct

	uint32_t				*numEntries;			// allows us to move our load pointer
	bcUnionEntry			*entries;				// these are accessed via the entryRoot in the tag structure... they comprise a balanced binary tree

	uint32_t				*numTags;				//	total number of tags (so we can move our load pointer appropriately)
	bcUnionTag				*tags;					//	array of bcUnionTag structures;
	uint32_t				 tagRoot;				//  tags comprise a balanced binary tree.   this is the root entry
};

struct fgxDebugInfo {
	uint32_t	 sourceIndex;
	uint32_t	 lineNum;
	fglOp		*op;
	uint32_t	 stackHeight;
};

struct bcDebugEntryCompareByLine {
	bool operator() ( fgxDebugInfo const &left, fgxDebugInfo const &right ) const
	{
		if ( left.sourceIndex < right.sourceIndex ) return true;
		if ( left.sourceIndex == right.sourceIndex && left.lineNum < right.lineNum ) return true;
		return false;
	}
};

struct bcDebugEntryCompareByOp {
	bool operator() ( fgxDebugInfo const &left, fgxDebugInfo const &right ) const
	{
		if ( left.op < right.op ) return true;
		return false;
	}
};

// this holds the mapped location of the actual globals.  it is initialized during load time and utlized during a cached start so we don't need to refind all the globals again
struct bcGlobalDef
{
	bcLoadImage *image;				
	size_t		 globalIndex;
};

struct bcLoadImage
{
	public:
	char						  name[256] = "";
	std::shared_ptr<uint8_t>	  fileImage;
	bool						  isLibrary = false;
	uint8_t						 *virtMemPtr = 0;
	fglOp						 *csBase = 0;
	const uint8_t				 *dsBase = 0;
	uint8_t						 *bssBase = 0;
	uint32_t					   bssSize = 0;

	// global variable definitions
	bcGlobalDef					 *globalDefs = 0;
	VAR							**globals = 0;
	uint32_t					  nGlobals = 0;
	fglOp						 *globalInit = 0;
	fgxGlobalEntry				 *globalSymbolTable = 0;
	uint32_t					  globalSymbolTableRoot = -1;
	uint32_t					  globalsExportRoot = 0;

	// debug info
	fgxDebugLineInfo			 *debugLineInfo = 0;
	uint32_t					  nDebugEntries = 0;
	bcSourceFile				  srcFiles;
	fgxSymbols					 *symbols = 0;
	uint32_t					  nSymbols = 0;
	fgxName						 *libraries = 0;
	uint32_t					  nLibraries = 0;
	fgxName						 *modules= 0;
	uint32_t					  nModules= 0;

	// filled in by debug handler ONLY upon first use...
	std::multiset<fgxDebugInfo, bcDebugEntryCompareByLine>	debugByLine;
	std::vector<fgxDebugInfo>								debugByOp;

	class vmInstance	 *instance = 0;

	uint32_t			  atom = 0;

	bcLoadImage ()
	{
	}
	bcLoadImage ( class vmInstance *instance, char const *name );
	~bcLoadImage ();

	void populateDebug ()
	{
		debugByOp.reserve ( nDebugEntries );
		if ( !debugByLine.size () )
		{
			for ( size_t loop = 0; loop < nDebugEntries; loop++ )
			{
				debugByLine.insert ( fgxDebugInfo { debugLineInfo[loop].sourceIndex, debugLineInfo[loop].lineNum, &csBase[debugLineInfo[loop].opOffset], debugLineInfo[loop].stackHeight } );
				debugByOp.push_back ( fgxDebugInfo { debugLineInfo[loop].sourceIndex, debugLineInfo[loop].lineNum, &csBase[debugLineInfo[loop].opOffset], debugLineInfo[loop].stackHeight } );
			}
		}
	}

	static void* operator new (size_t size, class vmInstance *instance);
	static void  operator delete (void *p, class vmInstance *instance);
	static void operator delete (void *p);
};

struct bcObjQuick
{
	union {
		bcClass				*classDef;
		char const			*name;
	};
	union {
		bcClassSearchEntry	*cEntry;
		bcClass				*cClass;
	};
};

#pragma pack ( pop )

// support functions
extern	void		 aArrayNew ( class vmInstance *instance, int64_t nPairs );

// byte-code interpreter functions
