
#include "compilerBCGenerator.h"
#include "avl.h"

compClassElementSearch::compClassElementSearch()
{
	memset ( leftIndex, 0, sizeof ( leftIndex ) );
	memset ( rightIndex, 0, sizeof ( rightIndex ) );
	memset ( leftNSIndex, 0, sizeof ( leftIndex ) );
	memset ( rightNSIndex, 0, sizeof ( rightIndex ) );
}


struct valCmpKey {
	cacheString				name;
	char const *			func = nullptr;
	compClassElementSearch *elem;

	valCmpKey ( cacheString const &name, compClassElementSearch *elem ) : name ( name ), elem ( elem )
	{}
	valCmpKey ( cacheString const & name, compClassElementSearch* elem, char const *func ) : name ( name ), func ( func ), elem ( elem )
	{}
	virtual ~valCmpKey ( ) {};
};

void compClass::serialize ( BUFFER *buff, uint32_t *nElements, uint32_t *nStart, uint32_t *nVtab )
{
	fgxClassEntry			*entry;
	fgxClassElement			 elem = {};
	fgxClassVtableEntry		 vTabEntry = {};
	fgxClassSetupEntry		 setupEntry = {};
	size_t					 loop;
	size_t					 loop2;
	size_t					 entryOffset;

	entryOffset = (uint32_t) bufferSize ( buff );
	bufferMakeRoom ( buff, sizeof ( fgxClassEntry ) );
	bufferAssume ( buff, sizeof ( fgxClassEntry ) );

	entry = (fgxClassEntry *) (bufferBuff ( buff ) + entryOffset);
	entry->atom = atom;
	entry->nElements = (int32_t) elements.size ();
	entry->newElem = newIndex;
	entry->defaultMethodElem = defaultMethodIndex;
	entry->defaultAccessElem = defaultAccessIndex;
	entry->defaultAssignElem = defaultAssignIndex;
	entry->releaseElem = releaseIndex;
	entry->packElem = packIndex;
	entry->unpackElem = unpackIndex;
	entry->nInstanceVar = nIvar;

	for ( loop = 0; loop < int(fgxClassElemSearchType::fgxClassElemMaxSearchType); loop++ )
	{
		if ( searchTree[loop]->avl_root )
		{
			entry->searchRootIndex[loop] = (((valCmpKey *) searchTree[loop]->avl_root->avl_data))->elem->index + 1;
		} else
		{
			entry->searchRootIndex[loop] = 0;
		}
	}
	for ( loop = 0; loop < int ( fgxClassElemSearchType::fgxClassElemMaxSearchType); loop++ )
	{
		if ( nsSearchTree[loop]->avl_root )
		{
			entry->nsSearchRootIndex[loop] = (((valCmpKey *) nsSearchTree[loop]->avl_root->avl_data))->elem->index + 1;
		} else
		{
			entry->nsSearchRootIndex[loop] = 0;
		}
	}

	bufferAlign ( buff, 16 );
	entry = (fgxClassEntry *) (bufferBuff ( buff ) + entryOffset);
	entry->offsetElements = (uint32_t) bufferSize ( buff );
	*nElements = (uint32_t) elements.size ();
	size_t outNameOffset = (uint32_t) bufferSize ( buff ) + sizeof ( elem ) *  elements.size ();
	for ( loop = 0; loop < elements.size (); loop++ )
	{
		elem.offsetName = (uint32_t) outNameOffset;
		outNameOffset += elements[loop].name.size() + 1;

		stringi fullName;
		if ( elements[loop].ns )
		{
			fullName = (stringi) elements[loop].ns + (stringi) elements[loop].name;
		} else
		{
			fullName = (stringi) elements[loop].name;
		}
		elem.offsetFullName = (uint32_t)outNameOffset;
		outNameOffset += fullName.size() + 1;

		elem.type = elements[loop].type;
		elem.dataMethodAccess = elements[loop].methodAccess.data.all;					
		elem.dataAssign		  = elements[loop].assign.data.all;
		elem.atomMethodAccess = elements[loop].methodAccess.atom;
		elem.atomAssign		  = elements[loop].assign.atom;
		for ( loop2 = 0; loop2 < int ( fgxClassElemSearchType::fgxClassElemMaxSearchType ); loop2++ )
		{
			elem.leftSearchIndex[loop2] = elements[loop].leftIndex[loop2];
			elem.rightSearchIndex[loop2] = elements[loop].rightIndex[loop2];
			elem.leftNSSearchIndex[loop2] = elements[loop].leftNSIndex[loop2];
			elem.rightNSSearchIndex
				[loop2] = elements[loop].rightNSIndex[loop2];
		}
		elem.isVirtual = elements[loop].isVirtual;
		elem.isStatic = elements[loop].isStatic;
		elem.objectDeltaMethodAccess	= elements[loop].methodAccess.objectStart;
		elem.objectDeltaAssign			= elements[loop].assign.objectStart;
		bufferWrite ( buff, &elem, sizeof ( elem ) );
	}

	for ( loop = 0; loop < elements.size (); loop++ )
	{
		bufferWrite ( buff, elements[loop].name.c_str(), elements[loop].name.size() + 1 );
		stringi fullName;
		if ( elements[loop].ns )
		{
			fullName = (stringi) elements[loop].ns + (stringi) elements[loop].name;
		} else
		{
			fullName = (stringi) elements[loop].name;
		}
		bufferWrite( buff, fullName.c_str(), fullName.size() + 1 );
	}

	bufferAlign ( buff, 16 );
	entry = (fgxClassEntry *) (bufferBuff ( buff ) + entryOffset);
	entry->offsetOverload = (uint32_t) bufferSize ( buff );
	for ( auto it : overload )
	{
		bufferPut32 ( buff, it );
	}

	bufferAlign ( buff, 16 );
	entry = (fgxClassEntry *) (bufferBuff ( buff ) + entryOffset);
	entry->nVtableEntries = (uint32_t) vTable.size ();
	entry->offsetVTable = (uint32_t) bufferSize ( buff );
	*nVtab = entry->nVtableEntries;

	for ( loop = 0; loop < vTable.size (); loop++ )
	{
		vTabEntry.delta = vTable[loop].delta;
		vTabEntry.atom = vTable[loop].atom;
		bufferWrite ( buff, &vTabEntry, sizeof ( vTabEntry ) );
	}

	bufferAlign ( buff, 16 );
	entry = (fgxClassEntry *) (bufferBuff ( buff ) + entryOffset);
	entry->nInherit = (uint32_t) ctxtInit.size();
	entry->offsetInherit = (uint32_t) bufferSize ( buff );
	*nStart = entry->nInherit;
	for ( loop = ctxtInit.size(); loop;  )
	{
		loop--;													// initialize in reverse order that we saw them so that bases are initialized before derived's
		setupEntry.atom = ctxtInit[loop].atom;
		setupEntry.objectOffset = ctxtInit[loop].objectDelta;
		setupEntry.vTableOffset = ctxtInit[loop].vTableOffset;
		bufferWrite ( buff, &setupEntry, sizeof ( setupEntry ) );
	}

	entry = (fgxClassEntry *) (bufferBuff ( buff ) + entryOffset);
	entry->nextClassOffset = (uint32_t) bufferSize ( buff );
}

