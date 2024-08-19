/*
      Direct Access Database system

*/
	
#pragma once

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "Utility/settings.h"

#include <stdint.h>
#include <tuple>
#include <unordered_map>
#include <set>
#include <Windows.h>

#include "ftx.h"
#include "blob.h"
#include "odbf.h"
#include "dbDef.h"
#include "Utility/util.h"
#include "..\Utility/stringi.h"
#include "bcVM/vmInstance.h"

struct DBS_REC_LOCK {
	uint64_t						 recNo;						/* record number of locked record					*/
	uint64_t						 cnt;						/* number of locks on the record					*/
	struct	DATABASE_CONNECTION		*dbConn;					/* pointer identifying the owner of the lock		*/
};

struct DATABASE_CONNECTION {
	struct DATABASE_CONNECTION		*next;						/* for list of free structures						*/
	TABLE_CONNECTION				*tblConn;					/* table connection									*/
	std::vector<FTX_CONNECTION>		 ftxConn;					/* array of all active ftx connections				*/
	std::vector<size_t>				 ftxXRef;					/* cross reference from opened to actual ftx's		*/
	size_t							 sFtx;						/* selected ftx										*/
	size_t							 userFtx;					/* user's selected ftx - go through xRef to generate sFtx */
	std::set<size_t >			 recLock;					/* to allow us to cleanup on shutdown				*/
	int								 inUse;
};

struct DATABASE {
	// protect the database structure
	SRRWLOCK								 lock;									/* header lock										*/
	size_t									 cCtr;
	DBDEF									*dbDef;
	TABLE									*tbl = 0;
	std::vector<FTX>						 ftx;
	std::vector<void const *(*)(vmInstance *instance, TABLE *tbl, FTX * ftx, FTX_CONNECTION *ftxConn)>	ftxCallBack;
	std::unordered_map<stringi, size_t>		 ftxLookup;									/* array of all active ftx's						*/
	std::unordered_map<size_t, DBS_REC_LOCK> recLock;

	DATABASE ( )
	{
	}
};

/* connection mgmt stuff */

/* database functions	*/

extern size_t							 dbCheck				( vmInstance *instance, char const *name );
extern bool								 dbRepair				( vmInstance *instance, char const *filename );
extern void								 dbClose				( DATABASE *db );
extern DATABASE							*dbOpen					( char const *name );
extern bool								 dbOpenIndex			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, stringi const &name, void const *( *cb )(vmInstance *instance, TABLE *db, FTX*ftx, FTX_CONNECTION *ftxConn), bool ( *filterCb )(vmInstance *instance, TABLE *db, FTX*ftx, FTX_CONNECTION *ftxConn) );
extern bool								 dbCreateIndex			( vmInstance *instance, DATABASE *db, stringi const &name, stringi const &keyExpr, bool isDescending, void const *( *cb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn), stringi const &filterExpr, bool ( *filterCb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn) );
extern bool								 dbCreateIndexExtended	( vmInstance *instance, DATABASE *db, stringi const &name, stringi const &keyExpr, bool isDescending, void const *( *cb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn), stringi const &filterExpr, bool ( *filterCb )(vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn) );
extern size_t							 dbRecordGet			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, struct BUFFER *buffer );
extern void								 dbRecordPut			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, char *data, size_t dataLen );
extern void								 dbCommit				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern uint64_t							 dbSeek					( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key                                          );
extern uint64_t							 dbSeekLen				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key,  size_t len                           );
extern uint64_t							 dbSeekLast				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key                                          );
extern uint64_t							 dbSeekLastLen			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void *key,  size_t len                           );
extern uint64_t							 dbGoto					( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno                                         );
extern uint64_t							 dbGoTop				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbIsTop				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern uint64_t							 dbGoBottom				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbIsBottom				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbEOF					( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbBOF					( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern uint64_t							 dbSkip					( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, int64_t count                                         );
extern size_t							 dbFieldGet				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num,    void *dest                       );
extern void								 dbFieldPut				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num,    void const *src,     size_t len                );
extern size_t							 dbOrder				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num                                            );
extern uint64_t							 dbAppend				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbReindex				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbFound				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern size_t							 dbFieldLen				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num                                            );
extern size_t							 dbFieldDec				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num                                            );
extern uint8_t							 dbFieldType			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num                                            );
extern size_t							 dbFieldPos				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, char const *name                                   );
extern char								*dbFieldName			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, size_t num,	char *dest					      );
extern uint64_t							 dbLastRec				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern uint64_t							 dbRecno				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern void								 dbDelete				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbRecall				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern bool								 dbIsDeleted			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn                                                     );
extern size_t							 dbFieldCount			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn );
extern size_t							 dbFindSubset			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void const *key, size_t len, size_t every, void **subset );
extern size_t							 dbCountMatching		( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, void const *key, size_t len );

extern void								 dbCopy					( vmInstance *instance, DATABASE *db, char const *directory );

extern size_t							 dbExprType				( vmInstance *instance, stringi const &expr, size_t &keyLen );


/* record locking stuff */
extern	bool						 dbIsRecLocked			( vmInstance *instance, DATABASE *db, uint64_t recno );
extern	bool						 dbLockRec				( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno );
extern	void						 dbLockAppend			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn );
extern	bool						 dbUnlockRec			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno );
extern	bool						 dbUnlockRecAll			( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, uint64_t recno );

extern void dbSyncIndex ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn );
extern void dbGoCold ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn );

extern void const *dbEval ( vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn );
extern bool dbFilterEval ( vmInstance *instance, TABLE *tbl, FTX *ftx, FTX_CONNECTION *ftxConn );
extern int	dbCmpKey ( vmInstance *instance, DATABASE *db, DATABASE_CONNECTION *dbConn, char const *key, size_t len );
