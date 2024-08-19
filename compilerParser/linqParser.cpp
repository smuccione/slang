/*

	block processor


*/

#include "fileParser.h"
#include "Utility/counter.h"

constexpr auto tidIdentifier = "$tid";

enum class linqStatementType {
	stNone	= 0,
	stFrom,
	stJoin,
	stLet,
	stWhere,
	stOrder,
	stSelect,
	stGroup,
};

const struct {
	char const		*name;
	int				 len;
	linqStatementType	 type;
} linqStatementName[] = {
							{
								"from",			4, linqStatementType::stFrom,
							},
							{
								"join",			4, linqStatementType::stJoin,
							},
							{
								"let",			3, linqStatementType::stLet,
							},
							{
								"where",		5, linqStatementType::stWhere,
							},
							{
								"orderby",		7, linqStatementType::stOrder,
							},
							{
								"select",		6, linqStatementType::stSelect,
							},
							{
								"group",		5, linqStatementType::stGroup,
							},
							{
								0,				0, linqStatementType::stFrom
							}
						};

struct linqOrderBy
{ 
	astNode *node = nullptr;
	bool ascending = true;
};

struct linqStatement 
{
	linqStatementType		type;
	srcLocation				statementLocation;				// necessary for inlay hints in the language server so we get the correct postion of the return type from the lambda for the statement
	struct {
		struct {
			astNode					*var = nullptr;
			astNode					*expr = nullptr;
		}	from;
		struct {
			astNode					*expr = nullptr;
		} where;
		struct {
			std::vector<linqOrderBy> order;
		} orderBy;
		struct {
			astNode					*var = nullptr;
			astNode					*in = nullptr;
			astNode					*on = nullptr;
			astNode					*equals = nullptr;
			astNode					*into = nullptr;
		} join;
		struct {
			astNode					*expr = nullptr;
			astNode					*by = nullptr;
			astNode					*into = nullptr;
		} group;
		struct {
			astNode					*var = nullptr;
			astNode					*expr = nullptr;
		} let;
		struct {
			astNode					*expr = nullptr;
			astNode					*into = nullptr;
		} select;
	};

	linqStatement()
	{
		type = linqStatementType::stNone;
	}

	linqStatement ( linqStatement &&old ) noexcept = default;
	linqStatement ( const linqStatement &old ) = delete;
	linqStatement ( linqStatementType type, srcLocation &loc ) : type ( type ), statementLocation ( loc )
	{
		// when we move to a union, put the code here to initialize the proper element
		switch ( type )
		{
			case linqStatementType::stFrom:
				from.expr = 0;
				break;
			case linqStatementType::stJoin:
				join.in = join.on = join.equals = 0;
				break;
			case linqStatementType::stLet:
				let.expr = 0;
				break;
			case linqStatementType::stWhere:
				where.expr = 0;
				break;
			case linqStatementType::stOrder:
				break;
			case linqStatementType::stSelect:
				select.expr = 0;
				break;
			case linqStatementType::stGroup:
				group.by = group.expr = 0;
				break;
			case linqStatementType::stNone:
				break;
		}
	}
};

