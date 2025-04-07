/*

	block processor


*/

#include "fileParser.h"

// only used here... this macro does not change source location at all!
#define BS_ADVANCE_EOL_FULL(ptr,lines) while ( _isspace ((ptr) ) || _iseol ( (ptr) ) || (*(ptr) == ';') ) { if ( (*(ptr)) == '\n' ) { (lines)++;} (ptr)++; }

static bool isArray ( char const *expr )
{
	uint32_t lines = 0;
	enum class stateType
	{
		inNothing,
		inBrace,
		inSingleQuote,
		inDoubleQuote,
	};
	std::queue<stateType>	state;

	if ( expr[0] == '{' )
	{
		expr++;
		state.push ( stateType::inBrace );
		while ( expr[0] && !state.empty () )
		{
			switch ( state.front () )
			{
				case stateType::inNothing:
					switch ( expr[0] )
					{
						case '{':
							state.push ( stateType::inBrace );
							break;
						case '\'':
							state.push ( stateType::inSingleQuote );
							break;
						case '\"':
							state.push ( stateType::inDoubleQuote );
							break;
					}
					break;
				case stateType::inSingleQuote:
					if ( expr[0] == '\'' )
					{
						state.pop ();
					}
					break;
				case stateType::inDoubleQuote:
					if ( expr[0] == '\\' && expr[1] == '"' )
					{
						expr++;
					}
					if ( expr[0] == '"' )
					{
						state.pop ();
					}
					break;
				case stateType::inBrace:
					switch ( expr[0] )
					{
						case '{':
							state.push ( stateType::inBrace );
							break;
						case '}':
							state.pop ();
							break;
					}
					break;
			}
			expr++;
		}
		BS_ADVANCE_EOL_FULL ( expr, lines );
		switch ( expr[0] )
		{
			case '=':
			case '[':
				return true;
			default:
				return false;
		}
	}
	return false;
}

