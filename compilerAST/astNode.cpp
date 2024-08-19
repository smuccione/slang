/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

astArray::astArray ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	uint32_t size = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );

	while ( size-- )
	{
		nodes.push_back ( OP_NODE_DESERIALIZE ( file, srcFile, expr ) );
	}
}

astArray::~astArray ( ) noexcept
{
	for ( auto it = nodes.begin ( ); it != nodes.end ( ); it++ )
	{
		if ( (*it) ) delete (*it);
	}
}

void astArray::serialize ( BUFFER *buff )
{
	buff->put ( ( uint32_t) nodes.size ( ) );
	for ( auto &it : nodes )
	{
		it->serialize ( buff );
	}
}

astArray::astArray ( const astArray &right )
{
	assert ( !nodes.size ( ) );
	for ( auto &it : right.nodes )
	{
		if ( it )
		{
			nodes.push_back ( new (std::nothrow) astNode ( *it ) );
		} else
		{
			nodes.push_back ( 0 );
		}
	}
}

astPList::~astPList ( ) noexcept
{
	for ( auto it = param.begin ( ); it != param.end ( ); it++ )
	{
		if ( (*it) ) delete (*it);
	}
}

astPList::astPList ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	uint32_t size = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );

	while ( size-- )
	{
		param.push_back ( OP_NODE_DESERIALIZE ( file, srcFile, expr ) );
	}

	uint32_t regionSize = *(uint32_t *)*expr;
	*expr += sizeof ( uint32_t );

	while ( regionSize-- )
	{
		paramRegion.push_back ( srcLocation ( srcFile, expr ) );
	}
}

void astPList::serialize ( BUFFER *buff )
{
	buff->put ( ( uint32_t) param.size ( ) );
	for ( auto &it : param )
	{
		it->serialize ( buff );
	}
	buff->put ( (uint32_t)paramRegion.size () );
	for ( auto &it : paramRegion )
	{
		it.serialize ( buff );
	}
}

astPList::astPList ( const astPList &old )
{
	for ( auto &it : old.param )
	{
		if ( it )
		{
			param.push_back ( new (std::nothrow) astNode ( *it ) );
		} else
		{
			param.push_back ( 0 );
		}
	}
	paramRegion = old.paramRegion;
}

astCond::~astCond ( ) noexcept
{
	if ( trueCond ) delete trueCond;
	if ( falseCond ) delete falseCond;
	trueCond = nullptr;
	falseCond = nullptr;
}

astCond::astCond ( const astCond &old )
{
	if ( trueCond ) delete trueCond;
	if ( falseCond ) delete falseCond;
	if ( old.trueCond ) trueCond = new (std::nothrow) astNode ( *old.trueCond );
	if ( old.falseCond ) falseCond = new (std::nothrow) astNode ( *old.falseCond );
}

astCond::astCond ( class opFile *file, sourceFile *srcFile, char const **expr )
{
	trueCond = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	falseCond = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astCond::serialize ( BUFFER *buff )
{
	trueCond->serialize ( buff );
	falseCond->serialize ( buff );
}

astCondBlock::~astCondBlock ( ) noexcept
{
	if ( condition ) delete condition;
	if ( block ) delete block;
	condition = nullptr;
	block = nullptr;
}

astCondBlock::astCondBlock ( const astCondBlock &old )
{
	if ( condition ) delete condition;
	if ( block ) delete block;
	if ( old.condition ) condition = new (std::nothrow) astNode ( *old.condition );
	if ( old.block ) block = new (std::nothrow) astNode ( *old.block );
}

astCondBlock::astCondBlock ( class opFile *file, sourceFile *srcFile, char const **expr )
{
	condition = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	block = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astCondBlock::serialize ( BUFFER *buff )
{
	condition->serialize ( buff );
	block->serialize ( buff );
}

astBasic::~astBasic ( ) noexcept
{
	for ( auto &it : blocks )
	{
		delete it;
	}
}

astBasic::astBasic ( const astBasic &old )
{
	for ( auto &it : old.blocks )
	{
		if ( it )
		{
			blocks.push_back ( new (std::nothrow) astNode ( *it ) );
		} else
		{
			blocks.push_back ( 0 );
		}
	}
	emitPops = old.emitPops;
}

astBasic::astBasic ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	emitPops = *( bool *) * expr;
	*expr += sizeof ( emitPops );

	uint32_t size = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );

	while ( size-- )
	{
		blocks.push_back ( OP_NODE_DESERIALIZE ( file, srcFile, expr ) );
	}
}

