#pragma once

#include "vmInstance.h"

// dispatches against a function object or other function type.   this is NOT used to call methods.
VAR *VAR::operator() ( uint32_t passedParams, class vmInstance *instance )
{
	auto obj = this;

	bcFuncDef *func;
	VAR *stackSave;
	VAR *stack;
	bcClassSearchEntry *cEntry = nullptr;
	bcClass *callerClass = nullptr;

	stackSave = stack = instance->stack;

	switch ( type )
	{
		case slangType::eATOM:
			func = instance->atomTable->atomGetFunc ( obj->dat.atom );
			break;
		case slangType::eSTRING:
			func = instance->atomTable->atomGetFunc ( obj->dat.str.c );
			break;
		case slangType::eOBJECT_ROOT:
			{
				if ( !obj->dat.ref.v )
				{
					throw errorNum::scNOT_A_FUNCTION;
				}
				callerClass = obj->dat.ref.v->dat.obj.classDef;

				if ( !callerClass->ops[int(fgxOvOp::ovFuncCall)] )
				{
					throw errorNum::scINVALID_OPERATOR;
				}
				cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

				if ( cEntry->isVirtual )
				{
					func = instance->atomTable->atomGetFunc ( obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
				} else
				{
					func = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
				}
			}
			break;
		default:
			throw errorNum::scNOT_A_FUNCTION;
	}

	auto nParams = passedParams;
	if ( func->nParams > passedParams )
	{
		// if we don't have enough parameters push nulls
		for ( ; nParams < func->nParams - func->isMethod ? 1 : 0; nParams++ )
		{
			auto pIndex = func->nParams - (nParams - passedParams) - 1;
			if ( func->defaultParams && func->defaultParams[pIndex] )
			{
				instance->interpretBCParam ( func, func, func->loadImage->csBase + func->defaultParams[pIndex], 0, instance->stack, instance->stack );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}

		memcpy ( instance->stack, stackSave - passedParams, passedParams );

		nParams = func->nParams;
	}

	if ( func->isMethod )
	{
		if ( cEntry->isVirtual )
		{
			// fixup object to point to the virtual context
			*(instance->stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
		} else
		{
			*(instance->stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->methodAccess.objectStart] );
		}
	}

	switch ( func->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( func, func->cs, nParams );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( func, nParams );
			break;
	}
	instance->stack = stackSave;
	return &instance->result;
}


// dispatches against a function object or other function type.   this is NOT used to call methods.
template<class ... Args>
VAR *VAR::operator() ( class vmInstance *instance, Args && ...args )
{
	auto obj = this;

	bcFuncDef *func;
	VAR *stackSave;
	bcClassSearchEntry *cEntry = nullptr;
	bcClass *callerClass = nullptr;

	stackSave = instance->stack;

	switch ( type )
	{
		case slangType::eATOM:
			func = instance->atomTable->atomGetFunc ( dat.atom );
			break;
		case slangType::eSTRING:
			func = instance->atomTable->atomGetFunc ( dat.str.c );
			break;
		case slangType::eOBJECT_ROOT:
			{
				if ( !obj->dat.ref.v )
				{
					throw errorNum::scNOT_A_FUNCTION;
				}

				callerClass = obj->dat.ref.v->dat.obj.classDef;

				if ( !callerClass->ops[int(fgxOvOp::ovFuncCall)] )
				{
					throw errorNum::scINVALID_OPERATOR;
				}
				cEntry = &callerClass->elements[callerClass->ops[int(fgxOvOp::ovFuncCall)] - 1];

				if ( cEntry->isVirtual )
				{
					func = instance->atomTable->atomGetFunc ( obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].atom ); \
				} else
				{
					func = instance->atomTable->atomGetFunc ( cEntry->methodAccess.atom );
				}
			}
			break;
		default:
			throw errorNum::scNOT_A_FUNCTION;
	}

	uint32_t nParams = sizeof...(Args);

	// if we don't have enough parameters push nulls
	for ( ; nParams < func->nParams; nParams++ )
	{
		auto pIndex = func->nParams - (nParams - sizeof...(Args)) - 1;
		if ( func->defaultParams && func->defaultParams[pIndex] )
		{
			instance->interpretBCParam ( func, func, func->loadImage->csBase + func->defaultParams[pIndex], 0, instance->stack, instance->stack );
		} else
		{
			(instance->stack++)->type = slangType::eNULL;
		}
	}

	// push our passed in arguments
	makeParams ( sizeof... (Args), types< Args... >{}, std::forward<Args> ( args )... );

	if ( func->isMethod )
	{
		if ( cEntry->isVirtual )
		{
			// fixup object to point to the virtual context
			*(instance->stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[obj->dat.ref.v->dat.obj.vPtr[cEntry->methodAccess.vTableEntry].delta] );
		} else
		{
			*(instance->stack++) = VAR_OBJ ( obj, &obj->dat.ref.v[cEntry->methodAccess.objectStart] );
		}
	}

	switch ( func->conv )
	{
		case fgxFuncCallConvention::opFuncType_Bytecode:
			instance->interpretBC ( func, func->cs, nParams );
			break;
		case fgxFuncCallConvention::opFuncType_cDecl:
			instance->funcCall ( func, nParams );
			break;
	}
	instance->stack = stackSave;
	return &instance->result;
}

template<class size, class Head, class ... Tail>
void VAR::makeParams ( class vmInstance *instance, size pCount, types< Head, Tail ...>, Head &&head, Tail && ...args )
{
	makeParams ( pCount, types<Tail...>{}, std::forward<Tail> ( args )... );
	*(instance->stack++) = vmInstance::toVar<Head>::convert ( instance, head );
}

template<class size, class Head >
void VAR::makeParams ( class vmInstance *instance, size pCount, types< Head>, Head &&head )
{
	*(instance->stack++) = vmInstance::toVar<Head>::convert ( instance, head );
}

template<class size>
void VAR::makeParams ( class vmInstance *instance, size pCount, types<> )
{}


