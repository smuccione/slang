/*

	block processor


*/

#include "compilerBCGenerator.h"

void compExecutable::combineBlocks ( opFunction *func )
{
	if ( func->codeBlock )
	{
		func->codeBlock->combineBlocks ();
	}
}

void compExecutable::combineBlocks ( void )
{
	for ( auto &it : file->functionList )
	{
		combineBlocks ( it.second );
	}
}
