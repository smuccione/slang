// array variant operations

#include "stdlib.h"

#include "bcInterpreter.h"
#include "op_variant.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcVM/exeStore.h"
#include "bcVM/fglTypes.h"
#include "bcVM/vmAtom.h"
#include "bcVM/vmInstance.h"
#include "bcVM/bcVM.h"
#include "bcVM/vmNativeInterface.h"

void op_arrDeref ( class vmInstance *instance, fglOp *ops )
{ 
	VAR		*arr;
	VAR		*value;
	VAR		*elem;
	VAR		*elemPrev;
	GRIP	g1 ( instance );

	arr		= instance->stack - 2;
	value	= instance->stack - 1;

	while ( TYPE ( arr ) == slangType::eREFERENCE )
	{
		arr = arr->dat.ref.v;
	}

	while ( TYPE ( value ) == slangType::eREFERENCE )
	{
		value = value->dat.ref.v;
	}

	switch ( TYPE ( arr ) )
	{
		case slangType::eSTRING:
			if ( TYPE ( value ) != slangType::eLONG )
			{
				throw errorNum::scILLEGAL_OPERAND;
			} else
			{
				if ( (value->dat.l > 0) && (value->dat.l <= (int)arr->dat.str.len) )
				{
					*(instance->stack - 2) = VAR_STR ( instance, arr->dat.str.c + value->dat.l - 1, 1 );
					instance->stack--;
					return;
				}
				*(instance->stack - 2) = VAR_NULL ( );
				instance->stack--;
				return;
			}
			break;

		case slangType::eARRAY_ROOT:
			if ( TYPE ( value ) != slangType::eLONG )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			arr = arr->dat.ref.v;
			switch ( arr->type )
			{
				case slangType::eARRAY_FIXED:
					// fast and easy access
					if ( (value->dat.l < arr->dat.arrayFixed.startIndex) || (value->dat.l > arr->dat.arrayFixed.endIndex) )
					{
						// TODO... this is for compatability, but it REALLY sucks!!!
						*(instance->stack - 2) = VAR_NULL();
						instance->stack--;
						return;
						throw errorNum::scFIXED_ARRAY_OUT_OF_BOUNDS;
					} else
					{
						*(instance->stack-2) = arr[value->dat.l - arr->dat.arrayFixed.startIndex + 1];
						instance->stack--;
						return;
					}
					break;
				case slangType::eARRAY_SPARSE:
					if ( !arr->dat.aSparse.v )
					{
						elem = allocNewElem ( instance, value->dat.l );

						arr->dat.aSparse.v = elem;
						arr->dat.aSparse.lastAccess = elem;
						arr->dat.aSparse.maxI = value->dat.l;

						*(instance->stack-2) = *elem->dat.aElem.var;
						instance->stack--;
						return;
					}

					// temporal locality... we put this here first as we assume that we will access the same array index more than once in a row
					if ( arr->dat.aSparse.lastAccess->dat.aElem.elemNum == value->dat.l )
					{
						*(instance->stack-2) = *arr->dat.aSparse.lastAccess->dat.aElem.var;
						instance->stack--;
						return;
					}
					// see if it's the NEXT one in the list, optimize for sequential access
					if ( arr->dat.aSparse.lastAccess->dat.aElem.next && arr->dat.aSparse.lastAccess->dat.aElem.next->dat.aElem.elemNum == value->dat.l )
					{
						*(instance->stack - 2) = *arr->dat.aSparse.lastAccess->dat.aElem.next->dat.aElem.var;
						arr->dat.aSparse.lastAccess = arr->dat.aSparse.lastAccess->dat.aElem.next;
						instance->stack--;
						return;
					}

					// additional temporal locality... optimize for sequential SPARSE access to the array
					if ( value->dat.l > arr->dat.aSparse.lastAccess->dat.aElem.elemNum )
					{
						elemPrev = arr->dat.aSparse.lastAccess;
						elem = elemPrev->dat.aElem.next;

						while ( elem && (elem->dat.aElem.elemNum < value->dat.l) )
						{
							elemPrev = elem;
							elem = elem->dat.aElem.next;
						}

						if ( elem && (elem->dat.aElem.elemNum == value->dat.l) )
						{
							*(instance->stack - 2) = *elem->dat.aElem.var;
							arr->dat.aSparse.lastAccess = elem;
							instance->stack--;
							return;
						}

						elem = allocNewElem ( instance, value->dat.l );

						elem->dat.aElem.next = elemPrev->dat.aElem.next;
						elemPrev->dat.aElem.next = elem;
						arr->dat.aSparse.lastAccess = elem;

						*(instance->stack - 2) = *elem->dat.aElem.var;
						instance->stack--;
						return;
					}

					// done with temporal optimizations... now see if new index preceeds the first current index
					if ( arr->dat.aSparse.v->dat.aElem.elemNum > value->dat.l )
					{
						// index is BEFORE the first entry
						elem = allocNewElem ( instance, value->dat.l );

						elem->dat.aElem.next = arr->dat.aSparse.v;
						arr->dat.aSparse.v = elem;
						arr->dat.aSparse.lastAccess = elem;

						*(instance->stack-2) = *elem->dat.aElem.var;
						instance->stack--;
						return;
					}

					// element is NOT after us, and is not less than the first element so it must be between us... need to start searching from first element on
					elem = arr->dat.aSparse.v;
						
					while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < value->dat.l) )
					{
						elem = elem->dat.aElem.next;
					}

					if ( elem->dat.aElem.elemNum == value->dat.l )
					{
						*(instance->stack-2) = *elem->dat.aElem.var;
						arr->dat.aSparse.lastAccess = elem;
						instance->stack--;
						return;
					}
					if ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum == value->dat.l) )
					{
						*(instance->stack-2) = *elem->dat.aElem.next->dat.aElem.var;
						arr->dat.aSparse.lastAccess = elem->dat.aElem.next;
						instance->stack--;
						return;
					} else
					{
						elem = allocNewElem ( instance, value->dat.l );

						elem->dat.aElem.next		= arr->dat.aSparse.v;
						arr->dat.aSparse.v			= elem;
						arr->dat.aSparse.maxI		= value->dat.l;
						arr->dat.aSparse.lastAccess	= elem;

						*(instance->stack-2) = *elem->dat.aElem.var;
						instance->stack--;
						return;
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			if ( arr->dat.ref.v->type == slangType::eNULL )
			{
				throw errorNum::scSEND_TO_NONOBJECT;
			}

			callerClass = arr->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovArrayDerefRHS)])
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovArrayDerefRHS)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( arr, &arr->dat.ref.v[arr->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( arr->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( arr, &arr->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}

			switch (  callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack--;
			while ( instance->result.type == slangType::eREFERENCE ) instance->result = *(instance->result.dat.ref.v);
			*(instance->stack-2) = instance->result;
			break;
		case slangType::eNULL:
			{
				if ( TYPE( value ) != slangType::eLONG )
				{
					throw errorNum::scILLEGAL_OPERAND;
				}
				elem = instance->om->allocVar( 3 );

				arr->type = slangType::eARRAY_ROOT;
				arr->dat.ref.v = &elem[0];
				arr->dat.ref.obj = arr->dat.ref.v;

				arr->dat.ref.v->type = slangType::eARRAY_SPARSE;
				arr->dat.ref.v->dat.aSparse.lastAccess = &elem[1];
				arr->dat.ref.v->dat.aSparse.v = &elem[1];
				arr->dat.ref.v->dat.aSparse.maxI = value->dat.l;

				arr->dat.ref.v->dat.aSparse.v->type = slangType::eARRAY_ELEM;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.elemNum = value->dat.l;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.next = 0;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.var = &elem[2];

				arr->dat.ref.v->dat.aSparse.v->dat.aElem.var->type = slangType::eNULL;

				*(instance->stack - 2) = VAR_REF ( arr, &elem[2] );
			}
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_arrDerefRef ( class vmInstance *instance, fglOp *ops )
{ 
	VAR		*arr;
	VAR		*value;
	VAR		*elem;
	VAR		*elemPrev;
	GRIP	 g1 ( instance );			// TODO: put old-style grip back into place so we don't lock the garbage collector during non-ffi operations

	arr		= instance->stack - 2;
	value	= instance->stack - 1;

	while ( TYPE ( arr ) == slangType::eREFERENCE )
	{
		arr = arr->dat.ref.v;
	}

	while ( TYPE ( value ) == slangType::eREFERENCE )
	{
		value = value->dat.ref.v;
	}

	switch ( TYPE ( arr ) )
	{
		case slangType::eARRAY_ROOT:
			if ( TYPE ( value ) != slangType::eLONG )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			arr = arr->dat.ref.v;
			switch ( arr->type )
			{
				case slangType::eARRAY_FIXED:
					// fast and easy access
					if ( (value->dat.l < arr->dat.arrayFixed.startIndex) || (value->dat.l > arr->dat.arrayFixed.endIndex) )
					{
						throw errorNum::scFIXED_ARRAY_OUT_OF_BOUNDS;
					} else
					{
						auto val = value->dat.l - arr->dat.arrayFixed.startIndex;
						*(instance->stack - 2) = VAR_REF ( arr, &arr[val + 1] );
						instance->stack--;
						return;
					}
					break;
				case slangType::eARRAY_SPARSE:
					if ( !arr->dat.aSparse.v )
					{
						elem = allocNewElem ( instance, value->dat.l );

						arr->dat.aSparse.v = elem;
						arr->dat.aSparse.lastAccess = elem;
						arr->dat.aSparse.maxI = value->dat.l;

						*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
						instance->stack--;
						return;
					}

					// temporal locality... we put this here first as we assume that we will access the same array index more than once in a row
					if ( arr->dat.aSparse.lastAccess->dat.aElem.elemNum == value->dat.l )
					{
						*(instance->stack - 2) = VAR_REF ( arr, arr->dat.aSparse.lastAccess->dat.aElem.var );
						instance->stack--;
						return;
					}
					// see if it's the NEXT one in the list, optimize for sequential access
					if ( arr->dat.aSparse.lastAccess->dat.aElem.next && arr->dat.aSparse.lastAccess->dat.aElem.next->dat.aElem.elemNum == value->dat.l )
					{
						*(instance->stack - 2) = VAR_REF ( arr, arr->dat.aSparse.lastAccess->dat.aElem.next->dat.aElem.var );
						arr->dat.aSparse.lastAccess = arr->dat.aSparse.lastAccess->dat.aElem.next;
						instance->stack--;
						return;
					}

					// additional temporal locality... optimize for sequential SPARSE access to the array
					if ( value->dat.l > arr->dat.aSparse.lastAccess->dat.aElem.elemNum )
					{
						elemPrev = arr->dat.aSparse.lastAccess;
						elem = elemPrev->dat.aElem.next;

						while ( elem && (elem->dat.aElem.elemNum < value->dat.l) )
						{
							elemPrev = elem;
							elem = elem->dat.aElem.next;
						}

						if ( elem && (elem->dat.aElem.elemNum == value->dat.l) )
						{
							*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
							arr->dat.aSparse.lastAccess = elem;
							instance->stack--;
							return;
						}
						elem = allocNewElem ( instance, value->dat.l );

						elem->dat.aElem.next = elemPrev->dat.aElem.next;
						elemPrev->dat.aElem.next = elem;
						arr->dat.aSparse.lastAccess = elem;
						arr->dat.aSparse.maxI = value->dat.l;

						*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
						instance->stack--;
						return;
					}

					// done with temporal optimizations... now see if new index preceeds the first current index
					if ( arr->dat.aSparse.v->dat.aElem.elemNum > value->dat.l )
					{
						// index is BEFORE the first entry
						elem = allocNewElem ( instance, value->dat.l );

						elem->dat.aElem.next = arr->dat.aSparse.v;
						arr->dat.aSparse.v = elem;
						arr->dat.aSparse.lastAccess = elem;

						*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
						instance->stack--;
						return;
					}

					elem = arr->dat.aSparse.v;
						
					while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < value->dat.l) )
					{
						elem = elem->dat.aElem.next;
					}

					if ( elem->dat.aElem.elemNum == value->dat.l )
					{
						*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
						arr->dat.aSparse.lastAccess = elem;
						instance->stack--;
						return;
					}
					if ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum == value->dat.l) )
					{
						*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
						arr->dat.aSparse.lastAccess = elem->dat.aElem.next;
						instance->stack--;
						return;
					} else
					{
						elem = allocNewElem ( instance, value->dat.l );

						elem->dat.aElem.next		= arr->dat.aSparse.v;
						arr->dat.aSparse.v			= elem;
						arr->dat.aSparse.maxI		= value->dat.l;
						arr->dat.aSparse.lastAccess	= elem;

						*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
						instance->stack--;
						return;
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			callerClass = arr->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovArrayDerefRHS)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovArrayDerefRHS)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack++) = VAR_OBJ ( arr, &arr->dat.ref.v[arr->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( arr->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack++) = VAR_OBJ ( arr, &arr->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}

			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			instance->stack--;
			*(instance->stack-2) = instance->result;

			switch ( TYPE ( &instance->result ) )
			{
				case slangType::eREFERENCE:
				case slangType::eOBJECT_ROOT:
				case slangType::eARRAY_ROOT:
					break;
				default:
					throw errorNum::scILLEGAL_ASSIGNMENT;
			}
			break;
		case slangType::eNULL:
			{
				if ( TYPE( value ) != slangType::eLONG )
				{
					throw errorNum::scILLEGAL_OPERAND;
				}
				elem = (VAR *)instance->om->allocVar( 3 );

				arr->type = slangType::eARRAY_ROOT;
				arr->dat.ref.v = &elem[0];
				arr->dat.ref.obj = &elem[0];

				arr->dat.ref.v->type = slangType::eARRAY_SPARSE;
				arr->dat.ref.v->dat.aSparse.lastAccess = &elem[1];
				arr->dat.ref.v->dat.aSparse.v = &elem[1];
				arr->dat.ref.v->dat.aSparse.maxI = value->dat.l;

				arr->dat.ref.v->dat.aSparse.v->type = slangType::eARRAY_ELEM;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.elemNum = value->dat.l;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.next = 0;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.var = &elem[2];

				arr->dat.ref.v->dat.aSparse.v->dat.aElem.var->type = slangType::eNULL;

				*(instance->stack - 2) = VAR_REF ( arr, arr->dat.ref.v->dat.aSparse.v->dat.aElem.var );
			}
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_arrStore ( class vmInstance *instance, fglOp *ops )
{ 
	VAR		*arr;
	VAR		*value;
	VAR		*index;
	VAR		*elem;
	VAR		*elemPrev;
	GRIP	 g1 ( instance );

	arr		= instance->stack - 1;
	index   = instance->stack - 2;
	value	= instance->stack - 3;

	while ( TYPE ( arr ) == slangType::eREFERENCE )
	{
		arr = arr->dat.ref.v;
	}

	while ( TYPE ( value ) == slangType::eREFERENCE )
	{
		value = value->dat.ref.v;
	}

	while ( TYPE ( index ) == slangType::eREFERENCE )
	{
		index = index->dat.ref.v;
	}

	switch ( TYPE ( arr ) )
	{
		case slangType::eARRAY_ROOT:
processArray:
			if ( TYPE ( index ) != slangType::eLONG )
			{
				throw errorNum::scILLEGAL_OPERAND;
			}
			arr = arr->dat.ref.v;
			switch ( arr->type )
			{
				case slangType::eARRAY_FIXED:
					// fast and easy access
					if ( (index->dat.l < arr->dat.arrayFixed.startIndex) || (index->dat.l > arr->dat.arrayFixed.endIndex) )
					{
						throw errorNum::scFIXED_ARRAY_OUT_OF_BOUNDS;
					} else
					{
						arr[index->dat.l - arr->dat.arrayFixed.startIndex + 1] = *value;
						instance->stack -= 2;
						return;
					}
					break;
				case slangType::eARRAY_SPARSE:
					if ( !arr->dat.aSparse.v )
					{
						elem = allocNewElem ( instance, index->dat.l );

						arr->dat.aSparse.v = elem;
						arr->dat.aSparse.lastAccess = elem;
						arr->dat.aSparse.maxI = index->dat.l;

						*elem->dat.aElem.var = *value;
						instance->stack -= 2;
						return;
					}

					// temporal locality... we put this here first as we assume that we will access the same array index more than once in a row
					if ( arr->dat.aSparse.lastAccess->dat.aElem.elemNum == index->dat.l )
					{
						*arr->dat.aSparse.lastAccess->dat.aElem.var = *value;
						instance->stack -= 2;
						return;
					} else
					{
						// done with temporal optimizations... now see if new index preceeds the first current index
						if ( arr->dat.aSparse.v->dat.aElem.elemNum > index->dat.l )
						{
							// index is BEFORE the first entry
							elem = allocNewElem ( instance, index->dat.l );

							elem->dat.aElem.next = arr->dat.aSparse.v;
							arr->dat.aSparse.v = elem;
							arr->dat.aSparse.lastAccess = elem;

							*elem->dat.aElem.var = *value;
							instance->stack -= 2;
							return;
						}

						// additional temporal locality... optimize for sequential access to the array
						if ( index->dat.l > arr->dat.aSparse.lastAccess->dat.aElem.elemNum )
						{
							elemPrev = arr->dat.aSparse.lastAccess;
							elem = elemPrev->dat.aElem.next; 

							while ( elem && (elem->dat.aElem.elemNum < index->dat.l) )
							{
								elemPrev = elem;
								elem = elem->dat.aElem.next;
							}

							if ( elem && (elem->dat.aElem.elemNum == index->dat.l) )
							{
								*(instance->stack - 2) = VAR_REF ( arr, elem->dat.aElem.var );
								arr->dat.aSparse.lastAccess = elem;
								instance->stack -= 2;
								return;
							}
							elem = allocNewElem ( instance, index->dat.l );

							elem->dat.aElem.next = elemPrev->dat.aElem.next;
							elemPrev->dat.aElem.next = elem;
							arr->dat.aSparse.lastAccess = elem;
							arr->dat.aSparse.maxI = index->dat.l;

							*elem->dat.aElem.var = *value;
							instance->stack -= 2;
							return;
						}

						if ( arr->dat.aSparse.lastAccess && arr->dat.aSparse.lastAccess->dat.aElem.elemNum <= index->dat.l )
						{
							elem = arr->dat.aSparse.lastAccess;
						} else
						{
							elem = arr->dat.aSparse.v;
						}
						
						while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < index->dat.l) )
						{
							elem = elem->dat.aElem.next;
						}

						if ( elem->dat.aElem.elemNum == index->dat.l )
						{
							*elem->dat.aElem.var = *value;
							arr->dat.aSparse.lastAccess = elem;
							instance->stack -= 2;
							return;
						}
						if ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum == index->dat.l) )
						{
							*elem->dat.aElem.next->dat.aElem.var = *value;
							arr->dat.aSparse.lastAccess = elem->dat.aElem.next;
							instance->stack -= 2;
							return;
						} else
						{
							elem = allocNewElem ( instance, index->dat.l );

							elem->dat.aElem.next		= arr->dat.aSparse.v;
							arr->dat.aSparse.v			= elem;
							arr->dat.aSparse.maxI		= index->dat.l;
							arr->dat.aSparse.lastAccess	= elem;

							*elem = *value;
							instance->stack -= 2;
							return;
						}
					}
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case slangType::eOBJECT_ROOT:
			// operator overload dispatch
			bcFuncDef				 *callerFDef;
			bcClass					 *callerClass;
			bcClassSearchEntry		 *cEntry;

			callerClass = arr->dat.ref.v->dat.obj.classDef;

			if ( !callerClass->ops[int(fgxOvOp::ovArrayDerefLHS)] )
			{
				throw errorNum::scINVALID_OPERATOR;
			}

			cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovArrayDerefLHS)] - 1];

			if ( cEntry->isVirtual )
			{
				*(instance->stack - 1) = VAR_OBJ ( arr, &arr->dat.ref.v[arr->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
				callerFDef = instance->atomTable->atomGetFunc ( arr->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom );
			} else
			{
				*(instance->stack - 1) = VAR_OBJ ( arr, &arr->dat.ref.v[cEntry->methodAccess.objectStart] );
				callerFDef = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
			}

			switch ( callerFDef->conv )
			{
				case fgxFuncCallConvention::opFuncType_Bytecode:
					instance->interpretBC ( callerFDef, ops + 1, callerFDef->nParams );
					break;
				case fgxFuncCallConvention::opFuncType_cDecl:
					instance->funcCall ( callerFDef, callerFDef->nParams );
					break;
				default:
					throw errorNum::scINTERNAL;
			}
			break;
		case slangType::eNULL:
			{
				if ( TYPE( index ) != slangType::eLONG )
				{
					throw errorNum::scILLEGAL_OPERAND;
				}
				elem = (VAR *)instance->om->allocVar ( 3 );

				arr->type = slangType::eARRAY_ROOT;
				arr->dat.ref.v = &elem[0];
				arr->dat.ref.obj = &elem[0];

				arr->dat.ref.v->type = slangType::eARRAY_SPARSE;
				arr->dat.ref.v->dat.aSparse.lastAccess = &elem[1];
				arr->dat.ref.v->dat.aSparse.v = &elem[1];
				arr->dat.ref.v->dat.aSparse.maxI = 0;

				arr->dat.ref.v->dat.aSparse.v->type = slangType::eARRAY_ELEM;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.elemNum = index->dat.l;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.next = 0;
				arr->dat.ref.v->dat.aSparse.v->dat.aElem.var = &elem[2];

				arr->dat.ref.v->dat.aSparse.v->dat.aElem.var->type = slangType::eNULL;

				goto processArray;
			}
			break;
		default:
			throw errorNum::scILLEGAL_OPERAND;
	}
}

