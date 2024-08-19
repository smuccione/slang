/*

	Byte Code Generator

*/

#pragma once

#include "Utility/settings.h"

#include <stddef.h>
#include <stack>
#include <vector>
#include <set>
#include <unordered_set>
#include <stdarg.h>
#include <deque>
#include <map>
#include <string>
#include <limits>


#include "compilerParser/symbolSpace.h"
#include "bcVM/exeStore.h"                       // for fgxClassElementScope, fgxClassElemSearchType::fgxClassElemMaxSearchType, fgxClassElementType, fgxClassElemSearchType, fgxDebugLineInfo, fgxTryCatchEntry, fgxOvOp, fgxFuncCallConvention, fgxFuncCallConvention::opFuncType_Bytecode
#include "Target/common.h"
#include "bcVM/Opcodes.h"
#include "bcInterpreter/bcInterpreter.h"
#include "atomList.h"
#include "compilerOptimizer.h"

#undef max

enum class compSymbolsScope {
	csStack = 1,
	csLocalStatic,
	csGlobalStatic,
	csGlobal,
	csField,
};

class compCodeSegment {
	BUFFER	buff;
	bool	genProfiling;
	class compExecutable &comp;

#ifdef _DEBUG
	uint32_t	nOps;
#endif

	std::stack<bool>	emitCode;
public:
	compCodeSegment ( class compExecutable &comp ) : buff ( 1024 * 1024 * 4 ), comp ( comp )
	{
		genProfiling = false;
		memset ( bufferBuff ( &buff ), 0, bufferFree ( &buff ) );
		emitCode.push ( true );

		// throw a noop at the 0 address.. this let's us use 0 offset as a flag
		putOp ( fglOpcodes::noop );
	}
	~compCodeSegment()
	{
	}

	void enableProfiling      ( );
	size_t size               ( );
	uint32_t nextOp           ( );
	void serialize            ( BUFFER *buff );
	void peepHole             ( fglOp op );
	void emitLabel            ( char *label );
	void emitLabel            ( cacheString const &label );
	void disableEmit          ( );
	void enableEmit           ( );
	void emitPush             ( );
	void emitPop              ( );
	void emitProfileCallStart ( fglOpcodes op );
	void emitProfileCallEnd   ( fglOpcodes op );
	void putOp                ( fglOpcodes op );
	uint32_t putOp            ( fglOpcodes op, int64_t int64Value );
	void putOp                ( fglOpcodes op, double doubleValue );
	uint32_t putOp            ( fglOpcodes op, int32_t index );
	uint32_t putOp            ( fglOpcodes op, uint32_t index );
	uint32_t putOp            ( fglOpcodes op, uint32_t val1, uint32_t val2 );
	uint32_t putOp            ( fglOpcodes op, uint32_t stackIndex, int16_t objectIndex, uint16_t nParams );
	void setdword             ( uint32_t offset, uint32_t value );
	fglOp *getOps             ( );
	uint32_t getNumOps        ( );
	uint32_t getLen           ( );
};

class compDataSegment {
public:
	BUFFER	buff;

public:
	compDataSegment () : buff ( 1024 * 1024 * 4 )
	{
	}
	~compDataSegment()
	{
	}

	uint32_t size() const
	{
		return (uint32_t)bufferSize( &buff );
	}

	uint32_t addString ( uint32_t len, char *str )
	{
		uint32_t	ret;

		ret = (uint32_t)bufferSize ( &buff );
		bufferPut32 ( &buff, len );
		bufferWrite ( &buff, str, len );
		bufferPut8 ( &buff, (char)0 );

		return ( ret );
	}

	uint32_t addString ( char const *str )
	{
		uint32_t	ret;
		uint32_t	len;

		len = (uint32_t)strlen ( str );

		ret = (uint32_t)bufferSize ( &buff );
		bufferPut32 ( &buff, len );
		bufferWrite ( &buff, (void *)str, len );
		bufferPut8 ( &buff, (char)0 );

		return ( ret );
	}

	uint32_t addString ( cacheString str )
	{
		uint32_t	ret;

		ret = ( uint32_t) bufferSize ( &buff );
		bufferPut32 ( &buff, (uint32_t)str.size () );
		bufferWrite ( &buff, ( void*) str.c_str(), str.size() );
		bufferPut8 ( &buff, ( char) 0 );

		return (ret);
	}

