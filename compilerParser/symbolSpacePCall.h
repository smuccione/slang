/*

	Symbol space

*/

#pragma once

#include <vector>

#include "symbolSpace.h"
#include "Utility/stringCache.h"

class symbolSpacePCall : public symbol
{
	// symbol query and setting
	public:
	symbolSpacePCall() : symbol ( symbolSpaceType::symTypePCALL )
	{}
	bool					 isDefined				( cacheString const &name, bool isAccess ) const override { return false; };
	bool					 isDefinedNS			( cacheString const &name, bool isAccess ) const override { return false; };
	int32_t					 getRollup				( void ) const override { return 1; };
	bool					 isStackBased			( void ) const override { return true; };
	cacheString const		 getSymbolName ( symbolSpaceScope scope, uint32_t index ) const override
	{
		return cacheString ();
	}
	size_t					 size					( ) const override { return 0; };
	friend class symbolStack;
};

// allows us to find out where our symbols are in a dynamically 
class symbolSpaceStackValue: public symbol
{
	public:
	symbolSpaceStackValue ( ) : symbol ( symbolSpaceType::symTypeStackValue )
	{}
	bool					 isDefined ( cacheString const &name, bool isAccess ) const override { return false; };
	bool					 isDefinedNS ( cacheString const &name, bool isAccess ) const override { return false; };
	int32_t					 getRollup ( void ) const override { return 1; };
	bool					 isStackBased ( void ) const override { return true; };
	cacheString const		 getSymbolName ( symbolSpaceScope scope, uint32_t index ) const override
	{
		return cacheString ();
	}
	size_t					 size ( ) const override { return 0; };
	friend class symbolStack;
};

// sets demarcation for inline functions for function/class resolution
class symbolSpaceInline : public symbol
{
	// symbol query and setting
	public:
	cacheString					 funcName;

	symbolSpaceInline () : symbol ( symbolSpaceType::symTypeInline )
	{}
	symbolSpaceInline ( cacheString &funcName ) : symbol ( symbolSpaceType::symTypeInline ), funcName ( funcName )
	{}
	bool					 isDefined ( cacheString const &name, bool isAccess ) const override { return false; };
	bool					 isDefinedNS ( cacheString const &name, bool isAccess ) const override { return false; };
	int32_t					 getRollup ( void ) const override { return 0; };
	bool					 isStackBased ( void ) const override { return false; };
	cacheString const		 getSymbolName ( symbolSpaceScope scope, uint32_t index ) const override
	{
		return cacheString ();
	}
	size_t					 size () const override { return 0; };
	friend class symbolStack;
};

