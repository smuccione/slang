#include "dbIoServer.h"
#include "Db.h"
#include "odbf.h"

static int far ftxCmpLong ( void const *p1, void const *p2, size_t len )
{
	return (*((long *) p1) - *((long *)p2));
}

static int far ftxCmpDouble ( void const *p1, void const *p2, size_t len )
{
	return (int) (*((double *) p1) - *((double *) p2));
}

static unsigned long ftxGetFreePage ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	FTXfree					*freeBlock;
	unsigned long			 page;

	if ( !ftx->getRootBlock ( )->free )
	{
		ftx->fileSize += FTX_PAGE_SIZE * FTX_PAGE_ALLOCATION_QUANTA;
		ftx->mapFile ( );

		// get it again... mapFile will have negated the pointer!

		for ( page = ftx->getRootBlock ( )->nBlocks; page < ftx->getRootBlock ( )->nBlocks + FTX_PAGE_ALLOCATION_QUANTA; page++ )
		{
			freeBlock = ftx->getFreeBlock ( page );

			freeBlock->next = ftx->getRootBlock ( )->free;
			ftx->getRootBlock ( )->free = page;
		}

		ftx->getRootBlock ( )->nBlocks += FTX_PAGE_ALLOCATION_QUANTA;
	}
	// allocate the free page
	page = ftx->getRootBlock ( )->free;

	freeBlock = ftx->getFreeBlock ( page );

	// avoid the update to freeBlock to save us a disk write
	ftx->getRootBlock ( )->free = freeBlock->next;

	return page;
}

static void ftxFreePage ( FTX *ftx, FTX_CONNECTION *ftxConn, uint64_t nPage )

{
	FTX_ROOT	*rootBlock;
	FTXfree		*freeBlock;
	
	freeBlock = ftx->getFreeBlock ( nPage );
	rootBlock = ftx->getRootBlock ( );

	freeBlock->next = rootBlock->free;
	rootBlock->free = (unsigned long)nPage;
}

static bool ftxLocate ( FTX *ftx, FTX_CONNECTION *ftxConn, uint64_t page, char const *newKey, size_t newKeyLen, char mode )
{
	FTX_ROOT				*root;
	FTX_BRANCH				*branch;
	FTX_LEAF				*leaf;
	char				    *keyStart;
	char				    *keyPtr;
	char				     type;
	size_t				     depSize;
	size_t					 keySize;
	size_t					 keyLen;
	size_t					 nKeys;
	size_t					 pos;
	size_t					 lo;
	size_t					 hi;
	int						 comp;
	bool					 res;
	
	depSize = ftx->getHeader()->depSize;
	keySize = ftx->getHeader()->keySize;
	
	type = ftx->getBlockType ( page );
	switch ( type )
	{
		case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
			root = ftx->getRootBlock ();
			keyStart = root->getData ( );
			keyLen   = ftx->getHeader()->keySize + sizeof ( unsigned long );
			nKeys    = root->nKeys;			
			depSize  = sizeof ( unsigned long );
			break;
		case FTX_ROOT_TYPE | FTX_LEAF_TYPE:
			root = ftx->getRootBlock ();
			keyStart = root->getData ( );
			keyLen   = static_cast<size_t>(ftx->getHeader()->keySize) + ftx->getHeader()->depSize;
			nKeys    = root->nKeys;
			break;
		case FTX_BRANCH_TYPE:
			branch = ftx->getBranchBlock ( page );
			keyStart = branch->getData();
			keyLen   = static_cast<size_t>(ftx->getHeader()->keySize) + sizeof (unsigned long );
			nKeys    = branch->nKeys;
			depSize  = sizeof ( unsigned long );
			break;
		case FTX_LEAF_TYPE:
			leaf = ftx->getLeafBlock ( page );
			keyStart = leaf->getData ( );
			keyLen   = static_cast<size_t>(ftx->getHeader()->keySize) + ftx->getHeader()->depSize;
			nKeys    = leaf->nKeys;
			break;
		default:
			return false;
	}

	if ( nKeys )
	{
		pos    = 0;
		switch ( mode )
		{
			case TOP:
				pos  = 0;
				break;
			case BOTTOM:
				if ( type & FTX_LEAF_TYPE )
				{
					pos = nKeys - 1;
				} else
				{
					pos = nKeys;
				}
				break;
			case LEFT:
				lo = 0;
				hi = nKeys - 1;
				
				while (lo < hi)
				{
					pos    = (lo + hi) >> 1;
					keyPtr = keyStart + pos * keyLen + depSize;
					
					if ( ftx->getHeader()->isDescending )
					{
						comp   = ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) > 0 );
					} else
					{
						comp   = ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) < 0 );
					}
					
					if ( comp )
					{
						lo = pos + 1;
					} else
					{
						hi = pos;
					}
				}
				if ( lo == nKeys - 1 )
				{
					keyPtr = keyStart + lo * keyLen + depSize;
					if ( ftx->getHeader()->isDescending )
					{
						if ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) > 0 )
						{
							lo++;
						}
					} else
					{
						if ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) < 0 )
						{
							lo++;
						}
					}
				}
				pos = lo;
				break;
			case LEFT_FOUND:
				lo = 0;
				hi = nKeys - 1;
				
				while (lo < hi)
				{
					pos    = (lo + hi) >> 1;
					keyPtr = keyStart + pos * keyLen + depSize;
					
					if ( ftx->getHeader()->isDescending )
					{
						comp   = ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) > 0 );
					} else
					{
						comp   = ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) < 0 );
					}
					
					if ( comp )
					{
						lo = pos + 1;
					} else
					{
						hi = pos;
					}
				}
				if ( (lo == nKeys - 1) && (type & FTX_BRANCH_TYPE) )
				{
					keyPtr = keyStart + lo * keyLen + depSize;
					if ( ftx->getHeader()->isDescending )
					{
						if ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) > 0 )
						{
							lo++;
						}
					} else
					{
						if ( (ftx->cmpRtn) ( keyPtr, newKey, newKeyLen ) < 0 )
						{
							lo++;
						}
					}
				}
				pos = lo;
				break;
			case RIGHT_FOUND:
				lo = 0;
				hi = nKeys - 1;

				while ( lo < hi )
				{
					pos = (lo + hi) >> 1;
					keyPtr = keyStart + pos * keyLen + depSize;

					if ( ftx->getHeader ()->isDescending )
					{
						comp = (ftx->cmpRtn) (keyPtr, newKey, newKeyLen) * -1;
					} else
					{
						comp = (ftx->cmpRtn) (keyPtr, newKey, newKeyLen);
					}

					if ( comp < 0 )
					{
						lo = pos + 1;
					} else if ( comp == 0 )
					{
						lo = pos;
						if ( pos == (lo + hi) >> 1 )
						{
							keyPtr = keyStart + hi * keyLen + depSize;
							if ( ftx->getHeader ()->isDescending )
							{
								comp = (ftx->cmpRtn) (keyPtr, newKey, newKeyLen) * -1;
							} else
							{
								comp = (ftx->cmpRtn) (keyPtr, newKey, newKeyLen);
							}
							if ( !comp ) lo++;
						}
					} else
					{
						hi = pos;
					}
				}
				if ( (lo == nKeys - 1) && (type & FTX_BRANCH_TYPE) )
				{
					keyPtr = keyStart + lo * keyLen + depSize;
					if ( ftx->getHeader ()->isDescending )
					{
						if ( (ftx->cmpRtn) (keyPtr, newKey, newKeyLen) >= 0 )
						{
							lo++;
						}
					} else
					{
						if ( (ftx->cmpRtn) (keyPtr, newKey, newKeyLen) <= 0 )
						{
							lo++;
						}
					}
				}
				pos = lo;
				break;
		}
	} else
	{
	   pos = 0;
	}
	keyPtr = keyStart + pos * keyLen;

	ftxConn->stack.push_back ( FTX_STACK{ page, nKeys, pos } );
	
	switch ( type )
	{
		case FTX_BRANCH_TYPE:
		case FTX_BRANCH_TYPE | FTX_ROOT_TYPE:
			res = ftxLocate( ftx, ftxConn, *((long *)keyPtr), newKey, newKeyLen, mode );
			return res;
		case FTX_LEAF_TYPE:
		case FTX_LEAF_TYPE | FTX_ROOT_TYPE:
			switch ( mode )
			{
				case TOP:
				case BOTTOM:
					if ( nKeys )
					{
						memcpy ( ftxConn->knownDep, keyPtr,           depSize );
						memcpy ( ftxConn->knownKey, keyPtr + depSize, keySize );
						return true;
					} else
					{
						return false;
					}
				case LEFT:
					memcpy ( ftxConn->knownDep, keyPtr,           depSize );
					memcpy ( ftxConn->knownKey, keyPtr + depSize, keySize );
					return true;
				case LEFT_FOUND:
				case RIGHT_FOUND:
					if ( nKeys && !(ftx->cmpRtn) ( keyPtr + depSize, newKey, newKeyLen ) )
					{
						memcpy ( ftxConn->knownDep, keyPtr,           depSize );
						memcpy ( ftxConn->knownKey, keyPtr + depSize, keySize );
						return true;
					} else
					{
						memcpy ( ftxConn->knownDep, keyPtr,           depSize );
						memcpy ( ftxConn->knownKey, keyPtr + depSize, keySize );
						return false;
					}
			}
			break;
	}
	return false;
}

static int ftxKeyAdd ( FTX * ftx, FTX_CONNECTION *ftxConn, char const *key, char const *dep )

