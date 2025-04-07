/*

	block processor


*/

#include "fileParser.h"

#define reservedDef \
DEF ("END", stEnd) \
DEF ("IF", stIf) \
DEF ("ELSE", stElse) \
DEF ("LABEL", stLabel) \
DEF ("RETURN", stReturn) \
DEF ("YIELD", stYield) \
DEF ("GOTO", stGoto) \
DEF ("FOREACH", stForEach) \
DEF ("FOR", stFor) \
DEF ("DO", stDo) \
DEF ("WHILE", stWhile) \
DEF ("SWITCH", stSwitch) \
DEF ("CASE", stCase) \
DEF ("BREAK", stBreak) \
DEF ("CONTINUE", stContinue) \
DEF ("DEFAULT", stDefault) \
DEF ("LOCAL", stLocal) \
DEF ("STATIC", stStatic) \
DEF ("TRY", stTry) \
DEF ("CATCH", stCatch) \
DEF ("FINALLY", stFinally) \
DEF ("CONST", stConst) \
DEF ("NAMESPACE", stNamespace) \
DEF ("USING", stUsing) \
DEF ("METHOD", stUsing) \
DEF ("ACCESS", stUsing) \
DEF ("ASSIGN", stUsing) \
DEF ("#PRAGMA", stPragma ) \
DEF ("#INCLUDE", stPPinclude ) \
DEF ("#IF", stPPif ) \
DEF ("#IFDEF", stPPifDef ) \
DEF ("#IFNDEF", stPPifNdef ) \
DEF ("#ELSE", stPPelse ) \
DEF ("#ELIF", stPPelIf ) \
DEF ("#ENDIF", stPPendIf ) \
DEF ("#ERROR", stPPerror ) \
DEF ("#WARN", stPPwarn )
//DEF ("FUNCTION", stFunction ) \		/// while we may think this SHOULD be resereved.. it's actually ok as it's a valid symbol to declare a lambda

#define reservedDefFgl \
DEF ("END", stEnd) \
DEF ("IF", stIf) \
DEF ("ELSEIF", stElseIf) \
DEF ("ELSE", stElse) \
DEF ("LABEL", stLabel) \
DEF ("RETURN", stReturn) \
DEF ("YIELD", stYield) \
DEF ("GOTO", stGoto) \
DEF ("FOREACH", stForEach) \
DEF ("FOR", stFor) \
DEF ("DO", stDo) \
DEF ("WHILE", stWhile) \
DEF ("REPEAT", stRepeat) \
DEF ("UNTIL", stUntil) \
DEF ("SWITCH", stSwitch) \
DEF ("CASE", stCase) \
DEF ("BREAK", stBreak) \
DEF ("CONTINUE", stContinue) \
DEF ("DEFAULT", stDefault) \
DEF ("LOCAL", stLocal) \
DEF ("STATIC", stStatic) \
DEF ("TRY", stTry) \
DEF ("CATCH", stCatch) \
DEF ("FINALLY", stFinally) \
DEF ("CONST", stConst) \
DEF ("METHOD", stUsing) \
DEF ("ACCESS", stUsing) \
DEF ("ASSIGN", stUsing) \
DEF ("#PRAGMA", stPragma ) \
DEF ("#INCLUDE", stPPinclude ) \
DEF ("#IF", stPPif ) \
DEF ("#IFDEF", stPPifDef ) \
DEF ("#IFNDEF", stPPifNdef ) \
DEF ("#ELSE", stPPelse ) \
DEF ("#ELIF", stPPelIf ) \
DEF ("#ENDIF", stPPendIf ) \
DEF ("#ERROR", stPPerror ) \
DEF ("#WARN", stPPwarn )
//DEF ("FUNCTION", stFunction ) \		/// while we may think this SHOULD be resereved.. it's actually ok as it's a valid symbol to declare a lambda

