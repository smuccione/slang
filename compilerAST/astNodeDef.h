/*

	class and type definitions for the expression parser

*/

#define NODE_OPS \
	NODE_DEF ( btBasic )     \
	NODE_DEF ( btInlineRet )     \
	NODE_DEF ( btFor )     \
	NODE_DEF ( btForEach )     \
	NODE_DEF ( btReturn )     \
	NODE_DEF ( btLabel )     \
	NODE_DEF ( btGoto )     \
	NODE_DEF ( btInline )     \
	NODE_DEF ( btIf )     \
	NODE_DEF ( btWhile )     \
	NODE_DEF ( btRepeat )     \
	NODE_DEF ( btSwitch )     \
	NODE_DEF ( btTry )     \
	NODE_DEF ( btCont )     \
	NODE_DEF ( btBreak )     \
	NODE_DEF ( btAnnotation )     \
	NODE_DEF ( btStatement )  /*generic statement for language server use... for things we don't really need to store in the AST */   \
	NODE_DEF ( btExit )     \
	NODE_DEF ( btExcept )     \
	NODE_DEF ( singleLineComment )	\
	NODE_DEF ( commentBlock ) \
	NODE_DEF ( metadata ) \
	NODE_DEF ( symbolDef )     \
	NODE_DEF ( stringValue )     \
	NODE_DEF ( compValue )     \
	NODE_DEF ( intValue )     \
	NODE_DEF ( doubleValue )     \
	NODE_DEF ( arrayValue )     \
	NODE_DEF ( varrayValue )     \
	NODE_DEF ( codeBlockValue )     \
	NODE_DEF ( symbolValue )     \
	NODE_DEF ( nameValue )     \
	NODE_DEF ( atomValue )     \
	NODE_DEF ( nullValue )     \
	NODE_DEF ( fieldValue )     \
	NODE_DEF ( codeBlockExpression )     \
	NODE_DEF ( funcValue )     \
	NODE_DEF ( pairValue )     \
	NODE_DEF ( nilValue )     \
	NODE_DEF ( senderValue ) \
	NODE_DEF ( errorValue )  /* used when language server detects an incomplete expression and needs to generate values in order to build an AST */   \
	NODE_DEF ( warnValue )  /* used when language server detects an incomplete expression and needs to generate values in order to build an AST */   \
	NODE_DEF ( paramPack )     \
	NODE_DEF ( paramExpand )     \
	NODE_DEF ( symbolPack )     \
	NODE_DEF ( intCast )     \
	NODE_DEF ( doubleCast )     \
	NODE_DEF ( stringCast )     \
	NODE_DEF ( openPeren )     \
	NODE_DEF ( closePeren )     \
	NODE_DEF ( add )     \
	NODE_DEF ( subtract )     \
	NODE_DEF ( multiply )     \
	NODE_DEF ( divide )     \
	NODE_DEF ( modulus )     \
	NODE_DEF ( power )     \
	NODE_DEF ( max )     \
	NODE_DEF ( min )     \
	NODE_DEF ( negate )     \
	NODE_DEF ( equal )     \
	NODE_DEF ( equali )     \
	NODE_DEF ( notEqual )     \
	NODE_DEF ( less )     \
	NODE_DEF ( lessEqual )     \
	NODE_DEF ( greater )     \
	NODE_DEF ( greaterEqual )     \
	NODE_DEF ( logicalAnd )     \
	NODE_DEF ( logicalOr )     \
	NODE_DEF ( logicalNot )     \
	NODE_DEF ( bitAnd )     \
	NODE_DEF ( bitOr )     \
	NODE_DEF ( bitXor )     \
	NODE_DEF ( shiftLeft )     \
	NODE_DEF ( shiftRight )     \
	NODE_DEF ( arrayDeref )     \
	NODE_DEF ( assign )     \
	NODE_DEF ( addAssign )     \
	NODE_DEF ( subAssign )     \
	NODE_DEF ( mulAssign )     \
	NODE_DEF ( divAssign )     \
	NODE_DEF ( modAssign )     \
	NODE_DEF ( powAssign )     \
	NODE_DEF ( bwAndAssign )     \
	NODE_DEF ( bwOrAssign )     \
	NODE_DEF ( bwXorAssign )     \
	NODE_DEF ( shLeftAssign )     \
	NODE_DEF ( shRightAssign )     \
	NODE_DEF ( postInc )     \
	NODE_DEF ( postDec )     \
	NODE_DEF ( preInc )     \
	NODE_DEF ( preDec )     \
	NODE_DEF ( twosComplement )     \
	NODE_DEF ( refCreate )     \
	NODE_DEF ( refPromote )     \
	NODE_DEF ( funcCall )     \
	NODE_DEF ( sendMsg )     \
	NODE_DEF ( nsDeref )     \
	NODE_DEF ( getEnumerator )     \
	NODE_DEF ( seq )     \
	NODE_DEF ( workAreaStart )     \
	NODE_DEF ( comp )     \
	NODE_DEF ( conditional )     \
	NODE_DEF ( econditional )     \
	NODE_DEF ( inc )     \
	NODE_DEF ( dec )     \
	NODE_DEF ( lambdaExpr )     \
	NODE_DEF ( range )     \
	NODE_DEF ( selfSend )     \
	NODE_DEF ( dummy )     \
	NODE_DEF ( coalesce)		/* null coalescing*/    \
	NODE_DEF ( cSend )			/* null coalescing*/	\
	NODE_DEF ( cArrayDeref )	/* null coalescing*/    \
	NODE_DEF ( endParse )		/* MUST be last entry - used additionally for range checking validation */
