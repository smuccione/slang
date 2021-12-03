/*
	Local init optimization

	walks the ast for each function and ensures that locals are initialized in the correct order

	1.	initialized locals
	2.	objects and variants assigned nul
	3.	all others grouped so that we can use a single reserve instruction

*/


#include "compilerOptimizer.h"
#include "bcVM\opcodes.h"

static astNode *optimizeBlockLocalInitCBCB ( astNode *node, symbolStack *sym, void *param )
{
	switch ( node->op )
	{
		case btBasic:
			if ( node->iData.basicData.locals && node->iData.basicData.locals->size() )
			{
				node->iData.basicData.locals->optimizeInit();
			}
			break;
	}
	return node;
}

void compBCFunc::optimizeBlockLocalInit()
{
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		symbolStack	sym ( file );

		if ( (*it).second->codeBlock )
		{
			astNodeWalk ( (*it).second->codeBlock, &sym, optimizeBlockLocalInitCBCB, 0 );
		}
	}
}
