// This is the main DLL file.

#include "compilerBCGenerator.h"
#include "transform.h"
#include "avl tree/avl.h"

#define MAKE_DOT 0

#if !defined (MAKE_DOT) || defined ( _NDEBUG )
#undef MAKE_DOT
#define MAKE_DOT 0
#endif

compExecutable::~compExecutable ()
{
	delete transform;

	for ( auto &it : funcDef )
	{
		delete it;
	}

	for ( auto &it : classDef )
	{
		delete it;
	}

	for ( auto &it : globals )
	{
		delete it;
	}
}

void compExecutable::clear ()
{
	file->staticElemMap.clear ();
	for ( auto &it : funcDef )
	{
		delete it;
	}
	funcDef.clear ();

	for ( auto &it : classDef )
	{
		delete it;
	}
	classDef.clear ();

	for ( auto it = file->classList.begin (); it != file->classList.end (); it++ )
	{
		for ( auto &elem : it->second->elems )
		{
			elem->data.method.virtOverrides.clear ();
			elem->data.prop.accessVirtOverrides.clear ();
			elem->data.prop.assignVirtOverrides.clear ();

			if ( elem->data.method.func.size() ) elem->data.method.virtOverrides.insert ( file->functionList.find ( elem->data.method.func )->second );
			if ( elem->data.prop.assign.size() ) elem->data.prop.assignVirtOverrides.insert ( file->functionList.find ( elem->data.prop.assign )->second );
			if ( elem->data.prop.access.size() ) elem->data.prop.accessVirtOverrides.insert ( file->functionList.find ( elem->data.prop.access )->second );
		}
		(*it).second->cClass = 0;
	}

	for ( auto it = file->functionList.begin (); it != file->functionList.end (); it++ )
	{
		(*it).second->cFunc = 0;
	}
	// do NOT clear globals!!!  
}

compExecutable::compExecutable ( opFile *file ) : codeSegment ( *this ), listing ( &codeSegment, &dataSegment, &atom, this ), opt ( file, this )
{
	genDebug = false;
	genProfiling = false;

	this->file = file;
	labelId = 0;
	bssSize = 0;
	transform = new opTransform ( file );
}

compExecutable::compExecutable () : codeSegment ( *this ), listing ( &codeSegment, &dataSegment, &atom, this ), opt ( nullptr, this )
{
	genDebug = false;
	genProfiling = false;

	file = 0;
	labelId = 0;
	bssSize = 0;
	transform = new opTransform ( file );
}

int globalAvlComp ( const void *avl_a, const void *avl_b, void *avl_param )
{
	return strccmp ( ((compGlobalSymbol *) avl_a)->name.c_str (), ((compGlobalSymbol *) avl_b)->name.c_str () );
}

void compExecutable::processGlobals ( int64_t sourceIndex, bool isLS )
{
	// add all our class static/const's to the symbols list
	for ( auto it = file->classList.begin (); it != file->classList.end (); it++ )
	{
		if ( !sourceIndex || (it->second->location.sourceIndex == sourceIndex) )
		{
			for ( auto it2 = (*it).second->elems.begin (); it2 != (*it).second->elems.end (); it2++ )
			{
				switch ( (*it2)->type )
				{
					case fgxClassElementType::fgxClassType_static:
						(*it2)->data.iVar.symbolName = file->sCache.get ( (stringi)(*it).second->name + "::" + (stringi)(*it2)->name );
						if ( !it->second->isInterface )
						{
							auto it3 = file->symbols.find ( (*it2)->data.iVar.symbolName );
							if ( it3 == file->symbols.end () )
							{
								//						(*it2)->data.iVar.initializer = 0;	// hack to keep from having to make a copy... we move ownership to the globals vector
								file->ns.add ( opSymbol::symbolClass::symbolClass_classStatic, (*it2)->data.iVar.symbolName, new astNode ( *(*it2)->data.iVar.initializer ), (*it2)->location, false, (*it).second->isInterface, isLS, stringi () );
								// these symbols must always be present if we're the definer
								file->symbols.find ( (*it2)->data.iVar.symbolName )->second.accessors.insert ( accessorType () );
							}
						}
						break;
					case fgxClassElementType::fgxClassType_const:
						(*it2)->data.iVar.symbolName = file->sCache.get ( (stringi)(*it).second->name + "::" + (stringi)(*it2)->name );
						if ( !it->second->isInterface )
						{
							auto it3 = file->symbols.find ( (*it2)->data.iVar.symbolName );
							if ( it3 == file->symbols.end () )
							{
								//							(*it2)->data.iVar.initializer = 0;	// hack to keep from having to make a copy... we move ownership to the globals vector
								file->ns.add ( opSymbol::symbolClass::symbolClass_classConst, (*it2)->data.iVar.symbolName, new astNode ( *(*it2)->data.iVar.initializer ), (*it2)->location, true, (*it).second->isInterface, isLS, stringi () );
								// these symbols must always be present if we're the definer
								file->symbols.find ( (*it2)->data.iVar.symbolName )->second.accessors.insert ( accessorType () );
							}
						}
						break;
					default:
						break;
				}
			}
		}
	}
}