{
	FTX_BRANCH				*branch;
	FTX_ROOT				*root;
	FTX_LEAF				*leaf;
	char					*keyStart;
	char					*ptr1;
	unsigned long			 leftPage;
	unsigned long			 rightPage;
	unsigned char			 lHalf;
	unsigned char			 rHalf;
	unsigned char			 nKeys;
	int						 keyLen;
	char					 tmpKey[1024];

	ftx->lastUpdate++;
	
	switch ( ftx->getBlockType ( ftxConn->stack.back().nPage ) )
	{
		case FTX_ROOT_TYPE | FTX_LEAF_TYPE:
			if ( ftxConn->stack.back().nKeys < ftx->getHeader()->nRootLeafKeys )
			{
				root = ftx->getRootBlock ( );

				keyStart = (char *)(&root[1]);
				keyLen   = ftx->getHeader()->keySize + ftx->getHeader()->depSize;
				nKeys    = root->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				memmove ( ptr1 + keyLen, ptr1, (nKeys - ftxConn->stack.back().pos) * keyLen );
				memcpy ( ptr1, dep, ftx->getHeader()->depSize );
				memcpy ( ptr1 + ftx->getHeader()->depSize, key, ftx->getHeader()->keySize );
				
				root->nKeys++;
			} else
			{
				leftPage = ftxGetFreePage ( ftx, ftxConn );
				rightPage = ftxGetFreePage ( ftx, ftxConn );

				root      = ftx->getRootBlock();
				keyStart = root->getData ( );
				keyLen    = ftx->getHeader()->keySize + ftx->getHeader()->depSize;
				nKeys     = root->nKeys;

				auto leftEntry = ftx->getLeafBlock ( leftPage );
				auto rightEntry = ftx->getLeafBlock ( rightPage );

				leftEntry->type = FTX_LEAF_TYPE;
				rightEntry->type = FTX_LEAF_TYPE;

				leftEntry->nKeys = lHalf = (unsigned char) (nKeys >> 1);
				rightEntry->nKeys = rHalf = (unsigned char)((nKeys >> 1) + (nKeys & 1));
				
				memcpy ( leftEntry->getData(), keyStart, static_cast<size_t>(lHalf) * keyLen );
				memcpy ( rightEntry->getData(), keyStart + static_cast<ptrdiff_t>(lHalf) * keyLen, static_cast<size_t>(rHalf) * keyLen );
				
				root->type        = FTX_ROOT_TYPE | FTX_BRANCH_TYPE;
				root->nKeys       = 1;
				
				keyLen = static_cast<int>(ftx->getHeader()->keySize + ftx->getHeader()->depSize);
				
				if ( ftxConn->stack.back().pos <= lHalf )
				{
					ptr1 = leftEntry->getData() + keyLen * ftxConn->stack.back().pos;
					
					memmove ( ptr1 + keyLen, ptr1, (lHalf - ftxConn->stack.back().pos + 1) * keyLen );
					memcpy ( ptr1, dep, ftx->getHeader()->depSize );
					memcpy ( ptr1 + ftx->getHeader()->depSize, key, ftx->getHeader()->keySize );
					
					leftEntry->nKeys++;
					lHalf++;
				} else
				{
					ptr1 = rightEntry->getData() + keyLen * (ftxConn->stack.back().pos - lHalf);
					
					memmove ( ptr1 + keyLen, ptr1, (rHalf - (ftxConn->stack.back().pos - lHalf) + 1) * keyLen );
					memcpy ( ptr1, dep, ftx->getHeader()->depSize );
					memcpy ( ptr1 + ftx->getHeader()->depSize, key, ftx->getHeader()->keySize );

					rightEntry->nKeys++;
				}
				
				*((unsigned long *)(keyStart                          )) = leftPage;
				*((unsigned long *)(keyStart + ftx->getHeader()->keySize + sizeof ( unsigned long ))) = rightPage;
				memcpy   (keyStart + sizeof ( unsigned long ), leftEntry->getData() + (static_cast<ptrdiff_t>(lHalf) - 1) * (static_cast<size_t>(ftx->getHeader()->keySize) + ftx->getHeader()->depSize) + ftx->getHeader()->depSize, ftx->getHeader()->keySize );
			}
			break;
		case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
			if ( ftxConn->stack.back().nKeys < ftx->getHeader()->nRootBranchKeys )
			{
				root = ftx->getRootBlock ( );
				keyStart = root->getData ( );
				keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
				nKeys    = root->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				memmove ( ptr1 + keyLen, ptr1, ((nKeys - ftxConn->stack.back().pos) * keyLen) + sizeof ( unsigned long ) );
				memcpy ( ptr1, dep, sizeof ( unsigned long ) );
				memcpy ( ptr1 + sizeof ( unsigned long ), key, ftx->getHeader()->keySize );
				
				root->nKeys++;
			} else
			{
				leftPage = ftxGetFreePage ( ftx, ftxConn );
				rightPage = ftxGetFreePage ( ftx, ftxConn );

				// need to get this after we get the free pages since any pointers may be invalidated by that operation
				root = ftx->getRootBlock ( );
				keyStart = root->getData ( );
				keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
				nKeys    = root->nKeys;
				
				auto leftEntry = ftx->getLeafBlock ( leftPage );
				auto rightEntry = ftx->getLeafBlock ( rightPage );

				leftEntry->type = FTX_BRANCH_TYPE;
				rightEntry->type = FTX_BRANCH_TYPE;

				leftEntry->nKeys	= (unsigned char)((lHalf = (unsigned char)((nKeys >> 1) + (nKeys & 1))) - 1);
				rightEntry->nKeys	= (unsigned char)((rHalf = (unsigned char)((nKeys >> 1) + 1          )) - 1);

				memcpy ( leftEntry->getData(),  keyStart, static_cast<size_t>(lHalf) * keyLen + sizeof ( unsigned long ) );
				memcpy ( rightEntry->getData(), keyStart + static_cast<ptrdiff_t>(lHalf) * keyLen, (static_cast<size_t>(rHalf) * keyLen) + sizeof ( unsigned long ) );
				
				root->type  = FTX_ROOT_TYPE | FTX_BRANCH_TYPE;
				root->nKeys = 1;
				
				if ( ftxConn->stack.back().pos <= lHalf )
				{
					ptr1 = leftEntry->getData() + keyLen * ftxConn->stack.back().pos;
					
					memmove ( ptr1 + keyLen, ptr1, (lHalf - ftxConn->stack.back().pos + 1) * keyLen );
					memcpy ( ptr1, dep, sizeof ( unsigned long ) );
					memcpy ( ptr1 + sizeof ( unsigned long ), key, ftx->getHeader()->keySize );
					
					leftEntry->nKeys++;
					lHalf++;
				} else
				{
					ptr1 = rightEntry->getData() + keyLen * (ftxConn->stack.back().pos - lHalf);
					
					memmove ( ptr1 + keyLen, ptr1, (rHalf - (ftxConn->stack.back().pos - lHalf) + 1) * keyLen );
					memcpy ( ptr1, dep, sizeof ( unsigned long ) );
					memcpy ( ptr1 + sizeof ( unsigned long ), key, ftx->getHeader()->keySize );
					
					rightEntry->nKeys++;
				}
				*((unsigned long *)(keyStart													)) = leftPage;
				*((unsigned long *)(keyStart + sizeof ( unsigned long ) + ftx->getHeader()->keySize)) = rightPage;
				memcpy    (keyStart + sizeof ( unsigned long ), leftEntry->getData() + (static_cast<ptrdiff_t>(lHalf) - 1) * keyLen + sizeof ( unsigned long ), ftx->getHeader()->keySize );
			}
			break;
		case FTX_LEAF_TYPE:
			if ( ftxConn->stack.back().nKeys < ftx->getHeader()->nLeafKeys )
			{
				leaf = ftx->getLeafBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = leaf->getData ( );
				keyLen   = ftx->getHeader()->keySize + ftx->getHeader()->depSize;
				nKeys    = leaf->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				memmove ( ptr1 + keyLen, ptr1, (nKeys - ftxConn->stack.back().pos) * keyLen );
				memcpy ( ptr1, dep, ftx->getHeader()->depSize );
				memcpy ( ptr1 + ftx->getHeader()->depSize, key, ftx->getHeader()->keySize );
				
				leaf->nKeys++;
			} else
			{
				leftPage = ftxGetFreePage ( ftx, ftxConn );
				rightPage = ftxGetFreePage ( ftx, ftxConn );

				leaf = ftx->getLeafBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = leaf->getData ( );
				keyLen   = ftx->getHeader()->keySize + ftx->getHeader()->depSize;
				nKeys    = leaf->nKeys;
				
				auto leftEntry = ftx->getLeafBlock ( leftPage );
				auto rightEntry = ftx->getLeafBlock ( rightPage );

				leftEntry->type = FTX_LEAF_TYPE;
				rightEntry->type = FTX_LEAF_TYPE;

				leftEntry->nKeys	= lHalf = (unsigned char) (nKeys >> 1);
				rightEntry->nKeys	= rHalf = (unsigned char)((nKeys >> 1) + (nKeys & 1));
				
				memcpy  ( leftEntry->getData(), keyStart, static_cast<size_t>(lHalf) * keyLen );
				memmove ( rightEntry->getData(), keyStart + static_cast<ptrdiff_t>(lHalf) * keyLen, static_cast<size_t>(rHalf) * keyLen );
				
				if ( ftxConn->stack.back().pos <= lHalf )
				{
					ptr1 = leftEntry->getData() + keyLen * ftxConn->stack.back().pos;
					
					memmove ( ptr1 + keyLen, ptr1, (lHalf - ftxConn->stack.back().pos + 1) * keyLen );
					memcpy ( ptr1, dep, ftx->getHeader()->depSize );
					memcpy ( ptr1 + ftx->getHeader()->depSize, key, ftx->getHeader()->keySize );
					
					leftEntry->nKeys++;
					lHalf++;
				} else
				{
					ptr1 = rightEntry->getData() + keyLen * (ftxConn->stack.back().pos - lHalf);
					
					memmove ( ptr1 + keyLen, ptr1, (rHalf - (ftxConn->stack.back().pos - lHalf) + 1) * keyLen );
					memcpy ( ptr1, dep, ftx->getHeader()->depSize );
					memcpy ( ptr1 + ftx->getHeader()->depSize, key, ftx->getHeader()->keySize );
					
					rightEntry->nKeys++;
				}

				ftxConn->stack.pop_back ( );

				memcpy ( tmpKey, leftEntry->getData() + (static_cast<ptrdiff_t>(lHalf) - 1) * keyLen + ftx->getHeader()->depSize, keyLen );
				ftxKeyAdd ( ftx, ftxConn, tmpKey, (char *)(&leftPage) );
			}
			break;
		case FTX_BRANCH_TYPE:
			if ( ftxConn->stack.back().nKeys < ftx->getHeader()->nLeafKeys )
			{
				branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = branch->getData ( );
				keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
				nKeys    = branch->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				memmove ( ptr1 + keyLen, ptr1, ((nKeys - ftxConn->stack.back().pos) * keyLen) + sizeof ( unsigned long ) );
				memcpy ( ptr1, dep, sizeof ( unsigned long ) );
				memcpy ( ptr1 + sizeof ( unsigned long ), key, ftx->getHeader()->keySize );
				
				branch->nKeys++;
			} else
			{
				leftPage = ftxGetFreePage ( ftx, ftxConn );
				rightPage = ftxGetFreePage ( ftx, ftxConn );

				branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = branch->getData ( );
				keyLen    = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
				nKeys     = branch->nKeys;
				
				auto leftEntry = ftx->getLeafBlock ( leftPage );
				auto rightEntry = ftx->getLeafBlock ( rightPage );

				leftEntry->type      = FTX_BRANCH_TYPE;
				rightEntry->type      = FTX_BRANCH_TYPE;
				
				leftEntry->nKeys     = (unsigned char)((lHalf = (unsigned char)((nKeys >> 1) + (nKeys & 1))) - 1);
				rightEntry->nKeys     = (unsigned char)((rHalf = (unsigned char)((nKeys >> 1) + 1          )) - 1);
				
				memcpy  ( leftEntry->getData(), keyStart, static_cast<size_t>(lHalf) * keyLen + sizeof ( unsigned long ) );
				memmove ( rightEntry->getData(), keyStart + static_cast<ptrdiff_t>(lHalf) * keyLen, static_cast<size_t>(rHalf) * keyLen + sizeof ( unsigned long ));
				
				if ( ftxConn->stack.back().pos <= lHalf )
				{
					ptr1 = leftEntry->getData() + keyLen * ftxConn->stack.back().pos;
					
					memmove ( ptr1 + keyLen, ptr1, ((lHalf - ftxConn->stack.back().pos) * keyLen) + sizeof ( unsigned long ) );
					memcpy ( ptr1, dep, sizeof ( unsigned long ) );
					memcpy ( ptr1 + sizeof ( unsigned long ), key, ftx->getHeader()->keySize );
					
					leftEntry->nKeys++;
					lHalf++;
				} else
				{
					ptr1 = rightEntry->getData() + keyLen * (ftxConn->stack.back().pos - lHalf);
					
					memmove ( ptr1 + keyLen, ptr1, ((rHalf - (ftxConn->stack.back().pos - lHalf) + 1) * keyLen) + sizeof ( unsigned long ) );
					memcpy ( ptr1, dep, sizeof ( unsigned long ) );
					memcpy ( ptr1 + sizeof ( unsigned long ), key, ftx->getHeader()->keySize );
					
					rightEntry->nKeys++;
				}
				ftxConn->stack.pop_back ( );

				memcpy ( tmpKey, leftEntry->getData() + (static_cast<ptrdiff_t>(lHalf) - 1) * keyLen + sizeof ( unsigned long ), keyLen );
				ftxKeyAdd ( ftx, ftxConn, tmpKey, (char *)(&leftPage) );
			}
			break;
	}
	return ( 1 );
}

