/*

	block processor


*/

#pragma once

#include "Utility/staticString.h"
#include "Utility/stringCache.h"

enum class statementType {
	stLine				= 1,
	stIf,
	stElseIf,
	stElse,
	stEnd,
	stOpenBrace,
	stCloseBrace,
	stLabel,
	stReturn,
	stYield,
	stGoto,
	stFor,
	stForEach,
	stWhile,
	stDo,
	stRepeat,
	stUntil,
	stSwitch,
	stCase,
	stContinue,
	stBreak,
	stDefault,
	stTry,
	stCatch,
	stFinally,
	stProcedure,
	stFunction,
	stIterator,
	stLocal,
	stGlobal,
	stField,
	stClass,
	stMethod,
	stAssign,
	stAccess,
	stOperator,
	stConst,
	stInherit,
	stProtected,
	stPublic,
	stPrivate,
	stMessage,
	stExtern,
	stLibrary,
	stModule,
	stDll,
	stInteger,
	stDouble,
	stString,
	stArray,
	stCodeblock,
	stObject,
	stVariant,
	stNamespace,
	stVirtual,
	stStatic,
	stExtension,
	stProperty,
	stGet,
	stSet,
	stFinal,
	stUsing,
	stPragma,
	stPPdefine,
	stPPinclude,
	stPPif,
	stPPifDef,
	stPPifNdef,
	stPPelse,
	stPPelIf,
	stPPendIf,
	stPPerror,
	stPPwarn,
};

struct statementList {
	char const			*name;
	int					 len;
	statementType		 type;
};

extern	const	statementList statementName[];
extern  const	std::map<statementString, statementList> reservedMap;
extern  const	std::map<statementString, statementList> reservedMapFgl;
extern  const	std::map<statementString, statementList> statementMap;
extern  const	std::map<statementString, statementList> statementMapFGL;
extern	const	std::map<statementString, statementList> ppMap;

extern	bool		 cmpTerminated	( char const *expr, char const *name, uint32_t len );
extern	char		*getLine		( class source &src );
extern	stringi		 buildString	( char const *preFix, char const *name, char const *postFix );
extern	stringi		 getSymbol		( class source &src );
