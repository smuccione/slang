/*

	block processor


*/

#include "compilerBCGenerator.h"

void compExecutable::pruneRefs ( opFunction *func )
{
	if ( func->isIterator ) return;

	symbolStack	sym ( file, func );

	errorLocality e ( &file->errHandler, func->location );

	for ( auto it2 = func->params.symbols.begin(); it2 != func->params.symbols.end(); it2++ )
	{
		if ( it2->initializer ) it2->initializer = it2->initializer->pruneRefs ( true );
	}

	if ( func->codeBlock ) func->codeBlock = func->codeBlock->pruneRefs ( true );
}

void compExecutable::pruneRefs ( void )
{
	for ( auto &it : file->symbols )
	{
		if ( it.second.loadTimeInitializable )
		{
			auto init = it.second.initializer;
			if ( init )
			{
				if ( init->getOp ( ) == astOp::assign )
				{
					init->right->pruneRefs ( true );
				} else
				{
					init->pruneRefs ( true );
				}
			}
		}
	}

	for ( auto it = file->classList.begin(); it != file->classList.end(); it++ )
	{
		for ( auto it2 = (*it).second->elems.begin(); it2 != (*it).second->elems.end(); it2++ )
		{
			switch ( (*it2)->type )
			{
				case fgxClassElementType::fgxClassType_iVar:
					if ( (*it2)->data.iVar.initializer->getOp () == astOp::assign ) 
					{
						(*it2)->data.iVar.initializer = (*it2)->data.iVar.initializer->pruneRefs ( true );
					}
					break;
				default:
					break;
			}
		}
	}

	for (auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		pruneRefs ( (*it).second );
	}
}
