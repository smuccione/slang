/*

	block processor


*/

#include "compilerBCGenerator.h"

bool compExecutable::hasFunctionReference ( opFunction *func )
{
	symbolStack	sym ( file, func );

	for ( auto it = func->params.symbols.begin(); it != func->params.symbols.end(); it++ )
	{
		if ( it->initializer && it->initializer->hasFunctionReference ( &sym ) ) return true;
	}

	if ( func->codeBlock && func->codeBlock->hasFunctionReference ( &sym ) ) return true;
	return false;
}


bool compExecutable::hasFunctionReference ( void )
{
	for ( auto funcIt = file->functionList.begin(); funcIt != file->functionList.end(); funcIt++ )
	{
		if ( hasFunctionReference ( (*funcIt).second ) ) return true;
	}
	return false;
}