void compExecutable::compileGlobalInitializers ( bool isSLL )
{
	struct avl_traverser	trav;
	class compGlobalSymbol *elem;

	auto globalRoot = avl_create ( (avl_comparison_func *) globalAvlComp, 0, 0 );
	uint32_t prevExportableRoot = 0;

	// add all our global symbols (function static, file statics and true globals) to the globals array
	for ( auto &it : file->symbols )
	{
		it.second.globalIndex = -1;
		if ( !it.second.accessors.empty() || it.second.symClass == opSymbol::symbolClass::symbolClass_functionStatic || (it.second.loadTimeInitializable && !it.second.isInterface) )
		{
			if ( it.second.symClass == opSymbol::symbolClass::symbolClass_functionStatic )
			{
				globals.push_back ( new compGlobalSymbol ( it.second.location, it.second.name, it.second.enclosingName, it.first, nullptr, it.second.isExportable ) );
			} else if ( it.second.loadTimeInitializable && !it.second.isInterface )
			{
				globals.push_back ( new compGlobalSymbol ( it.second.location, it.second.name, it.first, &it.second.initializer, it.second.isExportable ) );
			} else
			{
				globals.push_back ( new compGlobalSymbol ( it.second.location, it.second.name, it.first, nullptr, it.second.isExportable ) );
			}
			globals[globals.size () - 1]->globalIndex = (uint32_t) globals.size () - 1;
			globals[globals.size () - 1]->fileSymbolName = it.first;
			it.second.globalIndex = (uint32_t)globals[globals.size () - 1]->globalIndex;
			if ( globals[globals.size () - 1]->isExportable )
			{
				avl_insert ( globalRoot, globals[globals.size () - 1] );
				globalLinkable.push_back ( globals.size () - 1 );

				if ( !globalExportableRoot )
				{
					globalExportableRoot = (uint32_t)globals.size ();	// NOT -1... we're +1 more to allow us to use 0 to indicate done
				} else
				{
					globals[prevExportableRoot]->nextExportable = (uint32_t) globals.size ();
				}
				prevExportableRoot = (uint32_t) globals.size () - 1;
			}
		}
	}

	elem = (class compGlobalSymbol *) avl_t_first ( &trav, globalRoot );
	if ( elem )
	{
		while ( elem )
		{
			elem->left = -1;
			elem->right = -1;
			if ( trav.avl_node->avl_link[0] )
			{
				elem->left = ((class compGlobalSymbol *) trav.avl_node->avl_link[0]->avl_data)->globalIndex;
			}
			if ( trav.avl_node->avl_link[1] )
			{
				elem->right = ((class compGlobalSymbol *) trav.avl_node->avl_link[1]->avl_data)->globalIndex;
			}
			elem = (class compGlobalSymbol *) avl_t_next ( &trav );
		}
		this->globalRoot = ((class compGlobalSymbol *) globalRoot->avl_root->avl_data)->globalIndex;
	} else
	{
		this->globalRoot = -1;
	}

	avl_destroy ( globalRoot, 0 );

	// add all our class static/const's to the globals array
	for ( auto &it : file->classList )
	{
		for ( auto &it2 : it.second->elems )
		{
			switch ( it2->type )
			{
				case fgxClassElementType::fgxClassType_static:
				case fgxClassElementType::fgxClassType_const:
					it2->data.iVar.index = file->symbols.find ( it2->data.iVar.symbolName )->second.globalIndex;
					break;
				default:
					break;
			}
		}
		for ( auto &it2 : it.second->cClass->elements )
		{
			switch ( it2.type )
			{
				case fgxClassElementType::fgxClassType_static:
				case fgxClassElementType::fgxClassType_const:
					it2.methodAccess.data.globalIndex = file->symbols.find ( it2.symbolName )->second.globalIndex;
					break;
				default:
					break;
			}
		}
	}

	// ok, we've just built up all our global symbols (including unnamed statics)... now emit the initialization code 
	fixup = new compFixup;

	globalInitOffset = 0;

	if ( globals.size () )
	{
		globalInitOffset = (uint32_t) (codeSegment.size () / sizeof ( fglOp ));

		for ( auto it = globals.begin (); it != globals.end (); it++ )
		{
			if ( (*it)->init )
			{
				opFunction paramFunc ( file );
				paramFunc.name = file->newValue;	// some dummy for debug comparison

				symbolStack	 sym ( file, (*it)->enclosingName ? file->functionList.find ( (*it)->enclosingName )->second : &paramFunc );

				opUsingList opUse;
				symbolSpaceNamespace ns ( file, &file->ns, &opUse );
				sym.push ( &ns );

				if ( (*it)->location.sourceIndex )
				{
					listing.annotate ( &sym, "initializer for %s::%s", file->srcFiles.getName ( (*it)->location.sourceIndex ).c_str (), (*it)->name.c_str () );
				} else
				{
					listing.annotate ( &sym, "initializer for %s", (*it)->name.c_str () );
				}

				cacheString tmpLabel1;
				if ( isSLL && (*it)->isExportable )
				{
					tmpLabel1 = makeLabel ( "global_once%" );
					fixup->needFixup ( tmpLabel1, codeSegment.putOp ( fglOpcodes::jmpGlobalNoInit, (uint32_t) 0, (uint32_t) (*it)->globalIndex ) );
				}
				if ( (*(*it)->init)->getOp () == astOp::assign )
				{
					debug.addEntry ( (*(*it)->init)->right->location, codeSegment.getNumOps (), sym.getStackSize () );

					errorLocality e ( &file->errHandler, (*(*it)->init) );

					if ( !(*(*it)->init)->hasNoSideEffects ( &sym, true ) )
					{
//						file->errHandler.throwWarning ( isLS, warnNum::scwCONSTANT_ORDERING, (*(*it)->init)->left->data.s.c_str() );
					}
					compEmitNode ( 0, (*(*it)->init)->right, &sym, symVariantType, symVariantType, false, true );
					codeSegment.putOp ( fglOpcodes::storeGlobalPop, (uint32_t)(*it)->globalIndex );
					sym.popStack ();
				}
				if ( isSLL && (*it)->isExportable )
				{
					codeSegment.emitLabel ( tmpLabel1 );
					fixup->setRefPoint ( tmpLabel1, codeSegment.nextOp () );
					listing.label ( &sym, tmpLabel1 );
				}
			}
		}

		codeSegment.putOp ( fglOpcodes::pRet );
	}

	fixup->applyFixups ( codeSegment );
	delete fixup;
	fixup = 0;
}

