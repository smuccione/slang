#include "compilerBCGenerator.h"

void compExecutable::makeConstructorDestructors ( opClass *cls, bool &needsNew, bool &needsNew_base, bool &needsRelease, bool &needsRelease_base, bool isLS )
{
	bool needsBase = false;
	for ( auto &it : cls->elems )
	{
		errorLocality e ( &file->errHandler, it->location );
		switch ( it->type )
		{
			case fgxClassElementType::fgxClassType_method:
				if ( it->name == file->newValue )
				{
					needsNew = true;
				}
				if ( it->name == file->cNewValue )
				{
					needsNew = true;
				}
				if ( it->name == file->releaseValue )
				{
					needsRelease = true;
				}
				if ( it->name == file->cReleaseValue )
				{
					needsRelease = true;
				}
				break;
			case fgxClassElementType::fgxClassType_iVar:
			case fgxClassElementType::fgxClassType_static:
			case fgxClassElementType::fgxClassType_const:
				if ( it->data.iVar.initializer->getOp () == astOp::assign )
				{
					needsNew = true;
				}
				break;
			case fgxClassElementType::fgxClassType_inherit:
				if ( file->classList.find ( it->name ) != file->classList.end() )
				{
					auto baseClass = file->classList.find ( it->name )->second;
					makeConstructorDestructors ( baseClass, needsNew, needsNew_base, needsRelease, needsRelease_base, isLS );
					if ( it->isVirtual )
					{
						needsBase = true;
					}
				} else
				{
					if ( !isLS ) throw errorNum::scCLASS_NOT_FOUND;
					file->errors.push_back ( std::make_unique<astNode> ( errorNum::scCLASS_NOT_FOUND, it->location ) );
				}
				break;
			default:
				break;
		}
	}
	if ( needsBase )
	{
		needsNew = true;
		needsNew_base = true;
		if ( needsRelease )
		{
			needsRelease_base = true;
		}
	}
}

