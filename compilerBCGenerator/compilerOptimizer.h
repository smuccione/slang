/*
	compiler optimizer

*/

#pragma once

#include "compilerParser/fileParser.h"
#include "bcVM/Opcodes.h"

class optimizer {
	class	opFile			*file;
	class	opTransform		*transform;
	class	compExecutable	*compDef;

private:
	bool					 doInterprocedurealTypeInference = true;
	bool					 doInline = true;
	bool					 doInlineMakeDot = false;
	size_t					 inlineThreshold = 100;

private:
	// optimizer helper functions
	void		 simpleTransformBlock			( astNode *block, symbolStack *sym );
	void		 simpleTransformFunction		( opFunction *func );
	void		 constantFoldFunction			( opFunction *func );
	void		 makeLabelList					( astNode *block, std::unordered_map<cacheString, std::pair<astNode *, bool> > &labels );
	bool		 doesBlockHaveNeededLabel		( astNode *block, std::unordered_map<cacheString, std::pair<astNode *, bool> > &labels );
	astNode		*simpleDCEliminateBlock			( astNode *block, std::unordered_map<cacheString, std::pair<astNode *, bool> > &labels, symbolStack *sym );
	void		 dcEliminateFunction			( opFunction *func );
	astNode		*removeUnusedVariables			( astNode *block, symbolStack *sym );
	void		 removeUnusedVariables			( opFunction *func );
	void		 funcInlineWeight				( opFunction *func );

	// entry point for the optimizations
	void		 makeKnown						( unique_queue<accessorType> *scanQueue, bool inUseOnly );
	void		 doTypeInference				( cacheString const &entry, bool doMakeKnown, bool removedUnused, bool isLS );
	void		 simpleTransform				();
	void		 constantFold					();
	void		 constantFoldGlobals			();
	void		 simpleDCEliminate				();
	void		 removeUnusedVariables			();

	void		 findInlineCandidates			();
	void		 funcInline						( opFunction *func );
	void		 funcInline						();

public:
	bool		transformMade;

	optimizer ( opFile *fileDef, compExecutable *compDef );
	~optimizer ();

	// initiate the optimizations;
	void go ( cacheString const &entry );
	void goLS ( cacheString const &entry );
	void goLSError ( cacheString const &entry );
};