static char *ftxSkipForward ( FTX *ftx, FTX_CONNECTION *ftxConn )  /* must assure on leaf node before call */

{
	FTX_BRANCH				*branch;
	FTX_LEAF				*leaf;
	FTX_ROOT				*root;
	char					*keyStart;
	unsigned long			 nPage;
	unsigned char		     nKeys;
	int						 keyLen;
	
	if ( (int) ftxConn->stack.back().pos == ftxConn->stack.back().nKeys - 1 )
	{

		ftxConn->stack.pop_back ( );

		while ( ftxConn->stack.size() && ((int) ftxConn->stack.back().pos == ftxConn->stack.back().nKeys) )
		{
			ftxConn->stack.pop_back ( );
		}
		if ( !ftxConn->stack.size() )
		{
			return nullptr;
		}
		
		ftxConn->stack.back().pos++;
		
		char blockType;
		blockType = ftx->getBlockType ( ftxConn->stack.back ( ).nPage );
		do {
			if ( blockType & FTX_ROOT_TYPE )
			{
				root = ftx->getRootBlock ( );
				keyStart = root->getData ( );
				keyLen = static_cast<int>(ftx->getHeader ( )->keySize + sizeof ( unsigned long ));
			} else
			{
				branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = branch->getData ( );
				keyLen = static_cast<int>(ftx->getHeader ( )->keySize + sizeof ( unsigned long ));
			}
			
			nPage    = *((long *)(keyStart + keyLen * ftxConn->stack.back().pos));

			blockType = ftx->getBlockType ( nPage );
			if ( blockType & FTX_BRANCH_TYPE )
			{
				branch = ftx->getBranchBlock ( nPage );
				nKeys = branch->nKeys;
			} else
			{
				leaf = ftx->getLeafBlock ( nPage );
				nKeys    = leaf->nKeys;
			}

			ftxConn->stack.push_back ( FTX_STACK{ nPage, nKeys, 0 } );
		} while ( !(blockType & FTX_LEAF_TYPE) );
	} else if ( (int) ftxConn->stack.back().pos > ftxConn->stack.back().nKeys - 1 )
	{
		return nullptr;
	} else
	{
		ftxConn->stack.back().pos++;
	}

	switch ( ftx->getBlockType ( ftxConn->stack.back ( ).nPage ) )
	{
		case FTX_LEAF_TYPE:
			leaf     = ftx->getLeafBlock ( ftxConn->stack.back().nPage );
			keyStart = leaf->getData();
			keyLen = static_cast<int>(ftx->getHeader()->keySize + ftx->getHeader()->depSize);

			keyStart = keyStart + keyLen * ftxConn->stack.back ( ).pos;

			memcpy ( ftxConn->knownKey, keyStart + ftx->getHeader()->depSize, ftx->getHeader()->keySize );
			memcpy ( ftxConn->knownDep, keyStart, ftx->getHeader()->depSize );
			break;
		case FTX_LEAF_TYPE | FTX_ROOT_TYPE:
			root = ftx->getRootBlock ( );
			keyStart = root->getData ( );
			keyLen = static_cast<int>(ftx->getHeader()->keySize + ftx->getHeader()->depSize);

			keyStart = keyStart + keyLen * ftxConn->stack.back ( ).pos;

			memcpy ( ftxConn->knownKey, keyStart + ftx->getHeader()->depSize, ftx->getHeader()->keySize );
			memcpy ( ftxConn->knownDep, keyStart, ftx->getHeader()->depSize );
			break;
		default:
			return nullptr;
	}
	return ftxConn->knownDep;
}

static void *ftxSkipBackward ( FTX *ftx, FTX_CONNECTION *ftxConn ) /* must assure on leaf node before call */
{
	FTX_BRANCH				*branch;
	FTX_LEAF				*leaf;
	FTX_ROOT				*root;
	char					*keyStart;
	unsigned long			 nPage;
	unsigned char		     nKeys;
	int						 keyLen;
	int						 pos;
	
	if ( (int) ftxConn->stack.back().pos == 0 )
	{
		ftxConn->stack.pop_back ( );
		
		while ( ftxConn->stack.size() && ((int) ftxConn->stack.back().pos == 0) )
		{
			ftxConn->stack.pop_back ( );
		}
		if ( !ftxConn->stack.size() )
		{
			return ( 0 );
		}
		
		ftxConn->stack.back().pos--;
		
		char blockType;
		blockType = ftx->getBlockType ( ftxConn->stack.back ( ).nPage );
		do {
			if ( blockType & FTX_ROOT_TYPE )
			{
				root = ftx->getRootBlock ( );
				keyStart = root->getData();
				keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
			} else
			{
				branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = branch->getData();
				keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
			}
			
			nPage    = *((long *)(keyStart + keyLen * ftxConn->stack.back().pos));
			
			blockType = ftx->getBlockType ( nPage );
			switch ( blockType )
			{
				case FTX_BRANCH_TYPE:
					branch = ftx->getBranchBlock ( nPage );
					nKeys    = branch->nKeys;
					pos      = nKeys;
					break;
				case FTX_LEAF_TYPE:
					leaf = ftx->getLeafBlock ( nPage );
					nKeys    = leaf->nKeys;
					pos      = (unsigned char)(nKeys - 1);
					break;
				default:
					return nullptr;
			}
				
			ftxConn->stack.push_back ( FTX_STACK{ nPage, nKeys, static_cast<uint64_t>(pos) } );
		} while ( !(blockType & FTX_LEAF_TYPE) );
	} else
	{
		ftxConn->stack.back().pos--;
	}

	switch ( ftx->getBlockType ( ftxConn->stack.back ( ).nPage ) )
	{
		case FTX_LEAF_TYPE:
			leaf = ftx->getLeafBlock ( ftxConn->stack.back ( ).nPage );
			keyStart = leaf->getData ( );
			keyLen   = ftx->getHeader()->keySize + ftx->getHeader()->depSize;

			keyStart = keyStart + keyLen * ftxConn->stack.back().pos;

			memcpy ( ftxConn->knownKey, keyStart + ftx->getHeader()->depSize, ftx->getHeader()->keySize );
			memcpy ( ftxConn->knownDep, keyStart,  ftx->getHeader()->depSize );
			break;
		case FTX_LEAF_TYPE | FTX_ROOT_TYPE:
			branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
			keyStart = branch->getData ( );
			keyLen   = ftx->getHeader()->keySize + ftx->getHeader()->depSize;

			keyStart = keyStart + keyLen * ftxConn->stack.back().pos;

			memcpy ( ftxConn->knownKey, keyStart + ftx->getHeader()->depSize, ftx->getHeader()->keySize );
			memcpy ( ftxConn->knownDep, keyStart,  ftx->getHeader()->depSize );
			break;
		default:
			return nullptr;
	}
	return ftxConn->knownDep;
}