void compExecutable::genCode( char const* entry )
{
	LARGE_INTEGER							 start;
	LARGE_INTEGER							 end;
	LARGE_INTEGER							 frequency;
	for ( auto &it : file->libraries )
	{
		libraries.push_back ( it.fName );
	}
	for ( auto &it : file->modules )
	{
		modules.push_back ( it.fName );
	}

	QueryPerformanceFrequency( &frequency );
	frequency.QuadPart = frequency.QuadPart / 1000;
	QueryPerformanceCounter( &start );

	if ( MAKE_DOT ) makeDot( "dot/%s.after_parse.dot" );

	// process the global and static variables... this walks the various structures and assigns bss data space to global, 
	// global static, class static, and function/method static variables.
	// it will also allocate code to do any needed intialization
	processGlobals( (int64_t) 0, false );

	// minimize complexity
	combineBlocks();

	makeConstructorDestructors ( false, 0 );
	if ( MAKE_DOT ) makeDot( "dot/%s.after_makeConstructorDestructors.dot" );

	if ( file->errHandler.isFatal() ) return;

	// we REALLY want to have these non-generic here...  this way we can specialize the extension methods for all containers individually for each container
	makeExtensionMethods( entry ? false : true, 0 );
	if ( MAKE_DOT ) makeDot( "dot/%s.after_makeExtensionMethods.dot" );

	// TODO: move foreach to a spot after type inferencing but before inlining
	//		 this should let us optimize the code shape for some known types.
	//		 things like arrays, etc. do NOT need to use more expensive iterators if we already know the type.
	//		 we can certainly generate more optimal code shape
	compMakeForEach( (int64_t) 0, false );
	if ( MAKE_DOT ) makeDot( "dot/%s.after_makeforEach.dot" );
	if ( file->errHandler.isFatal () ) return;

	// class's must be compiled first... this actually injects the methods as functions into the function list so that they
	// can be emitted
	makeClass( false );
	if ( file->errHandler.isFatal () ) return;

	file->buildSymbolSearch ( true );

	// compile codeblocks.  must be done BEFORE make symbol so that make symbols will proces the symbols from the codeblock
	makeCodeblock();
	if ( MAKE_DOT ) makeDot( "dot/%s.after_makeCodeblock.dot" );
	if ( file->errHandler.isFatal () ) return;

	// while not technically an optimzation it's necessary to do this to prepare for function inlining, so we're including it
	// in the optmization timing.
	makeParentClosure( 0 );
	if ( MAKE_DOT ) makeDot( "dot/%s.after_makeParentClosure.dot" );
	if ( file->errHandler.isFatal () ) return;

	makeSymbols ( false, 0 );
	if ( MAKE_DOT ) makeDot( "dot/%s.after_makeSymbols.dot" );
	if ( file->errHandler.isFatal () ) return;

	makeAtoms ( false, 0 );
	if ( MAKE_DOT ) makeDot( "dot/%s.after_makeAtoms.dot" );
	if ( file->errHandler.isFatal () ) return;

	// do AFTER makeSymbols so we don't get any automatic at the top layer!!!
	genWrapper( (int64_t) 0 );
	if ( MAKE_DOT ) makeDot( "dot/%s.after_genWrapper.dot" );
	if ( file->errHandler.isFatal () ) return;

	pruneRefs();
	if ( MAKE_DOT ) makeDot( "dot/%s.after_pruneRefs.dot" );
	if ( file->errHandler.isFatal () ) return;

	compTagFinally();

	file->buildSymbolSearch ( false );

	if ( file->errHandler.isFatal() ) return;

	// we want to run the optimizer over the code.. there may well be byte-code generated via the default parameters
	opt.go ( file->sCache.get ( entry ? entry : "<NO MAIN>" ) );

	for ( auto& it : file->classList )
	{
		this->listing.emitClassInfo( it.second );
	}

	QueryPerformanceCounter( &end );

	tOptimizer = (end.QuadPart - start.QuadPart) / frequency.QuadPart;

	if ( this->file->errHandler.isFatal() ) return;

	QueryPerformanceCounter( &start );

	if ( MAKE_DOT ) makeDot( "dot/%s.before_codeEmission.dot" );
	// emit code for global initializers

	if ( file->errHandler.isFatal() ) return;

	compileGlobalInitializers( entry ? false : true );

	// any initializers will have been emitted... so handle emit them in the listing
	symbolStack			 sym( file );
	opUsingList			 opUse;
	symbolSpaceNamespace ns( file, &file->ns, &opUse );
	sym.push( &ns );

	listing.emitOps( &sym );

	if ( entry )
	{
		// to make it easier to find, always emit main first (just a debug thing)
		auto funcIt = file->functionList.find ( file->sCache.get ( entry ) );
		if ( funcIt != file->functionList.end () )
		{
			try
			{
				errorLocality e ( &file->errHandler, (*funcIt).second->location );
				compileFunction ( (*funcIt).second );
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( false, err );
			}
		}
	}

/*
	{
		// to make it easier to find, always emit main first (just a debug thing)
		auto funcIt = file->functionList.find ( file->sCache.get ( "objDatabase.open.method" ) );
		if ( funcIt != file->functionList.end () )
		{
			try
			{
				errorLocality e ( &file->errHandler, (*funcIt).second->location );
				compileFunction ( (*funcIt).second );
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( false, err );
			}
		}
	}
*/
	// compile the functions
	for ( auto it = file->functionList.begin(); it != file->functionList.end(); it++ )
	{
		if ( !it->second->isInterface && !it->second->isIterator && (!entry || (entry && it->second->name != entry)) )
		{
			try	{
				errorLocality e ( &file->errHandler, (*it).second->location );
				compileFunction ( (*it).second );
			} catch ( errorNum err )
			{
				file->errHandler.throwFatalError ( false, err );
			}
		}
	}

	if ( file->errHandler.isFatal() ) return;

	// just used to push the final op through the lister
	codeSegment.putOp( fglOpcodes::noop );
	listing.emitOps( &sym );

	// srcLocation of -1 is indicator that we've reached the end of debug info
	debug.addEnd( codeSegment.getNumOps(), sym.getStackSize() );

	QueryPerformanceCounter( &end );

	tBackEnd = (end.QuadPart - start.QuadPart) / frequency.QuadPart;
}