void compExecutable::makeClass ( opClass *oClass, bool isLS )
{
	// ensure every class has a constructor
	errorLocality e1 ( &file->errHandler, oClass->location );

	if ( oClass->isInterface )
	{
		atom.addProvisional ( oClass->name, atomListType::atomClass );
	} else
	{
		atom.add ( oClass->name, atomListType::atomClass );
	}

	errorLocality e2 ( &file->errHandler, oClass->location );
	compileClass ( oClass, isLS );
}

void compExecutable::makeClass ( bool isLS )
{
	// prune any unused elements from the class elements lists.   this happens when we know they are unused
	// we can't delete them at that point as we need the back-pointers to point back to the class's elements list
	// and deleting them can change the vector and cause invalid pointers
	file->staticElemMap.clear ();

	for ( auto &it : opOverloads )
	{
		it.clear ();
	}

	for ( auto &cls : file->classList )
	{
		for ( auto it = cls.second->elems.begin (); it != cls.second->elems.end (); )
		{
			if ( !(*it)->name.size () )
			{
				it = cls.second->elems.erase ( it );
			} else
			{
				it++;
			}
		}
		cls.second->cClass = nullptr;
	}
	for ( auto &cls : file->classList )
	{
		makeClass ( cls.second, isLS );
	}
}

uint32_t compClass::finalizeTree ( struct avl_table *tree, fgxClassElemSearchType searchType )
{
	struct avl_traverser	trav;
	compClassElementSearch *elem;
	compClassElementSearch *elem2;
	valCmpKey *avlElem;

	avlElem = (valCmpKey *)avl_t_first ( &trav, tree );
	if ( avlElem )
	{
		while ( avlElem )
		{
			elem = avlElem->elem;
			if ( trav.avl_node->avl_link[0] )
			{
				elem2 = ((valCmpKey*) trav.avl_node->avl_link[0]->avl_data)->elem;
				elem->leftIndex[int(searchType)] = elem2->index + 1;
			} else
			{
				elem->leftIndex[int(searchType)] = 0;
			}
			if ( trav.avl_node->avl_link[1] )
			{
				elem2 = ((valCmpKey*) trav.avl_node->avl_link[1]->avl_data)->elem;
				elem->rightIndex[int ( searchType)] = elem2->index + 1;
			} else
			{
				elem->rightIndex[int(searchType)] = 0;
			}
			avlElem = (valCmpKey *)avl_t_next ( &trav );
		}
		return ((valCmpKey *) tree->avl_root->avl_data)->elem->index + 1;
	}
	return 0;
}

uint32_t compClass::finalizeNSTree ( struct avl_table *tree, fgxClassElemSearchType searchType )
{
	struct avl_traverser	trav;
	compClassElementSearch *elem;
	compClassElementSearch *elem2;
	valCmpKey *avlElem;

	avlElem = (valCmpKey *) avl_t_first ( &trav, tree );
	if ( avlElem )
	{
		while ( avlElem )
		{
			elem = avlElem->elem;
			if ( trav.avl_node->avl_link[0] )
			{
				elem2 = ((valCmpKey*) trav.avl_node->avl_link[0]->avl_data)->elem;
				elem->leftNSIndex[int(searchType)] = elem2->index + 1;
			} else
			{
				elem->leftNSIndex[int(searchType)] = 0;
			}
			if ( trav.avl_node->avl_link[1] )
			{
				elem2 = ((valCmpKey*) trav.avl_node->avl_link[1]->avl_data)->elem;
				elem->rightNSIndex[int(searchType)] = elem2->index + 1;
			} else
			{
				elem->rightNSIndex[int(searchType)] = 0;
			}
			avlElem = (valCmpKey *) avl_t_next ( &trav );
		}
		return ((valCmpKey *) tree->avl_root->avl_data)->elem->index + 1;
	}
	return 0;
}

bool compClass::isElement ( struct avl_table *tree, cacheString const &name, fgxClassElementType type )
{
	valCmpKey key ( name, nullptr );
	return avl_find ( tree, &key ) ? true : false;
}

compClassElementSearch *compClass::getElement ( cacheString const &name )
{
	if ( name[0] == ':' )
	{
		return nullptr;		// if we're from the root then we can't be just within this class
	}
	auto it = elemMap.find ( name );
	if ( it!= elemMap.end() )
	{
		return &elements[it->second];
	}
	return nullptr;
}

