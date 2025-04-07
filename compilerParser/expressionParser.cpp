/*

	base expression parser, returns astNode expression tree

*/
#define NOMINMAX

#include "fileParser.h"

#define OPERATORS \
OP_DEF ( "()",			opUnary,			funcCall,		195,	195,	1, 0, 0, ovFuncCall )	\
OP_DEF ( "(",			opPeren,			openPeren,		  0,	990,	0, 0, 1, ovNone )	\
OP_DEF ( ")",			opPeren,			closePeren,		  0,	  0,	0, 0, 1, ovNone )	\
OP_DEF ( "(int)",		opUnary,			intCast,		180,	180,	0, 0, 0, ovNone )	\
OP_DEF ( "(double)",	opUnary,			doubleCast,		180,	180,	0, 0, 0, ovNone )	\
OP_DEF ( "(string)",	opUnary,			stringCast,		180,	180,	0, 0, 0, ovNone )	\
OP_DEF ( "&&",			opBinary,			logicalAnd,		 40,	 40,	1, 1, 1, ovNone )	\
OP_DEF ( "->",			opBinary,			workAreaStart,	194,	194,	1, 1, 1, ovNone )	\
OP_DEF ( "&=",			opBinary,			bwAndAssign,	 20,	 20,	0, 1, 1, ovNone )	\
OP_DEF ( "-=",			opBinary,			subAssign,		 20,	 20,	0, 1, 1, ovSubAssign )	\
OP_DEF ( "&",			opBWAndCompile,		dummy,			193,	193,	1, 1, 0, ovNone )	\
OP_DEF ( "--",			opPrePost,			dec,			185,	185,	1, 1, 0, ovNone )	\
OP_DEF ( "++",			opPrePost,			inc,			185,	185,	1, 1, 0, ovNone )	\
OP_DEF ( "-",			opSubNegate,		dummy,			140,	140,	0, 1, 0, ovNegate )	\
OP_DEF ( "::",			opNSSelf,			selfSend,		210,	210,	0, 0, 0, ovNone )	\
OP_DEF ( "+=",			opBinary,			addAssign,		 20,	 20,	0, 1, 1, ovAddAssign )	\
OP_DEF ( "*=",			opBinary,			mulAssign,		 20,	 20,	0, 1, 1, ovMulAssign )	\
OP_DEF ( "/=",			opBinary,			divAssign,		 20,	 20,	0, 1, 1, ovDivAssign )	\
OP_DEF ( "%=",			opBinary,			modAssign,		 20,	 20,	0, 1, 1, ovModAssign )	\
OP_DEF ( "**=",			opBinary,			powAssign,		 20,	 20,	0, 1, 1, ovPowAssign )	\
OP_DEF ( "|=",			opBinary,			bwOrAssign,		 20,	 20,	0, 1, 1, ovBwAndAssign )	\
OP_DEF ( "^=",			opBinary,			bwXorAssign,	 20,	 20,	0, 1, 1, ovBwOrAssign )	\
OP_DEF ( "+",			opBinary,			add,			140,	140,	1, 1, 1, ovAdd )	\
OP_DEF ( "-",			opBinary,			subtract,		140,	140,	1, 1, 1, ovSubtract )	\
OP_DEF ( "**",			opBinary,			power,			160,	160,	1, 1, 1, ovPower )	\
OP_DEF ( "*",			opBinary,			multiply,		160,	160,	1, 1, 1, ovMultiply )	\
OP_DEF ( "/",			opBinary,			divide,			160,	160,	1, 1, 1, ovDivide )	\
OP_DEF ( "%",			opBinary,			modulus,		160,	160,	1, 1, 1, ovModulus )	\
OP_DEF ( ":>",			opBinary,			max,			 27,	 27,	1, 1, 1, ovMax )	\
OP_DEF ( ":<",			opBinary,			min,			 27,	 27,	1, 1, 1, ovMin )	\
OP_DEF ( "=>",			opBinary,			lambdaExpr, 	 20,	 20,	0, 0, 1, ovNone )	\
OP_DEF ( "==",			opBinary,			equal,			100,	100,	1, 1, 1, ovEqual )	\
OP_DEF ( "~==",			opBinary,			equali,			100,	100,	1, 1, 1, ovEqualA )	\
OP_DEF ( "!=",			opBinary,			notEqual,		100,	100,	1, 1, 1, ovNotEqual )	\
OP_DEF ( "<<=",			opBinary,			shLeftAssign,	 20,	 20,	0, 1, 1, ovNone )	\
OP_DEF ( ">>=",			opBinary,			shRightAssign,	 20,	 20,	0, 1, 1, ovNone )	\
OP_DEF ( "<<",			opBinary,			shiftLeft,		120,	120,	1, 1, 1, ovNone )	\
OP_DEF ( ">>",			opBinary,			shiftRight,		120,	120,	1, 1, 1, ovNone )	\
OP_DEF ( "<=",			opBinary,			lessEqual,		100,	100,	1, 1, 1, ovLessEqual )	\
OP_DEF ( "<>",			opBinary,			notEqual,		100,	100,	1, 1, 1, ovNotEqual )	\
OP_DEF ( "<",			opBinary,			less,			100,	100,	1, 1, 1, ovLess )	\
OP_DEF ( ">=",			opBinary,			greaterEqual,	100,	100,	1, 1, 1, ovGreaterEqual )	\
OP_DEF ( ">",			opBinary,			greater,		100,	100,	1, 1, 1, ovGreater )	\
OP_DEF ( "||",			opBinary,			logicalOr,		 30,	 30,	1, 1, 1, ovNone )	\
OP_DEF ( "^",			opBinary,			bitXor,			 60,	 60,	1, 1, 1, ovBitXor )	\
OP_DEF ( "|",			opBinary,			bitOr,			 60,	 60,	1, 1, 1, ovBitOr )	\
OP_DEF ( "=",			opBinary,			assign,			 20,	 20,	0, 1, 1, ovAssign )	\
OP_DEF ( "...",			opPack,				paramPack,		185,	185,	1, 0, 0, ovNone )	\
OP_DEF ( "...",			opPack,				paramExpand,	185,	185,	0, 0, 0, ovNone )	\
OP_DEF ( "..",			opBinary,			range,			 30,	 30,    1, 0, 0, ovNone )	\
OP_DEF ( ".",			opBinary,			sendMsg,		195,	195,	1, 0, 0, ovNone )	\
OP_DEF ( "&",			opBinary,			bitAnd,			 80,	 80,	1, 1, 1, ovBitAnd )	\
OP_DEF ( "-",			opUnary,			negate,			170,	170,	0, 1, 0, ovNegate )	\
OP_DEF ( "&",			opUnary,			comp,			193,	193,	0, 1, 0, ovNone )	\
OP_DEF ( "[",			opUnary,			arrayDeref,		195,	195,	1, 1, 1, ovArrayDerefRHS )	\
OP_DEF ( "[",			opBinary,			arrayDeref,		195,	195,	1, 1, 2, ovArrayDerefLHS )	\
OP_DEF ( "++",			opUnary,			postInc,		185,	185,	1, 1, 0, ovPostInc )	\
OP_DEF ( "--",			opUnary,			postDec,		185,	185,	1, 1, 0, ovPostDec )	\
OP_DEF ( "++",			opUnary,			preInc,			170,	170,	0, 1, 0, ovPreInc )	\
OP_DEF ( "--",			opUnary,			preDec,			170,	170,	0, 1, 0, ovPreDec )	\
OP_DEF ( "~",			opUnary,			twosComplement,	170,	170,	0, 1, 0, ovTwosComplement )	\
OP_DEF ( "!",			opUnary,			logicalNot,		170,	170,	0, 1, 0, ovNot )	\
OP_DEF ( "@",			opUnary,			refCreate,		170,	170,	0, 0, 0, ovRefCreate )	\
OP_DEF ( "?",			opUnary,			conditional,	 25,	 25,	0, 0, 0, ovNone )	\
OP_DEF ( "??",			opBinary,			coalesce,		 26,	 26,	0, 0, 0, ovNone )	\
OP_DEF ( "?.",			opBinary,			cSend,			195,	195,	0, 0, 0, ovNone )	\
OP_DEF ( "?[",			opBinary,			cArrayDeref,	195,	195,	0, 0, 0, ovNone )	\
OP_DEF ( ":",			opUnary,			econditional,	 25,	 25,	0, 0, 0, ovNone )	\
OP_DEF ( ":",			opBinary,			pairValue,		 11,	 11,	0, 0, 0, ovNone )	\
OP_DEF ( ",",			opUnary,			seq,			 10,	 10,	1, 0, 0, ovNone )	\
OP_DEF ( "}",			opUnary,			endParse,		  0,	  0,	0, 0, 0, ovNone )	\
OP_DEF ( "]",			opUnary,			endParse,		  0,	  0,	0, 0, 0, ovNone )	\
OP_DEF ( ";",			opUnary,			endParse,		  0,	  0,	0, 0, 0, ovNone )	\
OP_DEF ( ";",			opPeren,			errorValue,		300,	300,	0, 0, 1, ovNone )

#define OP_DEF( token, type, op, stackPriority, infixPriority, isLeaf, isOverloadable, nParams, overloadOp ) {token, astOpCat::type, token ? strlen ( token ) : 0, astOp::op, stackPriority, infixPriority, isLeaf, isOverloadable, nParams, fgxOvOp::overloadOp},
opListDef opList[] = {
	OPERATORS
	OP_DEF ( 0,			opUnary,			endParse,		  0,	  0,	0, 0, 0, ovNone )								// mark end of table
};

std::vector<enum fgxOvOp>	opToOvXref;
static bool initMe = []( )
{
#undef NODE_DEF
#define NODE_DEF(op) opToOvXref.push_back ( fgxOvOp::ovNone );
	NODE_OPS
#undef OP_DEF
#define OP_DEF( token, type, op, stackPriority, infixPriority, isLeaf, isOverloadable, nParams, overloadOp ) opToOvXref[static_cast<size_t>(astOp::op)] = fgxOvOp::overloadOp;
		OPERATORS
		return true;
}();

std::vector<opListDef *> opToOpDef;

static bool initMe2 = []( )
{
	opToOpDef.resize ( (size_t) astOp::endParse + 1 );

	for ( size_t loop = 0; loop < sizeof ( opList ) / sizeof ( opList[0] ); loop++ )
	{
		if ( !opToOpDef[(size_t) opList[loop].op] )
		{
			opToOpDef[(size_t) opList[loop].op] = &opList[loop];
		}
	}
	return true;
}();

