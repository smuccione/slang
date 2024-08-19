
#include "compilerParser/fileParser.h"

#include "compilerOptimizer.h"
#include "compilerBCGenerator.h"
#include "compilerParser/fileParser.h"
#include "compilerParser/util.h"
#include "compilerParser/funcParser.h"
#include "compilerParser/classParser.h"
#include "bcVM/Opcodes.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"


#define DEF_OP(name) {#name}, 
static struct {
	char const	*txt;
} opNames[] = {
	SLANG_OPS
};
#undef DEF_OP

stringi const &compLister::getXrefName ( uint32_t offset )
{
	return cFile->fixup->getLabel ( offset );
}

void compLister::emitOps ( symbolStack *sym )
{
	fglOp				*op;
	size_t				 loop;
	size_t				 len;
	const char			*tmpStr;
	cacheString		 	 sName;
	symbolTypeClass		 ct;
	compClass			*cc;

	if ( level < listLevel::LIGHT ) return;

	len = bufferSize ( &buff );

	while ( lastOp < codeSegment->getNumOps() )
	{
		op = &(codeSegment->getOps()[lastOp]);

		bufferPrintf( &buff, "%05i  ", (op - codeSegment->getOps()) );

		if ( op->op == fglOpcodes::prefix )
		{
			bufferWriteString ( &buff, opNames[int(op->op)].txt );
			bufferWriteString ( &buff, "  " );
			lastOp++;
			op = &(codeSegment->getOps()[lastOp]);
		}

		bufferWriteString ( &buff, opNames[int(op->op)].txt );
		bufferWriteString ( &buff, "  " );

		switch ( op->op )
		{
			case fglOpcodes::once:
			case fglOpcodes::jmp:
			case fglOpcodes::jmpc:
			case fglOpcodes::jmpcv:
			case fglOpcodes::jmpcpop:
			case fglOpcodes::jmpcpopv:
			case fglOpcodes::jmpcpop2:
			case fglOpcodes::jmpnc:
			case fglOpcodes::jmpncv:
			case fglOpcodes::jmpncpop:
			case fglOpcodes::jmpncpopv:
				buff.setColor ( consoleColor::Red );
				bufferWriteString ( &buff, " " );
				bufferWriteString ( &buff, getXrefName ( (uint32_t) ((uint8_t *) &(op->imm.index) - (uint8_t *) codeSegment->getOps ( )) ).c_str ( ) );
				buff.setColor ( );
				break;
			case fglOpcodes::storeMulti:
			case fglOpcodes::storeMultiPop:
				bufferPrintf ( &buff, " %i", op->imm.index );
				break;
			case fglOpcodes::pushNumLong:
			case fglOpcodes::addiImm:
			case fglOpcodes::subiImm:
			case fglOpcodes::diviImm:
			case fglOpcodes::muliImm:
			case fglOpcodes::modiImm:
			case fglOpcodes::powiImm:
			case fglOpcodes::maxiImm:
			case fglOpcodes::miniImm:
			case fglOpcodes::ltiImm:
			case fglOpcodes::lteqiImm:
			case fglOpcodes::gtiImm:
			case fglOpcodes::gteqiImm:
			case fglOpcodes::eqiImm:
			case fglOpcodes::neqiImm:
				bufferPrintf ( &buff, " #%i", op->imm.intValue );
				break;
			case fglOpcodes::pop:
			case fglOpcodes::popadup:
			case fglOpcodes::adds:
			case fglOpcodes::addsmulti:
				bufferPrintf( &buff, " %i", op->imm.index );
				break;

			case fglOpcodes::pushNumDouble:
			case fglOpcodes::adddImm:
			case fglOpcodes::subdImm:
			case fglOpcodes::divdImm:
			case fglOpcodes::muldImm:
			case fglOpcodes::ltdImm:
			case fglOpcodes::lteqdImm:
			case fglOpcodes::gtdImm:
			case fglOpcodes::gteqdImm:
			case fglOpcodes::eqdImm:
			case fglOpcodes::neqdImm:
				bufferPrintf ( &buff, " #%f", op->imm.doubleValue );
				break;

			case fglOpcodes::pushFixedArray:
			case fglOpcodes::pushVariableArray:
			case fglOpcodes::pushAArray:
				bufferPrintf ( &buff, " [%i]", op->imm.index );
				break;

			case fglOpcodes::pushNull:
				bufferWriteString ( &buff, " NULL" );
				break;

			case fglOpcodes::castObject:
				if ( op->imm.index )
				{
					tmpStr = bufferBuff ( &(dataSegment->buff)) + sizeof ( long ) + op->imm.index;
					buff.setColor( consoleColor::Green );
					bufferPrintf ( &buff, " %.*s", *((unsigned long *) (bufferBuff ( &(dataSegment->buff) ) + op->imm.index) ), tmpStr);
					buff.setColor();
				}
				break;

			case fglOpcodes::pushString:
				buff.setColor ( consoleColor::Green );
				bufferWrite ( &buff, " \"", 2 );
				tmpStr = bufferBuff ( &(dataSegment->buff)) + sizeof ( long ) + op->imm.index;

				for ( loop = 0; loop < *((unsigned long *)(bufferBuff (&(dataSegment->buff)) + op->imm.index) ); loop++ )
				{
					switch ( *tmpStr )
					{
						case '\n':
							bufferWrite ( &buff, "\\n", 2 );
							break;
						case '\r':
							bufferWrite ( &buff, "\\r", 2 );
							break;
						case '\t':
							bufferWrite ( &buff, "\\t", 2 );
							break;
						case 0:
							bufferWrite ( &buff, "\0", 2 );
							break;
						default:
							bufferPut8 ( &buff, *tmpStr );
							break;
					}
					tmpStr++;
				}
				bufferWrite ( &buff, "\"", 1 );
				buff.setColor ( );
				break;

			case fglOpcodes::objAccess:
			case fglOpcodes::objAccessRef:
			case fglOpcodes::objStore:
			case fglOpcodes::objStorePop:	
				bufferWriteString ( &buff, " ." );
				buff.setColor( consoleColor::Green );
				bufferWrite ( &buff, bufferBuff ( &(dataSegment->buff)) + sizeof ( long ) + op->imm.index, *((unsigned long *) (bufferBuff ( &(dataSegment->buff) ) + op->imm.index)) );
				buff.setColor();
				break;

			case fglOpcodes::pushAtom:
				buff.setColor ( consoleColor::Cyan );
				bufferWriteString ( &buff, " " );
				bufferWriteString ( &buff, atoms->getAtom ( lastOp * sizeof ( fglOp ) + offsetof ( fglOp, imm.index ) ) );
				buff.setColor ( );
				break;
			case fglOpcodes::storeClassGlobal:
			case fglOpcodes::storeClassGlobalPop:
				sName = sym->getSymbolName ( symbolSpaceScope::symLocal, op->imm.objOp.stackIndex );

				ct = sym->getType ( sName, true );
				cc = sym->findClass ( ct.getClass ( ) );
				for ( auto &it : cc->elements )
				{
					if ( !it.isVirtual )
					{
						if ( sym->file->symbols.find ( it.symbolName )->second.globalIndex == op->imm.objOp.objectIndex )
						{
							buff.setColor( consoleColor::Yellow );
							bufferWriteString ( &buff, sName.c_str() );
							buff.setColor();
							bufferWriteString ( &buff, "." );
							buff.setColor( consoleColor::Green );
							bufferWriteString ( &buff, it.name.c_str ( ) );
							buff.setColor();
							break;
						}
					}
				}
				break;
			case fglOpcodes::move:
				bufferPrintf ( &buff, "{%d}", op->imm.index );
				break;

			case fglOpcodes::incLocali:
			case fglOpcodes::decLocali:
			case fglOpcodes::incLocalv:
			case fglOpcodes::decLocalv:
			case fglOpcodes::promote:
			case fglOpcodes::pushSymLocal:
			case fglOpcodes::pushSymLocalRef:
			case fglOpcodes::storeLocalNull:
			case fglOpcodes::storeLocal:
			case fglOpcodes::storeLocalv:
			case fglOpcodes::storeLocalPop:
			case fglOpcodes::storeLocalPopv:
			case fglOpcodes::storeLocalAdd:
			case fglOpcodes::storeLocalSub:
			case fglOpcodes::storeLocalMul:
			case fglOpcodes::storeLocalDiv:
			case fglOpcodes::storeLocalMod:
			case fglOpcodes::storeLocalPow:
			case fglOpcodes::storeLocalBwAnd:
			case fglOpcodes::storeLocalBwOr:
			case fglOpcodes::storeLocalBwXor:
			case fglOpcodes::storeLocalShLeft:
			case fglOpcodes::storeLocalShRight:  
			case fglOpcodes::makeParamPack:
				buff.setColor( consoleColor::Cyan );
				bufferPrintf ( &buff, " %s ", sym->getSymbolName ( symbolSpaceScope::symLocal, op->imm.index ).c_str() );
				buff.setColor();
				bufferPrintf( &buff, "{%d}", op->imm.index );
				break;
			case fglOpcodes::pushSymGlobal:
			case fglOpcodes::pushSymGlobalRef:
			case fglOpcodes::incGlobalv:
			case fglOpcodes::decGlobalv:
			case fglOpcodes::storeGlobalv:
			case fglOpcodes::storeGlobalPop:
			case fglOpcodes::storeGlobalPopv:
			case fglOpcodes::storeGlobalAdd:
			case fglOpcodes::storeGlobalSub:
			case fglOpcodes::storeGlobalMul:
			case fglOpcodes::storeGlobalDiv:
			case fglOpcodes::storeGlobalMod:
			case fglOpcodes::storeGlobalPow:
			case fglOpcodes::storeGlobalBwAnd:
			case fglOpcodes::storeGlobalBwOr:
			case fglOpcodes::storeGlobalBwXor:
			case fglOpcodes::storeGlobalShLeft:
			case fglOpcodes::storeGlobalShRight:
				bufferWriteString ( &buff, " " );
				bufferWriteString ( &buff, sym->getSymbolName ( symbolSpaceScope::symGlobal, op->imm.index ).c_str() );
				break;

			case fglOpcodes::waStoreField:
			case fglOpcodes::waStoreFieldPop:
				bufferWriteString ( &buff, " " );
				bufferWrite ( &buff, bufferBuff ( &(dataSegment->buff) ) + sizeof ( long ) + op->imm.index, *((unsigned long *) (bufferBuff ( &(dataSegment->buff) ) + op->imm.index)) );
				break;

			case fglOpcodes::pushContext:
				{
					bool contextFound = false;
					sName = sym->getSymbolName ( symbolSpaceScope::symLocal, op->imm.objOp.stackIndex );

					ct = sym->getType ( sName, true );
					cc = sym->findClass ( ct.getClass () );
					for ( auto it = cc->elements.crbegin (); it != cc->elements.crend (); it++ )
					{
						if ( it->type == fgxClassElementType::fgxClassType_inherit && it->methodAccess.objectStart == op->imm.objOp.objectIndex )
						{
							buff.setColor( consoleColor::Cyan );
							bufferPrintf( &buff, " %s%s ", it->ns.c_str(), it->name.c_str() );
							buff.setColor();
							bufferPrintf( &buff, "{ %d, %d }", op->imm.objOp.stackIndex, op->imm.objOp.objectIndex );
							contextFound = true; // NOLINT
							break;
						}
					}
					assert ( contextFound );
				}
				break;

			case fglOpcodes::modifyContext:
				bufferPrintf ( &buff, " %d{%d}", op->imm.objOp.stackIndex, op->imm.objOp.objectIndex );
				break;

			case fglOpcodes::callCB:
				bufferPrintf ( &buff, " {%d}", op->imm.index );
				break;

			case fglOpcodes::callV:
			case fglOpcodes::callVPop:
			case fglOpcodes::objCall:
			case fglOpcodes::objCallPop:
			case fglOpcodes::objConstructV:
				buff.setColor ( consoleColor::Cyan );
				bufferPrintf ( &buff, " ->%s (%i)", getXrefName ( (uint32_t) ((uint8_t *)&(op->imm.dual.val1) - (uint8_t *) codeSegment->getOps ( )) ).c_str(), op->imm.dual.val2 );
				buff.setColor ();
				break;

			case fglOpcodes::objConstruct:
				buff.setColor ( consoleColor::Cyan );
				bufferPrintf ( &buff, " %s (%i)", atoms->getAtom ( lastOp * sizeof ( fglOp ) + offsetof ( fglOp, imm.dual.val1 ) ), op->imm.dual.val2 );
				buff.setColor ( );
				break;

			case fglOpcodes::callC:
			case fglOpcodes::callCPop:
			case fglOpcodes::callBC:
			case fglOpcodes::callBCPop:
			case fglOpcodes::objCallFuncOv:
			case fglOpcodes::objCallFuncOvPop:
			case fglOpcodes::callBCTail:
				//lastOp * sizeof ( fglOp ) + offsetof ( fglOp, imm.dual.val1 ) )
				// the above computes the FIXUP location needed.  this is then searched and the actual name of the atom being fixedup is printed
				buff.setColor( consoleColor::Green );
				bufferPrintf ( &buff, " %s ", atoms->getAtom ( lastOp * sizeof ( fglOp ) + offsetof ( fglOp, imm.dual.val1 ) ));
				buff.setColor();
				bufferPrintf( &buff, " (%i)", op->imm.dual.val2 );
				break;

			case fglOpcodes::pushObjIVarRef:
			case fglOpcodes::pushObjVirtIVarRef:
			case fglOpcodes::pushObjVirtIVar:
			case fglOpcodes::pushObjIVar:
				sName = sym->getSymbolName ( symbolSpaceScope::symLocal, op->imm.objOp.stackIndex );

				ct = sym->getType ( sName, true );
				cc = sym->findClass ( ct.getClass ( ) );
				for ( auto &it : cc->elements )
				{
					if ( !it.isVirtual )
					{
						if ( it.methodAccess.data.varIndex == op->imm.objOp.objectIndex )
						{
							buff.setColor( consoleColor::Yellow );
							bufferWriteString ( &buff, sName.c_str() );
							buff.setColor();
							bufferWriteString ( &buff, "." );
							buff.setColor( consoleColor::Green );
							bufferWriteString ( &buff, it.name.c_str ( ) );
							buff.setColor();
							break;
						}
					}
				}
				bufferPrintf ( &buff, " {%i %i}", op->imm.objOp.stackIndex, op->imm.objOp.objectIndex );
				break;
			case fglOpcodes::storeClassIVar:
			case fglOpcodes::storeClassIVarPop:
			case fglOpcodes::incClassIVar:
			case fglOpcodes::decClassIVar:
				sName = sym->getSymbolName ( symbolSpaceScope::symLocal, op->imm.objOp.stackIndex );
			
				ct = sym->getType ( sName, true );
				cc = sym->findClass ( ct.getClass() );
				for ( auto &it : cc->elements )
				{
					if ( !it.isVirtual )
					{
						if ( it.methodAccess.data.varIndex == op->imm.objOp.objectIndex )
						{
							buff.setColor( consoleColor::Yellow );
							bufferWriteString ( &buff, sName.c_str() );
							buff.setColor();
							bufferWriteString ( &buff, "." );
							buff.setColor( consoleColor::Green );
							bufferWriteString ( &buff, it.name.c_str ( ) );
							buff.setColor();
							break;
						}
					}
				}
				bufferPrintf ( &buff, " {%i %i}", op->imm.objOp.stackIndex, op->imm.objOp.objectIndex );
				break;
			default:
				break;
		}

		len = bufferSize ( &buff ) - len;
		if ( annotation.size() )
		{
			buff.setColor ( consoleColor::Blue );
			if ( len < 60 )
			{
				while ( len < 60 )
				{
					bufferPut8 ( &buff, ' ' );
					len++;
				}
			} else
			{
				bufferWrite ( &buff, "   ", 3 );
			}

			bufferWriteString ( &buff, "; " );
			bufferWrite ( &buff, annotation.data<char const *> (), annotation.size () );
			annotation.reset ();
			buff.setColor ( );
		}
		bufferWriteString ( &buff, "\r\n" );

		lastOp++;
	}
}