compClassElementSearch *compClass::getElement ( fgxClassElemSearchType type, cacheString const &name )
{
	// we can NOT use the search tree... the search tree does NOT have all the methods that we need, specfically
	// it does not have new (and inherited new)/release methods.   we need these during construction since we
	// internally generate a base::new() call and need to find the element to call.   to do that we need to walk
	// the entire element tree and not just the search tree (which is there for runtime creation doesn't have
	// special methods included in it

	auto it = elemMap.find ( name );
	if ( it!= elemMap.end() )
	{
		auto elem = &elements[it->second];
		switch ( type )
		{
			case fgxClassElemSearchType::fgxClassElemAllPrivate:
				break;
			case fgxClassElemSearchType::fgxClassElemAllPublic:
				if ( elem->scope > fgxClassElementScope::fgxClassScope_public ) return nullptr;
				break;
			case fgxClassElemSearchType::fgxClassElemPublicMethod:
				if ( elem->type != fgxClassElementType::fgxClassType_method ) return nullptr;
				if ( elem->scope > fgxClassElementScope::fgxClassScope_public ) return nullptr;
				return elem;
			case fgxClassElemSearchType::fgxClassElemPrivateMethod:
				if ( elem->type != fgxClassElementType::fgxClassType_method ) return nullptr;
				return elem;
			case fgxClassElemSearchType::fgxClassElemPublicAccess:
				if ( elem->scope > fgxClassElementScope::fgxClassScope_public ) return nullptr;
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_prop:
						if ( elem->methodAccess.atom == -1 )
						{
							return nullptr;
						}
						break;
						// these are read accessable;
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
					case fgxClassElementType::fgxClassType_inherit:
						break;
					default:
						return nullptr;
				}
				break;
			case fgxClassElemSearchType::fgxClassElemPrivateAccess:
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_prop:
						if ( elem->methodAccess.atom == -1 )
						{
							return nullptr;
						}
						break;
						// these are read accessable;
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
					case fgxClassElementType::fgxClassType_const:
					case fgxClassElementType::fgxClassType_inherit:
						break;
					default:
						return nullptr;
				}
				break;
			case fgxClassElemSearchType::fgxClassElemPublicAssign:
				if ( elem->scope > fgxClassElementScope::fgxClassScope_public ) return nullptr;
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_prop:
						if ( elem->assign.atom == -1 )
						{
							return nullptr;
						}
						break;
						// these are read accessable;
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
						break;
					default:
						return nullptr;
				}
				break;
			case fgxClassElemSearchType::fgxClassElemPrivateAssign:
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_prop:
						if ( elem->assign.atom == -1 )
						{
							return nullptr;
						}
						break;
						// these are read accessable;
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
						break;
					default:
						return nullptr;
				}
				break;
			case fgxClassElemSearchType::fgxClassElemInherit:
				if ( elem->type != fgxClassElementType::fgxClassType_inherit ) return nullptr;
				return elem;
			default:
				return nullptr;
		}
		return elem;
	}
	return nullptr;
}

void compClass::buildTree ( void )
{
	for ( uint32_t loop = 0; loop < elements.size (); loop++ )
	{
		elements[loop].index = loop;			// so we can find our array index from an avl search

		cacheString name = elements[loop].name;
		cacheString fullName = elements[loop].ns.size ( ) ? oFile->sCache.get ( (stringi) elements[loop].ns + (stringi)elements[loop].name) : cacheString();

		switch ( elements[loop].scope )
		{
			case fgxClassElementScope::fgxClassScope_public:
				switch ( elements[loop].type )
				{
					case fgxClassElementType::fgxClassType_method:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateMethod)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPublicMethod)], new valCmpKey ( name, &elements[loop] ) );
						}
						if ( fullName )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateMethod)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPublicMethod)], new valCmpKey ( fullName, &elements[loop] ) );
						}
						break;
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPublicAssign)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( name, &elements[loop] ) );
						}
						if ( fullName )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPublicAssign)], new valCmpKey ( fullName, &elements[loop] ) );
						}
						break;
					case fgxClassElementType::fgxClassType_const:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( name, &elements[loop] ) );
						}
						if ( fullName )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( fullName, &elements[loop] ) );
						}
						break;
					case fgxClassElementType::fgxClassType_prop:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( name, &elements[loop] ) );

							if ( elements[loop].assign.atom )
							{
								avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( name, &elements[loop] ) );
								avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPublicAssign)], new valCmpKey ( name, &elements[loop] ) );
							}
							if ( elements[loop].methodAccess.atom )
							{
								avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( name, &elements[loop] ) );
								avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( name, &elements[loop] ) );
							}
						}
						if ( fullName )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( fullName, &elements[loop] ) );
							if ( elements[loop].assign.atom )
							{
								avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( fullName, &elements[loop] ) );
								avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPublicAssign)], new valCmpKey ( fullName, &elements[loop] ) );
							}
							if ( elements[loop].methodAccess.atom )
							{
								avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( fullName, &elements[loop] ) );
								avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( fullName, &elements[loop] ) );
							}
						}
						break;
					case fgxClassElementType::fgxClassType_inherit:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemInherit)], new valCmpKey ( name, &elements[loop] ) );
 							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( name, &elements[loop] ) );
						}
						if ( fullName )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPublic)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemInherit)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPublicAccess)], new valCmpKey ( fullName, &elements[loop] ) );
						}
						break;
					default:
						break;
				}
				break;
				// both private and protected are not visible outside of the class...
			case fgxClassElementScope::fgxClassScope_protected:
			case fgxClassElementScope::fgxClassScope_private:
				switch ( elements[loop].type )
				{
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( name, &elements[loop] ) );
						}
						if ( fullName.size () )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( fullName, &elements[loop] ) );
						}
						break;
					case fgxClassElementType::fgxClassType_const:
					case fgxClassElementType::fgxClassType_inherit:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( name, &elements[loop] ) );
						}
						if ( fullName.size () )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( fullName, &elements[loop] ) );
						}
						break;
					case fgxClassElementType::fgxClassType_prop:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							if ( elements[loop].assign.atom )
							{
								avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( name, &elements[loop] ) );
							}
							if ( elements[loop].methodAccess.atom )
							{
								avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( name, &elements[loop] ) );
							}
						}
						if ( fullName.size () )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							if ( elements[loop].assign.atom )
							{
								avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAssign)], new valCmpKey ( fullName, &elements[loop] ) );
							}
							if ( elements[loop].methodAccess.atom )
							{
								avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateAccess)], new valCmpKey ( fullName, &elements[loop] ) );
							}
						}
						break;
					case fgxClassElementType::fgxClassType_method:
						if ( !isElement ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateMethod)], elements[loop].name, elements[loop].type ) )
						{
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( name, &elements[loop] ) );
							avl_insert ( searchTree[int(fgxClassElemSearchType::fgxClassElemPrivateMethod)], new valCmpKey ( name, &elements[loop] ) );
						}
						if ( fullName.size () )
						{
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemAllPrivate)], new valCmpKey ( fullName, &elements[loop] ) );
							avl_insert ( nsSearchTree[int(fgxClassElemSearchType::fgxClassElemPrivateMethod)], new valCmpKey ( fullName, &elements[loop] ) );
						}
						break;
					default:
						break;
				}
				break;
		}
	}
}

