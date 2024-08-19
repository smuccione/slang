/* 
	Active Page preprocessor
*/

#include "Utility/settings.h"

#include <stdlib.h>
#include <stdint.h>
#include <stack>
#include <filesystem>

#include "compilerPreprocessor.h"
#include "Utility/buffer.h"
#include "Utility/funcky.h"
#include "Target/fileCache.h"
#include "LanguageServer/languageServer.h"

std::vector<stringi> semanticValues = {
#define DEF(value, mappedValue ) { stringi ( #mappedValue ) },
		SEMANTIC_TOKEN_TYPES
#undef DEF
};

// NOTE: we preceed all the outputString += statements with a ";"   this forces "! x +" to generate an error if the following line is a "!"

enum AP_COMP_STATE {
	apSTARTState,
	apHTMLState,
	apSLANGState,  
	apSLANGCodeState,
	apSLANGCodeCommentState,
	apSLANGCodeQuoteState,
	apSLANGCodeShortQuoteState,
	apSLANGHTMLState,
};

#define PUSH_STATE(newState) (stateStack.push ((newState)))

#define POP_STATE() {_popState ( buffer, stateStack, colNum, lineNum );}

using stateStack = std::stack < std::pair<AP_COMP_STATE, std::string>>;

static void _popState ( BUFFER *buffer, stateStack &stateStack, size_t colNum, size_t lineNum )
{
	if ( stateStack.size() )
	{
		auto [oldState, oldFileName] = stateStack.top ();
		stateStack.pop ();
		auto &[newState, newFileName] = stateStack.top ();

		switch ( oldState )
		{
			case apSLANGState:
				switch ( newState )
				{
					case apHTMLState:
					case apSLANGHTMLState:
						buffer->write ( "\r\n", 2 );
						if ( newFileName == oldFileName )
						{
							buffer->printf ( ";__outputString += [[pos: %zu %zu %s]]\"", colNum + 1, lineNum, newFileName.c_str () );
						} else
						{
							buffer->printf ( ";__outputString += [[pos: %zu %zu]]\"", colNum + 1, lineNum );
						}
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}
}

static struct BUFFER *apTranslatePage ( char const *fName, char const *inBuffer, size_t inLen, bool encapsulate, bool isSlang )
{
	BUFFER													*buffer = new BUFFER();
	stateStack												 stateStack;
	size_t													 stringLen = 0;
	std::filesystem::path									 fileName ( fName );
	std::vector<languageRegion>								 regions;
	std::vector < std::pair<srcLocation, stringi const &>>	 semanticTokens;

	fileName = std::filesystem::absolute ( fileName );
	if ( encapsulate )
	{
		buffer->write ( "[[pos: 0 0 (INTERNAL)]]global __outputString = new serverBuffer();\r\n", 45 );
	}

	PUSH_STATE ( std::pair ( apSTARTState, fileName.generic_string () ) );

	size_t lineNum = 1;
	size_t colNum = 1;
	size_t lastLineEmitted = 0;
	size_t loop = 0;

	regions.push_back ( languageRegion ( languageRegion::languageId::html, 0, colNum, lineNum ) );

	for ( ; loop < inLen - 1; loop++, colNum++ )
	{
		switch ( stateStack.top().first )
		{
			case apSTARTState:
				regions.back ().end ( colNum, lineNum );
				if ( inBuffer[loop] == '<' && inBuffer[loop+1] == '[' )
				{
					loop++;
					colNum++;
					PUSH_STATE ( std::pair ( apSLANGState, fileName.generic_string () ) );
					if ( isSlang )
					{
						regions.push_back ( languageRegion ( languageRegion::languageId::slang, 0, colNum + 1, lineNum ) );
					} else
					{
						regions.push_back ( languageRegion ( languageRegion::languageId::fgl, 0, colNum + 1, lineNum ) );
					}
				} else
				{
					buffer->printf ( "[[pos: 0 0 (INTERNAL)]];__outputString += [[pos: %zu %zu %s]]\"", colNum + 1, lineNum, fileName.generic_string ().c_str () );
					stringLen = 0;

					/* let the HTMLstate do the processing for this byte */
					loop--;
					colNum--;
					PUSH_STATE ( std::pair ( apHTMLState, fileName.generic_string () ) );
					if ( regions.back ().language != languageRegion::languageId::html )
					{
						regions.push_back ( languageRegion ( languageRegion::languageId::html, 0, colNum, lineNum ) );
					}
				}
				break;
			case apHTMLState:
				switch ( inBuffer[loop] )
				{
					case '<':
						/* start of slang section... */
						if ( inBuffer[loop + 1] == '[' )
						{
							regions.back ().end ( colNum - 1, lineNum );
							buffer->write ( "\"\r\n", 3 );
							loop++;
							colNum++;
							PUSH_STATE ( std::pair ( apSLANGState, fileName.generic_string () ) );
							if ( isSlang )
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::slang, 0, colNum + 1, lineNum ) );
							} else
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::fgl, 0, colNum + 1, lineNum ) );
							}
							if ( lineNum != lastLineEmitted + 1 )
							{
								buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
							}
							lastLineEmitted = lineNum;
						} else if ( !memcmpi (inBuffer + loop + 1, "script", 6) )
						{
							auto startChar = inBuffer + loop;
							auto tmpP = inBuffer + loop + 1 + 6;
							while ( (*tmpP == ' ') || (*tmpP == '\t') )
							{
								tmpP++;
							}
							if ( !memcmpi ( tmpP, "language", 8 ) )
							{
								tmpP += 8;

								while ( (*tmpP == ' ') || (*tmpP == '\t') )
								{
									tmpP++;
								}
								if ( *tmpP == '=' )
								{
									tmpP++;

									while ( (*tmpP == ' ') || (*tmpP == '\t') )
									{
										tmpP++;
									}

									if ( !memcmpi ( tmpP, "\"fgl\"", 5 ) || !memcmpi ( tmpP, "fgl", 3 ) || !memcmpi ( tmpP, "'fgl'", 5 ) )
									{
										if ( *tmpP == '\"' || *tmpP == '\'' )
										{
											tmpP += 2;	// for quotes
										}
										tmpP += 3;

										while ( (*tmpP == ' ') || (*tmpP == '\t') )
										{
											tmpP++;
										}

										if ( *tmpP == '>' )
										{
											loop = tmpP - inBuffer + 1;
											colNum += &inBuffer[loop] - startChar;
											buffer->write ( "\"\r\n", 3 );
											PUSH_STATE ( std::pair ( apSLANGState, fileName.generic_string () ) );
											regions.push_back ( languageRegion ( languageRegion::languageId::fgl, 0, colNum + 1, lineNum ) );

											if ( lineNum != lastLineEmitted + 1 )
											{
												buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
											}
											lastLineEmitted = lineNum;
										} else
										{
											buffer->put ( inBuffer[loop] );
										}
									} else if ( !memcmpi ( tmpP, "\"slang\"", 7 ) || !memcmpi ( tmpP, "slang", 5 ) || !memcmpi ( tmpP, "'slang'", 7 ) )
									{
										if ( *tmpP == '\"' || *tmpP == '\'' )
										{
											tmpP += 2;	// for quotes
										}
										tmpP += 5;

										while ( (*tmpP == ' ') || (*tmpP == '\t') )
										{
											tmpP++;
										}

										if ( *tmpP == '>' )
										{
											loop = tmpP - inBuffer + 1;
											colNum += &inBuffer[loop] - startChar;
											buffer->write ( "\"\r\n", 3 );
											PUSH_STATE ( std::pair ( apSLANGState, fileName.generic_string () ) );
											regions.push_back ( languageRegion ( languageRegion::languageId::slang, 0, colNum + 1, lineNum ) );
											if ( lineNum != lastLineEmitted + 1 )
											{
												buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
											}
											lastLineEmitted = lineNum;
										} else
										{
											buffer->put ( inBuffer[loop] );
										}
									} else
									{
										buffer->put ( inBuffer[loop] );
									}
								} else
								{
									buffer->put ( inBuffer[loop] );
								}
							} else
							{
								buffer->put ( inBuffer[loop] );
							}
						} else
						{
							buffer->put ( inBuffer[loop] );
						}
						break;
					case '\r':
						lineNum++;
						colNum = 0;
						buffer->write ( "\\r",2 );
						stringLen += 2;
						if ( inBuffer[loop + 1] == '\n' )
						{
							buffer->write ( "\\n",2 );
							stringLen += 2;
							loop++;
						}
						break;
					case '\n':
						lineNum++;
						colNum = 0;
						buffer->write ( "\\n",2 );
						stringLen += 2;
						break;
					case '\t':
						buffer->write ( "\\t",2 );
						stringLen += 2;
						break;
					case '\'':
						buffer->write ( "\\'",2 );
						stringLen += 2;
						break;
					case '\"':
						buffer->write ( "\\\"",2 );
						stringLen += 2;
						break;
					case '\\':
						buffer->write ( "\\\\",2 );
						stringLen += 2;
						break;
					default:
						buffer->put ( inBuffer[loop] );

						if ( ++stringLen >= 65500 )
						{
							// strings getting to big so split it
							buffer->write ( "\"\r\n", 3 );
							buffer->printf ( "[[pos: 0 0 (INTERNAL)]];__outputString += [[pos: %zu %zu %s]]\"", colNum + 1, lineNum, fileName.generic_string ().c_str () );
							stringLen = 0;
						}
						break;
				}
				break;
			case apSLANGHTMLState:
				switch ( inBuffer[loop] )
				{
					case '<':
						/* start of slang section... */
						if ( inBuffer[loop + 1] == '[' )
						{
							regions.back ().end ( colNum - 1, lineNum );
							loop++;
							colNum++;
							buffer->write ( "\"\r\n", 3 );
							PUSH_STATE ( std::pair ( apSLANGState, fileName.generic_string () ) );
							if ( isSlang )
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::slang, 0, colNum + 1, lineNum ) );
							} else
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::fgl, 0, colNum + 1, lineNum ) );
							}
							if ( lineNum != lastLineEmitted + 1 )
							{
								buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
							}
							lastLineEmitted = lineNum;
						} else
						{
							buffer->put ( inBuffer[loop] );
						}
						break;
					case '%':
						/* end of the slang html state */
						if ( inBuffer[loop + 1] == '>' )
						{
							regions.back ().end ( colNum - 1, lineNum );
							buffer->write ( "\"\r\n", 3 );
							loop++; // advance over the %> tag... will get an extra ++ from loop
							colNum++;
							POP_STATE();
							if ( isSlang )
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::slang, 0, colNum + 1, lineNum ) );
							} else
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::fgl, 0, colNum + 1, lineNum ) );
							}
							if ( lineNum != lastLineEmitted + 1 )
							{
								buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
							}
							lastLineEmitted = lineNum;
						} else
						{
							buffer->put ( inBuffer[loop] );
						}
						break;
					case '\r':
						lineNum++;
						colNum = 0;
						buffer->write ( "\\r",2 );
						stringLen += 2;
						if ( inBuffer[loop + 1] == '\n' )
						{
							buffer->write ( "\\n",2 );
							stringLen += 2;
							loop++;
						}
						break;
					case '\n':
						lineNum++;
						colNum = 0;
						buffer->write ( "\\n",2 );
						stringLen += 2;
						break;
					case '\t':
						buffer->write ( "\\t",2 );
						stringLen += 2;
						break;
					case '\'':
						buffer->write ( "\\'",2 );
						stringLen += 2;
						break;
					case '\"':
						buffer->write ( "\\\"",2 );
						stringLen += 2;
						break;
					case '\\':
						buffer->write ( "\\\\",2 );
						stringLen += 2;
						break;
					default:
						buffer->put ( inBuffer[loop] );
						if ( ++stringLen >= 65500 )
						{
							// strings getting to big so split it
							buffer->write ( "\"\r\n", 3 );
							buffer->printf ( "[[pos: 0 0 (INTERNAL)]];__outputString += [[pos: %zu %zu %s]]\"", colNum + 1, lineNum, fileName.generic_string ().c_str () );
							stringLen = 0;
						}
						break;
				}
				break;
			case apSLANGState:
		     	switch ( inBuffer[loop] )
				{
					case ']':
						if ( inBuffer[loop+1] == '>' )
						{
							regions.back ().end ( colNum - 1, lineNum );
							loop++; // advance over the ]> tag... will get an extra ++ from loop
							colNum++;
							POP_STATE();
							regions.push_back ( languageRegion ( languageRegion::languageId::html, 0, colNum + 1, lineNum ) );
						} else
						{
							buffer->put ( inBuffer[loop] );
						}
						break;
					case '<':
						if ( inBuffer[loop+1] == '%' )
						{
							regions.back ().end ( colNum - 1, lineNum );
							buffer->printf ( "[[pos: 0 0 (INTERNAL)]];__outputString += [[pos: %zu %zu $s]]\"", colNum + 1, lineNum, fileName.generic_string ().c_str () );
							stringLen = 0;
							loop++;
							colNum++;
							PUSH_STATE ( std::pair ( apSLANGHTMLState, fileName.generic_string () ) );
							regions.push_back ( languageRegion ( languageRegion::languageId::html, 0, colNum + 1, lineNum ) );
						} else if ( !memcmpi ( inBuffer + loop + 1, "/script>", 8 ) )
						{
							regions.back ().end ( colNum - 1, lineNum );
							loop += 8;
							colNum += 8;
							POP_STATE ();
							regions.push_back ( languageRegion ( languageRegion::languageId::html, 0, colNum + 1, lineNum ) );
						} else
						{
							buffer->put ( inBuffer[loop] ); 
						}
						break;
					case '!':
						semanticTokens.push_back ( { srcLocation ( 0, (uint32_t)colNum, (uint32_t)lineNum, (uint32_t)colNum + 1, (uint32_t)lineNum ), semanticValues[(size_t)astLSInfo::semanticSymbolType::punctuation] } );
						buffer->printf ( "[[pos: 0 0 (INTERNAL)]];__outputString += [[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
						stringLen = 0;
						PUSH_STATE ( std::pair ( apSLANGCodeState, fileName.generic_string () ) );
						break;
					case '\t':
					case ' ':
						/* this is leading crap.. we don't really need to emit this but do just for readability*/
						buffer->put ( inBuffer[loop] ); 
						break;
					case '\r':
						lineNum++;
						colNum = 0;
						buffer->put ( inBuffer[loop] ); 
						if ( inBuffer[loop + 1] == '\n' )
						{
							buffer->put ( inBuffer[loop+ 1] );
							loop++;
						}
						if ( lineNum != lastLineEmitted + 1 )
						{
							buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
						}
						lastLineEmitted = lineNum;
						break;
					case '\n':
						/* fix for unix style files (\n with no \r preceeding) */
						lineNum++;
						colNum = 0;
						if ( lineNum != lastLineEmitted + 1 )
						{
							buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
						}
						lastLineEmitted = lineNum;
						break;
					case '/':
						if ( inBuffer[loop+1] == '/' )
						{
							buffer->put ( inBuffer[loop] );
							buffer->put ( inBuffer[loop] );
							loop++;
							colNum++;
							PUSH_STATE ( std::pair ( apSLANGCodeCommentState, fileName.generic_string () ) );
						} else
						{
							buffer->put ( inBuffer[loop] ); 
						}
						break;
					default:
						loop--;
						colNum--;
						PUSH_STATE ( std::pair ( apSLANGCodeState, fileName.generic_string () ) );
						break;
				}
				break;
			case apSLANGCodeState:
				switch ( inBuffer[loop] )
				{
					case ']':
						if ( inBuffer[loop+1] == '>' )
						{
							regions.back ().end ( colNum, lineNum );
							loop++;
							colNum++;
							regions.push_back ( languageRegion ( languageRegion::languageId::html, 0, colNum + 3, lineNum ) );
							POP_STATE();	/* op the apSlangCodeState needs to handle the end */
							POP_STATE ();	/* another to get rid of the apSlangState */
						} else
						{
							buffer->put ( inBuffer[loop] );
						}
						break;
					case '<':
						if ( inBuffer[loop+1] == '%' )
						{
							buffer->printf ( "[[pos: 0 0 (INTERNAL)]];__outputString += [[pos: %zu %zu %s]]\"", colNum + 1, lineNum, fileName.generic_string ().c_str () );
							stringLen = 0;

							regions.back ().end ( colNum - 1, lineNum );
							loop++;
							colNum++;
							PUSH_STATE ( std::pair ( apSLANGHTMLState, fileName.generic_string () ) );
							if ( isSlang )
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::slang, 0, colNum + 1, lineNum ) );
							} else
							{
								regions.push_back ( languageRegion ( languageRegion::languageId::fgl, 0, colNum + 1, lineNum ) );
							}
						} else
						{
							buffer->put ( inBuffer[loop] ); 
						}
						break;
					case '/':
						if ( inBuffer[loop+1] == '/' )
						{
							buffer->put ( inBuffer[loop] );
							buffer->put ( inBuffer[loop] );
							PUSH_STATE ( std::pair ( apSLANGCodeCommentState, fileName.generic_string () ) );
							loop++;
							colNum++;
						} else
						{
							buffer->put ( inBuffer[loop] ); 
						}
						break;
					case '\"':
						buffer->put ( inBuffer[loop] );
						PUSH_STATE ( std::pair ( apSLANGCodeQuoteState, fileName.generic_string () ) );
						break;
					case '\'':
						buffer->put ( inBuffer[loop] );
						PUSH_STATE ( std::pair ( apSLANGCodeShortQuoteState, fileName.generic_string () ) );
						break;
					case '\r':
						lineNum++;
						colNum = 0;
						POP_STATE ();
						buffer->put ( inBuffer[loop] );
						if ( inBuffer[loop + 1] == '\n' )
						{
							buffer->put ( inBuffer[loop+ 1] );
							loop++;
						}
						if ( lineNum != lastLineEmitted + 1 )
						{
							buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str() );
						}
						lastLineEmitted = lineNum;
						break;

					case '\n':
						/* the only way we should ever get here is if we encounter a \n with no preceeding \r
						   this is a unix file... we shouldn't get here unless the editor f's up
						*/
						lineNum++;
						colNum = 0;
						POP_STATE ();
						if ( lineNum != lastLineEmitted + 1 )
						{
							buffer->printf ( "[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
						}
						lastLineEmitted = lineNum;
						break;
					default:
						buffer->put ( inBuffer[loop] ); 
						break;
				}
				break;
			case apSLANGCodeShortQuoteState:
				switch ( inBuffer[loop] )
				{
					case '\'':
						buffer->put ( inBuffer[loop] );
						POP_STATE();
						break;
					default:
						buffer->put ( inBuffer[loop] );
						break;
				}
				break;
			case apSLANGCodeCommentState:
				switch ( inBuffer[loop] )
				{
					case '\r':	// NOLINT (bugprone-branch-clone)
						// normal windows case... \r\n
						loop--;
						colNum--;
						POP_STATE ();
						break;

					case '\n':
						// \n with no preceeding \r
						loop--;
						colNum--;
						POP_STATE ();
						break;
					default:
						buffer->put ( inBuffer[loop] );
						break;
				}
				break;
			case apSLANGCodeQuoteState:
				switch ( inBuffer[loop] )
				{
					case '\\':
						buffer->put ( inBuffer[loop] );
						buffer->put ( inBuffer[loop + 1] );
						loop++;
						colNum++;
						break;
					case '\"':
						buffer->put ( inBuffer[loop] );
						POP_STATE ();
						break;
					default:
						buffer->put ( inBuffer[loop] );
						break;
				}
				break;
		}
	}

	if ( loop < inLen )
	{
		switch ( inBuffer[loop] )
		{
			case '\"':
				buffer->write ( "\\\"",2 );
				POP_STATE ();
				break;
			case '\r':
				lineNum++;
				buffer->write ( "\\r",2 );
				break;
			case '\n':
				buffer->write ( "\\n",2 );
				break;
			case '\t':
				buffer->write ( "\\t",2 );
				break;
			case '\'':
				buffer->write ( "\\'",2 );
				break;
			case '\\':
				buffer->write ( "\\\\",2 );
				break;
			default:
				buffer->put ( inBuffer[loop] );
				break;
		}
	}

	while ( stateStack.size() )
	{
		switch ( stateStack.top().first )
		{
			case apHTMLState:
			case apSLANGCodeQuoteState:
				buffer->write ( "\"\r\n", 3 );
				break;
			default:
				break;
		}
		stateStack.pop ();
	}

	regions.back ().end ( colNum, lineNum );

	if ( encapsulate )
	{
		buffer->write ( "\r\nreturn 1;\r\n", 13 );	// make sure we take in the NULL to terminate the string!
	} else
	{
		buffer->write ( "\r\n", 2 );
	}

	regions.back ().location.lineNumberEnd++;	// because we're going to add one additional CR at the end of the pragma's which will increase the range by one line.
	regions.back ().location.columnNumber = 1;
	regions.back ().location.columnNumberEnd = 1;
	for ( auto &it : regions )
	{
		buffer->printf ( "\r\n[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
		buffer->write ( "#pragma region ", 15 );
		switch ( it.language )
		{
			case languageRegion::languageId::html:
				buffer->write ( "html ", 5 );
				break;
			case languageRegion::languageId::slang:
				buffer->write ( "slang ", 6 );
				break;
			case languageRegion::languageId::fgl:
				buffer->write ( "fgl ", 4 );
				break;
		}
		buffer->printf ( "%u %u %u %u", it.location.columnNumber, it.location.lineNumberStart, it.location.columnNumberEnd, it.location.lineNumberEnd );
	}

	for ( auto &[location, tokenType] : semanticTokens )
	{
		buffer->printf ( "\r\n[[pos: %zu %zu %s]]", colNum + 1, lineNum, fileName.generic_string ().c_str () );
		buffer->printf ( "#pragma token %s %u %u %u %u", tokenType.c_str(), location.columnNumber, location.lineNumberStart, location.columnNumberEnd, location.lineNumberEnd );
	}
	buffer->put ( 0 );
//	buffer->write ( "\r\n", 3 );

	return buffer;
}

