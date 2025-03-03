/*
	class and type definitions for the expression parser

*/

#pragma once

#include <stdarg.h>

#include "Utility/settings.h"

#include "Utility/buffer.h"
#include "Utility/stringCache.h"
#include "bcVM/exeStore.h"
#include "astNodeDef.h"
#include "Utility/sourceFile.h"
#include "Utility/unique_queue.h"
#include "astNodeLSInfo.h"


#define NODE_DEF(name) name,
enum class astOp : size_t
{
	NODE_OPS
};
#undef NODE_DEF

/* the operator categories are only valid for operations... they are NOT valid for control flow nodes */
enum class astOpCat
{
	opUnary,
	opBinary,
	opPrePost,
	opBWAndCompile,
	opSubNegate,
	opPeren,
	opNSSelf,
	opNew,
	opPack,
};


struct opStack
{
	std::deque<astNode *>	elem;

	opStack ()
	{}

	~opStack ();
	void push ( astNode *node );
	astNode *popBack () noexcept;
	bool empty () noexcept;
	int	num () noexcept;
	astNode *popFront () noexcept;
	astNode *back () noexcept
	{
		return elem.back ();
	};
};

struct astWarn
{
	warnNum	num = warnNum::scwNO_WARNING;
	stringi text;
};

struct astCodeblock
{
	BUFFER *buff = nullptr;

	~astCodeblock () noexcept
	{
		if ( buff ) delete buff;
	}

	astCodeblock ( const astCodeblock &old )
	{
		buff = new (std::nothrow) BUFFER ();
		bufferWrite ( buff, bufferBuff ( old.buff ), bufferSize ( old.buff ) );
	}
	astCodeblock () noexcept
	{
		buff = new (std::nothrow) BUFFER ();
	}

	astCodeblock ( class opFile *file, class sourceFile *srcFile, char const **expr )
	{
		buff = new (std::nothrow) BUFFER ();

		uint32_t	len;
		len = *((uint32_t *) *expr);
		(*expr) += sizeof ( uint32_t );

		bufferWrite ( buff, *expr, len );
		(*expr) += len;
	}
	astCodeblock ( astCodeblock &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astCodeblock &operator = ( astCodeblock const &right )
	{
		return *this = astCodeblock ( right );
	}
	astCodeblock &operator = ( astCodeblock &&right ) noexcept
	{
		std::swap ( buff, right.buff );
		return *this;
	}

	void serialize ( BUFFER *buff )
	{
		bufferPut32 ( buff, (uint32_t) bufferSize ( this->buff ) );
		bufferWrite ( buff, bufferBuff ( this->buff ), bufferSize ( this->buff ) );
	}
};

struct astArray
{
	std::vector<class astNode *>	 nodes;

	astArray () noexcept
	{}
	astArray ( const astArray &old );
	astArray ( astArray &&old ) noexcept
	{
		*this = std::move ( old );
	}

	~astArray () noexcept;

	astArray ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astArray &operator = ( astArray const &right )
	{
		return *this = astArray ( right );
	}
	astArray &operator = ( astArray &&right ) noexcept
	{
		nodes.swap ( right.nodes );
		return *this;
	}

	void serialize ( BUFFER *buff );
};

struct astPList
{
	std::vector<class astNode *>	param;
	std::vector<srcLocation>		paramRegion;

	astPList () noexcept
	{}
	astPList ( const astPList &old );
	~astPList () noexcept;

	astPList ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astPList ( astPList &&old ) noexcept = default;
	astPList &operator = ( astPList const &right ) = default;
	astPList &operator = ( astPList &&right ) noexcept = default;

	void serialize ( BUFFER *buff );
};

struct astCondBlock
{
	class	astNode *condition = nullptr;
	class	astNode *block = nullptr;

	astCondBlock () noexcept
	{}
	~astCondBlock () noexcept;
	astCondBlock ( const astCondBlock &block );
	astCondBlock ( class opFile *file, sourceFile *srcFile, char const **expr );
	astCondBlock ( astCondBlock &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astCondBlock &operator = ( astCondBlock const &right )
	{
		return *this = astCondBlock ( right );
	}
	astCondBlock &operator = ( astCondBlock &&right ) noexcept
	{
		std::swap ( condition, right.condition );
		std::swap ( block, right.block );
		return *this;
	}

	void serialize ( BUFFER *buffer );
};

struct astCond
{
	astNode *trueCond = nullptr;
	astNode *falseCond = nullptr;