void compClass::addElement ( fgxClassElementType	 type,
							 fgxClassElementScope	 scope,
							 opClassElement			*e,
							 cacheString const		 &name,
							 cacheString const		 &ns,
							 int16_t				 objectOffset,
							 bool					 isVirtual,
							 bool					 isStatic,
							 bool				 	 isInterface,
							 uint32_t			 	 data,
							 opFunction				*func
)
{
	compClassElementSearch	elem;

	assert( type != fgxClassElementType::fgxClassType_method ||  func );

	elem.elem = e;
	elem.name = name;
	elem.ns = ns;
	elem.type = type;
	elem.scope = scope;
	elem.methodAccess.objectStart = objectOffset;
	elem.methodAccess.data.all = data;
	elem.isVirtual = isVirtual;
	elem.isStatic = isStatic;
	elem.methodAccess.func = func;
	elements.push_back ( elem );

	if ( elemMap.find ( name ) == elemMap.end () )
	{
		elemMap[name] = elements.size () - 1;
	}
	if ( ns.size () )
	{
		elemMap[oFile->sCache.get ( (stringi)ns + (stringi)name)] = elements.size () - 1;
	}
}

void compClass::addElement ( fgxClassElementType	 type,
							 fgxClassElementScope	 scope,
							 opClassElement			*e,
							 cacheString const		&name,
							 cacheString const		&ns,
							 int16_t				 objectOffset,
							 bool					 isVirtual,
							 bool					 isStatic,
							 bool				 	 isInterface,
							 uint32_t			 	 data,
							 cacheString const		&symbolName
)
{
	compClassElementSearch	elem;

	elem.elem = e;
	elem.name = name;
	elem.ns = ns;
	elem.type = type;
	elem.scope = scope;
	elem.methodAccess.objectStart = objectOffset;
	elem.methodAccess.data.all = data;
	elem.isVirtual = isVirtual;
	elem.isStatic = isStatic;
	elem.symbolName = symbolName;
	elements.push_back ( elem );
	if ( elemMap.find ( name ) == elemMap.end () )
	{
		elemMap[name] = elements.size () - 1;
	}
	if ( ns.size () )
	{
		elemMap[oFile->sCache.get ( (stringi)ns + (stringi)name)] = elements.size () - 1;
	}
}

void compClass::addElement ( fgxClassElementType	 type,
							 fgxClassElementScope	 scope,
							 opClassElement			*e,
							 cacheString const			&name,
							 cacheString const			&ns,
							 int16_t				 objectOffset,
							 bool					 isVirtual,
							 bool					 isStatic,
							 bool				 	 isInterface,
							 uint32_t			 	 atom,
							 uint32_t			 	 data,
							 opFunction				*func
)
{
	compClassElementSearch	elem;

	assert( type != fgxClassElementType::fgxClassType_method || func );

	elem.elem = e;
	elem.name = name;
	elem.ns = ns;
	elem.type = type;
	elem.scope = scope;
	elem.methodAccess.objectStart = objectOffset;
	elem.methodAccess.data.all = data;
	elem.methodAccess.atom = atom;
	elem.methodAccess.func = func;
	elem.isVirtual = isVirtual;
	elem.isStatic = isStatic;
	elements.push_back ( elem );
	if ( elemMap.find ( name ) == elemMap.end () )
	{
		elemMap[name] = elements.size () - 1;
	}
	if ( ns.size () )
	{
		elemMap[oFile->sCache.get ( (stringi)ns + (stringi)name)] = elements.size () - 1;

	}
}