static struct
{
	char const *str;
	uint32_t	 len;
} linqTermList[] = {
						{
							"end",			 3,
						},
						{
							"from",			 4,
						},
						{
							"in",			 2,
						},
						{
							"join",			 4,
						},
						{
							"on",			 2,
						},
						{
							"by",			 2,
						},
						{
							"equals",		 6,
						},
						{
							"into",			 4,
						},
						{
							"let",			 3,
						},
						{
							"where",		 5,
						},
						{
							"orderby",		 7,
						},
						{
							"order",		 5,
						},
						{
							"ascending",	 9,
						},
						{
							"descending",	10,
						},
						{
							"select",		 6,
						},
						{
							"group",		 5,
						},
						{
							0,				 0
						}
};

opStack::~opStack ( )
{
	while ( !elem.empty ( ) )
	{
		auto node = elem.front ( );
		if ( node ) delete node;
		elem.pop_front ( );
	}
}

void opStack::push ( astNode *node )
{
	elem.push_back ( node );
}

astNode *opStack::popBack ( ) noexcept
{
	astNode *node;

	node = elem.back ( );
	elem.pop_back ( );
	return (node);
}


astNode *opStack::popFront ( ) noexcept
{
	astNode *node;

	node = elem.front ( );
	elem.pop_front ( );
	return node;
}

bool opStack::empty ( ) noexcept
{
	return elem.empty ( );
}

int opStack::num ( ) noexcept
{
	return (int) elem.size ( );
}

struct sCONDITIONAL
{
	opStack *truePtr;
	opStack *falsePtr;
};