	astCond () noexcept
	{}
	~astCond () noexcept;
	astCond ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astCond ( const astCond &old );
	astCond ( astCond &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astCond &operator = ( astCond const &right )
	{
		return *this = astCond ( right );
	}
	astCond &operator = ( astCond &&right ) noexcept
	{
		std::swap ( trueCond, right.trueCond );
		std::swap ( falseCond, right.falseCond );
		return *this;
	}

	void serialize ( BUFFER *buff );
};

struct astBasic
{
	std::vector<astNode *> blocks;
	bool emitPops = true;

	astBasic () noexcept
	{}
	~astBasic () noexcept;
	astBasic ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astBasic ( const astBasic &old );
	astBasic ( astBasic &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astBasic &operator = ( astBasic const &right )
	{
		return *this = astBasic ( right );
	}
	astBasic &operator = ( astBasic &&right ) noexcept
	{
		std::swap ( blocks, right.blocks );
		std::swap ( emitPops, right.emitPops );
		return *this;
	}

	void serialize ( BUFFER *buff );
	bool isCombinable ();
};

struct astSwitch
{
	astNode *condition = nullptr;
	std::vector<astCondBlock *>	 caseList;
	cacheString					 breakLabel;

	astSwitch ( stringCache &sCache ) noexcept : breakLabel ( sCache.get ( "$switch_break" ) )
	{}
	~astSwitch () noexcept;
	astSwitch ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astSwitch ( const astSwitch &old );
	astSwitch ( astSwitch &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astSwitch &operator = ( astSwitch const &right )
	{
		return *this = astSwitch ( right );
	}
	astSwitch &operator = ( astSwitch &&old ) noexcept
	{
		std::swap ( condition, old.condition );
		std::swap ( caseList, old.caseList );
		std::swap ( breakLabel, old.breakLabel );
		return *this;
	}
	void serialize ( BUFFER *buff );
};

struct astIf
{
	std::vector<astCondBlock *>	 ifList;
	astNode *elseBlock = nullptr;

	astIf () noexcept
	{}
	~astIf () noexcept;
	astIf ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astIf ( const astIf &old );
	astIf ( astIf &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astIf &operator = ( astIf const &right )
	{
		return *this = astIf ( right );
	}
	astIf &operator = ( astIf &&old ) noexcept
	{
		std::swap ( ifList, old.ifList );
		std::swap ( elseBlock, old.elseBlock );
		return *this;
	}

	void serialize ( BUFFER *buff );
};

struct astInlineFunc
{
	cacheString					 funcName;
	astNode *node = nullptr;
	symbolTypeClass				 retType;
	int32_t						 retIndex = 0;
	bool						 needRet = false;

	astInlineFunc () {}
	~astInlineFunc () noexcept;
	astInlineFunc ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astInlineFunc ( const astInlineFunc &old );
	astInlineFunc ( astInlineFunc &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astInlineFunc &operator = ( astInlineFunc const &right )
	{
		return *this = astInlineFunc ( right );
	}
	astInlineFunc &operator = ( astInlineFunc &&old ) noexcept
	{
		std::swap ( funcName, old.funcName );
		std::swap ( node, old.node );
		std::swap ( retType, old.retType );
		return *this;
	}
	void serialize ( BUFFER *buff );
};

struct astGoto
{
	cacheString					 label;
	uint32_t					 nPop = 0;
	astNode *finallyBlock = nullptr;

	astGoto ( stringCache &sCache ) noexcept : label ( sCache.get ( "$goto" ) )
	{}
	~astGoto () noexcept;
	astGoto ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astGoto ( const astGoto &old );
	astGoto ( astGoto &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astGoto &operator = ( astGoto const &right )
	{
		return *this = astGoto ( right );
	}
	astGoto &operator = ( astGoto &&old ) noexcept
	{
		std::swap ( label, old.label );
		std::swap ( nPop, old.nPop );
		std::swap ( finallyBlock, old.finallyBlock );
		return *this;
	}
	void serialize ( BUFFER *buff );
};

struct astLoop
{
	astNode *initializers = nullptr;
	astNode *condition = nullptr;
	astNode *increase = nullptr;
	astNode *block = nullptr;
	cacheString  continueLabel;
	cacheString  breakLabel;

