/*

	astNode

*/

#include "compilerParser/fileParser.h"
#include "astNodeWalk.h"

#define NODE_DEF(name) {#name}, 
static struct {
	char const *txt;
} opNames[] = {
	NODE_OPS
};
#undef NODE_DEF

static void emitNode ( astNode *node, BUFFER &buff )
{
	bufferPrintf ( &buff, "\"N%x\" [", node );
	bufferPrintf ( &buff, "shape=\"%s\" ", "record" );
	bufferPrintf ( &buff, "label=\"<f0>%s", opNames[static_cast<size_t>(node->getOp())] );
	bufferPrintf ( &buff, "| %x", node );
}
static void emitNode ( astNode *node, BUFFER &buff, char const *shape, uint32_t sides )
{
	bufferPrintf ( &buff, "\"N%x\" [", node );
	bufferPrintf ( &buff, "shape=\"%s\" sides=%d ", shape, sides );
	bufferPrintf ( &buff, "label=\"%s", opNames[static_cast<size_t>(node->getOp ( ))] );
	bufferPrintf ( &buff, " | %x", node );
}
static void emitNode ( astNode *node, BUFFER &buff, int64_t value )
{
	bufferPrintf ( &buff, "\"N%x\" [", node );
	bufferPrintf ( &buff, "shape=\"polygon\" sides=6 " );
	bufferPrintf ( &buff, "label=\"%lld", value );
	bufferPrintf ( &buff, " | %x", node );
}
static void emitNode ( astNode *node, BUFFER &buff, char const * value )
{
	bufferPrintf ( &buff, "\"N%x\" [", node );
	bufferPrintf ( &buff, "shape=\"polygon\" sides=6 " );
	bufferPrintf ( &buff, "label=\"\\\"%s\\\"", value );
	bufferPrintf ( &buff, " | %x", node );
}
static void emitNode ( astNode *node, BUFFER &buff, cacheString const &value )
{

	bufferPrintf ( &buff, "\"N%x\" [", node );
	bufferPrintf ( &buff, "shape=\"polygon\" sides=6 " );
	bufferPrintf ( &buff, "label=\"\\\"%s\\\"", value.c_str() );
	bufferPrintf ( &buff, " | %x", node );
}
static void emitNode ( astNode *node, BUFFER &buff, double value )
{
	bufferPrintf ( &buff, "\"N%x\" [", node );
	bufferPrintf ( &buff, "shape=\"polygon\" sides=6 " );
	bufferPrintf ( &buff, "label=\"%e", value );
	bufferPrintf ( &buff, " | %x", node );
}

