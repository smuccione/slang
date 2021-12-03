/*
*	Does code formatting
*
*
*
*/


#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "LanguageServer/languageServer.h"
#include "compilerAst/astNodeWalk.h"

static auto comp = []( astNode const *left, astNode const *right ) {
	if ( left->location.sourceIndex == right->location.sourceIndex && left->location.lineNumberStart == right->location.lineNumberStart && left->location.columnNumber == right->location.columnNumber )
	{
		if ( left->getOp () == astOp::btStatement )
		{
			switch ( left->getSemanticType () )
			{
				case astLSInfo::semanticSymbolType::info:
					if ( right->getOp () == astOp::btStatement )
					{
						switch ( right->getSemanticType () )
						{
							case astLSInfo::semanticSymbolType::info:
								return *left < *right;
							default:
								return false;
						}
					}
					return true;
				default:
					return false;
			}
		}
		return false;
	} else return *left < *right;
};

struct sourceCache
{
	std::vector<char const *>		 linePtr;
	std::vector<size_t>				 lineLength;
	int64_t							 tabWidth;
	char const						*source = nullptr;

	sourceCache ( char const *source, int64_t tabWidth ) : source ( source ), tabWidth ( tabWidth )
	{
		auto ptr = source;
		size_t len = 0;
		linePtr.push_back ( source );
		while ( *ptr )
		{
			if ( *ptr == '\r' )
			{
				ptr++;
				if ( !*ptr ) break;
				lineLength.push_back ( len );
				if ( *ptr == '\n' )
				{
					ptr++;
				}
				linePtr.push_back ( ptr );
				len = 0;
			} else if ( *ptr == '\n' )
			{
				ptr++;
				lineLength.push_back ( len );
				linePtr.push_back ( ptr );
				len = 0;
			} else
			{
				len++;
				ptr++;
			}
		}
		lineLength.push_back ( len );
	}

	bool compare ( srcLocation const &loc, stringi const &newTxt )
	{
		auto line = loc.lineNumberStart - 1;
		auto lineEnd = loc.lineNumberEnd - 1;
		auto column = loc.columnNumber - 1;
		auto endColumn = loc.columnNumberEnd;

		auto ptr = linePtr[line] + column;
		auto ptrEnd = linePtr[lineEnd] + endColumn;

		if ( ptrEnd - ptr != newTxt.size () )
		{
			return false;
		}
		return !memcmp ( ptr, newTxt, newTxt.size () );
	}
	stringi getText ( srcLocation const &loc )
	{
		auto line = loc.lineNumberStart - 1;
		auto lineEnd = loc.lineNumberEnd - 1;
		auto column = loc.columnNumber - 1;

		stringi res;
		if ( line < lineEnd )
		{
			res += stringi ( linePtr[line] + column, (size_t)lineLength[line] - loc.columnNumber + 1 );
			line++;
			res += "\r\n";
			while ( line < lineEnd )
			{
				res += stringi ( linePtr[line], (size_t)lineLength[line] );
				line++;
				res += "\r\n";
			}
			res += stringi ( linePtr[line], loc.columnNumberEnd + 1 );
		} else
		{
			res = stringi ( linePtr[line] + column, (size_t)loc.columnNumberEnd - loc.columnNumber );
		}
		return res;
	}

	int64_t getIndent ( int64_t lineNum )
	{
		if ( !lineNum ) return 0;
		int64_t indent = 1;
		auto line = linePtr[lineNum - 1];
		while ( _isspace ( line ) )
		{
			if ( *line == '\t' )
			{
				indent += tabWidth;
				indent -= indent % tabWidth;
			}
			indent += 1;
			line++;
		}
		return indent;
	}

	int64_t getColumnNum ( int64_t lineNum, int64_t characterPos )
	{
		if ( !lineNum || !characterPos ) return 0;
		int64_t indent = 1;
		auto line = linePtr[lineNum - 1];
		while ( --characterPos )
		{
			if ( *line == '\t' )
			{
				indent += tabWidth;
				indent -= indent % tabWidth;
			}
			indent += 1;
			line++;
		}
		return indent;
	}
};

void insertSpace ( bool flag, BUFFER &buff )
{
	if ( flag ) buff.put ( ' ' );
}

void indentLine ( int64_t currentColumn, int64_t destinationColumn, BUFFER &buff, int64_t tabWidth, bool preferSpaces )
{
	if ( destinationColumn <= currentColumn ) return;

	if ( preferSpaces )
	{
		while ( currentColumn < destinationColumn )
		{
			buff.put ( ' ' );
			currentColumn++;
		}
	} else
	{
		while ( currentColumn < destinationColumn )
		{
			auto tabIndent = currentColumn;
			tabIndent += 4;
			tabIndent -= tabIndent % 4;
			if ( tabIndent < destinationColumn )
			{
				buff.put ( '\t' );
				currentColumn = tabIndent;
			} else
			{
				buff.put ( ' ' );
			}
			currentColumn++;
		}
	}
}