	astLoop ( stringCache &sCache ) noexcept : continueLabel ( sCache.get ( "$loop_continue" ) ), breakLabel ( sCache.get ( "$loop_break" ) )
	{}
	~astLoop () noexcept;
	astLoop ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astLoop ( const astLoop &old );
	astLoop ( astLoop &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astLoop &operator = ( astLoop const &right )
	{
		return *this = astLoop ( right );
	}
	astLoop &operator = ( astLoop &&old ) noexcept
	{
		std::swap ( initializers, old.initializers );
		std::swap ( condition, old.condition );
		std::swap ( increase, old.increase );
		std::swap ( block, old.block );
		std::swap ( continueLabel, old.continueLabel );
		std::swap ( breakLabel, old.breakLabel );
		return *this;
	}
	void serialize ( BUFFER *buff );
};

struct astForEach
{
	astNode *var = nullptr;
	astNode *in = nullptr;
	astNode *statement = nullptr;

	astForEach () noexcept
	{}
	~astForEach () noexcept;
	astForEach ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astForEach ( const astForEach &old );
	astForEach ( astForEach &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astForEach &operator = ( astForEach const &right )
	{
		return *this = astForEach ( right );
	}
	astForEach &operator = ( astForEach &&old ) noexcept
	{
		std::swap ( var, old.var );
		std::swap ( in, old.in );
		std::swap ( statement, old.statement );
		return *this;
	}
	void serialize ( BUFFER *buff );
};

struct astReturn
{
	astNode *node = nullptr;
	astNode *finallyBlock = nullptr;
	bool		 isYield = false;
	cacheString	 label; // = "$ret";		// for overridden return points
	uint32_t	 nPop = 0;

	astReturn ( stringCache &sCache ) noexcept : label ( sCache.get ( "$ret" ) )
	{}
	~astReturn () noexcept;
	astReturn ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astReturn ( const astReturn &old );
	astReturn ( astReturn &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astReturn &operator = ( astReturn const &right )
	{
		return *this = astReturn ( right );
	}
	astReturn &operator = ( astReturn &&old ) noexcept
	{
		std::swap ( node, old.node );
		std::swap ( finallyBlock, old.finallyBlock );
		std::swap ( isYield, old.isYield );
		std::swap ( label, old.label );
		std::swap ( nPop, old.nPop );
		return *this;
	}
	void serialize ( BUFFER *buff );
};

struct astTryCatch
{
	astNode *tryBlock = nullptr;
	astNode *errSymbol = nullptr;
	astNode *catchBlock = nullptr;
	astNode *finallyBlock = nullptr;

	astTryCatch () noexcept
	{}
	~astTryCatch () noexcept;
	astTryCatch ( class opFile *file, class sourceFile *srcFile, char const **expr );
	astTryCatch ( const astTryCatch &old );
	astTryCatch ( astTryCatch &&old ) noexcept
	{
		*this = std::move ( old );
	}
	astTryCatch &operator = ( astTryCatch const &right )
	{
		return *this = astTryCatch ( right );
	}
	astTryCatch &operator = ( astTryCatch &&old ) noexcept
	{
		std::swap ( tryBlock, old.tryBlock );
		std::swap ( errSymbol, old.errSymbol );
		std::swap ( catchBlock, old.catchBlock );
		std::swap ( finallyBlock, old.finallyBlock );
		return *this;
	}
	void serialize ( BUFFER *buff );
};

#define OP_NODE_DESERIALIZE(file,src,expr) ( *((*(expr))++) ? new (std::nothrow) astNode ( (file), (src), (expr) ) : 0 )

using astExtendedSelection = srcLocation;

class astNode final
{
	astOp					 op = astOp::dummy;

	struct semanticData
	{
		astLSInfo::semanticSymbolType		 symbolType = astLSInfo::semanticSymbolType::none;
		astLSInfo::semanticLineBreakType	 lineBreakType = astLSInfo::semanticLineBreakType::none;
		srcLocation							 assocLocation;
		astNode								*relatedNode = 0;

		private:
		semanticData ( astLSInfo::semanticSymbolType const &symbolType, astLSInfo::semanticLineBreakType const &lineBreakType, srcLocation const &assocLocation, astNode *relatedNode ) : symbolType ( symbolType ), lineBreakType ( lineBreakType ), assocLocation ( assocLocation ), relatedNode ( relatedNode )
		{}