void compExecutable::makeConstructorDestructors ( opClass *cls, bool isLS )
{
	bool hasNew = false;
	bool hasNew_base = false;
	bool hasRelease = false;
	bool hasRelease_base = false;
	bool needsNew = false;
	bool needsNew_base = false;
	bool needsRelease = false;
	bool needsRelease_base = false;

	for ( auto &it : cls->elems )
	{
		switch ( it->type )
		{
			case fgxClassElementType::fgxClassType_method:
				if ( it->name == file->newValue )
				{
					hasNew = true;
				}
				if ( it->name == file->newBaseValue )
				{
					hasNew_base = true;
				}
				if ( it->name == file->releaseValue )
				{
					hasRelease = true;
				}
				if ( it->name == file->releaseBaseValue )
				{
					hasRelease_base = true;
				}
				break;
			default:
				break;
		}
	}
	makeConstructorDestructors ( cls, needsNew, needsNew_base, needsRelease, needsRelease_base, isLS );

	if ( needsNew && !hasNew )
	{
		cacheString methName = file->sCache.get ( buildString ( cls->name.c_str (), "new", "method" ) );
		source src ( &file->srcFiles, file->sCache, isLS ? stringi ( "(INTERNAL)" ) : file->srcFiles.getName ( cls->location.sourceIndex ), "(){}", sourceFile::sourceFileType::none, isLS ? 0 : cls->location.lineNumberStart );
		auto method = file->parseMethod ( src, cls, methName.c_str (), true, false, false, srcLocation () );
		method->location = cls->location;
		cls->addMethod ( file->newValue, fgxClassElementType::fgxClassType_method, fgxClassElementScope::fgxClassScope_public, false, false, false, method, file, symVariantType, true, stringi() );
		method->isInterface = cls->isInterface;
	}
	if ( needsNew_base && !hasNew_base )
	{
		for ( auto &it : cls->elems )
		{
			if ( it->name == file->newValue )
			{
				auto newFunc = file->functionList.find ( it->data.method.func )->second;
				cacheString methName = file->sCache.get ( buildString ( cls->name.c_str (), "new_base", "method" ) );
				source src ( &file->srcFiles, file->sCache, isLS ? stringi ( "(INTERNAL)" ) : file->srcFiles.getName ( newFunc->location.sourceIndex ), "(){}", sourceFile::sourceFileType::none, isLS ? 0 : newFunc->location.lineNumberStart );
				auto method = file->parseMethod ( src, cls, methName.c_str (), true, false, false, srcLocation () );
				method->location = cls->location;
				delete method->codeBlock;
				method->codeBlock = new astNode ( *newFunc->codeBlock );
				method->isBaseMethod = true;
				cls->addMethod ( file->newBaseValue, fgxClassElementType::fgxClassType_method, fgxClassElementScope::fgxClassScope_public, false, false, false, method, file, symVariantType, true, stringi () );
				method->isInterface = cls->isInterface;
				break;
			}
		}
	}
	if ( needsRelease && !hasRelease )
	{
		cacheString methName = file->sCache.get ( buildString ( cls->name.c_str (), "release", "method" ) );
		source src ( &file->srcFiles, file->sCache, isLS ? stringi ( "(INTERNAL)" ) : file->srcFiles.getName ( cls->location.sourceIndex ), "(){}", sourceFile::sourceFileType::none, isLS ? 0 : cls->location.lineNumberStart );
		auto method = file->parseMethod ( src, cls, methName.c_str (), true, false, false, srcLocation () );
		method->location = cls->location;
		cls->addMethod ( file->releaseValue, fgxClassElementType::fgxClassType_method, fgxClassElementScope::fgxClassScope_public, true, false, false, method, file, symVariantType, true, stringi () );
		method->isInterface = cls->isInterface;
	}
	if ( needsRelease_base && !hasRelease_base )
	{
		for ( auto &it : cls->elems )
		{
			if ( it->name == file->releaseValue )
			{
				auto releaseFunc = file->functionList.find ( it->data.method.func )->second;
				cacheString methName = file->sCache.get ( buildString ( cls->name.c_str (), "release_base", "method" ) );
				source src ( &file->srcFiles, file->sCache, isLS ? stringi ( "(INTERNAL)" ) : file->srcFiles.getName ( releaseFunc->location.sourceIndex ), "(){}", sourceFile::sourceFileType::none, isLS ? 0 : releaseFunc->location.lineNumberStart );
				auto method = file->parseMethod ( src, cls, methName.c_str (), true, false, false, srcLocation () );
				method->location = cls->location;
				delete method->codeBlock;
				method->codeBlock = new astNode ( *releaseFunc->codeBlock );
				method->isBaseMethod = true;
				cls->addMethod ( file->releaseBaseValue, fgxClassElementType::fgxClassType_method, fgxClassElementScope::fgxClassScope_public, true, false, false, method, file, symVariantType, true, stringi () );
				method->isInterface = cls->isInterface;
				break;
			}
		}
	}
}

void compExecutable::makeConstructorDestructors ( bool isLS, int64_t sourceIndex )
{
	try
	{
		for ( auto &it : file->classList )
		{
			if ( !sourceIndex || it.second->location.sourceIndex == sourceIndex )
			{
				errorLocality e ( &file->errHandler, it.second->location );
				makeConstructorDestructors ( it.second, isLS );
			}
		}
	} catch ( errorNum err )
	{
		file->errHandler.throwFatalError ( isLS, err );
	}
}

