
#include "bcVM/bcVM.h"
#include "Utility/buffer.h"

VAR *allocNewElem ( class vmInstance *instance, int64_t num )
{
	VAR  *elem;
	VAR  *value;

	elem = instance->om->allocVar ( 2 );
	value = &elem[1];

	elem->type = slangType::eARRAY_ELEM;
	elem->dat.aElem.elemNum = (unsigned)num;
	elem->dat.aElem.var = value;
	elem->dat.aElem.next = 0;

	value->type = slangType::eNULL;

	return elem;
}

VAR *_arrayGet( class vmInstance *instance, VAR *val, int64_t num, int64_t *ind )
{
	VAR		*elem;
	int64_t  loop;
	VAR		*sparseRoot = nullptr;
	GRIP	 g1 ( instance );

	for ( loop = 0; loop < num; loop++ )
	{
		ARRAY_LOOP:
		switch ( TYPE( val ) )
		{
			case slangType::eARRAY_ROOT:
				val = val->dat.ref.v;
				goto ARRAY_LOOP;

			case slangType::eARRAY_SPARSE:
				sparseRoot = val;
				if ( val->dat.aSparse.v )
				{
					val = val->dat.aSparse.v;
				} else
				{
					elem = allocNewElem( instance, ind[loop] );

					val->dat.aSparse.lastAccess = elem;
					val->dat.aSparse.maxI = ind[loop];
					val->dat.aSparse.v = elem;

					val = elem;
				}
				goto ARRAY_LOOP;

			case slangType::eARRAY_FIXED:
				if ( (ind[loop] >= val->dat.arrayFixed.startIndex) && (ind[loop] <= val->dat.arrayFixed.endIndex) )
				{
					val = &val[ind[loop] - val->dat.arrayFixed.startIndex + 1];
				} else
				{
					throw errorNum::scFIXED_ARRAY_OUT_OF_BOUNDS;
				}
				break;

			case slangType::eARRAY_ELEM:
				assert ( sparseRoot );
				if ( val->dat.aElem.elemNum > ind[loop] )
				{
					elem = allocNewElem( instance, ind[loop] );

					sparseRoot->dat.aSparse.v = elem;
					elem->dat.aElem.next = val;

					val = elem->dat.aElem.var;

					sparseRoot->dat.aSparse.lastAccess = elem;
					break;
				}

				if ( sparseRoot->dat.aSparse.lastAccess && (sparseRoot->dat.aSparse.lastAccess->dat.aElem.elemNum <= ind[loop]) )
				{
					elem = sparseRoot->dat.aSparse.lastAccess;
				}
				else
				{
					elem = sparseRoot->dat.aSparse.v;
				}

				while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < ind[loop]) )
				{
					elem = elem->dat.aElem.next;
				}

				while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < ind[loop]) )
				{
					elem = elem->dat.aElem.next;
				}

				// in all cases, after this if series, elem must point to the array element being accessed (so that the lastAccess can be set correctly
				if ( elem->dat.aElem.elemNum == ind[loop] )
				{
				} else if ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum == ind[loop]) )
				{
					elem = elem->dat.aElem.next;
				} else
				{
					val = elem;

					if ( !val->dat.aElem.next )
					{
						sparseRoot->dat.aSparse.maxI = ind[loop];
					}
					elem = allocNewElem( instance, ind[loop] );
					elem->dat.aElem.next = val->dat.aElem.next;
					val->dat.aElem.next = elem;
				}
				sparseRoot->dat.aSparse.lastAccess = elem;
				val = elem->dat.aElem.var;
				break;

			case slangType::eNULL:
				val->type = slangType::eARRAY_ROOT;
				val->dat.ref.v = instance->om->allocVar ( 1 );
				val->dat.ref.obj = val->dat.ref.v;
				val->dat.ref.v->type = slangType::eARRAY_SPARSE;
				val->dat.ref.v->dat.aSparse.v = 0;
				val->dat.ref.v->dat.aSparse.lastAccess = 0;
				val->dat.ref.v->dat.aSparse.maxI = 0;
				goto ARRAY_LOOP;

			case slangType::eREFERENCE:
				val = val->dat.ref.v;
				goto ARRAY_LOOP;

			default:
				throw errorNum::scNONARRAY;
		}
	}
	return val;
}