		semanticData ( astLSInfo::semanticSymbolType const &symbolType, astLSInfo::semanticLineBreakType const &lineBreakType, srcLocation const &assocLocation ) : symbolType ( symbolType ), lineBreakType ( lineBreakType ), assocLocation ( assocLocation )
		{}

		semanticData ( astLSInfo::semanticSymbolType const &symbolType, astLSInfo::semanticLineBreakType const &lineBreakType ) : symbolType ( symbolType ), lineBreakType ( lineBreakType )
		{}

		friend class astNode;
	};

	private:
	std::variant <
		std::monostate,
		int64_t,
		double,
		cacheString,
		class symbol *,
		astArray,
		astCodeblock,
		astPList,
		astCond,
		astBasic,
		astSwitch,
		astIf,
		astInlineFunc,
		astGoto,
		astNode *,
		astLoop,
		astForEach,
		astReturn,
		astTryCatch,
		stringi,
		errorNum,
		astWarn,
		semanticData
	> nodeData;

	public:
	class astNode *left = nullptr;
	class astNode *right = nullptr;

	srcLocation				 location;
	astExtendedSelection	 extendedSelection;		// used for language server


	astOp getOp () const noexcept
	{
		return op;
	}

	void setOp ( stringCache &sCache, astOp newOp )
	{
		freeType ();
		initType ( sCache, newOp );
		op = newOp;
	}

	void setError ( errorNum err )
	{
		freeType ();
		if ( left )
		{
			left = nullptr;
			delete left;
		}
		if ( right )
		{
			delete right;
			right = nullptr;
		}
		nodeData = err;
		op = astOp::errorValue;
	}

	void forceOp ( astOp newOp ) noexcept
	{
		op = newOp;
	}

	errorNum errorData ()
	{
		return std::get<errorNum> ( nodeData );
	}

	astBasic &basicData ()
	{
		return std::get<astBasic> ( nodeData );
	}

	astBasic const &basicData () const
	{
		return std::get<astBasic> ( nodeData );
	}

	astArray &arrayData ()
	{
		return std::get<astArray> ( nodeData );
	}

	astArray const &arrayData () const
	{
		return std::get<astArray> ( nodeData );
	}

	astCodeblock &codeBlock ()
	{
		return std::get<astCodeblock> ( nodeData );
	}

	astCodeblock const &codeBlock () const
	{
		return std::get<astCodeblock> ( nodeData );
	}

	astPList &pList ()
	{
		return std::get<astPList> ( nodeData );
	}

	astPList const &pList () const
	{
		return std::get<astPList> ( nodeData );
	}

	astSwitch &switchData ()
	{
		return std::get<astSwitch> ( nodeData );
	}

	astSwitch const &switchData () const
	{
		return std::get<astSwitch> ( nodeData );
	}

	astCond &conditionData ()
	{
		return std::get<astCond> ( nodeData );
	}

	astCond const &conditionData () const
	{
		return std::get<astCond> ( nodeData );
	}

	astIf &ifData ()
	{
		return std::get<astIf> ( nodeData );
	}

	astIf const &ifData () const
	{
		return std::get<astIf> ( nodeData );
	}

	astInlineFunc &inlineFunc ()
	{
		return std::get<astInlineFunc> ( nodeData );
	}

	astInlineFunc const &inlineFunc () const
	{
		return std::get<astInlineFunc> ( nodeData );
	}

	astGoto &gotoData ()
	{
		return std::get<astGoto> ( nodeData );
	}

	astGoto const &gotoData () const
	{
		return std::get<astGoto> ( nodeData );
	}

	astNode *&node ()
	{
		return std::get<astNode *> ( nodeData );
	}

	astNode *const &node () const
	{
		return std::get<astNode *> ( nodeData );
	}

	astLoop &loopData ()
	{
		return std::get<astLoop> ( nodeData );
	}

	astLoop const &loopData () const
	{
		return std::get<astLoop> ( nodeData );
	}

	astForEach &forEachData ()
	{
		return std::get<astForEach> ( nodeData );
	}

	astForEach const &forEachData () const
	{
		return std::get<astForEach> ( nodeData );
	}

	astReturn &returnData ()
	{
		return std::get<astReturn> ( nodeData );
	}

	astReturn const &returnData () const
	{
		return std::get<astReturn> ( nodeData );
	}

	astTryCatch &tryCatchData ()
	{
		return std::get<astTryCatch> ( nodeData );
	}