void *_ftxSkip ( FTX *ftx, FTX_CONNECTION *ftxConn, int64_t num )
{
	char	tmpKey[1024];
	char	tmpDep[1024];	

	SRRWLocker l1 ( ftx->headerLock, false );

	if ( !num )
	{
		return ftxConn->knownDep;
	}
		
	if ( !ftxConn->stack.size() )
	{
		memcpy ( tmpKey, ftxConn->knownKey, ftx->getHeader()->keySize );
		memcpy ( tmpDep, ftxConn->knownDep, ftx->getHeader()->depSize );
			
		if ( !ftxLocate ( ftx, ftxConn, 1, ftxConn->knownKey, ftx->getHeader()->keySize, LEFT_FOUND ) )
		{
			return 0;
		}
			
		while ( (ftx->cmpRtn) ( tmpDep, ftxConn->knownDep, ftx->getHeader()->depSize ) )
		{
			if ( !ftxSkipForward ( ftx, ftxConn ) )
			{
				return 0;
			}
			if ( (ftx->cmpRtn) ( ftxConn->knownKey, tmpKey, ftx->getHeader()->keySize ) )
			{
				return 0;
			}
		}
	}
		
	if ( num > 0 )
	{
		while ( num )
		{
			if ( !(ftxSkipForward ( ftx, ftxConn )) )
			{
				return 0;
			}
			num--;
		}
	} else if ( num < 0 )
	{
		while ( num )
		{
			if ( !(ftxSkipBackward ( ftx, ftxConn )) )
			{
				return 0;
			}
			num++;
		}
	}
	
	return ftxConn->knownDep;
}

bool _ftxGoto ( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, void const *dep )
{
	SRRWLocker l1 ( ftx->headerLock, false );

	ftxConn->stack.clear ( );
		
	if ( !ftxLocate ( ftx, ftxConn, 1, (char *)key, ftx->getHeader()->keySize, LEFT_FOUND ) )
	{
		return false;
	}
		
	while ( (ftx->cmpRtn) ( (char *)dep, ftxConn->knownDep, ftx->getHeader()->depSize ) )
	{
		if ( !ftxSkipForward ( ftx, ftxConn ) )
		{
			return false;
		}
		if ( (ftx->cmpRtn) ((char *) key, ftxConn->knownKey, ftx->getHeader()->keySize ) )
		{
			return false;
		}
	}
	return true;
}

static void ftxKeyDelete ( FTX *ftx, FTX_CONNECTION *ftxConn ) /* must assure `on leaf node before call */

{
	FTX_BRANCH				*branch;
	FTX_ROOT				*root;
	FTX_LEAF				*leaf;
	char					*keyStart;
	char					*ptr1;
	unsigned char			 nKeys;
	int						 keyLen;
	char					 tmpKey[1024];

	if ( !ftxConn->stack.back().nKeys )
	{
		return;
	}

	ftx->lastUpdate++;
	
	if ( ftxConn->stack.back().nKeys == 1 )
	{
		if ( ftxConn->stack.size() != 1 )
		{
			ftxConn->stack.pop_back ( );
			ftxFreePage ( ftx, ftxConn, ftxConn->stack.back().nPage );
		} else
		{
			root = ftx->getRootBlock ( );
			root->type  = FTX_ROOT_TYPE | FTX_LEAF_TYPE;
			root->nKeys = 0;
			return;
		}
		
		while ( ftxConn->stack.size() && !ftxConn->stack.back().nKeys )
		{
			if ( ftxConn->stack.size() == 1 )
			{
				root = ftx->getRootBlock ( );
				root->type  = FTX_ROOT_TYPE | FTX_LEAF_TYPE;
				root->nKeys = 0;
				return;
			} else
			{
				ftxConn->stack.pop_back ( );
				ftxFreePage ( ftx, ftxConn, ftxConn->stack.back().nPage );
			}
		}
		
		switch ( ftx->getBlockType ( ftxConn->stack.back().nPage ) )
		{
			case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
				root     = ftx->getRootBlock();
				keyStart = root->getData();
				keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
				
				root->nKeys--;
				nKeys    = root->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				memmove ( ptr1, ptr1 + keyLen, ((nKeys - ftxConn->stack.back().pos + 1) * keyLen) + sizeof ( unsigned long ) );
				break;
			case FTX_BRANCH_TYPE:
				branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = branch->getData();
				keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
				
				branch->nKeys--;
				nKeys    = branch->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				if ( nKeys >= ftxConn->stack.back().pos )
				{
					memmove ( ptr1, ptr1 + keyLen, ((nKeys - ftxConn->stack.back().pos + 1) * keyLen) + sizeof ( unsigned long ) );
				} else
				{
				}
	            break;
			default:
				throw errorNum::scINTERNAL;
		}
		if ( ftxConn->stack.back().pos == (unsigned char)(ftxConn->stack.back().nKeys) && (ftxConn->stack.size() > 1) )
		{
			// get new maximum key
			ptr1 = keyStart + static_cast<ptrdiff_t>(nKeys) * keyLen + sizeof ( unsigned long );
			memcpy ( tmpKey, ptr1, ftx->getHeader()->keySize );
			
			while ( ftxConn->stack.size() > 1 )
			{
				// move up to our parent and see if we need to update
				ftxConn->stack.pop_back();

				// if our new maximum has been changed we need to modify it in the parent block
				if ( ftxConn->stack.back().pos != (unsigned char)ftxConn->stack.back().nKeys )
				{
					// we've found a non-max value... update it and break out of the loop, we're done
					switch ( ftx->getBlockType ( ftxConn->stack.back().nPage ) )
					{
						case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
							root = ftx->getRootBlock ( );
							keyStart = root->getData();
							keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
							
							ptr1     = keyStart + ftxConn->stack.back().pos * keyLen + sizeof ( unsigned long );
							
							memmove ( ptr1, tmpKey, ftx->getHeader()->keySize );
							break;
						case FTX_BRANCH_TYPE:
							branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
							keyStart = branch->getData();
							keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
							
							ptr1     = keyStart + ftxConn->stack.back().pos * keyLen + sizeof ( unsigned long );
							
							memmove ( ptr1, tmpKey, ftx->getHeader()->keySize );
							break;
					}
					break;
				} 
			}// while ( ftxConn->stack.size() && (ftxConn->stack.back().pos == (unsigned char)ftxConn->stack.back().nKeys) );
		}
	} else
	{
		switch ( ftx->getBlockType ( ftxConn->stack.back().nPage ) )
		{
			case FTX_ROOT_TYPE | FTX_LEAF_TYPE:
				root = ftx->getRootBlock ( );
				keyStart = root->getData();
				keyLen   = ftx->getHeader()->keySize + ftx->getHeader()->depSize;
				
				root->nKeys--;
				nKeys    = root->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				memmove ( ptr1, ptr1 + keyLen, (nKeys - ftxConn->stack.back().pos + 1) * keyLen );
				break;
			case FTX_LEAF_TYPE:
				leaf = ftx->getLeafBlock ( ftxConn->stack.back ( ).nPage );
				keyStart = leaf->getData();
				keyLen   = ftx->getHeader()->keySize + ftx->getHeader()->depSize;
				
				leaf->nKeys--;
				nKeys    = leaf->nKeys;
				
				ptr1     = keyStart + ftxConn->stack.back().pos * keyLen;
				
				if ( ftxConn->stack.back().pos != leaf->nKeys )
				{
					// not last one so have to move the rest down
					memmove ( ptr1, ptr1 + keyLen, (nKeys - ftxConn->stack.back().pos + 1) * keyLen );
				}
				break;
			default:
				throw errorNum::scINTERNAL;
		}
		
		// if it's the MAX leaf key then we need to modify the parents to reflect the new max key value
		if ( ftxConn->stack.back().pos == (unsigned char)(ftxConn->stack.back().nKeys - 1) && (ftxConn->stack.size() > 1) )	// pos is 0 based, nKeys is not
		{
			// get our new max value key and save it
			ptr1 = keyStart + (static_cast<ptrdiff_t>(nKeys) - 1) * keyLen + ftx->getHeader()->depSize;
			memcpy ( tmpKey, ptr1, ftx->getHeader()->keySize );
			
			
			while ( ftxConn->stack.size() > 1 )
			{
				// we only need to update the value if we're NOT the maximum pointer (max ptr has no KEY)
				// if we aren't the maximum pointer we then exit out of the loop... no need to update any additional parents.
				// if we ARE the maximum we have to continue up the parentage to see if we ever encounter a parent where we
				// are not the maximum and update it to reflet the new max.

				// lets move up and check the parent
				ftxConn->stack.pop_back();

				if ( ftxConn->stack.back().pos != (unsigned char)ftxConn->stack.back().nKeys )
				{
					// we've found a non-max value... update it and break out of the loop, we're done
					switch ( ftx->getBlockType ( ftxConn->stack.back().nPage ) )
					{
						case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
							root     = ftx->getRootBlock();
							keyStart = root->getData();
							keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
							
							ptr1     = keyStart + ftxConn->stack.back().pos * keyLen + sizeof ( unsigned long );
							
							memmove ( ptr1, tmpKey, ftx->getHeader()->keySize );
							break;
						case FTX_BRANCH_TYPE:
							branch = ftx->getBranchBlock ( ftxConn->stack.back ( ).nPage );
							keyStart = branch->getData();
							keyLen   = static_cast<int>(ftx->getHeader()->keySize + sizeof ( unsigned long ));
							
							ptr1     = keyStart + ftxConn->stack.back().pos * keyLen + sizeof ( unsigned long );
							
							memmove ( ptr1, tmpKey, ftx->getHeader()->keySize );
							break;
					}
					break;
				}
			}
		}
	}
	ftxConn->stack.clear ( );
}

