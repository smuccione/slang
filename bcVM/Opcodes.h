
/*

	fgl interperter and IL opcodes

*/

#pragma once

#include <stdint.h>

#include "opCodeDefinitions.h"

#define DEF_OP(name) name,
enum class fglOpcodes {
	SLANG_OPS
};
#undef DEF_OP

struct fglOp
{
	fglOpcodes		op;
	union {
		int64_t					 intValue;
		double					 doubleValue;
		uint32_t				 index;
		struct	{
			uint32_t			 val1;
			uint32_t			 val2;
		}			dual;
		struct
		{
			int32_t	 			 stackIndex;
			int16_t				 objectIndex;		// this can be either an offset from self into the object ivars or an vtable which will hold the offset
			uint16_t			 nParams;
		} objOp;
		void					*bpInformation;
	} imm;
};

extern bool opCanDoCall ( fglOpcodes op );