astNode *opFile::parseBlockFGL ( source &src, opFunction *func, bool noImplied, bool isLS, bool isAP, srcLocation const &formatStart )
{
	astNode						*nodeChild;
	int							 defaultPresent;
	std::vector<char const *>    symbols;
	stringi						 documentation;

	errorLocality e ( &errHandler, src );

	auto basicNode = new astNode ( sCache, astOp::btBasic, srcLocation ( src ) );

	if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
	if ( isLS && !noImplied ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeImpliedBlock, src ) );
	try
	{
		BS_ADVANCE_EOL ( this, isLS, src );

		if ( isLS ) basicNode->getExtendedSelection () = src;
		while ( *src )
		{
			while ( *src == ';' )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
				src++;
				BS_ADVANCE_EOL ( this, isLS, src );
			}

			errorLocality e ( &errHandler, src );

			auto [doc, commentStatement] = getDocumentation ( src );

			if ( commentStatement )
			{
				statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
				documentation = doc;
				BS_ADVANCE_EOL ( this, isLS, src );
				continue;
			}

			srcLocation statementStart = src;

			auto it = statementMapFGL.find ( statementString ( src ) );

			if ( ((it != statementMapFGL.end ()) && cmpTerminated ( src, it->second.name, it->second.len )) &&
				 (it->second.type != statementType::stOutput || (it->second.type == statementType::stOutput && isAP)) )
			{
				nodeChild = 0;
				try
				{
					switch ( it->second.type )
					{
						case statementType::stOutput:
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::method, src, it->second.len ) );
								src += it->second.len;

								BS_ADVANCE_EOL ( this, isLS, src );

								srcLocation srcSave = src;
								astNode *output = parseExpr ( src, false, false, func, false, isLS, isAP );
								basicNode->pushNode ( new astNode ( sCache, astOp::add, new astNode ( astOp::symbolValue, sCache.get ( "__outputString" ) ), output ) );
							}
							break;
						case statementType::stLocal:
						case statementType::stVariant:
						case statementType::stString:
						case statementType::stArray:
						case statementType::stInteger:
						case statementType::stDouble:
						case statementType::stObject:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it->second.len ) );
							src += it->second.len;

							BS_ADVANCE_EOL ( this, isLS, src );

							while ( 1 )
							{
								auto [doc, commentStatement] = getDocumentation ( src );

								if ( commentStatement )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
									documentation = doc;
									BS_ADVANCE_EOL ( this, isLS, src );
									continue;
								}

								srcLocation srcSave = src;
								astNode *init = parseExpr ( src, false, false, func, false, isLS, isAP );

								// verification of the initializer is done in symbolLocal constructor
								if ( init && init->getOp () != astOp::errorValue )
								{
									try
									{
										symbolTypeClass type = symUnknownType;
										switch ( it->second.type )
										{
											case statementType::stString:
												type = symStringType;
												break;
											case statementType::stArray:
												type = symArrayType;
												break;
											case statementType::stInteger:
												type = symIntType;
												break;
											case statementType::stDouble:
												type = symDoubleType;
												break;
											case statementType::stObject:
												type = symObjectType;
												break;
											case statementType::stLocal:
											case statementType::stVariant:
											default:
												break;
										}
										basicNode->pushNode ( new astNode ( new symbolLocal ( init, type, *init, documentation ), *init ) );
										if ( isLS ) basicNode->basicData ().blocks.back ()->getExtendedSelection ().setEnd ( src );
										if ( init->getOp() == astOp::assign )
										{
											bool hasError = false;
											for ( auto &it : symbols )
											{
												if ( it == init->left->symbolValue () )
												{
													if ( !isLS ) throw errorNum::scDUPLICATE_SYMBOL;
													errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, *init->left ) );
													hasError = true;
												}
											}
											if ( !hasError ) symbols.push_back ( init->left->symbolValue () );
										} else
										{
											bool hasError = false;
											for ( auto &it : symbols )
											{
												if ( it == init->symbolValue () )
												{
													if ( !isLS ) throw errorNum::scDUPLICATE_SYMBOL;
													errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, *init ) );
													hasError = true;
												}
											}
											if ( !hasError ) symbols.push_back ( init->symbolValue () );
										}

									} catch ( errorNum err )
									{
										if ( !isLS ) throw;
										errors.push_back ( std::make_unique<astNode> ( err, *init ) );
										delete init;
									}
								} else
								{
									if ( !isLS ) throw errorNum::scILLEGAL_IDENTIFIER;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, srcSave ) );
								}

								BS_ADVANCE ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									basicNode->basicData ().blocks.back ()->symbolDef ()->setDocumentation ( basicNode->basicData ().blocks.back ()->symbolDef ()->getSymbolName (), doc2 );
									delete commentStatement2;
									BS_ADVANCE ( src );
								}

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
									BS_ADVANCE_EOL ( this, isLS, src );
								} else
								{
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
										src.advanceGarbage ();
										basicNode->basicData ().blocks.back ()->setEnd ( src );
										break;
									}
								}
								documentation = stringi ();
							}
							break;
						case statementType::stField:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it->second.len ) );
							src += it->second.len;

							BS_ADVANCE_EOL ( this, isLS, src );

							while ( 1 )
							{
								auto [doc, commentStatement] = getDocumentation ( src );

								if ( commentStatement )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
									documentation = doc;
									BS_ADVANCE_EOL ( this, isLS, src );
									continue;
								}

								auto init = parseExpr ( src, false, false, func, false, isLS, isAP );
								if ( init->getOp () == astOp::symbolValue )
								{
									basicNode->pushNode ( new astNode ( new symbolField ( init, documentation ), src ) );
									if ( isLS ) basicNode->basicData ().blocks.back ()->getExtendedSelection ().setEnd ( src );
								} else
								{
									if ( !isLS ) throw errorNum::scINVALID_FIELD;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scINVALID_FIELD, *init ) );
									errHandler.setFatal ();
								}

								BS_ADVANCE ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement2 ) );
									basicNode->basicData ().blocks.back ()->symbolDef ()->setDocumentation ( basicNode->basicData ().blocks.back ()->symbolDef ()->getSymbolName (), doc2 );
									BS_ADVANCE ( src );
								}

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
									BS_ADVANCE_EOL ( this, isLS, src );
								} else
								{
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
										src.advanceGarbage ();
										basicNode->basicData ().blocks.back ()->setEnd ( src );
										break;
									}
								}
								documentation = stringi ();
							}
							break;
						case statementType::stStatic:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it->second.len ) );
							src += it->second.len;

							BS_ADVANCE_EOL ( this, isLS, src );

							while ( 1 )
							{
								auto [doc, commentStatement] = getDocumentation ( src );

								if ( commentStatement )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
									documentation = doc;
									BS_ADVANCE_EOL ( this, isLS, src );
									continue;
								}

								char qualPrefix[512];

								sprintf_s ( qualPrefix, sizeof ( qualPrefix ), "%s@%s@%u:", srcFiles.getName ( src.getSourceIndex () ).c_str (), func->name.c_str (), src.getLine () );
								auto init = parseExpr ( src, false, false, func, false, isLS, isAP );
								if ( init )
								{
									switch ( init->getOp () )
									{
										case astOp::assign:
											if ( init->left->getOp () == astOp::symbolValue )
											{
												basicNode->pushNode ( new astNode ( new symbolStatic ( this, sCache.get ( qualPrefix ), func->name, init, symUnknownType, documentation ), src ) );
											} else
											{
												errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, *init ) );
												basicNode->pushNode ( init );
												errHandler.setFatal ();
											}
											break;
										case astOp::symbolValue:
											basicNode->pushNode ( new astNode ( new symbolStatic ( this, sCache.get ( qualPrefix ), func->name, init, symUnknownType, documentation ), src ) );
											break;
										default:
											errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, *init ) );	
											basicNode->pushNode ( init );
											errHandler.setFatal ();
											break;
									}
								} else
								{
									basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_IDENTIFIER, src ) );
								}
								if ( isLS ) basicNode->basicData ().blocks.back ()->getExtendedSelection ().setEnd ( src );
								BS_ADVANCE ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement2 ) );
									basicNode->basicData ().blocks.back ()->symbolDef ()->setDocumentation ( basicNode->basicData ().blocks.back ()->symbolDef ()->getSymbolName (), doc2 );
									BS_ADVANCE ( src );
								}

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
									BS_ADVANCE_EOL ( this, isLS, src );
								} else
								{
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
										src.advanceGarbage ();
										basicNode->basicData ().blocks.back ()->setEnd ( src );
										break;
									}
								}
								documentation = stringi ();
							}
							break;
						case statementType::stGlobal:
						case statementType::stProcedure:
						case statementType::stFunction:
						case statementType::stIterator:
						case statementType::stClass:
						case statementType::stMethod:
						case statementType::stAssign:
						case statementType::stAccess:
						case statementType::stOperator:
						case statementType::stConst:
						case statementType::stInherit:
						case statementType::stProtected:
						case statementType::stPublic:
						case statementType::stPrivate:
						case statementType::stMessage:
						case statementType::stExtern:
						case statementType::stLibrary:
						case statementType::stDll:
							if ( !isLS ) throw errorNum::scNESTING;
							errors.push_back ( std::make_unique<astNode> ( errorNum::scNESTING, *statements.back().get() ) );
							basicNode->setEnd ( src );
							if ( isLS ) basicNode->getExtendedSelection ().setEnd ( src );
							return basicNode;
						case statementType::stPragma:
							if ( isLS )
							{
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordOther, src, it->second.len ) );
								src += it->second.len;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								srcLocation nameLoc = src;
								srcLocation pragmaLoc = src;
								auto name = getSymbol ( src );
								nameLoc.setEnd ( src );
								if ( name == "region" )
								{
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									auto language = getSymbol ( src );

									languageRegion region;
									if ( language == "html" )
									{
										region.language = languageRegion::languageId::html;
									} else if ( language == "slang" )
									{
										region.language = languageRegion::languageId::slang;
									} else if ( language == "fgl" )
									{
										region.language = languageRegion::languageId::fgl;
									}

									region.location.sourceIndex = src.getSourceIndex ();
									sscanf_s ( src, "%u %u %u %u", &region.location.columnNumber, &region.location.lineNumberStart, &region.location.columnNumberEnd, &region.location.lineNumberEnd );

									languageRegions[region.location] = region.language;
								} else if ( name == "token" )
								{
									char tokenName[256]{};
									srcLocation location;
									sscanf_s ( src, "%s %u %u %u %u", tokenName, (unsigned int)sizeof ( tokenName ), &location.columnNumber, &location.lineNumberStart, &location.columnNumberEnd, &location.lineNumberEnd );
									location.sourceIndex = src.getSourceIndex ();
									statements.push_back ( std::make_unique<astNode> ( astLSInfo::nameToSemanticTokenType[stringi ( tokenName )], location ) );
								} else
								{
									warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwUNKNOWN_PRAGMA, errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_PRAGMA, name.c_str() ) ) );
									warnings.back ().get ()->setLocation ( nameLoc );
								}
								pragmaLoc.setEnd ( src );
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::stringLiteral, pragmaLoc ) );
							}
							src.advanceEOL ();
							BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
							break;
						case statementType::stEnd:
						case statementType::stElseIf:
						case statementType::stElse:
						case statementType::stUntil:
						case statementType::stDefault:
						case statementType::stCase:
						case statementType::stCatch:
						case statementType::stFinally:
							if ( isLS )
							{
								if ( !noImplied ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterImpliedBlock, src ) );
								autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
								basicNode->getExtendedSelection ().setEnd ( src );
							}
							basicNode->setEnd ( src );
							return basicNode;
						case statementType::stReturn:
							if ( isLS )
							{
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, it->second.len ) );
							}
							nodeChild = new astNode ( sCache, astOp::btReturn, srcLocation ( src ) );
							src += it->second.len;
							BS_ADVANCE_COMMENT ( this, isLS, src );
							nodeChild->returnData ().isYield = false;
							nodeChild->returnData ().node = parseExpr ( src, true, false, func, false, isLS, isAP );
							if ( nodeChild->returnData ().node )
							{
								func->isProcedure = false;
							}
							nodeChild->setLocation ( nodeChild->returnData ().node );
							if ( isLS ) nodeChild->getExtendedSelection ().setEnd ( src );
							basicNode->pushNode ( nodeChild );
							break;
						case statementType::stYield:
							if ( isLS )
							{
								statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, it->second.len ) );
							}
							nodeChild = new astNode ( sCache, astOp::btReturn, srcLocation ( src ) );
							src += it->second.len;
							BS_ADVANCE_COMMENT ( this, isLS, src );
							nodeChild->returnData ().isYield = true;
							nodeChild->returnData ().node = parseExpr ( src, true, false, func, false, isLS, isAP );
							nodeChild->setLocation ( nodeChild->returnData ().node );
							if ( isLS ) nodeChild->getExtendedSelection ().setEnd ( src );
							basicNode->pushNode ( nodeChild );
							func->isProcedure = false;
							break;
						case statementType::stOpenBrace:
							{
								try
								{
									nodeChild = parseExpr ( src, true, false, func, false, isLS, isAP );
									if ( !nodeChild )
									{
										if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
										nodeChild = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
									}
									basicNode->pushNode ( nodeChild );
								} catch ( errorNum err )
								{
									if ( !isLS )
									{
										errHandler.throwFatalError ( isLS, err );
									} else
									{
										basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_EXPRESSION, src ) );
									}
									ADVANCE_EOL ( src );
								}
							}
							break;
						case statementType::stIf:
						case statementType::stFor:
						case statementType::stForEach:
						case statementType::stWhile:
						case statementType::stRepeat:
						case statementType::stLabel:
						case statementType::stGoto:
						case statementType::stSwitch:
						case statementType::stBreak:
						case statementType::stContinue:
						case statementType::stTry:
							{
								if ( isLS )
								{
									switch ( it->second.type )
									{
										case statementType::stIf:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeIf, src ) );
											break;
										case statementType::stFor:
										case statementType::stForEach:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFor, src ) );
											break;
										case statementType::stWhile:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeWhile, src ) );
											break;
										case statementType::stRepeat:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeRepeat, src ) );
											break;
										case statementType::stLabel:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeLabel, src ) );
											break;
										case statementType::stGoto:
										case statementType::stBreak:
										case statementType::stContinue:
											break;
										case statementType::stSwitch:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeSwitch, src ) );
											break;
										case statementType::stTry:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeTry, src ) );
											break;
										default:
											assert ( 0 );
											break;
									}
								}

								srcLocation srcSave = src;
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, it->second.len ) );
								src += it->second.len;
								srcSave.setEnd ( src );

								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

								try
								{
									switch ( it->second.type )
									{
										case statementType::stIf:
											nodeChild = new astNode ( sCache, astOp::btIf, srcSave );

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											goto startIf;

											while ( 1 )
											{
												if ( cmpTerminated ( src, "ELSEIF", 6 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeElse, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src,6 ) );
													src += 6;

													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
startIf:

													auto condBlock = std::make_unique<astCondBlock> ();

													if ( *src != '(' )
													{
														if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
														basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
													} else
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
														src++;
													}

													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													condBlock->condition = parseExpr ( src, true, false, func, false, isLS, isAP );

													if ( !condBlock->condition )
													{
														if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
														condBlock->condition = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );

													}
													if ( condBlock->condition->getOp () == astOp::assign )
													{
														warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
														warnings.back ().get ()->setLocation ( condBlock->condition );
													}
													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													if ( *src != ')' )
													{
														if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
														basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
													} else
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
														src++;
													}
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );
													condBlock->block = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );

													BS_ADVANCE_EOL ( this, isLS, src );

													nodeChild->ifData ().ifList.push_back ( condBlock.release () );
												} else if ( cmpTerminated ( src, "ELSE", 4 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeElse, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 4 ) );
													src += 4;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );
													nodeChild->ifData ().elseBlock = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );
													break;
												} else
												{
													break;
												}
											}

											if ( cmpTerminated ( src, "END", 3 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 3 ) );
												src += 2;
												if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
												src += 1;
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
											} else
											{
												// no END, ELSE, or IFELSE
												if ( !isLS ) throw errorNum::scMISSING_END;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
											}
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stForEach:
											nodeChild = new astNode ( sCache, astOp::btForEach, srcSave );

											if ( *src != '(' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
												src++;
											}
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

											nodeChild->forEachData ().var = parseExpr ( src, true, false, func, false, isLS, isAP );

											switch ( nodeChild->forEachData ().var->getOp () )
											{
												case astOp::symbolValue:
													// good
													break;
												case astOp::seq:
													if ( nodeChild->forEachData ().var->left->getOp () != astOp::symbolValue ) throw errorNum::scBAD_VARIABLE_DEF;
													if ( nodeChild->forEachData ().var->right->getOp () != astOp::symbolValue ) throw errorNum::scBAD_VARIABLE_DEF;
													break;
												default:
													break;
											}

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											if ( cmpTerminated ( src, "IN", 2 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 2 ) );
												src += 2;
											} else
											{
												// no END, ELSE, or IFELSE
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_IN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_IN, src ) );
											}
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

											nodeChild->forEachData ().in = parseExpr ( src, true, false, func, false, isLS, isAP );
											if ( !nodeChild->forEachData ().in )
											{
												// no END, ELSE, or IFELSE
												if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
												basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_EXPRESSION, src ) );
											}

											BS_ADVANCE_EOL ( this, isLS, src );
											if ( *src != ')' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
												src++;
											}
											if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );

											nodeChild->forEachData ().statement = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );

											if ( cmpTerminated ( src, "END", 3 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 3 ) );
												src += 2;
												if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
												src += 1;
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
											} else
											{
												// no END, ELSE, or IFELSE
												if ( !isLS ) throw errorNum::scMISSING_END;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
											}
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stFor:
											{
												nodeChild = new astNode ( sCache, astOp::btBasic, srcSave );		// to allow for scoped initializers

												auto f = std::make_unique<astNode> ( sCache, astOp::btFor, srcSave );

												if ( *src != '(' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													src++;
												}

												BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

												symbolType type = symbolType::symUnknown;

												auto it2 = statementMap.find ( statementString ( src ) );
												if ( it2 != statementMap.end () )
												{
													switch ( it2->second.type )
													{
														case statementType::stLocal:
														case statementType::stVariant:
														case statementType::stString:
														case statementType::stArray:
														case statementType::stInteger:
														case statementType::stDouble:
														case statementType::stObject:
															if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it2->second.len ) );
															src += it2->second.len;

															BS_ADVANCE_EOL ( this, isLS, src );

															switch ( it->second.type )
															{
																case statementType::stLocal:
																	type = symbolType::symUnknown;
																	break;
																case statementType::stVariant:
																	type = symbolType::symVariant;
																	break;
																case statementType::stString:
																	type = symbolType::symString;
																	break;
																case statementType::stArray:
																	type = symbolType::symArray;
																	break;
																case statementType::stInteger:
																	type = symbolType::symInt;
																	break;
																case statementType::stDouble:
																	type = symbolType::symDouble;
																	break;
																case statementType::stObject:
																	type = symbolType::symObject;
																	break;
																default:
																	break;
															}
															break;
														default:
															break;
													}
												}
												f->loopData ().initializers = parseExpr ( src, true, false, func, false, isLS, isAP );

												if ( it2 != statementMap.end () )
												{
													if ( (f->loopData ().initializers->getOp () == astOp::assign) && (f->loopData ().initializers->left->getOp () == astOp::symbolValue) )
													{
														nodeChild->addSymbol ( f->loopData ().initializers->left->symbolValue (), type, *f->loopData ().initializers->left, f->loopData ().initializers );
														f->loopData ().initializers = 0;
													}
												}

												BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
												if ( *src != ';' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_SEMICOLON;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_SEMICOLON, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
													src++;
												}
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												f->loopData ().condition = parseExpr ( src, true, false, func, false, isLS, isAP );
												if ( f->loopData ().condition && f->loopData ().condition->getOp () == astOp::assign )
												{
													warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
													warnings.back ().get ()->setLocation ( f->loopData ().condition );
												}

												BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
												if ( *src != ';' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_SEMICOLON;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_SEMICOLON, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
													src++;
												}

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												f->loopData ().increase = parseExpr ( src, true, false, func, false, isLS, isAP );

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != ')' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
													src++;
												}
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );

												f->loopData ().block = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );

												if ( cmpTerminated ( src, "END", 3 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 3 ) );
													src += 2;
													if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
													src += 1;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
												} else
												{
													// no END, ELSE, or IFELSE
													if ( !isLS ) throw errorNum::scMISSING_END;
													basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
												}
												f->setEnd ( src );
												nodeChild->setEnd ( src );
												nodeChild->pushNode ( f.release () );
												basicNode->pushNode ( nodeChild );
											}
											break;
										case statementType::stWhile:
											nodeChild = new astNode ( sCache, astOp::btWhile, srcSave );

											if ( *src != '(' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												src++;
											}
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

											nodeChild->loopData ().condition = parseExpr ( src, true, false, func, false, isLS, isAP );
											if ( nodeChild->loopData ().condition && nodeChild->loopData ().condition->getOp () == astOp::assign )
											{
												warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
												warnings.back ().get ()->setLocation ( nodeChild->loopData ().condition );
											}

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											if ( *src != ')' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
												src++;
											}

											if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );
											nodeChild->loopData ().block = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );

											if ( cmpTerminated ( src, "END", 3 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 3 ) );
												src += 2;
												if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
												src += 1;
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
												basicNode->pushNode ( nodeChild );
											} else
											{
												// no END, ELSE, or IFELSE
												if ( !isLS ) throw errorNum::scMISSING_END;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
											}
											nodeChild->setEnd ( src );
											break;
										case statementType::stRepeat:
											nodeChild = new astNode ( sCache, astOp::btRepeat, srcSave );

											nodeChild->loopData ().block = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );

											if ( cmpTerminated ( src, "UNTIL", 5 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 5 ) );
												src += 5;

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != '(' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													src++;
												}
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												nodeChild->loopData ().condition = parseExpr ( src, true, false, func, false, isLS, isAP );
												if ( nodeChild->loopData ().condition->getOp () == astOp::assign )
												{
													warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
													warnings.back ().get ()->setLocation ( nodeChild->loopData ().condition );
												}

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != ')' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
													src++;
												}
												basicNode->pushNode ( nodeChild );
											} else
											{
												// no END, ELSE, or IFELSE
												if ( !isLS ) throw errorNum::scMISSING_UNTIL;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_UNTIL, src ) );
											}
											nodeChild->setEnd ( src );
											break;
										case statementType::stSwitch:
											nodeChild = new astNode ( sCache, astOp::btSwitch, srcSave );
											if ( *src != '(' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												src++;
											}
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

											nodeChild->switchData ().condition = parseExpr ( src, true, false, func, false, isLS, isAP );

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											if ( *src != ')' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
												src++;
											}
											if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );
											if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeSwitchBody, src ) );
											defaultPresent = 0;

											for ( ;; )
											{
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												if ( cmpTerminated ( src, "CASE", 4 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCase, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 4 ) );
													src += 4;

													auto caseOp = std::make_unique<astCondBlock> ();

													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
													caseOp->condition = parseCaseExpr ( src, true, false, func, false, isLS );
													BS_ADVANCE_COMMENT ( this, isLS, src );
													if ( *src == ':' )
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::colon, src ) );
														src++;
													}
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCaseContents, src ) );
													caseOp->block = parseBlockFGL ( src, func, true, isLS, isAP, statementStart );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCaseContents, src ) );
													nodeChild->switchData ().caseList.push_back ( caseOp.release () );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCase, src ) );
												} else if ( cmpTerminated ( src, "DEFAULT", 7 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCase, src ) );

													srcLocation defLocation = src;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 7 ) );
													src += 7;
													defLocation.setEnd ( src );

													BS_ADVANCE_COMMENT ( this, isLS, src );
													if ( *src == ':' )
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::colon, src ) );
														src++;
													}

													auto caseOp = std::make_unique<astCondBlock> ();

													caseOp->condition = 0;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCaseContents, src ) );
													caseOp->block = parseBlockFGL ( src, func, true, isLS, isAP, statementStart );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCaseContents, src ) );
													nodeChild->switchData ().caseList.push_back ( caseOp.release () );
													if ( defaultPresent )
													{
														if ( !isLS ) throw errorNum::scMULTIPLE_DEFAULTS;
														basicNode->pushNode ( new astNode ( errorNum::scMULTIPLE_DEFAULTS, defLocation ) );
													}
													defaultPresent = 1;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCase, src ) );
												} else if ( cmpTerminated ( src, "END", 3 ) )
												{
													// we don't emit an afterBlockEnd here... the indentation/unindentation is accomplished via the before/after case and caseContents.   we are at the origional indenation when we hit the END
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 3 ) );
													src += 2;
													if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
													src += 1;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
													break;
												} else
												{
													if ( !isLS ) throw errorNum::scMISSING_END;
													basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
													break;
												}
											}
											if ( !nodeChild->switchData ().caseList.size () )
											{
												if ( !isLS ) throw errorNum::scEMPTY_SWITCH;
												basicNode->pushNode ( new astNode ( errorNum::scEMPTY_SWITCH, src ) );
											}
											basicNode->pushNode ( nodeChild );
											nodeChild->setEnd ( src );
											break;
										case statementType::stLabel:
											nodeChild = new astNode ( sCache, astOp::btLabel, srcSave );
											{
												srcLocation label = src;
												nodeChild->label() = sCache.get ( getSymbol ( src ) );
												label.setEnd ( src );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::label, label ) );
											}
											if ( !nodeChild->label ().size() )
											{
												if ( !isLS ) throw errorNum::scILLEGAL_LABEL;
												errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_LABEL, *nodeChild ) );
												errHandler.setFatal ();
											}
											nodeChild->setEnd ( src );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stGoto:
											nodeChild = new astNode ( sCache, astOp::btGoto, srcSave );
											{
												srcLocation label = src;
												nodeChild->gotoData ().label = sCache.get ( getSymbol ( src ) );
												label.setEnd ( src );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::label, label ) );
											}
											if ( !nodeChild->gotoData ().label.size() )
											{
												if ( !isLS ) throw errorNum::scILLEGAL_LABEL;
												errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_LABEL, *nodeChild ) );
												errHandler.setFatal ();
											}
											nodeChild->setEnd ( src );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stBreak:
											nodeChild = new astNode ( sCache, astOp::btBreak, srcSave );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stContinue:
											nodeChild = new astNode ( sCache, astOp::btCont, srcSave );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stTry:
											nodeChild = new astNode ( sCache, astOp::btTry, srcSave );
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											nodeChild->tryCatchData ().tryBlock = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );

											if ( cmpTerminated ( src, "CATCH", 5 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCatch, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 5 ) );
												src += 5;

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != '(' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													src++;
												}
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												nodeChild->tryCatchData ().errSymbol = parseExpr ( src, true, false, func, false, isLS, isAP );
												if ( !nodeChild->tryCatchData ().errSymbol || (nodeChild->tryCatchData ().errSymbol && nodeChild->tryCatchData ().errSymbol->getOp () != astOp::symbolValue) )
												{
													if ( !isLS ) throw errorNum::scINVALID_CATCH_EXPR;
													if ( !nodeChild->tryCatchData ().errSymbol )
													{
														errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, srcSave ) );
														nodeChild->tryCatchData ().errSymbol = new astNode ( sCache, astOp::symbolValue, "$errorValue" );
													} else
													{
														errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, *nodeChild->tryCatchData ().errSymbol ) );
													}
													errHandler.setFatal ();
												}

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != ')' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
													src++;
												}
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );

												nodeChild->tryCatchData ().catchBlock = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );

												if ( cmpTerminated ( src, "FINALLY", 7 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFinally, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 7 ) );
													src += 7;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );

													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													nodeChild->tryCatchData ().finallyBlock = parseBlockFGL ( src, func, false, isLS, isAP, statementStart );
												}

												if ( cmpTerminated ( src, "END", 3 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 3 ) );
													src += 2;
													if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
													src += 1;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
												} else
												{
													if ( !isLS ) throw errorNum::scMISSING_END;
													basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
												}
											} else if ( cmpTerminated ( src, "FINALLY", 7 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFinally, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 7 ) );
												src += 7;

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );

												nodeChild->tryCatchData ().errSymbol = (new astNode ( sCache, astOp::symbolValue, "$tmpCatchVariable" ))->setLocation ( nodeChild );
												nodeChild->tryCatchData ().finallyBlock = (new astNode ( sCache, astOp::btBasic, srcLocation ( src ) ))->setLocation ( nodeChild );
												nodeChild->tryCatchData ().finallyBlock->pushNode ( parseBlockFGL ( src, func, false, isLS, isAP, statementStart ) );

												if ( cmpTerminated ( src, "END", 3 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 3 ) );
													src += 2;
													if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
													src += 1;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlockEnd, src ) );
												} else
												{
													if ( !isLS ) throw errorNum::scMISSING_END;
													basicNode->pushNode ( new astNode ( errorNum::scMISSING_END, src ) );
												}
											} else
											{
												if ( !isLS ) throw errorNum::scMISSING_CONTROL_STRUCTURE;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_CONTROL_STRUCTURE, src ) );
											}
											basicNode->pushNode ( nodeChild );
											break;
										default:
											break;
									}
								} catch ( ... )
								{
									delete nodeChild;
									nodeChild = 0;
									throw;
								}
								if ( isLS ) nodeChild->getExtendedSelection ().setEnd ( src );
							}
							break;
						default:
							if ( !isLS ) throw errorNum::scUNSUPPORTED;
							basicNode->pushNode ( new astNode ( errorNum::scUNSUPPORTED, src ) );
							src.advanceEOL ();
							break;
					}
				} catch ( errorNum err )
				{
					assert ( !isLS );
					errHandler.throwFatalError ( isLS, err );
					ADVANCE_EOL ( src );
				}
			} else
			{
				try
				{
					nodeChild = parseExpr ( src, true, false, func, false, isLS, isAP );
					if ( !nodeChild )
					{
						if ( !isLS )  throw errorNum::scILLEGAL_EXPRESSION;
						if ( *src ) src++;
						nodeChild = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
					}
					if ( nodeChild->getOp () == astOp::equal )
					{
						errorLocality e ( &errHandler, nodeChild );
						warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwINCORRECT_COMPARISON, errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_COMPARISON ) ) );
						warnings.back ().get ()->setLocation ( nodeChild );
					}
					basicNode->pushNode ( nodeChild );
				} catch ( errorNum err )
				{
					assert ( !isLS );
					errHandler.throwFatalError ( isLS, err );
					ADVANCE_EOL ( src );
				}
			}
			documentation = stringi ();
			BS_ADVANCE_EOL_STATEMENT ( this, isLS, src, statementStart );
		}
		if ( isLS && !noImplied ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterImpliedBlock, src ) );
		basicNode->setEnd ( src );
		return basicNode;
	} catch ( ... )
	{
		assert ( !isLS );
		delete basicNode;
		throw;
	}
}

