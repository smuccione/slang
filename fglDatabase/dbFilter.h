/*

	database filter header file

*/


#pragma once

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <Windows.h>
#include <stdint.h>

struct FILTER_HEADER {
	int			version;
	int			offsetExpr;			/* offset in file of filter expression */
	int			lenExpr;			/* length in file of filter expression */
	int			offsetRecords;		/* offset in file of records matching expression */
	int			maxVRecs;			/* maximum virtual record number in the table */
};

struct FILTER_NODE {
	int			isLeaf;
	long		threadLeftOffset;
	long		threadRightOffset;
	int			nEntries;
	union	{
		struct {
			int recno;
		} leaf;
		struct {
			int recno;
		}dummy;
	};
};

struct FILTER_REC {
	int				recNo;
};

struct FILTER {
	struct FILTER	*next;

	HANDLE			 fHandle;				/* file Handle */
	HANDLE			 fMapObject;			/* handle to file mapping object */
	void			*mappedView;
	int				 mappedSize;
	__int64			 mappedOffset;			/* within the file */

	int				 include;				/* when set, records are IN the set */

	int				 maxVRec;				/* maximum virtual record number in table */

	char			 fName[MAX_PATH];

	int				 currenVRec;
	int				 currentRec;			/* current record */
	int				 numRecs;				/* number of FILTER_REC's in the file */
	FILTER_REC		*startRec;

	char			*expr;
};

extern	FILTER	*filterOpen			( char *fName, int include, struct DATABASE *db, struct DATABASE_CONNECTION *dbConn );
extern	int		 filterClose		( FILTER *filter );
extern	int		 filterErase		( FILTER *filter );
extern	int		 filterCreate		( char *fName, char *expr, struct DATABASE *db, struct DATABASE_CONNECTION *dbConn, struct VAR *cb );
extern	int		 filterSkip			( FILTER *filter, int count );
extern	int		 filterGoTop		( FILTER *filter );
extern  int		 filterGoBot		( FILTER *filter );