astNode *opFile::getNode ( source &src, lastOpType lastOp, opFunction *func, bool doSlang, bool isLS, bool isAP )
{
	if (!*src)
	{
		return (0);
	}

	auto node = new astNode ( sCache, astOp::nullValue, srcLocation ( src ) );

	try
	{
		if ( (*src == '\"') || (src[0] == '$' && src[1] == '"') || (src[0] == 'R' && src[1] == '"') || (src[0] == 'F' && src[1] == '"') || (src[0] == '$' && src[1] == 'R' && src[2] == '"') )
		{
			astNode		*retNode = node;
			bool		 interpolationMode = false;
			bool		 multiLine = false;
			bool		 fileContentsMode = false;
			bool		 isRaw = false;
			BUFFER		 multiLineTerminator;
			srcLocation	 regionStart = src;
			BUFFER		 buff;

			node->setOp ( sCache, astOp::stringValue );

			if ( src[0] == 'F' )
			{
				fileContentsMode = true;
				isRaw = true;
				src++;
			} else
			{
				if ( src[0] == '$' )
				{
					interpolationMode = true;
					src++;
				}
				if ( src[0] == 'R' )
				{
					multiLine = true;
					isRaw = true;
					src++;
				}
			}

			// remove "
			src++;

			if ( multiLine )
			{
				multiLineTerminator.put ( ')' );	//we're building a terminator which ends with ")<text>"
				// build the <text> part
				while ( src[0] != '(' )
				{
					if ( !src[0] || src[0] == '(' )
					{
						break;
					}
					multiLineTerminator.put ( *(src++) );
				}
				src++;	// consume the '('
				// add in the final terminating "
				multiLineTerminator.put ( '"' );
			}

			while ( *src )
			{
				if ( !multiLine && src[0] == '"' )
				{
					break;
				} else if ( multiLine && !memcmp ( src, multiLineTerminator.data<char *> (), multiLineTerminator.size () ))
				{
					// consume the terminator
					src += multiLineTerminator.size ();
					break;
				}
				if ( !isRaw && *src == '\\' )
				{
					src++;
					switch ( *src )
					{
						case '\r':
						case '\n':
							if ( !isLS )  throw errorNum::scILLEGAL_STRING_EXPANSION;
							node->setError ( errorNum::scILLEGAL_STRING_EXPANSION );
							node->setEnd ( src );
							errHandler.setFatal ();
							return node;
						case 'r':
							bufferPut8 ( &buff, '\r' );
							break;
						case 'n':
							bufferPut8 ( &buff, '\n' );
							break;
						case 't':
							bufferPut8 ( &buff, '\t' );
							break;
						case '\"':
							if ( multiLine ) // embedded " are allowed in multiline comments as an end quote is )" in multiline
							{
								bufferPut8 ( &buff, '\\' );
								bufferPut8 ( &buff, src[0] );
							} else
							{
								bufferPut8 ( &buff, src[0] );
							}
							break;
						case '\\':
						case '\'':
							bufferPut8 ( &buff, *src );
							break;
						case 'a':
							bufferPut8 ( &buff, '\a' );
							break;
						case 'b':
							bufferPut8 ( &buff, '\b' );
							break;
						case 'e':
							bufferPut8 ( &buff, (char) 27 );
							break;
						case 'f':
							bufferPut8 ( &buff, '\f' );
							break;
						case 'v':
							bufferPut8 ( &buff, '\v' );
							break;
						case 'z':
							bufferPut8 ( &buff, (char) 26 );
							break;
						case '?':
							bufferPut8 ( &buff, '\?' );
							break;
						case '0':
							bufferPut8 ( &buff, 0 );
							break;
						case 'x':
						case 'X':
							if ( _ishex ( (src + 1) ) && _ishex ( (src + 2) ) )
							{
								src++;	// skip over the x
								char tmpHex[3] = { *(src++),  *(src), 0 };

								bufferPut8 ( &buff, (uint8_t) std::stoi ( std::string ( tmpHex ), nullptr, 16 ) );
							}
							break;
						default:
							{
								errorLocality e ( &errHandler, src );
								errHandler.throwWarning ( isLS, warnNum::scwILLEGAL_ESCAPE_SEQUENCE, *src );
								bufferPut8 ( &buff, '\\' );
								bufferPut8 ( &buff, *src );
							}
							break;
					}
					src++;
				} else
				{
					switch ( *src )
					{
						case '\r':
						case '\n':
							if ( multiLine )
							{
								bufferPut8 ( &buff, *(src++) );
							} else
							{
								if ( !isLS ) throw errorNum::scILLEGAL_STRING_EXPANSION;
								node->setError ( errorNum::scILLEGAL_STRING_EXPANSION );
								node->setEnd ( src );
								errHandler.setFatal ();
								return node;
							}
							break;
						case '{':
							if ( (src[1] == '{') || !interpolationMode )
							{
								// {{   is an override to allow { to be inserted
								bufferPut8 ( &buff, '{' );
								src++;
							} else
							{
								src++;
								regionStart.setEnd ( src );

								bufferPut8 ( &buff, 0 );
								node->setOp ( sCache, astOp::stringValue );
								node->setLocation ( regionStart );
								node->stringValue() = bufferBuff ( &buff );
								bufferReset ( &buff );

								bool needStrInterpolate = false;
								// in this case this is what we do
								//			retNode = node(+)
								//	node				$strInterpolate( value, alignment, formatstring )
								retNode = new astNode ( sCache, astOp::add, retNode, new astNode ( sCache, astOp::funcCall, new astNode ( sCache, astOp::atomValue, "$strInterpolate" ), (astNode *)0 ) );

								retNode->right->pList ().paramRegion.push_back ( src );
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								retNode->right->pList().param.push_back ( _parseExpr ( src, false, false, false, true, false, func, doSlang, isLS, isAP ) );
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								retNode->right->pList().paramRegion.back().setEnd ( src );

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									needStrInterpolate = true;
									src++;
									retNode->right->pList ().paramRegion.push_back ( src );
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									retNode->right->pList().param.push_back ( _parseExpr ( src, false, false, false, true, false, func, doSlang, isLS, isAP ) );
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									retNode->right->pList ().paramRegion.back ().setEnd ( src );
								} else
								{
									retNode->right->pList ().paramRegion.push_back ( src );
									retNode->right->pList().param.push_back ( (new astNode ( sCache, astOp::intValue, (int64_t) 0 ))->setLocation ( src ) );
								}
								if ( *src == ':' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::colon, src ) );
									needStrInterpolate = true;
									src++;
									srcLocation stringLocation = src;

									retNode->right->pList ().paramRegion.push_back ( src );
									while ( *src && *src != '}' )
									{
										bufferPut8 ( &buff, *(src++) );
									}
									stringLocation.setEnd ( src );
									bufferPut8 ( &buff, 0 );
									retNode->right->pList().param.push_back ( (new astNode ( sCache, astOp::stringValue, bufferBuff ( &buff ) ))->setLocation ( stringLocation ) );
									retNode->right->pList ().paramRegion.back ().setEnd ( src );
								}
								if ( *src != '}' ) 
								{
									if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
									retNode->setError ( errorNum::scILLEGAL_EXPRESSION );
									retNode->setEnd ( src );
									errHandler.setFatal ();
									return retNode;
								}
								regionStart = src;
								src++;

								if ( !needStrInterpolate )
								{
									auto value = retNode->right->pList().param[0];
									retNode->right->pList().param[0] = nullptr;
									delete retNode->right;
									retNode->right = value;
								}

								// more string to process... create another node to read fixed string into
								retNode = new astNode ( sCache, astOp::add, retNode, new astNode ( sCache, astOp::stringValue ) );
								node = retNode->right;
								retNode->right->setLocation ( regionStart );
							}
							break;
						default:
							bufferPut8 ( &buff, *(src++) );
							break;
					}
				}
			}
			if ( *src )
			{
				src++;

				regionStart.setEnd ( src );

				if ( node->getOp ( ) == astOp::stringValue )
				{
					node->setLocation ( regionStart );
					bufferPut8 ( &buff, 0 );
					if ( fileContentsMode )
					{
						std::ifstream ifs ( buff.data<char const *>());
						if ( ifs.is_open () )
						{
							std::string content ( (std::istreambuf_iterator<char> ( ifs )),
												  (std::istreambuf_iterator<char> ()) );
							node->stringValue() = content;
						} else
						{
							if ( !isLS ) throw GetLastError ();
							retNode->setError ( (errorNum) GetLastError () );
							retNode->setEnd ( src );
							errHandler.setFatal ();
							return retNode;
						}
					} else
					{
						node->stringValue() = bufferBuff ( &buff );
					}
				}

				if ( retNode->getOp ( ) != astOp::stringValue )
				{
					retNode = new astNode ( sCache, astOp::compValue, retNode, (astNode *)0 );
				}
				node = retNode;
			} else
			{
				if ( !isLS )throw errorNum::scMISSING_QUOTE;
				retNode->setError ( errorNum::scMISSING_QUOTE );
				retNode->setEnd ( src );
				errHandler.setFatal ();
				return retNode;
			}
		} else if ( *src == '\'' )
		{
			BUFFER		 buff;

			// simple string
			src++;
			while ( *src != '\'' )
			{
				switch ( *src )
				{
					case '\r':
					case '\n':
						if ( !isLS ) throw errorNum::scMISSING_QUOTE;
						node->setError ( errorNum::scMISSING_QUOTE );
						node->setEnd ( src );
						errHandler.setFatal ();
						return node;
					default:
						bufferPut8 ( &buff, *src );
						break;
				}
				src++;
			}
			if ( *src )
			{
				src++;
			}
			bufferPut8 ( &buff, 0 );
			node->setOp ( sCache, astOp::stringValue );
			node->stringValue() = bufferBuff ( &buff );
		} else if ( (*src == '0') && ((*(src + 1) == 'x') || (*(src + 1) == 'X')) )
		{
			BUFFER		 buff;
			src += 2;

			while ( _ishex ( src ) || *src == '_' )
			{
				bufferPut8 ( &buff, *(src++) );
			}
			bufferPut8 ( &buff, 0 );

			node->setOp ( sCache, astOp::intValue );
			node->intValue() = (int64_t)std::stoull ( std::string ( bufferBuff ( &buff ) ), nullptr, 16 );
		} else if ( (*src == '0') && ((*(src + 1) == 'b') || (*(src + 1) == 'B')) )
		{
			BUFFER		 buff;
			src += 2;

			while ( *src == '0' || *src == '1' || *src == '_' )
			{
				bufferPut8 ( &buff, *(src++) );
			}
			bufferPut8 ( &buff, 0 );

			node->setOp ( sCache, astOp::intValue );
			node->intValue() = (int64_t)std::stoull ( std::string ( bufferBuff ( &buff ) ), nullptr, 2 );
		} else if ( (lastOp != lastOpType::symbol) && ((_isdigit ( src ) && (*src !='.')) || (((*src == '+') || (*src == '-') || (*src == '.')) && _isdigit ( src + 1 ))) )
		{
			BUFFER	buff;
			int		op = false;
			bool	decPresent = false;

			if ( ((*src == '-') && (*(src + 1) == '-')) ||
				 ((*src == '+') && (*(src + 1) == '+')) )
			{
				bufferPut8 ( &buff, *(src++) );
				bufferPut8 ( &buff, *(src++) );
				bufferPut8 ( &buff, 0 );
			} else
			{
				while ( _isnum ( src ) )
				{
					switch ( *src )
					{
						case '_':
							BS_ADVANCE ( src );
							break;
						case '+':
						case '-':
							if ( op )
							{
								goto done;
							}
							op |= 2;
							BS_ADVANCE ( src );
							break;
						case '.':
							if ( *(src + 1) == '.' )
							{
								// range operator
								goto done;
							}
							if ( decPresent )
							{
								if ( op > 1 )
								{
									goto done;
								}
							}
							decPresent = true;
							break;
					}
					bufferPut8 ( &buff, *(src++) );
					op |= 1;
				}

			done:
				bufferPut8 ( &buff, 0 );
			}
			if ( decPresent )
			{
				node->setOp ( sCache, astOp::doubleValue );
				node->doubleValue() = atof ( bufferBuff ( &buff ) );
			} else
			{
				node->setOp ( sCache, astOp::intValue );
				node->intValue() = _atoi64 ( bufferBuff ( &buff ) );
			}
		} else if ( (_issymbol ( src ) && !_isdigit ( src )) || (doSlang && src[0] == ':' && src[1] == ':' ) )
		{
			BUFFER		 buff;

			if ( lastOp != lastOpType::send )
			{
				if ( doSlang )
				{
					// this would be nice for symbols...
					auto it = reservedMap.find ( statementString ( src ) );
					if ( it != reservedMap.end () )
					{
						delete node;
						return nullptr;
					}
				} else
				{
					// this would be nice for symbols...
					auto it = reservedMapFgl.find ( statementString ( src ) );
					if ( it != reservedMapFgl.end () )
					{
						delete node;
						return nullptr;
					}
				}
			}

			// process symbol
			if ( src[0] == ':' && src[1] == ':' )
			{
				bufferPut8 ( &buff, ':' );
				bufferPut8 ( &buff, ':' );
				src += 2;
				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
			}
			while ( _issymbolb ( src ) )
			{
				bufferPut8 ( &buff, *(src++) );
				node->setEnd ( src );
				if ( !_issymbolb ( src ) )
				{
					if ( src[0] == ':' && src[1] == ':' )
					{
						bufferPut8 ( &buff, ':' );
						bufferPut8 ( &buff, ':' );
						src += 2;
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					} else
					{
						break;
					}
				}
			}
			if ( !buff.size () )
			{
				if ( !isLS ) throw errorNum::scILLEGAL_IDENTIFIER;
				node->setError ( errorNum::scILLEGAL_IDENTIFIER );
				errHandler.setFatal ();
			} else
			{
				bufferPut8 ( &buff, 0 );
				node->setOp ( sCache, astOp::symbolValue );
				node->symbolValue () = sCache.get ( buff.data<char const *>() );

				if ( !strccmp ( node->symbolValue (), "_" ) )
				{
					node->setOp ( sCache, astOp::nilValue );
				} else if ( !strccmp ( node->symbolValue (), "null" ) || !strccmp ( node->symbolValue (), "nil" ) )
				{
					node->setOp ( sCache, astOp::nullValue );
				} else if ( !strccmp ( node->symbolValue (), "true" ) )
				{
					node->setOp ( sCache, astOp::intValue );
					node->intValue () = 1;
				} else if ( !strccmp ( node->symbolValue (), "false" ) )
				{
					node->setOp ( sCache, astOp::intValue );
					node->intValue () = 0;
				} else if ( !strccmp ( node->symbolValue (), "sender" ) )
				{
					node->setOp ( sCache, astOp::senderValue );
				} else if ( !strccmp ( node->symbolValue (), "from" ) && doSlang )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, node->location ) );

					if ( lastOp == lastOpType::symbol )
					{
						if ( !isLS ) throw errorNum::scILLEGAL_ASSIGNMENT;
						node->setError ( errorNum::scILLEGAL_ASSIGNMENT );
						errHandler.setFatal ();
						return node;
					}
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeLinq, src ) );
					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					auto newNode = getLinq ( src, func, isLS );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterLinq, src ) );
					delete node;
					node = newNode;
				} else if ( !strccmp ( node->symbolValue (), "new" ) )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::newOperator, *node ) );

					astNode *newNode;

					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					if ( *src == '{' )
					{
						// anonymous type construction
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::startAnonymousClass, src ) );
						src++;
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, src ) );

						opClass *classDef = new opClass ();
						classDef->isInterface = false;
						classDef->usingList = ns.getUseList ();
						classDef->location = node->location;
						classDef->nameLocation = node->location;

						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

						char tmpName[512]{};
						sprintf_s ( tmpName, sizeof ( tmpName ), "anonymous$%s@%i:%u", srcFiles.getName ( node->location.sourceIndex ).c_str (), node->location.lineNumberStart, node->location.columnNumber );

						classDef->name = sCache.get ( tmpName );
						std::vector<std::pair<cacheString, astNode *>> elements;

						// anonymous type
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

						while ( 1 )
						{
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							newNode = _parseExpr ( src, false, false, false, false, true, func, doSlang, isLS, isAP );
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							if ( newNode->getOp () == astOp::assign || newNode->getOp () == astOp::pairValue )
							{
								if ( newNode->left->getOp () != astOp::symbolValue )
								{
									if ( !isLS ) throw errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME, src ) );
									errHandler.setFatal ();
								}
								elements.push_back ( { newNode->left->symbolValue (), newNode->right } );
								newNode->left = nullptr;
								newNode->right = nullptr;
								delete newNode;
							} else if ( newNode->getOp () == astOp::sendMsg )
							{
								if ( newNode->right->getOp () != astOp::symbolValue )
								{
									if ( !isLS ) throw errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME, src ) );
									errHandler.setFatal ();
								}
								elements.push_back ( { newNode->right->symbolValue (), newNode } );
							} else
							{
								if ( !isLS ) throw errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME;
								errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME, src ) );
								elements.push_back ( { sCache.get ( "error" ), newNode } );
								errHandler.setFatal ();
							}
							if ( *src == '}' )
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endAnonymousClass, src ) );
								src++;
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
								break;
							}
							if ( *src != ',' )
							{
								if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
								newNode = new astNode ( errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME, src );
								src.advanceGarbage ();
								newNode->setEnd ( src );
								elements.push_back ( { sCache.get ( "" ), newNode } );
								errHandler.setFatal ();
								errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME, src ) );
								goto anonymous_done;
							}
							src++;
						}
						if ( elements.empty () )
						{
							if ( !isLS ) throw errorNum::scINVALID_ANONYMOUS_TYPE_EMPTY;
							newNode = new astNode ( errorNum::scINVALID_ANONYMOUS_TYPE_EMPTY, src );
							src.advanceGarbage ();
							newNode->setEnd ( src );
							elements.push_back ( { sCache.get ( "" ), newNode } );
							errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_ANONYMOUS_TYPE_FIELD_NAME, src ) );
							errHandler.setFatal ();
						}

						anonymous_done:

						for ( auto &it : elements )
						{
							auto elem = new opClassElement ();
							elem->isVirtual = false;
							elem->isStatic = false;
							elem->type = fgxClassElementType::fgxClassType_iVar;
							elem->scope = fgxClassElementScope::fgxClassScope_public;
							elem->location = classDef->location;
							elem->data.iVar.initializer = new astNode ( sCache, astOp::symbolValue, it.first );
							elem->name = it.first;
							classDef->elems.push_back ( elem );
						}

						// construct the NEW method
						auto elem = new opClassElement ();
						elem->isVirtual = false;
						elem->isStatic = false;
						elem->type = fgxClassElementType::fgxClassType_method;
						elem->scope = fgxClassElementScope::fgxClassScope_public;
						elem->location = classDef->location;
						elem->name = newValue;

						BUFFER buff;
						buff += "( ";
						auto first = true;
						for ( auto &it : elements )
						{
							if ( !first )
							{
								buff += ", ";
							}
							first = false;
							buff += it.first.c_str ();
						}
						buff += ")";

						// initializer list for the NEW
						buff += " : ";
						first = true;
						for ( auto &it : elements )
						{
							if ( !first )
							{
								buff += ", ";
							}
							first = false;
							buff += it.first.c_str ();
							buff += " ( ";
							buff += it.first.c_str ();
							buff += " ) ";
						}
						// body
						buff += "{}\r\n";
						buff.put ( (char)0 );

						// now compile it
						auto expr = buff.data<char *> ();
						source fSrc ( &srcFiles, sCache, srcFiles.getName ( classDef->location.sourceIndex ), expr, classDef->location.lineNumberStart );
						auto newFunc = parseMethod ( fSrc, classDef, buildString ( classDef->name.c_str (), elem->name, "method" ).c_str (), true, isLS, isAP, node->location );
						elem->data.method.func = newFunc->name;
						elem->data.method.virtOverrides.insert ( newFunc );
						functionList.insert( { newFunc->name, newFunc } );
						newFunc->isInterface = false;
						newFunc->classDef = classDef;
						classDef->elems.push_back ( elem );

						node->setOp ( sCache, astOp::funcCall );
						node->left = (new astNode ( sCache, astOp::atomValue, "new" ))->setLocation ( node );

						node->pList ().param.push_back ( (new astNode ( sCache, astOp::nameValue, classDef->name ))->setLocation ( node ) );
						node->pList ().paramRegion.push_back ( *node );
						for ( auto &it : elements )
						{
							node->pList ().param.push_back ( it.second );
							node->pList ().paramRegion.push_back ( it.second->location );
						}
						classList.insert( { classDef->name, classDef } );
					} else if ( *src != '(' )
					{
						node->setOp ( sCache, astOp::funcCall );
						node->setLocation ( srcLocation() );

//						node->pList ().paramRegion.push_back ( src );
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
						newNode = getNode ( src, lastOpType::binary, func, doSlang, isLS, isAP );

						if ( !newNode || newNode->getOp () != astOp::symbolValue )
						{
							if ( !isLS ) throw errorNum::scILLEGAL_OPERAND;
							if ( !newNode )
							{
								newNode = new astNode ( errorNum::scILLEGAL_OPERAND, src );
							} else
							{
								newNode->setError ( errorNum::scILLEGAL_OPERAND );
							}
							errHandler.setFatal ();
						} else
						{
							*newNode = *astNode ( sCache, astOp::nameValue, newNode->symbolValue () ).setLocation ( newNode );
						}
						node->pList ().param.push_back ( newNode );
						BS_ADVANCE ( src );
//						node->pList ().paramRegion.back ().setEnd ( src );

						if ( *src == '(' )
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
							src++;
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, src ) );

							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							while ( *src != ')' )
							{
								node->pList ().paramRegion.push_back ( src );
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								newNode = _parseExpr ( src, false, false, false, false, false, func, doSlang, isLS, isAP );
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								node->pList ().paramRegion.back ().setEnd ( src );

								if ( !newNode )
								{
									newNode = new astNode ( sCache, astOp::nullValue );
								}
								node->pList ().param.push_back ( newNode );

								if ( (*src != ',') && (*src != ')') )
								{
									if ( !isLS )
									{
										delete newNode;
										throw errorNum::scINVALID_PARAMETER;
									}
									newNode->setError ( errorNum::scINVALID_PARAMETER );
									src.advanceGarbage ();
									newNode->setEnd ( src );
									errHandler.setFatal ();
								}
								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
								}
							}

							if ( *src != ')' )
							{
								if ( !isLS )
								{
									delete newNode;
									throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
								}
								newNode->setError ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN );
								src.advanceGarbage ();