	astTryCatch const &tryCatchData () const
	{
		return std::get<astTryCatch> ( nodeData );
	}

	astWarn const &warnData () const
	{
		return std::get<astWarn> ( nodeData );
	}

	int64_t &intValue ()
	{
		return std::get<int64_t> ( nodeData );
	}

	int64_t const &intValue () const
	{
		return std::get<int64_t> ( nodeData );
	}

	double &doubleValue ()
	{
		return std::get<double> ( nodeData );
	}

	double const &doubleValue () const
	{
		return std::get<double> ( nodeData );
	}

	cacheString &label ()
	{
		return std::get<cacheString> ( nodeData );
	}

	cacheString const &label () const
	{
		return std::get<cacheString> ( nodeData );
	}

	stringi &stringValue ()
	{
		return std::get<stringi> ( nodeData );
	}

	stringi const &stringValue () const
	{
		return std::get<stringi> ( nodeData );
	}

	cacheString &symbolValue ()
	{
		return std::get<cacheString> ( nodeData );
	}

	cacheString const &symbolValue () const
	{
		return std::get<cacheString> ( nodeData );
	}

	cacheString const &nameValue () const
	{
		return std::get<cacheString> ( nodeData );
	}

	cacheString &nameValue ()
	{
		return std::get<cacheString> ( nodeData );
	}

	astLSInfo::semanticSymbolType &getSemanticType ()
	{
		return std::get<semanticData> ( nodeData ).symbolType;
	}
	astLSInfo::semanticSymbolType const &getSemanticType () const
	{
		return std::get<semanticData> ( nodeData ).symbolType;
	}

	astLSInfo::semanticLineBreakType &getSemanticLineBreakType ()
	{
		return std::get<semanticData> ( nodeData ).lineBreakType;
	}
	astLSInfo::semanticLineBreakType const &getSemanticLineBreakType () const
	{
		return std::get<semanticData> ( nodeData ).lineBreakType;
	}

	srcLocation &getSemanticAssociatedLocation ()
	{
		return std::get<semanticData> ( nodeData ).assocLocation;
	}
	srcLocation const &getSemanticAssociatedLocation () const
	{
		return std::get<semanticData> ( nodeData ).assocLocation;
	}

	srcLocation &getExtendedSelection ()
	{
		return extendedSelection;
	}

	srcLocation const &getExtendedSelection () const
	{
		return extendedSelection;
	}

	class symbol *&symbolDef ()
	{
		return std::get<class symbol *> ( nodeData );
	}

	class symbol *const &symbolDef () const
	{
		return std::get<class symbol *> ( nodeData );
	}

	void initType ( stringCache &sCache, astOp op ) noexcept
	{
		switch ( op )
		{
			case astOp::btBreak:
			case astOp::btCont:
			case astOp::btGoto:
				nodeData = astGoto ( sCache );
				break;
			case astOp::btBasic:
				nodeData = astBasic ();
				break;
			case astOp::btSwitch:
				nodeData = astSwitch ( sCache );
				break;
			case astOp::btIf:
				nodeData = astIf ();
				break;
			case astOp::btInline:
				nodeData = astInlineFunc ();
				break;
			case astOp::btFor:
				nodeData = astLoop ( sCache );
				loopData ().breakLabel = sCache.get ( "$for_break" );
				loopData ().continueLabel = sCache.get ( "$for_continue" );
				break;
			case astOp::btWhile:
				nodeData = astLoop ( sCache );
				loopData ().breakLabel = sCache.get ( "$while_break" );
				loopData ().continueLabel = sCache.get ( "$while_continue" );
				break;
			case astOp::btRepeat:
				nodeData = astLoop ( sCache );
				loopData ().breakLabel = sCache.get ( "$repeat_break" );
				loopData ().continueLabel = sCache.get ( "$repeat_continue" );
				break;
			case astOp::btForEach:
				nodeData = astForEach ();
				break;
			case astOp::btTry:
				nodeData = astTryCatch ();
				break;
			case astOp::btReturn:
			case astOp::btInlineRet:
				nodeData = astReturn ( sCache );
				break;
			case astOp::btLabel:
			case astOp::symbolValue:
			case astOp::nameValue:
			case astOp::atomValue:
			case astOp::fieldValue:
			case astOp::funcValue:
				nodeData = cacheString ();
				break;
			case astOp::codeBlockExpression:
			case astOp::stringValue:
			case astOp::btAnnotation:
			case astOp::singleLineComment:
			case astOp::commentBlock:
			case astOp::metadata:
				nodeData = stringi ();
				break;
			case astOp::varrayValue:
			case astOp::arrayValue:
			case astOp::symbolPack:
				nodeData = astArray ();
				break;
			case astOp::codeBlockValue:
				nodeData = astCodeblock ();
				break;
			case astOp::funcCall:
			case astOp::arrayDeref:
			case astOp::cArrayDeref:
				nodeData = astPList ();
				break;
			case astOp::conditional:
				nodeData = astCond ();
				break;
			case astOp::intValue:
				nodeData = 0LL;
				break;
			case astOp::doubleValue:
				nodeData = 0.0;
				break;
			case astOp::errorValue:
				assert ( 0 );
				nodeData = errorNum::ERROR_OK;
				break;
			case astOp::warnValue:
				nodeData = astWarn ();
				break;
			case astOp::btStatement:
				assert ( 0 );
				nodeData = astNode::semanticData{astLSInfo::semanticSymbolType::keywordOther, astLSInfo::semanticLineBreakType::none};
				break;
			default:
				nodeData = std::monostate ();
				break;
		}
	}