bool _ftxIsTop ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	size_t	loop;
	char	tmpKey[1024];
	char	tmpDep[1024];	

	SRRWLocker l1 ( ftx->headerLock, false );

	if ( !ftxConn->stack.size() )
	{
		memcpy ( tmpKey, ftxConn->knownKey, ftx->getHeader()->keySize );
		memcpy ( tmpDep, ftxConn->knownDep, ftx->getHeader()->depSize );
			
		ftxConn->stack.clear ( );
			
		if ( !ftxLocate ( ftx, ftxConn, 1, ftxConn->knownKey, ftx->getHeader()->keySize, LEFT_FOUND ) )
		{
			return false;
		}
			
		while ( (ftx->cmpRtn) ( tmpDep, ftxConn->knownDep, ftx->getHeader()->depSize ) )
		{
			if ( !ftxSkipForward ( ftx, ftxConn ) )
			{
				return false;
			}
			if ( (ftx->cmpRtn) ( ftxConn->knownKey, tmpKey, ftx->getHeader()->keySize ) )
			{
				return false;
			}
		}
	}
		
	loop = ftxConn->stack.size() - 1;
		
	if ( ftxConn->stack[loop].pos != 0 )
	{
		return false;
	}
		
	while ( --loop )
	{
		if ( ftxConn->stack[loop].pos != 0 )
		{
			return false;
		}
	}
	return true;
}

bool _ftxIsBottom ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	size_t	loop;
	char	tmpKey[1024];
	char	tmpDep[1024];	

	SRRWLocker l1 ( ftx->headerLock, false );
	
	if ( !ftxConn->stack.size() )
	{
		memcpy ( tmpKey, ftxConn->knownKey, ftx->getHeader()->keySize );
		memcpy ( tmpDep, ftxConn->knownDep, ftx->getHeader()->depSize );
			
		ftxConn->stack.clear ( );
			
		if ( !ftxLocate ( ftx, ftxConn, 1, ftxConn->knownKey, ftx->getHeader()->keySize, LEFT_FOUND ) )
		{
			return false;
		}
			
		while ( (ftx->cmpRtn) ( tmpDep, ftxConn->knownDep, ftx->getHeader()->depSize ) )
		{
			if ( !ftxSkipForward ( ftx, ftxConn ) )
			{
				return false;
			}
			if ( (ftx->cmpRtn) ( ftxConn->knownKey, tmpKey, ftx->getHeader()->keySize ) )
			{
				return false;
			}
		}
	}
		
	loop = ftxConn->stack.size() - 1;
		
	if ( ftxConn->stack[loop].pos != (unsigned char) (ftxConn->stack[loop].nKeys - 1) )
	{
		return false;
	}
		
	while ( --loop )
	{
		if ( ftxConn->stack[loop].pos != (unsigned char)(ftxConn->stack[loop].nKeys) )
		{
			return false;
		}
	}
	return true;
}

bool _ftxKeyDelete ( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, void const *dep )
{
	SRRWLocker l1 ( ftx->headerLock, true );

	ftxConn->stack.clear();
		
	if ( !ftxLocate ( ftx, ftxConn, 1, (char  const *)key, ftx->getHeader()->keySize, LEFT_FOUND ) )
	{
		ftxConn->stack.clear ( );
		return false;
	}
		
	do {
		if ( !(ftx->cmpRtn) ( ftxConn->knownDep, (char const *)dep, ftx->getHeader()->depSize ) )
		{
			ftxKeyDelete ( ftx, ftxConn );
				
			return true;
		}
		if ( !ftxSkipForward ( ftx, ftxConn ) )
		{
			return false;
		}
	} while ( !(ftx->cmpRtn) ( ftxConn->knownKey, (char const *)key, ftx->getHeader()->keySize ) );
		
	ftxConn->stack.clear ( );
	return true;
}

bool _ftxKeyAdd ( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, void const *dep )
{
	SRRWLocker l1 ( ftx->headerLock, true );

	ftxConn->stack.clear ( );

	if ( !ftxLocate ( ftx, ftxConn, 1, (char const *)key, ftx->getHeader()->keySize, LEFT ) )
	{
		return false;
	}
		
	ftxKeyAdd ( ftx, ftxConn, (char const *) key, (char const *) dep );
		
	memcpy ( ftxConn->knownKey, key, ftx->getHeader()->keySize );
	memcpy ( ftxConn->knownDep, dep, ftx->getHeader()->depSize );

	return true;
}

void *_ftxSeek ( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, size_t len )
{
	char *ptr;

	SRRWLocker l1 ( ftx->headerLock, false );

	len = (len > (int) ftx->getHeader()->keySize ? ftx->getHeader()->keySize : len );
		
	ftxConn->stack.clear ( );

	if ( ftxLocate ( ftx, ftxConn, 1, (char *)key, len, LEFT_FOUND ) )
	{
		ptr = ftxConn->knownDep;
	} else
	{
		ptr = 0;
	}
	return ptr;
}

void *_ftxSeekLast ( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, size_t len )
{
	char *ptr;

	SRRWLocker l1 ( ftx->headerLock, false );

	len = (len > (int)ftx->getHeader ()->keySize ? ftx->getHeader ()->keySize : len);

	ftxConn->stack.clear ();

	if ( ftxLocate ( ftx, ftxConn, 1, (char *)key, len, RIGHT_FOUND ) )
	{
		ptr = ftxConn->knownDep;
	} else
	{
		ptr = 0;
	}
	return ptr;
}

void *_ftxGoTop ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	char *ptr;
	
	SRRWLocker l1 ( ftx->headerLock, false );

	ftxConn->stack.clear ( );
		
	if ( ftxLocate ( ftx, ftxConn, 1, 0L, 0, TOP ) )
	{
		ptr = ftxConn->knownDep;
	} else
	{
		ptr = 0;
	}
	return ptr;
}

void *_ftxGoBottom ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	char *ptr;

	SRRWLocker l1 ( ftx->headerLock, false );

	ftxConn->stack.clear ( );
		
	if ( ftxLocate ( ftx, ftxConn, 1, 0L, 0, BOTTOM ) )
	{
		ptr = ftxConn->knownDep;
	} else
	{
		ptr = 0;
	}
	return ptr;
}

size_t _ftxFindSubset ( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, size_t len, size_t every, void **subSet )
{
	size_t		 nEntries;
	size_t		 entryNum;
	size_t		 nRecs;

	SRRWLocker l1 ( ftx->headerLock, false );

	len = (len > (int) ftx->getHeader()->keySize ? ftx->getHeader()->keySize : len );

	*subSet = 0;
	entryNum = 0;
	nRecs = 1;

	ftxConn->stack.clear ( );
		
	if ( ftxLocate ( ftx, ftxConn, 1, (char *)key, len, LEFT_FOUND ) )
	{
		nEntries = 32;
		if ( !(*subSet = malloc ( (sizeof ( char ) * ftx->getHeader()->depSize) * nEntries )) )
		{
			return 0;
		}

		memcpy ( ((char *)*subSet + entryNum * ftx->getHeader()->depSize), ftxConn->knownDep, ftx->getHeader()->depSize );

		entryNum++;
		while ( 1 )
		{
			if ( !ftxSkipForward ( ftx, ftxConn ) )
			{
				/* reached end of file */
				if ( entryNum == nEntries )
				{
					nEntries += 1;
					if ( !(*subSet = realloc ( *subSet, (sizeof ( char ) * ftx->getHeader()->depSize) * nEntries )) )
					{
						return 0;
					}
				}
				memcpy ( ((char *)*subSet + entryNum * ftx->getHeader()->depSize), &nRecs, sizeof ( nRecs ));
				entryNum++;
				/* no more matches */
				return entryNum;
			}
			if ( (ftx->cmpRtn) ( (char *)key, ftxConn->knownKey, len ) )
			{
				if ( entryNum == nEntries )
				{
					nEntries += 1;
					if ( !(*subSet = realloc ( *subSet, (sizeof ( char ) * ftx->getHeader()->depSize) * nEntries )) )
					{
						return 0;
					}
				}
				memcpy ( ((char *)*subSet + entryNum * ftx->getHeader()->depSize), &nRecs, sizeof ( nRecs ));
				entryNum++;
				/* no more matches */
				return entryNum;
			}
			/* store only every EVERY'th record */
			if ( !(nRecs % every) )
			{
				if ( entryNum == nEntries )
				{
					nEntries += 1024;
					if ( !(*subSet = realloc ( *subSet, (sizeof ( char ) * ftx->getHeader()->depSize) * nEntries )) )
					{
						return entryNum;
					}
				}
				memcpy ( ((char *)*subSet + entryNum * ftx->getHeader()->depSize), ftxConn->knownDep, ftx->getHeader()->depSize );
				entryNum++;
			}
			nRecs++;
		}
	} else
	{
		return 0;
	}	
}