static astNode *formatNodeWalkCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, std::multiset<astNode *, decltype (comp)> &nodes, srcLocation const &loc )
{
	switch ( node->getOp () )
	{
		case astOp::btAnnotation:
		case astOp::btBasic:
		case astOp::btIf:
		case astOp::btWhile:
		case astOp::btForEach:
		case astOp::btFor:
		case astOp::btRepeat:
		case astOp::btBreak:
		case astOp::btCont:
		case astOp::btExit:
		case astOp::btInline:
		case astOp::btInlineRet:
		case astOp::btSwitch:
		case astOp::btReturn:
		case astOp::funcCall:
		case astOp::dummy:
		case astOp::arrayValue:
		case astOp::varrayValue:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
		case astOp::btLabel:
		case astOp::btGoto:
			break;
		case astOp::symbolDef:
			// we need to do this to ensure that the assigne node and lhs of an initializer are handled.   normally these would never be called as part of the astWalk
			switch ( node->symbolDef ()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( node->symbolDef ()->getInitializer ( node->symbolDef ()->getSymbolName () ) )
					{
						auto init = node->symbolDef ()->getInitializer ( node->symbolDef ()->getSymbolName () );
						if ( init->getOp () == astOp::assign )
						{
							// ast walker will call on the right hand side
							if ( init->location.sourceIndex == loc.sourceIndex )
							{
								nodes.insert ( init );
							}
							if ( init->left->location.sourceIndex == loc.sourceIndex )
							{
								nodes.insert ( init->left );
							}
						} else
						{
							if ( init->location.sourceIndex == loc.sourceIndex )
							{
								nodes.insert ( init );
							}
						}
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(node->symbolDef ());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							auto init = (*symbol)[it]->initializer;
							if ( init->location.sourceIndex == loc.sourceIndex )
							{
								if ( init->getOp () == astOp::assign )
								{
									// ast walker will call on the right hand side
									nodes.insert ( init );
									nodes.insert ( init->left );
								} else
								{
									nodes.insert ( init );
								}
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::atomValue:
			if ( node->location.sourceIndex == loc.sourceIndex )
			{
				if ( node->symbolValue () != sym->file->newValue )
				{
					nodes.insert ( node );
				}
			}
			break;
		default:
			if ( node->location.sourceIndex == loc.sourceIndex )
			{
				nodes.insert ( node );
			}
			break;
	}
	return node;
}

std::unordered_set<stringi>	capitalKeywords = {
												{ "CLASS" },
												{ "METHOD" },
												{ "PUBLIC" },
												{ "PRIVATE" },
												{ "NAMESPACE" },
												{ "LIBRARY" },
												{ "FUNCTION" },
												{ "END" },
												{ "ASSIGN" },
												{ "ACCESS" },
												{ "PROPERTY" },
												{ "GET" },
												{ "SET" },
};

char const *needsCapitalization ( stringi const &str )
{
	return capitalKeywords.find ( str ) != capitalKeywords.end () ? capitalKeywords.find ( str )->c_str () : nullptr;
}


std::pair<stringi, int32_t > reindentBlock ( stringi const &oldBlock, int64_t oldIndent, int64_t newIndent, bool indentLeading, int64_t tabWidth )
{
	std::vector<stringi>	lines;

	// switch form one's based to 0 based
	if ( newIndent < 0 ) newIndent = 0;
	if ( oldIndent < 0 ) oldIndent = 0;

	// split the comment into individual lines for processing
	auto ptr = oldBlock.c_str ();
	while ( *ptr )
	{
		stringi newLine;

		while ( *ptr )
		{
			if ( *ptr == '\r' )
			{
				ptr++;
				if ( *ptr == '\n' )
				{
					ptr++;
				};
				break;
			} else if ( *ptr == '\n' )
			{
				ptr++;
				break;
			}
			newLine += *(ptr++);
		}
		lines.push_back ( newLine );
	}

	// for each line, we want to remove the old indentation... this is NOT the entire indentation, but only as much as the start of the comment block was indented
	// we then want to re-indent based on the new indentation level

	stringi res;
	for ( auto &line : lines )
	{
		if ( res.size () )
		{
			res += "\r\n";
		}

		// remove the proper amount of leading old identation
		auto ptr = line.c_str ();
		int64_t currentIndent = 1;
		while ( currentIndent < oldIndent )
		{
			if ( *ptr == '\t' )
			{
				currentIndent += tabWidth;
				currentIndent -= currentIndent % tabWidth;
				currentIndent++;
			} else if ( *ptr == ' ' )
			{
				currentIndent++;
			} else
			{
				break;
			}
			ptr++;
		}

		// indent the starting line only when desired
		if ( res.size () || indentLeading )
		{
			int64_t currentColumn = 1;
			while ( currentColumn < newIndent )
			{
				auto tabIndent = currentColumn;
				tabIndent += 4;
				tabIndent -= tabIndent % 4;
				if ( tabIndent < newIndent )
				{
					res += '\t';
					currentColumn = tabIndent;
				} else
				{
					res += ' ';
				}
				currentColumn++;
			}
		}
		// add our line to the output
		res += ptr;
	}
	return { res, (int32_t)lines.size () ? (int32_t)lines.size () - 1 : 0 };
}

static int64_t getCurrentColumn ( BUFFER const &buff, int64_t tabWidth )
{
	int64_t column = 1;

	char const *ptr = buff.data<char const *> () + buff.size ();
	for ( int64_t index = 1; index <= (int64_t)buff.size (); index++ )
	{
		if ( index == (int64_t)buff.size () )
		{
			for ( ; index; index-- )
			{
				if ( ptr[-index] == '\t' )
				{
					column += tabWidth;
					column -= column % tabWidth;
				}
				column++;
			}
			return column;
		} else if ( ptr[-index] == '\r' || ptr[-index] == '\n' )
		{
			for ( index--; index; index-- )
			{
				if ( ptr[-index] == '\t' )
				{
					column += tabWidth;
					column -= column % tabWidth;
				}
				column++;
			}
			return column;
		}
	}
	return 0;
}

static bool needSpaceAfterValue ( std::multiset<astNode *, decltype (comp)>::iterator itNext )
{
	switch ( (*itNext)->getOp () )
	{
		case astOp::symbolValue:
		case astOp::stringValue:
		case astOp::compValue:
		case astOp::intValue:
		case astOp::doubleValue:
		case astOp::fieldValue:
		case astOp::funcValue:
		case astOp::senderValue:
		case astOp::nameValue:
		case astOp::nullValue:
		case astOp::atomValue:
		case astOp::preInc:
		case astOp::preDec:
		case astOp::getEnumerator:
		case astOp::negate:
		case astOp::comp:
		case astOp::twosComplement:
		case astOp::logicalNot:
		case astOp::refCreate:
		case astOp::paramExpand:
			return true;
			break;
		case astOp::btStatement:
			switch ( (*itNext)->getSemanticType () )
			{
				case astLSInfo::semanticSymbolType::keywordFlow:
				case astLSInfo::semanticSymbolType::keywordOther:
					return true;
					break;
			}
			break;
	}
	return false;
}

static auto getNextInfo ( std::multiset<astNode *, decltype (comp)>::iterator it, std::multiset<astNode *, decltype (comp)>::iterator itEnd )
{
	assert ( (*it)->getOp () == astOp::btStatement && (*it)->getSemanticType () == astLSInfo::semanticSymbolType::info );

	auto lastNext= it;
	while ( it != itEnd )
	{
		it = std::next ( it );
		if ( (*it)->getOp () == astOp::btStatement && (*it)->getSemanticType () == astLSInfo::semanticSymbolType::info )
		{
			return it;
		}
	}
	return lastNext;
}

static auto getPrevInfo ( std::multiset<astNode *, decltype (comp)>::iterator it, std::multiset<astNode *, decltype (comp)>::iterator itBegin )
{
	assert ( (*it)->getOp () == astOp::btStatement && (*it)->getSemanticType () == astLSInfo::semanticSymbolType::info );

	auto lastPrev = it;
	while ( it != itBegin )
	{
		it = std::prev ( it );
		if ( (*it)->getOp () == astOp::btStatement && (*it)->getSemanticType () == astLSInfo::semanticSymbolType::info )
		{
			return it;
		}
	}
	return lastPrev;
}


std::tuple<stringi, int64_t, srcLocation> opFile::formateCode ( astLSInfo const &info, char const *src, srcLocation const &loc, int64_t defaultIndent, languageRegion::languageId const languageId, int64_t tabWidth, bool preferSpaces )
{
	std::multiset<astNode *, decltype (comp)> nodes ( comp );

	languageServer::formatingFlags flags;

	symbolStack sym ( this );

	srcLocation retLocation = loc;

	for ( auto const &it : functionList )
	{
		if ( it.second->location.sourceIndex == loc.sourceIndex )
		{
			astNodeWalk ( it.second->codeBlock, &sym, formatNodeWalkCB, nodes, loc );

			for ( size_t param = it.second->params.size (); param; param-- )
			{
				if ( it.second->params[(uint32_t)param - 1]->name != selfValue )
				{
					auto init = it.second->params[(uint32_t)param - 1]->initializer;
					if ( init )
					{
						astNodeWalk ( init, &sym, formatNodeWalkCB, nodes, loc );
					}
				}
			}
		}
	}

	for ( auto &it : classList )
	{
		if ( it.second->location.sourceIndex == loc.sourceIndex )
		{
			for ( auto &elem : it.second->elems )
			{
				switch ( elem->type )
				{
					case fgxClassElementType::fgxClassType_iVar:
					case fgxClassElementType::fgxClassType_static:
						{
							auto init = elem->data.iVar.initializer;
							if ( init )
							{
								if ( init->getOp () == astOp::assign )
								{
									// the right hand side of the expression is handled via the genWrapper and it being inserted into the constructor.
//									astNodeWalk ( init, &sym, formatNodeWalkCB, nodes, loc );
//									nodes.insert ( init );
//									nodes.insert ( init->left );
								} else
								{
									nodes.insert ( init );
								}
							}
						}
						break;
					case fgxClassElementType::fgxClassType_inherit:
						break;
					case fgxClassElementType::fgxClassType_message:
						break;
				}
			}
		}
	}

	for ( auto &it : symbols )
	{
		if ( it.second.location.sourceIndex == loc.sourceIndex && it.second.isExportable )
		{
			astNodeWalk ( it.second.initializer, nullptr, formatNodeWalkCB, nodes, loc );
		}
	}

	for ( auto &it : statements )
	{
		if ( it.get ()->location.sourceIndex == loc.sourceIndex )
		{
			nodes.insert ( it.get () );
		}
	}

	BUFFER rsp;

	sourceCache code ( src, tabWidth );

	std::stack<int64_t>	indentStack;
	auto currentLine = loc.lineNumberStart;
	bool lineBreakEmitted = false;
	bool firstBreak = true;
	bool needSpace = false;
	bool initial = true;
	bool lastSpace = false;
	bool emitIndent = loc.columnNumber == 1 ? true : false;

	if ( !nodes.empty () )
	{
		auto searchNode = astNode ( astLSInfo::semanticSymbolType::funcCallEnd, loc );
		auto it = nodes.lower_bound ( &searchNode );

		auto &firstSearch = it;
		if ( firstSearch == nodes.end () ) firstSearch--;

		if ( it != nodes.begin () )
		{
			firstSearch--;
			if ( (*firstSearch)->location.lineNumberStart == (*it)->location.lineNumberStart )
			{
				firstBreak = false;
				emitIndent = false;
			}
		}

		lineBreakEmitted = firstBreak;

		if ( it != nodes.begin () ) it--;

		if ( defaultIndent == -1 )
		{
			while ( it != nodes.begin () )
			{
				if ( isInRegion ( (*it)->location ) )
				{
					auto controllingLine = (*it)->location.lineNumberStart;
					while ( it != nodes.begin () && (*--it)->location.lineNumberStart == controllingLine )
					{
					}
					if ( (*it)->location.lineNumberStart != controllingLine )
					{
						it++;
					}
					indentStack.push ( code.getIndent ( (*it)->location.lineNumberStart ) );
					break;
				}
				it--;
			}
			if ( indentStack.empty () )
			{
				switch ( languageRegions.begin ()->second )
				{
					case languageRegion::languageId::fgl:
					case languageRegion::languageId::slang:
						indentStack.push ( 1 );
						break;
					default:
						indentStack.push ( 5 );
						break;
				}
			}
		} else
		{
			indentStack.push ( defaultIndent );
		}

		std::erase_if ( nodes, [&loc]( auto v ) { return !loc.encloses ( *v ); } );

		if ( !nodes.empty () )
		{
			retLocation = (*nodes.begin ())->location;

			while ( !nodes.empty () )
			{
				auto last = std::prev ( nodes.end () );

				if ( (*last)->getOp () == astOp::btStatement && (*last)->getSemanticType () == astLSInfo::semanticSymbolType::info && (*last)->getSemanticLineBreakType () == astLSInfo::semanticLineBreakType::afterImpliedBlock )
				{
					nodes.erase ( last );
				} else
				{
					break;
				}
			}
			retLocation.setEnd ( (*std::prev ( nodes.end () ))->location );
		}

		currentLine = retLocation.lineNumberStart;
		auto lineNumStart = currentLine;

		for ( auto it = nodes.begin (); it != nodes.end (); it++ )
		{
			// for ease just have a next available
			// load any text for this node;
			auto txt = code.getText ( (*it)->location );

			// we're at the start of a line... we need to adjust the indentation points... we do NOT indent here, just push the value onto the stack!
			switch ( (*it)->getOp () )
			{
				case astOp::btStatement:
					switch ( (*it)->getSemanticType () )
					{
						case astLSInfo::semanticSymbolType::info:
							switch ( (*it)->getSemanticLineBreakType () )
							{
								case astLSInfo::semanticLineBreakType::beforeCase:
									if ( flags.indentCaseLabels )
									{
										indentStack.push ( indentStack.top () + tabWidth );
									} else
									{
										indentStack.push ( indentStack.top () );
									}
									break;
								case astLSInfo::semanticLineBreakType::beforeCaseContents:
									if ( flags.indentCaseContents )
									{
										indentStack.push ( indentStack.top () + tabWidth );
									} else
									{
										indentStack.push ( indentStack.top () );
									}
									break;
								case astLSInfo::semanticLineBreakType::afterCaseContents:
								case astLSInfo::semanticLineBreakType::afterCase:
								case astLSInfo::semanticLineBreakType::beforeEnd:
								case astLSInfo::semanticLineBreakType::afterBlock:
								case astLSInfo::semanticLineBreakType::afterImpliedBlock:
								case astLSInfo::semanticLineBreakType::afterLinq:
									if ( indentStack.size () > 1 )
									{
										indentStack.pop ();
									} else
									{
										indentStack.top () -= tabWidth;
									}
									break;
								case astLSInfo::semanticLineBreakType::afterBlockEnd:
								case astLSInfo::semanticLineBreakType::afterCondition:
								case astLSInfo::semanticLineBreakType::afterRepeat:
								case astLSInfo::semanticLineBreakType::afterEnd:
								case astLSInfo::semanticLineBreakType::beforeFor:
								case astLSInfo::semanticLineBreakType::beforeIf:
								case astLSInfo::semanticLineBreakType::beforeElse:
								case astLSInfo::semanticLineBreakType::beforeRepeat:
								case astLSInfo::semanticLineBreakType::beforeSwitch:
								case astLSInfo::semanticLineBreakType::beforeWhile:
								case astLSInfo::semanticLineBreakType::beforeDefault:
								case astLSInfo::semanticLineBreakType::beforeFunction:
								case astLSInfo::semanticLineBreakType::beforeElement:
								case astLSInfo::semanticLineBreakType::beforeClass:
								case astLSInfo::semanticLineBreakType::beforeScope:
								case astLSInfo::semanticLineBreakType::beforeStatement:
								case astLSInfo::semanticLineBreakType::afterFunction:
								case astLSInfo::semanticLineBreakType::afterClass:
								case astLSInfo::semanticLineBreakType::afterElement:
								case astLSInfo::semanticLineBreakType::afterScope:
								case astLSInfo::semanticLineBreakType::beforeCodeblock:
								case astLSInfo::semanticLineBreakType::afterCodeblock:
								case astLSInfo::semanticLineBreakType::beforeLabel:
								case astLSInfo::semanticLineBreakType::afterLabel:
								case astLSInfo::semanticLineBreakType::beforeTry:
								case astLSInfo::semanticLineBreakType::afterTry:
								case astLSInfo::semanticLineBreakType::beforeCatch:
								case astLSInfo::semanticLineBreakType::afterCatch:
								case astLSInfo::semanticLineBreakType::beforeFinally:
								case astLSInfo::semanticLineBreakType::afterFinally:
								case astLSInfo::semanticLineBreakType::beforeOpenBlock:
								case astLSInfo::semanticLineBreakType::beforeSwitchBody:
									break;
								case astLSInfo::semanticLineBreakType::afterOpenBlock:
								case astLSInfo::semanticLineBreakType::beforeImpliedBlock:
									if ( flags.indentNamespaceContents )
									{
										indentStack.push ( indentStack.top () + tabWidth );
									} else
									{
										indentStack.push ( indentStack.top () );
									}
									break;
								case astLSInfo::semanticLineBreakType::beforeNamespace:
									if ( flags.indentNamespaceContents )
									{
										indentStack.push ( indentStack.top () + tabWidth );
									} else
									{
										indentStack.push ( indentStack.top () );
									}
									break;
								case astLSInfo::semanticLineBreakType::afterNamespace:
									if ( indentStack.size () > 1 )
									{
										indentStack.pop ();
									} else
									{
										indentStack.top () -= tabWidth;
									}
									break;
								case astLSInfo::semanticLineBreakType::beforePerenContents:
									if ( flags.alignPerenthesis )
									{
										// for arrays we push TWO values on the indent stack... the first one being the position of the open oereb and the second one being the actual indent we want for subsequent values
										indentStack.push ( getCurrentColumn ( rsp, tabWidth ) - 1 );
										indentStack.push ( indentStack.top () + 1 + (flags.spaceBetweenFunctionNamePerenthesis ? 1 : 0) );
									}
									break;
								case astLSInfo::semanticLineBreakType::beforeArrayContents:
								case astLSInfo::semanticLineBreakType::beforeJsonContents:
									{
										// for arrays we push TWO values on the indent stack... the first one being the position of the open brace and the second one being the actual indent we want for subsequent values
										indentStack.push ( getCurrentColumn ( rsp, tabWidth ) - 1 );
										indentStack.push ( indentStack.top () + tabWidth );
									}
									break;
								case astLSInfo::semanticLineBreakType::afterPeren:
								case astLSInfo::semanticLineBreakType::afterPerenContents:
									if ( flags.alignPerenthesis )
									{
										if ( indentStack.size () > 1 )
										{
											indentStack.pop ();
										} else
										{
											indentStack.top () -= tabWidth;
										}
									}
									break;
								case astLSInfo::semanticLineBreakType::afterArray:
								case astLSInfo::semanticLineBreakType::afterArrayContents:
								case astLSInfo::semanticLineBreakType::afterJson:
								case astLSInfo::semanticLineBreakType::afterJsonContents:
									if ( indentStack.size () > 1 )
									{
										indentStack.pop ();
									} else
									{
										indentStack.top () -= tabWidth;
									}
									break;
								case astLSInfo::semanticLineBreakType::beforeLinq:
									indentStack.push ( getCurrentColumn ( rsp, tabWidth ) - 4 );	// 4 is the length of the word "FROM" which starts a linq statement.  It has nothing to do with tab width!!!
									break;
								default:
									assert ( 0 );
									break;
							}
					}
					break;
			}
			if ( indentStack.top () < 0 ) indentStack.top () = 0;

			//  we always need someting on the indent stack... if we don't we need to push what we have. 
			//  we may not have indent-stack entries if the user has entered multiple un-matched close blocks
			if ( indentStack.empty () )
			{
				indentStack.push ( code.getIndent ( (*it)->location.lineNumberStart ) );
			}

			/*
			*		Handle line breaks
			*
			*			the first thing we check is if we have a manually inserted break (e.g. the lineNumberStart is greater than the current line).
			*			    however we also don't want to simply ALWAYS do this.  There are times when we want to not emite a break, regardless and that is when
			*				we may want to do K&R (open brace after condition) or open-empty-close on same line
			*			if we haven't already emitted a line break we then check to see if we need to emit one by rule
			*
			*			when we do emite a line break we also clear any pending space needed flags
			*/
			if ( firstBreak || currentLine < (*it)->location.lineNumberStart )
			{
				firstBreak = false;
				while ( currentLine < (*it)->location.lineNumberStart )
				{
					rsp.write ( "\r\n" );
					currentLine++;
					if ( currentLine < (*it)->location.lineNumberStart )
					{
						auto blankText = code.getText ( { loc.sourceIndex, 1, currentLine, 1 + (uint32_t)code.lineLength[currentLine - 1], currentLine } );
						rsp.write ( blankText );
					}
					lineBreakEmitted = true;
					needSpace = false;
					lastSpace = true;
				}
			}

			if ( (*it)->getOp () == astOp::btStatement && (*it)->getSemanticType () == astLSInfo::semanticSymbolType::info )
			{
				if ( std::next ( it ) != nodes.end () && !lineBreakEmitted )
				{
					switch ( (*it)->getSemanticLineBreakType () )
					{
						case astLSInfo::semanticLineBreakType::afterCondition:
						case astLSInfo::semanticLineBreakType::afterRepeat:
							if ( (*std::next(it))->getOp () == astOp::btStatement && (*std::next ( it ))->getSemanticType () == astLSInfo::semanticSymbolType::info )
							{
								switch ( (*std::next ( it ))->getSemanticLineBreakType () )
								{
									case astLSInfo::semanticLineBreakType::beforeOpenBlock:
									case astLSInfo::semanticLineBreakType::beforeSwitchBody:
										if ( flags.newLineControlBrace )
										{
											rsp.write ( "\r\n" );
											lineBreakEmitted = true;
										}
										break;
									case astLSInfo::semanticLineBreakType::beforeImpliedBlock:
										// implied blocks are FGL entities and ALWAYS require a line break
										rsp.write ( "\r\n" );
										lineBreakEmitted = true;
										break;
									default:
										break;
								}
							}
							break;
						case astLSInfo::semanticLineBreakType::afterBlock:
							if ( !((*std::next ( it ))->getOp () == astOp::btStatement && (*std::next ( it ))->getSemanticType () == astLSInfo::semanticSymbolType::semicolon) )
							{
								auto nextInfo = getNextInfo ( it, nodes.end () );
								switch ( (*nextInfo)->getSemanticLineBreakType () )
								{
									case astLSInfo::semanticLineBreakType::beforeElse:
										if ( flags.newLineElse )
										{
											rsp.write ( "\r\n" );
											lineBreakEmitted = true;
										}
										break;
									case astLSInfo::semanticLineBreakType::beforeFinally:
										if ( flags.newLineFinally )
										{
											rsp.write ( "\r\n" );
											lineBreakEmitted = true;
										}
										break;
									case astLSInfo::semanticLineBreakType::beforeCatch:
										if ( flags.newLineWhile )
										{
											rsp.write ( "\r\n" );
											lineBreakEmitted = true;
										}
										break;
									case astLSInfo::semanticLineBreakType::beforeWhile:
										if ( flags.newLineWhile )
										{
											rsp.write ( "\r\n" );
											lineBreakEmitted = true;
										}
										break;
									default:
										rsp.write ( "\r\n" );
										lineBreakEmitted = true;
								}
							}
							break;
						case astLSInfo::semanticLineBreakType::afterImpliedBlock:
							if ( (*std::next ( it ))->getOp () != astOp::btStatement || (*std::next ( it ))->getSemanticType () != astLSInfo::semanticSymbolType::semicolon )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::beforeElse:
							if ( flags.newLineElse && !lineBreakEmitted )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::beforeFinally:
							if ( flags.newLineFinally && !lineBreakEmitted )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::beforeCatch:
							if ( flags.newLineWhile && !lineBreakEmitted )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::beforeWhile:
							if ( flags.newLineWhile && !lineBreakEmitted )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::beforeEnd:
						case astLSInfo::semanticLineBreakType::afterEnd:
						case astLSInfo::semanticLineBreakType::afterBlockEnd:
						case astLSInfo::semanticLineBreakType::beforeNamespace:
						case astLSInfo::semanticLineBreakType::beforeFor:
						case astLSInfo::semanticLineBreakType::beforeIf:
						case astLSInfo::semanticLineBreakType::beforeRepeat:
						case astLSInfo::semanticLineBreakType::beforeSwitch:
							rsp.write ( "\r\n" );
							lineBreakEmitted = true;
							break;
						case astLSInfo::semanticLineBreakType::afterNamespace:
							if ( flags.newLineWhile && !lineBreakEmitted )
							{
								auto prevInfo = getPrevInfo ( it, nodes.begin () );
								if ( (*prevInfo)->getSemanticLineBreakType () == astLSInfo::semanticLineBreakType::afterBlock )
								{
									if ( flags.newLineNamespaceBrace )
									{
										rsp.write ( "\r\n" );
										lineBreakEmitted = true;
									}
								} else if ( (*prevInfo)->getSemanticLineBreakType () == astLSInfo::semanticLineBreakType::afterImpliedBlock )
								{
									// implied blocks are FGL entities and ALWAYS require a line break
									rsp.write ( "\r\n" );
									lineBreakEmitted = true;
								}
							}
							break;
						case astLSInfo::semanticLineBreakType::afterCase:
						case astLSInfo::semanticLineBreakType::afterCaseContents:
							if ( !lineBreakEmitted )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::beforeCase:
						case astLSInfo::semanticLineBreakType::beforeCaseContents:
						case astLSInfo::semanticLineBreakType::beforeDefault:
						case astLSInfo::semanticLineBreakType::beforeFunction:
						case astLSInfo::semanticLineBreakType::beforeElement:
						case astLSInfo::semanticLineBreakType::beforeClass:
						case astLSInfo::semanticLineBreakType::beforeScope:
						case astLSInfo::semanticLineBreakType::beforeOpenBlock:
							if ( !lineBreakEmitted )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::afterOpenBlock:
							if ( !lineBreakEmitted )
							{
								rsp.write ( "\r\n" );
								lineBreakEmitted = true;
							}
							break;
						case astLSInfo::semanticLineBreakType::afterFunction:
						case astLSInfo::semanticLineBreakType::afterClass:
						case astLSInfo::semanticLineBreakType::afterElement:
						case astLSInfo::semanticLineBreakType::beforeJsonContents:
						case astLSInfo::semanticLineBreakType::afterJson:
						case astLSInfo::semanticLineBreakType::afterJsonContents:
						case astLSInfo::semanticLineBreakType::beforeImpliedBlock:
						case astLSInfo::semanticLineBreakType::afterScope:
						case astLSInfo::semanticLineBreakType::beforeCodeblock:
						case astLSInfo::semanticLineBreakType::afterCodeblock:
						case astLSInfo::semanticLineBreakType::beforeLabel:
						case astLSInfo::semanticLineBreakType::afterLabel:
						case astLSInfo::semanticLineBreakType::beforeTry:
						case astLSInfo::semanticLineBreakType::afterTry:
						case astLSInfo::semanticLineBreakType::afterCatch:
						case astLSInfo::semanticLineBreakType::afterFinally:
						case astLSInfo::semanticLineBreakType::beforeArrayContents:
						case astLSInfo::semanticLineBreakType::afterArrayContents:
						case astLSInfo::semanticLineBreakType::afterArray:
						case astLSInfo::semanticLineBreakType::beforePerenContents:
						case astLSInfo::semanticLineBreakType::afterPerenContents:
						case astLSInfo::semanticLineBreakType::afterPeren:
						case astLSInfo::semanticLineBreakType::beforeStatement:
						case astLSInfo::semanticLineBreakType::beforeSwitchBody:
						case astLSInfo::semanticLineBreakType::beforeLinq:
						case astLSInfo::semanticLineBreakType::afterLinq:
							break;
						default:
							assert ( 0 );
							break;
					}
				}
				if ( lineBreakEmitted )
				{
					needSpace = false;
					lastSpace = true;

					if ( (*std::next ( it ))->location.lineNumberStart > currentLine )
					{
						currentLine++;
					}
					continue;
				}
			}

			if ( lineBreakEmitted )
			{
				indentLine ( 1, indentStack.top (), rsp, tabWidth, preferSpaces );
			}

			if ( needSpace )
			{
				switch ( (*it)->getOp () )
				{
					case astOp::btStatement:
						switch ( (*it)->getSemanticType () )
						{
							case astLSInfo::semanticSymbolType::semicolon:
								// a semicolon halts a logical line and ends the need for a space so don't emit one.
								break;
							default:
								rsp.put ( ' ' );
								break;
						}
						break;
					default:
						rsp.put ( ' ' );
						break;
				}
				needSpace = false;
				lastSpace = true;
			}

			// we've now emitted proper line breaks, indentation and any necessary leading spaces from text output.
			switch ( (*it)->getOp () )
			{
				case astOp::btStatement:
					{
						switch ( (*it)->getSemanticType () )
						{
							case astLSInfo::semanticSymbolType::info:
								switch ( (*it)->getSemanticLineBreakType() )
								{
									case astLSInfo::semanticLineBreakType::beforeArrayContents:
									case astLSInfo::semanticLineBreakType::beforeJsonContents:
										needSpace = flags.spaceWithinArray;
										break;
									case astLSInfo::semanticLineBreakType::afterArrayContents:
									case astLSInfo::semanticLineBreakType::afterJsonContents:
										if ( !lastSpace )  insertSpace ( flags.spaceWithinArray, rsp );
										break;
									default:
										break;
								}
								break;
							case astLSInfo::semanticSymbolType::comment:
								if ( flags.preserveCommentIndentation )
								{
									auto column = getCurrentColumn ( rsp, tabWidth );
									auto indent = std::max ( column, (int64_t)(*it)->location.columnNumber );
									while ( column < (*it)->location.columnNumber )
									{
										rsp.put ( ' ' );
										column++;
									}
									auto res = reindentBlock ( txt, (*it)->location.columnNumber, indent, true, tabWidth );
									txt = std::get<0> ( res );
									currentLine += std::get<1> ( res );
									lastSpace = false;
								} else
								{
									auto res = reindentBlock ( txt, (*it)->location.columnNumber, indentStack.top (), !lineBreakEmitted, tabWidth );
									txt = std::get<0> ( res );
									currentLine += std::get<1> ( res );
								}
								rsp.write ( txt.c_str (), txt.length () );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::commaSeparator:
								if ( !lastSpace )  insertSpace ( flags.spaceBeforeCommas, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								if ( (*std::next ( it ))->getOp () == astOp::nilValue )
								{
									needSpace = flags.spaceBetweenAdjoiningCommas;
									lastSpace = false;
								} else
								{
									needSpace = flags.spaceAftereCommas;
									lastSpace = false;
								}
								break;
							case astLSInfo::semanticSymbolType::conditional:
								if ( !lastSpace )  insertSpace ( flags.spaceAroundConditionalOperators, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = flags.spaceAroundConditionalOperators;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::punctuation:
							case astLSInfo::semanticSymbolType::keywordFlow:
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = true;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::keywordOperator:
								if ( txt == "++" || txt == "--" )
								{
									if ( !lastSpace )  insertSpace ( flags.spaceBetweenUnaryOperatorAndOperand, rsp );
								}
								rsp.write ( txt.c_str (), txt.length () );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::keywordOther:
								if ( flags.capitalizeDeclarators  && languageId != languageRegion::languageId::slang )
								{
									auto capTxt = needsCapitalization ( txt );
									if ( capTxt )
									{
										rsp.write ( capTxt, strlen ( capTxt ) );
									} else
									{
										rsp.write ( txt.c_str (), txt.length () );
									}
								} else
								{
									rsp.write ( txt.c_str (), txt.length () );
								}
								needSpace = true;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::openPeren:
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = flags.spaceWithinPerenthesizedExpression;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::closePeren:
								if ( !lastSpace )  insertSpace ( flags.spaceWithinPerenthesizedExpression, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::funcCallStart:
								if ( !lastSpace ) insertSpace ( flags.spaceWithinArgumentList, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = flags.spaceWithinArgumentList;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::funcCallEnd:
								if ( !lastSpace )  insertSpace ( flags.spaceWithinArgumentList, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::startArrayAccess:
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = flags.spaceWithinArrayAccess;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::endArrayAccess:
								if ( !lastSpace )  insertSpace ( flags.spaceWithinArrayAccess, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::startJson:
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = flags.spaceWithinJSONObject;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::endJson:
								if ( !lastSpace )  insertSpace ( flags.spaceWithinJSONObject, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = needSpaceAfterValue ( std::next ( it ) );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::startFixedArray:
							case astLSInfo::semanticSymbolType::startVariableArray:
								rsp.write ( txt.c_str (), txt.length () );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::endFixedArray:
							case astLSInfo::semanticSymbolType::endVariableArray:
								rsp.write ( txt.c_str (), txt.length () );
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::label:
								if ( !lastSpace )  insertSpace ( true, rsp );
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = true;
								lastSpace = false;
								break;
							case astLSInfo::semanticSymbolType::cbParamSeparator:
							case astLSInfo::semanticSymbolType::endCodeblock:
							default:
								rsp.write ( txt.c_str (), txt.length () );
								needSpace = true;
								lastSpace = false;
								break;
						}
					}
					break;
				case astOp::singleLineComment:
					rsp.write ( txt.c_str (), txt.length () );
					lastSpace = false;
					break;
				case astOp::symbolValue:
				case astOp::stringValue:
				case astOp::compValue:
				case astOp::intValue:
				case astOp::doubleValue:
				case astOp::fieldValue:
				case astOp::funcValue:
				case astOp::senderValue:
				case astOp::nameValue:
				case astOp::nullValue:
				case astOp::atomValue:
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = needSpaceAfterValue ( std::next ( it ) );
					lastSpace = false;
					break;
				case astOp::add:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundOperators, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundOperators;
					lastSpace = false;
					break;
				case astOp::subtract:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundOperators, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundOperators;
					lastSpace = false;
					break;
				case astOp::multiply:
				case astOp::divide:
				case astOp::bitAnd:
				case astOp::bitOr:
				case astOp::bitXor:
				case astOp::shiftLeft:
				case astOp::shiftRight:
				case astOp::modulus:
				case astOp::logicalAnd:
				case astOp::logicalOr:
				case astOp::equal:
				case astOp::notEqual:
				case astOp::less:
				case astOp::lessEqual:
				case astOp::greater:
				case astOp::greaterEqual:
				case astOp::power:
				case astOp::max:
				case astOp::min:
				case astOp::equali:
				case astOp::workAreaStart:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundOperators, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundOperators;
					lastSpace = false;
					break;
				case astOp::codeBlockExpression:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundOperators, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundOperators;
					lastSpace = false;
					break;
				case astOp::conditional:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundConditionalOperators, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundConditionalOperators;
					lastSpace = false;
					break;
				case astOp::seq:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceBeforeCommas, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAftereCommas;
					lastSpace = false;
					break;
				case astOp::range:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundRangeOperator, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundRangeOperator;
					lastSpace = false;
					break;
				case astOp::selfSend:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceBeforeMemberOperator, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					lastSpace = false;
					lastSpace = false;
					break;
				case astOp::sendMsg:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceBeforeMemberOperator, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAfterMemberOperator;
					lastSpace = false;
					break;
				case astOp::errorValue:
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = true;
					lastSpace = false;
					break;
				case astOp::assign:
				case astOp::addAssign:
				case astOp::subAssign:
				case astOp::mulAssign:
				case astOp::powAssign:
				case astOp::modAssign:
				case astOp::divAssign:
				case astOp::bwAndAssign:
				case astOp::bwOrAssign:
				case astOp::bwXorAssign:
				case astOp::shLeftAssign:
				case astOp::shRightAssign:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundAssignmentOperators, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundAssignmentOperators;
					lastSpace = false;
					break;
				case astOp::preInc:
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceBetweenUnaryOperatorAndOperand || ((*std::next ( it ))->getOp () == astOp::add);
					lastSpace = false;
					break;
				case astOp::preDec:
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceBetweenUnaryOperatorAndOperand || ((*std::next ( it ))->getOp () == astOp::subtract);
					lastSpace = false;
					break;
				case astOp::getEnumerator:
				case astOp::negate:
				case astOp::comp:
				case astOp::twosComplement:
				case astOp::logicalNot:
				case astOp::refCreate:
				case astOp::paramPack:
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceBetweenUnaryOperatorAndOperand;
					lastSpace = false;
					break;
				case astOp::postInc:
				case astOp::postDec:
					rsp.write ( txt.c_str (), txt.length () );
					lastSpace = false;
					break;
				case astOp::pairValue:
					if ( !lineBreakEmitted && !lastSpace )  insertSpace ( flags.spaceAroundPairOperator, rsp );
					rsp.write ( txt.c_str (), txt.length () );
					needSpace = flags.spaceAroundPairOperator;
					lastSpace = false;
					break;
				case astOp::nilValue:
					break;
				default:
					// skip any potential first-line reset
					continue;
			}
			lineBreakEmitted = false;
		}
	}
	while ( currentLine < retLocation.lineNumberEnd )
	{
		rsp.write ( "\r\n" );
		currentLine++;
		if ( currentLine < retLocation.lineNumberEnd )
		{
			auto blankText = code.getText ( { retLocation.sourceIndex, 1, currentLine, 1 + (uint32_t)code.lineLength[currentLine - 1], currentLine } );
			rsp.write ( blankText );
		} else
		{
			if ( code.lineLength[currentLine - 1] )
			{
				auto blankText = code.getText ( { loc.sourceIndex, 1, currentLine, retLocation.columnNumberEnd + 1, currentLine } );
				rsp.write ( blankText );
			}
		}
		needSpace = false;
	}

	stringi retStr ( rsp.data<char const *> (), rsp.size () );
	if ( needSpace ) rsp.put ( ' ' );

	if ( !code.compare ( loc, retStr ) )
	{
		return { retStr, indentStack.top (), retLocation };
	} else
	{
		return { "", indentStack.top (), retLocation };
	}
}