void astBasic::serialize ( BUFFER *buff )
{
	buff->put ( emitPops );
	buff->put ( ( uint32_t) blocks.size ( ) );
	for ( auto &it : blocks )
	{
		it->serialize ( buff );
	}
}

bool astBasic::isCombinable ( )
{
	for ( auto &it : blocks )
	{
		switch ( it->getOp ( ) )
		{
			case astOp::symbolDef:
				return false;
			default:
				break;
		}
	}
	return true;
}

astSwitch::~astSwitch ( ) noexcept
{
	delete condition;
	for ( auto &it : caseList )
	{
		delete it;
	}
}

astSwitch::astSwitch ( const astSwitch &old ) : breakLabel ( old.breakLabel )
{
	if ( old.condition ) condition = new (std::nothrow) astNode ( *old.condition );
	for ( auto &it : old.caseList )
	{
		caseList.push_back ( new (std::nothrow) astCondBlock ( *it ) );
	}
}

astSwitch::astSwitch ( class opFile *file, class sourceFile *srcFile, char const **expr )  : breakLabel ( file->sCache.get ( "$switch_break" ) ) 	
{
	condition = OP_NODE_DESERIALIZE ( file, srcFile, expr );

	uint32_t size = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );
	while ( size-- )
	{
		caseList.push_back ( new (std::nothrow) astCondBlock ( file, srcFile, expr ) );
	}
}

void astSwitch::serialize ( BUFFER *buff )
{
	condition->serialize ( buff );

	bufferPut32 ( buff, ( uint32_t) caseList.size ( ) );
	for ( auto &it : caseList )
	{
		it->serialize ( buff );
	}
}

astIf::~astIf ( ) noexcept
{
	if ( elseBlock ) delete elseBlock;
	for ( auto &it : ifList )
	{
		delete it;
	}
}

astIf::astIf ( const astIf &old )
{
	if ( old.elseBlock ) elseBlock = new (std::nothrow) astNode ( *old.elseBlock );
	for ( auto &it : old.ifList )
	{
		ifList.push_back ( new (std::nothrow) astCondBlock ( *it ) );
	}
}

astIf::astIf ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	elseBlock = OP_NODE_DESERIALIZE ( file, srcFile, expr );

	uint32_t size = *( uint32_t *) * expr;
	*expr += sizeof ( uint32_t );
	while ( size-- )
	{
		ifList.push_back ( new (std::nothrow) astCondBlock ( file, srcFile, expr ) );
	}
}

void astIf::serialize ( BUFFER *buff )
{
	elseBlock->serialize ( buff );

	bufferPut32 ( buff, ( uint32_t) ifList.size ( ) );
	for ( auto &it : ifList )
	{
		it->serialize ( buff );
	}
}

astInlineFunc::~astInlineFunc ( ) noexcept
{
	if ( node ) delete node;
}

astInlineFunc::astInlineFunc ( const astInlineFunc &old )
{
	funcName = old.funcName;
	retType = old.retType;
	if ( old.node ) node = new (std::nothrow) astNode ( *old.node );
}

astInlineFunc::astInlineFunc ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	funcName = file->sCache.get ( expr );
	node = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astInlineFunc::serialize ( BUFFER *buff )
{
	funcName.serialize ( buff );
	node->serialize ( buff );
}

astGoto::~astGoto ( ) noexcept
{
	if ( finallyBlock ) delete finallyBlock;
}

