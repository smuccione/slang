/*

	Symbol space

*/

#pragma once

#include <vector>
#include <string>

class symbolClass: public symbol
{
	opFile	*file = 0;

public:
	cacheString		className;
	size_t			insertPos = 0;
	symbolTypeClass type;

	symbolClass ( ) : symbol ( symbolSpaceType::symTypeClass )
	{
	}

	symbolClass ( class opFile *file, cacheString const &className, size_t insertPos ) : symbol ( symbolSpaceType::symTypeClass ), file ( file ), className ( className ), insertPos ( insertPos ), type ( symbolType::symObject, className )
	{
	}

	symbolClass ( class opFile *file, const char **expr );

	symbolClass ( const symbolClass &old ) : symbol ( symbolSpaceType::symTypeClass )
	{
		file = old.file;
		className = old.className;
		insertPos = old.insertPos;
		type = old.type;
	}

	symbolClass ( symbolClass &&old )  noexcept : symbol ( symbolSpaceType::symTypeClass )
	{
		*this = std::move ( old );
	}

	symbolClass &operator =  ( const symbolClass &old )
	{
		file = old.file;
		className = old.className;
		insertPos = old.insertPos;
		type = old.type;
		return *this;
	}
	symbolClass &operator =  ( symbolClass &&old ) noexcept
	{
		std::swap ( file, old.file );
		std::swap ( className, old.className );
		std::swap ( insertPos, old.insertPos );
		std::swap ( type, old.type );
		return *this;
	}

	void serialize ( BUFFER *buff ) override
	{
		bufferWrite ( buff, (char *) className.c_str ( ), className.length ( ) + 1 );
		buff->put<size_t> ( insertPos );
	}

	// symbol query and setting
	class compClassElementSearch	*operator []	( cacheString const &name ) const;
	bool					 isDefined				( cacheString const &name, bool isAccess ) const override;
	bool					 isDefinedNS			( cacheString const &name, bool isAccess ) const override;

	// calling these functions registers interest in type change
	void					 setAccessed			( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) override;
	symbolTypeClass const    getFuncReturnType		( class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc ) const override;
	symbolTypeClass const    getMarkFuncReturnType	( class compExecutable *compDef, class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS, srcLocation const &loc ) const override;

	bool					 setType				( cacheString const &name, bool isAccess, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	void					 setType				( cacheString const &name, bool isAccess, cacheString const &className ) override {};
	void					 setParameterType		( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	void					 setParameterTypeNoThrow( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, class symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;

	 bool					 isFunc					( cacheString const &name, bool isAccess ) const override;
	 bool					 isProperty				( cacheString const &name, bool isAccess ) const override;
	 fgxFuncCallConvention	 getFuncCallingConv		( cacheString const &name, bool isAccess ) const override;

	bool					 isMethod				( cacheString const &name, bool isAccess ) const override;
	bool					 isExtension			( cacheString const &name, bool isAccess ) const override;
	bool					 isVirtual				( cacheString const &name, bool isAccess ) const override;
	bool					 isStatic				( cacheString const &name, bool isAccess ) const override;
	bool					 isVariantParam			( cacheString const &name, bool isAccess ) const override;
	bool					 isIVar					( cacheString const &name ) const override;

	cacheString const		 getFuncName			( cacheString const &name, bool isAccess ) const override;
	uint32_t				 getFuncNumParams		( cacheString const &name, bool isAccess ) const override;
	symbolTypeClass	const	 getFuncParamType		( cacheString const &name, bool isAccess, int32_t nParam, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const override;
	astNode					*getFuncDefaultParam	( cacheString const &name, bool isAccess, int32_t nParam ) const override;

	size_t					 size ( )				const override { return 0; };

	symbolTypeClass	const	 getType				( cacheString const &name, bool isAccess )  const override;
	symbolSpaceScope		 getScope				( cacheString const &name, bool isAccess ) const override;
	int32_t					 getIndex				( cacheString const &name, bool isAccess ) const override;

	int16_t					 getVtabEntry			( cacheString const &name, bool isAccess ) const override;
	int16_t					 getObjectOffset		( cacheString const &name, bool isAccess ) const override;
	compClass				*getClass				( void ) const override;
	cacheString const		 getSymbolName			( enum symbolSpaceScope scope, uint32_t index ) const  override;
	symbolSource			 getSymbolSource		( cacheString const &className ) override;
	srcLocation	const		&getDefinitionLocation	( cacheString const &name, bool isAccess ) const override;

	// function query
	friend class symbolStack;
};