bool _arrayIsElem( class vmInstance *instance, VAR *val, int64_t num, int64_t *ind )

{
	VAR		*elem;
	VAR		*array = nullptr;
	int64_t	 loop;

	for ( loop = 0; loop < num; loop++ )
	{
		ARRAY_LOOP:

		switch ( TYPE( val ) )
		{
			case slangType::eARRAY_ROOT:
				val = val->dat.ref.v;
				goto ARRAY_LOOP;
			case slangType::eARRAY_SPARSE:
				array = val;
				val = val->dat.aSparse.v;
				if ( !val )
				{
					return false;
				}
				goto ARRAY_LOOP;

			case slangType::eARRAY_FIXED:
				if ( (ind[loop] >= val->dat.arrayFixed.startIndex) && (ind[loop] <= val->dat.arrayFixed.endIndex) )
				{
					val = &val[ind[loop] - val->dat.arrayFixed.startIndex + 1];
				} else
				{
					return false;
				}
				break;

			case slangType::eARRAY_ELEM:
				assert ( array );
				elem = val;

				if ( elem->dat.aElem.elemNum > ind[loop] )
				{
					return false;
				}

				if ( array->dat.aSparse.lastAccess && (array->dat.aSparse.lastAccess->dat.aElem.elemNum <= ind[loop]) )
				{
					elem = array->dat.aSparse.lastAccess;
				}

				while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < ind[loop]) )
				{
					elem = elem->dat.aElem.next;
				}

				if ( elem->dat.aElem.elemNum == ind[loop] || (elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum == ind[loop])) )
				{
				} else
				{
					return false;
				}

				val = elem->dat.aElem.var;

				array->dat.aSparse.lastAccess = elem;
				break;
			case slangType::eNULL:
				return false;
			case slangType::eREFERENCE:
				val = val->dat.ref.v;
				goto ARRAY_LOOP;
			default:
				return false;
		}
	}
	return true;
}

VAR arrayFixed2( class vmInstance *instance, int64_t start, int64_t end, VAR const &defaultValue )
{
	VAR			 arrRoot{};
	VAR			*newVar;

	if ( end < start )
	{
		std::swap ( start, end );
	}

	newVar = instance->om->allocVar( (end - start + 1) + 1 );

	arrRoot.type = slangType::eARRAY_ROOT;
	arrRoot.dat.ref.v = newVar;
	arrRoot.dat.ref.obj = arrRoot.dat.ref.v;

	arrRoot.dat.ref.v->type = slangType::eARRAY_FIXED;
	arrRoot.dat.ref.v->dat.arrayFixed.startIndex = start;
	arrRoot.dat.ref.v->dat.arrayFixed.endIndex = end;

	for ( int64_t index = 1; index <= (end - start + 1); index++ )
	{
		arrRoot.dat.ref.v[index] = defaultValue;
	}

	return arrRoot;
}