char *compAPPreprocessor ( char const *fName, bool isSlang, bool encapsulate )
{
	BUFFER		 buffer ( 1024ull * 1024 * 4 );
	BUFFER		*apBuff;
	HANDLE		 hFile;
	DWORD		 fileSize;
	char		*ret;

	// do NOT use the file cache to read in the source!  there is no need to do this, we only ever need to read it in once!
	// we *do* however need to monitor the path
	// by monitoring we then check the apx for source file changes

	if ( (hFile = CreateFile ( fName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0 )) == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		return 0;
	}
	bufferMakeRoom ( &buffer, GetFileSize ( hFile, 0 ) );
	ReadFile ( hFile, bufferBuff ( &buffer ), GetFileSize ( hFile, 0 ), &fileSize, 0 );
	bufferAssume ( &buffer, fileSize );
	CloseHandle ( hFile );

	globalFileCache.monitor ( fName );

	apBuff = apTranslatePage ( fName, bufferBuff ( &buffer ), bufferSize ( &buffer ), encapsulate, isSlang );

	ret = compPreprocessor ( fName, bufferBuff ( apBuff ), isSlang );
	
	delete apBuff;

	return ret;
}

char *compAPPreprocessBuffer ( char const *fName, char const *src, bool isSlang, bool encapsulate )
{
	BUFFER		*apBuff;
	char *ret;

	apBuff = apTranslatePage ( fName, src, strlen ( src ), encapsulate, isSlang );

	ret = compPreprocessor ( fName, bufferBuff ( apBuff ), isSlang );

	delete apBuff;

	return ret;
}