	void freeType () noexcept;
	astNode ()
	{
		op = astOp::endParse;
	}
	astNode ( stringCache &sCache, astOp oper )
	{
		op = oper;
		initType ( sCache, op );
	}
	astNode ( stringCache &sCache, astOp oper, astNode *left, astNode *right )
	{
		op = oper;
		initType ( sCache, op );
		this->left = left;
		this->right = right;
	}
	astNode ( stringCache &sCache, astOp oper, astNode *left )
	{
		op = oper;
		initType ( sCache, op );
		this->left = left;
	}
	astNode ( stringCache &sCache, astOp oper, int64_t intValue )
	{
		op = oper;
		nodeData = intValue;
	}
	astNode ( stringCache &sCache, astOp oper, double dblValue )
	{
		op = oper;
		nodeData = dblValue;
	}
	astNode ( astOp oper, cacheString const &strValue )
	{
		op = oper;
		nodeData = strValue;
	}

	astNode ( stringCache &sCache, astOp oper, char const *const format, ... )
	{
		va_list		marker;

		op = oper;

		if ( oper == astOp::stringValue || oper == astOp::codeBlockExpression || oper == astOp::btAnnotation )
		{
			char		tmpBuff[512];
			va_start ( marker, format );
			vsprintf_s ( tmpBuff, sizeof ( tmpBuff ), format, marker );
			va_end ( marker );

			nodeData = stringi ( tmpBuff );
		} else
		{
			va_start ( marker, format );
			nodeData = sCache.get ( format, marker );
			va_end ( marker );
		}
	}

	astNode ( stringCache &sCache, warnNum num, char const *const format, ... )
	{
		va_list		marker;

		op = astOp::warnValue;


		char		tmpBuff[512];
		va_start ( marker, format );
		vsprintf_s ( tmpBuff, sizeof ( tmpBuff ), format, marker );
		va_end ( marker );

		nodeData = astWarn{num, stringi ( tmpBuff )};
	}

	astNode ( stringCache &sCache, astOp oper, cacheString const &str )
	{
		op = oper;
		nodeData = str;
	}

#if 0
	astNode ( stringCache &sCache, astOp oper, astNode *left, char const *format, ... )
	{
		va_list		marker;

		op = oper;
		this->left = left;

		va_start ( marker, format );
		nodeData = sCache.get ( format, marker );
	}
#endif

#if 0
	astNode ( file->sCache, astOp oper, char const *format, ... )
	{
		char		tmpBuff[512];
		va_list		marker;

		va_start ( marker, format );
		vsprintf_s ( tmpBuff, sizeof ( tmpBuff ), format, marker );

		op = oper;
		nodeData = tmpBuff;
}
	astNode ( file->sCache, astOp oper, astNode *left, char const *format, ... )
	{
		char		tmpBuff[512];
		va_list		marker;

		va_start ( marker, format );
		vsprintf_s ( tmpBuff, sizeof ( tmpBuff ), format, marker );

		op = oper;
		nodeData = tmpBuff;
		this->left = left;
	}
#endif