void compExecutable::genWrapper ( opFunction *func )
{
	compClass *classDef;
	astNode *outer = 0;
	astNode *newNode;

	assert ( !func->isIterator );

	if ( func->conv != fgxFuncCallConvention::opFuncType_Bytecode ) return;
	if ( !func->codeBlock ) return;

	symbolStack sym ( file, func );

	try
	{
		outer = (new astNode ( file->sCache, astOp::btBasic ))->setLocation ( func->codeBlock );
	
		// let's see if we are a class constructor
		if ( func->classDef && func->classDef->cClass->newIndex )
		{
			classDef = func->classDef->cClass;
			if ( classDef->elements[classDef->newIndex - 1].methodAccess.func == func )
			{
				// we're this class's constructor
				for ( auto &[oClass, virtInfo] : classDef->virtualClasses )
				{
					bool initialized = false;

					// for each virtual class, find a path to one of its instances so that we can generate a function call to it.
					for ( auto &inIt : func->initializers )
					{
						if ( oClass->name == inIt->left->symbolValue() )
						{
							auto const newName = file->sCache.get ( (stringi) oClass->name + "::new" );
							auto const baseName = file->sCache.get ( (stringi) oClass->name + "::new_base" );
							srcLocation loc = inIt->left->location;
							if ( sym.isDefined ( baseName, true ) )
							{
								inIt->left->symbolValue() = baseName;
							} else
							{
								inIt->left->symbolValue() = newName;
							}
							inIt->left->symbolValue() = newName;
							inIt->morphNew ( file );
							if ( func->isFGL )
							{
								inIt->left = new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), inIt->left );
								//inIt->left->forceLocation ( loc );
							}
							outer->pushNode ( inIt );
							inIt = nullptr;
							initialized = true;
							break;
						}
					}
					if ( !initialized )
					{
						// no initializer... just call default

						auto const newName = file->sCache.get ( (stringi) oClass->name + "::new" );
						auto const baseName = file->sCache.get ( (stringi) oClass->name + "::new_base" );

						if ( sym.isDefined ( baseName, true ) )
						{
							if ( func->isFGL )
							{
								outer->pushNode ( new astNode	(	file->sCache, astOp::funcCall, 
																	new astNode	(	file->sCache, astOp::sendMsg, 
																					new astNode ( file->sCache, astOp::symbolValue, "self" ), 
																					new astNode ( file->sCache, astOp::nameValue, baseName )
																				)
																																				
																)
												);
							} else
							{
								outer->pushNode (	new astNode	 (	file->sCache, astOp::funcCall, 
																	new astNode ( file->sCache, astOp::atomValue, baseName )
																 )
												);
							}
							//outer->basicData ().blocks.back ()->forceLocation ( outer );
						} else if ( sym.isDefined ( newName, true ) )
						{
							if ( func->isFGL )
							{
								outer->pushNode (	new astNode (	file->sCache, astOp::funcCall, 
																	new astNode (	file->sCache, astOp::sendMsg, 
																					new astNode ( file->sCache, astOp::symbolValue, "self" ),
																					new astNode ( file->sCache, astOp::nameValue, newName  )
																				 )
																 )
												);
							} else
							{
								outer->pushNode (	new astNode	 (	file->sCache, astOp::funcCall, 
																	new astNode ( file->sCache, astOp::atomValue, newName )
																 )
												);
							}
							//outer->basicData ().blocks.back ()->forceLocation ( outer );
						}
					}
				}

				// scan through all the elements
				// do we have an initializer for this element?
				for ( auto &elemIt : classDef->elements )
				{
					bool initialized = false;

					if ( !elemIt.ns.size () )
					{
						for ( auto inIt : func->initializers )
						{
							if ( inIt && inIt->getOp() == astOp::funcCall && elemIt.name == inIt->left->symbolValue() )
							{
								switch ( elemIt.type )
								{
									case fgxClassElementType::fgxClassType_inherit:
										if ( !elemIt.isVirtual )
										{
											newNode = new astNode ( *inIt );
											newNode->morphNew ( file );
											outer->pushNode ( newNode );
										}
										break;
									case fgxClassElementType::fgxClassType_iVar:
										if ( elemIt.elem->data.iVar.initializer->getOp() == astOp::assign )
										{
											errorLocality e ( &file->errHandler, inIt->location );
											throw errorNum::scDOUBLE_INITIALIZE;
										}
										if ( inIt->pList ().param.size () != 1 )
										{
											errorLocality e ( &file->errHandler, inIt->location );
											throw errorNum::scILLEGAL_METHOD_IVAR_INITIALIZER;
										}
										newNode = new astNode ( *inIt );
										newNode->morphAssignment ( file );
										outer->pushNode ( newNode );
										break;
									default:
										{
											errorLocality e ( &file->errHandler, inIt->location );
											throw errorNum::scILLEGAL_INITIALIZER;
										}
										break;
								}
								initialized = true;
								break;
							} else if ( inIt &&inIt->getOp () != astOp::funcCall )
							{
								// error state, will have already been flagged but need to do this for languag server
								outer->pushNode ( new astNode ( *inIt ) );
								initialized = true;
							}
						}
						if ( !initialized )
						{
							// default initialization
							switch ( elemIt.type )
							{
								case fgxClassElementType::fgxClassType_inherit:
									if ( !elemIt.isVirtual )
									{
										auto const newName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::new" );
										auto const baseName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::new_base" );

										if ( sym.isDefined ( baseName, true ) )
										{
											if ( func->isFGL )
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::nameValue, baseName ) ) ) );
											} else
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, baseName ) ) );
											}
											//outer->basicData ().blocks.back ()->forceLocation ( outer );
										} else if ( sym.isDefined ( newName, true ) )
										{
											if ( func->isFGL )
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::nameValue, newName ) ) ) );
											} else
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, newName ) ) );
											}
											//outer->basicData ().blocks.back ()->forceLocation ( outer );
										}
									}
									break;
								default:
									break;
							}
						}
					}
				}

				// emit any intializers for instance variables
				for ( auto elemIt : func->classDef->elems )
				{
					switch ( elemIt->type )
					{
						case fgxClassElementType::fgxClassType_iVar:
							if ( elemIt->data.iVar.initializer->getOp () == astOp::assign )
							{
								assert ( elemIt->name == elemIt->data.iVar.initializer->left->symbolValue () );
								if ( func->isFGL )
								{
									newNode = (new astNode ( file->sCache, astOp::assign ))->setLocation ( elemIt->data.iVar.initializer );
									newNode->left = new astNode ( file->sCache, astOp::sendMsg,
																   new astNode ( file->sCache, astOp::symbolValue, "self" ),
																   (new astNode ( file->sCache, astOp::nameValue, elemIt->name.c_str () ))->setLocation ( elemIt->data.iVar.initializer->left )
													);
									newNode->right = new astNode ( *elemIt->data.iVar.initializer->right );
//									newNode->forceLocation ( srcLocation () );
								} else
								{
									newNode = new astNode ( *elemIt->data.iVar.initializer );
								}
								//newNode->forceLocation ( outer );
								outer->pushNode ( newNode );
							}
							break;
						default:
							break;
					}
				}

				if ( func->classDef && func->classDef->cClass->cNewIndex )
				{
					newNode = new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, "$new" ) );
					auto pList = &newNode->pList();
					symbolStack	sym ( file, func );

					for ( uint32_t paramIndex = 1; paramIndex < func->params.size (); paramIndex++ )
					{
						pList->param.push_back ( new astNode ( file->sCache, astOp::symbolValue, func->params[paramIndex]->name.c_str () ) );
						pList->paramRegion.push_back ( srcLocation() );
					}
					outer->pushNode ( newNode );
				}
			}
		}
		if ( func->classDef && func->classDef->cClass->newBaseIndex )
		{
			classDef = func->classDef->cClass;
			if ( classDef->elements[classDef->newBaseIndex - 1].methodAccess.func == func )
			{
				// we're this class's constructor

				// scan through all the elements
				// do we have an initializer for this element?
				for ( auto &elemIt : classDef->elements )
				{
					bool initialized = false;

					if ( !elemIt.ns.size () )
					{
						for ( auto &inIt : func->initializers )
						{
							if ( elemIt.name == inIt->left->symbolValue() )
							{
								switch ( elemIt.type )
								{
									case fgxClassElementType::fgxClassType_inherit:
										newNode = new astNode ( *inIt );
										newNode->morphNew ( file );
										if ( func->isFGL )
										{
											newNode->left = new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), newNode->left );
										}
										outer->pushNode ( newNode );
										//outer->basicData ().blocks.back ()->forceLocation ( inIt->left );
										break;
									case fgxClassElementType::fgxClassType_iVar:
										if ( elemIt.elem->data.iVar.initializer )
										{
											errorLocality e ( &file->errHandler, inIt->location );
											throw errorNum::scDOUBLE_INITIALIZE;
										}
										if ( inIt->pList ().param.size () != 1 )
										{
											errorLocality e ( &file->errHandler, inIt->location );
											throw errorNum::scILLEGAL_METHOD_IVAR_INITIALIZER;
										}
										newNode = new astNode ( *inIt );
										newNode->morphAssignment ( file );
										outer->pushNode ( newNode );
										//outer->basicData ().blocks.back ()->forceLocation ( inIt->left );
										break;
									default:
										{
											errorLocality e ( &file->errHandler, inIt->location );
											throw errorNum::scILLEGAL_INITIALIZER;
										}
										break;
								}
								initialized = true;
								break;
							}
						}
						if ( !initialized )
						{
							// default initialization
							switch ( elemIt.type )
							{
								case fgxClassElementType::fgxClassType_inherit:
									if ( !elemIt.isVirtual )
									{
										auto const newName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::new" );
										auto const baseName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::new_base" );

										if ( sym.isDefined ( baseName, true ) )
										{
											if ( func->isFGL )
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, baseName ) ) ) );
											} else
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, baseName ) ) );
											}
											//outer->basicData ().blocks.back ()->forceLocation ( outer );
										} else if ( sym.isDefined ( newName, true ) )
										{
											if ( func->isFGL )
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, newName ) ) ) );
											} else
											{
												outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, newName ) ) );
											}
											//outer->basicData ().blocks.back ()->forceLocation ( outer );
										}
									}
									break;
								default:
									break;
							}
						}
					}
				}
				// emit any intializers for instance variables
				for ( auto elemIt : func->classDef->elems )
				{
					switch ( elemIt->type )
					{
						case fgxClassElementType::fgxClassType_iVar:
							if ( elemIt->data.iVar.initializer->getOp () == astOp::assign )
							{
								assert ( elemIt->name == elemIt->data.iVar.initializer->left->symbolValue () );
								if ( func->isFGL )
								{
									newNode = (new astNode ( file->sCache, astOp::assign ))->setLocation ( elemIt->data.iVar.initializer );
									newNode->left = new astNode ( file->sCache, astOp::sendMsg,
																   new astNode ( file->sCache, astOp::symbolValue, "self" ),
																   (new astNode ( file->sCache, astOp::symbolValue, elemIt->name.c_str () ))->setLocation ( elemIt->data.iVar.initializer->left )
																);
									newNode->right = new astNode ( *elemIt->data.iVar.initializer->right );
									newNode->forceLocation ( srcLocation () );
								} else
								{
									newNode = new astNode ( *elemIt->data.iVar.initializer );
								}
								//newNode->forceLocation ( outer );
								outer->pushNode ( newNode );
							}
							break;
						default:
							break;
					}
				}

				if ( func->classDef && func->classDef->cClass->cNewIndex )
				{
					newNode = new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, "$new" ) );
					auto pList = &newNode->pList();
					symbolStack	sym ( file, func );

					for ( uint32_t paramIndex = 1; paramIndex < func->params.size (); paramIndex++ )
					{
						pList->param.push_back ( new astNode ( file->sCache, astOp::symbolValue, func->params[paramIndex]->name.c_str () ) );
						pList->paramRegion.push_back ( srcLocation () );
					}
					outer->pushNode ( newNode );
				}
			}
		}

		outer->basicData().blocks.push_back ( func->codeBlock );

		if ( func->classDef && func->classDef->cClass->releaseIndex )
		{
			classDef = func->classDef->cClass;

			if ( classDef->elements[classDef->releaseIndex - 1].methodAccess.func == func )
			{
				// call any FFI destructor if necessary... this is the same as running our body so it should come first.
				if ( func->classDef && func->classDef->cClass->cReleaseIndex )
				{
					classDef = func->classDef->cClass;

					if ( classDef->elements[classDef->cReleaseIndex - 1].methodAccess.func == func )
					{
						newNode = new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, "$release" ) );
						newNode->morphRelease ( file );
						if ( func->isFGL )
						{
							newNode->left = new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), newNode->left );
						}
						outer->pushNode ( newNode );
						//outer->basicData ().blocks.back ()->forceLocation ( outer );
					}
				}

				// now call any non-virtual base class destructors
				for ( auto it = classDef->elements.rbegin (); it != classDef->elements.rend (); it++ )
				{
					auto elemIt = *it;
					// default initialization
					switch ( elemIt.type )
					{
						case fgxClassElementType::fgxClassType_inherit:
							if ( !elemIt.isVirtual && !elemIt.ns.size() )
							{
								auto const newName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::release" );
								auto const baseName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::release_base" );

								if ( sym.isDefined ( baseName, true ) )
								{
									if ( func->isFGL )
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, baseName ) ) ) );
									} else
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, baseName ) ) );
									}
									//outer->basicData ().blocks.back ()->forceLocation ( outer );
								} else if ( sym.isDefined ( newName, true ) )
								{
									if ( func->isFGL )
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, newName ) ) ) );
									} else
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, newName ) ) );
									}
									//outer->basicData ().blocks.back ()->forceLocation ( outer );
								}
							}
							break;
						default:
							break;
					}
				}

				// virtual classes are released last
				for ( auto it = classDef->virtualClasses.rbegin (); it != classDef->virtualClasses.rend (); it++ )
				{
					auto virtIt = *it;
					auto const newName = file->sCache.get ( (stringi) it->first->name + "::release" );
					auto const baseName = file->sCache.get ( (stringi) it->first->name + "::release_base" );
					if ( sym.isDefined ( baseName, true ) )
					{
						if ( func->isFGL )
						{
							outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, baseName ) ) ) );
						} else
						{
							outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, baseName ) ) );
						}
						//outer->basicData ().blocks.back ()->forceLocation ( outer );
					} else if ( sym.isDefined ( newName, true ) )
					{
						if ( func->isFGL )
						{
							outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, newName ) ) ) );
						} else
						{
							outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, newName ) ) );
						}
						//outer->basicData ().blocks.back ()->forceLocation ( outer );
					}
				}

				// now finally destroy this object
				newNode = new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, "system$release" ) );
				newNode->pList().param.push_back ( new astNode ( file->sCache, astOp::symbolValue, "self" ) );
				newNode->pList().paramRegion.push_back ( srcLocation() );
				outer->pushNode ( newNode );
				//outer->basicData ().blocks.back ()->forceLocation ( outer );
			}
		} 
		if ( func->classDef && func->classDef->cClass->releaseBaseIndex )
		{
			classDef = func->classDef->cClass;

			if ( classDef->elements[classDef->releaseBaseIndex - 1].methodAccess.func == func )
			{
				// call any FFI destructor
				if ( func->classDef && func->classDef->cClass->cReleaseIndex )
				{
					classDef = func->classDef->cClass;

					if ( classDef->elements[classDef->cReleaseIndex - 1].methodAccess.func == func )
					{
						newNode = new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, "$release" ) );
						newNode->morphRelease ( file );
						if ( func->isFGL )
						{
							newNode->left = new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), newNode->left );
						}
						outer->pushNode ( newNode );
						//outer->basicData ().blocks.back ()->forceLocation ( outer );
					}
				}

				// now call any non-virtual base class destructors
				for ( auto it = classDef->elements.rbegin (); it != classDef->elements.rend (); it++ )
				{
					auto elemIt = *it;
					// default initialization
					switch ( elemIt.type )
					{
						case fgxClassElementType::fgxClassType_inherit:
							if ( !elemIt.isVirtual && !elemIt.ns.size () )
							{
								auto const newName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::release" );
								auto const baseName = file->sCache.get ( (stringi) elemIt.ns + (stringi)elemIt.name + "::release_base" );

								if ( sym.isDefined ( baseName, true ) )
								{
									if ( func->isFGL )
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, baseName ) ) ) );
									} else
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, baseName ) ) );
									}
									//outer->basicData ().blocks.back ()->forceLocation ( outer );
								} else if ( sym.isDefined ( newName, true ) )
								{
									if ( func->isFGL )
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::sendMsg, new astNode ( file->sCache, astOp::symbolValue, "self" ), new astNode ( file->sCache, astOp::atomValue, newName ) ) ) );
									} else
									{
										outer->pushNode ( new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, newName ) ) );
									}
									//outer->basicData ().blocks.back ()->forceLocation ( outer );
								}
							}
							break;
						default:
							break;
					}
				}

				// now finally destroy this object
				newNode = new astNode ( file->sCache, astOp::funcCall, new astNode ( file->sCache, astOp::atomValue, "system$release" ) );
				newNode->pList().param.push_back ( new astNode ( file->sCache, astOp::symbolValue, "self" ) );
				newNode->pList().paramRegion.push_back ( srcLocation () );
				outer->pushNode ( newNode );
				//outer->basicData ().blocks.back ()->forceLocation ( outer );
			}
		}

		if ( !func->isProcedure && !func->codeBlock->alwaysReturns () )
		{
			auto x = new astNode ( file->sCache, astOp::btReturn );
			outer->basicData().blocks.push_back ( x );
		}
		outer->basicData().blocks.push_back ( (new astNode ( file->sCache, astOp::btLabel, "$ret" ))->setEnd ( func->location ) );

		func->codeBlock = outer;
	} catch ( ... )
	{
		if ( outer ) delete outer;
		throw;
	}
}

void compExecutable::genWrapper ( int64_t sourceIndex )
{
	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		// move variable initialization from symbol table into the basic block of the code
		// also generate ret epilog for functions
		if ( !sourceIndex || it->second->location.sourceIndex == sourceIndex )
		{
			try
			{
				if ( !(*it).second->isIterator )
				{
					errorLocality e ( &file->errHandler, (*it).second->location );
					genWrapper ( (*it).second );
				}
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( false, err );
			}
		}
	}
}