astNode *opFile::parseBlockSlang ( source &src, opFunction *func, bool virtualBrace, bool isLS, bool isAP, srcLocation const &formatStart )
{
	astNode						*nodeChild;
	int							 defaultPresent;
	bool						 multiStatement = true;
	bool						 first = true;
	std::vector<char const *>	 symbols;
	stringi						 documentation;
	errorLocality				 e ( &errHandler, src );

	auto basicNode = new astNode ( sCache, astOp::btBasic, srcLocation ( src ) );

	BS_ADVANCE_EOL ( this, isLS, src );
	srcLocation srcSave = src;

	if ( virtualBrace )
	{
		// do nothing here, but don't execute the other branches of the if tree
	} else if ( *src == '{' )
	{
		if ( isArray ( src ) )
		{
			try
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeImpliedBlock, src ) );	// indent point
				nodeChild = parseExpr ( src, true, false, func, true, isLS, isAP );
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterImpliedBlock, src ) );	// indent point
				if ( !nodeChild )
				{
					if ( !isLS )  throw errorNum::scILLEGAL_EXPRESSION;
					nodeChild = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
				}
				nodeChild->setEnd ( src );
				basicNode->pushNode ( nodeChild );
			} catch ( errorNum err )
			{
				assert ( !isLS );
				errHandler.throwFatalError ( isLS, err );
				ADVANCE_EOL ( src );
			}
			return basicNode;
		}
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeOpenBlock, src ) );
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, src ) );
		if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
		src++;
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterOpenBlock, src ) );		// indent point
	} else
	{
		multiStatement = false;
		if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
		if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeImpliedBlock, src ) );	// indent point
	}

	try
	{
		srcLocation statementStart = src;

		BS_ADVANCE_EOL ( this, isLS, src );
		while ( *(char const *)src && (multiStatement || first) )
		{
			BS_ADVANCE_EOL_STATEMENT ( this, isLS, src, statementStart );

			if ( !*(char const *)src ) break;

			first = false;
			while ( *src == ';' )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
				src++;
				BS_ADVANCE_EOL ( this, isLS, src );
			}

			BS_ADVANCE_EOL ( this, isLS, src );
			errHandler.setLine ( src );

			statementStart = src;

			auto [doc, commentStatement] = getDocumentation ( src );

			if ( commentStatement )
			{
				statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
				documentation = doc;
				BS_ADVANCE_EOL ( this, isLS, src );
				continue;
			}

			auto it = statementMap.find ( statementString ( src ) );

			if ( it != statementMap.end() &&  (it->second.type != statementType::stOutput || (it->second.type == statementType::stOutput && isAP)) )
			{
				nodeChild = 0;
				try
				{
					switch ( it->second.type )
					{
						case statementType::stOutput:
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::method, src, it->second.len ) );
								src += it->second.len;

								BS_ADVANCE_EOL ( this, isLS, src );

								srcLocation srcSave = src;
								astNode *output = parseExpr ( src, false, false, func, false, isLS, isAP );
								basicNode->pushNode ( new astNode ( sCache, astOp::add, new astNode ( astOp::symbolValue, sCache.get ( "__outputString" ) ), output ) );
							}
							break;
						case statementType::stLocal:
						case statementType::stVariant:
						case statementType::stString:
						case statementType::stArray:
						case statementType::stInteger:
						case statementType::stDouble:
						case statementType::stObject:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it->second.len ) );
							src += it->second.len;

							BS_ADVANCE_EOL ( this, isLS, src );

							while ( 1 )
							{
								auto [doc, commentStatement] = getDocumentation ( src );

								if ( commentStatement )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
									documentation = doc;
									BS_ADVANCE_EOL ( this, isLS, src );
									continue;
								}

								srcSave = src;
								astNode *init = parseExpr ( src, false, false, func, true, isLS, isAP );

								if ( init && init->getOp () != astOp::errorValue )
								{
									try
									{
										symbolTypeClass type = symUnknownType;
										switch ( it->second.type )
										{
											case statementType::stString:
												type = symStringType;
												break;
											case statementType::stArray:
												type = symArrayType;
												break;
											case statementType::stInteger:
												type = symIntType;
												break;
											case statementType::stDouble:
												type = symDoubleType;
												break;
											case statementType::stObject:
												type = symObjectType;
												break;
											case statementType::stLocal:
											case statementType::stVariant:
											default:
												break;
										}
										if ( init->getOp () == astOp::assign )
										{
											basicNode->pushNode ( new astNode ( new symbolLocal ( init, type, *init->left, documentation ), *init->left ) );
										} else
										{
											basicNode->pushNode ( new astNode ( new symbolLocal ( init, type, *init, documentation ), *init ) );
										}
										if ( init->right )
										{
											bool hasError = false;
											for ( auto &it : symbols )
											{
												if ( it == init->left->symbolValue () )
												{
													if ( !isLS ) throw errorNum::scDUPLICATE_SYMBOL;
													errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, *init->left ) );
													hasError = true;
												}
											}
											if ( !hasError ) symbols.push_back ( init->left->symbolValue () );
										} else
										{
											bool hasError = false;
											for ( auto &it : symbols )
											{
												if ( it == init->symbolValue () )
												{
													if ( !isLS ) throw errorNum::scDUPLICATE_SYMBOL;
													errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, *init ) );
													hasError = true;
												}
											}
											if ( !hasError ) symbols.push_back ( init->symbolValue () );
										}

									} catch ( errorNum err )
									{
										if ( !isLS ) throw;
										errors.push_back ( std::make_unique<astNode> ( err, *init ) );
										basicNode->pushNode ( init );
									}
								} else
								{
									if ( !isLS ) throw errorNum::scILLEGAL_IDENTIFIER;
									errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, srcSave ) );
								}
								BS_ADVANCE ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									basicNode->basicData ().blocks.back ()->symbolDef ()->setDocumentation ( basicNode->basicData ().blocks.back ()->symbolDef()->getSymbolName(), doc2 );
									delete commentStatement2;
									BS_ADVANCE ( src );
								}

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
									BS_ADVANCE_EOL ( this, isLS, src );
								} else
								{
									if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
										src.advanceGarbage ();
										basicNode->basicData ().blocks.back ()->setEnd ( src );
										break;
									}
								}
								documentation = stringi ();
							}
							break;
						case statementType::stField:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it->second.len ) );
							src += it->second.len;

							BS_ADVANCE_EOL ( this, isLS, src );

							while ( 1 )
							{
								auto [doc, commentStatement] = getDocumentation ( src );

								if ( commentStatement )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
									documentation = doc;
									BS_ADVANCE_EOL ( this, isLS, src );
									continue;
								}

								basicNode->pushNode ( new astNode ( new symbolField ( parseExpr ( src, false, false, func, true, isLS, isAP ), documentation ), src ) );

								BS_ADVANCE ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement2 ) );
									basicNode->basicData ().blocks.back ()->symbolDef ()->setDocumentation ( basicNode->basicData ().blocks.back ()->symbolDef ()->getSymbolName (), doc2 );
									BS_ADVANCE ( src );
								}								
								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
									BS_ADVANCE_EOL ( this, isLS, src );
								} else
								{
									if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
										src.advanceGarbage ();
										basicNode->basicData ().blocks.back ()->setEnd ( src );
										break;
									}
								}
								documentation = stringi ();
							}
							break;
						case statementType::stStatic:
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it->second.len ) );
							src += it->second.len;

							BS_ADVANCE_EOL ( this, isLS, src );

							while ( 1 )
							{
								auto [doc, commentStatement] = getDocumentation ( src );

								if ( commentStatement )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement ) );
									documentation = doc;
									BS_ADVANCE_EOL ( this, isLS, src );
									continue;
								}

								char qualPrefix[512];

								sprintf_s ( qualPrefix, sizeof ( qualPrefix ), "%s@%s@%u:", srcFiles.getName ( src.getSourceIndex () ).c_str (), func->name.c_str (), src.getLine () );
								auto init = parseExpr ( src, false, false, func, false, isLS, isAP );
								if ( init )
								{
									switch ( init->getOp () )
									{
										case astOp::assign:
											if ( init->left->getOp () == astOp::symbolValue )
											{
												basicNode->pushNode ( new astNode ( new symbolStatic ( this, sCache.get ( qualPrefix ), func->name, init, symUnknownType, documentation ), src ) );
											} else
											{
												errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, *init ) );
												basicNode->pushNode ( init );
												errHandler.setFatal ();
											}
											break;
										case astOp::symbolValue:
											basicNode->pushNode ( new astNode ( new symbolStatic ( this, sCache.get ( qualPrefix ), func->name, init, symUnknownType, documentation ), src ) );
											break;
										default:
											errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_IDENTIFIER, *init ) );
											basicNode->pushNode ( init );
											errHandler.setFatal ();
											break;
									}
								} else
								{
									basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_IDENTIFIER, src ) );
								}
								BS_ADVANCE ( src );

								auto [doc2, commentStatement2] = getDocumentation ( src );

								if ( commentStatement2 )
								{
									statements.push_back ( std::unique_ptr<astNode> ( commentStatement2 ) );
									basicNode->basicData ().blocks.back ()->symbolDef ()->setDocumentation ( basicNode->basicData ().blocks.back ()->symbolDef ()->getSymbolName (), doc2 );
									BS_ADVANCE ( src );
								}

								if ( *src == ',' )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
									BS_ADVANCE_EOL ( this, isLS, src );
								} else
								{
									if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, statementStart ) );
									if ( *src == ';' )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
										break;
									} else if ( _iseol ( src ) )
									{
										break;
									} else
									{
										if ( !isLS ) throw errorNum::scSYNTAX_EXTRA_ON_LINE;
										basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_EXTRA_ON_LINE, src ) );
										src.advanceGarbage ();
										basicNode->basicData ().blocks.back ()->setEnd ( src );
										break;
									}
								}
								documentation = stringi ();
							}
							break;
						case statementType::stEnd:
						case statementType::stElseIf:
						case statementType::stElse:
						case statementType::stUntil:
						case statementType::stCatch:
						case statementType::stFinally:
							if ( !isLS ) throw errorNum::scUNEXPECTED_STATEMENT;
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterImpliedBlock, src ) );
							if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, it->second.len ) );
							errors.push_back ( std::make_unique<astNode> ( errorNum::scUNEXPECTED_STATEMENT, src, it->second.len ) );
							src += it->second.len;
							break;
						case statementType::stGlobal:
						case statementType::stProcedure:
						case statementType::stFunction:
						case statementType::stIterator:
						case statementType::stClass:
						case statementType::stMethod:
						case statementType::stAssign:
						case statementType::stAccess:
						case statementType::stOperator:
						case statementType::stConst:
						case statementType::stInherit:
						case statementType::stProtected:
						case statementType::stPublic:
						case statementType::stPrivate:
						case statementType::stMessage:
						case statementType::stExtern:
						case statementType::stLibrary:
						case statementType::stDll:
							if ( !isLS ) throw errorNum::scNESTING;
							basicNode->pushNode ( new astNode ( errorNum::scNESTING, src, it->second.len ) );
							basicNode->setEnd ( src );
							return basicNode;
						case statementType::stPragma:
							if ( isLS )
							{
								src += it->second.len;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
								srcLocation nameLoc = src;
								auto name = getSymbol ( src );
								nameLoc.setEnd ( src );
								if ( name == "region" )
								{
									BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
									auto language = getSymbol ( src );

									languageRegion region;
									if ( language == "html" )
									{
										region.language = languageRegion::languageId::html;
									} else if ( language == "slang" )
									{
										region.language = languageRegion::languageId::slang;
									} else if ( language == "fgl" )
									{
										region.language = languageRegion::languageId::fgl;
									}

									region.location.sourceIndex = src.getSourceIndex ();
									sscanf_s ( src, "%u %u %u %u", &region.location.columnNumber, &region.location.lineNumberStart, &region.location.columnNumberEnd, &region.location.lineNumberEnd );

									languageRegions[region.location] = region.language;
								} else if ( name == "token" )
								{
									char tokenName[256]{};
									srcLocation location;
									sscanf_s ( src, "%s %u %u %u %u", tokenName, (unsigned int)sizeof ( tokenName ), &location.columnNumber, &location.lineNumberStart, &location.columnNumberEnd, &location.lineNumberEnd );
									location.sourceIndex = src.getSourceIndex ();
									statements.push_back ( std::make_unique<astNode> ( astLSInfo::nameToSemanticTokenType[stringi ( tokenName )], location ) );
								} else
								{
									warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwUNKNOWN_PRAGMA, errHandler.throwWarning ( isLS, warnNum::scwUNKNOWN_PRAGMA, name.c_str() ) ) );
									warnings.back ().get ()->setLocation ( nameLoc );
								}
							}
							src.advanceEOL ();
							break;
						case statementType::stDefault:
						case statementType::stCase:
							// do NOT emit statements here... they will be emitted in the switch case handler
							return basicNode;
						case statementType::stCloseBrace:
							if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
							if ( !virtualBrace )
							{
								if ( multiStatement )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlock, src ) );	// unindent point
								} else
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterImpliedBlock, src ) ); 	// unindent point
								}
							}
							basicNode->setEnd ( src );
							if ( !virtualBrace )
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, src ) );
								src += it->second.len;
								return basicNode;
							}
							return basicNode;
						case statementType::stOpenBrace:
							if ( isArray ( src ) )
							{
								try
								{
									nodeChild = parseExpr ( src, true, false, func, true, isLS, isAP );
									if ( !nodeChild )
									{
										if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
										nodeChild = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
									}
//									nodeChild->setEnd ( src );
									basicNode->pushNode ( nodeChild );
								} catch ( errorNum err )
								{
									assert ( !isLS );
									errHandler.throwFatalError ( isLS, err );
									ADVANCE_EOL ( src );
								}
							} else
							{
								nodeChild = new astNode ( sCache, astOp::btBasic, srcLocation ( src ) );
								nodeChild->basicData ().blocks.push_back ( parseBlockSlang ( src, func, false, isLS, isAP, statementStart ) );
								basicNode->pushNode ( nodeChild );
							}
							break;
						case statementType::stIf:
						case statementType::stFor:
						case statementType::stForEach:
						case statementType::stWhile:
						case statementType::stRepeat:
						case statementType::stYield:
						case statementType::stLabel:
						case statementType::stGoto:
						case statementType::stSwitch:
						case statementType::stBreak:
						case statementType::stReturn:
						case statementType::stContinue:
						case statementType::stTry:
							{
								if ( isLS )
								{
									autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
									switch ( it->second.type )
									{
										case statementType::stIf:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeIf, src ) );
											break;
										case statementType::stFor:
										case statementType::stForEach:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeFor, src ) );
											break;
										case statementType::stWhile:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeWhile, src ) );
											break;
										case statementType::stRepeat:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeRepeat, src ) );
											break;
										case statementType::stLabel:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeLabel, src ) );
											break;
										case statementType::stGoto:
										case statementType::stBreak:
										case statementType::stContinue:
										case statementType::stReturn:
										case statementType::stYield:
											break;
										case statementType::stSwitch:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeSwitch, src ) );
											break;
										case statementType::stTry:
											statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeTry, src ) );
											break;
										default:
											assert ( 0 );
											break;
									}
								}

								srcSave = src;

								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, it->second.len ) );
								src += it->second.len;
								srcSave.setEnd ( src );

								BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );		// eat any whitepace and comments, but halt on a semicolon (which is an error)

								try
								{
									switch ( it->second.type )
									{
										case statementType::stIf:
											nodeChild = new astNode ( sCache, astOp::btIf, srcSave );

											goto startIf;

											while ( *src )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeElse, src ) );
												if ( cmpTerminated ( src, "else", 4 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 4 ) );
													src += 4;

													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													if ( !cmpTerminated ( src, "if", 2 ) )
													{
														nodeChild->ifData ().elseBlock = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );
														break;
													}

													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 2 ) );
													src += 2;