astGoto::astGoto ( const astGoto &old ) : label ( old.label ), nPop ( old.nPop )
{
	if ( old.finallyBlock ) finallyBlock = new (std::nothrow) astNode ( *old.finallyBlock );
}

astGoto::astGoto ( class opFile *file, class sourceFile *srcFile, char const **expr ) : label ( file->sCache.get ( "$goto" ) ) 	
{
	label = file->sCache.get ( expr );

	finallyBlock = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astGoto::serialize ( BUFFER *buff )
{
	label.serialize ( buff );
	finallyBlock->serialize ( buff );
}

astLoop::~astLoop ( ) noexcept
{
	if ( initializers ) delete initializers;
	if ( condition ) delete condition;
	if ( increase ) delete increase;
	if ( block ) delete block;
}

astLoop::astLoop ( const astLoop &old ) : continueLabel ( old.continueLabel ), breakLabel ( old.breakLabel )
{
	if ( old.initializers ) initializers = new (std::nothrow) astNode ( *old.initializers );
	if ( old.condition ) condition = new (std::nothrow) astNode ( *old.condition );
	if ( old.increase ) increase = new (std::nothrow) astNode ( *old.increase );
	if ( old.block ) block = new (std::nothrow) astNode ( *old.block );
}

astLoop::astLoop ( class opFile *file, class sourceFile *srcFile, char const **expr ) : continueLabel ( file->sCache.get ( "$loop_continue" ) ), breakLabel ( file->sCache.get ( "$loop_break" ) ) 		
{
	initializers = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	condition = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	increase = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	block = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astLoop::serialize ( BUFFER *buff )
{
	initializers->serialize ( buff );
	condition->serialize ( buff );
	increase->serialize ( buff );
	block->serialize ( buff );
}

astForEach::~astForEach ( ) noexcept
{
	if ( var ) delete var;
	if ( in ) delete in;
	if ( statement ) delete statement;
}

astForEach::astForEach ( const astForEach &old )
{
	if ( old.var ) var = new (std::nothrow) astNode ( *old.var );
	if ( old.in ) in = new (std::nothrow) astNode ( *old.in );
	if ( old.statement ) statement = new (std::nothrow) astNode ( *old.statement );
}

astForEach::astForEach ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	var = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	in = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	statement = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astForEach::serialize ( BUFFER *buff )
{
	var->serialize ( buff );
	in->serialize ( buff );
	statement->serialize ( buff );
}

astReturn::~astReturn ( ) noexcept
{
	if ( finallyBlock ) delete finallyBlock;
	if ( node ) delete node;
}

astReturn::astReturn ( const astReturn &old ) : isYield ( old.isYield ), label ( old.label ), nPop ( old.nPop )
{
	if ( node ) delete node;
	if ( finallyBlock ) delete finallyBlock;

	if ( old.finallyBlock ) finallyBlock = new (std::nothrow) astNode ( *old.finallyBlock );
	if ( old.node ) node = new (std::nothrow) astNode ( *old.node );
}

astReturn::astReturn ( class opFile *file, class sourceFile *srcFile, char const **expr ) : label ( file->sCache.get ( "$ret" ) ) 
{
	isYield = *( bool *) * expr;
	*expr += sizeof ( isYield );
	finallyBlock = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	node = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astReturn::serialize ( BUFFER *buff )
{
	buff->put ( isYield );
	finallyBlock->serialize ( buff );
	node->serialize ( buff );
}

astTryCatch::~astTryCatch ( ) noexcept
{
	if ( tryBlock ) delete tryBlock;
	if ( errSymbol ) delete errSymbol;
	if ( catchBlock ) delete catchBlock;
	if ( finallyBlock ) delete finallyBlock;
}

astTryCatch::astTryCatch ( const astTryCatch &old )
{
	if ( old.tryBlock ) tryBlock = new (std::nothrow) astNode ( *old.tryBlock );
	if ( old.errSymbol ) errSymbol = new (std::nothrow) astNode ( *old.errSymbol );
	if ( old.catchBlock ) catchBlock = new (std::nothrow) astNode ( *old.catchBlock );
	if ( old.finallyBlock ) finallyBlock = new (std::nothrow) astNode ( *old.finallyBlock );
}

astTryCatch::astTryCatch ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	tryBlock = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	errSymbol = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	catchBlock = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	finallyBlock = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

void astTryCatch::serialize ( BUFFER *buff )
{
	tryBlock->serialize ( buff );
	errSymbol->serialize ( buff );
	catchBlock->serialize ( buff );
	finallyBlock->serialize ( buff );
}

astNode::~astNode ( ) noexcept
{
	if ( left )	delete left;
	if ( right ) delete right;
	freeType ();
}

void astNode::release ( void ) noexcept
{
	if ( left )	delete left;
	if ( right ) delete right;
	left = 0;
	right = 0;
	freeType ();
	op = astOp::dummy;
}

astNode::astNode ( astLSInfo::semanticSymbolType type, srcLocation const &src ) noexcept
{
	op = astOp::btStatement;
	nodeData = astNode::semanticData { type, astLSInfo::semanticLineBreakType::none };
	setLocation ( src );
}
astNode::astNode ( astLSInfo::semanticSymbolType type, srcLocation const &src, size_t len ) noexcept
{
	op = astOp::btStatement;
	nodeData = astNode::semanticData{ type, astLSInfo::semanticLineBreakType::none };
	setLocation ( src );
	location.columnNumberEnd = location.columnNumber + (uint32_t)len;
}

astNode::astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src ) noexcept
{
	op = astOp::btStatement;
	nodeData = astNode::semanticData{ type, lbType };
	setLocation ( src );
}
astNode::astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, size_t len ) noexcept
{
	op = astOp::btStatement;
	nodeData = astNode::semanticData{ type, lbType };
	setLocation ( src );
	location.columnNumberEnd = location.columnNumber + (uint32_t)len;
}

astNode::astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, srcLocation const &assocLocation ) noexcept
{
	op = astOp::btStatement;
	nodeData = astNode::semanticData{ type, lbType, assocLocation };
	setLocation ( src );
}

