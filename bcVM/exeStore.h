/*
	exeStore.h			FGX and LIB file format and stuctures

*/

#pragma once

#include <stdint.h>

#include "Target/common.h"

#define sxFileVersion		0x600
#define sxSig				"SLangExecutable\r\n\0"

enum class fgxOvOp {
	ovNone			= 0,
	ovAdd,
	ovSubtract,
	ovMultiply,
	ovDivide,
	ovModulus,
	ovPower,
	ovMax,
	ovMin,
	ovNegate,
	ovNot,
	ovEqual,
	ovEqualA,
	ovNotEqual,
	ovLess,
	ovLessEqual,
	ovGreater,
	ovGreaterEqual,
	ovBitAnd,
	ovBitOr,
	ovBitXor,
	ovShiftLeft,
	ovShiftRight,
	ovArrayDerefLHS,
	ovArrayDerefRHS,
	ovAssign,
	ovAddAssign,
	ovSubAssign,
	ovMulAssign,
	ovDivAssign,
	ovModAssign,
	ovPowAssign,
	ovBwAndAssign,
	ovBwOrAssign,
	ovBwXorAssign,
	ovShLeftAssign,
	ovShRightAssign,
	ovPostInc,
	ovPostDec,
	ovPreInc,
	ovPreDec,
	ovTwosComplement,
	ovRefCreate,
	ovFuncCall,
	ovWorkareaStart,
	ovMaxOverload
};

#pragma pack ( push, 4 )

enum class fgxAtomFixupType {
	fgxAtomFixup_atom	= 1,
	fgxAtomFixup_bcFunc
};

struct fgxAtomFixup {
	fgxAtomFixupType	type;
	uint32_t			atom;
	uint32_t			offset;
};

struct fgxAtomEntry {
	uint32_t			type;
	uint32_t			nameOffset;
	uint32_t			offset;
	uint64_t			hashKey;
};

struct fgxSymbols {
	uint32_t			 offsetName;
	char const			*name;				// filled in at load time
	uint32_t			 validStart;		// offsets in code segment where this symbols is valid
	uint32_t			 validEnd;
	uint32_t			 index;				// stack offset from basePointer or index into globals
	bool				 noRef;
	bool				 isStackBased;
	bool				 isParameter;
};

struct fgxTryCatchEntry {
	uint32_t			 tryOffset;
	uint32_t			 catchOffset;
	uint32_t			 stackSize;			// size stack (base relative) should be on call to catch.   This allows removal of any block locals in one swoop.
	uint32_t			 errSym;
};

enum class fgxFuncCallConvention {
	opFuncType_Bytecode			= 1,
	opFuncType_Bytecode_Native,
	opFuncType_cDecl,
};

struct fgxDebugLineInfo {
	uint32_t				 sourceIndex;
	uint32_t				 opOffset;
	uint32_t				 lineNum;
	uint32_t				 stackHeight;
};

struct fgxFuncEntry {
	fgxFuncCallConvention		conv;
	uint32_t					nextFuncOffset;
	uint32_t					atom;

	uint32_t					coRoutineAble;
	uint32_t					sourceIndex;				// source file index
	uint32_t					nParams;
	uint32_t					isMethod;
	uint32_t					isExtension;
	uint32_t					isConst;					// is safe for watch window execution

	uint32_t					symbolsOffset;				// offset into symbol table of the start of our local symbols
	uint32_t					nSymbols;

	uint32_t					codeOffset;
	uint32_t					lastCodeOffset;				// used for debugging to get inline'd functions correct
	uint32_t					tcOffset;

	uint32_t					staticsBSSOffset;

	uint32_t					defaultParamsOffset;		// table in ds of offsets in code of the default parameters
};

enum class fgxClassElemSearchType : int 
{
	fgxClassElemPublicMethod = 0,
	fgxClassElemPrivateMethod,
	fgxClassElemPublicAccess,
	fgxClassElemPublicAssign,
	fgxClassElemPrivateAccess,
	fgxClassElemPrivateAssign,
	fgxClassElemInherit,
	fgxClassElemAllPublic,
	fgxClassElemAllPrivate,
	fgxClassElemMaxSearchType
};

enum class fgxClassElementType : int
{
	fgxClassType_method	= 1,
	fgxClassType_iVar,
	fgxClassType_prop,
	fgxClassType_static,
	fgxClassType_const,
	fgxClassType_inherit,
	fgxClassType_message,
};