size_t _ftxCountMatching ( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, size_t len )
{
	size_t nRecs;
	
	SRRWLocker l1 ( ftx->headerLock, false );

	len = (len > (int) ftx->getHeader()->keySize ? ftx->getHeader()->keySize : len );

	nRecs = 1;
		
	ftxConn->stack.clear ( );
		
	if ( ftxLocate ( ftx, ftxConn, 1, (char *)key, len, LEFT_FOUND ) )
	{
		while ( 1 )
		{
			if ( !ftxSkipForward ( ftx, ftxConn ) )
			{
				/* reached end of file */
				return nRecs;
			}
			if ( (ftx->cmpRtn) ((char *) key, ftxConn->knownKey, len ) )
			{
				return nRecs;
			}
			nRecs++;
		}
	} else
	{
		return 0;
	}	
}

void _ftxDelete ( FTX *ftx )
{
	ftx->unMapFile ( );
	CloseHandle ( ftx->fileHandle );
	DeleteFile ( ftx->fName.c_str() );
	delete ftx;
}

FTX _ftxCreate ( char const *name, char const *keyExpr, size_t mode, size_t keySize, size_t depSize, bool isDescending, char exprType, char const *filterExpr, uint32_t updateCount )
{
	FTX	ftx;

	if ( keyExpr && (strlen ( keyExpr ) > 220) )
	{
		throw errorNum::scINVALID_KEY;
	}

	if ( (keySize > 1024) || (depSize > 1024) )
	{
		throw errorNum::scINVALID_KEY;
	}
	
	if ( (ftx.fileHandle = CreateFile ( name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		throw (errorNum) GetLastError ( );
	}
	ftx.fileSize = 2ull * FTX_PAGE_SIZE;	 /* one for root */
	ftx.mapFile ( );					 /* this will expand up to fileSize */

	ftx.getHeader()->keySize         = (uint16_t) keySize;
	ftx.getHeader()->depSize         = (uint16_t) depSize;
	ftx.getHeader()->nRootBranchKeys = (unsigned short)((FTX_PAGE_SIZE - sizeof (FTX_ROOT  ) - sizeof ( unsigned long )) / ( keySize + sizeof ( unsigned long ) ));
	ftx.getHeader()->nRootLeafKeys   = (unsigned short) ((FTX_PAGE_SIZE - sizeof (FTX_ROOT  )							) / ( keySize + depSize					 ));
	ftx.getHeader()->nBranchKeys     = (unsigned short) ((FTX_PAGE_SIZE - sizeof (FTX_BRANCH) - sizeof ( unsigned long )) / ( keySize + sizeof ( unsigned long ) ));
	ftx.getHeader()->nLeafKeys       = (unsigned short) ((FTX_PAGE_SIZE - sizeof (FTX_LEAF)								) / ( keySize + depSize					 ));
	ftx.getHeader()->isDescending    = static_cast<char>(isDescending);
	ftx.getHeader()->exprType        = exprType;

	ftx.lastUpdate				= 1;

	memset ( ftx.getHeader()->keyExpr, 0, 220  );
	
	if ( keyExpr )
	{
		strcpy_s ( ftx.getHeader()->keyExpr, sizeof ( ftx.getHeader()->keyExpr ), keyExpr );
	}

	if ( filterExpr )
	{
		ftx.getHeader()->isFilter = 1;
		strcpy_s (ftx.getHeader()->filterExpr, sizeof ( ftx.getHeader()->filterExpr ), filterExpr );
	}
	
	switch ( ftx.getHeader()->exprType )
	{
		case FTX_CHAR_EXPR:
			ftx.cmpRtn = memcmp;
			break;
		case FTX_LONG_EXPR:
			ftx.cmpRtn = ftxCmpLong;
			break;
		case FTX_DOUBLE_EXPR:
			ftx.cmpRtn = ftxCmpDouble;
			break;
		default:
			ftx.cmpRtn = memcmp;
			break;
	}
	
	ftx.fName = name;

	ftx.getRootBlock ( )->type			= FTX_ROOT_TYPE | FTX_LEAF_TYPE;
	ftx.getRootBlock ( )->free			= 0;
	ftx.getRootBlock ( )->nBlocks		= 2;
	ftx.getRootBlock ( )->nKeys			= 0;
//	ftx.getRootBlock ( )->updateCount	= updateCount;
	
	// for disk-sorted reindex
	ftx.nKeys = 0;
	ftx.maxPage = 2;	

	return ftx;
}

FTX _ftxOpen ( char const *name, int mode, uint32_t updateCount )
{
	FTX	ftx;

	if ( (ftx.fileHandle = CreateFile ( name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		throw (errorNum)GetLastError ( );
	}

	BY_HANDLE_FILE_INFORMATION	 fileInfo;
	GetFileInformationByHandle ( ftx.fileHandle, &fileInfo );
	ftx.fileSize = ((uint64_t) fileInfo.nFileSizeHigh << 32) + fileInfo.nFileSizeLow;

	ftx.mapFile ( );

	ftx.lastUpdate		= 1;

#if 0
	if ( ftx.getRootBlock ()->updateCount != updateCount )
	{
		throw errorNum::scDATABASE_INTEGRITY;
	}
#endif

	switch ( ftx.getHeader()->exprType )
	{
		case FTX_CHAR_EXPR:
			ftx.cmpRtn = memcmp;
			break;
		case FTX_LONG_EXPR:
			ftx.cmpRtn = ftxCmpLong;
			break;
		case FTX_DOUBLE_EXPR:
			ftx.cmpRtn = ftxCmpDouble;
			break;
		default:
			ftx.cmpRtn = memcmp;
			break;
	}
	
	ftx.fName = name;

	return ftx;
}

char *_ftxGetExpr ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	return ftx->getHeader()->keyExpr;
}

char *_ftxGetFilter( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	if ( ftx->getHeader()->isFilter )
	{
		return ( ftx->getHeader()->filterExpr );
	} else
	{
		return ( 0 );
	}
}

void _ftxClear ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	SRRWLocker l1 ( ftx->headerLock, true );

	ftx->unMapFile ( );

	SetFilePointer ( ftx->fileHandle, FTX_PAGE_SIZE * 2, 0, FILE_BEGIN );
	SetEndOfFile ( ftx->fileHandle );
	ftx->fileSize = 2ull * FTX_PAGE_SIZE;
	ftx->nKeys = 0;

	ftx->mapFile ( );

	ftx->lastUpdate++;
	ftx->getRootBlock()->type       = FTX_ROOT_TYPE | FTX_LEAF_TYPE;
	ftx->getRootBlock ( )->free		= 0;
	ftx->getRootBlock ( )->nBlocks  = 2;
	ftx->getRootBlock ( )->nKeys    = 0;
}

void _ftxAdjustFileSize ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	size_t		 nBlocksRequired;

	// compute the actual number of blocks from the number of keys we actually added (may not have added deleted keys)
	if ( ftx->getHeader()->nLeafKeys < 4 )
	{
		// make sure to round up
		nBlocksRequired = (ftx->nKeys + (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1)) / ftx->getHeader()->nLeafKeys;
	} else
	{
		// make sure to round up
		nBlocksRequired = (ftx->nKeys  + (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 2)) / (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1);
	}

	ftx->fileSize = (2 + nBlocksRequired) * FTX_PAGE_SIZE;
#if 0
	ftx->unMapFile ( );
	LARGE_INTEGER addr;
	LARGE_INTEGER newAddr;
	addr.QuadPart = ftx->fileSize;
	SetFilePointerEx ( ftx->fileHandle, addr, &newAddr, FILE_BEGIN );
	SetEndOfFile ( ftx->fileHandle);
#endif
	ftx->mapFile ( );

	ftx->getRootBlock ( )->nBlocks = static_cast<unsigned long>(nBlocksRequired) + 2;
}

void _ftxAddKeys ( FTX *ftx, FTX_CONNECTION *ftxConn, int keyType, size_t nKeys )
{
	size_t		 nBlocksRequired;

	// add space for nKeys worth of additional branch keys

	switch ( keyType )
	{
		case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
		case FTX_ROOT_TYPE | FTX_LEAF_TYPE:
			// never add any root blocks... there can be only one
			return;
		case FTX_BRANCH_TYPE:
			if ( ftx->getHeader()->nBranchKeys < 4 )
			{
				// need to be dense
				nBlocksRequired = (nKeys  + (static_cast<size_t>(ftx->getHeader()->nBranchKeys) - 1)) / (ftx->getHeader()->nBranchKeys);
			} else
			{
				nBlocksRequired = (nKeys  + (static_cast<size_t>(ftx->getHeader()->nBranchKeys) - 2)) / (static_cast<size_t>(ftx->getHeader()->nBranchKeys) - 1);
			}
			break;
		case FTX_LEAF_TYPE:
			if ( ftx->getHeader()->nLeafKeys < 4 )
			{
				nBlocksRequired = (nKeys + (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1)) / ftx->getHeader()->nLeafKeys;
			} else
			{
				nBlocksRequired = (nKeys + (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 2)) / (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1);
			}
			break;
	}

	auto startBlock = ftx->getRootBlock ( )->nBlocks;
	nBlocksRequired += startBlock;
	ftx->unMapFile ( );
	ftx->fileSize = nBlocksRequired * FTX_PAGE_SIZE;
	ftx->mapFile ( );

	ftx->getRootBlock ( )->nBlocks = (unsigned long)nBlocksRequired;

	memset ( ftx->data + (static_cast<ptrdiff_t>(startBlock) * FTX_PAGE_SIZE), 0, (nBlocksRequired - startBlock) * FTX_PAGE_SIZE );

	ftx->nKeys	 = 0;
}

void _ftxKeyAddToIndex ( FTX *ftx, FTX_CONNECTION *ftxConn, char const *key, char const *dep )
{
	size_t					 page;
	size_t					 keyPos;
	FTX_LEAF				*leaf;
	char					*keyStart;
	size_t					 depSize;
	size_t					 keySize;
	size_t					 keyLen;
	char					*keyPtr;

	if ( ftx->getHeader()->nLeafKeys < 4 )
	{
		page	= static_cast<size_t>(ftx->nKeys)  / (ftx->getHeader()->nLeafKeys);
	} else
	{
		page	= static_cast<size_t>(ftx->nKeys)  / (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1);	// extra space for addition at end
	}

	page += 2;		// add 2 to get past header and root block

	leaf = ftx->getLeafBlock ( page );
	keyPos  = leaf->nKeys;

	depSize = ftx->getHeader()->depSize;
	keySize = ftx->getHeader()->keySize;

	keyStart = leaf->getData ( );
	keyLen   = static_cast<size_t>(ftx->getHeader()->keySize) + ftx->getHeader()->depSize;
	keyPtr = keyStart + keyPos * keyLen;

	// copy dependency/key data to index 
	memcpy ( keyPtr, dep, depSize );
	memcpy ( keyPtr + depSize, key, keySize );

	leaf->nKeys++;
	ftx->nKeys++;

	if ( !leaf->type )
	{
		leaf->type = FTX_LEAF_TYPE;
	}
}

static void ftxKeyAddToBranchIndex ( FTX *ftx, FTX_CONNECTION *ftxConn, char const *key, char const *dep, int offset )
{
	size_t					 page;
	size_t					 keyPos;
	FTX_BRANCH				*branch;
	FTX_ROOT				*root;
	char					*keyStart;
	size_t					 depSize;
	size_t					 keySize;
	size_t					 keyLen;
	char					*keyPtr;

	if ( ftx->getHeader()->nBranchKeys < 4 )
	{
		page	= static_cast<size_t>(ftx->nKeys) / (ftx->getHeader()->nBranchKeys);
	} else
	{
		page	= static_cast<size_t>(ftx->nKeys) / (static_cast<size_t>(ftx->getHeader()->nBranchKeys) - 1);	// extra space for addition at end
	}

	page += offset;		// add 2 to get past header and root block

	switch ( ftx->getBlockType ( page ) )
	{
		case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
			root = ftx->getRootBlock ( );
			keyStart = root->getData ( );
			
			keyPos  = root->nKeys;
			depSize = sizeof ( unsigned long );
			keySize = ftx->getHeader()->keySize;
			keyLen   = ftx->getHeader()->keySize + depSize;
			root->nKeys++;
			break;
		case 0:
		case FTX_BRANCH_TYPE:
			branch = ftx->getBranchBlock ( page );
			keyStart = branch->getData ( );
			keyPos  = branch->nKeys;
			
			depSize = sizeof ( unsigned long );
			keySize = ftx->getHeader()->keySize;
			keyLen   = ftx->getHeader()->keySize + depSize;

			branch->nKeys++;

			branch->type = FTX_BRANCH_TYPE;
			break;
		default:
			throw errorNum::scINTERNAL;
	}

	keyPtr = keyStart + keyPos * keyLen;

	// copy dependency/key data to index 
	memcpy ( keyPtr, dep, depSize );
	memcpy ( keyPtr + depSize, key, keySize );

	ftx->nKeys++;
}

static int ftxKeyFixupBranchKeys ( FTX *ftx, FTX_CONNECTION *ftxConn, int page )
{
	FTX_BRANCH	*branch;
	FTX_ROOT	*root;

	switch ( ftx->getBlockType ( page ) )
	{
		case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
			root = ftx->getRootBlock();
			root->nKeys--;
			break;
		case 0:
		case FTX_BRANCH_TYPE:
			branch = ftx->getBranchBlock ( page );
			branch->nKeys--;
			break;
	}
	return ( 1 );
}

static _inline void *ftxGetLeafKeyPtr ( FTX *ftx, FTX_LEAF *leaf, size_t nKey )
{
	unsigned long	 keyPos;
	char			*keyStart;
	char			*keyPtr;

	if ( ftx->getHeader()->nLeafKeys < 4 )
	{
		keyPos  = nKey % ftx->getHeader()->nLeafKeys;
	} else
	{
		keyPos  = nKey % (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1);
	}

	keyStart = leaf->getData ( );

	keyPtr = keyStart + static_cast<ptrdiff_t>(keyPos) * (static_cast<size_t>(ftx->getHeader()->keySize) + ftx->getHeader()->depSize);

	return keyPtr;
}

static _inline void *ftxGetLargestKeyPtr ( FTX *ftx, uint64_t page )
{
	FTX_ROOT		*root;
	FTX_LEAF		*leaf;
	FTX_BRANCH		*branch;
	char			*keyStart;
	unsigned int	 depSize;
	unsigned int	 keySize;
	unsigned int	 keyPos;
	char			*keyPtr;

	switch ( (uint32_t) ftx->getBlockType ( page ) )
	{
		case FTX_LEAF_TYPE:
			leaf = ftx->getLeafBlock ( page );
			keyStart = leaf->getData ( );

			keyPos  = leaf->nKeys - 1;

			depSize = ftx->getHeader()->depSize;
			keySize = ftx->getHeader()->keySize;
			break;
		case FTX_BRANCH_TYPE:
			branch = ftx->getBranchBlock ( page );
			keyStart = branch->getData ( );

			keyPos  = branch->nKeys - 1;

			depSize = sizeof ( unsigned long );
			keySize = ftx->getHeader()->keySize;
			break;
		case FTX_ROOT_TYPE | FTX_BRANCH_TYPE:
			root = ftx->getRootBlock ( );
			keyStart = root->getData ( );

			keyPos = root->nKeys + 1;

			depSize = sizeof ( unsigned long );
			keySize = ftx->getHeader ( )->keySize;
			break;
		case FTX_ROOT_TYPE | FTX_LEAF_TYPE:
			root = ftx->getRootBlock ( );
			keyStart = root->getData ( );

			keyPos = root->nKeys + 1;

			depSize = ftx->getHeader ( )->depSize;
			keySize = ftx->getHeader ( )->keySize;
			break;
		default:
			throw errorNum::scINTERNAL;
	}

	keyPtr = keyStart + static_cast<ptrdiff_t>(keyPos) * (static_cast<size_t>(keySize) + depSize);
	return keyPtr + depSize;
}

static _inline int ftxSortCompare ( FTX *ftx, FTX_CONNECTION *ftxConn, size_t a, size_t b )
{
	char	*ptr1;
	char	*ptr2;
	int		 res;
	size_t	 aPage;
	size_t	 bPage;

	auto header = ftx->getHeader ( );
	if ( header->nLeafKeys < 4 )
	{
		aPage	 = a / (static_cast<size_t>(header->nLeafKeys));			// extra space for addition at end
	} else
	{
		aPage	 = a / (static_cast<size_t>(header->nLeafKeys) - 1);		// extra space for addition at end
	}
	aPage   += 2;									// add 2 to get past header and root block

	if ( header->nLeafKeys < 4 )
	{
		bPage	 = b / (static_cast<size_t>(header->nLeafKeys));			// extra space for addition at end
	} else
	{
		bPage	 = b / (static_cast<size_t>(header->nLeafKeys) - 1);		// extra space for addition at end
	}
	bPage   += 2;									// add 2 to get past header and root block

	auto aLeaf = ftx->getLeafBlock ( aPage );
	auto bLeaf = ftx->getLeafBlock ( bPage );

	ptr1 = (char *)ftxGetLeafKeyPtr ( ftx, aLeaf, a );
	ptr2 = (char *)ftxGetLeafKeyPtr ( ftx, bLeaf, b );

	res = ftx->cmpRtn ( ptr1 + header->depSize, ptr2 + header->depSize, header->keySize );

	if ( header->isDescending )
	{
		res = -res;
	}
	return res;
}

static _inline void ftxSortSwap ( FTX *ftx, FTX_CONNECTION *ftxConn, int a, int b )
{
	char	*ptr1;
	char	*ptr2;
	size_t	 aPage;
	size_t	 bPage;
	char	 tmpBuff[FTX_PAGE_SIZE];

	if ( ftx->getHeader()->nLeafKeys < 4 )
	{
		aPage	 = static_cast<size_t>(a) / (ftx->getHeader()->nLeafKeys);		// extra space for addition at end
	} else
	{
		aPage	 = static_cast<size_t>(a) / (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1);	// extra space for addition at end
	}
	aPage   += 2;									// add 2 to get past header and root block

	if ( ftx->getHeader()->nLeafKeys < 4 )
	{
		bPage	 = static_cast<size_t>(b) / (ftx->getHeader()->nLeafKeys);		// extra space for addition at end
	} else
	{
		bPage	 = static_cast<size_t>(b) / (static_cast<size_t>(ftx->getHeader()->nLeafKeys) - 1);	// extra space for addition at end
	}
	bPage   += 2;									// add 2 to get past header and root block

	auto aLeaf = ftx->getLeafBlock ( aPage );
	auto bLeaf = ftx->getLeafBlock ( bPage );

	ptr1 = (char *)ftxGetLeafKeyPtr ( ftx, aLeaf, a );
	ptr2 = (char *)ftxGetLeafKeyPtr ( ftx, bLeaf, b );

	memcpy ( tmpBuff,	ptr1,	 static_cast<size_t>(ftx->getHeader()->depSize) + ftx->getHeader()->keySize );
	memcpy ( ptr1,		ptr2,	 static_cast<size_t>(ftx->getHeader()->depSize) + ftx->getHeader()->keySize );
	memcpy ( ptr2,		tmpBuff, static_cast<size_t>(ftx->getHeader()->depSize) + ftx->getHeader()->keySize );
}

static void ftxSortQuicksort ( FTX *ftx, FTX_CONNECTION *ftxConn, unsigned num )
{
    unsigned int lo, hi;              /* ends of sub-array currently sorting */
    unsigned int mid;                  /* points to middle of subarray */
    unsigned int loguy, higuy;        /* traveling pointers for partition step */
    unsigned size;              /* size of the sub-array */
	unsigned int lostk[64]{}, histk[64]{};
    int stkptr;                 /* stack for saving sub-array to be processed */

    /* Note: the number of stack entries required is no more than
       1 + log2(size), so 30 is sufficient for any array */

    if (num < 2 )
        return;                 /* nothing to do */

    stkptr = 0;                 /* initialize stack */

    lo = 0;
    hi = num-1;        /* initialize limits */

    /* this entry point is for pseudo-recursion calling: setting
       lo and hi and jumping to here is like recursion, but stkptr is
       prserved, locals aren't, so we preserve stuff on the stack */
recurse:

    size = (hi - lo) + 1;        /* number of el's to sort */

    /* below a certain size, it is faster to use a O(n^2) sorting method */
//    if (0 && size <= CUTOFF) {
//         shortsort(lo, hi, 1, comp);
//    }
//    else 
	{
        /* First we pick a partititioning element.  The efficiency of the
           algorithm demands that we find one that is approximately the
           median of the values, but also that we select one fast.  Using
           the first one produces bad performace if the array is already
           sorted, so we use the middle one, which would require a very
           wierdly arranged array for worst case performance.  Testing shows
           that a median-of-three algorithm does not, in general, increase
           performance. */

        mid = lo + (size / 2);      /* find middle element */
        ftxSortSwap ( ftx, ftxConn, static_cast<int>(mid), static_cast<int>(lo) );               /* swap it to beginning of array */

        /* We now wish to partition the array into three pieces, one
           consisiting of elements <= partition element, one of elements
           equal to the parition element, and one of element >= to it.  This
           is done below; comments indicate conditions established at every
           step. */

        loguy = lo;
        higuy = hi + 1;

        /* Note that higuy decreases and loguy increases on every iteration,
           so loop must terminate. */
		for ( ;; )
		{
            /* lo <= loguy < hi, lo < higuy <= hi + 1,
               A[i] <= A[lo] for lo <= i <= loguy,
               A[i] >= A[lo] for higuy <= i <= hi */

            do  {
                loguy += 1;
            } while (loguy <= hi && ftxSortCompare ( ftx, ftxConn, loguy, lo) <= 0);

            /* lo < loguy <= hi+1, A[i] <= A[lo] for lo <= i < loguy,
               either loguy > hi or A[loguy] > A[lo] */

            do  {
                higuy -= 1;
            } while (higuy > lo && ftxSortCompare ( ftx, ftxConn, higuy, lo) >= 0);

            /* lo-1 <= higuy <= hi, A[i] >= A[lo] for higuy < i <= hi,
               either higuy <= lo or A[higuy] < A[lo] */

            if (higuy < loguy)
                break;

            /* if loguy > hi or higuy <= lo, then we would have exited, so
               A[loguy] > A[lo], A[higuy] < A[lo],
               loguy < hi, highy > lo */

            ftxSortSwap ( ftx, ftxConn, static_cast<int>(loguy), static_cast<int>(higuy));

            /* A[loguy] < A[lo], A[higuy] > A[lo]; so condition at top
               of loop is re-established */
        }

        /*     A[i] >= A[lo] for higuy < i <= hi,
               A[i] <= A[lo] for lo <= i < loguy,
               higuy < loguy, lo <= higuy <= hi
           implying:
               A[i] >= A[lo] for loguy <= i <= hi,
               A[i] <= A[lo] for lo <= i <= higuy,
               A[i] = A[lo] for higuy < i < loguy */

        ftxSortSwap ( ftx, ftxConn, static_cast<int>(lo), static_cast<int>(higuy));     /* put partition element in place */

        /* OK, now we have the following:
              A[i] >= A[higuy] for loguy <= i <= hi,
              A[i] <= A[higuy] for lo <= i < higuy
              A[i] = A[lo] for higuy <= i < loguy    */

        /* We've finished the partition, now we want to sort the subarrays
           [lo, higuy-1] and [loguy, hi].
           We do the smaller one first to minimize stack usage.
           We only sort arrays of length 2 or more.*/

        if ( higuy - 1 - lo >= hi - loguy ) {
            if (lo + 1 < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy - 1;
                ++stkptr;
            }                           /* save big recursion for later */

            if (loguy < hi) {
                lo = loguy;
                goto recurse;           /* do small recursion */
            }
        }
        else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                ++stkptr;               /* save big recursion for later */
            }

            if (lo + 1 < higuy) {
                hi = higuy - 1;
                goto recurse;           /* do small recursion */
            }
        }
    }

    /* We have sorted the array, except for any pending sorts on the stack.
       Check if there are any, and do them. */

    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;           /* pop subarray from stack */
    }
    else
        return;                 /* all subarrays done */
}