static astNode *makeDotNodesCB ( astNode *node, astNode *parent, symbolStack *sym, bool isAccess, bool isInFunctionCall, BUFFER &buff )
{
	uint32_t		arrIndex;

	switch ( node->getOp() )
	{
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					emitNode ( node, buff );
					if ( node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName ( ) )->getOp() == astOp::assign )
					{
						bufferPrintf ( &buff, "|<f%x> symbol %s", node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName ( ) )->right, node->symbolDef()->getSymbolName ( ).c_str() );
					} else
					{
						bufferPrintf ( &buff, "| symbol %s", node->symbolDef()->getSymbolName ( ).c_str() );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						emitNode ( node, buff );
						auto symbol = dynamic_cast<symbolSpaceParams *>(node->symbolDef());
						for ( uint32_t it = 0; it < symbol->size ( ); it++ )
						{
							if ( (*symbol)[it]->initializer->getOp () == astOp::assign )
							{
								bufferPrintf ( &buff, "|<f%x> symbol %s", (*symbol)[it]->initializer, (*symbol)[it]->name.c_str () );
							} else
							{
								bufferPrintf ( &buff, "|symbol %s", (*symbol)[it]->name.c_str () );
							}
						}
					}
					break;
				default:
					return node;

			}
			break;
		case astOp::btBasic:
			emitNode ( node, buff );
			for (auto it = node->basicData().blocks.begin(); it != node->basicData().blocks.end(); it++ )
			{
				bufferPrintf ( &buff, "|<f%x> N%x", (*it), (*it) );
			}
			break;
		case astOp::btSwitch:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> Switch Cond", node->switchData().condition );
			for ( auto it = node->switchData().caseList.begin(); it != node->switchData().caseList.end(); it++ )
			{
				bufferPrintf ( &buff, "|<f%x> case astOp::Cond", (*it)->condition );
				bufferPrintf ( &buff, "|<f%x> case astOp::Block", (*it)->block );
			}
			break;
		case astOp::btIf:
			emitNode ( node, buff );
			for ( auto it = node->ifData().ifList.begin(); it != node->ifData().ifList.end(); it++ )
			{
				bufferPrintf ( &buff, "|<f%x> cond", (*it)->condition );
				bufferPrintf ( &buff, "|<f%x> block", (*it)->block );
			}
			if ( node->ifData().elseBlock )
			{
				bufferPrintf ( &buff, "|<f%x> else", node->ifData().elseBlock );
			}
			break;
		case astOp::btInline:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> %x", node->inlineFunc().node, node->inlineFunc().node );
			break;
		case astOp::btFor:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> initializers", node->loopData().initializers );
			bufferPrintf ( &buff, "|<f%x> condition", node->loopData().condition );
			bufferPrintf ( &buff, "|<f%x> increase", node->loopData().increase );
			bufferPrintf ( &buff, "|<f%x> statement", node->loopData().block );
			break;
		case astOp::btForEach:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> in", node->forEachData().in );
			bufferPrintf ( &buff, "|<f%x> statement", node->forEachData().statement );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> condition", node->loopData().condition );
			bufferPrintf ( &buff, "|<f%x> block", node->loopData().block );
			break;
		case astOp::btGoto:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|dest = %s", node->gotoData().label.c_str() );
			break;
		case astOp::btTry:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> try", node->tryCatchData().tryBlock );
			bufferPrintf ( &buff, "|<f%x> catch", node->tryCatchData().catchBlock );
			bufferPrintf ( &buff, "|<f%x> finally", node->tryCatchData().finallyBlock );
			break;
		case astOp::btReturn:
			emitNode ( node, buff );
			if ( node->returnData ().node )
			{
				bufferPrintf ( &buff, "|<f%x> return", node->returnData ().node );
			}
			if ( node->returnData ().finallyBlock )
			{
				bufferPrintf ( &buff, "|<f%x> return", node->returnData ().finallyBlock );
			}
			break;
		case astOp::btInlineRet:
			emitNode ( node, buff );
			if ( node->returnData().node )
			{
				bufferPrintf ( &buff, "|<f%x> inlineRet", node->returnData().node );
			}
			if ( node->returnData().finallyBlock )
			{
				bufferPrintf ( &buff, "|<f%x> inlineRet", node->returnData().finallyBlock );
			}
			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			emitNode ( node, buff );
			arrIndex = 1;
			for ( auto it = node->arrayData().nodes.begin(); it != node->arrayData().nodes.end(); it++ )
			{
				bufferPrintf ( &buff, "|<f%x> %d", (*it), arrIndex++ );
			}
			break;
		case astOp::funcCall:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> Func", node->left );
			arrIndex = 1;
			for ( auto it = node->pList().param.begin(); it != node->pList().param.end(); it++ )
			{
				bufferPrintf ( &buff, "|<f%x> %d", (*it), arrIndex++ );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> array", node->left );
			arrIndex = 1;
			for ( auto it = node->pList().param.begin(); it != node->pList().param.end(); it++ )
			{
				bufferPrintf ( &buff, "|<f%x> %d", (*it), arrIndex );
			}
			break;
		case astOp::conditional:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> condition", node->left );
			bufferPrintf ( &buff, "|<f%x> true", node->conditionData().trueCond );
			bufferPrintf ( &buff, "|<f%x> false", node->conditionData().falseCond );
			break;
		case astOp::intValue:
			emitNode ( node, buff, node->intValue() );
			break;
		case astOp::doubleValue:
			emitNode ( node, buff, node->doubleValue() );
			break;
		case astOp::btLabel:
			emitNode ( node, buff, node->label() );
			break;
		case astOp::codeBlockValue:
		case astOp::nullValue:
		case astOp::funcValue:
		case astOp::nilValue:
			emitNode ( node, buff, "polygon", 6 );
			break;
		case astOp::pairValue:
			emitNode ( node, buff );
			bufferPrintf ( &buff, "|<f%x> Name", node->left );
			bufferPrintf ( &buff, "|<f%x> Value", node->right );
			break;
		case astOp::fieldValue:
			emitNode ( node, buff, "diamond", 4 );
			break;
		case astOp::codeBlockExpression:
		case astOp::stringValue:
		case astOp::btAnnotation:
		case astOp::singleLineComment:
		case astOp::commentBlock:
		case astOp::metadata:
			emitNode ( node, buff, node->stringValue () );
			break;
		case astOp::atomValue:
		case astOp::symbolValue:
			emitNode ( node, buff, node->symbolValue() );
			break;
		case astOp::nameValue:
			emitNode ( node, buff, node->nameValue () );
			break;
		default:
			emitNode ( node, buff, "circle", 1 );
			break;
	}
	bufferPrintf ( &buff, "\"]\r\n" );
	return node;
}

