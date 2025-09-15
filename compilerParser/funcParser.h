/*

	block parser definitions

*/

#pragma once

#include "fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"


class opFunction
{
	public:
	size_t							 index = 0;
	bool							 inUse = false;
	bool							 inUseSave = false;
	bool							 inlineUse = false;				// keep the function definition around for bookkeeping purposes, however don't emit it it and don't typeinfer it
	bool							 needScan = false;
	bool							 isForcedVariant = false;
	bool							 inlineBlockerPresent = false;
	bool							 toBeDeleted = false;

	fgxFuncCallConvention			 conv = fgxFuncCallConvention::opFuncType_cDecl;
	bool							 isInterface = false;

	cacheString						 name;
	stringi							 documentation;
	stringi							 returnDocumentation;

	srcLocation						 location;
	srcLocation						 extendedSelection;
	srcLocation						 nameLocation;

	bool							 isIterator = false;
	symbolTypeClass					 retType = symUnknownType;
	symbolSpaceParams				 params;

	bool							 isBaseMethod = false;

	bool							 isFGL = false;					// flag to signal FGL source code... this is important to implment fgl scoping rules
	opClass *classDef = nullptr;
	opUsingList						 usingList;

	bool							 isExtension = false;			// this is an extension method
	bool							 isStatic = false;				// this is a static method
	bool							 isVariantParam = false;		// last parameter is a var arg array
	bool							 isProcedure = true;			// true if function does not return a value
	bool							 isPcountParam = false;
	bool							 isPure = false;
	bool							 isConst = false;
	bool							 isLambda = false;
	bool							 lsIgnore = false;				// set to true for generated methods that the language server should ignore

	bool							 isAutoMain = false;

	// defines for certain parasitic operations
	bool							 unknownClassInstantiation = false;
	bool							 unknownFunctionCall = false;

	astNode *codeBlock = nullptr;			// ast block for this function (if any, may be a native block so may not exist)
	astNode *nonInlinedBlock = nullptr;		// for inlining, so we can get better inline performance when using weights

	std::recursive_mutex			 accessorLock;
	std::unordered_multimap<accessorType, srcLocation> accessors;						// for type inferencing in case our type changes = these are callers
	std::unordered_multimap<accessorType, srcLocation> called;						// for type inferencing in case our type changes = these are called

	std::vector<astNode *>			 initializers;

	std::unordered_map<cacheString, std::pair<astNode *, bool>> labelList;

	cacheString						 parentClosure;

	size_t							 weight = 0;

	opFunction ( class opFile *file );
	opFunction ( class opFile *file, sourceFile *srcFile, char const **expr );

	opFunction ( opFunction const &old );
	opFunction ( opFunction &&old ) = delete;

	opFunction *setPure ()
	{
		isPure = true;
		return this;
	}
	opFunction *setConst ()
	{
		isConst = true;
		return this;
	}

	virtual ~opFunction ();

	void	 serialize ( BUFFER *buff, bool serializeFunctionBody );

	symbolTypeClass const &getParamType ( uint32_t paramNum, accessorType const &acc, unique_queue<accessorType> *scanQueue );
	void					setParamType ( class compExecutable *comp, class symbolStack *sym, uint32_t paramNum, symbolTypeClass const &type, unique_queue<accessorType> *scanQueue );
	void					setParamTypeNoThrow ( class compExecutable *comp, class symbolStack *sym, uint32_t paramNum, symbolTypeClass const &type, unique_queue<accessorType> *scanQueue );
	void					setAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc );
	void					setWeakAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation  const &loc );
	void					setInlineUsed ( accessorType const &acc, unique_queue<accessorType> *scanQueue, srcLocation const &loc );
	void					setCalled ( accessorType const &acc, srcLocation const &loc );

	symbolTypeClass const   getReturnType ()
	{
		return retType.weakify ();
	}
	symbolTypeClass const   getMarkReturnType ( class compExecutable *comp, symbolStack const *sym, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS, srcLocation const &loc );
	void					setReturnType ( symbolTypeClass const &type, unique_queue<accessorType> *scanQueue, srcLocation const &loc );

	class	 compFunc *cFunc = nullptr;

	stringi getFunctionSignature ();
	stringi getSimpleFunctionSignature ();
	stringi const &getDocumentation ()
	{
		return documentation;
	}
	stringi getSimpleName ()
	{
		if ( name.find ( "." ) != stringi::npos )
		{
			char methodName[256];
			_token ( name.c_str (), ".", _numtoken ( name.c_str (), "." ) - 1, methodName );
			return methodName;
		} else
		{
			return name;
		}
	}
	std::vector<std::tuple<stringi, stringi, srcLocation const &>> getParameterHelp ();
	std::vector<std::tuple<stringi, stringi, srcLocation const &>> getSlimParameterHelp ();
};