#define statementDef \
DEF ("END", stEnd) \
DEF ("{", stOpenBrace) \
DEF ("}", stCloseBrace) \
DEF ("IF", stIf) \
DEF ("ELSE", stElse) \
DEF ("LABEL", stLabel) \
DEF ("RETURN", stReturn) \
DEF ("YIELD", stYield) \
DEF ("GOTO", stGoto) \
DEF ("FOREACH", stForEach) \
DEF ("FOR", stFor) \
DEF ("DO", stDo) \
DEF ("WHILE", stWhile) \
DEF ("SWITCH", stSwitch) \
DEF ("CASE", stCase) \
DEF ("BREAK", stBreak) \
DEF ("CONTINUE", stContinue) \
DEF ("DEFAULT", stDefault) \
DEF ("PROCEDURE", stProcedure) \
DEF ("FUNCTION", stFunction) \
DEF ("ITERATOR", stIterator) \
DEF ("LOCAL", stLocal) \
DEF ("GLOBAL", stGlobal) \
DEF ("FIELD", stField) \
DEF ("STATIC", stStatic) \
DEF ("EXTENSION", stExtension ) \
DEF ("CLASS", stClass) \
DEF ("METHOD", stMethod) \
DEF ("ASSIGN", stAssign) \
DEF ("ACCESS", stAccess) \
DEF ("TRY", stTry) \
DEF ("CATCH", stCatch) \
DEF ("FINALLY", stFinally) \
DEF ("INHERIT", stInherit) \
DEF ("PROTECTED:", stProtected) \
DEF ("PUBLIC:", stPublic) \
DEF ("PRIVATE:", stPrivate) \
DEF ("MESSAGE", stMessage) \
DEF ("EXTERN", stExtern) \
DEF ("LIBRARY", stLibrary) \
DEF ("MODULE", stModule) \
DEF ("DLL", stDll) \
DEF ("OPERATOR", stOperator) \
DEF ("CONST", stConst) \
DEF ("INTEGER", stInteger) \
DEF ("INT", stInteger) \
DEF ("FLOAT", stDouble) \
DEF ("DOUBLE", stDouble) \
DEF ("STRING", stString) \
DEF ("ARRAY", stArray) \
DEF ("CODEBLOCK", stCodeblock) \
DEF ("OBJECT", stObject) \
DEF ("VARIANT", stVariant) \
DEF ("VAR", stVariant) \
DEF ("NAMESPACE", stNamespace) \
DEF ("USING", stUsing) \
DEF ("VIRTUAL", stVirtual) \
DEF ("STATIC", stStatic) \
DEF ("EXTENSION", stExtension ) \
DEF ("PROPERTY", stProperty) \
DEF ("GET", stGet ) \
DEF ("SET", stSet ) \
DEF ("FINAL", stFinal) \
DEF ("!", stOutput ) \
DEF ("#PRAGMA", stPragma ) \
DEF ("#INCLUDE", stPPinclude ) \
DEF ("#IF", stPPif ) \
DEF ("#IFDEF", stPPifDef ) \
DEF ("#IFNDEF", stPPifNdef ) \
DEF ("#ELSE", stPPelse ) \
DEF ("#ELIF", stPPelIf ) \
DEF ("#ENDIF", stPPendIf ) \
DEF ("#ERROR", stPPerror ) \
DEF ("#WARN", stPPwarn )

#define statementDefFGL \
DEF ("END", stEnd) \
DEF ("{", stOpenBrace) \
DEF ("}", stCloseBrace) \
DEF ("IF", stIf) \
DEF ("ELSEIF", stElseIf) \
DEF ("ELSE", stElse) \
DEF ("LABEL", stLabel) \
DEF ("RETURN", stReturn) \
DEF ("YIELD", stYield) \
DEF ("GOTO", stGoto) \
DEF ("FOREACH", stForEach) \
DEF ("FOR", stFor) \
DEF ("DO", stDo) \
DEF ("WHILE", stWhile) \
DEF ("REPEAT", stRepeat) \
DEF ("UNTIL", stUntil) \
DEF ("SWITCH", stSwitch) \
DEF ("CASE", stCase) \
DEF ("BREAK", stBreak) \
DEF ("CONTINUE", stContinue) \
DEF ("DEFAULT", stDefault) \
DEF ("PROCEDURE", stProcedure) \
DEF ("FUNCTION", stFunction) \
DEF ("ITERATOR", stIterator) \
DEF ("LOCAL", stLocal) \
DEF ("GLOBAL", stGlobal) \
DEF ("FIELD", stField) \
DEF ("CLASS", stClass) \
DEF ("METHOD", stMethod) \
DEF ("ASSIGN", stAssign) \
DEF ("ACCESS", stAccess) \
DEF ("TRY", stTry) \
DEF ("CATCH", stCatch) \
DEF ("FINALLY", stFinally) \
DEF ("INHERIT", stInherit) \
DEF ("PROTECTED:", stProtected) \
DEF ("PUBLIC:", stPublic) \
DEF ("PRIVATE:", stPrivate) \
DEF ("MESSAGE", stMessage) \
DEF ("EXTERN", stExtern) \
DEF ("LIBRARY", stLibrary) \
DEF ("DLL", stDll) \
DEF ("OPERATOR", stOperator) \
DEF ("CONST", stConst) \
DEF ("VIRTUAL", stVirtual) \
DEF ("STATIC", stStatic) \
DEF ("EXTENSION", stStatic) \
DEF ("FINAL", stFinal) \
DEF ("!", stOutput ) \
DEF ("#PRAGMA", stPragma ) \
DEF ("#INCLUDE", stPPinclude ) \
DEF ("#IF", stPPif ) \
DEF ("#IFDEF", stPPifDef ) \
DEF ("#IFNDEF", stPPifNdef ) \
DEF ("#ELSE", stPPelse ) \
DEF ("#ELIF", stPPelIf ) \
DEF ("#ENDIF", stPPendIf ) \
DEF ("#ERROR", stPPerror ) \
DEF ("#WARN", stPPwarn)

