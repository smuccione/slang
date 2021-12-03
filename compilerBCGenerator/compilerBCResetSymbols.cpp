/*

	block processor


*/

#include "compilerBCGenerator.h"

void compExecutable::resetSymbols ( opFunction *func )
{
	symbolStack	sym ( file );

	func->retType.resetType ( );
	func->params.resetType ( file );

	if ( func->codeBlock )
	{
		func->codeBlock->resetType( file );
	}
}

void compExecutable::resetSymbols ( void )
{
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		resetSymbols ( (*it).second );
	}
}