//								newNode->setEnd ( src );
								errHandler.setFatal ();
							}
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
							src++;
						}
						if ( !node->pList ().param.size () )
						{
							if ( !isLS ) throw errorNum::scMISSING_CLASSNAME;
							node->pList ().param.push_back ( new astNode ( errorNum::scMISSING_CLASSNAME, src ) );
							node->pList ().paramRegion.push_back ( src );
							errHandler.setFatal ();
						}
						if ( !doSlang && node->pList ().param[0]->getOp () == astOp::nameValue )
						{
							// HACK: convert session to webSession for FGL code for backwards compatability
							if ( node->pList ().param[0]->nameValue () == "session" )
							{
								*node->pList ().param[0] = *astNode ( sCache, astOp::nameValue, sCache.get ( "webSession" ) ).setLocation ( node->pList ().param[0] );
							}
						}
						node->left = (new astNode ( sCache, astOp::atomValue, "new" ))->setLocation ( node );
					}
					return node;
				} else if ( !strccmp ( node->symbolValue (), "function" ) )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::function, *node ) );

					char tmpName[512]{};

					sprintf_s ( tmpName, sizeof ( tmpName ), LAMBDA_ID "%s@%i:%u", srcFiles.getName ( src.getSourceIndex () ).c_str (), src.getLine (), src.getColumn () );

					auto bound = parseFunc ( src, tmpName, doSlang, isLS, isAP, node->location );
					bound->isLambda = true;
					bound->nameLocation = node->location;
					addElem ( bound, symVariantType, true );
					node->adjustLocation ( bound->location );	// make sure the location encloses the name
					node->setOp ( sCache, astOp::funcValue );
					node->symbolValue () = sCache.get ( tmpName );
					if ( func )
					{
						bound->parentClosure = ns.makeName ( func->name );
					}
				}
			}
			return node;
		} else if ( *src == '(' )
		{
			if ( (lastOp != lastOpType::symbol) )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
				src++;
				srcLocation perenLocSave = src;

				node->setOp ( sCache, astOp::openPeren );

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

				if ( !strccmp ( src, "integer" ) )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, perenLocSave ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 7 ) );
					src += 7;

					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					if ( *src == ')' )
					{
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
						src++;
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
						node->setOp ( sCache, astOp::intCast );
					}
				} else if ( !strccmp ( src, "int" ) )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, perenLocSave ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 3 ) );
					src += 3;

					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					if ( *src == ')' )
					{
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
						src++;
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
						node->setOp ( sCache, astOp::intCast );
					}
				} else if ( !strccmp ( src, "double" ) )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, perenLocSave ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 6 ) );
					src += 6;

					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					if ( *src == ')' )
					{
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
						src++;
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
						node->setOp ( sCache, astOp::doubleCast );
					}
				} else if ( !strccmp ( src, "string" ) )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, perenLocSave ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 6 ) );
					src += 6;

					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					if ( *src == ')' )
					{
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
						src++;
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
						node->setOp ( sCache, astOp::stringCast );
					}
				}
			} else
			{
				node->setOp ( sCache, astOp::funcCall );

				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::funcCallStart, src ) );
				src++;
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, src ) );

				while ( *src != ')' )
				{
					astNode *newNode;

					node->pList ().paramRegion.push_back ( src );
					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

					if ( *src == ')' )
					{
						break;
					}
					newNode = _parseExpr ( src, false, false, false, false, false, func, doSlang, isLS, isAP );
					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					node->pList ().paramRegion.back ().setEnd ( src );

					if ( !newNode )
					{
						newNode = new astNode ( sCache, astOp::nullValue );
					}
					node->pList ().param.push_back ( newNode );

					if ( (*src != ',') && (*src != ')') )
					{
						if ( !isLS ) throw errorNum::scINVALID_PARAMETER;
						node->pList ().param.push_back ( new astNode ( errorNum::scINVALID_PARAMETER, src ) );		// push an error value for display in language server
						node->pList ().paramRegion.back ().setEnd ( src );
						errHandler.setFatal ();
						return node;
					}
					if ( *src == ',' )
					{
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ));
						src++;
						if ( isLS )
						{
							if ( *src == ')' )
							{
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::funcCallEnd, src ));
//								newNode = (new astNode ( sCache, astOp::nilValue ))->setLocation ( src );
//								node->pList ().param.push_back ( newNode );
								src++;
							}
						}
					}
				}

				if ( *src != ')' )
				{
					if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
					node->pList ().param.push_back ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );		// push an error value for display in language server
					node->pList ().paramRegion.back ().setEnd ( src );
					errHandler.setFatal ();
				} else
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::funcCallEnd, src ));
					src++;
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
				}
				return node;
			}
		} else if ( (src[0] == '[') || (src[0] == '?' && src[1] == '[') )
		{
			bool isNullCoalescing = false;
			if ( src[0] == '?' )
			{
				isNullCoalescing = true;
			}
			if ( lastOp == lastOpType::symbol )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::startArrayAccess, src, isNullCoalescing ? 2 : 1 ) );
				if ( isNullCoalescing )
				{
					src++;
				}
				src++;
				node->setOp ( sCache, isNullCoalescing ? astOp::cArrayDeref : astOp::arrayDeref );

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

				while ( *src != ']' )
				{
					astNode *newNode;

					node->pList ().paramRegion.push_back ( src );
					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					newNode = _parseExpr ( src, false, false, false, false, false, func, doSlang, isLS, isAP );
					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					node->pList().param.push_back ( newNode );
					node->pList ().paramRegion.back ().setEnd ( src );

					if ( (*src != ',') && (*src != ']') )
					{
						if ( !isLS ) throw errorNum::scINVALID_PARAMETER;
						node->pList ().param.push_back ( new astNode ( errorNum::scINVALID_PARAMETER, src ) );		// push an error value for display in language server
						node->pList ().paramRegion.push_back ( src );
						src.advanceGarbage ( ']' );
						node->pList ().param.back ()->setEnd ( src );
						errHandler.setFatal ();
						break;
					}
					if ( *src == ',' )
					{
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
						src++;
					}
				}

				if ( *src != ']' )
				{
					if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
					node->pList ().param.push_back ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );		// push an error value for display in language server
					node->pList ().paramRegion.push_back ( src );
					src.advanceGarbage ( ']' );
					node->pList ().param.back ()->setEnd ( src );
					node->pList ().paramRegion.back().setEnd ( src );
					errHandler.setFatal ();
				} else
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endArrayAccess, src ) );
					src++;
				}
				if ( !node->pList().param.size ( ) )
				{
					if ( !isLS ) throw errorNum::scINVALID_PARAMETER;
					node->pList ().param.push_back ( new astNode ( errorNum::scINVALID_PARAMETER, src ) );		// push an error value for display in language server
					node->pList ().param.push_back ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );		// push an error value for display in language server
					errHandler.setFatal ();
				}
			} else if ( !isNullCoalescing )
			{
				astArray *arr;

				node->setOp ( sCache, astOp::varrayValue );
				arr = arrayJsonDef ( src, false, func, doSlang, isLS, isAP );
				node->arrayData() = *arr;
				delete arr;
			} else
			{
				if ( !isLS ) throw errorNum::scINVALID_OPERATOR;
				node->setError ( errorNum::scINVALID_OPERATOR );
				errHandler.setFatal ();
			}
		} else if ( *src == '{' )
		{
			char const *srcStart = src;
			errorNum err = errorNum::ERROR_OK;

			source srcSave = src;

			src++;
			BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

			if ( *src == '|' )
			{
				src++;

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				while ( *src && (*src != '|') && !_iseol ( src ) )
				{
					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					auto cbNode = getNode ( src, lastOpType::binary, func, doSlang, isLS, isAP );
					if ( cbNode && cbNode->getOp ( ) != astOp::symbolValue )
					{
						if ( !isLS ) throw errorNum::scILLEGAL_IDENTIFIER;
						err = errorNum::scILLEGAL_IDENTIFIER;
						errHandler.setFatal ();
					}
					if ( cbNode ) delete cbNode;

					BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

					if ( (*src != '|') && (*src != ',') )
					{
						if ( !isLS ) throw errorNum::scCODEBLOCK_PARAMETERS;
						err = errorNum::scCODEBLOCK_PARAMETERS;
						errHandler.setFatal ();
					}

					if ( *src == ',' )
					{
						src++;
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					}
				}

				if ( *src )
				{
					src++;
				}

				try
				{
					delete _parseExpr ( src, true, false, false, false, false, 0, false, false, isAP );
				} catch ( errorNum )
				{
				}

				if ( *src != '}' )
				{
					if ( !isLS ) throw errorNum::scMISSING_CLOSE_BRACE;
					err = errorNum::scMISSING_CLOSE_BRACE;
					errHandler.setFatal ();
				}
				src++;

				node->setOp ( sCache, astOp::codeBlockExpression );
				node->stringValue() = stringi ( srcStart, src - srcStart );
			} else
			{
				src = srcSave;
				node->setOp ( sCache, astOp::arrayValue );
				auto arr = arrayDef ( src, false, func, doSlang, isLS, isAP );
				node->arrayData() = std::move ( *arr );
				delete arr;
			}
			if ( err != errorNum::ERROR_OK )
			{
				node->setError ( err );
			}
		} else if ( (src[0] == '/' && src[1] == '/') )
		{
			src += 2;
			BUFFER buff;
			while ( *src && !_iseol( src ) )
			{
				buff.put( *(src++) );
			}
			node->setOp( sCache, astOp::singleLineComment );
			node->stringValue() = buff.data<char const *>();
		} else if ( (src[0] == '/' && src[1] == '*') )
		{
			src += 2;
			BUFFER buff;
			while ( *src && (src[0] != '*' && src[1] != '/') )
			{
				buff.put( *(src++) );
			}
			src += 2;
			node->setOp( sCache, astOp::commentBlock );
			node->stringValue() = buff.data<char const *>();
		} else
		{
			BUFFER		 buff;
			// process operator
			size_t loop = 0;
			for ( ; opList[loop].c; loop++ )
			{
				if ( (*opList[loop].c == *src) && !memcmp ( opList[loop].c + 1, src + 1, static_cast<size_t>(opList[loop].cLen) - 1) )
				{
					src += opList[loop].cLen;
					node->setOp ( sCache, opList[loop].op );

					switch ( opList[loop].opCat )
					{
						case astOpCat::opUnary:
						case astOpCat::opBinary:
							break;
						case astOpCat::opPack:
							if ( lastOp != lastOpType::symbol )
							{
								node->setOp ( sCache, astOp::paramPack );
							} else
							{
								node->setOp ( sCache, astOp::paramExpand );
							}
							break;
						case astOpCat::opPrePost:
							if ( node->getOp ( ) == astOp::dec )
							{
								if ( lastOp != lastOpType::symbol )
								{
									node->setOp ( sCache, astOp::preDec );
								} else
								{
									node->setOp ( sCache, astOp::postDec );
								}
							} else
							{
								if ( lastOp != lastOpType::symbol )
								{
									node->setOp ( sCache, astOp::preInc );
								} else
								{
									node->setOp ( sCache, astOp::postInc );
								}
							}
							break;
						case astOpCat::opBWAndCompile:
							if ( lastOp != lastOpType::symbol )
							{
								node->setOp ( sCache, astOp::comp );
							} else
							{
								node->setOp ( sCache, astOp::bitAnd );
							}
							break;
						case astOpCat::opSubNegate:
							if ( lastOp != lastOpType::symbol )
							{
								node->setOp ( sCache, astOp::negate );
							} else
							{
								node->setOp ( sCache, astOp::subtract );
							}
							break;
						default:
							break;
					}
					break;
				}
			}
			if ( !opList[loop].c )
			{
				// didn't find the operator
				if ( !isLS ) throw errorNum::scINVALID_OPERATOR;
				node->setError ( errorNum::scINVALID_OPERATOR );
				src++;
				errHandler.setFatal ();
			}
		}
		node->setEnd ( src );
		return node;
	} catch ( ... )
	{
		delete node;
		throw;
	}
}

