/*

	Symbol space

*/

#pragma once

#include <vector>

class symbolSpaceNamespace : public symbol
{
	class opFile			*file = 0;
	class opNamespace		*ns = 0;
	class opUsingList		*useList = 0;

public:
	symbolSpaceNamespace() : symbol ( symbolSpaceType::symTypeNamespace ), ns ( nullptr ), useList ( nullptr )
	{
	}
	symbolSpaceNamespace ( class opFile *file, class opNamespace *ns, opUsingList *useList) : symbol ( symbolSpaceType::symTypeNamespace ),  file ( file ), ns ( ns ), useList ( useList )
	{
	}
	symbolSpaceNamespace ( class opFile *file ) : symbol ( symbolSpaceType::symTypeNamespace ), file ( file )
	{};

	symbolSpaceNamespace ( const symbolSpaceNamespace &old ) = default;
	symbolSpaceNamespace ( symbolSpaceNamespace &&old ) noexcept = default;
	symbolSpaceNamespace &operator = ( const symbolSpaceNamespace &old ) = default;
	symbolSpaceNamespace &operator = ( symbolSpaceNamespace &&old ) noexcept = default;

	bool					 isDefined				( cacheString const &name, bool isAccess ) const override;
	bool					 isDefinedNS			( cacheString const &name, bool isAccess ) const override;
	symbolSpaceScope		 getScope				( cacheString const &name, bool isAccess ) const override;
	symbolTypeClass	const	 getType				( cacheString const &name, bool isAccess ) const override;
	cacheString const		 getSymbolName			( symbolSpaceScope scope, uint32_t index ) const override;
	int32_t					 getIndex				( cacheString const &name, bool isAccess ) const override;
	cacheString const		 getFuncName			( cacheString const &name, bool isAccess ) const override;
	uint32_t				 getFuncNumParams		( cacheString const &name, bool isAccess ) const override;
	astNode					*getFuncDefaultParam	( cacheString const &name, bool isAccess, int32_t nParam ) const override;
	fgxFuncCallConvention	 getFuncCallingConv		( cacheString const &name, bool isAccess ) const override;
	size_t					 getFuncSourceIndex		( cacheString const &name, bool isAccess ) const override;
	bool					 isFunc					( cacheString const &name, bool isAccess ) const override;
	bool					 isClass				( cacheString const &name, bool isAccess ) const override;
	astNode					*getInitializer			( cacheString const &name  ) const noexcept override;
	int16_t					 getObjectOffset		( cacheString const &name, bool isAccess ) const override;

	// set interest in type changes
	void					 setAccessed			( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	symbolTypeClass const    getFuncReturnType		( class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const override;
	symbolTypeClass const    getMarkFuncReturnType	( class compExecutable *comp, class symbolStack const *sym, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS ) const override;
	symbolTypeClass	const	 getFuncParamType		( cacheString const &name, bool isAccess, int32_t nParam, accessorType const &acc, unique_queue<accessorType> *scanQueue ) const override;

	bool					 isMethod				( cacheString const &name, bool isAccess ) const override;
	bool					 isIterator				( cacheString const &name, bool isAccess ) const override;
	bool					 isStatic				( cacheString const &name, bool isAccess ) const override;
	bool					 isExtension			( cacheString const &name, bool isAccess ) const override;
	bool					 isVariantParam			( cacheString const &name, bool isAccess ) const override;
	bool					 isPcountParam			( cacheString const &name, bool isAccess ) const override;
	bool					 isVirtual				( cacheString const &name, bool isAccess ) const override;
	bool					 isIVar					( cacheString const &name ) const override;
	bool					 isInterface			( cacheString const &name, bool isAccess ) const override;
	bool					 isConst				( cacheString const &name, bool isAccess ) const override;
	bool					 isPure					( cacheString const &name, bool isAccess ) const override;

	bool					 setType				( cacheString const &name, bool isAccess, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	void					 setType				( cacheString const &name, bool isAccess, cacheString const &className ) override { };
	void					 setParameterType		( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	void					 setParameterTypeNoThrow( class compExecutable *comp, class symbolStack *sym, cacheString const &name, bool isAccess, uint32_t pNum, symbolTypeClass const &type, accessorType const &acc, unique_queue<accessorType> *scanQueue ) override;
	class compClass			*findClass				( cacheString const &name ) const override;

	size_t					 size					( ) const override { return 0; };

	cacheString const		 getResolvedName		( cacheString const &name, bool isAccess ) const override;
	symbolSource			 getSymbolSource		( cacheString const &className ) override;
	srcLocation const		&getDefinitionLocation	( cacheString const &name, bool isAccess ) const override;
};

