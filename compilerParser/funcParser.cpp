/*

	block processor


*/

#include <tuple>
#include "fileParser.h"
#include "symbolSpaceClass.h"
#include "symbolSpacePCall.h"
#include "symbolSpaceNamespace.h"

class symbolFactoryStore
{
	symbol *sym;

	public:
	symbolFactoryStore ( symbol *sym ) : sym ( sym )
	{}
	~symbolFactoryStore ()
	{
		delete sym;
	}
	template<typename R>
	operator R *()
	{
		return static_cast<R *>(sym);
	}
};
opFunction::opFunction ( class opFile *file )
{}

opFunction::opFunction ( class opFile *file, sourceFile *srcFile, char const **expr )
{
	uint32_t	 len;
	uint32_t	 loop;

	conv = (fgxFuncCallConvention) * *expr;
	(*expr)++;

	isIterator = (bool) **expr;
	(*expr)++;

	isStatic = (bool) **expr;
	(*expr)++;

	isVariantParam = (bool) **expr;
	(*expr)++;

	isPcountParam = (bool) **expr;
	(*expr)++;

	isFGL = (bool) **expr;
	(*expr)++;

	isConst = (bool) **expr;
	(*expr)++;

	isPure = (bool) **expr;
	(*expr)++;

	isProcedure = (bool) **expr;
	(*expr)++;

	isLambda = (bool) **expr;
	(*expr)++;

	isExtension = (bool) **expr;
	(*expr)++;

	isAutoMain = (bool) **expr;
	(*expr)++;

	unknownFunctionCall = (bool) **expr;
	(*expr)++;

	unknownClassInstantiation = (bool) **expr;
	(*expr)++;

	name = file->sCache.get ( expr );
	parentClosure = file->sCache.get ( expr );
	documentation = stringi ( expr );

	location = srcLocation ( srcFile, expr );
	nameLocation = srcLocation ( srcFile, expr );

	retType = symbolTypeClass ( file, expr );

	symbolFactoryStore s ( symbol::symbolFactory ( file, srcFile, expr ) );
	params = *(symbolSpaceParams *) s;

	codeBlock = OP_NODE_DESERIALIZE ( file, srcFile, expr );

	classDef = 0;

	len = *((uint32_t *) *expr);
	(*expr) += sizeof ( uint32_t );
	for ( loop = 0; loop < len; loop++ )
	{
		initializers.push_back ( OP_NODE_DESERIALIZE ( file, srcFile, expr ) );
	}

	usingList = opUsingList ( file, expr );
}

opFunction::opFunction ( opFunction const &old )
{
	inUse = false;
	inlineUse = false;

	conv = old.conv;
	isInterface = old.isInterface;
	name = old.name;
	location = old.location;
	nameLocation = old.nameLocation;
	isIterator = old.isIterator;
	isFGL = old.isFGL;
	classDef = old.classDef;
	usingList = old.usingList;
	isExtension = old.isExtension;
	isStatic = old.isStatic;
	isVariantParam = old.isVariantParam;
	isPcountParam = old.isPcountParam;
	isProcedure = old.isProcedure;
	isPure = old.isPure;
	isConst = old.isConst;
	isLambda = old.isLambda;
	isForcedVariant = false;
	isBaseMethod = old.isBaseMethod;
	isAutoMain = old.isAutoMain;
	weight = old.weight;
	unknownClassInstantiation = old.unknownClassInstantiation;
	unknownFunctionCall = old.unknownFunctionCall;
	if ( old.codeBlock ) codeBlock = new astNode ( *old.codeBlock );
	for ( auto &it : old.initializers )
	{
		initializers.push_back ( new astNode ( *it ) );
	}
	usingList = old.usingList;
	params = old.params;
	parentClosure = old.parentClosure;
}