	astNode ( errorNum err, srcLocation const &src ) noexcept;
	astNode ( errorNum err, srcLocation const &src, size_t len ) noexcept;
	astNode ( astLSInfo::semanticSymbolType type, srcLocation const &src ) noexcept;
	astNode ( astLSInfo::semanticSymbolType type, srcLocation const &src, size_t len ) noexcept;
	astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src ) noexcept;
	astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, size_t len ) noexcept;
	astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, srcLocation const &assocLocation ) noexcept;
	astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, srcLocation const &assocLocation, astNode *relatedNode ) noexcept;
	astNode ( astLSInfo::semanticSymbolType type, astLSInfo::semanticLineBreakType lbType, srcLocation const &src, size_t len, srcLocation const &assocLocation ) noexcept;
	astNode ( stringCache &sCache, astOp oper, srcLocation const &src ) noexcept;
	astNode ( class symbol *sym, srcLocation const &src ) noexcept;
	astNode ( class symbol *sym );
	astNode ( const astNode &old );
	astNode ( astNode &&old ) noexcept;
	astNode &operator = ( const astNode &old ) noexcept;
	astNode &operator = ( astNode &&old ) noexcept;

	operator srcLocation const &() const
	{
		return location;
	}
	operator srcLocation const &()
	{
		return location;
	}
	// deserializer
	astNode ( class opFile *file, class sourceFile *srcFile, char const **expr );

	~astNode () noexcept;
	void release () noexcept;

	bool operator == ( astNode const &right ) const
	{
		if ( op != right.op ) return false;
		switch ( op )
		{
			case astOp::intValue:
				if ( intValue () != right.intValue () ) return false;
				break;
			case astOp::doubleValue:
				if ( doubleValue () != right.doubleValue () ) return false;
				break;
			case astOp::stringValue:
			case astOp::codeBlockExpression:
				if ( stringValue () != right.stringValue () ) return false;
				break;
			case astOp::atomValue:
			case astOp::funcValue:
			case astOp::fieldValue:
				if ( symbolValue () != right.symbolValue () ) return false;
				break;
			case astOp::nameValue:
				if ( nameValue () != right.nameValue () ) return false;
				break;
			case astOp::nullValue:
				break;
			default:
				// TODO: flesh this out for future use
				return false;
		}
		return true;
	}

	// this is a relatively dangerous function to use... it does NOT handle releasing the data union...  this is by design
	// TODO: assure that op is not one of the values that have the data union utilizing complex constructors (avoid memory leakage)

	astNode *setLine ( uint32_t line ) noexcept
	{
		location.lineNumberStart = line;
		location.lineNumberEnd = line;
		return this;
	}

	astNode *forceLocation ( astNode const *block )
	{
		return forceLocation ( block->location );
	}

	astNode *forceLocation ( srcLocation const &loc );

	astNode *setLocation ( astNode const *block ) noexcept
	{
		if ( block )
		{
			location = block->location;
		}
		return this;
	}

	astNode *setLocation ( srcLocation const &location ) noexcept
	{
		this->location = location;
		this->extendedSelection = location;
		return this;
	}

	astNode *setEnd ( source const &src ) noexcept
	{
		location.setEnd ( src );
		extendedSelection.setEnd ( src );
		return this;
	}

	astNode *setEnd ( srcLocation const &src ) noexcept
	{
		location.setEnd ( src );
		extendedSelection.setEnd ( src );
		return this;
	}

	astNode *setLocation ( source const &src ) noexcept;

	bool operator < ( astNode const &rhs ) const
	{
		return location < rhs.location;
	}

	void pushStatement ( astLSInfo::semanticSymbolType type, srcLocation &src, size_t len )
	{
		basicData ().blocks.push_back ( new astNode ( type, src ) );
		basicData ().blocks.back ()->location.columnNumberEnd = (uint32_t) (basicData ().blocks.back ()->location.columnNumber + len);
	}
	void pushStatement ( astLSInfo::semanticSymbolType type, srcLocation &src )
	{
		basicData ().blocks.push_back ( new astNode ( type, src ) );
	}

	void pushNode ( astNode *block )
	{
		basicData ().blocks.push_back ( block );
	}

	void pushNodeFront ( astNode *block )
	{
		basicData ().blocks.insert ( basicData ().blocks.begin (), block );
	}