opListDef *opFile::getOperator ( char const *name, uint32_t nParams )
{
	int		 loop;

	for ( loop = 0; opList[loop].c; loop++ )
	{
		if ( (opList[loop].nParams == nParams) && !strncmp ( opList[loop].c, name, opList[loop].cLen ) )
		{
			return &opList[loop];
		}
		if ( (opList[loop].op == astOp::funcCall) && !strncmp ( opList[loop].c, name, opList[loop].cLen ) )
		{
			return &opList[loop];
		}
	}
	throw errorNum::scINTERNAL;
}

char const *parseOperator ( source &src )
{
	int		 loop;

	if ( !memcmp ( "()", src, 2 ) )
	{
		src += 2;
		return "()";
	}

	for ( loop = 0; opList[loop].c; loop++ )
	{
		if ( (*opList[loop].c == *src) && !memcmp ( opList[loop].c + 1, src + 1, static_cast<size_t>(opList[loop].cLen) - 1 ) )
		{
			src += opList[loop].cLen;
			return opList[loop].c;
		}
	}
	throw errorNum::scINTERNAL;
}

astArray *opFile::arrayJsonDef ( source &src, char onlySimpleExpressions, opFunction *func, bool doSlang, bool isLS, bool isAP )
{
	astArray	*array;
	astNode		*node;
	bool		 pairCapable = true;
	bool		 isPair = false;
	source		 srcSave = src;

	array = new astArray;

	if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::startJson, src ) );
	src++;
	BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
	if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeJsonContents, src ) );

	try
	{
		BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

		if ( *src == ']' )
		{
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJsonContents, src ) );
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endJson, src ) );
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJson, src ) );
			src++;
			return array;
		}

		while ( *src )
		{
			BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
			if ( *src == ',' )
			{
				node = (new astNode ( sCache, astOp::nullValue, srcLocation ( src ) ))->setLocation ( srcLocation () );
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
				src++;
				pairCapable = false;

				if ( isPair )
				{
					if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
					node->setError ( errorNum::scARRAY_ENUMERATION );
					errHandler.setFatal ();
				}
				array->nodes.push_back ( node );
			} else
			{
				node = _parseExpr ( src, false, false, false, false, true, func, doSlang, isLS, isAP );
				if ( !node )
				{
					if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
					errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXPRESSION, src ) );
					errHandler.setFatal ();
				} else
				{
					if ( node->getOp () == astOp::pairValue )
					{
						if ( !pairCapable )
						{
							if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
							node->setError ( errorNum::scARRAY_ENUMERATION );
							errHandler.setFatal ();
						}
						isPair = true;
						// note: can be overrideen by doing (value) if you want it to be a symbol.. 
						if ( node->left->getOp () == astOp::symbolValue )
						{
							*node->left = *astNode ( sCache, astOp::stringValue, node->left->symbolValue ().c_str () ).setLocation ( node->left );
						}
					}

					// add it to our array
					array->nodes.push_back ( node );
				}
				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				if ( *src == ']' )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJsonContents, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endJson, src ) );
					src++;
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJson, src ) );
					return array;
				}

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				if ( *src != ',' )
				{
					if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
					array->nodes.push_back ( new astNode ( errorNum::scARRAY_ENUMERATION, src ) );
					src.advanceGarbage ();
					array->nodes.back ()->setEnd ( src );
					errHandler.setFatal ();
				}
				// skip over comma and go on to next array element
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
				src++;

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				if ( *src == ']' )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJsonContents, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endJson, src ) );
					src++;
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJson, src ) );
					return array;
				}
			}
		}
		if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
		array->nodes.push_back ( new astNode ( errorNum::scARRAY_ENUMERATION, srcSave ) );
		array->nodes.back ()->setEnd ( src );
		errHandler.setFatal ();
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJsonContents, src ) );
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterJson, src ) );
		return array;
	} catch ( ... )
	{
		delete array;
		throw;
	}
}

astArray *opFile::arrayDef ( source &src, char onlySimpleExpressions, opFunction *func, bool doSlang, bool isLS, bool isAP )

