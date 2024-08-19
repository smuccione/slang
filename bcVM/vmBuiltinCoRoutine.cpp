/*
		IO Support functions

*/


#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"

static void cCoroutine ( vmInstance *instance, VAR_OBJ *obj, VAR *func, size_t evalStackSize, size_t cStackSize )
{
	auto coRoutine = obj->makeObj<vmCoroutine *> ( instance );

	*coRoutine = instance->createCoroutine ( func, evalStackSize, cStackSize );
}

void cCoroutineRelease ( class vmInstance *instance, VAR_OBJ *obj )
{
	auto coRoutine = obj->getObj<vmCoroutine *> ();

	for ( auto it = instance->coRoutines.begin(); it != instance->coRoutines.end(); it++ )
	{
		if ( (*it) == *coRoutine )
		{
			instance->coRoutines.erase ( it );
			break;
		}
	}

	delete coRoutine;
}

VAR cCoroutineYield ( vmInstance *instance, VAR_OBJ *obj, VAR *param, nParamType nParams )
{
	auto coRoutine = obj->getObj<vmCoroutine *> ();

	instance->coRoutineStack.push ( instance->currentCoroutine );

	(*coRoutine)->yield ( param , nParams + 1);

	return instance->result;
}

int cCoroutineIsReady ( vmInstance *instance, VAR_OBJ *obj )
{
	auto coRoutine = obj->getObj<vmCoroutine *> ();

	if ( (*coRoutine)->getStatus() != vmCoroutine::vmCoroutineStatus::vmCoroutine_complete )
	{
		return 1;
	} else
	{
		return 0;
	}
}

void yieldReturn ( vmInstance *instance, VAR *param )
{
	if ( instance->coRoutineStack.size() )
	{
		auto coRoutine = instance->coRoutineStack.top();
		instance->coRoutineStack.pop();

		if ( coRoutine->getStatus() == vmCoroutine::vmCoroutineStatus::vmCoroutine_complete )
		{
			throw errorNum::scCOROUTINE_CONTINUE;
		}

		coRoutine->yield ( param, 0 );
	} else
	{
		throw errorNum::scCOROUTINE_NORETURN;
	}
}

void yieldTo ( vmInstance *instance, VAR_OBJ *obj, VAR *param )
{
	if ( strccmp ( obj->dat.obj.classDef->name, "coRoutine" ) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	auto coRoutine = obj->getObj<vmCoroutine *> ();

	if ( (*coRoutine)->getStatus() == vmCoroutine::vmCoroutineStatus::vmCoroutine_complete )
	{
		throw errorNum::scCOROUTINE_CONTINUE;
	}

	(*coRoutine)->yield ( param, 0);
}

static vmInspectList *cCoroutineInspector ( class vmInstance *instance, bcFuncDef *func, VAR *obj, uint64_t start, uint64_t end )
{
	VAR				*stack;

	auto coRoutine = VAR_OBJ(obj).getObj<vmCoroutine *> ();

	auto vars = new vmInspectList();

	auto evalStack = (*coRoutine)->getStack ( &stack );

	while ( stack > evalStack )
	{
		if ( stack->type == slangType::ePCALL )
		{
			vars->entry.push_back ( static_cast<vmDebugVar *>(new vmDebugLocalVar ( instance, "", stack )) );
		}
		stack--;
	}

	return vars;
}

static VAR_OBJ_TYPE<"coRoutine"> cCoroutineGetEnumerator ( vmInstance *instance, VAR_OBJ *obj )
{
	return *obj;
}

static VAR cCoroutineGetCurrent ( vmInstance *instance, VAR_OBJ *obj )
{
	return *classIVarAccess ( obj, "$current" );
}

static uint32_t cCoroutineMoveNext ( vmInstance *instance, VAR_OBJ *obj )
{
	instance->coRoutineStack.push ( instance->currentCoroutine );

	auto coRoutine = obj->getObj<vmCoroutine *> ();

	(*coRoutine)->yield ( obj, 0 );

	// save off the returned value... this is the value of current.   it is not returned here but is returned when current is requested
	auto index = classIVarAccess ( obj, "$current" );
	*index = instance->result;

	if ( (*coRoutine)->getStatus() != vmCoroutine::vmCoroutineStatus::vmCoroutine_complete )
	{
		return 1;
	} else
	{
		return 0;
	}
}

void builtinCoroutineInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		CLASS ( "coRoutine" );
			INHERIT ( "Queryable" );
			METHOD( "new", cCoroutine, DEF( 3, "8192" ), DEF( 4, "0" ) );
			METHOD( "release", cCoroutineRelease );
			ACCESS( "isReady", cCoroutineIsReady ) CONST;
			OP ( "()", cCoroutineYield );

			// enumeration stuff
			METHOD( "getEnumerator", cCoroutineGetEnumerator ) CONST;
			METHOD( "moveNext", cCoroutineMoveNext );
			ACCESS( "current", cCoroutineGetCurrent ) CONST;
			PRIVATE
				IVAR ( "$current" );
			INSPECTORCB( cCoroutineInspector );
		END;
	
		FUNC( "yieldReturn", yieldReturn, DEF( 1, "null" ) );
		FUNC( "yieldTo", yieldTo, DEF( 2, "null" ) );
	END;
}
