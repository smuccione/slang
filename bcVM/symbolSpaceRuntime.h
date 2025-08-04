/*

	Symbol space

*/

#pragma once

#include "stdint.h"
#include "compilerParser/symbolSpace.h"
#include <vector>

class symbolSpaceRuntime : public symbolSpace
{
	class vmInstance		*instance;

public:

	symbolSpaceRuntime ( class vmInstance *instance ) : instance ( instance )
	{
	}
	symbolSpaceRuntime ( void ) {};

	bool						 isDefined				( char const *name ) const;
	symbolSpaceScope			 getScope				( char const *name, bool isAssign ) const;
	class symbolTypeClass		 getType				( char const *name ) const;
	const char					*getSymbolName			( enum symbolSpaceScope scope, size_t index ) const;
	void						 setParameterType		( char const *name, size_t pNum, symbolTypeClass &type, bool *typeUpdated);
	void						 setParameterTypeNoThrow( char const *name, size_t pNum, symbolTypeClass &type, bool *typeUpdated );
	size_t						 getIndex				( char const *name ) const;
	char const					*getFuncName			( char const *name ) const;
	size_t						 getFuncNumParams		( char const *name ) const;
	class astNode				*getFuncDefaultParam	( char const *name, int32_t nParam ) const;
	symbolTypeClass				 getFuncReturnType		( char const *name, accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const & ) const;
	symbolTypeClass				 getFuncParamType		( char const *name, int32_t nParam ) const;
	fgxFuncCallConvention		 getFuncCallingConv		( char const *name ) const;
	bool						 isFunc					( char const *name ) const;

	void						 setAssigned			( char const *name ) { };
	void						 setAccessed			( char const *name ) { }; 
	void						 setType				( char const *name, symbolTypeClass const &type ) { };
	void						 setType				( char const *name, char const *className ) { };

	class compClass				*findClass				( char const *name ) const;

};

