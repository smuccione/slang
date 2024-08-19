#include "compilerBCGenerator.h"
#include "compilerAST/astNodeWalk.h"
#include "Utility/counter.h"

static astNode *dupLambdasCb ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool, opFunction *func )
{
	switch ( node->getOp () )
	{
		case astOp::funcValue:
			{
				opFunction *newChild = new opFunction ( *sym->file->functionList.find ( node->symbolValue() )->second );

				newChild->name = sym->file->sCache.get ( (stringi) func->name + "@" + (stringi)newChild->name );

				sym->file->functionList.insert ( { newChild->name, newChild } );

				node->symbolValue() = newChild->name;

				newChild->parentClosure = func->name;
				if (newChild->params.symbols.size ( ))
				{
					// NOTE: we're not sending a parent symbol stack, so we will never have any closure add's for parameter lambda's
					for (auto it = newChild->params.symbols.begin ( ); it != newChild->params.symbols.end ( ); it++)
					{
						if (it->initializer && it->initializer->getOp() == astOp::assign)
						{
							astNodeWalk ( it->initializer->right, sym, dupLambdasCb, func );
						}
					}
				}

			}
			break;
		default:
			break;
	}
	return node;
}

bool compExecutable::makeExtensionMethods ( opClass *mostDerived, opClass *cls, bool generic, accessorType const &acc, unique_queue<accessorType> *scanQueue  )
{
	auto ret = false;
	auto size = cls->elems.size ();
	for ( size_t loop = 0; loop < size; loop++ )
	{
		auto &it = cls->elems[loop];
		switch ( it->type )
		{
			case fgxClassElementType::fgxClassType_method:
				if ( cls != mostDerived )
				{
					auto func = file->functionList.find ( it->data.method.func )->second;

					if ( func->isExtension )
					{
						bool found = false;
						for ( auto &baseElem : mostDerived->elems )
						{
							if ( baseElem->name == it->name )
							{
								// overridden already
								found = true;
								break;
							}
						}
						if ( found ) break;

						if ( generic )
						{
							mostDerived->addMethod ( it->name, it->type, it->scope, false, false, true, func, file, it->symType, false, it->documentation );
							mostDerived->elems.back()->data.method.virtOverrides.insert ( file->functionList.find ( it->data.method.func )->second );
						} else
						{
							auto newFunc = new opFunction ( *func );
							newFunc->name = file->sCache.get ( (stringi) mostDerived->name + "." + (stringi)newFunc->name );

							mostDerived->addMethod ( it->name, it->type, it->scope, false, false, true, newFunc, file, it->symType, true, it->documentation );
							mostDerived->elems.back()->data.method.virtOverrides.insert ( file->functionList.find ( newFunc->name )->second );

							newFunc->classDef = mostDerived;
							symbolStack sym ( file, newFunc );
							astNodeWalk ( newFunc->codeBlock, &sym, dupLambdasCb, newFunc );

							newFunc->params[newFunc->params.getSymbolName ( symbolSpaceScope::symLocalParam, (uint32_t) newFunc->params.size () - 1 )]->setType ( symbolTypeClass ( symbolType::symObject, mostDerived->name ), accessorType(), nullptr );
							if ( newFunc->params.symbols.size () )
							{
								// NOTE: we're not sending a parent symbol stack, so we will never have any closure add's for parameter lambda's
								for ( auto it = newFunc->params.symbols.begin (); it != newFunc->params.symbols.end (); it++ )
								{
									if ( it->initializer && it->initializer->getOp () == astOp::assign )
									{
										astNodeWalk ( it->initializer->right, &sym, dupLambdasCb, newFunc );
									}
								}
							}
							ret = true;
						}
					}
				}
				break;
			case fgxClassElementType::fgxClassType_inherit:
				{
					// NOTE: missing base class will case an error later during compClass construction, no need to handle it here
					if ( file->classList.find ( it->name ) != file->classList.end () )
					{
						auto base = file->classList.find ( it->name )->second;
						ret |= makeExtensionMethods ( mostDerived, base, generic, acc, scanQueue );
					}
				}
				break;
			default:
				break;
		}
	}
	return ret;
}

bool compExecutable::makeExtensionMethod ( opClass *mostDerived, bool generic, accessorType const &acc, unique_queue<accessorType> *scanQueue  )
{
	if ( makeExtensionMethods ( mostDerived, mostDerived, generic, acc, scanQueue  ) )
	{
		for ( auto it = classDef.begin (); it != classDef.end (); it++ )
		{
			if ( !strccmp ( (*it)->name, mostDerived->name.c_str() ) )
			{
				classDef.erase ( it );
				break;
			}
		}
		makeClass ( mostDerived, false );
		return true;
	}
	return false;
}

void compExecutable::makeExtensionMethods ( bool generic, int64_t sourceIndex )
{
	for ( auto &it : file->classList )
	{
		if ( !sourceIndex || (it.second->location.sourceIndex == sourceIndex) )
		{
			if ( !it.second->isInterface )
			{
				makeExtensionMethods ( it.second, it.second, generic, accessorType (), nullptr );
			}
		}
	}
}