void compClass::addElement ( fgxClassElementType	 type,
							 fgxClassElementScope	 scope,
							 opClassElement			*e,
							 cacheString const			&name,
							 cacheString  const			&ns,
							 int16_t				 objectOffset,
							 bool					 isVirtual,
							 bool					 isStatic,
							 bool				 	 isInterface,
							 uint32_t			 	 atomMethodAccess,
							 uint32_t			 	 atomAssign,
							 uint32_t			 	 dataMethodAccess,
							 uint32_t			 	 dataAssign,
							 opFunction				*methodAccessFunc,
							 opFunction				*assignFunc
							)
{
	compClassElementSearch	elem;

	assert( type != fgxClassElementType::fgxClassType_method || methodAccessFunc );

	elem.elem = e;
	elem.name = name;
	elem.ns = ns;
	elem.type = type;
	elem.scope = scope;
	elem.methodAccess.objectStart = objectOffset;
	elem.methodAccess.func = methodAccessFunc;
	elem.methodAccess.data.all = dataMethodAccess;
	elem.methodAccess.atom = atomMethodAccess;
	elem.assign.objectStart = objectOffset;
	elem.assign.func = assignFunc;
	elem.assign.data.all = dataAssign;
	elem.assign.atom = atomAssign;
	elem.isVirtual = isVirtual;
	elem.isStatic = isStatic;
	elements.push_back ( elem );
	if ( elemMap.find ( name ) == elemMap.end () )
	{
		elemMap[name] = elements.size () - 1;
	}
	if ( ns.size () )
	{
		elemMap[oFile->sCache.get ( (stringi)ns + (stringi)name)] = elements.size () - 1;
	}
}
// NOTE: we do NOT want to pass message map by reference... during recusion we want to copy this so we don't propagate messages to other incorrect levels after exiting the recursion
void compClass::make ( class compExecutable &comp, opClassElement *parentElem, opClass &oClass, fgxClassElementScope scope, int16_t &varPos, opClass *virtClass, std::vector<std::tuple<opClass *, uint32_t>> &virtFixup, bool virtInstantiation, std::map<cacheString, cacheString> messageMap, bool isLS )
{
	uint32_t								 atomMethodAccess;
	uint32_t								 atomAssign;
	int16_t									 objectPos = varPos;
	std::deque<int32_t>						 inheritPos;

	stringi nsTmp;
	for ( auto &it : elemStack )
	{
		nsTmp += (stringi)it->name + "::";
	}

	auto ns = oFile->sCache.get ( nsTmp );

	if ( virtClass )
	{
		// get the atom of our class

		auto it = virtualClasses.find ( &oClass );
		if ( it == virtualClasses.end () )
		{
			virtStart[&oClass] = nIvar;
			virtualClasses[&oClass] = { parentElem, ns, elemStack, varPos };
		}
		auto atom = comp.atom.addProvisional ( oClass.name, atomListType::atomClass );

		addElement ( fgxClassElementType::fgxClassType_inherit,
					 scope,
					 parentElem,
					 oClass.name,
					 ns,
					 objectPos,
					 true,
					 false,
					 oClass.isInterface,
					 (uint32_t)vTable.size(),
					 0
		);
		virtFixup.push_back ( std::make_tuple ( &oClass, (uint32_t) vTable.size() ) );
		vTable.push_back ( compClassVTable ( atom, 0 ) );
	} else
	{
		// get the atom of our class
		auto atom = comp.atom.addProvisional ( oClass.name, atomListType::atomClass );

		if ( varPos )		// only do this for things that are not our most-derived class
		{
			addElement ( fgxClassElementType::fgxClassType_inherit,
						 scope,
						 parentElem,
						 oClass.name,
						 ns,
						 objectPos,
						 virtInstantiation,
						 false,
						 oClass.isInterface,
						 varPos,
						 0
			);
			// set up an initialization context (used at object construction time to allow us to set up our class pointer and vtable pointer)
			ctxtInit.push_back ( compClassContext ( atom, varPos, (uint32_t) vTable.size () ) );
		}
		// gets incremented for all real instantiations of a class (not a virtual inherited class not yet made real)
		varPos++;
		// add in an additional object_root which is a pointer back to our most derived class
		varPos++;
	}

	if ( objectPos || virtClass )
	{
		elemStack.push_back ( &oClass );
	}


	nsTmp.clear ();
	for ( auto &it : elemStack )
	{
		nsTmp += (stringi)it->name + "::";
	}
	ns = oFile->sCache.get ( nsTmp );

	// process all the methods and instance variables

	for ( auto &elem : oClass.elems )
	{
		// scope is most restricted scope
		auto elemScope = scope > elem->scope ? scope : elem->scope;
		bool noVirt = false;
		cacheString name = elem->name;
		if ( messageMap.find ( oFile->sCache.get ( (stringi) ns + (stringi) name ) ) != messageMap.end() )
		{
			name = messageMap.find ( oFile->sCache.get ( (stringi) ns + (stringi)name ) )->second;
		}

		switch ( elem->type )
		{
			case fgxClassElementType::fgxClassType_message:
				messageMap[elem->data.message.oldName] = elem->name;
				break;
			case fgxClassElementType::fgxClassType_method:
				if ( name == oFile->defaultValue )
				{
					name = oFile->defaultMethodValue;
				}
				// create an atom entry for our method
				if ( name == oFile->newValue || name == oFile->newBaseValue || name == oFile->cNewValue || name == oFile->_packValue || name == oFile->_unpackValue || name == oFile->defaultPropValue )
				{
					elem->isVirtual = false;
					noVirt = true;
				}
				if ( elem->name == oFile->releaseValue || elem->name == oFile->releaseBaseValue )
				{
					elem->isVirtual = true;
					noVirt = true;
				}

				if ( oClass.isInterface )
				{
					atomMethodAccess = comp.atom.addProvisional ( elem->data.method.func, atomListType::atomFuncref );
				} else
				{
					atomMethodAccess = comp.atom.add ( elem->data.method.func, atomListType::atomFuncref );
				}
				
				if ( comp.file->functionList.find ( elem->data.method.func )->second->isExtension )
				{
					if ( !objectPos )
					{
						// we don't inherit extension methods... they are handled differently using makeExtensionMethods
						addElement (	elem->type,
										elemScope,
										elem,
										name,
										cacheString(),
										0,
										false,
										false,
										oClass.isInterface,
										atomMethodAccess,
										0,
										comp.file->functionList.find ( elem->data.method.func )->second
						);
					}
				} else
				{
					addElement ( elem->type,
								 elemScope,
								 elem,
								 name,
								 ns,
								 objectPos,
								 (elem->isVirtual || virtClass) && !noVirt,
								 elem->isStatic,
								 oClass.isInterface,
								 atomMethodAccess,
								 (uint32_t) vTable.size (),
								 comp.file->functionList.find ( elem->data.method.func )->second
					);

					elements.back ().elem->data.method.virtOverrides.insert ( comp.file->functionList.find ( elem->data.method.func )->second );

					if ( elem->isVirtual || virtClass )
					{
						int32_t delta = 0;
						// if it's virtual we need to compute the delta (if overridden) between our object and the derived object
						if ( elem->isVirtual && !noVirt )
						{
							for ( auto it = elements.begin (); it != elements.end (); it++ )
							{
								if ( (it->name == name) && it->ns == ns )
								{
									if ( it->ns == ns )
									{
										// not an override
										if ( !virtClass )
										{
											vTable.push_back ( compClassVTable ( atomMethodAccess, it->methodAccess.objectStart ) );
										}
									} else
									{
										// we're being overridden
										delta = it->methodAccess.objectStart - objectPos;
										if ( !virtClass )
										{
											vTable.push_back ( compClassVTable ( atomMethodAccess, (int16_t)delta ) );
										}
									}

									elements.back ().elem->data.method.virtOverrides.insert ( comp.file->functionList.find ( elem->data.method.func )->second );
									break;
								}
							}
						} else
						{
							delta = 0;
						}
						if ( virtClass )
						{
							virtFixup.push_back ( std::make_tuple ( &oClass, (uint32_t) vTable.size () ) );
							vTable.push_back ( compClassVTable ( atomMethodAccess, (int16_t)delta ) );
						}
						if ( elem->isStatic )
						{
							errorLocality e ( &oFile->errHandler, elem->location );
							if ( !isLS ) throw errorNum::scSTATIC_VIRTUAL; // should never get here, but just in case
							lsErrors.push_back( { errorNum::scSTATIC_VIRTUAL, elem } );
							continue;
						}
					}

					if ( !virtClass )
					{
						if ( (name == oFile->newValue) && !objectPos )
						{
							newIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->cNewValue) && !objectPos )
						{
							cNewIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->releaseValue) && !objectPos )
						{
							releaseIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->newBaseValue) && !objectPos )
						{
							newBaseIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->releaseBaseValue) && !objectPos )
						{
							releaseBaseIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->cReleaseValue) && !objectPos )
						{
							cReleaseIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->_packValue) && !objectPos )
						{
							packIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->_unpackValue) && !objectPos )
						{
							unpackIndex = (uint32_t) elements.size ();
						} else if ( (name == oFile->defaultMethodValue) )
						{
							defaultMethodIndex = (uint32_t) elements.size ();
						}
					}
				}
				break;
			case fgxClassElementType::fgxClassType_prop:
				if ( name == oFile->defaultValue )
				{
					name = oFile->defaultPropValue;
				}
				// create an atom entry for our method
				atomMethodAccess = -1;
				atomAssign = -1;
				if ( oClass.isInterface )
				{
					if ( elem->data.prop.access.size ( ) )
					{
						atomMethodAccess = comp.atom.addProvisional ( elem->data.prop.access, atomListType::atomFuncref );
					}
					if ( elem->data.prop.assign.size ( ) )
					{
						atomAssign = comp.atom.addProvisional ( elem->data.prop.assign, atomListType::atomFuncref );
					}
				} else
				{
					if ( elem->data.prop.access.size ( ) )
					{
						atomMethodAccess = comp.atom.add ( elem->data.prop.access, atomListType::atomFuncref );
					}
					if ( elem->data.prop.assign.size ( ) )
					{
						atomAssign = comp.atom.add ( elem->data.prop.assign, atomListType::atomFuncref );
					}
				}

				addElement ( elem->type,
							 elemScope,
							 elem,
							 name,
							 ns,
							 objectPos,
							 elem->isVirtual || virtClass,
							 false,
							 oClass.isInterface,
							 atomMethodAccess,
							 atomAssign,
							 ( uint32_t) vTable.size ( ),
							 ( uint32_t) vTable.size ( ) + (elem->data.prop.access.size ( ) ? 1 : 0),
							 elem->data.prop.access.size ( )?comp.file->functionList.find ( elem->data.prop.access )->second :nullptr,
							 elem->data.prop.assign.size ( )?comp.file->functionList.find ( elem->data.prop.assign )->second :nullptr
				);

				if ( elem->data.prop.access.size() )
				{
					elements.back ().elem->data.prop.accessVirtOverrides.insert ( comp.file->functionList.find ( elem->data.prop.access )->second );
				}
				if ( elem->data.prop.assign.size() )
				{
					elements.back ().elem->data.prop.assignVirtOverrides.insert ( comp.file->functionList.find ( elem->data.prop.assign )->second );
				}

				if ( elem->isVirtual || virtClass )
				{

					int32_t deltaAccess = 0;
					int32_t deltaAssign = 0;

					// if it's virtual we need to compute the delta (if overridden) between our object and the derived object
					if ( elem->isVirtual )
					{
						for ( auto &it : elements )
						{
							if ( (it.name == name) && (it.ns == ns) )
							{
								// we're being overridden
								if ( it.methodAccess.func )
								{
									deltaAccess = it.methodAccess.objectStart - objectPos;
									elements.back ().elem->data.prop.accessVirtOverrides.insert ( comp.file->functionList.find ( it.methodAccess.func->name )->second );
								}
								if ( it.assign.func )
								{
									deltaAssign = it.assign.objectStart - objectPos;
									elements.back ().elem->data.prop.assignVirtOverrides.insert ( comp.file->functionList.find ( it.assign.func->name )->second );
								}
								break;
							}
						}
					}
					if ( virtClass )
					{
						// the virtual object fixup's are of course identical, but we need to have the vtable map exactly to the virtFixup table
						if ( atomMethodAccess != -1 )
						{
							virtFixup.push_back ( std::make_tuple ( &oClass, ( uint32_t) vTable.size ( ) ) );
							vTable.push_back ( compClassVTable ( atomMethodAccess, (int16_t)deltaAccess ) );
						}
						if ( atomAssign != -1 )
						{
							virtFixup.push_back ( std::make_tuple ( &oClass, ( uint32_t) vTable.size ( ) ) );
							vTable.push_back ( compClassVTable ( atomAssign, (int16_t)deltaAssign ) );
						}
					}
					if ( elem->isStatic )
					{
						if ( !isLS ) throw errorNum::scSTATIC_VIRTUAL; // should never get here, but just in case
						lsErrors.push_back( { errorNum::scSTATIC_VIRTUAL, elem } );
						continue;
					}
				}

				if ( !virtClass )
				{
					if ( (name == oFile->defaultPropValue) )
					{
						if ( elem->data.prop.assign.size ( ) )
						{
							defaultAssignIndex = ( uint32_t) elements.size ( );
						}
						if ( elem->data.prop.access.size ( ) )
						{
							defaultAccessIndex = ( uint32_t) elements.size ( );
						}
					}
				}
				break;
			case fgxClassElementType::fgxClassType_iVar:
				if ( virtClass )
				{
					addElement ( elem->type,
								 elemScope,
								 elem,
								 name,
								 ns,
								 objectPos,
								 true,
								 false,
								 oClass.isInterface,
								 (uint32_t) vTable.size(),
								 0
					);
					virtFixup.push_back ( std::make_tuple ( &oClass, (uint32_t) vTable.size () ) );
					vTable.push_back ( compClassVTable ( 0, static_cast<int16_t>(varPos - objectPos) ) );
				} else
				{
					addElement ( elem->type,
								 elemScope,
								 elem,
								 name,
								 ns,
								 objectPos,
								 false,
								 false,
								 oClass.isInterface,
								 varPos++,
								 0
					);
				}
				break;
			case fgxClassElementType::fgxClassType_static:
			case fgxClassElementType::fgxClassType_const:
				addElement ( elem->type,
							 elemScope,
							 elem,
							 name,
							 ns,
							 objectPos,
							 false,
							 false,
							 oClass.isInterface,
							 elem->data.iVar.index,
							 elem->data.iVar.symbolName
				);
				break;
			default:
				break;
		}
	}

	// process all the overloaded operators.   these are handled just like any other method except that they also add their element index to the overload[] array for quick reference at runtime
	for ( size_t ovIndex = 0; ovIndex < oClass.opOverload.size (); ovIndex++ )
	{
		uint32_t atom;

		if ( oClass.opOverload[ovIndex] )
		{
			auto elemScope = scope > oClass.opOverload[ovIndex]->scope ? scope : oClass.opOverload[ovIndex]->scope;
			if ( oClass.isInterface )
			{
				atom = comp.atom.addProvisional ( oClass.opOverload[ovIndex]->data.method.func, atomListType::atomFuncref );
			} else
			{
				atom = comp.atom.add ( oClass.opOverload[ovIndex]->data.method.func, atomListType::atomFuncref );
			}

			auto *elem = oClass.opOverload[ovIndex];

			addElement ( elem->type,
						 elemScope,
						 elem,
						 elem->name,
						 ns,
						 objectPos,
						 elem->isVirtual || virtClass,
						 elem->isStatic,
						 oClass.isInterface,
						 atom,
						 (uint32_t) vTable.size (),
						 comp.file->functionList.find ( elem->data.method.func )->second
			);

			elements.back ().elem->data.method.virtOverrides.insert ( comp.file->functionList.find ( elem->data.method.func )->second );

			if ( elem->isVirtual || virtClass )
			{
				int32_t delta = 0;
				// if it's virtual we need to compute the delta (if overridden) between our object and the derived object
				if ( elem->isVirtual )
				{
					for ( auto it = elements.begin (); it != elements.end (); it++ )
					{
						if ( (it->name == elem->name) && (it->ns == ns) )
						{
							if ( it->ns == ns )
							{
								// not an override
								if ( !virtClass )
								{
									vTable.push_back ( compClassVTable ( atom, it->methodAccess.objectStart ) );
								}
							} else
							{
								// we're being overridden
								delta = it->methodAccess.objectStart - objectPos;
								if ( !virtClass )
								{
									vTable.push_back ( compClassVTable ( atom, (int16_t)delta ) );
								}
							}

							elements.back ().elem->data.method.virtOverrides.insert ( comp.file->functionList.find ( it->methodAccess.func->name )->second );
							break;
						}
					}
				} else
				{
					delta = 0;
				}
				if ( virtClass )
				{
					virtFixup.push_back ( std::make_tuple ( &oClass, (uint32_t) vTable.size () ) );
					vTable.push_back ( compClassVTable ( atom, (int16_t)delta ) );
				}

				if ( elem->isStatic )
				{
					if ( !isLS ) throw errorNum::scSTATIC_VIRTUAL; // should never get here, but just in case
					lsErrors.push_back( { errorNum::scSTATIC_VIRTUAL, elem } );
					continue;
				}
			}
			if ( !overload[ovIndex] )
			{
				overload[ovIndex] = (uint32_t) (elements.size ());	// needs to be one more than the index.  0 is not present
			}
		}
	}

	for ( auto elemIt = oClass.elems.begin (); elemIt != oClass.elems.end (); elemIt++ )
	{
		auto elemScope = scope > (*elemIt)->scope ? scope : (*elemIt)->scope;
		switch ( (*elemIt)->type )
		{
			case fgxClassElementType::fgxClassType_inherit:
				{
					auto c = comp.file->ns.getClass ( (*elemIt)->name );

					if ( c )
					{
						if ( c->isFinal )
						{
							if ( (*elemIt)->isStatic )
							{
								errorLocality e ( &oFile->errHandler, (*elemIt)->location );
								if ( !isLS ) throw errorNum::scINHERIT_FINAL; // should never get here, but just in case
								lsErrors.push_back ( { errorNum::scINHERIT_FINAL, *elemIt } );
								continue;
							}
						}
						if ( (*elemIt)->isVirtual )
						{
							if ( virtClass )
							{
								virtOrderRules.push_back ( { c, virtClass } );
							}
							make ( comp, *elemIt, *c, elemScope, varPos, c, virtFixup, false, messageMap, isLS );
						} else
						{
							make ( comp, *elemIt, *c, elemScope, varPos, virtClass, virtFixup, false, messageMap, isLS );
						}
					} else
					{
						errorLocality e ( &oFile->errHandler, (*elemIt)->location );
						if ( !isLS ) throw errorNum::scCLASS_NOT_FOUND; // should never get here, but just in case
						lsErrors.push_back ( { errorNum::scCLASS_NOT_FOUND, *elemIt } );
					}
				}
				break;
			default:
				break;
		}
	}

	if ( objectPos || virtClass )
	{
		elemStack.pop_back ();	// remove our name from the inherited namespace stack
	}
}