VAR_STR _arrayToNameValue( class vmInstance *instance, VAR *array, char const *sep1, char const *sep2 )
{
	BUFFER	 buff;
	int64_t	 ind[2] = { 1,1 };
	VAR		*var = nullptr;

	if ( (TYPE( array ) != slangType::eARRAY_SPARSE) && (TYPE( array ) != slangType::eARRAY_FIXED) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	while ( _arrayIsElem( instance, array, 2, ind ) )
	{
		var = _arrayGet( instance, array, 2, ind );

		while ( TYPE( var ) == slangType::eREFERENCE )
		{
			var = var->dat.ref.v;
		}

		if ( TYPE( var ) != slangType::eSTRING )
		{
			throw errorNum::scINVALID_PARAMETER;
		}

		bufferPrintf( &buff, "%s%s", var->dat.str.c, sep1 );

		ind[1] = 2;
		var = _arrayGet( instance, array, 2, ind );

		while ( TYPE( var ) == slangType::eREFERENCE )
		{
			var = var->dat.ref.v;
		}

		switch ( TYPE( var ) )
		{
			case slangType::eSTRING:
				bufferMakeRoom( &buff, (var->dat.str.len * 3) + 1 );
				bufferAssume( &buff, encHexExtendedX( var->dat.str.c,  (int)var->dat.str.len, buff.buff + buff.buffPos ) - 1 );
				break;
			case slangType::eLONG:
				bufferPrintf( &buff, "%i", var->dat.l );
				break;
			case slangType::eDOUBLE:
				bufferPrintf( &buff, "%f", var->dat.d );
				break;
			case slangType::eNULL:
				break;
			default:
				throw errorNum::scINVALID_PARAMETER;
		}

		bufferPrintf( &buff, "%s", sep2 );

		ind[0]++;
		ind[1] = 1;
	}

	bufferPut8 ( &buff, 0 );

	return VAR_STR ( instance, buff );
}

int64_t _amaxi_ ( VAR *var )
{
	if ( TYPE ( var ) != slangType::eARRAY_ROOT )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	var = var->dat.ref.v;
	
	switch ( TYPE ( var ) )
	{
		case slangType::eARRAY_SPARSE:
			if ( !var->dat.aSparse.v )
			{
				return ( 0 );
			} else
			{
				return ( var->dat.aSparse.maxI );
			}
			break;
		case slangType::eARRAY_FIXED:
			return ( var->dat.arrayFixed.endIndex );
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}
}

int64_t _amini_ ( VAR *var )
{
	if ( TYPE ( var ) != slangType::eARRAY_ROOT )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	var = var->dat.ref.v;
	
	switch ( TYPE ( var ) )
	{
		case slangType::eARRAY_SPARSE:
			if ( !var->dat.aSparse.v )
			{
				return ( 0 );
			} else
			{
				return ( var->dat.aSparse.v->dat.aElem.elemNum );
			}
			break;
		case slangType::eARRAY_FIXED:
			return ( var->dat.arrayFixed.startIndex );
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
}

VAR _anexti ( class vmInstance *instance, VAR *arr, VAR *indicie )
{
	VAR *elem = nullptr;

	while ( TYPE ( arr ) == slangType::eREFERENCE )
	{
		arr = arr->dat.ref.v;
	}

	while ( TYPE ( indicie ) == slangType::eREFERENCE )
	{
		indicie = indicie->dat.ref.v;
	}

	switch ( TYPE ( arr ) )
	{
		case slangType::eARRAY_SPARSE:
			switch ( TYPE ( indicie ) )
			{
				case slangType::eLONG:
					elem = arr->dat.aSparse.v;
					
					if ( elem->dat.aElem.elemNum > indicie->dat.l )
					{
						elem->dat.aSparse.lastAccess = elem;

						instance->result.type	= slangType::eLONG;
						instance->result.dat.l	= indicie->dat.l;
					}

					if ( arr->dat.aSparse.lastAccess &&  (arr->dat.aSparse.lastAccess->dat.aElem.elemNum <= indicie->dat.l ) )
					{
						elem = arr->dat.aSparse.lastAccess;
					}

					while ( elem->dat.aElem.next && ( elem->dat.aElem.next->dat.aElem.elemNum < indicie->dat.l) )
					{
						elem = elem->dat.aElem.next;
					}
					
					if ( elem->dat.aElem.elemNum == indicie->dat.l || (elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum == indicie->dat.l)) )
					{
					} else
					{
						instance->result.type = slangType::eNULL;
					}

					if ( arr->dat.aSparse.lastAccess && arr->dat.aSparse.lastAccess->dat.aElem.elemNum == indicie->dat.l )
					{
						if ( arr->dat.aSparse.lastAccess->dat.aElem.next )
						{
							arr->dat.aSparse.lastAccess = arr->dat.aSparse.lastAccess->dat.aElem.next;
							instance->result.type	= slangType::eLONG;
							instance->result.dat.l	= arr->dat.aSparse.lastAccess->dat.aElem.elemNum;
						} else
						{
							instance->result.type = slangType::eNULL;
						}
						break;
					}
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
			break;
	}
	return instance->result;
}