astNode::astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, srcLocation const &assocLocation, astNode *relatedNode ) noexcept
{
	op = astOp::btStatement;
	nodeData = astNode::semanticData{type, lbType, assocLocation, relatedNode };
	setLocation ( src );
}

astNode::astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, size_t len, srcLocation const &assocLocation ) noexcept
{
	op = astOp::btStatement;
	nodeData = astNode::semanticData{ type, lbType, assocLocation };
	setLocation ( src );
	location.columnNumberEnd = location.columnNumber + (uint32_t)len;
}

astNode::astNode ( stringCache &sCache, astOp oper, srcLocation const &src ) noexcept
{
	op = oper;
	initType ( sCache, op );
	setLocation ( src );
}

astNode::astNode ( errorNum err, srcLocation const &src ) noexcept
{
	op = astOp::errorValue;
	nodeData = err;
	location = src;
}

astNode::astNode ( errorNum err, srcLocation const &src, size_t len ) noexcept
{
	op = astOp::errorValue;
	nodeData = err;
	location = src;
	location.columnNumberEnd = location.columnNumber + (uint32_t)len;
}
astNode *astNode::setLocation ( source const &src ) noexcept
{
	location.set ( src );
	extendedSelection.setEnd ( src );
	return this;
}

astNode::astNode ( symbol *sym, srcLocation const &src ) noexcept : op ( astOp::symbolDef )
{
	nodeData = sym;
	setLocation ( src );
}