#define ppDefFGL \
DEF ("#PRAGMA", stPragma ) \
DEF ("#INCLUDE", stPPinclude ) \
DEF ("#IF", stPPif ) \
DEF ("#IFDEF", stPPifDef ) \
DEF ("#IFNDEF", stPPifNdef ) \
DEF ("#ELSE", stPPelse ) \
DEF ("#ELIF", stPPelIf ) \
DEF ("#ENDIF", stPPendIf ) \
DEF ("#ERROR", stPPerror ) \
DEF ("#WARN", stPPwarn)


const statementList statementName[] = {
#define DEF(a,b) {a, sizeof ( a ) - 1, statementType::b},
	statementDef
#undef DEF
	{0,				 0, statementType::stIf}				// terminator... must be here
};

const std::map<statementString, statementList> statementMap =
{
#define DEF(a,b) { statementString ( a, sizeof ( a ) - 1 ), statementList { a, sizeof ( a ) - 1, statementType::b } },
	statementDef
#undef DEF
};

const std::map<statementString, statementList> reservedMap =
{
#define DEF(a,b) { statementString ( a, sizeof ( a ) - 1 ), statementList { a, sizeof ( a ) - 1, statementType::b } },
	reservedDef
#undef DEF
};

const std::map<statementString, statementList> reservedMapFgl =
{
#define DEF(a,b) { statementString ( a, sizeof ( a ) - 1 ), statementList { a, sizeof ( a ) - 1, statementType::b } },
	reservedDefFgl
#undef DEF
};

const std::map<statementString, statementList> statementMapFGL =
{
#define DEF(a,b) { statementString ( a, sizeof ( a ) - 1 ), statementList { a, sizeof ( a ) - 1, statementType::b } },
	statementDefFGL
#undef DEF
};

const std::map<statementString, statementList> ppMap =
{
#define DEF(a,b) { statementString ( a, sizeof ( a ) - 1 ), statementList { a, sizeof ( a ) - 1, statementType::b } },
	ppDefFGL
#undef DEF
};


bool cmpTerminated ( char const *expr, char const *name, uint32_t len )
{
	if ( !memcmpi ( expr, name, len ) )
	{
		expr += len;

		// make sure we're followed by some sort of terminator to finish off the statement
		// {} are special case since they CAN have a symbol that follows them... they're always terminating
		if ( (*name == '{') || (*name == '}') || !*expr || !_issymbol ( expr ) )
		{
			return (true);
		}
		return (false);
	}
	return (false);
}

char *getLine ( source &src )
{
	BUFFER	 tmpBuff;
	char *ret;

	while ( !_isspace ( src ) && !_iseol ( src ) )
	{
		bufferPut8 ( &tmpBuff, *src );
		src++;
	}
	bufferPut8 ( &tmpBuff, 0 );

	ret = _strdup ( bufferBuff ( &tmpBuff ) );
	return (ret);
}

stringi getSymbol ( source &src )
{
	stringi buff;

	if ( !*src )
	{
		return buff;
	}

	if ( _issymbol ( src ) && !_isdigit ( src ) )
	{
		while ( _issymbolb ( src ) )
		{
			buff += *(src++);
		}
	} else
	{
		return "";
	}
	return buff;
}

stringi	buildString ( char const *preFix, char const *name, char const *postFix )
{
	stringi		str;

	if ( *preFix )
	{
		str = preFix;
		str += ".";
	}
	str += name;
	str += ".";
	str += postFix;

	return str;
}