void compClass::make ( compExecutable &comp, opClass &oClass, bool isLS )
{
	std::vector<std::tuple<opClass *, uint32_t>> virtFixup;
	std::map<cacheString, cacheString> messageMap;

	oClass.cClass = this;

	// first context... this makes us the top-most super class
	strncpy_s ( name, sizeof ( name ), oClass.name.c_str (), _TRUNCATE );
	strncpy_s ( nSpace, sizeof ( nSpace ), comp.file->srcFiles.getName ( oClass.location.sourceIndex ), _TRUNCATE );

	// all directly inherit classes should have been constructed.   now build the indirectly inherited classes (virtual bases)
	//   we do this at the end so that they do not reorder the elements within a directly inherited base.   all directly inherited
	//   bases MUST always maintain the same construction regardless of any indirectly inherited bases which may or may not need
	//   concrete implementations due to prior definitions.  by delaying concrete implementations to after all directly inherited
	//   bases we ensure that this is the case and abi sanity
	make ( comp, nullptr, oClass, fgxClassElementScope::fgxClassScope_public, nIvar, nullptr, virtFixup, false, messageMap, isLS );

	for ( auto &[virtClass, virtInfo] : virtualClasses )
	{
		virtStart[virtClass] = nIvar;
		make ( comp, virtInfo.elem, *virtClass, fgxClassElementScope::fgxClassScope_public, nIvar, nullptr, virtFixup, true, messageMap, isLS );
	}

	// apply all the fixups
	for ( auto &it : virtFixup )
	{
		vTable[std::get<1> ( it )].delta = static_cast<int16_t>(vTable[std::get<1> ( it )].delta + (int16_t)(virtStart[std::get<0> ( it )]));
	}

	for ( auto &it : elements )
	{
		switch ( it.type )
		{
			case fgxClassElementType::fgxClassType_static:
			case fgxClassElementType::fgxClassType_const:
				oFile->staticElemMap.insert( { oFile->sCache.get( (stringi) oClass.name + "::" + (stringi) it.ns + (stringi) it.name ), &it } );
				break;
			case fgxClassElementType::fgxClassType_method:
				if  (	(it.name == oFile->newValue) || (it.name == oFile->newBaseValue) || (it.name == oFile->cNewValue) ||
						(it.name == oFile->releaseValue) || (it.name == oFile->releaseBaseValue) || (it.name == oFile->cReleaseValue) 
					)
				{
					oFile->staticElemMap.insert( { oFile->sCache.get( (stringi) oClass.name + "::" + (stringi) it.ns + (stringi) it.name ), &it } );
				}
				break;
			default:
				break;
		}
	}
}