	uint32_t addLong ( uint32_t value )
	{
		uint32_t	ret;

		ret = (uint32_t)bufferSize ( &buff );
		bufferPut32 ( &buff, value );
		return ( ret );
	}

	uint32_t addBuffer ( uint32_t len )
	{
		uint32_t	ret;

		ret = (uint32_t)bufferSize ( &buff );
		bufferMakeRoom ( &buff, len );
		bufferAssume ( &buff, len );
		return ( ret );
	}

	void *getRawBuff ( ) const
	{
		return bufferBuff ( &buff );
	}

	void serialize ( BUFFER *buff ) const
	{
		buff->write ( bufferBuff ( &this->buff ), bufferSize ( &this->buff ) );
	}
};

struct compDebugEntry {
	uint32_t			 sourceIndex;
	uint32_t			 lineNum;
	uint32_t			 addr;			// executable address of the start of this line
	uint32_t			 stackHeight;
};


class compDebug {
public:
	std::vector<compDebugEntry>	 lineEntry;

	void addEntry ( srcLocation const &location, uint32_t address, uint32_t stackHeight )
	{
		if ( !lineEntry.size () )
		{
			lineEntry.push_back ( compDebugEntry { location.sourceIndex, location.lineNumberStart, address, stackHeight } );
		} else if ( (location.sourceIndex != lineEntry.back ().sourceIndex) || (location.lineNumberStart != lineEntry.back ().lineNum) )
		{
			lineEntry.push_back ( compDebugEntry { location.sourceIndex, location.lineNumberStart, address, stackHeight } );
		}
	}

	void addEnd ( uint32_t address, uint32_t stackHeight )
	{
		lineEntry.push_back ( compDebugEntry{ UINT_MAX, UINT_MAX, address, stackHeight } );
	}

	void serialize ( BUFFER *buffer )
	{
		buffer->put ( (uint32_t)lineEntry.size() );

		for ( auto &it : lineEntry )
		{
			fgxDebugLineInfo	lineInfo = {};

			lineInfo.sourceIndex = it.sourceIndex;
			lineInfo.lineNum	 = it.lineNum;
			lineInfo.opOffset	 = it.addr;
			lineInfo.stackHeight = it.stackHeight;
			buffer->put ( lineInfo );
		}
	}
};

class compFunc {
public:
	fgxFuncCallConvention		 conv = fgxFuncCallConvention::opFuncType_Bytecode;
	bool						 isInterface = false;

	cacheString					 name;
	cacheString					 nSpace;
	srcLocation					 location;

	uint32_t					 atom = 0;
	uint32_t					 nParams = 0;
	uint32_t					 codeOffset = 0;
	uint32_t					 lastCodeOffset = 0;
	uint32_t					 symOffset = 0;				// offset into symbols array of the start of this functions symbols
	uint32_t					 nSymbols = 0;				// total number of all symbols
	uint32_t					 tcOffset = 0;
	uint32_t					 globalsBSSOffset = 0;

	uint32_t					 defParamOffset = 0;

	bool						 isMethod = false;
	bool						 isExtension = false;
	bool						 isConst = false;

	class symbolSpaceParams		*params = nullptr;

	compDebug					 debug;

	compFunc ()
	{
	}

	void serialize ( BUFFER *buff );
};

class compFixupElement {
public:
	cacheString 							label;
	uint32_t							offset;
	std::vector<uint32_t>				fixups;

	static const decltype(offset) invalidOffset = std::numeric_limits<decltype(offset)>::max ( );

	compFixupElement ( cacheString const &label ) : label ( label )
	{
		offset = invalidOffset;
	}

	compFixupElement ( compFixupElement const &old )
	{
		offset = old.offset;
		label = old.label;
		fixups = old.fixups;
	}

	compFixupElement( compFixupElement &&old ) noexcept
	{
		*this = std::move ( old );
	}

	compFixupElement &operator = ( compFixupElement const &old )
	{
		return *this = compFixupElement ( old );
	}

	compFixupElement &operator = ( compFixupElement &&old ) noexcept
	{
		std::swap ( offset, old.offset );
		std::swap ( label, old.label );
		std::swap ( fixups, old.fixups );
		return *this;
	}
};

class compFixup {
	uint32_t	unique = 0;

public:
	std::unordered_map<cacheString, compFixupElement *>				labels;
	std::unordered_map<uint32_t, std::vector<compFixupElement *>>	addressToLabel;