void op_vArrayNew ( class vmInstance *instance, int64_t num )
{
	VAR			*val;
	VAR			*array;
	VAR			*elem;
	GRIP		 g1 ( instance );

	val = instance->stack - num;		// start of array values

	array = instance->om->allocVar ( 1 + num * 2 /* aSparce + (aElem + value) * 2 */ );
	elem = &array[1];

	array->type = slangType::eARRAY_SPARSE;

	if ( num )
	{
		array->dat.aSparse.v = elem;
		array->dat.aSparse.lastAccess = elem;
		array->dat.aSparse.maxI = num;
	} else
	{
		array->dat.aSparse.v = 0;
		array->dat.aSparse.lastAccess = 0;
		array->dat.aSparse.maxI = 0;
	}

	for ( int64_t ctr = 1; ctr <= num;val++, ctr++ )
	{
		elem->type = slangType::eARRAY_ELEM;
		elem->dat.aElem.var = &elem[1];
		elem->dat.aElem.elemNum = ctr;
		if ( ctr != num  )
		{
			elem->dat.aElem.next = &elem[2];
		} else
		{
			elem->dat.aElem.next = 0;
		}

		elem[1] = *val;
		elem += 2;
	}
	instance->stack -= num;
	instance->stack->type = slangType::eARRAY_ROOT;
	instance->stack->dat.ref.v = array;
	instance->stack->dat.ref.obj = array;
	instance->stack++;
}