int classNameCmp ( valCmpKey *elem1, valCmpKey *elem2 )
{
	return memcmpi ( elem1->name.c_str(), elem2->name.c_str (), elem1->name.size() + 1 );
}

void compExecutable::compileClass ( opClass *oClass, bool isLS )
{
	compClass *cClass;

	cClass = new compClass ( file, oClass );

	cClass->isInterface = oClass->isInterface;

	strcpy_s ( cClass->name, sizeof ( cClass->name ), oClass->name.c_str () );
	strcpy_s ( cClass->nSpace, sizeof ( cClass->nSpace ), file->srcFiles.getName ( oClass->location.sourceIndex ) );

	cClass->isInterface = oClass->isInterface;
	cClass->atom = atom.getAtom ( cClass->name );

	// this builds the element tree's as well as context for inheritance
	cClass->make ( *this, *oClass, isLS );

	// this builds the search tree's (AVL balanced tree) for the invidiual types (methods, properties, Propertys, etc.)

	cClass->buildTree ();
	cClass->finalize ();

	classDef.push_back ( cClass );

	// set the TYPE of self
	for ( auto it = oClass->elems.begin (); it != oClass->elems.end (); it++ )
	{
		switch ( (*it)->type )
		{
			case fgxClassElementType::fgxClassType_method:
				if ( !file->functionList.find ( (*it)->data.method.func )->second->isStatic && !file->functionList.find ( (*it)->data.method.func )->second->isExtension )
				{
					file->functionList.find ( (*it)->data.method.func )->second->params.setType ( file->selfValue, true, oClass->name );
				}
				break;
			case fgxClassElementType::fgxClassType_prop:
				if ( !(*it)->isStatic )
				{
					if ( (*it)->data.prop.access.size ( ) )
					{
						file->functionList.find ( (*it)->data.prop.access )->second->params.setType ( file->selfValue, true, oClass->name );
					}
					if ( (*it)->data.prop.assign.size ( ) )
					{
						file->functionList.find ( (*it)->data.prop.assign )->second->params.setType ( file->selfValue, true, oClass->name );
					}
				}
				break;
			default:
				break;
		}
	}
	for ( auto it = oClass->opOverload.begin (); it != oClass->opOverload.end (); it++ )
	{
		if ( *it )
		{
			file->functionList.find ( (*it)->data.method.func )->second->params.setType ( file->selfValue, true, oClass->name  );
		}
	}

	for ( size_t index = 0; index < int(fgxOvOp::ovMaxOverload); index++ )
	{
		if ( cClass->overload[index] )
		{
			opOverloads[index].push_back ( &cClass->elements[cClass->overload[index] - 1] );
		}
	}
}

