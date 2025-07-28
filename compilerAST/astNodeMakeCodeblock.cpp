/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"
#include "compilerParser/classParser.h"
#include "astNodeWalk.h"
#include "bcVM/fglTypes.h"

static astNode *compileCodeBlockCB ( astNode *node, astNode *parent, symbolStack *parentSym, bool isAccess, bool isInFunctionCall, class errorHandler *err )
{
	switch ( node->getOp() )
	{
		case astOp::codeBlockExpression:
			{
				opFunction			 oFunc ( parentSym->file );
				symbolSpaceParams	*symbols = new symbolSpaceParams ( );
				opFile				 file;

				source src ( &file.srcFiles, file.sCache, parentSym->file->srcFiles.getName ( node->location.sourceIndex), node->stringValue(), sourceFile::sourceFileType::none, node->location.lineNumberStart );

				if ( *src != '{' )
				{
					throw errorNum::scMISSING_OPEN_BRACE;
				}
				src++;	// go past the starting {

				BS_ADVANCE_EOL_COMMENT ( &file, false, src );

				if ( *src != '|' )
				{
					throw errorNum::scMISSING_OPEN_BRACE;
				}
				src++;
				BS_ADVANCE_EOL_COMMENT ( &file, false, src );

				while ( *src && (*src != '|') )
				{
					auto param = parentSym->file->getNode ( src, opFile::lastOpType::binary, &oFunc, true, false, false );
					if ( param->getOp() != astOp::symbolValue )
					{
						delete param;
						throw errorNum::scILLEGAL_IDENTIFIER;
					}

					symbols->add ( param->symbolValue(), symVariantType, srcLocation (), param, false, stringi() );

					BS_ADVANCE_EOL_COMMENT ( &file, false, src );

					if ( (*src != '|') && (*src != ',') )
					{
						throw errorNum::scCODEBLOCK_PARAMETERS;
					}

					if ( *src == ',' )
					{
						src++;
					}
				}

				if ( *src )
				{
					src++;
				}

				auto nParams = (uint32_t) symbols->size ( );

				auto cbNode = parentSym->file->_parseExpr ( src, true, false, false, false, false, 0, false, false, false );

				if ( *src != '}' )
				{
					throw errorNum::scMISSING_CLOSE_BRACE;
				}
				src++;

				vmCodeBlock		*cb = 0;
				astNode			*retNode;

				try
				{
					oFunc.params = *symbols;

					symbolSpaceLocalAdd localAdd;
					symbolStack		sym ( parentSym->file, &oFunc, &localAdd );
					compExecutable		comp ( parentSym->file );

					node->release ( );
					*node = astNode ( sym.file->sCache, astOp::codeBlockValue );

					auto exp = &node->codeBlock();

					comp.fixup = new compFixup;

					retNode = new astNode ( sym.file->sCache, astOp::btReturn, srcLocation ( src ) );
					retNode->returnData().isYield = false;
					retNode->returnData().node = cbNode;

					astNode *codeBlock = new astNode ( sym.file->sCache, astOp::btBasic );
					codeBlock->pushNode ( retNode );
					codeBlock->pushNode ( new astNode ( sym.file->sCache, astOp::btLabel, "$ret" ) );
					codeBlock->pushNode ( new astNode ( sym.file->sCache, astOp::btExit ) );

					codeBlock->makeSymbolsCodeBlock ( &parentSym->file->errHandler, &sym, nullptr, nullptr, nullptr, nullptr, &localAdd, nullptr, false );
					codeBlock->makeAtomsCodeBlock ( &sym, &parentSym->file->errHandler );
					codeBlock->forceVariants ( &sym );
					symbols->makeKnown ( );

					comp.fixupGoto ( codeBlock );

					for ( uint32_t symIndex = 0; symIndex < symbols->size ( ); symIndex++ )
					{
						visibleSymbols	fgxSym;

						symbols->getFgxSymbol ( fgxSym, symIndex );
						fgxSym.index = symbols->getIndex ( fgxSym.name, true );
						fgxSym.validStart = 0;
						fgxSym.validEnd = 0;

						comp.symbols.push_back ( fgxSym );
					}

					for ( auto it = codeBlock->basicData().blocks.begin ( ); it != codeBlock->basicData().blocks.end ( ); it++ )
					{
						if ( (*it)->getOp() == astOp::symbolDef )
						{
							visibleSymbols	fgxSym;

							if ( (*it)->symbolDef()->getFgxSymbol ( fgxSym ) )
							{
								fgxSym.index = sym.getIndex ( fgxSym.name, true );
								fgxSym.validStart = 0;
								fgxSym.validEnd = 0;
								comp.symbols.push_back ( fgxSym );
							}
							(*it)->release ( );
						}
					}

					comp.compEmitNode ( 0, codeBlock, &sym, symVariantType, symVariantType, false, true );
					comp.fixup->applyFixups ( comp.codeSegment );
					delete comp.fixup;

					bufferMakeRoom ( exp->buff, sizeof ( vmCodeBlock ) );
					bufferAssume ( exp->buff, sizeof ( vmCodeBlock ) );

					cb = (vmCodeBlock *) bufferBuff ( exp->buff );
					cb->nParams = nParams;

					cb = (vmCodeBlock *) bufferBuff ( exp->buff );
					cb->csOffset = (uint32_t) bufferSize ( exp->buff );
					comp.codeSegment.serialize ( exp->buff );

					cb = (vmCodeBlock *) bufferBuff ( exp->buff );
					cb->dsOffset = (uint32_t) bufferSize ( exp->buff );
					comp.dataSegment.serialize ( exp->buff );

					cb = (vmCodeBlock *) bufferBuff ( exp->buff );
					cb->bssSize = comp.bssSize;

					cb = (vmCodeBlock *) bufferBuff ( exp->buff );
					cb->symOffset = (uint32_t) bufferSize ( exp->buff );
					cb->nSymbols = (uint32_t) comp.symbols.size ( );

					uint32_t symbolNameOffset = (uint32_t) (bufferSize ( exp->buff ) + sizeof ( fgxSymbols ) * comp.symbols.size ( ));
					for ( auto it = comp.symbols.begin ( ); it != comp.symbols.end ( ); it++ )
					{
						fgxSymbols	fgxSym = {};

						fgxSym.offsetName = symbolNameOffset;
						symbolNameOffset += (uint32_t) (it->name.size ( ) + 1);

						fgxSym.name = 0;
						fgxSym.validStart = (*it).validStart;
						fgxSym.validEnd = (*it).validEnd;
						fgxSym.index = (*it).index;
						fgxSym.noRef = (*it).noRef;
						fgxSym.isStackBased = (*it).isStackBased;

						bufferWrite ( exp->buff, &fgxSym, sizeof ( fgxSym ) );
					}
					for ( auto it = comp.symbols.begin ( ); it != comp.symbols.end ( ); it++ )
					{
						bufferWrite ( exp->buff, (*it).name.c_str ( ), (*it).name.size ( ) + 1 );
					}

					cb = (vmCodeBlock *) bufferBuff ( exp->buff );
					cb->storageSize = (uint32_t) bufferSize ( exp->buff );
					cb->isStatic = true;

					delete codeBlock;
					return node;
				} catch ( ... )
				{
					if ( cb ) free ( cb );
					throw;
				}
				break;
			}
			break;
		default:
			break;
	}
	return node;
}


void astNode::makeCodeblock ( symbolStack *sym, class errorHandler *err )
{
	astNodeWalk ( this, sym, compileCodeBlockCB, err );
}
