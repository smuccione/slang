/*
		IO Support functions

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"

static void assert_eq ( class vmInstance *instance, VAR *val1, VAR *val2 )
{
	if ( TYPE( val1 ) != TYPE( val2 ) ) throw errorNum::scASSERT;

	switch ( TYPE( val1 ) )
	{
		case slangType::eOBJECT_ROOT:
		case slangType::eARRAY_ROOT:
			if ( val1->dat.ref.v != val2->dat.ref.v ) throw errorNum::scASSERT;
			break;
		case slangType::eLONG:
			if ( val1->dat.l != val2->dat.l ) throw errorNum::scASSERT;
			break;
		case slangType::eDOUBLE:
			if ( val1->dat.d != val2->dat.d ) throw errorNum::scASSERT;
			break;
		case slangType::eSTRING:
			if ( val1->dat.str.len != val2->dat.str.len ) throw errorNum::scASSERT;
			if ( strncmp( val1->dat.str.c, val2->dat.str.c, val1->dat.str.len ) != 0 ) throw errorNum::scASSERT;
			break;
		default:
			throw errorNum::scASSERT;
	}
}

static void assert_ne ( class vmInstance *instance, VAR *val1, VAR *val2 )
{
	if ( TYPE( val1 ) == TYPE( val2 ) )
	{
		switch ( TYPE( val1 ) )
		{
			case slangType::eOBJECT_ROOT:
			case slangType::eARRAY_ROOT:
				if ( val1->dat.ref.v == val2->dat.ref.v ) throw errorNum::scASSERT;
				break;
			case slangType::eLONG:
				if ( val1->dat.l == val2->dat.l ) throw errorNum::scASSERT;
				break;
			case slangType::eDOUBLE:
				if ( val1->dat.d == val2->dat.d ) throw errorNum::scASSERT;
				break;
			case slangType::eSTRING:
				if ( val1->dat.str.len == val2->dat.str.len )
				{
					if ( !strncmp( val1->dat.str.c, val2->dat.str.c, val1->dat.str.len ) ) throw errorNum::scASSERT;
				}
				break;
			default:
				throw errorNum::scASSERT;
		}
	}
}

static void assert_gt ( class vmInstance *instance, VAR *val1, VAR *val2 )
{
	if ( TYPE( val1 ) != TYPE( val2 ) ) throw errorNum::scASSERT;

	switch ( TYPE( val1 ) )
	{
		case slangType::eLONG:
			if ( val1->dat.l > val2->dat.l ) throw errorNum::scASSERT;
			break;
		case slangType::eDOUBLE:
			if ( val1->dat.d > val2->dat.d ) throw errorNum::scASSERT;
			break;
		case slangType::eSTRING:
			{
				auto len = val1->dat.str.len > val2->dat.str.len ? val1->dat.str.len : val2->dat.str.len;
				if ( strncmp( val1->dat.str.c, val2->dat.str.c, len ) > 0 ) throw errorNum::scASSERT;
			}
			break;
		default:
			throw errorNum::scASSERT;
	}
}
static void assert_ge( class vmInstance *instance, VAR *val1, VAR *val2 )
{
	if ( TYPE( val1 ) != TYPE( val2 ) ) throw errorNum::scASSERT;

	switch ( TYPE( val1 ) )
	{
		case slangType::eLONG:
			if ( val1->dat.l >= val2->dat.l ) throw errorNum::scASSERT;
			break;
		case slangType::eDOUBLE:
			if ( val1->dat.d >= val2->dat.d ) throw errorNum::scASSERT;
			break;
		case slangType::eSTRING:
			{
				auto len = val1->dat.str.len > val2->dat.str.len ? val1->dat.str.len : val2->dat.str.len;
				if ( strncmp( val1->dat.str.c, val2->dat.str.c, len ) >= 0 ) throw errorNum::scASSERT;
			}
			break;
		default:
			throw errorNum::scASSERT;
	}
}
static void assert_lt ( class vmInstance *instance, VAR *val1, VAR *val2 )
{
	if ( TYPE( val1 ) != TYPE( val2 ) ) throw errorNum::scASSERT;

	switch ( TYPE( val1 ) )
	{
		case slangType::eLONG:
			if ( val1->dat.l < val2->dat.l ) throw errorNum::scASSERT;
			break;
		case slangType::eDOUBLE:
			if ( val1->dat.d < val2->dat.d ) throw errorNum::scASSERT;
			break;
		case slangType::eSTRING:
			{
				auto len = val1->dat.str.len > val2->dat.str.len ? val1->dat.str.len : val2->dat.str.len;
				if ( strncmp( val1->dat.str.c, val2->dat.str.c, len ) < 0 ) throw errorNum::scASSERT;
			}
			break;
		default:
			throw errorNum::scASSERT;
	}
}
static void assert_le ( class vmInstance *instance, VAR *val1, VAR *val2 )
{
	if ( TYPE( val1 ) != TYPE( val2 ) ) throw errorNum::scASSERT;

	switch ( TYPE( val1 ) )
	{
		case slangType::eLONG:
			if ( val1->dat.l < val2->dat.l ) throw errorNum::scASSERT;
			break;
		case slangType::eDOUBLE:
			if ( val1->dat.d < val2->dat.d ) throw errorNum::scASSERT;
			break;
		case slangType::eSTRING:
			{
				auto len = val1->dat.str.len > val2->dat.str.len ? val1->dat.str.len : val2->dat.str.len;
				if ( strncmp( val1->dat.str.c, val2->dat.str.c, len ) <= 0 ) throw errorNum::scASSERT;
			}
			break;
		default:
			throw errorNum::scASSERT;
	}
}

static void assert_file_out( vmInstance *instance, char const *fName )
{

}

static void assert_out( vmInstance *instance, char const *fName )
{

}

void builtinUnitTest ( class vmInstance *instance, opFile *file )
{
	//--------------------------------------------------------------------------
	//                     Unite Test Support Functions
	//--------------------------------------------------------------------------
	REGISTER( instance, file );
		FUNC ( "assert_eq", assert_eq );
		FUNC ( "assert_ne", assert_ne );
		FUNC ( "assert_lt", assert_lt );
		FUNC ( "assert_le", assert_le );
		FUNC ( "assert_gt", assert_gt );
		FUNC ( "assert_ge", assert_ge );
		FUNC ( "assert_output_eq_file", assert_file_out );
		FUNC ( "assert_output_eq", assert_out );
	END;
}