{
	astArray	*array;
	astNode		*node;
	bool		 pairCapable = true;
	bool		 isPair = false;
	source		 srcSave = src;

	array = new astArray;

	if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::startFixedArray, src ) );
	src++;
	if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeArrayContents, src ) );
	BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

	try
	{
		if ( *src == '}' )
		{
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArrayContents, src ) );
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endFixedArray, src ) );
			src++;
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArray, src ) );
			return(array);
		}

		while ( *src )
		{
			BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
			if ( *src == ',' )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
				node = new astNode ( sCache, astOp::nilValue, srcLocation ( src ) );			// a nullValue is something that exists... this is empty, doesn't exist, nilValue is just a placeholder that will FUCNTION as a null
				src++;
				pairCapable = false;

				if ( isPair )
				{
					if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
					node->setError ( errorNum::scARRAY_ENUMERATION );
					errHandler.setFatal ();
				}
				array->nodes.push_back ( node );
			} else
			{
				node = _parseExpr ( src, false, false, false, false, true, func, doSlang, isLS, isAP );
				if ( !node )
				{
					if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
					errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_EXPRESSION, src ) );
					errHandler.setFatal ();
				} else
				{
					if ( node->getOp () == astOp::pairValue )
					{
						if ( !pairCapable )
						{
							if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
							node->setError ( errorNum::scARRAY_ENUMERATION );
							errHandler.setFatal ();
						}
						isPair = true;
						// note: can be overrideen by doing (value) if you want it to be a symbol.. 
						if ( node->left->getOp () == astOp::symbolValue )
						{
							*node->left = astNode ( sCache, astOp::stringValue, node->left->symbolValue ().c_str () );
						}
					}

					// add it to our array
					array->nodes.push_back ( node );
				}
				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				if ( *src == '}' )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArrayContents, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endFixedArray, src ) );
					src++;
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArray, src ) );
					return (array);
				}

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				if ( *src != ',' )
				{
					if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
					array->nodes.push_back ( new astNode ( errorNum::scARRAY_ENUMERATION, src ) );
					src.advanceGarbage ();
					array->nodes.back ()->setEnd ( src );
					errHandler.setFatal ();
				}

				// skip over comma and go on to next array element
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
				src++;

				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				if ( *src == '}' )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArrayContents, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endFixedArray, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArray, src ) );
					src++;
					return (array);
				}
			}
		}
		if ( !isLS ) throw errorNum::scARRAY_ENUMERATION;
		array->nodes.push_back ( new astNode ( errorNum::scARRAY_ENUMERATION, srcSave ) );
		array->nodes.back ()->setEnd ( src );
		errHandler.setFatal ();
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArrayContents, src ) );
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterArray, src ) );
		return array;
	} catch ( ... )
	{
		delete array;
		throw;
	}
}

static errorNum seqToParam ( astNode *node, symbolSpaceParams &param )
{
	switch ( node->getOp ( ) )
	{
		case astOp::dummy:
			seqToParam ( node->left, param );
			break;
		case astOp::seq:
			seqToParam ( node->left, param );
			seqToParam ( node->right, param );
			break;
		case astOp::symbolValue:
			try
			{
				param.add ( node->symbolValue (), symVariantType, node->location, new astNode ( *node ), false, stringi() );
			} catch ( errorNum err )
			{
				return err;
			}
			break;
		case astOp::nullValue:
			break;
		default:
			return errorNum::scLAMBDA_PARAMETER;
	}
	return errorNum::ERROR_OK;
}

bool opFile::isContinuationSymbol ( source &src, opFile::lastOpType lastOp )
{
	size_t loop = 0;
	for ( ; opList[loop].c; loop++ )
	{
		if ( (*opList[loop].c == *src) && !memcmp ( opList[loop].c + 1, src + 1, static_cast<size_t>(opList[loop].cLen) - 1 ) )
		{
			switch ( opList[loop].opCat )
			{
				case astOpCat::opBinary:
					return true;
				case astOpCat::opPack:
				case astOpCat::opSubNegate:
					if ( lastOp == lastOpType::symbol )
					{
						return true;
					}
					break;
				default:
					break;
			}
			break;
		}
	}
	return false;
}