char const *compLister::getSymbolScopeName ( symbolSpaceScope scope )
{
	switch ( scope )
	{
		case symbolSpaceScope::symGlobal:
			return ( "global" );
		case symbolSpaceScope::symLocal:
			return ( "local" );
		case symbolSpaceScope::symLocalStatic:
			return ( "localStatic" );
		case symbolSpaceScope::symLocalField:
			return ( "localField" );
		case symbolSpaceScope::symLocalParam:
			return ( "localParam" );
		case symbolSpaceScope::symLocalConst:
			return ( "localConst" );
		case symbolSpaceScope::symClassConst:
			return ( "classConst" );
		case symbolSpaceScope::symClassAssign:
			return ( "classAssign" );
		case symbolSpaceScope::symClassAccess:
			return ( "classAccess" );
		case symbolSpaceScope::symClassMethod:
			return ( "classMethod" );
		case symbolSpaceScope::symClassStatic:
			return ( "classStatic" );
		case symbolSpaceScope::symClassIVar:
			return ( "classIvar" );
		case symbolSpaceScope::symClassInherit:
			return ( "classInherit" );
		case symbolSpaceScope::symScopeUnknown:
		default:
			return ( "unknown" );
	}
	throw errorNum::scINTERNAL;
}

void compLister::setLastLine ( srcLocation const &location )
{
	auto &fName = cFile->file->srcFiles.getName ( location.sourceIndex );
	auto &src = cFile->file->getSourceListing ( fName );
	src.setLastLine ( location.lineNumberStart - 1 );
}

