/*

	peeophole optimizer

*/

#pragma once

#include "compilerBCGenerator/compilerBCGenerator.h"


class phPattern {
public:
	astNode		*pat = nullptr;
	astNode		*res = nullptr;

	phPattern ( char const *match, char const *tran );
	~phPattern();
};


class opTransform {
public:
	cacheString		 i1;					// symbol for i1
	cacheString		 d1;					// symbol for i1
	cacheString		 s1;					// symbol for i1
	int64_t			 iValue1 = 0;			// value for value1
	double			 dValue1 = 0.0;	
	char			*sValue1 = nullptr;		// value for value1
	astNode			*exp = nullptr;

	bool		 iValue1Valid = false;
	bool		 dValue1Valid = false;
	bool		 sValue1Valid = false;
	bool		 expValid = false;
	bool		 expUsed = false;

	opFile		 *file = nullptr;

	cacheString		i1Value;
	cacheString		d1Value;
	cacheString		s1Value;
	cacheString		expValue;
	cacheString		iExpValue;
	cacheString		i990Value;

	opTransform ( class opFile *file );

	~opTransform()
	{
	}

	bool	 phCompare		( astNode *pat, astNode *match, symbolStack *sym );
	astNode	*phGen			( astNode *pat );

private:
	astNode	*doTransform	( astNode *pat );

public:
	astNode	*phScan			( astNode *node, symbolStack *sym );
	astNode	*transform		( astNode *node, symbolStack *sym, bool *transformMade );
	void	 deleteNode		( astNode *node );
};

class phPatterns {
public:
	phPatterns();
	~phPatterns();
};