	class symbolLocal *addSymbol ( cacheString const &name, symbolTypeClass const &type, srcLocation const &loc, astNode *init = 0 );
	class symbolLocal *addSymbolFront ( cacheString const &name, symbolTypeClass const &type, srcLocation const &loc, astNode *init = 0 );
	size_t infixPriority ();
	size_t stackPriority ();
	bool isLeft ();
	bool isUnaryOp ();
	bool isBinaryOp ();
	bool isValue ();
	bool isValueType ();
	bool isTrue ();
	bool isFalse ();
	bool isNull ();

	astNode *firstBreak ( class symbolStack *sym );	// used when doing switch case specialization to ensure we don't have breaks/etc embedded

	// here are some optimzation steps

	// determines wheter a symbol is a variable or a function identifier (atom) and converts to atoms if necessary.  Also converts workspace references from symbols
	// to field or string identifiers as appropriate so we don't have inadvertant symbol creation.  Also converts nSpace references to strings instead of symbols
	class symbolTypeClass	 getType ( class symbolStack *sym );
	symbolType				 getCompType ( class symbolStack *sym );

	void					 morphAssignment ( opFile *file );
	void					 morphNew ( opFile *file );
	void					 morphRelease ( opFile *file );
	void					 serialize ( BUFFER *buff );



	bool					 hasNoSideEffects ( symbolStack *sym, bool allowArrayAccess );
	bool					 hasNoSideEffectsDebugAdapter ( symbolStack *sym, bool allowArrayAccess );
	bool					 isMetadata ();
	bool					 alwaysReturns ();
	void					 removeUnnecessaryJmp ( symbolStack *sym, cacheString const &label );
	astNode *isReturn ( symbolStack *sym );
	// here are some optimzation steps
	astNode *pruneRefs ( bool prune );
	astNode *symbolsToIVar ( cacheString const &name, class symbolStack &sym, bool prepend );

	symbolTypeClass			typeInfer ( class compExecutable *comp, accessorType const &acc, class errorHandler *err, symbolStack *sym, symbolTypeClass *retType, bool interProcedural, bool needValue, bool markUsed, unique_queue<accessorType> *scanQueue, bool isLS );
	void					markInUse ( class compExecutable *comp, accessorType const &acc, class errorHandler *err, symbolStack *sym, bool interProcedural, unique_queue<accessorType> *scanQueue, bool isLS );

	// these call node walkers
	bool					hasCodeblock ();
	bool					hasFunctionReference ( class symbolStack *sym );
	void					forceVariants ( symbolStack *sym );

	void					makeSymbols ( class errorHandler *err, class symbolStack *sym, astNode *addPt, class symbolStack *parentSym, class opFunction *func, class compExecutable *file, class symbolSpaceLocalAdd *localAdd, class symbolSpaceClosures *closureAdd, bool isLS, bool isAccess );
	void					makeSymbolsCodeBlock ( class errorHandler *err, class symbolStack *sym, astNode *addPt, class symbolStack *parentSym, class opFunction *func, class compExecutable *file, class symbolSpaceLocalAdd *localAdd, class symbolSpaceClosures *closureAdd, bool isAccess );
	void					validate ( symbolStack *sym, class errorHandler *err );
	void					makeAtoms ( symbolStack *sym, class errorHandler *err, bool isLS, opFunction *func );
	void					makeCodeblock ( symbolStack *sym, class errorHandler *err );
	void					makeAtomsCodeBlock ( symbolStack *sym, class errorHandler *err );
	bool					makeKnown ( class opFile *file );
	void					combineBlocks ();
	void					resetType ( class opFile *file );
	void					renameLocal ( cacheString const &oldName, cacheString const &newName );
	void					propagateValue ( cacheString const &oldName, astNode *value );
	void					makeDotNodeList ( symbolStack *sym, BUFFER *buff );
	void					makeDotGraph ( symbolStack *sym, BUFFER *buff );
	void					fixupReturn ( symbolStack *sym, class opFunction *func );
	void					addLocal ( astNode *init, symbolTypeClass &type );
	size_t					complexity ();
	void					adjustLocation ( srcLocation &parentLocation );

	// language server support
	friend void swap ( astNode &first, astNode &second ) noexcept;
};

extern void markClassMethodsInuse ( class compExecutable *comp, accessorType const &acc, class opClass *cls, symbolStack const *sym, unique_queue<accessorType> *scanQueue, bool isGeneric, bool isLS );
extern void markClassInuse ( class compExecutable *comp, accessorType const &acc, class opClass *cls, symbolStack const *sym, unique_queue<accessorType> *scanQueue, bool isLS );