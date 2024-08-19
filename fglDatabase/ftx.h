/*
    Advanced association driver with high performance concurrent access.


*/

#pragma once

#include "Utility/settings.h"

#include <vector>

#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmInstance.h"
#include "Utility/funcky.h"
#include "bcVM/fglTypes.h"
#include "bcInterpreter/op_variant.h"
#include "Utility/buffer.h"
#include "Utility/util.h"
#include "Utility/stringi.h"

#define FTX_PAGE_SIZE				2048ull
#define FTX_PAGE_ALLOCATION_QUANTA	16ull

#define TOP         1
#define BOTTOM      2
#define RIGHT_FOUND 3
#define LEFT        4
#define LEFT_FOUND  5

#define FTX_DEF_STACK		256	

#define FTX_LEAF_TYPE		1
#define FTX_BRANCH_TYPE		2
#define FTX_ROOT_TYPE		4

#define FTX_ERROR_EXPR		0
#define FTX_CHAR_EXPR		1
#define FTX_LONG_EXPR		2
#define FTX_DOUBLE_EXPR		3

#pragma pack ( push, FTX, 1 )
struct FTX_HEADER {
	unsigned	short	keySize;
	unsigned	short	depSize;
	unsigned	short	nRootBranchKeys;
	unsigned	short	nRootLeafKeys;
	unsigned	short	nBranchKeys;
	unsigned	short	nLeafKeys;
				char	isDescending;
				char	exprType;
				char	keyExpr[220];
	unsigned	short	isFilter;
				char	filterExpr[220];
};

struct FTX_ROOT {
   unsigned char	type;
   unsigned long	free;
//   uint32_t			updateCount;
   unsigned long	nBlocks;
   unsigned char	nKeys;

   char *getData ( )
   {
	   return (char *)&this[1];
   }
};

struct FTX_BRANCH {
   unsigned char type;
   unsigned char nKeys;
   char *getData ( )
   {
	   return (char *)&this[1];
   }
};

struct FTX_LEAF {
   unsigned char type;
   unsigned char nKeys;
   char *getData ( )
   {
	   return (char *)&this[1];
   }
};

struct FTXfree {
   unsigned long next;
   char *getData ( )
   {
	   return (char *)&this[1];
   }
};
#pragma pack ( pop, FTX )

struct FTX_STACK{
	uint64_t  nPage;
	uint64_t  nKeys;
	uint64_t	pos;
};

struct FTX_CONNECTION {
	FTX_CONNECTION			 *next = nullptr;

	int						  lastUpdate = 0;	/* for consistency... if an update was done we need to re-seek */
	std::vector<FTX_STACK>	  stack;

	char					  knownKey[1024];
	char					  knownDep[1024];
	char					  hotKey[1024];
	char					  hotDep[1024];	
};

struct FTX {
	stringi					   fName;
	stringi					   indexExpression;

	// for db server support
	size_t					   which;				/* allows fast work with xref						*/

	// header lock
	SRRWLOCK				   headerLock;			/* header lock										*/
	HANDLE					   fileHandle = 0;		/* file handle										*/
	HANDLE					   fileMap = 0;			/* memory mapped handle								*/
	uint8_t					  *data = 0;			/* pointer to mapped view of memory					*/

	// used during construction
	unsigned long			   nKeys = 0;		// number of keys in ftx during indexing operation
	unsigned long			   maxPage = 0;		// current max page during indexing

	int						   lastUpdate;	/* for consistency					*/
	int						 (*cmpRtn) (void const *p1, void const *p2, size_t len );

	size_t					   fileSize;

	bcFuncDef							 cbFunc;
	std::shared_ptr <bcLoadImage>		 cbLoadImage;
	std::shared_ptr <vmCodeBlock>		 cb;

	FTX ( )
	{
	}

	FTX ( FTX &&old ) noexcept
	{
		*this = std::move(old);
	}

	FTX ( FTX const &old ) noexcept
	{
		fName = old.fName;
		indexExpression = old.indexExpression;
		which = old.which;

		DuplicateHandle ( GetCurrentProcess ( ), old.fileHandle,
						  GetCurrentProcess ( ), &fileHandle, 0,
						  TRUE, DUPLICATE_SAME_ACCESS );

		nKeys = old.nKeys;
		maxPage = old.maxPage;
		lastUpdate = old.lastUpdate;
		cmpRtn = old.cmpRtn;
		fileSize = old.fileSize;
		cbFunc = old.cbFunc;
		cbLoadImage = old.cbLoadImage;
		cb = old.cb;

		// must be done AFTER fileSize is initialized
		mapFile ( );

		//  SRRWLOCK				   headerLock;			we're not copying... we're creating a whole new one...  this is wrong if we want to really share ftx's
	}

	FTX &operator = ( FTX &&old ) noexcept
	{
		std::swap ( fName, old.fName );
		std::swap ( indexExpression, old.indexExpression );
		std::swap ( which, old.which );
		std::swap ( headerLock, old.headerLock );
		std::swap ( fileHandle, old.fileHandle );
		std::swap ( fileMap, old.fileMap );
		std::swap ( data, old.data );
		std::swap ( nKeys, old.nKeys );
		std::swap ( maxPage, old.maxPage );
		std::swap ( lastUpdate, old.lastUpdate );
		std::swap ( cmpRtn, old.cmpRtn );
		std::swap ( fileSize, old.fileSize );
		std::swap ( cbFunc, old.cbFunc );
		std::swap ( cbLoadImage, old.cbLoadImage );
		std::swap ( cb, old.cb );

		return *this;
	}

