/*

	block processor


*/

#include "compilerBCGenerator.h"

void compExecutable::makeSymbols ( class opFunction *func, bool isLS )
{
    if ( func->parentClosure.size() ) return;		// don't make enclosed functions... this will be done via parent

	errorLocality e ( &file->errHandler, func->location );

	std::vector<symbolLocal>	closures;
	symbolStack sym ( file, func );
	symbolSpaceClosures closureAdd ( &closures );
	symbolSpaceLocalAdd localAdd;

	sym.push ( &closureAdd );
	sym.push ( &localAdd );

	if (func->params.symbols.size ( ))
	{
		// need to do this... the default parameters are not built under the context of the function
		opFunction paramFunc ( file );

		paramFunc.name = file->newValue;	// some dummy for debug comparison
		paramFunc.usingList = func->usingList;
		symbolStack paramSym ( file, &paramFunc );
		std::vector<symbolLocal>	paramClosureSave;
		symbolSpaceClosures paramClosures ( &paramClosureSave );
		symbolSpaceLocalAdd paramAdd;

		astNode addPt ( file->sCache, astOp::btBasic );

		// NOTE: we're not sending a parent symbol stack, so we will never have any closure add's for parameter lambda's
		for (auto it = func->params.symbols.begin ( ); it != func->params.symbols.end ( ); it++)
		{
			if (it->initializer && it->initializer->getOp() == astOp::assign)
			{
				it->initializer->right->makeSymbols ( &file->errHandler, &paramSym, &addPt, nullptr, nullptr, this, &paramAdd, &paramClosures, isLS, false );
			}
		}
		if ( paramAdd.size() )
		{
			if ( !isLS ) throw errorNum::scINITIALIZER_DECLARING_VARIABLE;
			for ( auto &it : paramAdd.locals )
			{
				file->errors.push_back ( std::make_unique<astNode> ( errorNum::scINITIALIZER_DECLARING_VARIABLE, it.location ) );
			}
		}
	}

	if ( func->codeBlock )
	{
		func->codeBlock->makeSymbols ( &file->errHandler, &sym, 0, 0, func, this, &localAdd, &closureAdd, isLS, false );
	}
}

void compExecutable::makeSymbols ( bool isLS, int64_t sourceIndex )
{
	symbolStack sym ( file );
	opUsingList opUse;
	symbolSpaceNamespace ns ( file, &file->ns, &opUse );

	symbolSpaceLocalAdd localAdd;

	sym.push ( &localAdd );
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
							init->right->makeSymbols ( &file->errHandler, &sym, 0, 0, 0, this, &localAdd, 0, isLS, false );
						} else
						{
							init->makeSymbols ( &file->errHandler, &sym, 0, 0, 0, this, &localAdd, 0, isLS, false );
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
			symbolClass cls ( file, it->second->name, 0 );
			auto size = sym.size ();
			sym.push ( &cls );
			for ( auto it2 = (*it).second->elems.begin (); it2 != (*it).second->elems.end (); it2++ )
			{
				switch ( (*it2)->type )
				{
					case fgxClassElementType::fgxClassType_iVar:
						if ( (*it2)->data.iVar.initializer->getOp () == astOp::assign )
						{
							try
							{
								errorLocality e ( &file->errHandler, (*it).second->location );
								astNode addPt ( file->sCache, astOp::btBasic );
								(*it2)->data.iVar.initializer->makeSymbols ( &file->errHandler, &sym, &addPt, 0, 0, this, &localAdd, 0, isLS, false );
							} catch ( errorNum err )
							{
								file->errHandler.throwFatalError ( isLS, err );
							}
						}
						break;
					default:
						break;
				}
			}
			sym.resize ( size );
		}
	}
	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		if ( !sourceIndex || (it->second->location.sourceIndex == sourceIndex) )
		{
			try
			{
				errorLocality e ( &file->errHandler, (*it).second->location );
				makeSymbols ( (*it).second, isLS );
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( isLS, err );
			}
		}
	}
}