void compLister::pushLine ( srcLocation const &location )
{
	auto &fName = cFile->file->srcFiles.getName ( location.sourceIndex );
	auto &src = cFile->file->getSourceListing ( fName );
	src.pushLine ( location.lineNumberStart - 1 );
}
void compLister::popLine ( srcLocation const &location  )
{
	auto &fName = cFile->file->srcFiles.getName ( location.sourceIndex );
	auto &src = cFile->file->getSourceListing ( fName );
	src.popLine ( );
}
void compLister::emitSource ( stringi const &fName, uint32_t lineNum )
{
	if ( level < listLevel::LIGHT ) return;

	char const *line;

	auto &src = cFile->file->getSourceListing ( fName );
	if ( (line = src.getLine ( lineNum )) )
	{
		buff.setColor ( consoleColor::DarkGray );
		bufferWrite ( &buff, ";  ", 3 );
		bufferWrite ( &buff, line, src.getLineLen ( lineNum ) );
		bufferWrite ( &buff, "\r\n", 2 );
		buff.setColor ( );
	}
	src.lastLine = lineNum;
	return;
}

void compLister::emitContSource ( srcLocation const &location )
{
	auto &fName = cFile->file->srcFiles.getName ( location.sourceIndex );
	if ( level < listLevel::LIGHT ) return;

	if ( !fName.size() ) return;

	if ( fName == "(INTERNAL)" )
	{
		return;
	}

	auto &src = cFile->file->getSourceListing ( fName );
	while ( src.lastLine <= location.lineNumberStart )
	{
		emitSource ( fName, src.lastLine + 1 );
	}
}