	compFixup()
	{
	}

	~compFixup ( )
	{
		for ( auto &it : labels )
		{
			delete it.second;
		}
	}

	compFixupElement &getFixup ( cacheString const &label )
	{
		if ( labels.find ( label ) == labels.end ( ) )
		{
			labels[label] = new compFixupElement ( label );
		}
		return *(labels.find ( label )->second);
	}

	void needFixup ( cacheString const &label, uint32_t offset )
	{
		auto &fu = getFixup ( label );
		fu.fixups.push_back ( offset );
	}

	void setRefPoint ( cacheString const &label, uint32_t offset )
	{
		auto &fu = getFixup ( label );
#if defined ( _DEBUG )
		if ( fu.offset != compFixupElement::invalidOffset )
		{
			DebugBreak ( );
		}
#endif
		fu.offset = offset;
	}

	uint32_t getUnique ()
	{
		return ++unique;
	}

	void applyFixups ( compCodeSegment &codeSegment )
	{
		for ( auto &it : labels )
		{
			for ( auto &fu : it.second->fixups )
			{
#if defined ( _DEBUG )
				if ( it.second->offset == compFixupElement::invalidOffset )
				{
					DebugBreak ( );
				}
#endif
				codeSegment.setdword ( fu, it.second->offset );
			}
		}
	}

	cacheString &getLabel ( uint32_t fixupOffset )
	{
		for ( auto &it : labels )
		{
			for ( auto &fu : it.second->fixups )
			{
				if ( fu == fixupOffset )
				{
					return it.second->label;
				}
			}
		}
		throw errorNum::scINTERNAL;
	}
};

class compTryCatchElem {
public:
	uint32_t		tryOffset;
	uint32_t		catchOffset;
	uint32_t		errSym;
	uint32_t		stackSize;

	compTryCatchElem ( uint32_t tryOffset, uint32_t catchOffset, uint32_t errSym, uint32_t stackSize ) : tryOffset ( tryOffset ), catchOffset ( catchOffset ), errSym ( errSym ), stackSize ( stackSize )
	{
	}
};

class compTryCatch {
public:
	std::vector<compTryCatchElem>		 tcList;

	void add ( uint32_t tryOffset, uint32_t catchOffset, uint32_t errSym, uint32_t stackSize )
	{
		tcList.push_back ( compTryCatchElem ( tryOffset, catchOffset, errSym, stackSize ) );
	}

	void addEndOfFunction ( )
	{
		add ( 0xFFFFFFFF, 0xFFFFFFFF, 0, 0 );	// end of list
	}

	void serialize ( BUFFER *buff )
	{
		fgxTryCatchEntry	entry = {};

		add ( 0xFFFFFFFF, 0xFFFFFFFF, 0, 0 );	// end of list
		for ( auto it = tcList.begin(); it != tcList.end(); it++ )
		{
			entry.catchOffset	= (*it).catchOffset;
			entry.tryOffset		= (*it).tryOffset;
			entry.errSym		= (*it).errSym;
			entry.stackSize		= (*it).stackSize;
			buff->put ( entry );
		}
	}
};

class compGlobalSymbol {
public:
	cacheString			  symbolName;
	cacheString			  name;
	cacheString			  enclosingName;
	srcLocation			  location;
	uint32_t			  globalIndex = 0;
	uint32_t			  nextExportable = 0;
	uint32_t			  left = 0;
	uint32_t			  right = 0;
	bool				  isExportable = true; 				// this is mainly used for linkage purposes...  only GLOBALS need to be linked up to outside units.   Things with file or class scope do not
	cacheString			  fileSymbolName;
	class	astNode		**init = nullptr;

	compGlobalSymbol ( srcLocation const &location, cacheString const &name, cacheString const &symbolName, bool isExportable) : symbolName ( symbolName ), name ( name ), location ( location ), isExportable (isExportable)
	{
		assert ( name.size ( ) );
	}
	compGlobalSymbol ( srcLocation const &location, cacheString const &name, cacheString const &symbolName, astNode **init, bool isExportable) : symbolName ( symbolName ), name ( name ), location ( location ), isExportable (isExportable), init ( init )
	{
		assert ( name.size ( ) );
	}
	compGlobalSymbol ( srcLocation const &location, cacheString const &name, cacheString const &enclosingName, cacheString const &symbolName, astNode **init, bool isExportable ) : symbolName ( symbolName ), name ( name ), enclosingName ( enclosingName ), location ( location ), isExportable ( isExportable ), init ( init )
	{
		assert ( name.size () );
	}
	~compGlobalSymbol()
	{
	}
};


