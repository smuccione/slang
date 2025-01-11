/*
	class and type definitions for the expression parser

*/

#pragma once

#include "bcVM/exeStore.h"
#include "compilerAST/astNode.h"

struct opListDef {
	char const	*c;
	astOpCat	 opCat;
	size_t		 cLen;
	astOp		 op;
	size_t		 stackPriority;
	size_t		 infixPriority;
	bool		 isLeft;
	bool		 isOverloadable;
	size_t		 nParams;
	fgxOvOp		 overloadOp;
};

extern opListDef					opList[];
extern std::vector<enum fgxOvOp>	opToOvXref;
extern std::vector<opListDef *>		opToOpDef;

char const				*parseOperator	( class source &src );