astNode *opFile::makeLambda ( srcLocation &src, opFunction *func, std::vector<linqProjectionVars> &projVars, astNode *expr, astNode *secondParam )
{
	char		 tmpName[512];	
	opFunction	*lambda = new opFunction ( this );
	astNode		*node;

	sprintf_s ( tmpName, sizeof ( tmpName ), LAMBDA_ID "%s@%i:%u:%I64u", srcFiles.getName ( src.sourceIndex ).c_str ( ), src.lineNumberStart, src.columnNumber, GetUniqueCounter() );
	for ( auto ptr = tmpName; *ptr; ptr++ )
	{
		switch ( ptr[0] )
		{
			case ':':
			case '\\':
				ptr[0] = '_';
				break;
		}
	}

	lambda->conv			= fgxFuncCallConvention::opFuncType_Bytecode;
	lambda->classDef		= 0;
	lambda->location		= src;
	lambda->usingList		= ns.getUseList();
	lambda->name			= sCache.get ( tmpName );
	lambda->parentClosure	= func->name;
	lambda->isProcedure		= false;
	lambda->isLambda		= true;

	if ( projVars.size() > 1 )
	{
		symbolStack			sym ( this );
		symbolSpaceParams	param;

		for ( auto it = projVars.begin ( ); it != projVars.end ( ); it++ )
		{
			param.add ( it->name->symbolValue(), symVariantType, it->name->location, new astNode ( sCache, astOp::symbolValue, it->name->symbolValue () ), false, stringi () );
			it->name->location = srcLocation ();
		}
		sym.push ( &param );

		lambda->params.add ( new astNode ( sCache, astOp::symbolValue, tidIdentifier ), symVariantType, stringi() );

		expr = expr->symbolsToIVar ( sCache.get ( tidIdentifier ), sym, true );
	} else
	{
		lambda->params.add ( (new astNode ( sCache, astOp::symbolValue, projVars[0].name->symbolValue () ))->setLocation ( projVars[0].name->location ), symVariantType, stringi () );
		projVars[0].name->location = srcLocation ();
	}

	if ( secondParam )
	{
		lambda->params.add ( secondParam->symbolValue(), symVariantType, secondParam->location, (new astNode ( sCache, astOp::symbolValue, secondParam->symbolValue () ))->setLocation ( secondParam->location ), false, stringi () );
		secondParam->location = srcLocation ();
	}

	lambda->codeBlock = new astNode ( sCache, astOp::btBasic );
	astNode *ret = new astNode ( sCache, astOp::btReturn );
	ret->returnData().node = expr;
	ret->returnData().isYield = false;
	lambda->codeBlock->pushNode ( ret );

	node = new astNode ( sCache, astOp::funcValue, tmpName );

	ns.add ( lambda, true );
	return node;
}

static astNode *makeTransparentId ( opFile *file, opFunction *func, std::vector<linqProjectionVars> &projVars )
{
	astNode		*node;

	node = new astNode ( file->sCache, astOp::arrayValue );

	for ( auto it = projVars.begin(); it != projVars.end(); it++ )
	{
		node->arrayData().nodes.push_back ( new astNode(	file->sCache,
															astOp::pairValue, 
															(new astNode ( file->sCache, astOp::stringValue, it->name->symbolValue ().c_str() ))->setLocation ( it->name->location ),
															(*it).expr ? (*it).expr : new astNode ( file->sCache, astOp::symbolValue,  it->name->symbolValue ().c_str() )
												        )
										  );
		(*it).expr = 0;
	}

	return node;
}