astNode::astNode ( symbol *sym ) : op ( astOp::symbolDef )
{
	nodeData = sym;
	switch ( symbolDef()->symType )
	{
		case symbolSpaceType::symTypeLocal:
			{
				auto init = symbolDef()->getInitializer ( symbolDef()->getSymbolName ( ) );
				if ( init )
				{
					setLocation ( init );
				}
			}
			break;
		case symbolSpaceType::symTypeParam:
			{
				symbolSpaceParams &param = dynamic_cast<symbolSpaceParams &>(*symbolDef());

				if ( param.size ( ) )
				{
					auto init = param.getInitializer ( param.getSymbolName ( symbolSpaceScope::symLocalParam, 0 ) );
					setLocation ( init );
				}
			}
			break;
		default:
			break;
	}
}

void astNode::freeType () noexcept
{
	switch ( op )
	{
		case astOp::symbolDef:
			{
				auto tmp = std::get_if<class symbol *> ( &nodeData );
				if ( tmp )
				{
					delete *tmp;
					*tmp = nullptr;
				}
			}
			break;
		default:
			break;
	}
	nodeData = std::monostate();
}

astNode::astNode ( class opFile *file, class sourceFile *srcFile, char const **expr )
{
	op = (astOp) * *expr;
	(*expr)++;

	location = srcLocation ( srcFile, expr );

	switch ( op )
	{
		case astOp::symbolDef:
			nodeData = symbol::symbolFactory ( file, srcFile, expr );
			break;
		case astOp::btBreak:
		case astOp::btCont:
		case astOp::btGoto:
			nodeData = astGoto ( file, srcFile, expr );
			break;
		case astOp::btBasic:
			nodeData = astBasic ( file, srcFile, expr );
			break;
		case astOp::btSwitch:
			nodeData = astSwitch ( file, srcFile, expr );
			break;
		case astOp::btIf:
			nodeData = astIf ( file, srcFile, expr );
			break;
		case astOp::btInline:
			nodeData = astInlineFunc ( file, srcFile, expr );
			break;
		case astOp::btFor:
			nodeData = astLoop ( file, srcFile, expr );
			loopData ().breakLabel = file->sCache.get ( "$for_break" );
			loopData().continueLabel = file->sCache.get ( "$for_continue" );
			break;
		case astOp::btWhile:
			nodeData = astLoop ( file, srcFile, expr );
			loopData().breakLabel = file->sCache.get ( "$while_break" );
			loopData().continueLabel = file->sCache.get ( "$while_continue" );
			break;
		case astOp::btRepeat:
			nodeData = astLoop ( file, srcFile, expr );
			loopData().breakLabel = file->sCache.get ( "$repeat_break" );
			loopData().continueLabel = file->sCache.get ( "$repeat_continue" );
			break;
		case astOp::btForEach:
			nodeData = astForEach ( file, srcFile, expr );
			break;
		case astOp::btTry:
			nodeData = astTryCatch ( file, srcFile, expr );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			nodeData = astReturn ( file, srcFile, expr );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			nodeData = astArray ( file, srcFile, expr );
			break;
		case astOp::codeBlockValue:
			nodeData = astCodeblock ( file, srcFile, expr );
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			nodeData = astPList ( file, srcFile, expr );
			break;
		case astOp::conditional:
			nodeData = astCond ( file, srcFile, expr );
			break;
		case astOp::btLabel:
			nodeData = file->sCache.get ( expr );
			break;
		case astOp::codeBlockExpression:
		case astOp::stringValue:
		case astOp::singleLineComment:
		case astOp::commentBlock:
		case astOp::metadata:
			nodeData = stringi ( expr );
			break;
		case astOp::nameValue:
		case astOp::symbolValue:
		case astOp::atomValue:
		case astOp::fieldValue:
		case astOp::funcValue:
		case astOp::btAnnotation:
			nodeData = file->sCache.get ( expr );
			break;
		case astOp::doubleValue:
			nodeData = *( double *) (*expr);
			*expr += sizeof ( double );
			break;
		case astOp::intValue:
			nodeData = *( int64_t *) (*expr);
			*expr += sizeof ( int64_t );
			break;
		default:
			break;
	}

	left = OP_NODE_DESERIALIZE ( file, srcFile, expr );
	right = OP_NODE_DESERIALIZE ( file, srcFile, expr );
}