void compLister::emitContSourceEnd ( srcLocation const &location )
{
	auto &fName = cFile->file->srcFiles.getName ( location.sourceIndex );
	if ( level < listLevel::LIGHT ) return;

	if ( !fName.size() ) return;

	if ( fName == "(INTERNAL)" )
	{
		return;
	}

	auto &src = cFile->file->getSourceListing ( fName );
	while ( src.lastLine <= location.lineNumberEnd )
	{
		emitSource ( fName, src.lastLine + 1 );
	}
}

char const *compLister::getsymbolTypeClassName ( symbolTypeClass const &type )
{
	return type;
}

void compLister::emitFuncInfo ( opFunction *func )
{
	if ( level < listLevel::LIGHT ) return;
	buff.setColor ( consoleColor::Cyan );
	buff.write ( "\r\n\r\n" );
	if ( func->isStatic ) buff.write ( "[[static]] " );
	if ( func->isExtension ) buff.write ( "[[extension]] " );
	if ( func->classDef && !func->isStatic && !func->isExtension ) buff.write ( "[[method]] " );
	if ( func->isFGL ) buff.write ( "[[fgl]] " );
	if ( func->isLambda) buff.write ( "[[lambda]] " );
	if ( func->isProcedure) buff.write ( "[[procedure]] " );
	if ( func->isVariantParam) buff.write ( "[[variantParam]] " );
	if ( func->isPure ) buff.write ( "[[pure]] " );
	if ( func->isConst ) buff.write ( "[[const]] " );
	buff.setColor ( consoleColor::Magenta );
	if ( func->isProcedure )
	{
		buff.write ( "<none>" );
	} else
	{
		buff.write ( getsymbolTypeClassName ( func->getReturnType() ) );
	}
	buff.setColor ( consoleColor::Cyan );
	buff.write ( " function " );
	buff.write ( func->parentClosure.size() ? " closure " : " " );
	buff.write ( func->name.c_str ( ) );
	buff.write ( " ( " );

	for ( uint32_t loop = 0; loop < func->params.size(); loop++ )
	{
		buff.setColor ( consoleColor::Magenta );
		buff.write ( getsymbolTypeClassName ( func->params[loop]->getType() ) );
		buff.write ( " " );
		buff.setColor ( consoleColor::Green );
		buff.write ( func->params[loop]->name.c_str ( ) );
		if ( loop != func->params.size() - 1 ) bufferPrintf ( &buff, ", " );
	}
	buff.setColor ( consoleColor::Cyan );
	buff.write ( " )\r\n" );
	buff.setColor ( );
}

