/*

	system class for session object
*/

#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"


#define START_BUFFER_SIZE	16384
#define INC_BUFFER_SIZE		65536

static void cBufferNew ( class vmInstance *instance, VAR_OBJ *obj )
{
	*(*obj)["buff"] = VAR_STR ( (char *) instance->om->alloc ( sizeof ( char ) * START_BUFFER_SIZE ), START_BUFFER_SIZE );
	*(*obj)["size"] = VAR ( (int64_t) 0 );
}

static void cStaticBufferNew ( class vmInstance *instance, VAR_OBJ *obj, VAR_STR *value )
{
	*(*obj)["buff"] = *value;
	*(*obj)["size"] = VAR ( value->dat.str.len );
}

/* class initialization */

static VAR cBufferAddAssign ( class vmInstance *instance, VAR_OBJ *obj, VAR *data )
{
	char const	*cData;
	char		*cTmp;
	char		 tmp[33];
	size_t		 len;

	while ( TYPE ( data ) == slangType::eREFERENCE ) data = data->dat.ref.v;

	/* get the object */
	switch ( TYPE ( data ) )
	{
		case slangType::eSTRING:
			len = data->dat.str.len;
			break;
		case slangType::eLONG:
			_i64toa_s ( data->dat.l, tmp, sizeof( tmp ), 10 );
			len = strlen ( tmp );
			break;
		case slangType::eDOUBLE:
			len = _snprintf_s ( tmp, sizeof ( tmp ), _TRUNCATE, "%.4f", data->dat.d );
			break;
		case slangType::eNULL:
			instance->result = *obj;
			return instance->result;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	/* get the size var */
	auto size = (*obj)["size"];
	auto buff = (*obj)["buff"];

	if ( (int)size->dat.l + len + 1 > (int)buff->dat.str.len )
	{
		/* damn.. out of room... need to allocate a bigger buffer */

		cTmp = (char *)instance->om->alloc ( sizeof ( char ) * (buff->dat.str.len + len + 1 + INC_BUFFER_SIZE) );

		buff->dat.str.len += len + INC_BUFFER_SIZE;

		memcpy ( cTmp, buff->dat.str.c, size->dat.l );
		buff->dat.str.c = cTmp;
	}

	switch ( TYPE ( data ) )
	{
		case slangType::eSTRING:
			cData = data->dat.str.c;
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			cData = tmp;
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	memcpy ( const_cast<char *>(buff->dat.str.c) + size->dat.l, cData, len );
	size->dat.l += (int32_t)len;
	const_cast<char *>(buff->dat.str.c)[size->dat.l] = 0;

	return *obj;
}

static VAR cBufferAssign ( class vmInstance *instance, VAR_OBJ *obj, VAR *data )
{
	char const	*cData;
	char		*cTmp;
	char		 tmp[33];
	size_t		 len;

	while ( TYPE ( data ) == slangType::eREFERENCE ) data = data->dat.ref.v;

	switch ( TYPE ( data ) )
	{
		case slangType::eSTRING:
			len = data->dat.str.len;
			break;
		case slangType::eLONG:
			_i64toa_s ( data->dat.l, tmp, sizeof( tmp ), 10 );
			len = strlen ( tmp );
			break;
		case slangType::eDOUBLE:
			len = _snprintf_s ( tmp, sizeof ( tmp ), _TRUNCATE, "%.4f", data->dat.d );
			break;
		case slangType::eNULL:
			instance->result.type = slangType::eNULL;
			return instance->result;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	auto size = (*obj)["size"];
	auto buff = (*obj)["buff"];

	if ( len + 1 > (int)buff->dat.str.len )
	{
		/* damn.. out of room... need to allocate a bigger buffer */

		cTmp = (char *)instance->om->alloc ( sizeof ( char ) * (len + 1 + INC_BUFFER_SIZE) );

		buff->dat.str.len += len + START_BUFFER_SIZE;
		buff->dat.str.c = cTmp;
	}

	switch ( TYPE ( data ) )
	{
		case slangType::eSTRING:
			cData = data->dat.str.c;
			break;
		case slangType::eLONG:
		case slangType::eDOUBLE:
			cData = tmp;
			break;
		default:
			throw errorNum::scINTERNAL;
	}

	memcpy ( const_cast<char *>(buff->dat.str.c), cData, len );
	size->dat.l			 = (int32_t)len;
	const_cast<char *>(buff->dat.str.c)[len] = 0;

	return *obj;
}

static VAR cBufferStr ( class vmInstance *instance, VAR_OBJ *obj )
{
	auto size = (*obj)["size"];
	auto buff = (*obj)["buff"];

	instance->result.type			= slangType::eSTRING;
	instance->result.dat.str.c		= buff->dat.str.c;
	instance->result.dat.str.len	= size->dat.l;

	return instance->result;
}

class bufferStrInspector : public vmDebugVar {
	const char		*name;
	size_t			nameLen;
	const char		*value;
	size_t			 valueLen;
	bool			 doFree;
public:
	~bufferStrInspector ()
	{
		if ( doFree )
		{
			free ( (void *)value );
			free ( (void *)name );
		}
	}

	bufferStrInspector ( char const *name, size_t nameLen, char const *value, size_t valueLen, bool doFree = false ) : doFree ( doFree )
	{
		this->name = name;
		this->nameLen = nameLen;
		this->value = value;
		this->valueLen = valueLen;
	}

	const char			*getName ( void ) override
	{
		return name;
	}
	size_t			 getNameLen ( void ) override
	{
		return nameLen;
	}
	bool				 isStringValue ( void ) override
	{
		return true;
	}
	const char			*getValue ( void ) override
	{
		return value;
	}
	size_t			 getValueLen ( void ) override
	{
		return valueLen;
	}
	stringi				 getType ( void ) override
	{
		return "string";
	}
	class vmInspectList	*getChildren ( class vmInstance *instance, bcFuncDef *funcDef, size_t start, size_t end )  override
	{
		throw errorNum::scINTERNAL;
	}
	bool				 hasChildren ( void ) override
	{
		return false;
	}
};

static vmInspectList *cBufferInspector ( class vmInstance *instance, bcFuncDef *func, VAR *obj, uint64_t start, uint64_t end )
{
	vmInspectList		*vars;

	auto size = (*static_cast<VAR_OBJ*>(obj))["size"];
	auto buff = (*static_cast<VAR_OBJ *>(obj))["buff"];

	vars = new vmInspectList();
	vars->entry.push_back ( static_cast<vmDebugVar *>(new bufferStrInspector ( "str", 3, buff->dat.str.c, size->dat.l )) );

	return vars;
}

auto cBufferLen ( class vmInstance *instance, VAR_OBJ *obj)
{
	auto size = (*obj)["size"];
	return size->dat.l;
}

void builtinSystemBufferInit ( class vmInstance *instance, opFile *file )
{
	REGISTER( instance, file );
		CLASS ( "serverBuffer" );
			METHOD( "new", cBufferNew );
			ACCESS( "str", cBufferStr )  CONST;
			ACCESS( "value", cBufferStr )  CONST;
			METHOD( "len", cBufferLen )  CONST;
			OP ( "=", cBufferAssign );
			OP ( "+=", cBufferAddAssign );
			PRIVATE
				IVAR ( "buff" );
				IVAR ( "size" );
			INSPECTORCB ( cBufferInspector );
		END;

		CLASS ( "stringBuffer" );
			METHOD( "new", cBufferNew );
			ACCESS( "str", cBufferStr )  CONST;
			ACCESS( "value", cBufferStr )  CONST;
			METHOD( "len", cBufferLen )  CONST;
			OP ( "=", cBufferAssign );
			OP ( "+=", cBufferAddAssign );
			PRIVATE
				IVAR ( "buff" );
				IVAR ( "size" );
			INSPECTORCB ( cBufferInspector );
		END;

		CLASS ( "$staticBuffer" );
			METHOD( "new", cStaticBufferNew );
			ACCESS( "str", cBufferStr )  CONST;
			ACCESS( "value", cBufferStr )  CONST;
			METHOD ( "len", cBufferLen )  CONST;
			PRIVATE
				IVAR ( "buff" );
				IVAR ( "size" );
			INSPECTORCB ( cBufferInspector );
		END;
	END;
}