bool compClass::virtualClassPredClass::operator()( opClass const *l, opClass const *r ) const
{
	// ensure ordering of virtual constructors in case they have dependencies
	for ( auto &it : virtOrderRules )
	{
		if ( std::get<0> ( it ) == l && std::get<1> ( it ) == r )
		{
			return true;
		} else if ( std::get<0> ( it ) == r && std::get<1> ( it ) == l )
		{
			return false;
		}

	}
	// no dependency but we must still be consistent as a predicate so choose a collating sequence.  in this case address of structures is consistent, fast, and suffices
	return l < r;
};


compClass::compClass ( opFile *oFile, opClass *oClass ) : virtOrderRules(), virtualClassPred ( virtOrderRules ), virtualClasses ( virtualClassPred )
{
	size_t	loop;

	this->oFile = oFile;
	this->oClass = oClass;
	atom = 0;
	nVirtual = 0;
	nIvar = 0;
	isInterface = true;

	overload.resize ( int ( fgxOvOp::ovMaxOverload ) );
	for ( auto &it : overload ) it = 0;

	for ( loop = 0; loop < int(fgxClassElemSearchType::fgxClassElemMaxSearchType); loop++ )
	{
		searchTree[loop] = avl_create ( (avl_comparison_func *) classNameCmp, 0, 0 );
		nsSearchTree[loop] = avl_create ( (avl_comparison_func *) classNameCmp, 0, 0 );
	}
}

void valCmpKeyCleanup (void* avl_item, void* avl_param)
{
	valCmpKey *key = reinterpret_cast<valCmpKey*>(avl_item);

	delete key;
}

compClass::~compClass ()
{
	oClass->cClass = nullptr;
	for ( size_t treeTloop = 0; treeTloop < int( fgxClassElemSearchType::fgxClassElemMaxSearchType); treeTloop++ )
	{
		avl_destroy ( searchTree[treeTloop], valCmpKeyCleanup);
		avl_destroy ( nsSearchTree[treeTloop], valCmpKeyCleanup);

	}
}