void opFunction::serialize ( BUFFER *buff, bool serializeFunctionBody )
{
	bufferPut8 ( buff, int ( conv ) );
	bufferPut8 ( buff, isIterator );
	bufferPut8 ( buff, isStatic );
	bufferPut8 ( buff, isVariantParam );
	bufferPut8 ( buff, isPcountParam );
	bufferPut8 ( buff, isFGL );
	bufferPut8 ( buff, isConst );
	bufferPut8 ( buff, isPure );
	bufferPut8 ( buff, isProcedure );
	bufferPut8 ( buff, isLambda );
	bufferPut8 ( buff, isExtension );
	bufferPut8 ( buff, isAutoMain );
	bufferPut8 ( buff, unknownFunctionCall );
	bufferPut8 ( buff, unknownClassInstantiation );

	name.serialize ( buff );
	parentClosure.serialize ( buff );
	documentation.serialize ( buff );

	location.serialize ( buff );
	nameLocation.serialize ( buff );

	retType.serialize ( buff );

	params.serialize ( buff );

	if ( serializeFunctionBody )
	{
		codeBlock->serialize ( buff );
	} else
	{
		static_cast<astNode *>(nullptr)->serialize ( buff ); // NOLINT ( clang-analyzer-core.CallAndMessage )
	}

	bufferPut32 ( buff, (uint32_t) initializers.size () );
	for ( auto it = initializers.begin (); it != initializers.end (); it++ )
	{
		(*it)->serialize ( buff );
	}

	usingList.serialize ( buff );
}

opFunction::~opFunction ()
{
	if ( codeBlock )
	{
		delete codeBlock;
	}
	if ( nonInlinedBlock )
	{
		delete nonInlinedBlock;
	}

	for ( auto it = initializers.begin (); it != initializers.end (); it++ )
	{
		delete *it;
	}
}

void opFunction::setAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	std::lock_guard g1 ( accessorLock );
	if ( !inUse )
	{
		for ( auto &it : accessors )
		{
			if ( scanQueue ) scanQueue->push ( it );
		}
		inUse = true;
		if ( scanQueue ) scanQueue->push ( this );
	}
	accessors.insert ( acc );
}

void opFunction::setWeakAccessed ( accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	std::lock_guard g1 ( accessorLock );
	accessors.insert ( acc );
}

void opFunction::setInlineUsed ( accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	if ( !inlineUse )
	{
		inlineUse = true;
	}
}

symbolTypeClass const opFunction::getMarkReturnType ( class compExecutable *comp, symbolStack const *sym, accessorType const &acc, unique_queue<accessorType> *scanQueue, bool isLS )
{
	compClass *classDef;
	if ( retType.hasClass () && (classDef = sym->findClass ( retType.getClass () )) )
	{
		markClassInuse ( comp, acc, classDef->oClass, sym, scanQueue, isLS );
	}
	return retType.weakify ();
}

void opFunction::setReturnType ( symbolTypeClass const &type, unique_queue<accessorType> *scanQueue )
{
	std::lock_guard g1 ( accessorLock );
	assert ( !isProcedure );
	if ( retType &= type )
	{
		for ( auto &it : accessors )
		{
			if ( scanQueue ) scanQueue->push ( it );
		}
	}
}

void opFunction::setParamType ( class compExecutable *comp, symbolStack *sym, uint32_t paramNum, symbolTypeClass const &type, unique_queue<accessorType> *scanQueue )
{
	if ( type == symbolType::symUnknown )
	{
		return;
	}
	if ( paramNum >= params.size () )
	{
		return;
	}

	if ( (conv == fgxFuncCallConvention::opFuncType_cDecl) && type.hasClass () )
	{
		auto classDef = sym->findClass ( type.getClass () );
		markClassMethodsInuse ( comp, this, classDef->oClass, sym, scanQueue, true, false );
	}

	if ( isVariantParam )
	{
		if ( paramNum >= params.size () - 1 )
		{
			// don't need to type check the parameter pack portion... this is an array and any parameters at that position will be converted to variant array elements
			return;
		}
	}
	params[paramNum]->setInitType ( type, this, scanQueue );
}

