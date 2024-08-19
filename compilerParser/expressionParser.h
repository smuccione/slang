/*
	class and type definitions for the expression parser

*/

#pragma once

#include "bcVM/exeStore.h"
#include "compilerAST/astNode.h"

struct opListDef {
	char const	*c;
	astOpCat	 opCat;
	int			 cLen;
	astOp		 op;
	int			 stackPriority;
	int			 infixPriority;
	bool		 isLeft;
	bool		 isOverloadable;
	int			 nParams;
	fgxOvOp		 overloadOp;
};

extern opListDef					opList[];
extern std::vector<enum fgxOvOp>	opToOvXref;
extern std::vector<opListDef *>		opToOpDef;

char const				*parseOperator	( class source &src );
