// bcVM.h

#pragma once

#include "bcVM/vmInstance.h"

#define PUSH(val)		(*(instance->stack++) = (val))
#define POP(n)			(instance->stack -= n)

#define TYPE(n)			((n)->type)

static inline VAR *DEREF ( VAR *x ) { while ( TYPE ( x ) == slangType::eREFERENCE ) x = x->dat.ref.v; return x; }

extern VAR				 arrayFixed2				( class vmInstance *instance, int64_t start, int64_t end, VAR const &defaultValue );

extern int64_t			 _amaxi_					( VAR *var );
extern int64_t			 _amini_					( VAR *var );

extern VAR				*allocNewElem				( class vmInstance *instance, int64_t );
extern VAR				*_arrayGet					( class vmInstance *instance, VAR  *, int64_t, int64_t * );
extern bool				 _arrayIsElem				( class vmInstance *instance, VAR  *, int64_t, int64_t * );
extern VAR				 _anexti					( class vmInstance *instance, VAR *arr, VAR *indicie );
extern VAR_STR			 _arrayToNameValue			( class vmInstance *instance, VAR *arr, char const *sep1, char const *sep2 );

extern VAR_OBJ			*init_obj ( vmInstance *instance, bcClass *callerClass );

template < typename... T>
auto &arrayGet ( class vmInstance *instance, VAR *arr, T... t)
{
	int64_t index[] = { t..., };
	return *_arrayGet ( instance, arr, sizeof... ( t ), index );
}

template < typename... T>
auto &arrayGet ( class vmInstance *instance, VAR &arr, T... t)
{
	int64_t index[] = { t..., };
	return *_arrayGet ( instance, &arr, sizeof... ( t ), index );
}

template < typename... T>
auto arrayIsElem ( class vmInstance *instance, VAR *arr, T... t)
{
	int64_t index[] = { t..., };
	return *_arrayIsElem ( instance, arr, sizeof... ( t ), index );
}

template < typename... T>
auto arrayIsElem ( class vmInstance *instance, VAR &arr, T... t)
{
	int64_t index[] = { t..., };
	return *_arrayIsElem ( instance, &arr, sizeof... ( t ), index );
}