void opFunction::setParamTypeNoThrow ( class compExecutable *comp, symbolStack *sym, uint32_t paramNum, symbolTypeClass const &type, unique_queue<accessorType> *scanQueue )
{
	if ( type == symbolType::symUnknown )
	{
		return;
	}
	if ( paramNum >= params.size () )
	{
		return;
	}

	if ( (conv == fgxFuncCallConvention::opFuncType_cDecl) && type.hasClass () )
	{
		auto classDef = sym->findClass ( type.getClass () );
		markClassMethodsInuse ( comp, this, classDef->oClass, sym, scanQueue, true, false );
	}

	if ( isVariantParam )
	{
		if ( paramNum >= params.size () )
		{
			// don't need to type check the parameter pack portion... this is an array and any parameters at that position will be converted to variant array elements
			return;
		}
	}
	params[paramNum]->setInitTypeNoThrow ( type, this, scanQueue );
}

stringi opFunction::getFunctionSignature ()
{
	stringi sig;

	if ( isStatic ) sig += "[[static]] ";
	if ( isExtension ) sig += "[[extension]] ";
	if ( classDef && !isStatic && !isExtension ) sig += "[[method]] ";
	if ( isFGL ) sig += "[[fgl]] ";
	if ( isLambda ) sig += "[[lambda]] ";
	if ( isProcedure ) sig += "[[procedure]] ";
	if ( isPure ) sig += "[[pure]] ";
	if ( isConst ) sig += "[[const]] ";
	if ( isAutoMain) sig += "[[auto main]] ";
	if ( parentClosure.size () ) sig += stringi ( "[[closure " ) + parentClosure.c_str () + "]] ";
	if ( isProcedure )
	{
		sig += "<none>";
	} else
	{
		sig += (char const *) retType;
		sig += " ";
	}
	if ( classDef )
	{
		sig += "method ";
	} else
	{
		sig += "function ";
	}
	sig += name.c_str ();
	sig += " ( ";

	for ( uint32_t loop = 0; loop < params.size (); loop++ )
	{
		if ( loop ) sig += ", ";
		sig += (char const *) params[loop]->getType ();
		sig += " ";
		sig += params[loop]->name;
	}
	if ( isVariantParam )
	{
		if ( params.size () ) sig += ", ";
		sig += "...";
	}
	sig += " )";
	return sig;
}

stringi opFunction::getSimpleFunctionSignature ()
{
	stringi sig = "";

	if ( !isProcedure )
	{
		stringi t = retType;
		sig += t + " ";
	}
	if ( isIterator )
	{
		sig += "iterator ";
	}
	if ( classDef )
	{
		if ( isExtension )
		{
			sig += "extension ";
		}
		sig += "method ";

		char methodName[256];
		_token ( name.c_str (), ".", _numtoken ( name.c_str (), "." ) - 1, methodName );
		sig += classDef->name;
		sig += "::";
		sig += methodName;
	} else
	{
		sig += "function ";
		sig += name.c_str ();
	}
	sig += " ( ";

	for ( uint32_t loop = classDef ? 1 : 0; loop < params.size (); loop++ )
	{
		if ( loop > ( classDef ? 1U : 0U ) ) sig += ", ";
		sig += (params[loop]->getType ()). operator stringi();
		sig += " ";
		sig += params[loop]->name;
	}
	if ( isVariantParam )
	{
		if ( params.size () ) sig += ", ";
		sig += "...";
	}
	sig += " )";
	return sig;
}

std::vector<std::tuple<stringi, stringi, srcLocation const &>> opFunction::getParameterHelp ()
{
	std::vector<std::tuple<stringi, stringi, srcLocation const &>>	sig;

	for ( uint32_t loop = classDef ? 1 : 0; loop < params.size (); loop++ )
	{
		stringi p;
		p = (params[loop]->getType ()). operator stringi();
		p += " ";
		p += params[loop]->name;
		sig.push_back ( {p, params[loop]->getDocumentation (), params[loop]->getLocation ()} );
	}
	return sig;
}