bool compExecutable::genLS ( char const* entry, int64_t sourceIndex )
{
	file->errHandler.resetFatalError();

	processGlobals( sourceIndex, true );
	makeConstructorDestructors( true, sourceIndex );
	makeExtensionMethods( true, sourceIndex );
	compMakeForEach( sourceIndex, true );
	makeClass( true );
	makeParentClosure( sourceIndex );
	file->buildSymbolSearch ( true );
	makeSymbols( true, sourceIndex );
	file->buildSymbolSearch ( false );
	makeAtoms( true, sourceIndex );
	file->buildSymbolSearch ( false );
	// do AFTER makeSymbols so we don't get any automatic at the top layer!!!
	genWrapper( sourceIndex );
	// we want to run the optimizer over the code.. there may well be byte-code generated via the default parameters
	optimizer ( file, this ).goLS ( file->sCache.get ( entry ? entry : "<NO MAIN>" ) );

	return true;
}
 
void compExecutable::serialize ( BUFFER *buff, bool isLibrary )
{
	std::vector<compFunc *>::size_type	 funcIndex;
	std::vector<compClass *>::size_type	 classIndex;
	fgxFileHeader *header;
	uint32_t							 count;
	uint32_t							 loop;
	uint32_t							 nCtxt;
	uint32_t							 nElems;
	uint32_t							 nVtab;
	fgxGlobalEntry						 gEntry = {};

	bufferMakeRoom ( buff, sizeof ( fgxFileHeader ) );
	bufferAssume ( buff, sizeof ( fgxFileHeader ) );
	header = (fgxFileHeader *) bufferBuff ( buff );
	memset ( header, 0, sizeof ( *header ) );

	memcpy ( header->sig, sxSig, sizeof ( header->sig ) );
	header->version = sxFileVersion;

	header->offsetBSSGlobals = bssSize;
	header->offsetGlobalInitializers = (uint32_t)globalInitOffset;
	bssSize += (uint32_t) (globals.size () * sizeof ( ptrdiff_t /* VAR */ ));
	header->bssSize = bssSize;

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetGlobalSymbolTable = (uint32_t) bufferSize ( buff );
	for ( loop = 0; loop < globals.size (); loop++ )
	{
		strcpy_s ( gEntry.name, sizeof ( gEntry ), globals[loop]->name );
		gEntry.index = globals[loop]->globalIndex;
		gEntry.left = globals[loop]->left;
		gEntry.right = globals[loop]->right;
		gEntry.isExportable = globals[loop]->isExportable;
		gEntry.nextExportable = globals[loop]->nextExportable;
		bufferWrite ( buff, &gEntry, sizeof ( gEntry ) );
	}

	header = (fgxFileHeader *) bufferBuff ( buff );
	header->globalRoot = globalRoot;
	header->numGlobals = (uint32_t) globals.size ();
	header->globalExportRoot = (uint32_t)globalExportableRoot;

	std::stable_sort ( symbols.begin (), symbols.end (), []( visibleSymbols const &left, visibleSymbols const &right ) {return left.validStart < right.validStart; } );
	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetSymbolTable = (uint32_t) bufferSize ( buff );
	header->numSymbols = (uint32_t) symbols.size ();

	size_t symbolNameOffset = (uint32_t) bufferSize ( buff ) + sizeof ( fgxSymbols ) * symbols.size ();
	for ( loop = 0; loop < symbols.size (); loop++ )
	{
		fgxSymbols	fgxSym = {};

		fgxSym.offsetName = (uint32_t) symbolNameOffset;
		symbolNameOffset += symbols[loop].name.size () + 1;

		fgxSym.name			= 0;
		fgxSym.validStart	= symbols[loop].validStart;
		fgxSym.validEnd		= symbols[loop].validEnd;
		fgxSym.index		= symbols[loop].index;
		fgxSym.noRef		= symbols[loop].noRef;
		fgxSym.isStackBased = symbols[loop].isStackBased;
		fgxSym.isParameter	= symbols[loop].isParameter;

		bufferWrite ( buff, &fgxSym, sizeof ( fgxSym ) );
	}

	for ( loop = 0; loop < symbols.size (); loop++ )
	{
		bufferWrite ( buff, symbols[loop].name.c_str (), symbols[loop].name.size () + 1 );
	}

	// MUST be done AFTER the BSS Globals values have been written as this will change the bss size requirement
	header = (fgxFileHeader *)bufferBuff ( buff ); 
	header->bssSize = bssSize;

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetAtomTable = (uint32_t) bufferSize ( buff );
	atom.serializeAtoms ( buff );

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetAtomFixups = (uint32_t) bufferSize ( buff );
	atom.serializeFixups ( buff );

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetTCList = (uint32_t) bufferSize ( buff );
	tcList.serialize ( buff );

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetCodeSegment = (uint32_t) bufferSize ( buff );
	codeSegment.serialize ( buff );

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetDataSegment = (uint32_t) bufferSize ( buff );
	dataSegment.serialize ( buff );

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetSourceFiles = (uint32_t) bufferSize ( buff );
	file->srcFiles.serializeCompiled ( buff );

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetDebug = (uint32_t) bufferSize ( buff );
	debug.serialize ( buff );

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->offsetFuncList = (uint32_t) bufferSize ( buff );

	count = 0;
	for ( funcIndex = 0; funcIndex < funcDef.size (); funcIndex++ )
	{
		if ( !funcDef[funcIndex]->isInterface )
		{
			count++;
		}
	}
	bufferPut32 ( buff, count );
	for ( funcIndex = 0; funcIndex < funcDef.size (); funcIndex++ )
	{
		if ( !funcDef[funcIndex]->isInterface )
		{
			if ( !funcDef[funcIndex]->isInterface ) funcDef[funcIndex]->serialize ( buff );
		}
	}

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->numFuncDefs = count;
	header->offsetClassList = (uint32_t) bufferSize ( buff );

	count = 0;
	for ( classIndex = 0; classIndex < classDef.size (); classIndex++ )
	{
		if ( !classDef[classIndex]->isInterface ) count++;
	}
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->numClass = count;
	bufferPut32 ( buff, count );
	for ( classIndex = 0; classIndex < classDef.size (); classIndex++ )
	{
		if ( !classDef[classIndex]->isInterface )
		{
			classDef[classIndex]->serialize ( buff, &nElems, &nCtxt, &nVtab );
			header = (fgxFileHeader *) bufferBuff ( buff );
			header->numElements += nElems;
			header->numContextEntries += nCtxt;
			header->numVtableEntries += nVtab;
		}
	}

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->numLibraries = (uint32_t) libraries.size ();
	header->offsetLibraries = (uint32_t) bufferSize ( buff );
	for ( auto &it : libraries )
	{
		fgxName lib = {};

		std::filesystem::path p ( it.c_str () );
		p.replace_extension ( ".sll" );

		strcpy_s ( lib.name, sizeof ( lib.name ), p.generic_string ().c_str () );
		buff->write ( &lib, sizeof ( lib ) );
	}

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->numModules = (uint32_t) modules.size ();
	header->offsetModules = (uint32_t) bufferSize ( buff );
	for ( auto &it : modules )
	{
		fgxName lib = {};
		std::filesystem::path p ( it.c_str () );
		p.replace_extension ( ".slb" );

		strcpy_s ( lib.name, sizeof ( lib.name ), p.generic_string ().c_str () );
		buff->write ( &lib, sizeof ( lib ) );
	}

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	header->numBuildDirectories = (uint32_t) file->buildSources.size ();
	header->offsetBuildDirectories = (uint32_t) bufferSize ( buff );
	for ( auto &it : file->buildSources )
	{
		fgxName dir = {};

		strcpy_s ( dir.name, sizeof ( dir.name ), it.c_str () );
		buff->write ( &dir, sizeof ( dir ) );
	}

	bufferAlign ( buff, 16 );
	header = (fgxFileHeader *) bufferBuff ( buff );
	if ( isLibrary )
	{
		header->offsetInterface = (uint32_t) bufferSize ( buff );
		file->serialize ( buff, true );
	} else
	{
		header->offsetInterface = 0;
	}
}
