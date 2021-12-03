/*

	Byte Code Generator

*/

#include "compilerBCGenerator.h"

void compCodeSegment::enableProfiling ( void )
{
	genProfiling = true;
}
size_t compCodeSegment::size()
{
	return bufferSize( &buff );
}

uint32_t compCodeSegment::nextOp ( void )
{
	return (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
}

void compCodeSegment::serialize ( BUFFER *buff )
{
	buff->write ( bufferBuff ( &this->buff ), bufferSize ( &this->buff ) );
}

void compCodeSegment::peepHole ( fglOp op )
{
}

void compCodeSegment::emitLabel ( char *label )
{
	emitCode.top() = true;
}

void compCodeSegment::emitLabel ( cacheString const &label )
{
	emitCode.top ( ) = true;
}

void compCodeSegment::disableEmit ( void )
{
	emitCode.top() = false;
}

void compCodeSegment::enableEmit ( void )
{
	emitCode.top() = true;
}

void compCodeSegment::emitPush()
{
	emitCode.push ( emitCode.top() );
}
void compCodeSegment::emitPop()
{
	if ( !emitCode.size() ) throw errorNum::scINTERNAL;
	emitCode.pop ();
}

void compCodeSegment::emitProfileCallStart (  fglOpcodes op )
{
	switch ( op )
	{
		case fglOpcodes::callBCTail:
		case fglOpcodes::objBuild:
		case fglOpcodes::objConstruct:
		case fglOpcodes::objConstructV:
			break;
		default:
			if ( genProfiling && opCanDoCall ( op ) )
			{
				fglOp	*o;

				bufferMakeRoom ( &buff, sizeof ( fglOp ) );

				o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

				memset ( o, 0, sizeof ( fglOp ) );
				o->op = fglOpcodes::profCallStart;
				bufferAssume ( &buff, sizeof ( fglOp ) );
			}
			break;
	}
}

void compCodeSegment::emitProfileCallEnd (  fglOpcodes op )
{
	switch ( op )
	{
		case fglOpcodes::callBCTail:
		case fglOpcodes::objConstruct:
		case fglOpcodes::objConstructV:
			break;
		default:
			if ( genProfiling && opCanDoCall ( op ) )
			{
				fglOp	*o;

				bufferMakeRoom ( &buff, sizeof ( fglOp ) );

				o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

				memset ( o, 0, sizeof ( fglOp ) );
				o->op = fglOpcodes::profCallEnd;
				bufferAssume ( &buff, sizeof ( fglOp ) );
			}
			break;
	}
}

void compCodeSegment::putOp ( fglOpcodes op )
{
	fglOp	*o;

	if ( emitCode.top() )
	{
		emitProfileCallStart ( op );

		bufferMakeRoom ( &buff, sizeof ( fglOp ) );

		o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

		memset ( o, 0, sizeof ( fglOp ) );
		o->op = op;

		bufferAssume ( &buff, sizeof ( fglOp ) );

		emitProfileCallEnd ( op );
#ifdef _DEBUG
		nOps = (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
#endif
	}
}

uint32_t compCodeSegment::putOp ( fglOpcodes op, int64_t int64Value )
{
	fglOp	*o;
	uint32_t	 indexOffset;

	if ( emitCode.top() )
	{
		emitProfileCallStart ( op );

		bufferMakeRoom ( &buff, sizeof ( fglOp ) );

		o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

		memset ( o, 0, sizeof ( fglOp ) );
		o->op = op;
		o->imm.intValue = int64Value;

		indexOffset = (uint32_t) bufferSize ( &buff ) + offsetof ( fglOp, imm.intValue );

		bufferAssume ( &buff, sizeof ( fglOp ) );

		emitProfileCallEnd ( op );

#ifdef _DEBUG
		nOps = (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
#endif
		return ( indexOffset );
	} 
	return 0;
}

void compCodeSegment::putOp ( fglOpcodes op, double doubleValue )
{
	fglOp	*o;

	if ( emitCode.top() )
	{
		emitProfileCallStart ( op );

		bufferMakeRoom ( &buff, sizeof ( fglOp ) );

		o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

		memset ( o, 0, sizeof ( fglOp ) );
		o->op = op;
		o->imm.doubleValue = doubleValue;

		bufferAssume ( &buff, sizeof ( fglOp ) );

		emitProfileCallEnd ( op );
#ifdef _DEBUG
		nOps = (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
#endif
	} 
}

uint32_t compCodeSegment::putOp ( fglOpcodes op, uint32_t index )
{
	fglOp	*o;
	uint32_t	 indexOffset;

	if ( emitCode.top() )
	{
		emitProfileCallStart ( op );

		bufferMakeRoom ( &buff, sizeof ( fglOp ) );

		o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

		memset ( o, 0, sizeof ( fglOp ) );
		o->op = op;
		o->imm.index = index;

		indexOffset = (uint32_t)bufferSize ( &buff ) + offsetof ( fglOp, imm.index );

		bufferAssume ( &buff, sizeof ( fglOp ) );

		emitProfileCallEnd ( op );

#ifdef _DEBUG
		nOps = (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
#endif
		return ( indexOffset );
	}
	return 0;
}

uint32_t compCodeSegment::putOp ( fglOpcodes op, int32_t index )
{
	fglOp *o;
	uint32_t	 indexOffset;

	if ( emitCode.top () )
	{
		emitProfileCallStart ( op );

		bufferMakeRoom ( &buff, sizeof ( fglOp ) );

		o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

		memset ( o, 0, sizeof ( fglOp ) );
		o->op = op;
		o->imm.index = (uint32_t)index;

		indexOffset = (uint32_t)bufferSize ( &buff ) + offsetof ( fglOp, imm.index );

		bufferAssume ( &buff, sizeof ( fglOp ) );

		emitProfileCallEnd ( op );

#ifdef _DEBUG
		nOps = (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
#endif
		return (indexOffset);
	}
	return 0;
}

uint32_t compCodeSegment::putOp ( fglOpcodes op, uint32_t val1, uint32_t val2 )
{
	fglOp	*o;
	uint32_t	 indexOffset;

	if ( emitCode.top() )
	{
		emitProfileCallStart ( op );

		bufferMakeRoom ( &buff, sizeof ( fglOp ) );

		o = (fglOp *)(bufferBuff ( &buff ) + bufferSize ( &buff ));

		memset ( o, 0, sizeof ( fglOp ) );
		o->op = op;
		o->imm.dual.val1 = val1;
		o->imm.dual.val2 = val2;

		indexOffset = (uint32_t)bufferSize ( &buff )  + offsetof ( fglOp, imm.dual.val1 );

		bufferAssume ( &buff, sizeof ( fglOp ) );

		emitProfileCallEnd ( op );

#ifdef _DEBUG
		nOps = (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
#endif
		return indexOffset;
	}
	return 0;
}
 
uint32_t compCodeSegment::putOp ( fglOpcodes op, uint32_t stackIndex, int16_t objectIndex, uint16_t nParams )
{
	fglOp	*o;
	uint32_t	 indexOffset;

	if ( emitCode.top () )
	{
		emitProfileCallStart ( op );

		bufferMakeRoom ( &buff, sizeof ( fglOp ) );

		o = (fglOp *) (bufferBuff ( &buff ) + bufferSize ( &buff ));

		memset ( o, 0, sizeof ( fglOp ) );
		o->op = op;
		o->imm.objOp.stackIndex = (int32_t)stackIndex;
		o->imm.objOp.objectIndex = objectIndex;
		o->imm.objOp.nParams = nParams;

		indexOffset = (uint32_t) bufferSize ( &buff ) + offsetof ( fglOp, imm.dual.val1 );

		bufferAssume ( &buff, sizeof ( fglOp ) );

		emitProfileCallEnd ( op );

#ifdef _DEBUG
		nOps = (uint32_t)(bufferSize ( &buff ) / sizeof ( fglOp ));
#endif
		return indexOffset;
	}
	return 0;
}

void compCodeSegment::setdword ( uint32_t offset, uint32_t value )
{
	if ( offset )
	{
		*(uint32_t *)((char*)bufferBuff ( &buff ) + offset) = value;
	}
}

fglOp *compCodeSegment::getOps()
{
	return (fglOp *)bufferBuff( &buff );
}

uint32_t compCodeSegment::getNumOps()
{
	return (uint32_t)bufferSize( &buff ) / sizeof( fglOp );
}

uint32_t compCodeSegment::getLen()
{
	return (uint32_t)bufferSize( &buff );
}


