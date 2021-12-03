#include "compilerBCGenerator.h"

void compExecutable::makeCodeblock ( opFunction *func )
{
	symbolStack	sym ( file );
	symbolSpaceNamespace	ns ( file, &file->ns, &func->usingList );
	symbolClass				symClass;
	symbolSpacePCall		symPC;
	symbolClass				symParClass;

	if ( func->isInterface ) return;

	sym.push ( &ns );
	if ( !func->isFGL )
	{
		if ( func->classDef )
		{
			sym.push ( &symClass );
		}
	}
	sym.push ( &func->params );
	sym.push ( &symPC );

	errorLocality e ( &file->errHandler, func->location );
	if ( func->codeBlock )
	{
		func->codeBlock->makeCodeblock ( &sym, &file->errHandler );
	}
}

void compExecutable::makeCodeblock ( void )
{
	symbolStack sym ( file );
	opUsingList opUse;
	symbolSpaceNamespace ns ( file, &file->ns, &opUse );
	sym.push ( &ns );

	for ( auto &it : file->symbols )
	{
		if ( it.second.loadTimeInitializable )
		{
			auto init = it.second.initializer;
			if ( init )
			{
				if ( init->getOp ( ) == astOp::assign )
				{
					init->right->makeCodeblock ( &sym, &file->errHandler );
				} else
				{
					init->makeCodeblock ( &sym, &file->errHandler );
				}
			}
		}
	}

	for ( auto it = file->classList.begin ( ); it != file->classList.end ( ); it++ )
	{
		for ( auto it2 = (*it).second->elems.begin ( ); it2 != (*it).second->elems.end ( ); it2++ )
		{
			switch ( (*it2)->type )
			{
				case fgxClassElementType::fgxClassType_iVar:
				case fgxClassElementType::fgxClassType_static:
					if ( (*it2)->data.iVar.initializer->getOp () == astOp::assign ) {
						(*it2)->data.iVar.initializer->makeCodeblock ( &sym, &file->errHandler );
					}
					break;
				default:
					break;
			}
		}
	}
	for ( auto funcIt = file->functionList.begin ( ); funcIt != file->functionList.end ( ); funcIt++ )
	{
		makeCodeblock ( (*funcIt).second );
	}
}
