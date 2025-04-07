/*
IO Support functions

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "vmAllocator.h"

static int vmIsEqual ( VAR *v1, VAR *v2, int isCaseSensitive )
{
	switch ( v1->type )
	{
		case slangType::eLONG:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.l == v2->dat.l;
				case slangType::eDOUBLE:
					return (double)v1->dat.l == v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.l == atoi ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eDOUBLE:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.d == (double)v2->dat.l;
				case slangType::eDOUBLE:
					return v1->dat.d == v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.d == atof ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eSTRING:
			switch ( v2->type )
			{
				case slangType::eSTRING:
					if ( !isCaseSensitive )
					{
						if ( !memcmpi ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) )
						{
							return 1;
						}
						return 0;
					}
					if ( !memcmp ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) )
					{
						return 1;
					}
					return 0;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return 0;
}

static int vmIsLessThan ( VAR *v1, VAR *v2, int isCaseSensitive )
{
	switch ( v1->type )
	{
		case slangType::eLONG:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.l < v2->dat.l;
				case slangType::eDOUBLE:
					return (double)v1->dat.l < v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.l < atoi ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eDOUBLE:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.d < (double)v2->dat.l;
				case slangType::eDOUBLE:
					return v1->dat.d < v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.d < atof ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eSTRING:
			switch ( v2->type )
			{
				case slangType::eSTRING:
					if ( !isCaseSensitive )
					{
						if ( memcmpi ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) < 0 )
						{
							return 1;
						}
						return 0;
					}
					if ( memcmp ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) < 0 )
					{
						return 1;
					}
					return 0;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return 0;
}

static int vmIsLessThanOrEqual ( VAR *v1, VAR *v2, int isCaseSensitive )
{
	switch ( v1->type )
	{
		case slangType::eLONG:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.l <= v2->dat.l;
				case slangType::eDOUBLE:
					return (double)v1->dat.l <= v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.l <= atoi ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eDOUBLE:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.d <= (double)v2->dat.l;
				case slangType::eDOUBLE:
					return v1->dat.d <= v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.d <= atof ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eSTRING:
			switch ( v2->type )
			{
				case slangType::eSTRING:
					if ( !isCaseSensitive )
					{
						if ( memcmpi ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) <= 0 )
						{
							return 1;
						}
						return 0;
					}
					if ( strncmp ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) <= 0 )
					{
						return 1;
					}
					return 0;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return 0;
}

static int vmIsGreaterThan ( VAR *v1, VAR *v2, int isCaseSensitive )
{
	switch ( v1->type )
	{
		case slangType::eLONG:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.l > v2->dat.l;
				case slangType::eDOUBLE:
					return (double)v1->dat.l > v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.l > atoi ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eDOUBLE:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.d > (double)v2->dat.l;
				case slangType::eDOUBLE:
					return v1->dat.d > v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.d > atof ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eSTRING:
			switch ( v2->type )
			{
				case slangType::eSTRING:
					if ( !isCaseSensitive )
					{
						if ( memcmpi ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) > 0 )
						{
							return 1;
						}
						return 0;
					}
					if ( strncmp ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) > 0 )
					{
						return 1;
					}
					return 0;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return 0;
}

static int vmIsGreaterThanOrEqual ( VAR *v1, VAR *v2, int isCaseSensitive )
{
	switch ( v1->type )
	{
		case slangType::eLONG:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.l >= v2->dat.l;
				case slangType::eDOUBLE:
					return (double)v1->dat.l >= v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.l >= atoi ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eDOUBLE:
			switch ( v2->type )
			{
				case slangType::eLONG:
					return v1->dat.d >= (double)v2->dat.l;
				case slangType::eDOUBLE:
					return v1->dat.d >= v2->dat.d;
				case slangType::eSTRING:
					return v1->dat.d >= atof ( v2->dat.str.c );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		case slangType::eSTRING:
			switch ( v2->type )
			{
				case slangType::eSTRING:
					if ( !isCaseSensitive )
					{
						if ( memcmpi ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) >= 0 )
						{
							return 1;
						}
						return 0;
					}
					if ( strncmp ( v1->dat.str.c, v2->dat.str.c, v1->dat.str.len ) >= 0 )
					{
						return 1;
					}
					return 0;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	return 0;
}

void builtinLegacyInit ( class vmInstance *instance, opFile *file )
{
	//--------------------------------------------------------------------------
	//                     General System Support Functions
	//--------------------------------------------------------------------------
	REGISTER ( instance, file );
		FUNC ( "isEqual", vmIsEqual, DEF ( 3, "false" ) );
		FUNC ( "isLessThan", vmIsLessThan, DEF ( 3, "false" ) );
		FUNC ( "isLessThanOrEqual", vmIsLessThanOrEqual, DEF ( 3, "false" ) );
		FUNC ( "isGreaterThan", vmIsGreaterThan, DEF ( 3, "false" ) );
		FUNC ( "isGreaterThanOrEqual", vmIsGreaterThanOrEqual, DEF ( 3, "false" ) );
	END;
}