static astNode *makeDotGraphCB ( astNode *node, astNode *parent, symbolStack *sym, bool isInFunctionCall, bool isAccess, BUFFER &buff )
{
	switch ( node->getOp() )
	{
		case astOp::symbolDef:
			switch ( node->symbolDef()->symType )
			{
				case symbolSpaceType::symTypeStatic:
				case symbolSpaceType::symTypeLocal:
					if ( node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName ( ) )->getOp() == astOp::assign )
					{
						bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName ( ) ), node->symbolDef()->getInitializer ( node->symbolDef()->getSymbolName ( ) ) );
					}
					break;
				case symbolSpaceType::symTypeParam:
					{
						auto symbol = dynamic_cast<symbolSpaceParams *>(node->symbolDef());
						for ( uint32_t it = 0; it < symbol->size (); it++ )
						{
							if ( (*symbol)[it]->initializer->getOp () == astOp::assign )
							{
								bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*symbol)[it]->initializer, (*symbol)[it]->initializer );
							}
						}
					}
					break;
				default:
					break;
			}
			break;
		case astOp::btBasic:
			for (auto it = node->basicData().blocks.begin(); it != node->basicData().blocks.end(); it++ )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it), (*it) );
			}
			break;
		case astOp::btSwitch:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->switchData().condition, node->switchData().condition );
			for ( auto it = node->switchData().caseList.begin(); it != node->switchData().caseList.end(); it++ )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it)->condition, (*it)->condition );
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it)->block, (*it)->block );
			}
			break;
		case astOp::btIf:
			for ( auto it = node->ifData().ifList.begin(); it != node->ifData().ifList.end(); it++ )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it)->condition, (*it)->condition );
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it)->block, (*it)->block );
			}
			if ( node->ifData().elseBlock )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->ifData().elseBlock, node->ifData().elseBlock );
			}
			break;
		case astOp::btInlineRet:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->node(), node->node() );
			break;
		case astOp::btInline:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->inlineFunc().node, node->inlineFunc().node );
			break;
		case astOp::btFor:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->loopData().initializers, node->loopData().initializers );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->loopData().condition, node->loopData().condition );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->loopData().increase, node->loopData().increase );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->loopData().block, node->loopData().block );
			break;
		case astOp::btForEach:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->forEachData().in, node->forEachData().in );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->forEachData().statement, node->forEachData().statement );
			break;
		case astOp::btWhile:
		case astOp::btRepeat:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->loopData().condition, node->loopData().condition );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->loopData().block, node->loopData().block );
			break;
		case astOp::btTry:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->tryCatchData().tryBlock, node->tryCatchData().tryBlock );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->tryCatchData().catchBlock, node->tryCatchData().catchBlock );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->tryCatchData().finallyBlock, node->tryCatchData().finallyBlock );
			break;
		case astOp::btReturn:
			if ( node->returnData().node )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->returnData().node, node->returnData().node );
			}
			if ( node->returnData().finallyBlock )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->returnData().finallyBlock, node->returnData().finallyBlock );
			}

			break;
		case astOp::varrayValue:
		case astOp::arrayValue:
		case astOp::symbolPack:
			for ( auto it = node->arrayData().nodes.begin(); it != node->arrayData().nodes.end(); it++ )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it), (*it) );
			}
			break;
		case astOp::funcCall:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->left, node->left );
			for ( auto it = node->pList().param.begin(); it != node->pList().param.end(); it++ )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it), (*it) );
			}
			break;
		case astOp::arrayDeref:
		case astOp::cArrayDeref:
			for ( auto it = node->pList().param.begin(); it != node->pList().param.end(); it++ )
			{
				bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, (*it), (*it) );
			}
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->left, node->left );
			break;
		case astOp::conditional:
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->left, node->left );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->conditionData().trueCond, node->conditionData().trueCond );
			bufferPrintf ( &buff, "\"N%x\":<f%x> -> \"N%x\"\r\n", node, node->conditionData().falseCond, node->conditionData().falseCond );
			break;
		default:
			if ( node->left )
			{
				bufferPrintf ( &buff, "\"N%x\" -> \"N%x\" [color=\"green\"]\r\n", node, node->left, opNames[static_cast<size_t>(node->getOp())] );
			}
			if ( node->right )
			{
				bufferPrintf ( &buff, "\"N%x\" -> \"N%x\" [color=\"blue\"]\r\n", node, node->right, opNames[static_cast<size_t>(node->getOp())] );
			}
			break;
	}
	return node;
}


void astNode::makeDotNodeList  ( symbolStack *sym, BUFFER *buff )
{
	astNodeWalk ( this, sym, makeDotNodesCB, *buff );
}


void astNode::makeDotGraph ( symbolStack *sym, BUFFER *buff )
{
	astNodeWalk ( this, sym, makeDotGraphCB, *buff );
}