astNode::astNode ( astNode &&old ) noexcept
{
	*this = std::move ( old );
}

astNode::astNode ( const astNode &old )
{
	release ( );

	op = old.op;
	location = old.location;
	extendedSelection = old.extendedSelection;

	nodeData = old.nodeData;

	switch ( old.op )
	{
		case astOp::symbolDef:
			switch ( old.symbolDef()->symType )
			{
				case symbolSpaceType::symTypeLocal:
					symbolDef() = new (std::nothrow) symbolLocal ( *dynamic_cast<symbolLocal *>(old.symbolDef()) );
					break;
				case symbolSpaceType::symTypeClass:
					symbolDef() = new (std::nothrow) symbolClass ( *dynamic_cast<symbolClass *>(old.symbolDef()) );
					break;
				case symbolSpaceType::symTypeStatic:
					symbolDef() = new (std::nothrow) symbolStatic ( *dynamic_cast<symbolStatic *>(old.symbolDef()) );
					break;
				case symbolSpaceType::symTypeField:
					symbolDef() = new (std::nothrow) symbolField ( *dynamic_cast<symbolField *>(old.symbolDef()) );
					break;
				case symbolSpaceType::symTypeParam:
					symbolDef() = new (std::nothrow) symbolSpaceParams ( *dynamic_cast<symbolSpaceParams *>(old.symbolDef()) );
					break;
				case symbolSpaceType::symTypeInline:
					symbolDef() = new (std::nothrow)symbolSpaceInline ( *dynamic_cast<symbolSpaceInline *>(old.symbolDef()) );
					break;
				default:
					symbolDef() = nullptr;
					break;
			}
			break;
#if 0
		case astOp::btFor:
			loopData().breakLabel = file->sCache.get ( "$for_break" );
			loopData().continueLabel = file->sCache.get ( "$for_continue" );
			break;
		case astOp::btWhile:
			loopData().breakLabel = file->sCache.get ( "$while_break" );
			loopData().continueLabel = file->sCache.get ( "$while_continue" );
			break;
		case astOp::btRepeat:
			loopData().breakLabel = file->sCache.get ( "$repeat_break" );
			loopData().continueLabel = file->sCache.get ( "$repeat_continue" );
			break;
#endif
		default:
			break;
	}
	if ( old.left )	left = new (std::nothrow) astNode ( *old.left );
	if ( old.right ) right = new (std::nothrow) astNode ( *old.right );
}

astNode &astNode::operator = ( const astNode &old ) noexcept
{
	return *this = astNode ( old );
}

astNode &astNode::operator = ( astNode &&old ) noexcept
{
	// free it an initialize it to the old type and copy old into us
	std::swap ( op, old.op );
	std::swap ( nodeData, old.nodeData );
	std::swap ( location, old.location );
	std::swap ( extendedSelection, old.extendedSelection );
	std::swap ( left, old.left );
	std::swap ( right, old.right );

	return *this;
}