enum class fgxClassElementScope : int
{
	fgxClassScope_public		= 1,
	fgxClassScope_private,
	fgxClassScope_protected,
};

struct fgxClassVtableEntry {
	uint32_t	delta;
	uint32_t	atom;
	bool		executable;
};

struct fgxClassOperator {
	uint32_t			elemIndex;
};

struct fgxClassSetupEntry
{
	uint32_t	atom;
	uint32_t	objectOffset;
	uint32_t	vTableOffset;
};

struct fgxClassElement {
	fgxClassElementType	type;

	uint32_t			offsetName;
	uint32_t			offsetFullName;

	uint32_t			dataMethodAccess;
	uint32_t			atomMethodAccess;		// atom for methods and access properties
	int32_t				objectDeltaMethodAccess;

	uint32_t			dataAssign;
	uint32_t			atomAssign;				// atom for assignment properties
	int32_t				objectDeltaAssign;

	bool				isVirtual;
	bool				isStatic;

	uint32_t			leftSearchIndex[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	uint32_t			rightSearchIndex[int ( fgxClassElemSearchType::fgxClassElemMaxSearchType )];

	uint32_t			leftNSSearchIndex[int ( fgxClassElemSearchType::fgxClassElemMaxSearchType )];
	uint32_t			rightNSSearchIndex[int ( fgxClassElemSearchType::fgxClassElemMaxSearchType )];
};

struct fgxClassEntry {
	uint32_t			 nextClassOffset;

	uint32_t			 atom;

	uint32_t			 nElements;
	uint32_t			 offsetElements;
	uint32_t			 offsetOverload;
	
	uint32_t			 nInstanceVar;
		
	uint32_t			 nVtableEntries;
	uint32_t			 offsetVTable;
	uint32_t			 offsetVTableSetup;

	uint32_t			 offsetInitializers;

	uint32_t			 searchRootIndex[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	uint32_t			 nsSearchRootIndex[int(fgxClassElemSearchType::fgxClassElemMaxSearchType)];

	uint32_t			 constElem;			// offset into elems array
	uint32_t			 newElem;			// offset into elems array
	uint32_t			 releaseElem;		// offset into elems array
	uint32_t			 defaultMethodElem;	// offset into elems array
	uint32_t			 defaultAccessElem;	// offset into elems array
	uint32_t			 defaultAssignElem;	// offset into elems array
	uint32_t			 packElem;			// offset into elems array
	uint32_t			 unpackElem;		// offset into elems array

	uint32_t			 nInherit;
	uint32_t			 offsetInherit;
};

struct fgxGlobalEntry {	
	char				 name[MAX_NAME_SZ];
	uint32_t			 index;
	uint32_t			 left;
	uint32_t			 right;
	uint32_t			 nextExportable;
	bool				 isExportable;
};

struct fgxName {
	char				 name[MAX_NAME_SZ];
};

struct fgxFileHeader {
	char			 sig[sizeof (sxSig)];
	uint32_t		 version; 

	// element offsets from start of header
	uint32_t		 offsetAtomTable;
	uint32_t		 offsetAtomFixups;
	uint32_t		 offsetCodeSegment;
	uint32_t		 offsetDataSegment;
	uint32_t		 offsetFuncList;
	uint32_t		 offsetClassList;
	uint32_t		 offsetTCList;
	uint32_t		 offsetBSSGlobals;
	uint32_t		 offsetGlobalSymbolTable;
	uint32_t		 offsetSymbolTable;
	uint32_t		 offsetLibraries;
	uint32_t		 offsetModules;
	uint32_t		 offsetDebug;
	uint32_t		 offsetGlobalInitializers;
	uint32_t		 offsetInterface;
	uint32_t		 offsetSourceFiles;
	uint32_t		 offsetBuildDirectories;

	// count of each element type
	uint32_t		 numGlobals;
	uint32_t		 globalExportRoot;

	uint32_t		 numSymbols;
	uint32_t		 numFuncDefs;
	uint32_t		 numElements;
	uint32_t		 numClass;
	uint32_t		 numContextEntries;
	uint32_t		 numVtableEntries;
	uint32_t		 numLibraries;
	uint32_t		 numModules;
	uint32_t		 numBuildDirectories;

	uint32_t		 bssSize;					// amount of bss requred
	uint32_t		 globalRoot;				// index into the globals of the root element for the tree
};

#pragma pack ( pop )
