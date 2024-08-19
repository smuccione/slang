/*

		Symbol space

*/

#pragma once

#include "Utility/stringCache.h"
#include "expressionParser.h"
#include "errorHandler.h"
#include "compilerAST/astNode.h"

enum class symbolSpaceType {
	symTypeLocal,
	symTypeClass,
	symTypeStatic,
	symTypeField,
	symTypePCALL,
	symTypeParam,
	symTypeNamespace,
	symTypeStackValue,
	symTypeInline,
};

enum class symbolSpaceScope {
	symGlobal			= 1,
	symLocal,
	symLocalStatic,			
	symLocalField,
	symLocalParam,
	symPCall,
	symLocalConst,
	symClassConst,
	symClassAssign,
	symClassAccess,
	symClassMethod,
	symClassStatic,
	symClassIVar,
	symClassInherit,
	symClass,
	symFunction,
	symScopeUnknown
};

struct visibleSymbols {
	cacheString	 name;
	uint32_t	 validStart = 0;			// offsets in code segment where this symbols is valid
	uint32_t	 validEnd = 0;
	int32_t		 index = 0;					// stack offset from basePointer or index into globals
	bool		 noRef = false;
	bool		 isStackBased = false;
	bool		 isParameter = false;
};

class symbol
{
public:
	symbolSpaceType	symType;
	symbol ( symbolSpaceType symType )  noexcept : symType ( symType )
	{
	}

	virtual ~symbol ()
	{};