void _ftxKeySort ( FTX *ftx, FTX_CONNECTION *ftxConn )
{
	FTX_ROOT				*rootBlock;
	unsigned int			 blockStart;
	unsigned int			 blockEnd;
	unsigned int			 branchStart;
	unsigned int			 nBranchKeys;
	unsigned int			 allBranchStart;
	char					*ptr1;

	if ( ftx->nKeys )
	{
		// ok... sort all the leaf keys
		ftxSortQuicksort ( ftx, ftxConn, ftx->nKeys );

		printf ( " tree..." );
		// now iterate through and build the tree

		// we can't fit it all in a single root... we have to build intermediary nodes

		rootBlock = ftx->getRootBlock ( );
		rootBlock->type = FTX_ROOT_TYPE | FTX_BRANCH_TYPE;	// created as root|leaf... make it root|branch

		blockStart = 2;

		allBranchStart = rootBlock->nBlocks;
		while ( 1 )
		{
			blockEnd    = rootBlock->nBlocks;
			nBranchKeys = (blockEnd - blockStart);

			// reset the key number to start the branch key add at 0
			ftx->nKeys = 0;

			if ( nBranchKeys <= (unsigned int)(ftx->getHeader()->nRootBranchKeys - 1) )
			{
				// we can fit inside a single branch node

				while ( blockStart < blockEnd )
				{
					ptr1 = (char *)ftxGetLargestKeyPtr ( ftx, blockStart );
					ftxKeyAddToBranchIndex ( ftx, ftxConn, ptr1, (char *)&blockStart, 1 );

					blockStart++;
				}

				// all done... at root level

				// we need to fix up all the branch's counts
				ftxKeyFixupBranchKeys ( ftx, ftxConn, 1);

				while ( allBranchStart < rootBlock->nBlocks )
				{
					ftxKeyFixupBranchKeys ( ftx, ftxConn, static_cast<int>(allBranchStart) );
					allBranchStart++;
				}
				break;
			} else
			{
				// branchStart will be the beginning of the next tier of branch's
				branchStart = rootBlock->nBlocks;

				_ftxAddKeys ( ftx, ftxConn, FTX_BRANCH_TYPE, nBranchKeys );
				rootBlock = ftx->getRootBlock ( );

				// where are we beginning our add at?
				while ( blockStart < blockEnd )
				{
					ptr1 = (char *)ftxGetLargestKeyPtr ( ftx, blockStart );

					ftxKeyAddToBranchIndex ( ftx, ftxConn, ptr1, (char *)&blockStart, static_cast<int>(branchStart) );

					blockStart++;
				}
			}

			blockStart = branchStart;
		}
	}
}