astNode *opFile::getLinq ( source &src, opFunction *func, bool isLS )
{
	int							 loop;
	std::vector<linqStatement>	 linqNodes;
	astNode						*node;
	bool						 fatalParse = false;
	srcLocation					 statementLocation = src;

	try {
		loop = 0;
		goto parseFrom;

		while ( *src )
		{
			if ( *src == ';' )
			{
				if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
				src++;
				break;
			}
			if ( *src == ')' || *src == '}' )
			{
				break;
			}

			errHandler.setLine ( src );

			// TODO: right now we just throw away comments... can we do better and turn this into documentation?

			for ( loop = 0; linqStatementName[loop].name; loop++ )
			{
				if ( cmpTerminated ( src, linqStatementName[loop].name, linqStatementName[loop].len ) )
				{
					statementLocation = src;

					if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, linqStatementName[loop].len ) );
					(src) += linqStatementName[loop].len;

					switch ( linqStatementName[loop].type )
					{
						case linqStatementType::stFrom:
parseFrom:
							{
								linqStatement statement ( linqStatementType::stFrom, statementLocation );
								node = parseLinqExpr ( src, true, false, func, true, isLS );

								if ( !node )
								{
									if ( !isLS ) throw errorNum::scLINQ_EXPRESSION;
									errHandler.setFatal ();
									fatalParse = true;
								}
								BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

								if ( !cmpTerminated ( src, "in", 2 ) )
								{
									if ( (*src == ';') )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::semicolon, src ) );
										src++;
									} else  if ( *src == ')' || *src == '}' || *src == ']' )
									{
									} else
									{
										if ( !isLS )
										{
											throw errorNum::scLINQ_MISSING_IN;
										}
										errHandler.setFatal ();
										fatalParse = true;
									}
									if ( linqNodes.size() ) throw errorNum::scLINQ_MISSING_IN;

									// special case... support for
									// x = from y;  
									//
									// this forces conversion from y to an enumerable type (necessary until we implement extension methods)

									astNode *result;

									result = new astNode ( sCache, astOp::getEnumerator, node, (astNode *)0 );

									return result;
								}

								if ( node->getOp() != astOp::symbolValue )
								{
									if ( !isLS ) throw errorNum::scLINQ_SYMBOL_EXPECTED;
									errHandler.setFatal ();
									fatalParse = true;
								}

								statement.from.var = node;

								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 2 ) );	// IN
								(src) += 2;
								BS_ADVANCE_EOL_COMMENT ( this, isLS, src );

								statement.from.expr = parseLinqExpr ( src, true, false, func, true, isLS );

								linqNodes.push_back ( std::move ( statement ) );
							}
							break;
						case linqStatementType::stJoin:
							{
								linqStatement statement ( linqStatementType::stJoin, statementLocation );

								node = parseLinqExpr ( src, true, false, func, true, isLS );

								if ( node->getOp() != astOp::symbolValue )
								{
									if ( !isLS ) throw errorNum::scLINQ_SYMBOL_EXPECTED;
									errHandler.setFatal ();
									fatalParse = true;
								}

								statement.join.var = node;

								BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

								if ( cmpTerminated ( src, "in", 2 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 2 ) );
									(src) += 2;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

									statement.join.in = parseLinqExpr ( src, true, false, func, true, isLS );

									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
								}
								if ( cmpTerminated ( src, "on", 2 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 2 ) );
									(src) += 2;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

									statement.join.on = parseLinqExpr ( src, true, false, func, true, isLS );

									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
								}
								if ( cmpTerminated ( src, "equals", 6 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 6 ) );
									(src) += 6;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

									statement.join.equals = parseLinqExpr ( src, true, false, func, true, isLS );

									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
								}
								if ( cmpTerminated ( src, "into", 4 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 4 ) );
									(src) += 4;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

									node = parseLinqExpr ( src, true, false, func, true, isLS );
									if ( node->getOp() != astOp::symbolValue )
									{
										if ( !isLS ) throw errorNum::scLINQ_SYMBOL_EXPECTED;
										errHandler.setFatal ();
										fatalParse = true;
									}

									statement.join.into = node;

									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
								}
								linqNodes.push_back ( std::move ( statement ) );
							}
							break;
						case linqStatementType::stGroup:
							{
								linqStatement statement ( linqStatementType::stGroup, statementLocation );

								statement.group.expr = parseLinqExpr ( src, true, false, func, true, isLS );

								BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

								if ( !cmpTerminated ( src, "by", 2 ) )
								{
									if ( !isLS ) throw errorNum::scLINQ_MISSING_BY;
									errHandler.setFatal ();
									fatalParse = true;
								}
								if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 2 ) );
								(src) += 2;
								BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

								statement.group.by = parseLinqExpr ( src, true, false, func, true, isLS );

								BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

								if ( cmpTerminated ( src, "into", 4 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 4 ) );
									(src) += 4;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

									node = parseLinqExpr ( src, true, false, func, true, isLS );
									if ( node->getOp() != astOp::symbolValue )
									{
										if ( !isLS ) throw errorNum::scLINQ_SYMBOL_EXPECTED;
										errHandler.setFatal ();
										fatalParse = true;
									}

									statement.group.into = node;
								}
								linqNodes.push_back ( std::move ( statement ) );
							}
							break;
						case linqStatementType::stWhere:
							{
								linqStatement statement ( linqStatementType::stWhere, statementLocation );
								statement.where.expr = parseLinqExpr ( src, true, false, func, true, isLS );
								linqNodes.push_back ( std::move ( statement ) );
							}
							break;
						case linqStatementType::stLet:
							{
								linqStatement statement ( linqStatementType::stLet, statementLocation );
								statement.let.expr = parseLinqExpr ( src, true, false, func, true, isLS );
								if ( statement.let.expr->getOp() != astOp::assign )
								{
									if ( !isLS ) throw errorNum::scLINQ_INVALID_LET;
									errHandler.setFatal ();
									fatalParse = true;
								}
								if ( statement.let.expr->left->getOp() != astOp::symbolValue )
								{
									if( !isLS ) throw errorNum::scLINQ_INVALID_LET;
									errHandler.setFatal ();
									fatalParse = true;
								}
								statement.let.var = statement.let.expr->left;
								statement.let.expr->left = nullptr;

								linqNodes.push_back ( std::move ( statement ) );
							}
							break;
						case linqStatementType::stOrder:
							{
								linqStatement statement ( linqStatementType::stOrder, statementLocation );

								for ( ;; )
								{
									linqOrderBy	order;

									order.node = parseLinqExpr ( src, false, false, func, true, isLS );
									order.ascending = true;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
									if ( cmpTerminated ( src, "descending", 10 ) )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 10 ) );
										(src) += 10;
										order.ascending = false;
									} else if ( cmpTerminated ( src, "ascending", 9 ) )
									{
										if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 9 ) );
										(src) += 9;
									}

									statement.orderBy.order.push_back ( order );

									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
									if ( *src != ',' )
									{
										break;
									}
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::commaSeparator, src ) );
									src++;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
								} 
								linqNodes.push_back ( std::move ( statement ) );
							}
							break;
						case linqStatementType::stSelect:
							{
								linqStatement statement ( linqStatementType::stSelect, statementLocation );
								statement.select.expr = parseLinqExpr ( src, true, false, func, true, isLS );

								if ( cmpTerminated ( src, "into", 4 ) )
								{
									if ( isLS ) statements.push_back ( std::make_unique<astNode> ( astLSInfo::semanticSymbolType::keywordFlow, src, 4 ) );
									(src) += 4;
									BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );

									node = parseLinqExpr ( src, true, false, func, true, isLS );
									if ( node->getOp() != astOp::symbolValue )
									{
										if ( !isLS ) throw errorNum::scLINQ_SYMBOL_EXPECTED;
										errHandler.setFatal ();
										fatalParse = true;
									}

									statement.select.into = node;
								}

								linqNodes.push_back ( std::move ( statement ) );
							}
							break;
						case linqStatementType::stNone:
							break;
					}
					linqNodes.back ().statementLocation.setEnd ( src );
					break;
				}
			}
			if ( !linqStatementName[loop].name )
			{
				if ( !isLS ) throw errorNum::scILLEGAL_EXPRESSION;
				errHandler.setFatal ();
				fatalParse = true;
			}
			BS_ADVANCE_EOL_NO_SEMI ( this, isLS, src );
		}
	} catch ( ... )
	{
		assert ( !isLS );
		ADVANCE_EOL ( src );
		throw;
	}

	//	Generate the code 
	if ( (linqNodes[linqNodes.size() -1].type != linqStatementType::stSelect) && (linqNodes[linqNodes.size() -1].type != linqStatementType::stGroup) )
	{
		throw errorNum::scLINQ_TERMINATE;
	}

	astNode *result;

	if ( !fatalParse )
	{
		astNode *newNode;

		auto it = linqNodes.begin ();
		std::vector<linqProjectionVars> projVar;

		projVar.push_back ( (*it).from.var );

		result = new astNode ( sCache, astOp::getEnumerator, (*it).from.expr, (astNode *)0 );
		astPList *pList;

		for ( it++; it != linqNodes.end (); it++ )
		{
			decltype(it) itNext = it;
			itNext++;

			switch ( (*it).type )
			{

				case linqStatementType::stFrom:
					newNode = new astNode ( sCache, astOp::sendMsg );
					newNode->left = result;
					newNode->right = new astNode ( sCache, astOp::atomValue, "SelectMany" );

					result = new astNode ( sCache, astOp::funcCall, newNode );
					pList = &result->pList ();
					pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).from.expr ) );
					pList->paramRegion.push_back ( {} );

					if ( (*itNext).type == linqStatementType::stSelect )
					{
						// from x1 in e1 
						// from x2 in e2 
						// select v

						// ( e1 ). SelectMany ( x1 => e2, (x1, x2 ) => v )

						pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*itNext).select.expr, (*it).from.var ) );
						pList->paramRegion.push_back ( {} );

						it++;
					} else
					{
						// from x1 in e1 
						// from x2 in e2 
						// ...

						// ( e1 ). SelectMany ( x1 => e2, ( x1, x2 ) => { x1: x1, x2: x2 } )

						projVar.push_back ( (*it).from.var );
						astNode *arr = makeTransparentId ( this, func, projVar );
						projVar.pop_back ();

						pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, arr, (*it).from.var ) );
						pList->paramRegion.push_back ( {} );

						projVar.push_back ( (*it).from.var );
					}
					break;
				case linqStatementType::stLet:
					{
						newNode = new astNode ( sCache, astOp::sendMsg );
						newNode->left = result;
						newNode->right = new astNode ( sCache, astOp::atomValue, "Select" );

						result = new astNode ( sCache, astOp::funcCall, newNode );
						pList = &result->pList ();

						projVar.push_back ( linqProjectionVars ( (*it).let.var, (*it).let.expr->right ) );
						(*it).let.expr->right = 0;
						delete (*it).let.expr;
						(*it).let.expr = 0;

						while ( (*itNext).type == linqStatementType::stLet )
						{
							projVar.push_back ( linqProjectionVars ( (*itNext).let.var, (*itNext).let.expr->right ) );
							(*itNext).let.expr->right = 0;
							delete (*itNext).let.expr;
							(*itNext).let.expr = 0;
							itNext++;
						}

						astNode *arr = makeTransparentId ( this, func, projVar );

						itNext = it;
						while ( (*itNext).type == linqStatementType::stLet )
						{
							projVar.pop_back ();
							itNext++;
						}

						pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, arr ) );
						pList->paramRegion.push_back ( {} );

						for ( ;; )
						{
							projVar.push_back ( linqProjectionVars ( (*it).let.var ) );
							if ( (it + 1)->type != linqStatementType::stLet )
							{
								break;
							}
							it++;
						}
					}
					break;
				case linqStatementType::stWhere:
					{
						auto expr = (*it).where.expr;
						// combine multiple where clauses into a single compound predicate
						while ( (*itNext).type == linqStatementType::stWhere )
						{
							expr = new astNode ( sCache, astOp::logicalAnd, expr, (*itNext).where.expr );
							it++;
							itNext++;
						}
						newNode = new astNode ( sCache, astOp::sendMsg );
						newNode->left = result;
						newNode->right = new astNode ( sCache, astOp::atomValue, "Where" );

						result = new astNode ( sCache, astOp::funcCall, newNode );
						pList = &result->pList ();

						pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, expr ) );
						pList->paramRegion.push_back ( {} );
					}
					break;
				case linqStatementType::stOrder:
					{
						auto it2 = (*it).orderBy.order.begin ();

						newNode = new astNode ( sCache, astOp::sendMsg );
						newNode->left = result;
						if ( (*it2).ascending )
						{
							newNode->right = new astNode ( sCache, astOp::atomValue, "OrderBy" );
						} else
						{
							newNode->right = new astNode ( sCache, astOp::atomValue, "OrderByDescending" );
						}

						result = new astNode ( sCache, astOp::funcCall, newNode );
						pList = &result->pList ();

						pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it2).node ) );
						pList->paramRegion.push_back ( {} );

						for ( it2++; it2 != (*it).orderBy.order.end (); it2++ )
						{
							newNode = new astNode ( sCache, astOp::sendMsg );
							newNode->left = result;
							if ( (*it2).ascending )
							{
								newNode->right = new astNode ( sCache, astOp::atomValue, "ThenBy" );
							} else
							{
								newNode->right = new astNode ( sCache, astOp::atomValue, "ThenByDescending" );
							}
							result = new astNode ( sCache, astOp::funcCall, newNode );
							pList = &result->pList ();

							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it2).node ) );
							pList->paramRegion.push_back ( {} );
						}
					}
					break;
				case linqStatementType::stSelect:
					if ( (*it).select.expr->getOp () == astOp::symbolValue )
					{
						if ( projVar[0].name->symbolValue () == (*it).select.expr->symbolValue () )
						{
							// TODO: update when we're using a transparent identifier
							// trival case, do nothing
							delete (*it).select.expr;
							break;
						}
					}
					newNode = new astNode ( sCache, astOp::sendMsg );
					newNode->left = result;
					newNode->right = new astNode ( sCache, astOp::atomValue, "Select" );

					result = new astNode ( sCache, astOp::funcCall, newNode );
					pList = &result->pList ();

					pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).select.expr ) );
					pList->paramRegion.push_back ( {} );

					if ( (*it).select.into )
					{
						projVar.clear ();
						projVar.push_back ( (*it).select.into );
					}
					break;
				case linqStatementType::stGroup:
					// from x in y group x.Name by x.Country
					//
					// y.GroupBy(x => x.Country, x => c.Name)

					newNode = new astNode ( sCache, astOp::sendMsg );
					newNode->left = result;
					newNode->right = new astNode ( sCache, astOp::atomValue, "GroupBy" );

					result = new astNode ( sCache, astOp::funcCall, newNode );
					pList = &result->pList ();

					pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).group.by ) );
					pList->paramRegion.push_back ( {} );
					pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).group.expr ) );
					pList->paramRegion.push_back ( {} );

					if ( (*it).group.into )
					{
						projVar.clear ();
						projVar.push_back ( (*it).group.into );
					}
					break;
				case linqStatementType::stJoin:
					if ( (*it).join.into )
					{
						newNode = new astNode ( sCache, astOp::sendMsg );
						newNode->left = result;											// e1
						newNode->right = new astNode ( sCache, astOp::atomValue, "GroupJoin" );

						result = new astNode ( sCache, astOp::funcCall, newNode );
						pList = &result->pList ();

						if ( (*itNext).type == linqStatementType::stSelect )
						{
							// from x1 in e1
							// join x2 in e2 on k1 equals k2 into g
							// select v
							//
							// ( e1 ) . GroupJoin( e2 , x1 => k1 , x2 => k2 , ( x1, g ) => v )

							pList->param.push_back ( (*it).join.in );										// e2
							pList->paramRegion.push_back ( {} );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).join.on ) );		// x1 => k1
							pList->paramRegion.push_back ( {} );

							std::vector<linqProjectionVars> p2;
							p2.push_back ( (*it).join.var );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, p2, (*it).join.equals ) );		// x2 => k2
							pList->paramRegion.push_back ( {} );

							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*itNext).select.expr, (*it).join.into ) );		// (x1, g ) => v
							pList->paramRegion.push_back ( {} );

							it++;	// move to the select... we'll move off this at the end of the for loop
						} else
						{
							// from x1 in e1
							// join x2 in e2 on k1 equals k2 into g
							// …
							//
							// from * in ( e1 ) . GroupJoin ( e2 , x1 => k1 , x2 => k2 , ( x1 , g ) => new { x1 , g } )
							// …

							pList->param.push_back ( (*it).join.in );												// e2
							pList->paramRegion.push_back ( {} );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).join.on ) );		// x1 => k1
							pList->paramRegion.push_back ( {} );

							std::vector<linqProjectionVars> p2;
							p2.push_back ( (*it).join.var );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, p2, (*it).join.equals ) );		// x2 => k2
							pList->paramRegion.push_back ( {} );

							projVar.push_back ( (*it).join.into );
							astNode *arr = makeTransparentId ( this, func, projVar );	// new { x1 , g }
							projVar.pop_back ();

							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, arr, (*it).join.into ) );		// ( x1, g ) => new { x1 , g }
							pList->paramRegion.push_back ( {} );
						}
					} else
					{
						newNode = new astNode ( sCache, astOp::sendMsg );
						newNode->left = result;									// e1
						newNode->right = new astNode ( sCache, astOp::atomValue, "Join" );

						result = new astNode ( sCache, astOp::funcCall, newNode );
						pList = &result->pList ();

						if ( (*itNext).type == linqStatementType::stSelect )
						{
							// from x1 in e1
							// join x2 in e2 on k1 equals k2
							// select v
							//
							// ( e1 ) . Join( e2 , x1 => k1 , x2 => k2 , ( x1 , x2 ) => v )

							pList->param.push_back ( (*it).join.in );												// e2
							pList->paramRegion.push_back ( {} );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).join.on ) );		// x1 => k1
							pList->paramRegion.push_back ( {} );

							std::vector<linqProjectionVars> p2;
							p2.push_back ( (*it).join.var );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, p2, (*it).join.equals ) );		// x2 => k2
							pList->paramRegion.push_back ( {} );

							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*itNext).select.expr, (*it).join.var ) );		// (x1, x2) => v
							pList->paramRegion.push_back ( {} );

							it++;	// move to the select... we'll move off this at the end of the for loop
						} else
						{
							// from x1 in e1
							// join x2 in e2 on k1 equals k2 
							// …
							//
							// from * in ( e1 ) . Join( e2 , x1 => k1 , x2 => k2 , ( x1 , x2 ) => new { x1 , x2 } )
							// …
							pList->param.push_back ( (*it).join.in );												// e2
							pList->paramRegion.push_back ( {} );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, (*it).join.on ) );		// x1 => k1
							pList->paramRegion.push_back ( {} );

							std::vector<linqProjectionVars> p2;
							p2.push_back ( (*it).join.var );
							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, p2, (*it).join.equals ) );		// x2 => k2
							pList->paramRegion.push_back ( {} );

							projVar.push_back ( (*it).join.var );
							astNode *arr = makeTransparentId ( this, func, projVar );
							projVar.pop_back ();

							pList->param.push_back ( makeLambda ( (*it).statementLocation, func, projVar, arr, (*it).join.var ) );		// (x1, x2) => v
							pList->paramRegion.push_back ( {} );
						}
					}
					break;
				case linqStatementType::stNone:
					break;
			}
		}
	} else
	{
		assert ( isLS );
		// in the error case we need to keep all the language server information around somehow.  we do this by just returning a btBasic composed of everything that was parsed out
		// this allows semantic highlighting as such as well as code formattting to function
		result = new astNode ( sCache, astOp::btBasic );

		for ( auto const &it : linqNodes )
		{
			switch ( it.type )
			{

				case linqStatementType::stSelect:
					if ( it.select.expr ) result->pushNode ( it.select.expr );
					if ( it.select.into ) result->pushNode ( it.select.into );
					break;
				case linqStatementType::stLet:
					if ( it.let.var ) result->pushNode ( it.let.var );
					if ( it.let.expr ) result->pushNode ( it.let.expr );
					break;
				case linqStatementType::stWhere:
					if ( it.where.expr ) result->pushNode ( it.where.expr );
					break;
				case linqStatementType::stOrder:
					for ( auto const &orderIt : it.orderBy.order )
					{
						if ( orderIt.node ) result->pushNode ( orderIt.node );
					}
					break;
				case linqStatementType::stFrom:
					if ( it.from.var ) result->pushNode ( it.from.var );
					if ( it.from.expr ) result->pushNode ( it.from.expr );
					break;
				case linqStatementType::stGroup:
					if ( it.group.expr ) result->pushNode ( it.group.expr );
					if ( it.group.into ) result->pushNode ( it.group.into );
					if ( it.group.by ) result->pushNode ( it.group.by );
					break;
				case linqStatementType::stJoin:
					if ( it.join.var ) result->pushNode ( it.join.var );
					if ( it.join.in ) result->pushNode ( it.join.in );
					if ( it.join.on ) result->pushNode ( it.join.on );
					if ( it.join.equals ) result->pushNode ( it.join.equals );
					if ( it.join.into ) result->pushNode ( it.join.into );
					break;
				case linqStatementType::stNone:
					break;
			}
		}
	}
	return result;
}