void astNode::serialize ( BUFFER *buff )
{
	if ( (ptrdiff_t) this )
	{
		bufferPut8 ( buff, 1 );
	} else
	{
		bufferPut8 ( buff, 0 );
		return;
	}
	bufferPut8 ( buff, static_cast<uint8_t>(op) );

	location.serialize ( buff );

	switch ( op )
	{
		case astOp::symbolDef:
			symbolDef()->serialize ( buff );
			break;
		case astOp::btBreak:
		case astOp::btCont:
		case astOp::btGoto:
			gotoData().serialize ( buff );
			break;
		case astOp::btBasic:
			basicData().serialize ( buff );
			break;
		case astOp::btSwitch:
			switchData().serialize ( buff );
			break;
		case astOp::btIf:
			ifData().serialize ( buff );
			break;
		case astOp::btInline:
			inlineFunc().serialize ( buff );
			break;
		case astOp::btFor:
		case astOp::btWhile:
		case astOp::btRepeat:
			loopData().serialize ( buff );
			break;
		case astOp::btForEach:
			forEachData().serialize ( buff );
			break;
		case astOp::btTry:
			tryCatchData().serialize ( buff );
			break;
		case astOp::btReturn:
		case astOp::btInlineRet:
			returnData().serialize ( buff );
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			arrayData().serialize ( buff );
			break;
		case astOp::codeBlockValue:
			codeBlock().serialize ( buff );
			break;
		case astOp::funcCall:
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			pList().serialize ( buff );
			break;
		case astOp::conditional:
			conditionData().serialize ( buff );
			break;
		case astOp::btLabel:
			label ().serialize ( buff );
			break;
		case astOp::stringValue:
		case astOp::codeBlockExpression:
		case astOp::singleLineComment:
		case astOp::commentBlock:
		case astOp::metadata:
			stringValue ().serialize ( buff );
			break;
		case astOp::nameValue:
			nameValue ().serialize ( buff );
			break;
		case astOp::symbolValue:
		case astOp::atomValue:
		case astOp::fieldValue:
		case astOp::funcValue:
		case astOp::btAnnotation:
			symbolValue ().serialize ( buff );
			break;
		case astOp::doubleValue:
			buff->put ( doubleValue() );
			break;
		case astOp::intValue:
			buff->put ( intValue() );
			break;
		default:
			break;
	}
	left->serialize ( buff );
	right->serialize ( buff );
}

void astNode::addLocal ( astNode *node, symbolTypeClass &type )
{
	assert ( op == astOp::btBasic );

	symbol *sym;

	if ( node->getOp() == astOp::assign )
	{
		sym = new (std::nothrow) symbolLocal ( node->left->symbolValue(), type, location, "auto-generated", node );
	} else
	{
		assert ( node->getOp() == astOp::symbolValue );
		sym = new (std::nothrow) symbolLocal ( node->symbolValue(), type, location, "auto-generated", node );
	}
	for ( auto it = basicData().blocks.begin ( ); it != basicData().blocks.end ( ); it++ )
	{
		switch ( (*it)->getOp() )
		{
			case astOp::symbolDef:
				basicData().blocks.insert ( it, (new (std::nothrow) astNode ( sym ))->setLocation ( this ) );
				return;
			default:
				break;
		}
	}
	basicData().blocks.push_back ( (new (std::nothrow) astNode ( sym ) )->setLocation ( this ) );
}

symbolLocal *astNode::addSymbol ( cacheString const &name, symbolTypeClass const &type, srcLocation const &loc, astNode *init )
{
	assert ( op == astOp::btBasic );
	auto sym = new (std::nothrow) symbolLocal ( name, type, loc, "automatic", init );

	auto it = basicData().blocks.crbegin ( );
	while ( it != basicData().blocks.crend ( ) )
	{
		if ( (*it)->getOp ( ) == astOp::symbolDef )
		{
			auto x = (new (std::nothrow) astNode ( sym ))->setLocation ( loc );
			basicData().blocks.insert ( it.base ( ), x  );
			return sym;
		}
		it++;
	}

	basicData().blocks.insert ( it.base ( ), (new (std::nothrow) astNode ( sym ))->setLocation ( loc ) );
	return sym;
}

symbolLocal *astNode::addSymbolFront ( cacheString const &name, symbolTypeClass const &type, srcLocation const &loc, astNode *init )
{
	assert ( op == astOp::btBasic );
	auto sym = new (std::nothrow) symbolLocal ( name, type, loc, "undeclared", init );
	basicData().blocks.insert ( basicData().blocks.begin(), (new (std::nothrow) astNode ( sym ))->setLocation ( loc ) );
	return sym;
}

static astNode *forceLocationCB ( astNode *node, astNode *parent, class symbolStack *sym, bool isAccess, bool, accessorType const &acc, srcLocation const &loc )
{
	node->location = loc;
	return node;
}

astNode *astNode::forceLocation ( srcLocation const &loc )
{
	astNodeWalk ( this, nullptr, forceLocationCB, accessorType (), loc );
	return this;
}