std::vector<std::tuple<stringi, stringi, srcLocation const &>> opFunction::getSlimParameterHelp ()
{
	std::vector<std::tuple<stringi, stringi, srcLocation const &>>	sig;

	for ( uint32_t loop = classDef ? 1 : 0; loop < params.size (); loop++ )
	{
		stringi p;
		p += params[loop]->name;
		sig.push_back ( {p, (params[loop]->getType ()). operator stringi(), params[loop]->getLocation ()} );
	}
	return sig;
}


symbolTypeClass const &opFunction::getParamType ( uint32_t paramNum, accessorType const &acc, unique_queue<accessorType> *scanQueue )
{
	if ( (paramNum) >= params.size () )
	{
		return symWeakVariantType;
	}

	if ( scanQueue ) scanQueue->push ( acc );
	return params[paramNum]->getType ();
}

opFunction *opFile::_parseFunc ( source &src, char const *name, bool parseInitializers, bool autoMain, bool doBraces, bool cbParams, bool isLS, bool isAP, srcLocation const &formatStart, opClass *classDef )
{
	opFunction		*func;
	errorLocality	 e ( &errHandler, src );
	stringi			 documentation;

	func = new opFunction ( this );
	func->conv = fgxFuncCallConvention::opFuncType_Bytecode;

	func->location.set ( src );
	func->classDef = classDef;
	func->usingList = ns.getUseList ();
	func->isVariantParam = false;
	func->isFGL = !doBraces;

	if ( !name )
	{
		errHandler.throwFatalError ( isLS, errorNum::scUNKNOWN_FUNCTION );
		return (func);
	}

	func->name = sCache.get ( name );

	BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

	try
	{
		if ( !autoMain )
		{
			if ( (*src == '(' && !cbParams) || (*src == '|' && cbParams) )
			{
				if ( isLS )
				{
					if ( *src == '(' )
					{
						statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::funcCallStart, src ) );
					} else if ( *src == '|' )
					{
						statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::cbParamSeparator, src ) );
					}
				}
				src++;
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforePerenContents, src ) );

				BS_ADVANCE_EOL ( this, isLS, src );
				errHandler.setLine ( src );

				if ( *src == ')' )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::funcCallEnd, src ) );
					(src)++;
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterFunction, src ) );
				} else if ( *src == '|' )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::cbParamSeparator, src ) );
					(src)++;
				} else
				{
					while ( 1 )
					{
						source srcSave = src;

						try
						{
							if ( (*src == '.') && !memcmp ( src + 1, "..", 2 ) )
							{
								(src) += 3;

								while ( 1 )
								{
									BS_ADVANCE_EOL ( this, isLS, src );
									errHandler.setLine ( src );

									auto [doc, commentStatement] = getDocumentation ( src );

									if ( commentStatement )
									{
										statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
										documentation = doc;
										BS_ADVANCE_EOL ( this, isLS, src );
									} else
									{
										break;
									}
								}

								cacheString symbolTmp;
								try
								{
									symbolTmp = sCache.get ( getSymbol ( src ) );
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::function, src ) );
								} catch ( errorNum e )
								{
									if ( !isLS ) throw;
									errors.push_back ( std::make_unique<astNode> ( e, src ) );
									symbolTmp = sCache.get ( stringi ( "errorName" ) + src.getColumn () + ":" + src.getLine () );
								}

								astNode *varArg = (new astNode ( sCache, astOp::symbolValue, symbolTmp.c_str () ))->setLocation ( srcSave );
								func->params.add ( varArg, symArrayType, documentation );
								documentation.clear ();
								func->isVariantParam = true;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								if ( (src)[0] != ')' ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
							} else if ( doBraces )
							{
								if ( cmpTerminated ( src, "integer", 7 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 7 ) );
									(src) += 7;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symIntType, documentation );
								} else if ( cmpTerminated ( src, "float", 5 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 5 ) );
									(src) += 5;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symDoubleType, documentation );
								} else if ( cmpTerminated ( src, "double", 6 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 6 ) );
									(src) += 6;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symDoubleType, documentation );
								} else if ( cmpTerminated ( src, "string", 6 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 6 ) );
									(src) += 6;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symStringType, documentation );
								} else if ( cmpTerminated ( src, "array", 5 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 5 ) );
									(src) += 5;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symArrayType, documentation );
								} else if ( cmpTerminated ( src, "codeblock", 9 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 9 ) );
									(src) += 9;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symCodeblockType, documentation );
								} else if ( cmpTerminated ( src, "object", 6 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 6 ) );
									(src) += 6;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symObjectType, documentation );
								} else if ( cmpTerminated ( src, "variant", 7 ) || (doBraces && cmpTerminated ( src, "var", 3 )) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 7 ) );
									(src) += 7;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symVariantType, documentation );
								} else if ( cmpTerminated ( src, "function", 8 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, 8 ) );
									(src) += 8;
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symFuncType, documentation );
								} else
								{
									func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symUnknownType, documentation );
								}
							} else
							{
								func->params.add ( parseExpr ( src, false, false, func, doBraces, isLS, isAP ), symUnknownType, documentation );
							}
						} catch ( errorNum err )
						{
							errors.push_back ( std::make_unique<astNode> ( err, srcSave ) );
							errors.back ().get ()->setEnd ( src );
							if ( !isLS ) throw;
							src++;
						}
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

						if ( (*src == ')' && !cbParams) || (*src == '|' && cbParams) )
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPerenContents, src ) );
							if ( isLS )
							{
								if ( *src == ')' )
								{
									statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::funcCallEnd, src ) );
								} else if ( *src == '|' )
								{
									statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::cbParamSeparator, src ) );
								}
							}
							src++;
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterPeren, src ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterFunction, src ) );
							break;
						}
						if ( *src != ',' )
						{
							if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
							errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
						} else
						{
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
							src++;
						}
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
					}
				}

				BS_ADVANCE ( src );

				if ( *src == ':' )
				{
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::colon, src ) );
					if ( parseInitializers )
					{
						(src)++;
						BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

						while ( 1 )
						{
							errorLocality e ( &errHandler, src );

							astNode *initializer = parseExpr ( src, false, false, func, doBraces, isLS, isAP );

							if ( initializer )
							{
								switch ( initializer->getOp () )
								{
									case astOp::funcCall:
										if ( initializer->left->getOp () != astOp::symbolValue )
										{
											if ( !isLS ) throw errorNum::scILLEGAL_IDENTIFIER;
											errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, *initializer ) );
										}
										break;
									case astOp::symbolValue:
										break;
									default:
										if ( !isLS ) throw errorNum::scILLEGAL_IDENTIFIER;
										errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, *initializer ) );
										break;
								}
								func->initializers.push_back ( initializer );
							} else
							{
								errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, src ) );
							}
							BS_ADVANCE_COMMENT ( this, isLS, src );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterFunction, src ) );
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							if ( *src != ',' )
							{
								break;
							}
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
							src++;
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
						}
					} else
					{
						if ( !isLS ) throw errorNum::scILLEGAL_METHOD_INITIALIZER;
						errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_METHOD_INITIALIZER, src ) );
					}
				}
			} else if ( doBraces )
			{
				if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
				errors.push_back ( std::make_unique<astNode> ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
			}
		}

		BS_ADVANCE ( src );

		auto [doc, commentStatement] = getDocumentation ( src );

		if ( commentStatement )
		{
			statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
			func->documentation = doc;
		}

		BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

		if ( !memcmp ( src, "=>", 2 ) )
		{
			if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::expressionFunctionDelimiter, src, 2 ) );
			src += 2;
			BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

			func->codeBlock = (new astNode ( sCache, astOp::btBasic ))->setLocation ( src );
			func->isProcedure = false;
			auto ret = (new astNode ( sCache, astOp::btReturn, srcLocation ( src ) ))->setLocation ( src );
			ret->returnData ().isYield = false;
			ret->returnData ().node = parseExpr ( src, true, false, func, doBraces, isLS, isAP );
			func->codeBlock->pushNode ( ret );
			return func;
		}

		//		if ( isLS && !doBraces ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::startImpliedBlock, src, 1 ) );

		func->codeBlock = parseBlock ( src, func, doBraces, autoMain, isLS, isAP, formatStart );
		func->location.setEnd ( func->codeBlock->location );
		//		if ( isLS && !doBraces ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::endImpliedBlock, src, 1 ) );

				// always need to have a top most locals symbolSpace just in case we need to add automatics... this way we have something to add automatics to!

		BS_ADVANCE ( src );

		errHandler.setLine ( src );
		if ( !autoMain )
		{
			if ( !doBraces )
			{
				BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
				if ( cmpTerminated ( src, "END", 3 ) )
				{
					statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 3 ) );
					src += 2;
					if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
					src += 1;
					func->location.setEnd ( src );
				} else
				{
					if ( !isLS ) throw errorNum::scMISSING_END;
					errors.push_back ( std::make_unique<astNode> ( errorNum::scMISSING_END, src ) );
				}
			}
		} else
		{
			BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
			if ( cmpTerminated ( src, "}", 1 ) )
			{
				if ( !isLS ) throw errorNum::scEXTRA_BRACE;
				errors.push_back ( std::make_unique<astNode> ( errorNum::scEXTRA_BRACE, src ) );
				statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, 1 ) );
				src++;
			}
		}
	} catch ( errorNum err )
	{
		errHandler.throwFatalError ( isLS, err );
	}
	return (func);
}