void compLister::emitClassInfo (opClass *cl )
{
	if ( cl->isInterface && level < listLevel::VERBOSE ) return;
	if ( level < listLevel::LIGHT ) return;
	buff.setColor ( consoleColor::DarkYellow );
	buff.write ( "\r\nclass " );
	buff.write ( cl->name.c_str ( ) );
	buff.write ( "\r\n" );
	for (auto &it : cl->cClass->elements)
	{
		buff.setColor ( consoleColor::Green );
		buff.write ( "   " );
		buff.write ( it.name.c_str ( ) );
		buff.write ( " " );
		buff.setColor ( consoleColor::Yellow );
		if ( it.isStatic ) bufferWriteString ( &buff, "STATIC " );
		if ( it.isVirtual ) bufferWriteString ( &buff, "VIRTUAL " );
		switch (it.type)
		{
			case fgxClassElementType::fgxClassType_method:
				if ( it.methodAccess.func->isExtension ) bufferWriteString ( &buff, "EXTENSION " );
				buff.write ( "METHOD ( ");
				for ( size_t index = 0; index < it.methodAccess.func->params.size (); index++ )
				{
					if ( index ) buff.write ( ", " );
					symbolTypeClass p = it.methodAccess.func->params[(uint32_t)index]->getType ();
					buff.setColor ( consoleColor::Magenta );
					buff.write ( getsymbolTypeClassName ( p ) );
					buff.write ( " " );
					buff.setColor ( consoleColor::Green );
					buff.write ( it.methodAccess.func->params[(uint32_t)index]->getName () );
					buff.setColor ( consoleColor::Yellow );
				}
				buff.write ( " ) : " );
				buff.setColor ( consoleColor::Magenta );
				buff.write ( getsymbolTypeClassName ( it.methodAccess.func->getReturnType() ) );
				buff.write ( "   " );
				buff.setColor ( consoleColor::Yellow );
				if ( it.isVirtual )
				{
					buff.write ( "VTABLE ENTRY: " );
					buff.write ( it.methodAccess.data.vTabEntry );
				}
				break;
			case fgxClassElementType::fgxClassType_prop:
				if ( it.methodAccess.func )
				{
					bufferWriteString ( &buff, "ACCESS ( " );
					for ( size_t index = 0; index < it.methodAccess.func->params.size (); index++ )
					{
						if ( index ) buff.write ( ", " );
						symbolTypeClass p = it.methodAccess.func->params[(uint32_t)index]->getType ();
						buff.setColor ( consoleColor::Magenta );
						buff.write ( getsymbolTypeClassName ( p ) );
						buff.write ( " " );
						buff.setColor ( consoleColor::Green );
						buff.write ( it.methodAccess.func->params[(uint32_t)index]->getName () );
						buff.setColor ( consoleColor::Yellow );
					}
					buff.write ( " ) : " );
					buff.setColor ( consoleColor::Magenta );
					buff.write ( getsymbolTypeClassName ( it.methodAccess.func->getReturnType() ) );
					buff.write ( "   " );
					buff.setColor ( consoleColor::Yellow );
					if ( it.isVirtual )
					{
						buff.write ( "VTABLE ENTRY: " );
						buff.write ( it.methodAccess.data.vTabEntry );
					}
				}
				if ( it.assign.func )
				{
					if ( it.methodAccess.func )
					{
						buff.write ( " )\r\n" );
						buff.setColor ( consoleColor::Green );
						buff.write ( "   " );
						buff.write ( it.name.c_str ( ) );
						buff.write ( " " );
						buff.setColor ( consoleColor::Yellow );
						if ( it.isStatic ) bufferWriteString ( &buff, "STATIC " );
						if ( it.isVirtual ) bufferWriteString ( &buff, "VIRTUAL " );
					}
					bufferWriteString ( &buff, "ASSIGN ( " );
					for ( size_t index = 0; index < it.assign.func->params.size (); index++ )
					{
						if ( index ) buff.write ( ", " );
						symbolTypeClass p = it.assign.func->params[(uint32_t)index]->getType ();
						buff.setColor ( consoleColor::Magenta );
						buff.write ( getsymbolTypeClassName ( p ) );
						buff.write ( " " );
						buff.setColor ( consoleColor::Green );
						buff.write ( it.assign.func->params[(uint32_t)index]->getName () );
						buff.setColor ( consoleColor::Yellow );
					}
					buff.write ( " ) : " );
					buff.setColor ( consoleColor::Magenta );
					buff.write ( getsymbolTypeClassName ( it.assign.func->getReturnType() ) );
					buff.write ( "   " );
					buff.setColor ( consoleColor::Yellow );
					if ( it.isVirtual )
					{
						buff.write ( "VTABLE ENTRY: " );
						buff.write ( it.assign.data.vTabEntry );
					}
				}
				break;
			case fgxClassElementType::fgxClassType_iVar:
				buff.write ( "IVAR ");
				buff.setColor ( consoleColor::Magenta );
				buff.write ( getsymbolTypeClassName ( it.elem->symType ) );
				buff.write ( " ");
				buff.setColor ( consoleColor::Yellow );
				if (it.isVirtual)
				{
					buff.write ( "VTABLE ENTRY: " );
					buff.write ( it.methodAccess.data.vTabEntry );
				}
				else
				{
					buff.write ( "objOffset: " );
					buff.write ( it.methodAccess.objectStart );
					buff.write ( "   varOffset : " );
					buff.write ( it.methodAccess.data.varIndex );
				}
				buff.write ( "  " );
				break;
			case fgxClassElementType::fgxClassType_static:
				buff.write ( "IVAR  ");
				buff.setColor ( consoleColor::Magenta );
				buff.write ( getsymbolTypeClassName ( it.elem->symType ) );
				buff.write ( " ");
				buff.setColor ( consoleColor::Yellow );
				break;
			case fgxClassElementType::fgxClassType_const:
				buff.write ( "CONST   ");
				buff.setColor ( consoleColor::Magenta );
				buff.write ( getsymbolTypeClassName ( it.elem->symType ) );
				buff.write ( " ");
				buff.setColor ( consoleColor::Yellow );
				break;
			case fgxClassElementType::fgxClassType_inherit:
				buff.write ( "INHERIT ");
				if ( it.isVirtual )
				{
					buff.write ( "VTABLE ENTRY: " );
					buff.write ( it.methodAccess.data.vTabEntry );
				} else
				{
					buff.write ( "objOffset: " );
					buff.write ( it.methodAccess.objectStart );
					buff.write ( "   varOffset : " );
					buff.write ( it.methodAccess.data.varIndex );
				}
				buff.write ( "  " );
				break;
			case fgxClassElementType::fgxClassType_message:
				buff.write ( "MESSAGE ");
				break;
		}
		buff.write ( "\r\n" );
	}
	if ( cl->cClass->vTable.size () )
	{
		buff.setColor ( consoleColor::Green );
		buff.write ( "   VTABLE:" );
		buff.setColor ( consoleColor::Yellow );
		for ( auto &it : cl->cClass->vTable )
		{
			buff.write ( "       " );
			buff.printf ( "atom: %u   delta: %hi\r\n", it.atom, it.delta );
		}
	}
	buff.setColor ( );
}
void compLister::emitSymbolInfo ( symbolStack *symStack, symbol *sym )
{
	if ( level < listLevel::LIGHT ) return;

	buff.setColor ( consoleColor::Yellow );

	auto size = symStack->size ();
	symStack->push ( sym );

	switch ( sym->symType )
	{
		case symbolSpaceType::symTypeLocal:
		case symbolSpaceType::symTypeField:
		case symbolSpaceType::symTypeStatic:
			bufferPrintf ( &buff, "%-12s ", getSymbolScopeName ( sym->getScope ( sym->getSymbolName (), true ) ) );
			buff.setColor ( consoleColor::Magenta );
			bufferPrintf ( &buff, "%-13s ", (char const *) sym->getType ( sym->getSymbolName (), true ) );
			buff.setColor ( consoleColor::Yellow );
			bufferPrintf ( &buff, "%s {%i}\r\n",sym->getSymbolName ( ).c_str(), symStack->getIndex( sym->getSymbolName ( ), true ) );
			break;
		case symbolSpaceType::symTypeParam:
			{
				auto symbol = dynamic_cast<symbolSpaceParams *>(sym);
				for ( uint32_t it = (uint32_t)symbol->size ( ); it; it-- )
				{
					bufferPrintf ( &buff, "%-12s ", getSymbolScopeName ( symbolSpaceScope::symLocalParam ) );
					buff.setColor ( consoleColor::Magenta );
					bufferPrintf ( &buff, "%-13s ", (char const *) (*symbol)[it - 1]->getType ( ) );
					buff.setColor ( consoleColor::Yellow );
					bufferPrintf ( &buff, "%s {%i}\r\n", (*symbol)[it - 1]->name.c_str ( ), symStack->getIndex( (*symbol)[it - 1]->name, true )  );
				}
			}
			break;
		case symbolSpaceType::symTypeClass:
			bufferPrintf ( &buff, "class " );
			buff.setColor ( consoleColor::Green );
			bufferPrintf ( &buff, "%s\r\n", dynamic_cast<symbolClass *>(sym)->className.c_str() );
			break;
		default:
			break;
	}
	symStack->resize ( size );
	buff.setColor ();
}