struct compClassVTable {
	uint32_t		atom = 0;				// 0 if none, otherwise atom number
	int16_t			delta = 0;				// offset from self object to the destination object of atom or to the ivar

	compClassVTable ()
	{}

	compClassVTable ( uint32_t atom, int16_t objectDelta ) : atom ( atom ), delta ( objectDelta )
	{}

	compClassVTable ( int16_t ivarDelta ) : atom ( 0 ), delta ( ivarDelta )
	{}
};

class compVirtConstDest
{
	public:
	uint32_t	atom;
	int16_t		ojectStart;
	int16_t		objectStart;
};

class compClassElementSearch
{
public:
	fgxClassElementType		 type = fgxClassElementType::fgxClassType_method;
	fgxClassElementScope	 scope = fgxClassElementScope::fgxClassScope_public;
	cacheString 			 name = cacheString();
	cacheString 			 ns = cacheString();

	uint32_t				 index = 0;						// index into element array (index of self)

	class opClassElement	*elem = nullptr;

	uint32_t				 leftIndex[int (fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	uint32_t				 rightIndex[int (fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	uint32_t				 leftNSIndex[int (fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	uint32_t				 rightNSIndex[int (fgxClassElemSearchType::fgxClassElemMaxSearchType)];

	bool					 isVirtual = false;
	bool					 isStatic = false;

	cacheString				 symbolName;

	// structure to hold everything for ivars, methods, get-prop's, consts, inherits
	struct
	{
		union
		{
			uint32_t		 varIndex;
			uint32_t		 vTabEntry;
			uint32_t		 all;
			uint32_t		 globalIndex;
		}					 data = { 0 };
		int16_t				 objectStart = 0;				// offset of the object this element relates to (not used for virtual elements)
		uint32_t			 atom = -1;
		class opFunction	*func = nullptr;				// for methods... to make it easy to get the return type
	} methodAccess;

	// structure to hold everything for set-props
	struct
	{
		union
		{
			uint32_t		 varIndex;
			uint32_t		 vTabEntry;
			uint32_t		 all = 0xFFFF;
		}					 data;
		int16_t				 objectStart = 0;				// offset of the object this element relates to (not used for virtual elements)
		uint32_t			 atom = -1;
		class opFunction	*func = nullptr;				// for methods... to make it easy to get the return type
	} assign;

	compClassElementSearch ();
};

class compClassContext {
public:
	uint32_t			atom;
	int32_t				objectDelta;
	uint32_t			vTableOffset;

//	bool				virt;

	compClassContext ()
	{
		atom			= 0;
		vTableOffset	= 0;
		objectDelta		= 0;
	}
	compClassContext ( uint32_t atom, int32_t objectDelta, uint32_t vTableOffset ) : atom ( atom ), objectDelta ( objectDelta ), vTableOffset ( vTableOffset )
	{
	}
};

class compClass {
	class opFile							*oFile = nullptr;
public:
	class opClass							*oClass = nullptr;

	char									 name[MAX_NAME_SZ] = "";
	char									 nSpace[MAX_NAME_SZ] = "";

	bool									 isInterface = false;

	uint32_t								 atom = 0;
	uint32_t								 nVirtual = 0;				// number of virtual methods for THIS class, NOT including ancesters
	int16_t									 nIvar = 0;					// sized as if this was an object delta offset 

	std::deque<opClass *>					 elemStack;					// used to search for virtual overrides, not really a stack, but we erase elements as we recurse back up
	std::vector<uint32_t>					 overload;					// index into element of the operator
	std::vector<compClassElementSearch>		 elements;
	std::vector<compClassVTable>			 vTable;
	std::vector<compClassContext>			 ctxtInit;

	std::unordered_map<cacheString, size_t>				elemMap;

	std::vector<std::pair<errorNum, opClassElement *>>	lsErrors;

	// we have to ensure that virtual classes are instantiated in the correct order.  We may have a virtual class inherit from a virtual class.   We need to ensure that 
	// not only are virtual classes 
	std::vector<std::tuple<opClass *, opClass *>>	virtOrderRules;

	struct virtualClassPredClass
	{
		std::vector<std::tuple<opClass *, opClass *>>	const &virtOrderRules;
		virtualClassPredClass ( std::vector<std::tuple<opClass *, opClass *>> &rules ) : virtOrderRules ( rules )
		{}
		virtualClassPredClass ( virtualClassPredClass const &old ) : virtOrderRules ( old.virtOrderRules )
		{}
		virtualClassPredClass ( virtualClassPredClass &&old ) noexcept : virtOrderRules ( old.virtOrderRules )
		{}
		bool operator()( opClass const *l, opClass const *r ) const;
	} virtualClassPred;

	struct virtualClassInfo
	{
		class opClassElement	*elem = nullptr;
		cacheString				 ns;
		std::deque<opClass *>	 elemStack;
		int16_t					 objStart = 0;
	};

	std::map<opClass *, virtualClassInfo, decltype(virtualClassPred) >	virtualClasses;
	std::vector<std::tuple<opClass *, size_t>>							virtFixup;
	std::map<opClass *, size_t>											virtStart;

	std::vector<compClassElementSearch *>	 constructors;
	std::vector<compClassElementSearch *>	 destructors;
	std::vector<compClassElementSearch *>	 pack;
	std::vector<compClassElementSearch *>	 unpack;

	// the avl trees are used to build our search lists... they get passed down to inherited class's... used for both overriding virtual methods
	// and for adding public and protected elements to our supers search tree

	struct avl_table						*searchTree[int (fgxClassElemSearchType::fgxClassElemMaxSearchType)];
	struct avl_table						*nsSearchTree[int (fgxClassElemSearchType::fgxClassElemMaxSearchType)];

	uint32_t								 newBaseIndex = 0;
	uint32_t								 newIndex = 0;
	uint32_t								 cNewIndex = 0;
	uint32_t								 defaultMethodIndex = 0;
	uint32_t								 defaultAssignIndex = 0;
	uint32_t								 defaultAccessIndex = 0;
	uint32_t								 releaseIndex = 0;
	uint32_t								 releaseBaseIndex = 0;
	uint32_t								 cReleaseIndex = 0;
	uint32_t								 packIndex = 0;
	uint32_t								 unpackIndex = 0;

	compClass ( opFile *oFile, opClass *oClass );
	~compClass();

	uint32_t finalizeTree ( struct avl_table *tree, fgxClassElemSearchType searchType );
	uint32_t finalizeNSTree ( struct avl_table *tree, fgxClassElemSearchType searchType );

	void finalize ()
	{
		uint32_t	loop;

		for ( loop = 0; loop < int(fgxClassElemSearchType::fgxClassElemMaxSearchType); loop++ )
		{
			finalizeTree ( searchTree[loop], (fgxClassElemSearchType) loop );
			finalizeNSTree ( nsSearchTree[loop], (fgxClassElemSearchType) loop );
		}
	}

	void	addElement	(	fgxClassElementType		 type,
							fgxClassElementScope	 scope,
							 opClassElement			*e,
							cacheString const		&name,
							cacheString const		&ns,
							int16_t					 objectOffset,
							bool					 isVirtual,
							bool					 isStatic,
							bool					 isInterface,
							uint32_t				 data,
							class	opFunction		*func
						);
	void	addElement	(	 fgxClassElementType	type,
							 fgxClassElementScope	scope,
							 opClassElement			*e,
							 cacheString const		&name,
							 cacheString const		&ns,
							 int16_t				 objectOffset,
							 bool					 isVirtual,
							 bool					 isStatic,
							 bool					 isInterface,
							 uint32_t				 data,
							 cacheString const		&symbolName
	);
	void	addElement	(	fgxClassElementType		 type,
							fgxClassElementScope	 scope,
							 opClassElement			*e,
							cacheString const		&name,
							cacheString const		&ns,
							int16_t					 objectOffset,
							bool					 isVirtual,
							bool					 isStatic,
							bool					 isInterface,
							uint32_t				 atom,
							uint32_t				 data,
							class	opFunction		*func
						);
	void	addElement	(	fgxClassElementType		 type,
							fgxClassElementScope	 scope,
							 opClassElement			*e,
							cacheString const		&name,
							cacheString const		&ns,
							int16_t					 objectOffset,
							bool					 isVirtual,
							bool					 isStatic,
							bool					 isInterface,
						    uint32_t			 	 methodAccessAtom,
							uint32_t			 	 assignAtom,
							uint32_t				 dataMethodAccess,
							uint32_t				 dataAssign,
							class	opFunction		*methodAccessFunc,
							class	opFunction		*assignFunc
						);

	void	buildTree	( );

	compClassElementSearch *getElement		( fgxClassElemSearchType type, cacheString const &name );
	compClassElementSearch *getElement		( cacheString const &name );
	
	compClassElementSearch		*getDefaultMethod	( )
	{
		if ( !defaultMethodIndex ) return 0;
		return &elements[(int64_t)defaultMethodIndex -1];
	}
	compClassElementSearch		*getDefaultAccess	( )
	{
		if ( !defaultAccessIndex ) return 0;
		return &elements[(int64_t)defaultAccessIndex -1];
	}
	compClassElementSearch		*getDefaultAssign	( )
	{
		if ( !defaultAssignIndex ) return 0;
		return &elements[(int64_t)defaultAssignIndex -1];
	}

	bool						 isElement		( struct avl_table *tree, cacheString const &name, fgxClassElementType type );
	void						 make			( class compExecutable &comp, opClassElement *parentElem, opClass &oClass, fgxClassElementScope scope, int16_t &varPos, opClass *virtClass, std::vector<std::tuple<opClass *, uint32_t>> &virtFixup, bool virtInstantiation, std::map<cacheString, cacheString> messageMap, bool isLS );
	void						 make			( class compExecutable &comp, class opClass &oClass, bool isLS );
	void						 serialize		( BUFFER *buff, uint32_t *nElements, uint32_t *nCntxt, uint32_t *nVtab );
};

class compLister
{
	public:
	enum class listLevel
	{
		NONE,
		LIGHT,
		VERBOSE,
	}									level = listLevel::NONE;

	compCodeSegment *codeSegment;
	compDataSegment *dataSegment;
	atomList *atoms;

	class compExecutable *cFile;

	private:
	uint32_t							 lastOp;
	BUFFER								 annotation;

	stringi const &getXrefName ( uint32_t offset );
	public:
	compLister ( compCodeSegment *cs, compDataSegment *ds, atomList *a, class compExecutable *cFile ) : codeSegment ( cs ), dataSegment ( ds ), atoms ( a ), cFile ( cFile ), buff ( 1204 * 1024 * 16 )
	{
		lastOp = 1;
	}

	virtual ~compLister ()
	{}

	BUFFER								 buff;

	char const *getListing ()
	{
		if ( level < listLevel::LIGHT ) return "";

		bufferPut8 ( &buff, 0 );
		bufferUnPut ( &buff );
		return bufferBuff ( &buff );
	}

	void annotate ( symbolStack *sym, char const *format, ... )
	{
		if ( level < listLevel::LIGHT ) return;

		va_list argptr = {};

		emitOps( sym );

		va_start(argptr, format);
		if ( annotation.size() )
		{
			annotation.printf (  ", " );
			annotation.vprintf ( format, argptr );
		} else
		{
			annotation.vprintf ( format, argptr );
		}
	}
	
	void label ( symbolStack *sym, char const *label )
	{
		if ( label[0] == '%' ) return;
		if ( level < listLevel::LIGHT ) return;
		emitOps( sym );
		buff.setColor ( consoleColor::Red );
		buff.write ( label );
		buff.write ( ":\r\n" );
		buff.setColor ();
	}
	
	void label ( symbolStack *sym, stringi &label )
	{
		if ( label.c_str()[0] == '%' ) return;
		if ( level < listLevel::LIGHT ) return;
		emitOps ( sym );
		buff.setColor ( consoleColor::Red );
		buff.write ( label.c_str() );
		buff.write ( ":\r\n" );
		buff.setColor ( );
	}

	void printf ( char const *format, ... )
	{
		if ( level < listLevel::LIGHT ) return;
		va_list  argptr = {};;
		char		outBuff[1024];
		uint32_t	len;

		va_start(argptr, format);
		len = vsprintf_s ( outBuff, sizeof ( outBuff ), format, argptr );
		bufferWrite ( &buff, outBuff, len );
	}

	void		 setLastLine				( srcLocation const &location );
	void		 pushLine					( srcLocation const &location );
	void		 popLine					( srcLocation const &location );
	void		 emitSource					( stringi const &fName, uint32_t lineNum  );
	void		 emitContSource				( srcLocation const &location );
	void		 emitContSourceEnd			( srcLocation const &location );
	const char	*getsymbolTypeClassName		( symbolTypeClass const &type );
	const char	*getSymbolScopeName			( symbolSpaceScope scope );
	void		 emitFuncInfo				( opFunction *func );
	void		 emitClassInfo				( opClass *func );
	void		 emitSymbolInfo				( symbolStack *symStack , class symbol *sym );
	void		 emitOps					( symbolStack *sym );
	void		 printOutput				();
	void		 printOutput				( FILE * );
};

class compExecutable {
public:
	atomList						 atom;
	compCodeSegment					 codeSegment;
	compDataSegment					 dataSegment;
	compDebug						 debug;
	uint32_t						 bssSize;

	compLister						 listing;

	uint32_t						 labelId;

	optimizer						 opt;

	std::vector<compFunc *>			 funcDef;
	std::vector<compClass *>		 classDef;
	std::vector<cacheString>		 libraries;				// all libraries used to make this image
	std::vector<cacheString>		 modules;				// modules used to build this image...
	compTryCatch					 tcList;

	std::vector<visibleSymbols>		 symbols;				// visible symbols (visibility based upon op address)

	std::vector<compGlobalSymbol *>	 globals;
	std::vector<size_t>				 globalLinkable;			// vector of global variables that require linking to other sll's or primary executable
	size_t							 globalInitOffset = 0;
	uint32_t						 globalRoot = 0;			// for run-time search of globals (library support mainly as all referenced are fixed at compile time)
	uint32_t						 globalExportableRoot = 0;	// to allow us to only scan for exportable globals for fixup purposes

	class	opFile					*file = nullptr;

	class	opTransform				*transform = nullptr;

	compFixup						*fixup = nullptr; 					// transitory... only valid during instantiation of a particular function.

	char							 tmpStringBuffer[128]="";

	// stats
	uint64_t						 tOptimizer;
	uint64_t						 tBackEnd;

	std::vector<compClassElementSearch *>	opOverloads[int(fgxOvOp::ovMaxOverload)];

	// compilation options
private:
	bool							 genDebug;
	bool							 genJustMyCode = true;
	bool							 genProfiling;

public:
	compExecutable( opFile *file );
	compExecutable();
	~compExecutable();

	cacheString makeLabel ( char const *name );

	void clear();

	void enableDebugging ( )
	{
		genDebug = true;
	}
	void enableProfiling ( )
	{
		genProfiling = true;
		codeSegment.enableProfiling();
	}

	void							 serialize				( BUFFER *buff, bool isLibrary );

	// iteratest through the class's and processes the globals and compiles the class
	void							 makeClass				( bool isLS );
	void							 makeClass				( opClass *oClass, bool isLS );

	// builds the class definitions (including inherited elements)
	void							 compileClass			( opClass *oClass, bool isLS );

	// initiates the code emission 
	void							 genCode				( char const *entry );
	bool							 genLS					( char const *entry, int64_t sourceIndex );

	// generates code for individual functions
	void							 compileFunction		( opFunction *func );

	// processes globals.. may emit code for intializers
	void							 processGlobals				( int64_t sourceIndex, bool isLS );
	void							 compileGlobalInitializers	( bool isSLL = false );

	// BC Code emitters
	void							 compEmitDefaultValue	( opFunction *funcDef, symbolStack *sym, symbolTypeClass const &needType );
	class	symbolTypeClass			 compEmitNode			( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue );
	void							 compEmitContext		( class symbolStack *sym, compClassElementSearch *elem, int32_t index, bool isAccess, bool isVirtual );
	void							 compEmitContext		( class symbolStack *sym, cacheString const &symName, int32_t index, bool isAccess, bool isVirtual );
	class	symbolTypeClass			 compEmitBinaryOp		( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, fglOpcodes intOp, fglOpcodes doubleOp, fglOpcodes stringOp, fglOpcodes varOp, fglOpcodes intOpImm, fglOpcodes doubleOpImm );
	class	symbolTypeClass			 compEmitUnaryOp		( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, fglOpcodes intOp, fglOpcodes doubleOp, fglOpcodes stringOp, fglOpcodes varOp );
	class	symbolTypeClass			 compEmitOpAssign		( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, fglOpcodes opCode, fglOpcodes opLocalV, fglOpcodes opGlobalv, fglOpcodes opClassIvar, fglOpcodes opClassVirtIVar, fglOpcodes opRefv, fgxOvOp ovOp );
	class	symbolTypeClass			 compEmitAssign			( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue );
	class	symbolTypeClass			 compEmitIncOp			( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, bool pre, bool isInc, fglOpcodes localInt, fglOpcodes localDouble, fglOpcodes localVar, fglOpcodes globalInt, fglOpcodes globalDouble, fglOpcodes globalVar, fglOpcodes objVar, fglOpcodes objVarVirt, fgxOvOp ovOp );
	uint32_t						 compEmitConcat			( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue );
	class	symbolTypeClass			 compEmitFunc			( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue, bool tailCall );
	int32_t							 compEmitFuncParameters	( class opFunction *func, cacheString const &name, bool isAccess, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, int32_t nParamsNeeded, int32_t nParamsSent, int32_t numHidden, cacheString const &funcName, bool safeCalls );
	void							 compEmitLValue			( class opFunction *func, class astNode *node, class symbolStack *sym, symbolTypeClass const &needType, symbolTypeClass const &retType, bool safeCalls, bool needValue  );
	void							 compEmitFuncCall		( class opFunction *func, cacheString const &name, bool isAccess, class astNode *node, class symbolStack *sym, bool needValue, uint32_t nParams, bool tailCall, compClassElementSearch *elem );
	class	symbolTypeClass			 compEmitCast			( symbolTypeClass const &type, symbolTypeClass const &needType );

	void							 compMakeForEach		( class opFunction *func, bool isLS );
	void							 compMakeForEach		( int64_t sourceIndex, bool isLS );
	bool							 compMakeIterator		( class opFile *file, opFunction *func, bool generic, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS );
	void							 compMakeIterator		( bool generic, bool isLS );
	cacheString						 compMakeLambda			( class opFile *file, opFunction *func, std::vector<symbolLocal> &closures, bool isStatic, bool isExtension, bool isLS );
	void							 makeParentClosure		( int64_t sourceIndex );

	// utilize nodewalk to do their thing
	void							 combineBlocks			( );
	void							 combineBlocks			( class opFunction *func );
	void							 makeAtoms				( bool isLS, int64_t sourceIndex );
	void							 makeAtoms				( class opFunction *func, bool isLS );
	void							 pruneRefs				( );
	void							 pruneRefs				( class opFunction *func);
	void							 convertPsuedoObjCall	( bool isLS );
	void							 convertPsuedoObjCall	( class opFunction *func, bool isLS);
	void							 makeCodeblock			( );
	void							 makeCodeblock			( class opFunction *func );
	void							 resetSymbols			( );
	void							 resetSymbols			( class opFunction *func);
	void							 makeSymbols			( bool isLS, int64_t sourceIndex );
	void							 makeSymbols			( class opFunction *func, bool isLS );
	bool							 hasFunctionReference	( );
	bool							 hasFunctionReference	( class opFunction *func );
	void							 fixupGoto				( class astNode *node );
	void							 fixupGoto				( class opFunction *func );
	void							 fixupGoto				();
	void							 compGenErrorsWarnings		( opFunction *func, bool isLS );
	void							 compGenErrorsWarnings		( bool isLS );
	void							 genWrapper				( opFunction *func );
	void							 genWrapper				( int64_t sourceIndex );
	void							 makeConstructorDestructors ( opClass *cls, bool &needsNew, bool &needsNew_base, bool &needsRelease, bool &needsRelease_base, bool isLS );
	void							 makeConstructorDestructors ( opClass *cls, bool isLS );
	void							 makeConstructorDestructors ( bool isLS, int64_t sourceIndex );
	bool							 makeExtensionMethods	( opClass *baseClass, opClass *cls, bool generic, accessorType const &acc, unique_queue<accessorType> *scanQueue );
	bool							 makeExtensionMethod 	( opClass *cls, bool generic, accessorType const &acc, unique_queue<accessorType> *scanQueue );
	void							 makeExtensionMethods	( bool generic, int64_t sourceIndex );
	void							 makeDot				( char const *fPattern );
	void							 makeDot				( class opFunction *func, char const *fPattern );

	void							 getInfo				( astLSInfo &info, uint32_t sourceIndex );

private:
	void							compTagFinally			();
};

// NOTE: MUST be at the end so that we resolve friends correctly... make iterators specifically
#include "compilerParser/fileParser.h"