	FTX &operator = ( FTX const &old )
	{
		return *this = FTX ( old );
	}

	virtual ~FTX ( )
	{
		if ( fileMap )	unMapFile ( );
		if ( fileHandle ) CloseHandle ( fileHandle );
	}

	void compileKey ( vmInstance *instance, stringi const &expression );

	FTX_HEADER *getHeader ( )
	{
		return (FTX_HEADER*) data;
	}

	size_t getDepSize ( )
	{
		return getHeader ( )->depSize;
	}

	size_t getKeySize ( )
	{
		return getHeader ( )->keySize;
	}

	void unMapFile ( )
	{
		if ( fileMap )
		{
			UnmapViewOfFile ( data );
			CloseHandle ( fileMap );
			fileMap = 0;
		}
	}

	void setUpdateCount ( uint32_t updateCount )
	{
//		getRootBlock ()->updateCount = updateCount;
	}

	uint32_t getUpdateCount ()
	{
		return 0;
//		return getRootBlock ()->updateCount;
	}

	void setFileSize ( size_t size )
	{
		LARGE_INTEGER	newAddr;

		unMapFile ();
		SetFilePointerEx ( fileHandle, LARGE_INTEGER{{0}}, &newAddr, FILE_END );
		SetEndOfFile ( fileHandle );
		mapFile ();
	}

	void mapFile ( )
	{
		unMapFile ();

		fileMap = CreateFileMapping ( fileHandle, NULL, PAGE_READWRITE, (DWORD) (fileSize >> 32), (DWORD) (fileSize & 0xFFFFFFFF), NULL );
		data = (uint8_t *) MapViewOfFile ( fileMap, FILE_MAP_ALL_ACCESS, 0, 0, fileSize );
	}

	FTX_ROOT *getRootBlock ( )
	{
		return (FTX_ROOT *) (data + (FTX_PAGE_SIZE));

	}
	FTX_LEAF *getLeafBlock ( size_t nBlock )
	{
		return (FTX_LEAF *) (data + (nBlock * FTX_PAGE_SIZE));

	}
	FTX_BRANCH *getBranchBlock ( size_t nBlock )
	{
		return (FTX_BRANCH *) (data + (nBlock * FTX_PAGE_SIZE));

	}
	FTXfree *getFreeBlock ( size_t nBlock )
	{
		return (FTXfree *) (data + (nBlock * FTX_PAGE_SIZE));
	}

	char getBlockType ( size_t nBlock )
	{
		return *(char *)(data + (nBlock * FTX_PAGE_SIZE));
	}
};

// open/close functions
extern FTX   _ftxCreate			( char const *fname, char const *keyExpr, size_t mode, size_t keySize, size_t depSize, bool isDescending, char exprType, char const *filterExpr, uint32_t updateCount );
extern FTX   _ftxOpen			( char const *fname, int mode, uint32_t updateCount );
extern void _ftxDelete			( FTX *ftx );		/* so we can rebuild corrupted index's */

// maniuplation functions
extern bool		 _ftxKeyAdd			( FTX *ftx, FTX_CONNECTION *ftxConn,    void const *key,   void const *dep   );
extern bool		 _ftxKeyDelete		( FTX *ftx, FTX_CONNECTION *ftxConn,    void const *key,   void const *dep   );
extern void		*_ftxSeek			( FTX *ftx, FTX_CONNECTION *ftxConn,    void const *key,	 size_t len   );
extern void		*_ftxSeekLast		( FTX *ftx, FTX_CONNECTION *ftxConn,    void const *key,	 size_t len   );
extern void		*_ftxSkip			( FTX *ftx, FTX_CONNECTION *ftxConn,	int64_t		count        );
extern void		*_ftxGoTop			( FTX *ftx, FTX_CONNECTION *ftxConn                              );
extern void		*_ftxGoBottom		( FTX *ftx, FTX_CONNECTION *ftxConn                              );
extern bool		 _ftxIsTop			( FTX *ftx, FTX_CONNECTION *ftxConn                              );
extern bool		 _ftxIsBottom		( FTX *ftx, FTX_CONNECTION *ftxConn                              );
extern bool		 _ftxGoto			( FTX *ftx, FTX_CONNECTION *ftxConn,    void const *key,   void const *dep   );
extern void		 _ftxClear			( FTX *ftx, FTX_CONNECTION *ftxConn                              );
extern char		*_ftxGetExpr		( FTX *ftx, FTX_CONNECTION *ftxConn );
extern char		*_ftxGetFilter		( FTX *ftx, FTX_CONNECTION *ftxConn );
extern size_t	 _ftxCountMatching	( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, size_t len );
extern size_t	 _ftxFindSubset		( FTX *ftx, FTX_CONNECTION *ftxConn, void const *key, size_t len, size_t every, void **subSet );

// private
extern void		_ftxKeySort			( FTX *ftx, FTX_CONNECTION *ftxConn );
extern void		_ftxKeyAddToIndex	( FTX *ftx, FTX_CONNECTION *ftxConn, char const *key, char const *dep );
extern void		_ftxAdjustFileSize	( FTX *ftx, FTX_CONNECTION *ftxConn );
extern void		_ftxAddKeys			( FTX *ftx, FTX_CONNECTION *ftxConn, int keyType, size_t nKeys );