void compLister::printOutput ( )
{
	::printf ( "%.*s", (int) buff.size ( ), buff.data<char *> ( ) );
	return;

	auto s = buff.data<char *> ( );
	size_t offset = 0;


	auto lastPrintOffset = offset;
	while ( offset < buff.size ( ) )
	{
		auto it = escapeToWindowsColorCode.find ( s + offset );
		if ( it != escapeToWindowsColorCode.end ( ) )
		{
			::printf ( "%.*s", (int)(offset - lastPrintOffset), s + lastPrintOffset );
			setConsoleColor ( it->second );
			offset += it->first.length ( ) - 1;
			lastPrintOffset = offset + 1;
		}
		offset++;
	}
	::printf ( "%.*s",  (int)(offset - lastPrintOffset), s + lastPrintOffset );
}

void compLister::printOutput ( FILE * file )
{
	auto s = buff.data<char *> ( );
	size_t offset = 0;

	auto lastPrintOffset = offset;
	while ( offset < buff.size ( ) )
	{
		auto it = escapeToWindowsColorCode.find ( s + offset );
		if ( it != escapeToWindowsColorCode.end ( ) )
		{
			fprintf ( file, "%.*s", (int) (offset - lastPrintOffset), s + lastPrintOffset );
			offset += it->first.length ( ) - 1;
			lastPrintOffset = offset + 1;
		}
		offset++;
	}
	fprintf ( file, "%.*s", (int) (offset - lastPrintOffset), s + lastPrintOffset );
}