void FTX::compileKey ( vmInstance *instance, stringi const &expression )
{
	static fgxTryCatchEntry		 nullTcList[] = { {0xFFFFFFFF, 0xFFFFFFFF, 0} };

	indexExpression = expression;

	cbLoadImage = std::make_shared<bcLoadImage>(instance, "<ftxkey>" );

	instance->stack = instance->eval + 1;

	cb.reset ( _compileIndexCodeblock ( instance, indexExpression.c_str ( ) ) );

	cbFunc.name = "<index key>";
	cbFunc.symbols = (fgxSymbols *) ((uint8_t *) cb.get() + cb->symOffset);
	cbFunc.atom = 0;
	cbFunc.coRoutineAble = false;
	cbFunc.conv = fgxFuncCallConvention::opFuncType_Bytecode;
	cbFunc.retType = bcFuncPType::bcFuncPType_Variant;
	cbFunc.nParams = cb->nSymbols;
	cbFunc.nSymbols = cb->nSymbols;
	cbFunc.tcList = nullTcList;
	cbFunc.loadImage = cbLoadImage.get();
	cbFunc.cs = (fglOp *) ((uint8_t *) cb.get ( ) + cb->csOffset) + 1;
	cbLoadImage->csBase = (fglOp *) ((uint8_t *) cb.get ( ) + cb->csOffset);
	cbLoadImage->dsBase = (uint8_t *) ((uint8_t *) cb.get ( ) + cb->dsOffset);
	cb->bssSize = 0;
}