opFunction *opFile::parseFunc ( source &src, char const *name, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart )
{
	return (_parseFunc ( src, name, false, false, doBraces, false, isLS, isAP, formatStart ));
}

opFunction *opFile::parseCBFunc ( source &src, char const *name, bool doBraces, bool isLS, bool isAP )
{
	return (_parseFunc ( src, name, false, false, doBraces, true, isLS, isAP, srcLocation () ));
}

opFunction *opFile::parseAnonymousFunc ( source &src, char const *name, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart )
{
	opFunction *func;
	func = _parseFunc ( src, name, false, false, doBraces, false, isLS, isAP, formatStart );
	return func;
}

opFunction *opFile::parseFuncAutoMain ( source &src, char const *name, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart )
{
	return (_parseFunc ( src, name, false, true, doBraces, false, isLS, isAP, formatStart ));
}

opFunction *opFile::parseMethod ( source &src, opClass *classDef, char const *name, bool doBraces, bool isLS, bool isAP, srcLocation const &formatStart )
{
	opFunction *func;
	char		 methodName[64];
	source srcSave = src;
	_token ( name, ".", _numtoken ( name, "." ) - 1, methodName );

	if ( !strccmp ( methodName, "new" ) )
	{
		func = _parseFunc ( src, name, true, false, doBraces, false, isLS, isAP, formatStart, classDef );
	} else
	{
		func = _parseFunc ( src, name, false, false, doBraces, false, isLS, isAP, formatStart, classDef );
	}
	func->params.add ( sCache.get ( "self" ), symbolTypeClass ( symbolType::symObject, classDef->name ), srcLocation ( srcSave ), (new astNode ( sCache, astOp::symbolValue, "self" ))->setLocation ( srcLocation ( srcSave ) ), true, "self" );
	return func;
}

opFunction *opFile::findFunc ( char const *name, char const *nSpace )
{
	for ( auto it = functionList.begin (); it != functionList.end (); it++ )
	{
		if ( !strccmp ( (*it).second->name.c_str (), name ) )
		{
			return (*it).second;
		}
	}
	return 0;
}