startIf:
													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													if ( *src != '(' )
													{
														if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
														basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
													} else
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
														src++;
													};

													auto condBlock = std::make_unique<astCondBlock> ();

													condBlock->condition = parseExpr ( src, true, false, func, true, isLS, isAP );
													if ( !condBlock->condition )
													{
														if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
														condBlock->condition = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
													}
													if ( condBlock->condition->getOp () == astOp::assign )
													{
														warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
														warnings.back ().get ()->setLocation ( condBlock->condition );
													}
													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													if ( *src != ')' )
													{
														if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
														basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
													} else
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
														src++;
													}
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );

													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													condBlock->block = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );

													nodeChild->ifData ().ifList.push_back ( condBlock.release () );
													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												} else
												{
													break;
												}
											}
											nodeChild->setEnd ( src );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stForEach:
											nodeChild = new astNode ( sCache, astOp::btForEach, srcSave );

											if ( *src != '(' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												src++;
											}
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

											nodeChild->forEachData ().var = parseExpr ( src, true, false, func, false, isLS, isAP );

											if ( nodeChild->forEachData ().var )
											{
												switch ( nodeChild->forEachData ().var->getOp () )
												{
													case astOp::symbolValue:
														// good
														break;
													case astOp::seq:
														if ( nodeChild->forEachData ().var->left->getOp () != astOp::symbolValue ) if ( !isLS ) throw errorNum::scBAD_VARIABLE_DEF;
														if ( nodeChild->forEachData ().var->right->getOp () != astOp::symbolValue ) if ( !isLS ) throw errorNum::scBAD_VARIABLE_DEF;
														break;
													default:
														break;
												}
											} else
											{
												if ( !isLS ) throw errorNum::scILLEGAL_IDENTIFIER;
												nodeChild->forEachData ().var = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
											}

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											if ( cmpTerminated ( src, "IN", 2 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 2 ) );
												src += 2;

												BS_ADVANCE_EOL ( this, isLS, src );

												nodeChild->forEachData ().in = parseExpr ( src, true, false, func, true, isLS, isAP );
												if ( !nodeChild->forEachData ().in )
												{
													if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
													nodeChild->forEachData ().in = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
												}
											} else
											{
												// no IN statement
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_IN;
												nodeChild->forEachData ().in = new astNode ( errorNum::scILLEGAL_EXPRESSION, src );
											}

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											if ( *src != ')' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												break;
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
												src++;
											}
											if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

											nodeChild->forEachData ().statement = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );

											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stFor:
											{
												nodeChild = new astNode ( sCache, astOp::btBasic, srcSave );	// this is to hold any for scoped local symbol definitions, e.g.  for ( local x =...

												auto f = std::make_unique<astNode> ( sCache, astOp::btFor, srcLocation ( src ) );			// this is the for loop proper

												if ( *src != '(' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													src++;
												}
												BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

												symbolType type = symbolType::symUnknown;

												auto it2 = statementMap.find ( statementString ( src ) );
												if ( it2 != statementMap.end () )
												{
													switch ( it2->second.type )
													{
														case statementType::stLocal:
														case statementType::stVariant:
														case statementType::stString:
														case statementType::stArray:
														case statementType::stInteger:
														case statementType::stDouble:
														case statementType::stObject:
															if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::type, src, it2->second.len ) );
															src += it2->second.len;

															BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

															switch ( it2->second.type )
															{
																case statementType::stLocal:
																	type = symbolType::symUnknown;
																	break;
																case statementType::stVariant:
																	type = symbolType::symVariant;
																	break;
																case statementType::stString:
																	type = symbolType::symString;
																	break;
																case statementType::stArray:
																	type = symbolType::symArray;
																	break;
																case statementType::stInteger:
																	type = symbolType::symInt;
																	break;
																case statementType::stDouble:
																	type = symbolType::symDouble;
																	break;
																case statementType::stObject:
																	type = symbolType::symObject;
																	break;
																default:
																	break;
															}
															break;
														default:
															break;
													}
												}
												f->loopData ().initializers = parseExpr ( src, true, false, func, true, isLS, isAP );

												if ( it2 != statementMap.end () )
												{
													if ( (f->loopData ().initializers->getOp () == astOp::assign) && (f->loopData ().initializers->left->getOp () == astOp::symbolValue) )
													{
														nodeChild->addSymbol ( f->loopData ().initializers->left->symbolValue (), type, *f->loopData ().initializers->left, f->loopData ().initializers );
														f->loopData ().initializers = 0;
													}
												}

												BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
												if ( *src != ';' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_SEMICOLON;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_SEMICOLON, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
													src++;
												}
												BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

												f->loopData ().condition = parseExpr ( src, true, false, func, true, isLS, isAP );
												if ( f->loopData ().condition )
												{
													if ( f->loopData ().condition->getOp () == astOp::assign )
													{
														warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
														warnings.back ().get ()->setLocation ( f->loopData ().condition );
													}
												}
												BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
												if ( *src != ';' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_SEMICOLON;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_SEMICOLON, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
													src++;
												}
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												f->loopData ().increase = parseExpr ( src, true, false, func, true, isLS, isAP );

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != ')' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
													src++;
												}
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												f->loopData ().block = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );
												f->setEnd ( src );
												nodeChild->setEnd ( src );
												nodeChild->pushNode ( f.release () );
												basicNode->pushNode ( nodeChild );
											}
											break;
										case statementType::stWhile:
											nodeChild = new astNode ( sCache, astOp::btWhile, srcSave );

											if ( *src != '(' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												src++;
											}
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											nodeChild->loopData ().condition = parseExpr ( src, true, false, func, true, isLS, isAP );
											if ( !nodeChild->loopData ().condition )
											{
												if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
												basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_EXPRESSION, src ) );
											} else
											{
												if ( nodeChild->loopData ().condition->getOp () == astOp::assign )
												{
													warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
													warnings.back ().get ()->setLocation ( nodeChild->loopData ().condition );
												}
											}

											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											if ( *src != ')' )
											{
												if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
												basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
											} else
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
												src++;
											}
											if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );
											nodeChild->loopData ().block = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );
											nodeChild->setEnd ( src );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stRepeat:
											if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
											basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_EXPRESSION, statements.back().get()->location ) );
											break;
										case statementType::stDo:
											nodeChild = new astNode ( sCache, astOp::btRepeat, srcSave );
											nodeChild->loopData ().block = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );
											if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeWhile, src ) );

											if ( cmpTerminated ( src, "while", 5 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 5 ) );
												src += 5;

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												if ( *src != '(' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													src++;
												}
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												nodeChild->loopData ().condition = parseExpr ( src, true, false, func, true, isLS, isAP );

												if ( !nodeChild->loopData ().condition )
												{
													if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
													basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_EXPRESSION, src ) );
												} else
												{
													if ( nodeChild->loopData ().condition->getOp () == astOp::assign )
													{
														warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT, errHandler.throwWarning ( isLS, warnNum::scwPOSSIBLY_INCORRECT_ASSIGNMENT ) ) );
														warnings.back ().get ()->setLocation ( nodeChild->loopData ().condition );
													}
												}

												if ( *src != ')' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
													src++;
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );
												}
											} else
											{
												// no while
												if ( !isLS ) throw errorNum::scMISSING_WHILE;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_WHILE, src ) );
											}
											basicNode->pushNode ( nodeChild );
											nodeChild->setEnd ( src );
											break;
										case statementType::stSwitch:
											{
												nodeChild = new astNode ( sCache, astOp::btSwitch, srcSave );

												if ( *src != '(' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													src++;
												}
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );


												nodeChild->switchData ().condition = parseExpr ( src, true, false, func, true, isLS, isAP );
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												if ( !nodeChild->switchData ().condition )
												{
													if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
													basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_EXPRESSION, src ) );
												}

												if ( *src != ')' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
													src++;
												}
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCondition, src ) );

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												defaultPresent = 0;

												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeSwitchBody, src ) );

												if ( *src != '{' )
												{
													if ( !isLS ) throw errorNum::scMISSING_OPEN_BRACE;
													basicNode->pushNode ( new astNode ( errorNum::scMISSING_OPEN_BRACE, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, src ) );
													src++;
												}

												while ( *src )
												{
													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													if ( cmpTerminated ( src, "CASE", 4 ) )
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCase, src ) );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 4 ) );
														src += 4;

														auto caseOp = std::make_unique<astCondBlock> ();

														BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
														caseOp->condition = parseCaseExpr ( src, true, false, func, true, isLS );
														if ( !caseOp->condition )
														{
															basicNode->pushNode ( new astNode ( errorNum::scILLEGAL_EXPRESSION, src ) );
														}
														BS_ADVANCE_COMMENT ( this, isLS, src );
														if ( *src == ':' )
														{
															if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::colon, src ) );
															src++;
														}
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCaseContents, src ) );
														caseOp->block = parseBlockSlang ( src, func, true, isLS, isAP, statementStart );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCaseContents, src ) );
														nodeChild->switchData ().caseList.push_back ( caseOp.release () );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCase, src ) );
													} else if ( cmpTerminated ( src, "DEFAULT", 7 ) )
													{
														srcLocation defLocation = src;
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCase, src ) );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 7 ) );
														src += 7;
														defLocation.setEnd ( src );
														BS_ADVANCE_COMMENT ( this, isLS, src );
														if ( *src == ':' )
														{
															if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::colon, src ) );
															src++;
														}

														auto caseOp = std::make_unique<astCondBlock> ();

														caseOp->condition = nullptr;

														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::beforeCaseContents, src ) );
														caseOp->block = parseBlockSlang ( src, func, true, isLS, isAP, statementStart );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCaseContents, src ) );
														nodeChild->switchData ().caseList.push_back ( caseOp.release () );
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterCase, src ) );

														if ( defaultPresent )
														{
															if ( !isLS ) throw errorNum::scMULTIPLE_DEFAULTS;
															caseOp->condition = new astNode ( errorNum::scMULTIPLE_DEFAULTS, defLocation );
														}
														defaultPresent = 1;
													} else 	if ( *src == '}' )
													{
														if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::punctuation, src ) );
														src += 1;
														break;
													} else
													{
														if ( !isLS ) throw errorNum::scMISSING_CLOSE_BRACE;
														basicNode->pushNode ( new astNode ( errorNum::scMISSING_CLOSE_BRACE, src ) );
														break;
													}
												}
												nodeChild->setEnd ( src );
												basicNode->pushNode ( nodeChild );
											}
											break;
										case statementType::stLabel:
											nodeChild = new astNode ( sCache, astOp::btLabel, srcSave );
											{
												srcLocation label = src;
												nodeChild->label () = sCache.get ( getSymbol ( src ) );
												label.setEnd ( src );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::label, label ) );
											}
											if ( !nodeChild->label ().size() )
											{
												if ( !isLS ) throw errorNum::scILLEGAL_LABEL;
												errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_LABEL, *nodeChild ) );
												errHandler.setFatal ();
											}
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stGoto:
											nodeChild = new astNode ( sCache, astOp::btGoto, srcSave );
											{
												srcLocation label = src;
												nodeChild->gotoData ().label = sCache.get ( getSymbol ( src ) );
												label.setEnd ( src );
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::label, label ) );
											}
											if ( !nodeChild->gotoData ().label.size () )
											{
												if ( !isLS ) throw errorNum::scILLEGAL_LABEL;
												errors.push_back ( std::make_unique<astNode> ( errorNum::scILLEGAL_LABEL, *nodeChild ) );
												errHandler.setFatal ();
											}
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stBreak:
											nodeChild = new astNode ( sCache, astOp::btBreak, srcSave );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stContinue:
											nodeChild = new astNode ( sCache, astOp::btCont, srcSave );
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stReturn:
											nodeChild = new astNode ( sCache, astOp::btReturn, srcSave );
											nodeChild->returnData ().isYield = false;
											nodeChild->returnData ().node = parseExpr ( src, true, false, func, true, isLS, isAP );
											if ( nodeChild->returnData ().node )
											{
												func->isProcedure = false;
											}
											basicNode->pushNode ( nodeChild );
											break;
										case statementType::stYield:
											nodeChild = new astNode ( sCache, astOp::btReturn, srcSave );
											nodeChild->returnData ().isYield = true;
											nodeChild->returnData ().node = parseExpr ( src, true, false, func, true, isLS, isAP );
											basicNode->pushNode ( nodeChild );
											func->isProcedure = false;
											break;
										case statementType::stTry:
											nodeChild = new astNode ( sCache, astOp::btTry, srcSave );
											nodeChild->tryCatchData ().tryBlock = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );
											BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
											if ( cmpTerminated ( src, "CATCH", 5 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 5 ) );
												src += 5;

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != '(' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_OPEN_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_OPEN_PEREN, statements.back ().get ()->location ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::openPeren, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													src++;
												}
												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												nodeChild->tryCatchData ().errSymbol = parseExpr ( src, true, false, func, true, isLS, isAP );
												if ( !nodeChild->tryCatchData ().errSymbol || (nodeChild->tryCatchData ().errSymbol && nodeChild->tryCatchData ().errSymbol->getOp () != astOp::symbolValue) )
												{
													if ( !isLS ) throw errorNum::scINVALID_CATCH_EXPR;
													if ( !nodeChild->tryCatchData ().errSymbol )
													{
														errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, srcSave ) );
														nodeChild->tryCatchData ().errSymbol = new astNode ( sCache, astOp::symbolValue, "$errorValue" );
													} else
													{
														errors.push_back ( std::make_unique<astNode> ( errorNum::scDUPLICATE_SYMBOL, *nodeChild->tryCatchData ().errSymbol ) );
													}
													errHandler.setFatal ();
												}

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );
												if ( *src != ')' )
												{
													if ( !isLS ) throw errorNum::scSYNTAX_MISSING_CLOSE_PEREN;
													basicNode->pushNode ( new astNode ( errorNum::scSYNTAX_MISSING_CLOSE_PEREN, src ) );
												} else
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::withinCondition, src ) );
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::closePeren, src ) );
													src++;
												}
												nodeChild->tryCatchData ().catchBlock = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												if ( cmpTerminated ( src, "FINALLY", 7 ) )
												{
													if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 7 ) );
													src += 7;

													BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

													nodeChild->tryCatchData ().finallyBlock = parseBlockSlang ( src, func, false, isLS, isAP, statementStart );
												}
												basicNode->pushNode ( nodeChild );
											} else if ( cmpTerminated ( src, "FINALLY", 7 ) )
											{
												if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 7 ) );
												src += 7;

												BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

												nodeChild->tryCatchData ().errSymbol = (new astNode ( sCache, astOp::symbolValue, "$tmpCatchVariable" ))->setLocation ( nodeChild );
												nodeChild->tryCatchData ().finallyBlock = (new astNode ( sCache, astOp::btBasic, srcLocation ( src ) ))->setLocation ( nodeChild );
												nodeChild->tryCatchData ().finallyBlock->pushNode ( parseBlockSlang ( src, func, false, isLS, isAP, statementStart ) );
												basicNode->pushNode ( nodeChild );
											} else
											{
												if ( !isLS ) throw errorNum::scMISSING_CONTROL_STRUCTURE;
												basicNode->pushNode ( new astNode ( errorNum::scMISSING_CONTROL_STRUCTURE, statements.back().get()->location ) );
											}
											nodeChild->setEnd ( src );
											break;
										default:
											break;
									}
								} catch ( ... )
								{
									delete nodeChild;
									nodeChild = 0;
									throw;
								}
							}
							if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
							break;
						default:
							if ( !isLS ) throw errorNum::scUNSUPPORTED;
							basicNode->pushNode ( new astNode ( errorNum::scUNSUPPORTED, src ) );
							src.advanceEOL ();
							break;
					}
				} catch ( errorNum err )
				{
					errHandler.throwFatalError ( isLS, err );
					ADVANCE_EOL ( src );
				}
			} else
			{
				try
				{
					nodeChild = parseExpr ( src, true, false, func, true, isLS, isAP );
					if ( !nodeChild && *src )
					{
						srcLocation extraLocation = src;
						src++;
						extraLocation.setEnd ( src );
						statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::comment, extraLocation ) );
						if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
					} else if ( nodeChild )
					{
						if ( nodeChild->getOp () == astOp::equal )
						{
							errorLocality e ( &errHandler, nodeChild );
							warnings.push_back ( std::make_unique<astNode> ( sCache, warnNum::scwINCORRECT_COMPARISON, errHandler.throwWarning ( isLS, warnNum::scwINCORRECT_COMPARISON ) ) );
							warnings.back ().get ()->setLocation ( nodeChild );
						}
						if ( nodeChild->getOp () == astOp::symbolValue )
						{
							BS_ADVANCE ( src );
							if ( *src == ':' )
							{
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::colon, src ) );
								src++;
								auto newNode = (new astNode ( sCache, astOp::btLabel ))->setLocation ( nodeChild );
								newNode->label () = nodeChild->symbolValue ();
								delete nodeChild;
								nodeChild = newNode;
							}
						}
						basicNode->pushNode ( nodeChild );
					}
				} catch ( errorNum err )
				{
					errHandler.throwFatalError ( isLS, err );
//					errHandler.throwFatalError ( isLS, err, )
					ADVANCE_EOL ( src );
				}
			}
			documentation = stringi ();
		}
		if ( !virtualBrace && multiStatement )
		{
			if ( !isLS ) throw errorNum::scMISSING_CLOSE_BRACE;
			basicNode->pushNode ( new astNode ( errorNum::scMISSING_CLOSE_BRACE, src ) );
		}
		if ( !virtualBrace )
		{
			if ( multiStatement )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterBlock, src ) );	// unindent point
			} else
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::afterImpliedBlock, src ) ); 	// unindent point
				if ( isLS ) autoFormatPoints.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::info, astLSInfo::semanticLineBreakType::autoFormatPoint, src, formatStart ) );
			}
		}
		basicNode->setEnd ( src );
		return basicNode;
	} catch ( ... )
	{
		delete basicNode;
		throw;
	}
}

astNode *opFile::parseBlock ( source &src, opFunction *func, bool doBrace, bool virtualBrace, bool isLS, bool isAP, srcLocation const &formatStart )
{
	if ( doBrace )
	{
		return parseBlockSlang ( src, func, virtualBrace, isLS, isAP, formatStart );
	} else
	{
		return parseBlockFGL ( src, func, virtualBrace, isLS, isAP, formatStart );
	}
}