astNode *opFile::_parseExpr ( source &src, bool sValid, bool onlySimpleExpressions, bool linqExpr, bool eCondValid, bool pairValid, opFunction *func, bool doSlang, bool isLS, bool isAP )
{
	int32_t		 perenCount;
	lastOpType	 lastOp;
	astNode		*node = nullptr;
	opStack		 poStack;
	opStack		 ioStack;
	astNode		*elem;
	astNode		*operand1;
	astNode		*operand2;
	source		 documentationSourceSave = src;
	bool		 hasDocument = false;

	if ( (*src == ')') || (*src == ']') )
	{
		return 0;
	}

	try
	{
		BS_ADVANCE ( src );

		// lastOp = 1 - double operand		lastOp = 2	- single operand	(ok.. so shoot me.. it's backwards)		-- old non enum ledgend 
		lastOp = lastOpType::binary;
		perenCount = 0;

		ioStack.push ( (new astNode ( sCache, astOp::endParse ))->setLocation ( src ) );

		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeStatement, src ) );

		while ( *src )
		{
			errorLocality e ( &errHandler, src );

			if ( _iseol ( src ) && (!perenCount || ((lastOp != lastOpType::binary) && (lastOp != lastOpType::send))) )
			{
				source srcSave = src;

				srcSave.bsAdvanceEol ( false );
				if ( !isContinuationSymbol ( srcSave, lastOp ) )
				{
					break;
				}
				src = srcSave;
			}
			// always terminates
			if ( *src == ';' )
			{
				break;
			} else
			{
				if ( linqExpr )
				{
					bool doExit = false;
					for ( uint32_t loop = 0; linqTermList[loop].str && !doExit; loop++ )
					{
						if ( cmpTerminated ( src, linqTermList[loop].str, linqTermList[loop].len ) )
						{
							doExit = true;
							break;
						}
					}
					if ( doExit )
					{
						break;
					}
				} else
				{
					if ( (!doSlang && (lastOp == lastOpType::symbol) && cmpTerminated ( src, "end", 3 )) || ((lastOp == lastOpType::symbol) && cmpTerminated ( src, "in", 2 )) )
					{
						break;
					}
					if ( doSlang && !perenCount && (lastOp == lastOpType::symbol) && *src == '{' )
					{
						break;
					}
				}
			}

			source srcSave = src;

			node = getNode ( src, lastOp, func, doSlang, isLS, isAP );

			if ( src.emitDebug() )
			{
				node->forceEmitDebug = true;
			}

			if ( !node )
			{
				if ( !perenCount )
				{
					break;
				}
				{
					if ( !isLS ) throw errorNum::scUNBALANCED_PERENS;

					ioStack.push ( new astNode ( errorNum::scUNBALANCED_PERENS, src ) );
					// this is a top-level error... guaranteed everything else after this will be misinterpreted so just don't bother.
					while ( !ioStack.empty () )
					{
						elem = ioStack.popBack ();
						poStack.push ( elem );
					}
					opStack evalStack;

					while ( poStack.elem.size () )
					{
						node = poStack.popFront ();
						switch ( evalStack.num () )
						{
							case 0:
								evalStack.push ( node );
								break;
							default:
								operand1 = evalStack.popBack ();
								node->left = operand1;
								evalStack.push ( node );
								break;
						}
					}
					errHandler.setFatal ();
					return evalStack.popBack ();
				}
			}

			switch ( node->getOp ( ) )
			{
				case astOp::singleLineComment:
					if ( isLS )
					{

						/*   WHY DID I DO THIS???
						if ( (!perenCount || ((lastOp != lastOpType::binary) && (lastOp != lastOpType::send))) )
						{
							src = srcSave;
							goto done;
						}
						*/
						statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::comment, node->location ) );
						documentationSourceSave = srcSave;
						hasDocument = true;
						if ( perenCount || (lastOp == lastOpType::binary) )
						{
							BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
						} else
						{
							BS_ADVANCE ( src );
						}
						delete node;
						continue;
					}
					break;
				case astOp::commentBlock:
					if ( isLS )
					{
						statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::comment, node->location ) );
						documentationSourceSave = srcSave;
						hasDocument = true;
						delete node;
						if ( perenCount || (lastOp == lastOpType::binary) )
						{
							BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
						} else
						{
							BS_ADVANCE ( src );
						}
						continue;
					}
					break;
				case astOp::metadata:
				case astOp::stringValue:
				case astOp::intValue:
				case astOp::doubleValue:
				case astOp::symbolValue:				
				case astOp::codeBlockValue:
				case astOp::codeBlockExpression:
				case astOp::varrayValue:
				case astOp::arrayValue:
				case astOp::symbolPack:
				case astOp::nullValue:
				case astOp::nilValue:
				case astOp::funcValue:
				case astOp::senderValue:
				case astOp::getEnumerator:
				case astOp::compValue:
					if ( lastOp == lastOpType::symbol )
					{
						if ( !isLS ) throw errorNum::scINVALID_OPERATOR;
						node->setError ( errorNum::scINVALID_OPERATOR );
						errHandler.setFatal ();
					}
					lastOp = lastOpType::symbol;

					poStack.push ( node );
					break;
				case astOp::errorValue:
					lastOp = lastOpType::symbol;
					poStack.push ( node );
					break;
				case astOp::endParse:
					delete node;
					if ( --perenCount < 0 )
					{
						src--;
						goto done;
					}
					if ( !isLS ) throw errorNum::scINTERNAL;
					errHandler.setFatal ();
					goto done;

				case astOp::openPeren:
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, node->location ) );
					lastOp = node->isUnaryOp () ? lastOpType::unary : lastOpType::binary;
					perenCount++;

					elem = ioStack.popBack ( );
					while ( elem->stackPriority ( ) >= node->infixPriority ( ) )
					{
						if ( !node->isLeft ( ) && !elem->isLeft ( ) && (elem->stackPriority ( ) == node->infixPriority ( )) )
						{
							break;
						}
						poStack.push ( elem );
						elem = ioStack.popBack ( );
					}
					ioStack.push ( elem );
					ioStack.push ( node );
					break;
				case astOp::closePeren:
					{
						// THIS IS CRITICAL.   This ensures that when we have a () we actually insert something IN THE MIDDLE of the ().   Otherwise the dummy will get attached to 
						// something incorrect.   WHY should we allow ()???   BECAUSE OF LAMBDAS!!!   () => x;
						if ( lastOp == lastOpType::binary )
						{
							ioStack.push ( (new astNode ( sCache, astOp::nullValue ))->setLocation ( node ) );
						}
	
						if ( --perenCount < 0 )
						{
							src--;
							delete node;
							node = nullptr;
							goto done;
						}
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, node->location ) );
						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );

						if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, node->location ) );
						lastOp = lastOpType::symbol;

						elem = ioStack.popBack ();
						while ( (elem->getOp () != astOp::openPeren) && (elem->getOp () != astOp::endParse) )
						{
							poStack.push ( elem );
							elem = ioStack.popBack ();
						}

						auto dummy = new astNode ( sCache, astOp::dummy );
						dummy->setLocation ( elem->location );

						delete node;
						node = nullptr;


						if ( elem->getOp () == astOp::endParse )
						{
							ioStack.push ( elem );
						} else
						{
							delete elem;
						}

						// this inserts a dummy item into the tree.
						// this is to ensure that in the case of
						//		(a)()
						//		(a).<xxx>
						//		<xxx>.(a)
						//		that (a) is interpreted as an expression and not as a symbol
						//	these dummy values MAY be stripped out if not in the above conditions by the constant fold handler
						poStack.push ( dummy );
					}
					break;
				case astOp::seq:
					if ( !sValid && !perenCount )
					{
						// sequential operator is not supported (we're probably parsing a function parameter list
						// instead we need to treat this as a terminator
						delete node;
						if ( hasDocument )
						{
							src = documentationSourceSave;
						} else
						{
							src = srcSave;
						}
						goto done;
					}
					if ( lastOp != lastOpType::symbol )
					{
						if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
						poStack.push ( new astNode ( errorNum::scMISSING_SYMBOL, src ) );		// push an error symbol onto the poStack
						errHandler.setFatal ();
					}

					lastOp = lastOpType::binary;

					elem = ioStack.popBack ( );
					while ( elem->stackPriority ( ) >= node->infixPriority ( ) )
					{
						if ( !node->isLeft ( ) && !elem->isLeft ( ) && (elem->stackPriority ( ) == node->infixPriority ( )) )
						{
							break;
						}
						poStack.push ( elem );
						elem = ioStack.popBack ( );
					}
					ioStack.push ( elem );
					ioStack.push ( node );
					break;

				case astOp::econditional:
					if ( perenCount )
					{
						if ( !isLS ) throw errorNum::scUNBALANCED_PERENS;
						node->setError ( errorNum::scUNBALANCED_PERENS );
						poStack.push ( node );
						lastOp = lastOpType::symbol;
						errHandler.setFatal ();
						break;
					}
					if ( eCondValid )
					{
						delete node;
						src--;
						goto done;
					}
					if ( pairValid )
					{
						node->setOp ( sCache, astOp::pairValue );

						if ( lastOp != lastOpType::symbol )
						{
							if ( !isLS ) throw errorNum::scINVALID_OPERATOR;
							errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_OPERATOR, src ) );
							errHandler.setFatal ();
						}
						lastOp = lastOpType::symbol;
					} else
					{
						if ( !isLS ) throw errorNum::scINVALID_OPERATOR;
						// for language server mode just pretend this is a valid pair but emite an errorValue to show in the editor
						node->setOp ( sCache, astOp::pairValue );
						if ( lastOp != lastOpType::symbol )
						{
							errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_OPERATOR, src ) );
						}
						errHandler.setFatal ();
					}
					[[fallthrough]];
				default:
					if ( ((lastOp == lastOpType::unary) && node->isUnaryOp ( )) || (((lastOp == lastOpType::binary) || (lastOp == lastOpType::send)) && !node->isUnaryOp ( )) )
					{
						if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
						poStack.push ( (new astNode ( errorNum::scMISSING_SYMBOL, node->location ))->setLocation ( node ) );
						errHandler.setFatal ();
					}
					lastOp = node->isUnaryOp ( ) ? (node->isLeft ( ) ? lastOpType::symbol : lastOpType::binary) : lastOpType::binary;

					if ( (node->getOp ( ) == astOp::funcCall) && node->left )
					{
						// equivalent to a symbol
						lastOp = lastOpType::symbol;
						poStack.push ( node );
					} else
					{
						elem = ioStack.popBack ( );
						while ( elem->stackPriority ( ) >= node->infixPriority ( ) )
						{
							if ( !node->isLeft ( ) && !elem->isLeft ( ) && (elem->stackPriority ( ) == node->infixPriority ( )) )
							{
								break;
							}
							poStack.push ( elem );
							elem = ioStack.popBack ( );
						}
						ioStack.push ( elem );

						switch ( node->getOp ( ) )
						{
							case astOp::conditional:
								lastOp = lastOpType::symbol;

								node->setOp ( sCache, astOp::conditional );

								try
								{
									node->conditionData().trueCond = _parseExpr ( src, false, onlySimpleExpressions, false, true, false, func, doSlang, isLS, isAP );
									BS_ADVANCE_EOL ( this, isLS, src );
									if ( *src != ':' )
									{
										if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
										node->conditionData ().falseCond = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
										src.advanceGarbage ();
										node->conditionData ().falseCond->setEnd ( src );
										errHandler.setFatal ();
									} else
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::conditional, src ) );
										src++;
										BS_ADVANCE_EOL ( this, isLS, src );
										node->conditionData ().falseCond = _parseExpr ( src, false, onlySimpleExpressions, false, false, false, func, doSlang, isLS, isAP );
									}
								} catch ( errorNum err )
								{
									if ( !isLS ) throw;
									node->setError ( err );
									errHandler.setFatal ();
								}
								ioStack.push ( node );
								break;
							case astOp::paramExpand:
								if ( sValid )
								{
									if ( !isLS ) throw errorNum::scILLEGAL_EXPANSION;
									node->setError ( errorNum::scILLEGAL_EXPANSION );
									errHandler.setFatal ();
								}
								lastOp = lastOpType::symbol;
								ioStack.push ( node );
								break;
							case astOp::postDec:
							case astOp::postInc:
							case astOp::selfSend:
								lastOp = lastOpType::symbol;
								ioStack.push ( node );
								break;
							case astOp::sendMsg:
							case astOp::cSend:
								if ( isLS ) statements.push_back( std::make_unique<astNode>( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeSendMsg, node->location ) );
								lastOp = lastOpType::send;
								ioStack.push ( node );
								break;
							default:
								ioStack.push ( node );
								break;
						}
					}
					break;
			}
			if ( perenCount || (lastOp == lastOpType::binary) )
			{
				BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
			} else
			{
				BS_ADVANCE ( src );
			}
			hasDocument = false;
		}


	done:
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterStatement, src ) );


		while ( !ioStack.empty ( ) )
		{
			elem = ioStack.popBack ( );

			if ( elem->getOp ( ) == astOp::openPeren )
			{
				if ( !isLS )
				{
					delete elem;
					throw errorNum::scMISSING_CLOSE_BRACE;
				}
				elem->setError ( errorNum::scMISSING_CLOSE_BRACE );
				errHandler.setFatal ();
			}
			poStack.push ( elem );
		}

		// now convert the postfix stack into a tree
		opStack evalStack;
		size_t  numMsgSend = 0;

		try
		{
			while ( poStack.elem.size ( ) )
			{
				node = poStack.popFront ( );

				switch ( node->getOp ( ) )
				{
					case astOp::commentBlock:
					case astOp::singleLineComment:
						break;
					case astOp::stringValue:
					case astOp::intValue:
					case astOp::doubleValue:
					case astOp::symbolValue:
					case astOp::codeBlockValue:
					case astOp::codeBlockExpression:
					case astOp::varrayValue:
					case astOp::arrayValue:
					case astOp::symbolPack:
					case astOp::nullValue:
					case astOp::funcValue:
					case astOp::senderValue:
					case astOp::getEnumerator:
					case astOp::nilValue:
						evalStack.push ( node );
						break;
					case astOp::compValue:
						evalStack.push ( node->left );
						node->left = 0;
						delete node;
						break;
					case astOp::less:
					case astOp::lessEqual:
					case astOp::greater:
					case astOp::greaterEqual:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand2 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							case 1:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = evalStack.popBack();
								operand2 = (new astNode( errorNum::scMISSING_SYMBOL, operand1->isValueType() ? node->location : operand1->location ));
								errHandler.setFatal ();
								break;
							default:
								operand2 = evalStack.popBack ( );
								operand1 = evalStack.popBack ( );
								break;
						}
						node->left = operand1;
						node->right = operand2;
						evalStack.push ( node );

						switch ( node->left->getOp ( ) )
						{
							case astOp::less:
							case astOp::lessEqual:
							case astOp::greater:
							case astOp::greaterEqual:
								{
									errorLocality e ( &errHandler, node );
									errHandler.throwWarning ( isLS, warnNum::scwCOMPARISON_COMBINING );
								}
								break;
							default:
								break;
						}
						break;
					case astOp::equal:
					case astOp::notEqual:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand2 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							case 1:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = evalStack.popBack();
								operand2 = (new astNode( errorNum::scMISSING_SYMBOL, operand1->isValueType() ? node->location : operand1->location ));
								errHandler.setFatal ();
								break;
							default:
								operand2 = evalStack.popBack ( );
								operand1 = evalStack.popBack ( );
								break;
						}
						node->left = operand1;
						node->right = operand2;
						evalStack.push ( node );

						switch ( node->left->getOp ( ) )
						{
							case astOp::equal:
							case astOp::notEqual:
								{
									errorLocality e ( &errHandler, node );
									errHandler.throwWarning ( isLS, warnNum::scwCOMPARISON_COMBINING );
								}
								break;
							default:
								break;
						}
						break;
					case astOp::pairValue:
					case astOp::add:
					case astOp::subtract:
					case astOp::multiply:
					case astOp::divide:
					case astOp::modulus:
					case astOp::power:
					case astOp::max:
					case astOp::min:
					case astOp::logicalAnd:
					case astOp::logicalOr:
					case astOp::bitAnd:
					case astOp::bitOr:
					case astOp::bitXor:
					case astOp::shiftLeft:
					case astOp::shiftRight:
					case astOp::addAssign:
					case astOp::subAssign:
					case astOp::mulAssign:
					case astOp::divAssign:
					case astOp::modAssign:
					case astOp::powAssign:
					case astOp::bwAndAssign:
					case astOp::bwOrAssign:
					case astOp::bwXorAssign:
					case astOp::shLeftAssign:
					case astOp::shRightAssign:
//					case astOp::workAreaStart:
					case astOp::range:
					case astOp::workAreaStart:
					case astOp::seq:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand2 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							case 1:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = evalStack.popBack ( );
								operand2 = (new astNode ( errorNum::scMISSING_SYMBOL, operand1->isValueType() ? node->location : operand1->location ));
								errHandler.setFatal ();
								break;
							default:
								operand2 = evalStack.popBack ( );
								operand1 = evalStack.popBack ( );
								break;
						}
						node->left = operand1;
						node->right = operand2;
						evalStack.push ( node );
						break;
					case astOp::sendMsg:
					case astOp::cSend:
						switch ( evalStack.num() )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand2 = (new astNode( errorNum::scMISSING_SYMBOL, node->location ));
								operand1 = (new astNode( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal();
								break;
							case 1:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = evalStack.popBack();
								operand2 = (new astNode( errorNum::scMISSING_SYMBOL, operand1->isValueType() ? node->location : operand1->location ));
								errHandler.setFatal();
								break;
							default:
								operand2 = evalStack.popBack();
								operand1 = evalStack.popBack();
								break;
						}
						node->left = operand1;
						node->right = operand2;
						evalStack.push( node );
						numMsgSend++;
						break;
					case astOp::assign:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand2 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							case 1:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = evalStack.popBack();
								operand2 = (new astNode( errorNum::scMISSING_SYMBOL, operand1->isValueType() ? node->location : operand1->location ));
								errHandler.setFatal ();
								break;
							default:
								operand2 = evalStack.popBack ( );
								operand1 = evalStack.popBack ( );
								break;
						}
						if ( operand1->getOp ( ) == astOp::arrayValue )
						{
							node->left = operand1;
							node->left->forceOp ( astOp::symbolPack );
						} else
						{
							node->left = operand1;
						}
						node->right = operand2;
						evalStack.push ( node );
						break;
					case astOp::selfSend:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							default:
								operand1 = evalStack.popBack ( );
								break;
						}
						{
							astNode *self = new astNode ( sCache, astOp::symbolValue, "self" );
							node->setOp ( sCache, astOp::sendMsg );
							node->left = self;
							if ( operand1->getOp () == astOp::symbolValue )
							{
								operand1->forceOp ( astOp::nameValue );
							}
							node->right = operand1;
							evalStack.push ( node );
						}
						break;
					case astOp::negate:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							default:
								operand1 = evalStack.popBack ( );
								break;
						}
						node->left = operand1;
						evalStack.push ( node );
						break;
					case astOp::funcCall:
						if ( node->left )
						{
							evalStack.push ( node );
						} else
						{
							switch ( evalStack.num () )
							{
								case 0:
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
									errHandler.setFatal ();
									break;
								default:
									operand1 = evalStack.popBack ( );
									break;
							}
							node->left = operand1;
							evalStack.push ( node );
						}
						if ( node->left->getOp ( ) == astOp::symbolValue )
						{
							// HACK!   allows for compatibility with old engine which allowed session=session() because it didn't have real namespaces or symbol stack mangement
							if ( !strccmp ( node->left->symbolValue(), "new" ) )
							{
								// convert any strings to nameValue's
								// we do this because string's are slow, but cacheString's are fast.
								if ( node->pList().param.size ( ) && (node->pList().param[0]->getOp ( ) == astOp::stringValue)  )
								{
									*node->pList ().param[0] = *astNode ( sCache, astOp::nameValue, sCache.get ( node->pList().param[0]->stringValue() ) ).setLocation ( node->pList ().param[0] );
								}
								//  for FGL code only we convert session() calls to webSession   we do this to allow session = session() ididom that was in rapant use but is invalid due to 
								//      new scoping rules
								if ( !doSlang && node->pList().param.size ( ) && (node->pList().param[0]->getOp ( ) == astOp::nameValue) && !strccmp ( node->pList().param[0]->nameValue(), "session" ) )
								{
									*node->pList ().param[0] = *astNode ( sCache, astOp::nameValue, sCache.get ( "webSession" ) ).setLocation ( node->pList ().param[0] );
								}
							} else if ( !strccmp ( node->left->symbolValue(), "session" ) )
							{
								*node->left = *astNode ( sCache, astOp::symbolValue, sCache.get ( "webSession" ) ).setLocation ( node->left );
							} else if ( !strccmp ( node->left->symbolValue (), "throw" ) )
							{
								*node->left = *astNode ( sCache, astOp::symbolValue, sCache.get ( "except" ) ).setLocation ( node->left );
							}
						}
						break;
					case astOp::lambdaExpr:
						{
							switch ( evalStack.num () )
							{
								case 0:
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									operand2 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
									operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
									errHandler.setFatal ();
									break;
								case 1:
									if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
									operand1 = evalStack.popBack ();
									operand2 = (new astNode ( errorNum::scMISSING_SYMBOL, operand1->isValueType () ? node->location : operand1->location ));
									errHandler.setFatal ();
									break;
								default:
									operand2 = evalStack.popBack ();
									operand1 = evalStack.popBack ();
									break;
							}

							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOperator, node->location ) );

							char		 tmpName[512];
							opFunction *lambda = new opFunction ( this );

							sprintf_s ( tmpName, sizeof ( tmpName ), LAMBDA_ID "%s@%i:%u", srcFiles.getName ( node->location.sourceIndex ).c_str (), node->location.lineNumberStart, node->location.columnNumber );

							lambda->conv = fgxFuncCallConvention::opFuncType_Bytecode;

							lambda->location = node->location;
							lambda->classDef = 0;
							lambda->usingList = ns.getUseList ();
							lambda->name = sCache.get ( tmpName );
							lambda->parentClosure = func->name;
							lambda->isProcedure = false;
							lambda->isLambda = true;

							lambda->codeBlock = (new astNode ( sCache, astOp::btBasic ))->setLocation ( operand1 );

							auto err = seqToParam ( operand1, lambda->params );
							if ( err != errorNum::ERROR_OK )
							{
								if ( !isLS ) throw err;
								lambda->params.clear ();
								// NOTE: this is stupid and nonsensical... HOWEVER, we need the symbols for the language server so that we can generate them during a format operation
								//		 we need SOME place to stick them so adding them to the codeblock is as good as any.   They will just be walked and sorted based upon their
								//		 source location during formatting.  never executed. (hence, the setFatal).   We will never even get here if we're running as an LS
								lambda->codeBlock->pushNode ( new astNode ( *operand1 ) );
								errHandler.setFatal ();
							}

							astNode *ret = new astNode ( sCache, astOp::btReturn );
							ret->returnData ().isYield = false;
							ret->returnData ().node = operand2;
							lambda->codeBlock->pushNode ( ret );

							lambda->codeBlock->adjustLocation ( lambda->location );
							operand1->adjustLocation ( lambda->location );

							lambda->nameLocation = node->location;

							node->setOp ( sCache, astOp::funcValue );
							node->symbolValue () = sCache.get ( tmpName );
							evalStack.push ( node );

							ns.add ( lambda, true );
							delete operand1;
						}
						break;
					case astOp::arrayDeref:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							default:
								operand1 = evalStack.popBack ( );
								break;
						}
						if ( operand1->getOp ( ) == astOp::arrayDeref )
						{
							// append all our index's to the existing dereference operation
							for ( auto it = node->pList().param.begin ( ); it != node->pList().param.end ( ); it++ )
							{
								operand1->pList().param.push_back ( *it );
							}
							for ( auto it = node->pList ().paramRegion.begin (); it != node->pList ().paramRegion.end (); it++ )
							{
								operand1->pList ().paramRegion.push_back ( *it );
							}
							node->pList().param.clear ( );
							delete node;
							evalStack.push ( operand1 );
						} else
						{
							node->left = operand1;
							evalStack.push ( node );
						}
						break;
					case astOp::logicalNot:
					case astOp::postInc:
					case astOp::postDec:
					case astOp::preInc:
					case astOp::preDec:
					case astOp::twosComplement:
					case astOp::refCreate:
					case astOp::comp:
					case astOp::conditional:
					case astOp::paramPack:
					case astOp::paramExpand:
					case astOp::dummy:
					case astOp::metadata:
						switch ( evalStack.num () )
						{
							case 0:
								if ( !isLS ) throw errorNum::scMISSING_SYMBOL;
								operand1 = (new astNode ( errorNum::scMISSING_SYMBOL, node->location ));
								errHandler.setFatal ();
								break;
							default:
								operand1 = evalStack.popBack ( );
								break;
						}
						node->left = operand1;
						evalStack.push ( node );
						break;
					case astOp::errorValue:
						switch ( evalStack.num () )
						{
							case 0:
								evalStack.push ( node );
								break;
							default:
								operand1 = evalStack.popBack ();
								node->left = operand1;
								evalStack.push ( node );
								break;
						}
						break;
					case astOp::endParse:
						delete node;
						break;
					case astOp::btBasic:	// ONLY when in LS mode
						if ( !isLS ) throw errorNum::scINTERNAL;
						switch ( evalStack.num () )
						{
							case 0:
								evalStack.push ( node );
								break;
							default:
								operand1 = evalStack.popBack ();
								node->pushNode ( operand1 );
								evalStack.push ( node );
								break;
						}
						break;
					default:
						throw errorNum::scINTERNAL;
				}
			}

			if ( evalStack.empty ( ) )
			{
				return nullptr;
			}

			if ( isLS )
			{
				while ( numMsgSend-- )
				{
					statements.push_back( std::make_unique<astNode>( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterSendMsg, src) );
				}
//				autoFormatPoints.push_back( std::make_unique<astNode>( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, exprStart ) );
			}

			node = evalStack.popBack ( );

			if ( !evalStack.empty ( ) )
			{
				if ( !isLS ) throw errorNum::scINTERNAL;
				errHandler.setFatal ();
			}

			while ( !poStack.empty ( ) )
			{
				poStack.popBack ( );
			}
		} catch ( ... )
		{
			throw;
		}
	} catch ( ... )
	{
		throw;
	}
	return node;
}

astNode *opFile::parseExpr ( source &src, bool sValid, bool onlySimpleExpressions, opFunction *func, bool doSlang, bool isLS, bool isAP )
{
	return _parseExpr ( src, sValid, onlySimpleExpressions, false, false, false, func, doSlang, isLS, isAP );
}

astNode *opFile::parseLinqExpr ( source &src, bool sValid, bool onlySimpleExpressions, opFunction *func, bool doSlang, bool isLS )
{
	return _parseExpr ( src, sValid, onlySimpleExpressions, true, false, false, func, doSlang, isLS, false );
}

astNode *opFile::parseCaseExpr ( source &src, bool sValid, bool onlySimpleExpressions, opFunction *func, bool doSlang, bool isLS )
{
	return _parseExpr ( src, sValid, onlySimpleExpressions, false, true, false, func, doSlang, isLS, false );
}
