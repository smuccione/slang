/*

	block processor


*/

#include "compilerBCGenerator.h"

void compExecutable::makeAtoms ( opFunction *func, bool isLS )
{
	if ( func->isIterator ) return;

	/* NOTE: we're not concerned about the ORDER of symbols here... we're not generating code, so no stack index will be taken
			what we *are* interested in, however is the presence of the symbols, so that local definitions will override global definitions
			and we wont' turn variables into atoms incorrectly
	*/
	symbolStack	sym ( file, func );

	errorLocality e1 ( &file->errHandler, func->location );

	if ( func->params.symbols.size() )
	{
		// need to do this... the default parameters are not built under the context of the function
		opFunction paramFunc ( file );
		paramFunc.name = file->newValue;	// some dummy for debug comparison
		paramFunc.usingList = func->usingList;
		symbolStack paramSym ( file, &paramFunc );
		for (auto it = func->params.symbols.begin ( ); it != func->params.symbols.end ( ); it++)
		{
			if ( it->initializer && it->initializer->getOp () == astOp::assign )
			{
				it->initializer->right->makeAtoms ( &paramSym, &file->errHandler, isLS, nullptr );
			}
		}
	}

	if ( func->codeBlock ) 
	{
		func->codeBlock->makeAtoms ( &sym, &file->errHandler, isLS, func );
	}
}


void compExecutable::makeAtoms ( bool isLS, int64_t sourceIndex )
{
	symbolStack sym ( file );
	opUsingList opUse;
	symbolSpaceNamespace ns ( file, &file->ns, &opUse );
	sym.push ( &ns );

	for ( auto &it : file->symbols )
	{
		if ( !sourceIndex || (it.second.location.sourceIndex == sourceIndex) )
		{
			if ( it.second.loadTimeInitializable )
			{
				auto init = it.second.initializer;
				if ( init )
				{
					try
					{
						errorLocality e ( &file->errHandler, init->location );
						if ( init->getOp () == astOp::assign )
						{
							init->right->makeAtoms ( &sym, &file->errHandler, isLS, nullptr );
						} else
						{
							init->makeAtoms ( &sym, &file->errHandler, isLS, nullptr );
						}
					} catch ( errorNum err )
					{
						file->errHandler.throwFatalError ( isLS, err );
					}
				}
			}
		}
	}

	for ( auto it = file->classList.begin (); it != file->classList.end (); it++ )
	{
		if ( !sourceIndex || (it->second->location.sourceIndex == sourceIndex) )
		{
			for ( auto it2 = (*it).second->elems.begin (); it2 != (*it).second->elems.end (); it2++ )
			{
				switch ( (*it2)->type )
				{
					case fgxClassElementType::fgxClassType_static:
						try
						{
							errorLocality e ( &file->errHandler, (*it2)->data.iVar.initializer->location );
							if ( (*it2)->data.iVar.initializer->getOp () == astOp::assign ) {
								(*it2)->data.iVar.initializer->makeAtoms ( &sym, &file->errHandler, isLS, nullptr );
							}
						} catch ( errorNum err )
						{
							file->errHandler.throwFatalError ( isLS, err );
						}
						break;
					default:
						break;
				}
			}
		}
	}
	for ( auto funcIt = file->functionList.begin (); funcIt != file->functionList.end (); funcIt++ )
	{
		if ( !sourceIndex || (funcIt->second->location.sourceIndex == sourceIndex) )
		{
			try
			{
				errorLocality e ( &file->errHandler, (*funcIt).second->location );
				makeAtoms ( (*funcIt).second, isLS );
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( isLS, err );
			}
		}
	}
}