	// symbol query and setting
	virtual	bool					 isDefined				( cacheString const &name, bool isAccess ) const = 0;
	virtual	bool					 isDefinedNS			( cacheString const &name, bool isAccess ) const = 0;
	virtual bool					 isAccessed				( cacheString const &name, bool isAccess ) const { return true; };
	virtual bool					 isAssigned				( cacheString const &name, bool isAccess ) const { return true; };
	virtual bool					 isInitialized			( cacheString const &name, bool isAccess ) const { return true; };
	virtual bool					 isClosedOver			( cacheString const &name ) const { return false; };
	virtual	bool					 isMethod				( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isIterator				( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isExtension			( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isStatic				( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isVariantParam			( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isPcountParam			( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isVirtual				( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isIVar					( cacheString const &name ) const { return false; };
	virtual	bool					 isConst				( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isPure					( cacheString const &name, bool isAccess ) const { return false; };
	virtual	bool					 isInterface			( cacheString const &name, bool isAccess ) const { return false; };
	virtual	symbolTypeClass	const	 getType				( cacheString const &name, bool isAccess ) const { static symbolTypeClass sym ( symbolType::symUnknown ); return sym; };
	virtual int32_t					 getRollup				( void ) const { return 0; };
	virtual bool					 isStackBased			( void ) const { return false; };
	virtual	cacheString const		 getSymbolName			( symbolSpaceScope scope, uint32_t index ) const { throw errorNum::scINTERNAL; };
	virtual	cacheString const		 getSymbolName			( void ) const { throw errorNum::scINTERNAL; };
	virtual	symbolSpaceScope		 getScope				( cacheString const &name, bool isAccess ) const { throw errorNum::scINTERNAL; };
	virtual	int32_t					 getIndex				( cacheString const &name, bool isAccess ) const { throw errorNum::scINTERNAL; };
	virtual	class compClass			*findClass				( cacheString const &name ) const { return 0; };
	virtual void					 setDocumentation		( cacheString const &name, stringi const &documentation ) {};
	virtual stringi const			&getDocumentation		( cacheString const &name, bool isAccess ) { throw errorNum::scINTERNAL; };

	// these functions not only set used, but also register interest in the type of the result.  this allows us to iterate over the changes during type inferencing
	virtual	void					 setAllLocalAccessed	( accessorType const &acc, unique_queue<accessorType> *scanQueue ) {};
	virtual	void					 setAccessed			( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) { throw errorNum::scINTERNAL; };
	virtual	void					 setInlineUsed			( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) {};
	virtual	symbolTypeClass	const	 getFuncReturnType		( class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const { throw errorNum::scINTERNAL; };
	virtual	symbolTypeClass	const	 getMarkFuncReturnType	( class compExecutable *comp, class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS ) const { throw errorNum::scINTERNAL; };
	virtual	symbolTypeClass	const	 getFuncParamType		( cacheString const &name, bool isAccess, int32_t nParam, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const { throw errorNum::scINTERNAL; };

	// these functions can change type or access state and therefore can update a scan queue
	virtual bool					 setType				( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) { throw errorNum::scINTERNAL; };
	virtual void					 setInitType			( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) { throw errorNum::scINTERNAL; };	// specialized for initializers
	virtual	void					 setParameterType		( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) { throw errorNum::scINTERNAL; };
	virtual	void					 setParameterTypeNoThrow( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) { throw errorNum::scINTERNAL; };

	// used ONLY to set class type of self or similar.  it is used PRE type inferencing and therefore does not need scanQueue access
	virtual void					 setType				( cacheString const &name, bool isAccess, cacheString const &className ) { throw errorNum::scINTERNAL; };

	// book keeping functions
	virtual	void					 setInitialized			( cacheString const &name, bool isAccess ) { throw errorNum::scINTERNAL; };
	virtual	void					 setClosedOver			( cacheString const &name ) { throw errorNum::scINTERNAL; };

	// class query
	virtual	int16_t					 getVtabEntry			( cacheString const &name, bool isAccess ) const { return -1; }; 
	virtual	int16_t					 getObjectOffset		( cacheString const &name, bool isAccess ) const { return -1; };
	virtual	compClass				*getClass				( void ) const { return 0; };

	// function query
	virtual bool					 isFunc					( cacheString const &name, bool isAccess ) const  { return false; };
	virtual bool					 isClass				( cacheString const &name, bool isAccess ) const  { return false; };
	virtual bool					 isProperty				( cacheString const &name, bool isAccess ) const  { return false; };
	virtual fgxFuncCallConvention	 getFuncCallingConv		( cacheString const &name, bool isAccess ) const  { throw errorNum::scINTERNAL; }

	virtual cacheString const		 getFuncName			( cacheString const &name, bool isAccess ) const
	{
		return cacheString ();
	}

	virtual	size_t					 getFuncSourceIndex		( cacheString const &name, bool isAccess ) const { throw errorNum::scINTERNAL; };
	virtual	uint32_t				 getFuncNumParams		( cacheString const &name, bool isAccess ) const { throw errorNum::scINTERNAL; };
	virtual	class astNode			*getFuncDefaultParam	( cacheString const &name, bool isAccess, int32_t nParam ) const { throw errorNum::scINTERNAL; };
	virtual	class astNode			*getInitializer			( cacheString const &name ) const noexcept { return 0; };

	virtual	bool					 getFgxSymbol			( visibleSymbols &fgxSym, size_t index = 0 ) const { return false; }
	virtual size_t					 size					( ) const { throw errorNum::scINTERNAL; };
	virtual	void					 setInitializer			( cacheString const &name, astNode *initializer ) { throw errorNum::scINTERNAL; };

	virtual void					 serialize				( BUFFER *buff ) 
	{ 
		bufferWrite ( buff, &symType, sizeof ( symType ) );
	};

	virtual	void					 rename					( cacheString const &oldName, cacheString const &newName ) { throw errorNum::scINTERNAL; };
	virtual void					 resetType				( class opFile *file ) {};

	virtual symbolSource			 getSymbolSource		( cacheString const &name )
	{
		return std::monostate ();
	}
	virtual	srcLocation	const		&getDefinitionLocation	( cacheString const &name, bool isAccess ) const
	{
		throw errorNum::scINTERNAL;
	};

	virtual	cacheString const		 getResolvedName		( cacheString const &name, bool isAccess ) const
	{
		return name;
	};

	static symbol *symbolFactory ( class opFile *file, class sourceFile *srcFile, const char **expr );

	friend class symbolStack;
};

#include "symbolSpaceLocal.h"
#include "symbolSpaceStatic.h"
#include "symbolSpaceField.h"
#include "symbolSpacePCall.h"
#include "symbolSpaceParam.h"
#include "symbolSpaceClass.h"
#include "symbolSpaceClosure.h"
#include "symbolSpaceNamespace.h"
#include "symbolSpaceLocalAdd.h"

#include "nameSpace.h"

class symbolStack {
	// we need to use either references or pointers here
	// we can't have a stack of value types... aside from the pushing-popping being expensive, we actually need to work with the symbols during different procesing streams
	std::vector<symbol *>		 syms;
public:
	/*
		the following are simply storage spots for things that we intend to push onto the stack.
		this is purely convenience.   we provide a symbolStack constructor that file and func.   it then 
		pushes the appropriate symbols (namespace, parameters, pcall, class, parentclass, etc.) onto the stack
		but to do this it needs the structures to be able to push them onto the stack.   without this convenience
		anyone initializing a symbol stack would need to create those structures and push them, leading to lots of
		code duplication as well as well as making it prone to error and/or modification
	*/
	opFile							*file;			// necessary to get access to source file name
	symbolSpaceNamespace			 ns;
	symbolSpacePCall				 sPCall;
	symbolClass						 sClass;
	symbolClass						 sParentClass;
	opUsingList 					 usingList;

	symbolStack ( class opFile *file, class opFunction *func = 0, class symbolSpaceLocalAdd *localAdd = 0 );
	symbolStack ( const symbolStack& old )
	{
		file = old.file;
		syms = old.syms;
	}
	symbolStack ( symbolStack&& old )  noexcept
	{
		*this = std::move ( old );
	}
	symbolStack& operator = ( const symbolStack &old )
	{
		file = old.file;
		syms = old.syms;
		return *this;
	}

	symbolStack& operator = ( symbolStack &&old ) noexcept
	{
		std::swap ( file, old.file );
		std::swap ( syms, old.syms );
		std::swap ( ns, old.ns );
		std::swap ( sPCall, old.sPCall );
		std::swap ( sClass, old.sClass );
		std::swap ( sParentClass, old.sParentClass );
		return *this;
	}

	size_t						 size ( )
	{
		return syms.size ( );
	}
	void						 resize ( size_t size )
	{
		syms.resize ( size );
	}

	void						 setClosedOver	( cacheString const &name );
	bool						 isAccessed		( cacheString const &name, bool isAccess ) const;
	bool						 isAssigned		( cacheString const &name, bool isAccess ) const;
	bool						 isInitialized	( cacheString const &name, bool isAccess ) const;
	bool						 isClosedOver	( cacheString const &name ) const;
	bool						 isDefined		( cacheString const &name, bool isAccess ) const;
	bool						 isDefinedNS	( cacheString const &name, bool isAccess ) const;
	bool						 isMethod		( cacheString const &name, bool isAccess ) const;
	bool						 isIterator		( cacheString const &name, bool isAccess ) const;
	bool						 isExtension	( cacheString const &name, bool isAccess ) const;
	bool						 isStatic		( cacheString const &name, bool isAccess ) const;
	bool						 isVariantParam	( cacheString const &name, bool isAccess ) const;
	bool						 isPcountParam  ( cacheString const &name, bool isAccess ) const;
	bool						 isVirtual		( cacheString const &name, bool isAccess ) const;
	bool						 isIVar			( cacheString const &name, bool isAccess ) const;
	bool						 isInterface	( cacheString const &name, bool isAccess ) const;
	bool						 isConst		( cacheString const &name, bool isAccess ) const;
	bool						 isPure			( cacheString const &name, bool isAccess ) const;

	cacheString const			 getSymbolName	( symbolSpaceScope scope, uint32_t index ) const;
	symbolSpaceScope			 getScope		( cacheString const &name, bool isAccess) const;
	int32_t						 getIndex		( cacheString const &name, bool isAccess ) const;


	int16_t						 getVtabEntry	( cacheString const &name, bool isAccess ) const;
	int16_t						 getObjectOffset( cacheString const &name, bool isAccess ) const;
	compClass					*getClass		( void ) const ;

	symbolTypeClass	const		 getType		( cacheString const &name, bool isAccess ) const;
	bool						 isFuncBC		( cacheString const &name, bool isAccess ) const;
	bool						 isFuncC		( cacheString const &name, bool isAccess ) const;
	bool						 isFunc			( cacheString const &name, bool isAccess ) const;
	bool						 isClass		( cacheString const &name, bool isAccess ) const;
	bool						 isProperty		( cacheString const &name, bool isAccess ) const;

	// returns the number of storage locations on the stack for this sym instance.
	// in general sym instances are created when procsessing a function, so this will be on a per-function basis
	// but we have no general limitation 
	uint32_t					 getStackSize ( )
	{
		uint32_t	size = 0;
		for ( auto &it : syms )
		{
			size += it->getRollup ( );
		}
		return size;
	}

	void						 insert		( symbol *sym, size_t pos )
	{
		syms.insert ( syms.end () - pos, sym );
	}
	void						 push		( symbol *sym )
	{
		syms.push_back ( sym );
	}
	void						 pop ( size_t cnt = 1 )
	{
		while ( cnt-- )
		{
			syms.pop_back ( );
		}
	}

	void						 pushStack	( size_t cnt = 1 )
	{
#if _DEBUG
		// we do this crap otherwise it looks all messed up under natvis.  for some reason it insists on adding display items just because it's a static pointer
		while ( cnt-- )
		{
			symbolSpaceStackValue *stack = new symbolSpaceStackValue ();
			syms.push_back ( stack );
		}
#else
		static symbolSpaceStackValue stack;
		while ( cnt-- )
		{
			syms.push_back ( &stack );
		}
#endif
	}

	void						 pushStack ( char const *format, ... )
	{
#if _DEBUG
		// we do this crap otherwise it looks all messed up under natvis.  for some reason it insists on adding display items just because it's a static pointer
		va_list defParams;
		va_start ( defParams, format );
		symbolSpaceStackValue *stack = new symbolSpaceStackValue ( format, defParams );
		va_end ( defParams );
		syms.push_back ( stack );
#else
		static symbolSpaceStackValue stack;
		syms.push_back ( &stack );
#endif
	}

	void						 popStack	( size_t cnt = 1 )
	{
#if _DEBUG
		while ( cnt-- )
		{
			assert ( syms.back ()->symType == symbolSpaceType::symTypeStackValue );
			delete syms.back ();
			syms.pop_back ();
		}
#else
		while ( cnt-- )
		{
			assert ( syms.back ( )->symType == symbolSpaceType::symTypeStackValue );
			syms.pop_back ( );
		}
#endif
	}

	compClass					*findClass				( cacheString const &name ) const;


	void						 setParameterType		( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue );
	void						 setParameterTypeNoThrow( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue );
	cacheString const			 getFuncName			( cacheString const &name, bool isAccess ) const;									// converts from the scoped name to a potentially fully decorated class name
	class astNode				*getFuncDefaultParam	( cacheString const &name, bool isAccess, int32_t nParam ) const;
	int32_t						 getFuncNumParams		( cacheString const &name, bool isAccess ) const;
	symbolTypeClass				 getFuncReturnType		( cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue  ) const;
	symbolTypeClass				 getMarkFuncReturnType	( class compExecutable *comp, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS ) const;
	size_t						 getFuncSourceIndex		( cacheString const &name, bool isAccess ) const;
	class astNode				*getInitializer			( cacheString const &name ) const;

	char const					*getInlineFunc			(  ) const;
	uint32_t					 getInlineFuncRetIndex	(  ) const;
	bool						 getInlineFuncNeedValue	(  ) const;

	// need to pacc accessor to register interest in type changes
	void						 setAllLocalAccessed	( accessorType const &acc, unique_queue<accessorType> *scanQueue ); // when we have code-blocks we can NOT optimize away any in-scope symbols... we therefore must mark them all as in-use (which they can potentially be)
	symbolTypeClass				 getFuncParamType		( cacheString const &name, bool isAccess, int32_t nParam, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const;
	void						 setAccessed			( cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue );

	bool						 setType				( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue );
	void						 setType				( cacheString const &name, bool isAccess, cacheString const &className );

	symbolSource				 getSymbolSource		( cacheString const &name ) const;

	cacheString const			 getResolvedName		( cacheString const &name, bool isAccess ) const;


//	template <typename ...T>
//	friend astNode *astNodeWalk ( astNode *block, class symbolStack *sym, astNode *(*cb) (astNode *node, astNode *parent, symbolStack *sym, bool isAccess, T &&...t), T &&...t );

	friend bool		 inlineBlock ( astNode *block, symbolStack *sym, opFunction *func, std::vector<opFunction *> &inlineNest );
};

