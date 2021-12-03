/*
	HTTP Client functions

*/

#include "compilerParser/fileParser.h"
#include "compilerBCGenerator/compilerBCGenerator.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/vmNativeInterface.h"
#include "builtinDom.h"

static void cAddToArray( class vmInstance *instance, VAR_OBJ *nodeList, VAR_OBJ *obj )
{
	auto length = (*nodeList)["length"];

	if ( TYPE( length ) != slangType::eLONG )
	{
		length->type = slangType::eLONG;
		length->dat.l = 0;
	}
	*(*(*nodeList)["array"])[length->dat.l] = VAR_REF ( obj, obj );
	
	length->dat.l++;
}

static void cAddToArrayUnique( class vmInstance *instance, VAR_OBJ *nodeList, VAR *obj )
{
//	auto tagName = (*nodeList)["tagName"];
	auto length = (*nodeList)["length"];
	auto array = (*nodeList)["array"];

	if ( TYPE( length ) != slangType::eLONG )
	{
		length->type = slangType::eLONG;
		length->dat.l = 0;
	}

	// make sure it's not already present
	int64_t index = 1;
	while ( index <= length->dat.l )
	{
		auto var = _arrayGet( instance, array, 1, &index );

		while ( TYPE( var ) == slangType::eREFERENCE )
		{
			var = var->dat.ref.v;
		}
		if ( var == obj )
		{
			return;
		}
		index++;
	}

	// not found so add it in.
	length->dat.l++;

	arrayGet ( instance, array, length->dat.l ) = VAR_REF ( obj, obj );
}

typedef int( *ProcessCallback )	(class vmInstance *instance, VAR_OBJ *obj, void const *cargo1, void const *cargo2, void const *cargo3);
typedef int( *ProcessCallbackNS )(class vmInstance *instance, VAR_OBJ *obj, char const *ns, void const *cargo1, void const *cargo2, void const *cargo3);

static int cNodeWalk( class vmInstance *instance, VAR_OBJ *obj, ProcessCallback process, void const *cargo1, void const *cargo2, void const *cargo3, int doProcess )
{
	VAR			*child;

	if ( TYPE( obj ) == slangType::eOBJECT_ROOT )
	{
		if ( doProcess )
		{
			/* process this object */
			if ( (process)(instance, obj, cargo1, cargo2, cargo3) )
			{
				/* we've been signalled to exit */
				instance->result = *obj;
				/* terminate the walk */
				return 1;
			}
		}

		if ( !(child = classIVarAccess( obj, "FIRSTCHILD" )) )
		{
			return 0;
		}

		while ( TYPE( child ) == slangType::eREFERENCE )
		{
			child = child->dat.ref.v;
		}

		while ( TYPE( child ) == slangType::eOBJECT_ROOT )
		{
			/* walk this child */
			if ( cNodeWalk( instance, static_cast<VAR_OBJ *>(child), process, cargo1, cargo2, cargo3, 1 ) )
			{
				return 1;
			}

			if ( !(child = classIVarAccess( child, "NEXTSIBLING" )) )
			{
				return 0;
			}
			while ( TYPE( child ) == slangType::eREFERENCE )
			{
				child = child->dat.ref.v;
			}
		}
	}

	return 0;
}

static int cNodeWalkNS( class vmInstance *instance, VAR_OBJ *obj, ProcessCallbackNS process, char const *ns, void const *cargo1, void const *cargo2, void const *cargo3 )

{
	VAR			*child;

	if ( TYPE( obj ) == slangType::eOBJECT_ROOT )
	{
		/* process this object */
		if ( (process)(instance, static_cast<VAR_OBJ *>(obj), ns, cargo1, cargo2, cargo3) )
		{
			/* we've been signalled to exit */
			instance->result = *obj;

			/* terminate the walk */
			return 1;
		}

		if ( !(child = classIVarAccess( obj, "FIRSTCHILD" )) )
		{
			return 0;
		}

		while ( TYPE( child ) == slangType::eREFERENCE )
		{
			child = child->dat.ref.v;
		}

		while ( TYPE( child ) == slangType::eOBJECT_ROOT )
		{
			/* walk this child */
			if ( cNodeWalkNS( instance, static_cast<VAR_OBJ *>(child), process, ns, cargo1, cargo2, cargo3 ) )
			{
				return 1;
			}

			if ( !(child = classIVarAccess( child, "NEXTSIBLING" )) )
			{
				return 0;
			}
			while ( TYPE( child ) == slangType::eREFERENCE )
			{
				child = child->dat.ref.v;
			}
		}
	}

	return 0;
}

int cCompareNameAndAdd( class vmInstance *instance, VAR_OBJ *obj, VAR *nodeList, char const *name, void *data )

{
	VAR *val;

	if ( !(val = classIVarAccess( obj, "NAME" )) )
	{
		if ( !(val = classIVarAccess( obj, "NODENAME" )) )
		{
			return 0;
		}
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !strccmp( name, val->dat.str.c ) )
		{
			cAddToArray( instance, static_cast<VAR_OBJ *>(nodeList), obj );
		}
	}

	/* we never force an exit no matter what */
	return 0;
}

int cCompareNameAndAddNS( class vmInstance *instance, VAR_OBJ *obj, char const *ns, VAR *nodeList, char const *name, void *data )

{
	VAR *val;
	VAR	*nSpace;

	if ( !(val = classIVarAccess( obj, "NAME" )) )
	{
		if ( !(val = classIVarAccess( obj, "NODENAME" )) )
		{
			return 0;
		}
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !(nSpace = classIVarAccess( obj, "nSpaceURI" )) )
		{
			return 0;
		}

		if ( TYPE( nSpace ) == slangType::eSTRING )
		{
			if ( strccmp( ns, nSpace->dat.str.c ) )
			{
				return 0;
			}
		}

		if ( !strccmp( name, val->dat.str.c ) )
		{
			cAddToArray( instance, static_cast<VAR_OBJ *>(nodeList), obj );
		}
	}

	/* we never force an exit no matter what */
	return 0;
}

int cCompareTagAndAdd( class vmInstance *instance, VAR_OBJ *obj, VAR *nodeList, char const *name, void *data )

{
	VAR *val;

	if ( !(val = classIVarAccess( obj, "TAGNAME" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !strccmp( name, val->dat.str.c ) )
		{
			cAddToArray( instance, static_cast<VAR_OBJ *>(nodeList), obj );
		}
	}

	/* we never force an exit no matter what */
	return 0;
}

int cCompareTagAndAddNS( class vmInstance *instance, VAR_OBJ *obj, char const *ns, VAR *nodeList, char const *name, void *data )

{
	VAR *val;
	VAR	*nSpace;

	if ( !(val = classIVarAccess( obj, "TAGNAME" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !(nSpace = classIVarAccess( obj, "nSpaceURI" )) )
		{
			return 0;
		}

		if ( TYPE( nSpace ) == slangType::eSTRING )
		{
			if ( strccmp( ns, nSpace->dat.str.c ) )
			{
				return 0;
			}
		}

		if ( !strccmp( name, val->dat.str.c ) )
		{
			cAddToArray( instance, static_cast<VAR_OBJ *>(nodeList), obj );
		}
	}

	/* we never force an exit no matter what */
	return 0;
}

int cFindIdAndExit( class vmInstance *instance, VAR_OBJ *obj, VAR *nodeList, char const *name, void *data )

{
	VAR *val;

	if ( !(val = classIVarAccess( obj, "ID" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !strccmp( name, val->dat.str.c ) )
		{
			/* got one so get out */
			return 1;
		}
	}

	return 0;
}

int cFindTagAndExit( class vmInstance *instance, VAR_OBJ *obj, VAR *nodeList, char const *name, void *data )

{
	VAR *val;

	if ( !(val = classIVarAccess( obj, "TAGNAME" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !strccmp( name, val->dat.str.c ) )
		{
			/* got one so get out */
			return 1;
		}
	}

	return 0;
}

int cFindAndExit( class vmInstance *instance, VAR_OBJ *obj, VAR *nodeList, char const *name, void *data )

{
	VAR *val;

	if ( !(val = classIVarAccess( obj, "NAME" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !strccmp( name, val->dat.str.c ) )
		{
			/* got one so get out */
			return 1;
		}
	}

	return 0;
}

int cFindNodeNameAndExit( class vmInstance *instance, VAR_OBJ *obj, VAR *nodeList, char const *name, void *data )

{
	VAR *val;

	if ( !(val = classIVarAccess( obj, "NODENAME" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !strccmp( name, val->dat.str.c ) )
		{
			/* got one so get out */
			return 1;
		}
	}

	return 0;
}

int cFindNodeNameAndExitNS( class vmInstance *instance, VAR_OBJ *obj, char const *ns, VAR *nodeList, char const *name, void *data )

{
	VAR *val;
	VAR	*nSpace;

	if ( !(val = classIVarAccess( obj, "NODENAME" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		if ( !(nSpace = classIVarAccess( obj, "nSpaceURI" )) )
		{
			return 0;
		}

		if ( TYPE( nSpace ) == slangType::eSTRING )
		{
			if ( strccmp( ns, nSpace->dat.str.c ) )
			{
				return 0;
			}
		}

		if ( !strccmp( name, val->dat.str.c ) )
		{
			/* got one so get out */
			return 1;
		}
	}

	return 0;
}

int cFindPostDataNodes( class vmInstance *instance, VAR_OBJ *obj, VAR *array, VAR *destArray, void *data )

{
	VAR		*name;
	VAR		*nodeName;
	VAR		*value;
	VAR		*var;
	int64_t	 indicie[2]{};

	if ( !(nodeName = classIVarAccess( obj, "NODENAME" )) )
	{
		return 0;
	}

	if ( TYPE( nodeName ) == slangType::eSTRING )
	{
		if ( !strccmp( nodeName->dat.str.c, "INPUT" ) ||
			!strccmp( nodeName->dat.str.c, "SELECT" )
			)
		{
			if ( !(name = classIVarAccess( obj, "NAME" )) )
			{
				return 0;
			}

			if ( TYPE( name ) == slangType::eSTRING )
			{
				if ( destArray )
				{
					// only fill in the destination array with matching names

					indicie[0] = 1;
					indicie[1] = 1;

					while ( _arrayIsElem( instance, destArray, 2, indicie ) )
					{
						var = _arrayGet( instance, destArray, 2, indicie );

						while ( TYPE( var ) == slangType::eREFERENCE )
						{
							var = var->dat.ref.v;
						}

						if ( !strccmp( name->dat.str.c, var->dat.str.c ) )
						{
							indicie[1] = 2;

							var = _arrayGet( instance, destArray, 2, indicie );

							if ( TYPE( var ) != slangType::eNULL )
							{
								break;
							}

							if ( !(value = classIVarAccess( obj, "VALUE" )) )
							{
								return 0;
							}

							*var = VAR_REF ( obj, value );
							break;
						}
						indicie[0]++;
					}
				} else
				{
					// stick everything into the post-arrayData()ay
					if ( !(value = classIVarAccess( obj, "VALUE" )) )
					{
						return 0;
					}

					arrayGet( instance, array, (*((long *)data)), 1 ) = VAR_REF ( obj, name );
					arrayGet( instance, array, (*((long *)data)), 2 ) = VAR_REF ( obj, value );
					(*((long *)data))++;
				}
			}
		}
	}

	return 0;
}

VAR cElementGetAttribute( class vmInstance *instance, VAR_OBJ *self, char const *name )

{
	VAR			*attrib;
	VAR			*array;
	VAR			*length;
	VAR			*val;
	int64_t		 loop;

	instance->result.type = slangType::eNULL;

	if ( !(attrib = classIVarAccess( self, "ATTRIBUTES" )) )
	{
		return instance->result;
	}

	while ( TYPE( attrib ) == slangType::eREFERENCE )
	{
		attrib = attrib->dat.ref.v;
	}

	if ( !(array = classIVarAccess( attrib, "ARRAY" )) )
	{
		return instance->result;
	}

	while ( TYPE( array ) == slangType::eREFERENCE )
	{
		array = array->dat.ref.v;
	}

	if ( !(length = classIVarAccess( attrib, "LENGTH" )) )
	{
		return instance->result;
	}

	while ( TYPE( length ) == slangType::eREFERENCE )
	{
		length = length->dat.ref.v;
	}

	if ( TYPE( length ) == slangType::eLONG )
	{

		for ( loop = 0; loop < length->dat.l; loop++ )
		{
			val = &arrayGet( instance, array, loop, 1 );

			while ( TYPE( val ) == slangType::eREFERENCE )
			{
				val = val->dat.ref.v;
			}

			if ( TYPE( val ) == slangType::eSTRING )
			{
				if ( !strccmp( val->dat.str.c, name ) )
				{
					arrayGet ( instance, array, loop, 2 ) = VAR_REF ( array, val );
					return instance->result;
				}
			}
		}
	}
	return instance->result;
}

static VAR cNodeFindElements( class vmInstance *instance, VAR_OBJ *self, char const *name )
{
	classNew( instance, "NODELIST", 0 );

	cNodeWalk( instance, self, (ProcessCallback)cCompareNameAndAdd, instance->result.dat.ref.v, name, 0, 0 );

	return instance->result;
}

static VAR cNodeFindElement( class vmInstance *instance, VAR_OBJ *self, char const *name )
{
	instance->result.type = slangType::eNULL;

	if ( !cNodeWalk( instance, self, (ProcessCallback)cFindIdAndExit, 0, name, 0, 0 ) )
	{
		if ( !cNodeWalk( instance, self, (ProcessCallback)cFindAndExit, 0, name, 0, 0 ) )
		{
			if ( !cNodeWalk( instance, self, (ProcessCallback)cFindNodeNameAndExit, 0, name, 0, 0 ) )
			{
				cElementGetAttribute( instance, self, name );
			}
		}
	}

	return instance->result;
}

VAR cNodeFindElementsTag( class vmInstance *instance, VAR_OBJ *self, char const *name )
{
	classNew( instance, "NODELIST", 0 );

	cNodeWalk( instance, self, (ProcessCallback)cCompareNameAndAdd, instance->result.dat.ref.v, name, 0, 0 );

	return instance->result;
}

VAR cNodeFindElementsNS( class vmInstance *instance, VAR_OBJ *self, char const *name, char const *ns )
{
	classNew( instance, "NODELIST", 0 );

	cNodeWalkNS( instance, self, (ProcessCallbackNS)cCompareTagAndAddNS, ns, instance->result.dat.ref.v, name, 0 );

	return instance->result;
}

VAR cNodeFindElementsTagNS( class vmInstance *instance, VAR_OBJ *self, char const *name, char const *ns )
{
	classNew( instance, "NODELIST", 0 );

	cNodeWalkNS( instance, self, (ProcessCallbackNS)cCompareTagAndAddNS, ns, instance->result.dat.ref.v, name, 0 );

	return instance->result;
}

VAR cNodeDefaultFind( class vmInstance *instance, VAR_OBJ *self, char const *name )
{
	instance->result.type = slangType::eNULL;

	if ( !cNodeWalk( instance, self, (ProcessCallback)cFindIdAndExit, 0, name, 0, 0 ) )
	{
		if ( !cNodeWalk( instance, self, (ProcessCallback)cFindAndExit, 0, name, 0, 0 ) )
		{
			if ( !cNodeWalk( instance, self, (ProcessCallback)cFindNodeNameAndExit, 0, name, 0, 0 ) )
			{
				cElementGetAttribute( instance, self, name );
			}
		}
	}

	return instance->result;
}

VAR cFindNodeName( class vmInstance *instance, VAR_OBJ *self, char const *nodeName )
{
	instance->result.type = slangType::eNULL;

	cNodeWalk( instance, self, (ProcessCallback)cFindNodeNameAndExit, 0, nodeName, 0, 0 );

	return instance->result;
}

VAR cFindNodeNameNS( class vmInstance *instance, VAR_OBJ *self, char const *nodeName, char const *nSpace )
{
	instance->result.type = slangType::eNULL;

	cNodeWalkNS( instance, self, (ProcessCallbackNS)cFindNodeNameAndExitNS, nSpace, 0, nodeName, 0 );

	return instance->result;
}

int cNodeHasAttributes( class vmInstance *instance, VAR_OBJ *obj )
{
	auto val = classIVarAccess( obj, "ATTRIBUTES" );

	while ( TYPE( val ) == slangType::eREFERENCE )
	{
		val = val->dat.ref.v;
	}

	val = classIVarAccess( val, "LENGTH" );

	return (TYPE( val ) == slangType::eNULL ? 0 : 1);
}

int cNodeHasChild( class vmInstance *instance, VAR_OBJ *obj )
{
	auto val = classIVarAccess ( obj, "NAME" );

	while ( TYPE( val ) == slangType::eREFERENCE )
	{
		val = val->dat.ref.v;
	}

	return (TYPE( val ) == slangType::eNULL ? 0 : 1);
}

VAR cHtmlDocumentGetPostData( class vmInstance *instance, VAR_OBJ *self, nParamType numParams )
{
	int			 arrayIndex;
	VAR			*val;

	if ( numParams > 0 )
	{
		val = numParams[0];
		while ( TYPE( val ) == slangType::eREFERENCE )
		{
			val = val->dat.ref.v;
		}
	} else
	{
		val = 0;
	}

	instance->result.type = slangType::eNULL;

	arrayIndex = 1;

	cNodeWalk( instance, self, (ProcessCallback)cFindPostDataNodes, &(instance->result), val, &arrayIndex, 1 );

	if ( numParams > 0 )
	{
		instance->result = *val;
	}

	return instance->result;
}

int cNodeCreateElement( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cNodeIsSupported( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cNodeLocalName( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

size_t cCharacterDataLength( class vmInstance *instance, VAR_OBJ *self )

{
	VAR			*val;

	if ( !(val = classIVarAccess( self, "DATA" )) )
	{
		return 0;
	}

	if ( TYPE( val ) == slangType::eSTRING )
	{
		return (val->dat.str.len);
	}

	return 0;
}

VAR cElementPseudoValueAccess( class vmInstance *instance, VAR_OBJ *self )
{
	VAR			*child;
	VAR			*text;
	VAR			*data;

	instance->result.type = slangType::eNULL;

	if ( !(child = classIVarAccess( self, "FIRSTCHILD" )) )
	{
		return instance->result;
	}

	while ( child  )
	{
		while ( TYPE( child ) == slangType::eREFERENCE )
		{
			child = child->dat.ref.v;
		}

		if ( TYPE ( child ) == slangType::eNULL )
		{
			break;
		}

		if ( !(text = classIVarAccess( child, "NODENAME" )) )
		{
			return instance->result;
		}

		while ( TYPE( text ) == slangType::eREFERENCE )
		{
			text = text->dat.ref.v;
		}

		if ( TYPE( text ) == slangType::eSTRING )
		{
			if ( !strccmp( text->dat.str.c, "#text" ) )
			{
				if ( !(data = classIVarAccess( child, "DATA" )) )
				{
					return instance->result;
				}
				instance->result = VAR_REF ( child, data );
				return instance->result;
			}
		}

		if ( !(child = classIVarAccess( child, "NEXTSIBLING" )) )
		{
			return instance->result;
		}

	}

	return instance->result;
}

static VAR cArrayAccess( class vmInstance *instance, VAR_OBJ *self, VAR *param )

{
	VAR			*array;
	VAR			*name;
	VAR			*length;
	int64_t		 loop;
	int64_t		 indicie[2]{};

	instance->result.type = slangType::eNULL;

	if ( !(array = classIVarAccess( self, "ARRAY" )) )
	{
		return instance->result;
	}

	switch ( TYPE( param ) )
	{
		case slangType::eSTRING:
			if ( !(length = classIVarAccess( self, "LENGTH" )) )
			{
				return instance->result;
			}

			indicie[1] = 1;
			if ( TYPE( length ) == slangType::eLONG )
			{
				for ( loop = 0; loop <= length->dat.l; loop++ )
				{
					indicie[0] = loop;

					// array is name/value pairs

					name = _arrayGet( instance, array, 2, indicie );

					while ( TYPE( name ) == slangType::eREFERENCE )
					{
						name = name->dat.ref.v;
					}

					if ( TYPE( name ) == slangType::eSTRING )
					{
						if ( !strccmp( name->dat.str.c, param->dat.str.c ) )
						{
							indicie[1] = 2;

							instance->result = VAR_REF ( array, _arrayGet( instance, array, 2, indicie ) );
							return instance->result;
						}
					}
				}
			}
			break;
		case slangType::eLONG:
			indicie[0] = param->dat.l;
			instance->result = *(_arrayGet( instance, array, 1, indicie ));
			break;
		default:
			break;
	}

	return instance->result;
}

int cElementGetAttributeNS( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cHTMLCollectionItem( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cHTMLCollectionNamedItem( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentDocumentType( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentImplementation( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentElement( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateElement( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateTextNode( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateCDataSection( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateProcessingInstruction( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateEntityReference( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

VAR cDocumentGetElementsByTagName( class vmInstance *instance, VAR_OBJ *self, char const *name )
{
	instance->result.type = slangType::eNULL;

	cNodeWalk( instance, self, (ProcessCallback)cFindTagAndExit, instance->result.dat.ref.v, name, 0, 0 );

	return instance->result;
}

int cDocumentImportNode( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateElementNS( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateAttributeNS( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentGetElementsByTagNameNS( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

VAR cDocumentGetElementsByID( class vmInstance *instance, VAR_OBJ *self, char const *name )
{
	instance->result.type = slangType::eNULL;

	cNodeWalk( instance, self, (ProcessCallback)cFindIdAndExit, instance->result.dat.ref.v, name, 0, 0 );

	return instance->result;
}

int cDocumentCreateContent( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateAttribute( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cDocumentCreateComment( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

int cNamedNodeMapItem( class vmInstance *instance, VAR_OBJ *self )

{
	return 0;
}

VAR cNamedNodeMapGetNamedItem( class vmInstance *instance, VAR_OBJ *self, char const *name )
{
	VAR			*array;
	VAR			*value;
	int64_t		 indicie[2]{};

	instance->result.type = slangType::eNULL;

	if ( !(array = classIVarAccess( self, "ARRAY" )) )
	{
		return instance->result;
	}

	indicie[0] = 0;
	indicie[1] = 1;

	while ( _arrayIsElem( instance, array, 2, indicie ) )
	{
		value = _arrayGet( instance, array, 2, indicie );
		while ( TYPE( value ) == slangType::eREFERENCE )
		{
			value = value->dat.ref.v;
		}
		if ( TYPE( value ) != slangType::eSTRING )
		{
			return instance->result;
		}
		if ( !strccmp( value->dat.str.c, name ) )
		{
			indicie[1] = 2;
			instance->result = *(_arrayGet( instance, array, 2, indicie ));
			return instance->result;
		}
		indicie[0]++;
	}

	return instance->result;
}

int cNamedNodeMapGetNamedItemNS( class vmInstance *instance, VAR_OBJ *self, char const *nSpace, char const *name )
{
	return 0;
}

void cHtmlDocumentNew( vmInstance *instance, VAR_OBJ *self, char *data, nParamType nParams )
{
	int64_t		 tagsOnly;
	VAR			*val;

	tagsOnly = 0;

	if ( nParams == 1 )
	{
		val = nParams[0];
		if ( TYPE( val ) == slangType::eLONG )
		{
			tagsOnly = val->dat.l;
		} else
		{
			throw errorNum::scINVALID_PARAMETER;
		}
	} else if ( nParams > 2 )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	domParse( instance, self, data, tagsOnly );
}

void cXMLDocumentNew( vmInstance *instance, VAR_OBJ *self, char *data, bool noSimpleText )

{
	xmlParse( instance, self, data, noSimpleText );
}

enum class selOps
{
	SEL_START = 1,
	SEL_MATCH_DESC,
	SEL_MATCH_CHILD,
	SEL_MATCH_FIRST_CHILD,
	SEL_MATCH_SIB,
	SEL_MATCH_TAG_ID,
	SEL_MATCH_ATTR_EQUAL,
	SEL_MATCH_ATTR_LIST,
	SEL_MATCH_ATTR_PRESENT,
	SEL_DONE
};

struct OP
{
	selOps	op;
	char	dat[256];
	char	dat2[256];
};

extern int processSelectors( class vmInstance *instance, VAR_OBJ *obj, VAR_OBJ *nodeList, OP *op, VAR *parent );

int processChildren( class vmInstance *instance, VAR_OBJ *obj, VAR_OBJ *nodeList, OP *op )
{
	VAR *child;

	child = classIVarAccess( obj, "FIRSTCHILD" );

	while ( TYPE( child ) == slangType::eREFERENCE )
	{
		child = child->dat.ref.v;
	}

	while ( TYPE( child ) == slangType::eOBJECT_ROOT )
	{
		/* walk this child */
		processSelectors( instance, static_cast<VAR_OBJ *>(child), nodeList, op, obj );

		classIVarAccess( child, "NEXTSIBLING" );

		while ( TYPE( child ) == slangType::eREFERENCE )
		{
			child = child->dat.ref.v;
		}
	}
	return 1;
}


int processSelectors( class vmInstance *instance, VAR_OBJ *obj, VAR_OBJ *nodeList, OP *op, VAR *parent )
{
	VAR		*val;
	VAR		*sib;
	VAR		 resSave;
	char	 token[256];
	int		 nToken;
	int		 loop;

	switch ( op->op )
	{
		case selOps::SEL_DONE:
			return 1;
		case selOps::SEL_MATCH_DESC:
			val = classIVarAccess( obj, "TAGNAME" );
			if ( (*op->dat == '*') || ((TYPE( val ) == slangType::eSTRING) && !strccmp( op->dat, val->dat.str.c )) )
			{
				if ( op[1].op == selOps::SEL_DONE )
				{
					cAddToArrayUnique( instance, nodeList, obj );
					processChildren( instance, obj, nodeList, op );
				} else
				{
					switch ( (op + 1)->op )
					{
						case selOps::SEL_MATCH_DESC:
						case selOps::SEL_MATCH_CHILD:
						case selOps::SEL_MATCH_FIRST_CHILD:
						case selOps::SEL_MATCH_SIB:
						case selOps::SEL_MATCH_TAG_ID:
							// process descendants or siblings
							processChildren( instance, obj, nodeList, op + 1 );
							break;
						case selOps::SEL_MATCH_ATTR_EQUAL:
						case selOps::SEL_MATCH_ATTR_LIST:
						case selOps::SEL_MATCH_ATTR_PRESENT:
							// attr's sub match an already found node... reprocess the same node
							processSelectors( instance, obj, nodeList, op + 1, parent );
							break;
						default:
							break;
					}
				}
			} else
			{
				processChildren( instance, obj, nodeList, op );
			}
			return 1;

		case selOps::SEL_MATCH_CHILD:
			val = classIVarAccess( obj, "TAGNAME" );
			if ( (*op->dat == '*') || ((TYPE( val ) == slangType::eSTRING) && !strccmp( op->dat, val->dat.str.c )) )
			{
				if ( op[1].op == selOps::SEL_DONE )
				{
					cAddToArrayUnique( instance, nodeList, obj );
				} else
				{
					processChildren( instance, obj, nodeList, op + 1 );
				}
			}
			break;
		case selOps::SEL_MATCH_FIRST_CHILD:
			sib = classIVarAccess( parent, "FIRSTCHILD" );
			while ( TYPE( sib ) == slangType::eREFERENCE )
			{
				sib = sib->dat.ref.v;
			}

			// only check if we're the first child... otherwise skip and just descend
			if ( sib == obj )
			{
				if ( (val = classIVarAccess( obj, "TAGNAME" )) )
				{
					if ( (*op->dat == '*') || ((TYPE( val ) == slangType::eSTRING) && !strccmp( op->dat, val->dat.str.c )) )
					{
						if ( op[1].op == selOps::SEL_DONE )
						{
							cAddToArrayUnique( instance, nodeList, obj );
						} else
						{
							processChildren( instance, obj, nodeList, op + 1 );
						}
					}
				}
			}
			processChildren( instance, obj, nodeList, op );
			return 1;

		case selOps::SEL_MATCH_SIB:
			// get our parents first child
			val = parent;
			sib = classIVarAccess( parent, "PREVIOUSSIBLING" );
			while ( sib && (TYPE( sib ) == slangType::eREFERENCE) )
			{
				sib = sib->dat.ref.v;
			}
			while ( sib && (TYPE( sib ) != slangType::eNULL) )
			{
				val = sib;
				sib = classIVarAccess( sib, "PREVIOUSSIBLING" );
				while ( sib && (TYPE( sib ) == slangType::eREFERENCE) )
				{
					sib = sib->dat.ref.v;
				}
			}
			sib = val;

			while ( sib && (TYPE( sib ) != slangType::eNULL) )
			{
				while ( TYPE( sib ) == slangType::eREFERENCE )
				{
					sib = sib->dat.ref.v;
				}

				// are we not us?
				if ( sib != parent )
				{
					val = classIVarAccess( sib, "TAGNAME" );
					if ( (*op->dat == '*') || ((TYPE( val ) == slangType::eSTRING) && !strccmp( op->dat, val->dat.str.c )) )
					{
						if ( op[1].op == selOps::SEL_DONE )
						{
							cAddToArrayUnique( instance, nodeList, static_cast<VAR_OBJ *>(sib) );
						} else
						{
							processChildren( instance, obj, nodeList, op + 1 );
						}
					}
				}
				sib = (*static_cast<VAR_OBJ *>(sib))["NEXTSIBLING"];
			}
			break;
		case selOps::SEL_MATCH_ATTR_PRESENT:
			// save the result location
			resSave = (instance)->result;

			cNamedNodeMapGetNamedItem( instance, static_cast<VAR_OBJ *>((*obj)["ATTRIBUTES"]), op->dat );

			if ( TYPE( &(instance)->result ) != slangType::eNULL )
			{
				if ( op[1].op == selOps::SEL_DONE )
				{
					cAddToArrayUnique( instance, nodeList, obj );
				} else
				{
					processChildren( instance, obj, nodeList, op + 1 );
				}
			}
			// restore result
			(instance)->result = resSave;

			// descend and reprocess from descendant match
			while ( op->op != selOps::SEL_MATCH_DESC )
			{
				op--;
			}
			processChildren( instance, obj, nodeList, op );
			return 1;
		case selOps::SEL_MATCH_ATTR_EQUAL:
			// save the result location
			resSave = (instance)->result;

			cNamedNodeMapGetNamedItem( instance, static_cast<VAR_OBJ *>((*obj)["attributes"]), op->dat );

			if ( TYPE( &(instance)->result ) != slangType::eNULL )
			{
				if ( !strccmp( (instance)->result.dat.str.c, op->dat2 ) )
				{
					if ( op[1].op == selOps::SEL_DONE )
					{
						cAddToArrayUnique( instance, nodeList, obj );
					} else
					{
						processChildren( instance, obj, nodeList, op + 1 );
					}
				}
			}
			// restore result
			(instance)->result = resSave;

			// descend and reprocess from descendant match
			while ( op->op != selOps::SEL_MATCH_DESC )
			{
				op--;
			}
			processChildren( instance, obj, nodeList, op );
			return 1;
		case selOps::SEL_MATCH_ATTR_LIST:
			// save the result location
			resSave = (instance)->result;

			cNamedNodeMapGetNamedItem ( instance, static_cast<VAR_OBJ *>((*obj)["attributes"]), op->dat );

			// TODO: get rid of token crap
			if ( TYPE( &(instance)->result ) != slangType::eNULL )
			{
				nToken = _numtoken( (instance)->result.dat.str.c, " " );
				for ( loop = 0; loop < nToken; loop++ )
				{
					_tokenn( (instance)->result.dat.str.c, " ", loop + 1, token, sizeof( token ) );
					if ( !strccmp( token, op->dat2 ) )
					{
						if ( op[1].op == selOps::SEL_DONE )
						{
							cAddToArrayUnique( instance, nodeList, obj );
						} else
						{
							processChildren( instance, obj, nodeList, op + 1 );
						}
						break;
					}
				}
			}
			// restore result
			(instance)->result = resSave;

			// descend and reprocess from descendant match
			while ( op->op != selOps::SEL_MATCH_DESC )
			{
				op--;
			}
			processChildren( instance, obj, nodeList, op );
			return 1;
		default:
			break;
	}
	// descend and reprocess from descendant match
	while ( (op->op != selOps::SEL_MATCH_DESC) && (op->op != selOps::SEL_START) )
	{
		op--;
	}
	if ( op->op == selOps::SEL_START )
	{
		op++;
	}
	processChildren( instance, static_cast<VAR_OBJ *>(parent), nodeList, op );
	return 1;
}

int cDocumentSelect( VAR_OBJ *self, class vmInstance *instance, char const *sel )
{

	int		 nOps;
	OP		 op[32]{};
	char	*tmpC;
	int		 tmpLen;

	nOps = 0;

	// so we don't go beyond start when recovering from a match child failure
	op[nOps++].op = selOps::SEL_START;

	// process initial start selector... this is always a tag match???
	op[nOps].op = selOps::SEL_MATCH_DESC;

	// skip any leading spaces
	while ( _isspace( sel ) ) sel++;

	goto process_tag;

	while ( *sel && (nOps < 127) )
	{
		switch ( *sel )
		{
			case '>':
				op[nOps].op = selOps::SEL_MATCH_CHILD;
				break;
			case ':':
				sel++;
				if ( !memcmpi( sel, "first-child", 11 ) )
				{
					// modifies previous op
					nOps--;

					sel += 11;
					op[nOps].op = selOps::SEL_MATCH_FIRST_CHILD;
				} else
				{
					throw errorNum::scINVALID_PARAMETER;
				}
				break;
			case '+':
				op[nOps].op = selOps::SEL_MATCH_SIB;
				break;
			case '#':
				op[nOps].op = selOps::SEL_MATCH_TAG_ID;
				break;
			case '[':
				op[nOps].op = selOps::SEL_MATCH_ATTR_PRESENT;
				break;
			default:
				if ( _issymbolb( sel ) || (*sel == '*') )
				{
					op[nOps].op = selOps::SEL_MATCH_DESC;
				} else
				{
					throw errorNum::scINVALID_PARAMETER;
				}
				sel--;	// will be incremented later to correct position
				break;
		}
		// skip over the operation
		sel++;

		process_tag:

		// skip any spaces
		while ( _isspace( sel ) ) sel++;

		switch ( op[nOps].op )
		{
			case selOps::SEL_MATCH_DESC:
			case selOps::SEL_MATCH_CHILD:
			case selOps::SEL_MATCH_SIB:
				tmpC = op[nOps].dat;
				tmpLen = sizeof( op[nOps].dat ) - 1;

				if ( *sel == '*' )
				{
					*tmpC++ = *sel++;
					*tmpC = 0;
				} else
				{
					while ( _issymbolb( sel ) && tmpLen )
					{
						*tmpC++ = *sel++;
						tmpLen--;
					}
					*tmpC = 0;
				}
				break;
			case selOps::SEL_MATCH_ATTR_PRESENT:
				tmpC = op[nOps].dat;
				tmpLen = sizeof ( op[nOps].dat ) - 1;

				while ( _issymbolb ( sel ) && tmpLen )
				{
					*tmpC++ = *sel++;
					tmpLen--;
				}
				*tmpC = 0;

				// skip white space
				while ( _isspace ( sel ) ) sel++;

				if ( *sel == '=' )
				{
					op[nOps].op = selOps::SEL_MATCH_ATTR_EQUAL;
					sel++;
				} else if ( *sel == '~' && *(sel + 1) == '=' )
				{
					op[nOps].op = selOps::SEL_MATCH_ATTR_LIST;
					sel += 2;
				}
				if ( op[nOps].op != selOps::SEL_MATCH_ATTR_PRESENT )
				{
					// skip any spaces
					while ( _isspace ( sel ) ) sel++;

					if ( *sel != '"' )
					{
						throw errorNum::scINVALID_PARAMETER;
					}
					sel++;

					tmpC = op[nOps].dat2;
					tmpLen = sizeof ( op[nOps].dat2 ) - 1;
					while ( _issymbolb ( sel ) && tmpLen )
					{
						*tmpC++ = *sel++;
						tmpLen--;
					}
					*tmpC = 0;

					if ( *sel != '"' )
					{
						throw errorNum::scINVALID_PARAMETER;
					}
					sel++;
				}
				// skip any spaces
				while ( _isspace ( sel ) ) sel++;

				if ( *sel != ']' )
				{
					throw errorNum::scINVALID_PARAMETER;
				}
				sel++;
				break;
			case selOps::SEL_MATCH_FIRST_CHILD:
				sel--;
				break;
			default:
				break;
		}
		// skip any spaces
		while ( _isspace ( sel ) ) sel++;

		nOps++;
	}

	op[nOps].op = selOps::SEL_DONE;

	processChildren ( instance, self, classNew ( instance, "NODELIST", 0 ), op + 1 );

	return 1;
}

VAR *cGetNodeMapNameByIndex ( class vmInstance *instance, VAR_OBJ *obj, int64_t index )
{
	VAR			*array;
	int64_t		 indicie[2]{};

	instance->result.type = slangType::eNULL;

	if ( !(array = classIVarAccess ( obj, "ARRAY" )) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	indicie[0] = index;
	indicie[1] = 1;

	if ( _arrayIsElem ( instance, array, 2, indicie ) )
	{
		return _arrayGet ( instance, array, 2, indicie );
	}
	return nullptr;
}

VAR *cGetNodeMapValueByIndex ( class vmInstance *instance, VAR_OBJ *obj, int64_t index )
{
	VAR			*array;
	int64_t		 indicie[2]{};

	instance->result.type = slangType::eNULL;

	if ( !(array = classIVarAccess ( obj, "ARRAY" )) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	indicie[0] = index;
	indicie[1] = 1;

	if ( _arrayIsElem ( instance, array, 2, indicie ) )
	{
		return _arrayGet ( instance, array, 2, indicie );
	}
	return nullptr;
}

void builtinDOMInit ( class vmInstance *instance, ::opFile *file )
{
	/************************** NODE ****************************/

	REGISTER( instance, file );
		CLASS ( "NODE" );
			ACCESS ( "DEFAULT", cNodeDefaultFind );
			IVAR ( "NODENAME" );
			IVAR ( "NODEVALUE" );
			IVAR ( "NODETYPE" );
			IVAR ( "PARENTNODE" );
			IVAR ( "CHILDNODES" );
			IVAR ( "FIRSTCHILD" );
			IVAR ( "LASTCHILD" );
			IVAR ( "PREVIOUSSIBLING" );
			IVAR ( "NEXTSIBLING" );
			IVAR ( "ATTRIBUTES" );
			IVAR ( "OWNERDOCUMENT" );
			IVAR ( "nSpaceURI" );
			IVAR ( "PREFIX" );
			IVAR ( "LOCALNAME" );
			METHOD ( "HASCHILDNODES", cNodeHasChild );
			METHOD ( "ISSUPPORTED", cNodeIsSupported );
			METHOD ( "HASATTRIBUTES", cNodeHasAttributes );
			METHOD ( "GETELEMENTBYNAME", cNodeFindElement );
			METHOD ( "GETELEMENTSBYNAME", cNodeFindElements );
			METHOD ( "GETELEMENTSBYNAMENS", cNodeFindElementsNS );
			METHOD ( "GETELEMENTSBYTAG", cNodeFindElementsTag );
			METHOD ( "GETELEMENTSBYTAGNS", cNodeFindElementsTagNS );
		
		END;
			/************************** DOCUMENT ****************************/

		CLASS ( "DOCUMENT" );
			INHERIT ( "NODE" );
			IVAR ( "DOCUMENTTYPE" );
			IVAR ( "IMPLEMENTATION" );
			IVAR ( "ELEMENT" );
			METHOD ( "CREATEELEMENT", cDocumentCreateElement );
			METHOD ( "CREATEELEMENTNS", cDocumentCreateElementNS );
			METHOD ( "CREATETEXTNODE", cDocumentCreateTextNode );
			METHOD ( "CREATECOMMENT", cDocumentCreateComment );
			METHOD ( "CREATECDATASECTION", cDocumentCreateCDataSection );
			METHOD ( "CREATEPROCESSINGINSTRUCTION", cDocumentCreateProcessingInstruction );
			METHOD ( "CREATEATTRIBUTE", cDocumentCreateAttribute );
			METHOD ( "CREATEENTITYREFERENCE", cDocumentCreateEntityReference );
			METHOD ( "GETELEMENTSBYTAGNAME", cDocumentGetElementsByTagName );
			METHOD ( "IMPORTNODE", cDocumentImportNode );
			METHOD ( "CREATEATTRIBUTENS", cDocumentCreateAttributeNS );
			METHOD ( "GETELEMENTSBYTAGNAMENS", cDocumentGetElementsByTagNameNS );
			METHOD ( "GETELEMENTSBYID", cDocumentGetElementsByID );
			METHOD ( "SELECT", cDocumentSelect );
		END;

		/************************** NODE LIST ****************************/

		CLASS ( "NODELIST" );
			IVAR ( "ARRAY" );
			IVAR ( "LENGTH" );
			OP ( "[", cArrayAccess );
			METHOD ( "ITEM", cArrayAccess );
		END;

		/************************** NAMED NODE MAP **********************/

		CLASS ( "NAMEDNODEMAP" );
			IVAR ( "ARRAY" );
			IVAR ( "LENGTH" );
			OP ( "[", cArrayAccess );
			METHOD ( "ITEM", cArrayAccess );
			METHOD ( "GETNAMEDITEM", cNamedNodeMapGetNamedItem );
			METHOD ( "GETNAMEDITEMNS", cNamedNodeMapGetNamedItemNS );
		END;


		/******************************** NODE TYPES ******************************/

		/******************************* CHARACTER DATA ***************************/

		CLASS ( "CHARACTERDATA" );
			INHERIT ( "NODE" );
			IVAR ( "DATA" );
			ACCESS ( "LENGTH", cCharacterDataLength );
		END;

		/******************************* ATTR ***************************/

		CLASS ( "ATTR" );
			INHERIT ( "NODE" );
			IVAR ( "DATA" );
			ACCESS ( "LENGTH", cCharacterDataLength );
		END;

		/******************************* Element ***************************/

		CLASS ( "ELEMENT" );
			INHERIT ( "NODE" );
			ACCESS ( "VALUE", cElementPseudoValueAccess );
			IVAR ( "TAGNAME" );
			METHOD ( "GETATTRIBUTE", cElementGetAttribute );
			METHOD ( "GETATTRIBUTENS", cElementGetAttributeNS );
		END;

		/******************************* Comment ***************************/

		CLASS ( "COMMENT" );
			INHERIT ( "CHARACTERDATA" );
		END;

		/******************************* Text ***************************/

		CLASS ( "TEXT" );
			INHERIT ( "CHARACTERDATA" );
		END;

		/************************ Processing Instruction ******************/

		CLASS ( "PROCESSINGINSTRUCTION" );
			INHERIT ( "NODE" );
			IVAR ( "DATA" );
			ACCESS ( "LENGTH", cCharacterDataLength );
		END;

		/******************************* Notation ***************************/

		CLASS ( "NOTATION" );
			INHERIT ( "NODE" );
			IVAR ( "PUBLICID" );
			IVAR ( "SYSTEMID" );
		END;

		/******************************* HTML ELEMENT ***************************/

		CLASS ( "HTMLELEMENT" );
			INHERIT ( "ELEMENT" );
			IVAR ( "CLASS" );
			IVAR ( "DIR" );
			IVAR ( "ID" );
			IVAR ( "LANG" );
			IVAR ( "TITLE" );
			IVAR ( "STYLE" );
		END;


		/******************************* HTML COLLECTION ***************************/

		CLASS ( "HTMLCOLLECTION" );
			OP ( "[", cArrayAccess );
			IVAR ( "ARRAY" );
			IVAR ( "LENGTH" );
			METHOD ( "ITEM", cArrayAccess );
			METHOD ( "NAMEDITEM", cFindNodeName );
		END;

		/******************************* HTML ***************************/

		CLASS ( "HTMLHTMLELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "VERSION" );
		END;

		/******************************* HEAD ***************************/

		CLASS ( "HTMLHEADELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "PROFILE" );
		END;

		/******************************* LINK  ***************************/

		CLASS ( "HTMLLINKELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "DISABLED" );
			IVAR ( "CHARSET" );
			IVAR ( "HREF" );
			IVAR ( "HREFLANG" );
			IVAR ( "MEDIA" );
			IVAR ( "REL" );
			IVAR ( "REV" );
			IVAR ( "TARGET" );
			IVAR ( "TYPE" );
		END;

		/******************************* TITLE ***************************/

		CLASS ( "HTMLTITLEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "TEXT" );
		END;

		/******************************* META ***************************/

		CLASS ( "HTMLMETAELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "CONTENT" );
			IVAR ( "HTTPEQUIV" );
			IVAR ( "NAME" );
			IVAR ( "SCHEME" );
		END;

		/******************************* BASE ***************************/

		CLASS ( "HTMLBASEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "BASE" );
			IVAR ( "TARGET" );
		END;

		/******************************* IS INDEX ***************************/

		CLASS ( "HTMLISINDEXELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FORM" );
			IVAR ( "PROMPT" );
		END;

		/******************************* STYLE ***************************/

		CLASS ( "HTMLSTYLEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "DISABLED" );
			IVAR ( "MEDIA" );
			IVAR ( "TYPE" );
		END;

		/******************************* BODY ***************************/

		CLASS ( "HTMLBODYELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALINK" );
			IVAR ( "BACKGROUND" );
			IVAR ( "BGCOLOR" );
			IVAR ( "LINK" );
			IVAR ( "TEXT" );
			IVAR ( "VLINK" );
		END;

		/******************************* FORM ***************************/

		CLASS ( "HTMLFORMELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ELEMENTS" );
			IVAR ( "LENGTH" );
			IVAR ( "NAME" );
			IVAR ( "ACCEPTCHARSET" );
			IVAR ( "ACTION" );
			IVAR ( "ENCTYPE" );
			IVAR ( "METHOD" );
			IVAR ( "TARGET" );
		END;

		/******************************* SELECT ***************************/

		CLASS ( "HTMLSELECTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "TYPE" );
			IVAR ( "SELECTEDINDEX" );
			IVAR ( "VALUE" );
			IVAR ( "LENGTH" );
			IVAR ( "FORM" );
			IVAR ( "OPTIONS" );
			IVAR ( "DISABLED" );
			IVAR ( "MULTIPLE" );
			IVAR ( "NAME" );
			IVAR ( "SIZE" );
			IVAR ( "TABINDEX" );
			IVAR ( "VERSION" );
		END;

		/******************************* OPT GROUP ***************************/

		CLASS ( "HTMLOPTGROUPELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "DISABLED" );
			IVAR ( "LABEL" );
		END;

		/******************************* OPTION ***************************/

		CLASS ( "HTMLOPTIONELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FORM" );
			IVAR ( "DEFAULTSELECTED" );
			IVAR ( "TEXT" );
			IVAR ( "INDEX" );
			IVAR ( "DISABLED" );
			IVAR ( "LABEL" );
			IVAR ( "SELECTED" );
			IVAR ( "VALUE" );
		END;

		/******************************* INPUT ***************************/

		CLASS ( "HTMLINPUTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "DEFAULTVALUE" );
			IVAR ( "DEFAULTCHECKED" );
			IVAR ( "FORM" );
			IVAR ( "ACCEPT" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "ALIGN" );
			IVAR ( "ALT" );
			IVAR ( "CHECKED" );
			IVAR ( "DISABLED" );
			IVAR ( "MAXLENGTH" );
			IVAR ( "NAME" );
			IVAR ( "READONLY" );
			IVAR ( "SIZE" );
			IVAR ( "SRC" );
			IVAR ( "TABINDEX" );
			IVAR ( "TYPE" );
			IVAR ( "USEMAP" );
			IVAR ( "VALUE" );
		END;

		/******************************* TEXT AREA ***************************/

		CLASS ( "HTMLTEXTARRAY_ELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "DEFAULTVALUE" );
			IVAR ( "FORM" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "COLS" );
			IVAR ( "DISABLED" );
			IVAR ( "NAME" );
			IVAR ( "READONLY" );
			IVAR ( "ROWS" );
			IVAR ( "TABINDEX" );
			IVAR ( "TYPE" );
			IVAR ( "VALUE" );
		END;

		/******************************* BUTTON ***************************/

		CLASS ( "HTMLBUTTONELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FORM" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "DISABLED" );
			IVAR ( "NAME" );
			IVAR ( "TABINDEX" );
			IVAR ( "TYPE" );
			IVAR ( "VALUE" );
		END;

		/******************************* LABEL ***************************/

		CLASS ( "HTMLLABELELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FORM" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "HTMLFOR" );
		END;

		/******************************* FIELD SET ***************************/

		CLASS ( "HTMLFIELDSETELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FORM" );
		END;

		/******************************* LEGEND ***************************/

		CLASS ( "HTMLLEGENDELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FORM" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "ALIGN" );
		END;

		/******************************* ULIST ***************************/

		CLASS ( "HTMLULISTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "COMPACT" );
			IVAR ( "TYPE" );
		END;

		/******************************* OLIST ***************************/

		CLASS ( "HTMLOLISTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "COMPACT" );
			IVAR ( "START" );
			IVAR ( "TYPE" );
		END;

		/******************************* DLIST ***************************/

		CLASS ( "HTMLDLISTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "COMPACT" );
		END;

		/******************************* DIRECTORY ***************************/

		CLASS ( "HTMLDIRECTORYELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "COMPACT" );
		END;

		/******************************* MENU ***************************/

		CLASS ( "HTMLMENUELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "VERCOMPACTSION" );
		END;

		/******************************* LI ***************************/

		CLASS ( "HTMLLIELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "TYPE" );
			IVAR ( "VALUE" );
		END;

		/******************************* DIV ***************************/

		CLASS ( "HTMLDIVELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
		END;

		/******************************* PARAGRAPH ***************************/

		CLASS ( "HTMLPARAGRAPHELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
		END;

		/******************************* HEADING ***************************/

		CLASS ( "HTMLHEADINGELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
		END;

		/******************************* QUOTE ***************************/

		CLASS ( "HTMLQUOTEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "CITE" );
		END;

		/******************************* PRE ***************************/

		CLASS ( "HTMLPREELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "WIDTH" );
		END;

		/******************************* BR ***************************/

		CLASS ( "HTMLBRELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "CLEAR" );
		END;

		/******************************* BASE FONT ***************************/

		CLASS ( "HTMLBASEFONTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "COLOR" );
			IVAR ( "FACE" );
			IVAR ( "SIZE" );
		END;

		/******************************* FONT ***************************/
	
		CLASS ( "HTMLFONTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "COLOR" );
			IVAR ( "FACE" );
			IVAR ( "SIZE" );
		END;

		/******************************* HR ***************************/

		CLASS ( "HTMLHRELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
			IVAR ( "NOSHADE" );
			IVAR ( "SIZE" );
			IVAR ( "WIDTH" );
		END;

		/******************************* INS ***************************/

		CLASS ( "HTMLINSELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "CITE" );
			IVAR ( "DATETIME" );
		END;

		/******************************* DEL ***************************/

		CLASS ( "HTMLDELELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "CITE" );
			IVAR ( "DATETIME" );
		END;

		/******************************* ANCHOR ***************************/

		CLASS ( "HTMLANCHORELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "CHARSET" );
			IVAR ( "COORDS" );
			IVAR ( "HREF" );
			IVAR ( "HREFLANG" );
			IVAR ( "NAME" );
			IVAR ( "REL" );
			IVAR ( "REV" );
			IVAR ( "SHAPE" );
			IVAR ( "TABINDEX" );
			IVAR ( "TARGET" );
			IVAR ( "TYPE" );
		END;

		/******************************* IMAGE ***************************/

		CLASS ( "HTMLIMAGEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "NAME" );
			IVAR ( "ALIGN" );
			IVAR ( "ALT" );
			IVAR ( "BORDER" );
			IVAR ( "HEIGHT" );
			IVAR ( "HSPACE" );
			IVAR ( "ISMAP" );
			IVAR ( "LONGDESC" );
			IVAR ( "SRC" );
			IVAR ( "USEMAP" );
			IVAR ( "VSPACE" );
			IVAR ( "WIDTH" );
		END;

		/******************************* OBJECT ***************************/

		CLASS ( "HTMLOBJECTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FORM" );
			IVAR ( "CODE" );
			IVAR ( "ALIGN" );
			IVAR ( "ARCHIVE" );
			IVAR ( "BORDER" );
			IVAR ( "CODEBASE" );
			IVAR ( "CODETYPE" );
			IVAR ( "DATA" );
			IVAR ( "DECLARE" );
			IVAR ( "HEIGHT" );
			IVAR ( "HSPACE" );
			IVAR ( "NAME" );
			IVAR ( "STANDBY" );
			IVAR ( "TABINDEX" );
			IVAR ( "TYPE" );
			IVAR ( "USEMAP" );
			IVAR ( "VSPACE" );
			IVAR ( "WIDTH" );
		END;

		/******************************* PARAM ***************************/

		CLASS ( "HTMLPARAMELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "NAME" );
			IVAR ( "TYPE" );
			IVAR ( "VALUE" );
			IVAR ( "VALUETYPE" );
		END;

		/******************************* APPLET ***************************/

		CLASS ( "HTMLAPPLETELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
			IVAR ( "ALT" );
			IVAR ( "ARCHIVE" );
			IVAR ( "CODE" );
			IVAR ( "CODEBASE" );
			IVAR ( "HEIGHT" );
			IVAR ( "HSPACE" );
			IVAR ( "NAME" );
			IVAR ( "OBJECT" );
			IVAR ( "VSPACE" );
			IVAR ( "WIDTH" );
		END;

		/******************************* MAP ***************************/

		CLASS ( "HTMLMAPELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "ALT" );
			IVAR ( "COORDS" );
			IVAR ( "HREF" );
			IVAR ( "NOHREF" );
			IVAR ( "SHAPE" );
			IVAR ( "TABINDEX" );
			IVAR ( "TARGET" );
		END;

		/******************************* AREA ***************************/

		CLASS ( "HTMLARRAY_ELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ACCESSKEY" );
			IVAR ( "ALT" );
			IVAR ( "COORDS" );
			IVAR ( "HREF" );
			IVAR ( "NOHREF" );
			IVAR ( "SHAPE" );
			IVAR ( "TABINDEX" );
			IVAR ( "TARGET" );
		END;

		/******************************* SCRIPT ***************************/

		CLASS ( "HTMLSCRIPTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "TEXT" );
			IVAR ( "HTMLFOR" );
			IVAR ( "EVENT" );
			IVAR ( "CHARSET" );
			IVAR ( "DEFER" );
			IVAR ( "SRC" );
			IVAR ( "TYPE" );
		END;

		/******************************* TABLE ***************************/

		CLASS ( "HTMLTABLEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "CAPTION" );
			IVAR ( "THEAD" );
			IVAR ( "TFOOT" );
			IVAR ( "ROWS" );
			IVAR ( "TBODIES" );
			IVAR ( "ALIGN" );
			IVAR ( "BGCOLOR" );
			IVAR ( "BORDER" );
			IVAR ( "CELLPADDING" );
			IVAR ( "CELLSPACING" );
			IVAR ( "FRAME" );
			IVAR ( "RULES" );
			IVAR ( "SUMMARY" );
			IVAR ( "WIDTH" );
		END;

		/******************************* SPAN ***************************/

		CLASS ( "HTMLSPANELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* SUP ***************************/

		CLASS ( "HTMLSUPELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* SUB ***************************/

		CLASS ( "HTMLSUBELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* BDO ***************************/

		CLASS ( "HTMLBDOELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* TT ***************************/

		CLASS ( "HTMLTTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* I ***************************/

		CLASS ( "HTMLIELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* B ***************************/

		CLASS ( "HTMLBELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* U ***************************/

		CLASS ( "HTMLUELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* S ***************************/

		CLASS ( "HTMLSELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* STRIKE ***************************/

		CLASS ( "HTMLSTRIKEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* BIG ***************************/

		CLASS ( "HTMLBIGELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* SMALL  ***************************/

		CLASS ( "HTMLSMALLELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* EM ***************************/

		CLASS ( "HTMLEMELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* STRONG ***************************/

		CLASS ( "HTMLSTRONGELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* DFN ***************************/

		CLASS ( "HTMLDFNELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* CODE ***************************/

		CLASS ( "HTMLCODEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* SAMP ***************************/

		CLASS ( "HTMLSAMPELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* KBD ***************************/

		CLASS ( "HTMLKBDELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* VAR ***************************/

		CLASS ( "HTMLVARELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* CITE ***************************/

		CLASS ( "HTMLCITEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* ACRONYM ***************************/

		CLASS ( "HTMLACRONYMELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* ABBR  ***************************/

		CLASS ( "HTMLABBRELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* DD ***************************/

		CLASS ( "HTMLDDELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* DT  ***************************/

		CLASS ( "HTMLDTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* NOFRAMES ***************************/

		CLASS ( "HTMLNOFRAMESELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* NOSCRIPT  ***************************/

		CLASS ( "HTMLNOSCRIPTELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* ADDRESS ***************************/

		CLASS ( "HTMLADDRESSELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* CENTER  ***************************/

		CLASS ( "HTMLCENTERELEMENT" );
			INHERIT ( "HTMLELEMENT" );
		END;

		/******************************* TABLE CAPTION ***************************/

		CLASS ( "HTMLTABLECAPTIONELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
		END;


		/******************************* TABLE COL ***************************/

		CLASS ( "HTMLTABLECOLELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
			IVAR ( "CH" );
			IVAR ( "CHOFF" );
			IVAR ( "SPAN" );
			IVAR ( "VALIGN" );
			IVAR ( "WIDTH" );
		END;

		/******************************* TABLE SECTION ***************************/

		CLASS ( "HTMLTABLESECTIONELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
			IVAR ( "CH" );
			IVAR ( "CHOFF" );
			IVAR ( "VALIGN" );
			IVAR ( "ROWS" );
		END;


		/******************************* TABLE ROW ***************************/

		CLASS ( "HTMLTABLEROWELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ROWINDEX" );
			IVAR ( "SECTIONROWINDEX" );
			IVAR ( "CELLS" );
			IVAR ( "ALIGN" );
			IVAR ( "BGCOLOR" );
			IVAR ( "CH" );
			IVAR ( "CHOFF" );
			IVAR ( "VALIGN" );
		END;


		/******************************* TABLE CELL ***************************/

		CLASS ( "HTMLTABLECELLELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "CELLINDEX" );
			IVAR ( "ABBR" );
			IVAR ( "ALIGN" );
			IVAR ( "AXIS" );
			IVAR ( "BGCOLOR" );
			IVAR ( "CH" );
			IVAR ( "CHOFF" );
			IVAR ( "COLSPAN" );
			IVAR ( "HEADERS" );
			IVAR ( "HEIGHT" );
			IVAR ( "NOWRAP" );
			IVAR ( "ROWSPAN" );
			IVAR ( "SCOPE" );
			IVAR ( "VALIGN" );
			IVAR ( "WIDTH" );
		END;


		/******************************* FRAME SET ***************************/

		CLASS ( "HTMLFRAMESETELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ROWS" );
			IVAR ( "COLS" );
		END;

		/******************************* FRAME ***************************/

		CLASS ( "HTMLFRAMEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "FRAMEBORDER" );
			IVAR ( "LONGDESC" );
			IVAR ( "MARGINHEIGHT" );
			IVAR ( "MARGINWIDTH" );
			IVAR ( "NAME" );
			IVAR ( "NORESIZE" );
			IVAR ( "SCROLLING" );
			IVAR ( "SRC" );
		END;

		/******************************* LI FRAME ELEMENT ***************************/

		CLASS ( "HTMLLIFRAMEELEMENT" );
			INHERIT ( "HTMLELEMENT" );
			IVAR ( "ALIGN" );
			IVAR ( "FRAMEBORDER" );
			IVAR ( "HEIGHT" );
			IVAR ( "LONGDESC" );
			IVAR ( "MARGINHEIGHT" );
			IVAR ( "MARGINWIDTH" );
			IVAR ( "NAME" );
			IVAR ( "SCROLLING" );
			IVAR ( "SRC" );
			IVAR ( "WIDTH" );
		END;

			/************************** HTML DOCUMENT ****************************/

		CLASS ( "HTMLDOCUMENT" );
			REQUIRE(	"sparseArrayEnumerator",
						"NODE",
						"DOCUMENT",
						"NODELIST",
						"NAMEDNODEMAP",
						"CHARACTERDATA",
						"ATTR",
						"ELEMENT",
						"COMMENT",
						"TEXT",
						"PROCESSINGINSTRUCTION",
						"NOTATION",
						"HTMLELEMENT",
						"HTMLCOLLECTION",
						"HTMLHTMLELEMENT",
						"HTMLHEADELEMENT",
						"HTMLLINKELEMENT",
						"HTMLTITLEELEMENT",
						"HTMLMETAELEMENT",
						"HTMLBASEELEMENT",
						"HTMLISINDEXELEMENT",
						"HTMLSTYLEELEMENT",
						"HTMLBODYELEMENT",
						"HTMLFORMELEMENT",
						"HTMLSELECTELEMENT",
						"HTMLOPTGROUPELEMENT",
						"HTMLOPTIONELEMENT",
						"HTMLINPUTELEMENT",
						"HTMLTEXTARRAY_ELEMENT",
						"HTMLBUTTONELEMENT",
						"HTMLLABELELEMENT",
						"HTMLFIELDSETELEMENT",
						"HTMLLEGENDELEMENT",
						"HTMLULISTELEMENT",
						"HTMLOLISTELEMENT",
						"HTMLDLISTELEMENT",
						"HTMLDIRECTORYELEMENT",
						"HTMLMENUELEMENT",
						"HTMLLIELEMENT",
						"HTMLDIVELEMENT",
						"HTMLPARAGRAPHELEMENT",
						"HTMLHEADINGELEMENT",
						"HTMLQUOTEELEMENT",
						"HTMLPREELEMENT",
						"HTMLBRELEMENT",
						"HTMLBASEFONTELEMENT",
						"HTMLFONTELEMENT",
						"HTMLHRELEMENT",
						"HTMLINSELEMENT",
						"HTMLDELELEMENT",
						"HTMLANCHORELEMENT",
						"HTMLIMAGEELEMENT",
						"HTMLOBJECTELEMENT",
						"HTMLPARAMELEMENT",
						"HTMLAPPLETELEMENT",
						"HTMLMAPELEMENT",
						"HTMLARRAY_ELEMENT",
						"HTMLSCRIPTELEMENT",
						"HTMLTABLEELEMENT",
						"HTMLSPANELEMENT",
						"HTMLSUPELEMENT",
						"HTMLSUBELEMENT",
						"HTMLBDOELEMENT",
						"HTMLTTELEMENT",
						"HTMLIELEMENT",
						"HTMLBELEMENT",
						"HTMLUELEMENT",
						"HTMLSELEMENT",
						"HTMLSTRIKEELEMENT",
						"HTMLBIGELEMENT",
						"HTMLSMALLELEMENT",
						"HTMLEMELEMENT",
						"HTMLSTRONGELEMENT",
						"HTMLDFNELEMENT",
						"HTMLCODEELEMENT",
						"HTMLSAMPELEMENT",
						"HTMLKBDELEMENT",
						"HTMLVARELEMENT",
						"HTMLCITEELEMENT",
						"HTMLACRONYMELEMENT",
						"HTMLABBRELEMENT",
						"HTMLDDELEMENT",
						"HTMLDTELEMENT",
						"HTMLNOFRAMESELEMENT",
						"HTMLNOSCRIPTELEMENT",
						"HTMLADDRESSELEMENT",
						"HTMLCENTERELEMENT",
						"HTMLTABLECAPTIONELEMENT",
						"HTMLTABLECOLELEMENT",
						"HTMLTABLESECTIONELEMENT",
						"HTMLTABLEROWELEMENT",
						"HTMLTABLECELLELEMENT",
						"HTMLFRAMESETELEMENT",
						"HTMLFRAMEELEMENT",
						"HTMLLIFRAMEELEMENT",
						"HTMLDOCUMENT"
					);
			PROTECTED
				METHOD( "NEW", cHtmlDocumentNew );
			PUBLIC
				INHERIT ( "DOCUMENT" );
				IVAR ( "TITLE" );
				IVAR ( "REFERER" );
				IVAR ( "DOMAIN" );
				IVAR ( "URL" );
				IVAR ( "BODY" );
				IVAR ( "IMAGES" );
				IVAR ( "APPLETS" );
				IVAR ( "LINKS" );
				IVAR ( "FORMS" );
				IVAR ( "ANCHORS" );
				IVAR ( "COOKIE" );
				METHOD ( "GETPOSTDATA", cHtmlDocumentGetPostData );
		END;

			/************************** XML DOCUMENT ****************************/

		CLASS ( "XMLDOCUMENT" );
			REQUIRE (	"sparseArrayEnumerator",
						"NODE",
						"DOCUMENT",
						"NODELIST",
						"NAMEDNODEMAP",
						"CHARACTERDATA",
						"ATTR",
						"ELEMENT",
						"COMMENT",
						"TEXT",
						"PROCESSINGINSTRUCTION",
						"NOTATION"
					);
			INHERIT ( "DOCUMENT" );
			PROTECTED
				METHOD( "NEW", cXMLDocumentNew, DEF ( 3, "0" ) );
			PUBLIC
				IVAR ( "TITLE" );

				CODE ( R"sl( 
							method xml2Obj ( xml )
							{
								if ( xml.nodeType == 1 )
								{
									if ( xml.firstChild )
									{
										local textChild = 0, cDataChild = 0, hasElementChild = false;

										for ( local n = xml.firstChild; n; n = n.nextSibling )
										{
											switch ( n.nodeType )
											{
												case 1:
													hasElementChild = true;
													break;
												case 3:
													textChild++;
													break;
												case 4:
													cDataChild++
													break;
											}
										}
										if ( hasElementChild )
										{
											local o = new aArray
											if ( xml.attributes.length )
											{
												foreach ( it in xml.attributes.array )
												{
													o["@"+ it[1]] = it[2];
												}		
											}
											for ( local n = xml.firstChild; n; n = n.nextSibling )	
											{
												if (n.nodeType == 3)
												{
													if ( o.has ( "#text" ) )
													{
														o["#text"] += n.nodeValue;
													} else
													{
														o["#text"] = n.nodeValue;
													}
												} else if (n.nodeType == 4)
												{
													if ( o.has ( "#cData" ) )
													{
														o["#cData"] += n.nodeValue;
													} else
													{
														o["#cData"] = n.nodeValue;
													}
												} else if (o.has ( n.nodeName ) ) 
												{  
													if ( type ( o[n.nodeName] ) == "A" )
													{
														o[n.nodeName][o[n.nodeName].len()+1] = xml2obj ( n );
													} else
													{
														o[n.nodeName] = [o[n.nodeName], xml2obj(n)];
													}
												} else
												{
													o[n.nodeName] = xml2obj(n);
												}
											}
											return o;
										} else if (textChild) 
										{ 
											if (!xml.attributes.length)
											{
												return xml.value;
											} else
											{
												local o = new aArray
												if ( xml.attributes.length )
												{
													foreach ( it in xml.attributes.array )
													{
														o["@"+ it[1]] = it[2];
													}		
												}
												o["#text"] = xml.value;
												return o;
											}
										} else if (cdataChild) 
										{
											if (!xml.attributes.length)
											{
												return xml.value;
											} else
											{
												local o = new aArray
												if ( xml.attributes.length )
												{
													foreach ( it in xml.attributes.array )
													{
														o["@"+ it[1]] = it[2];
													}		
												}
												o["#cData"] = xml.value;
												return o;
											}
										}
									} else
									{
										if (!xml.attributes.length)
										{
											return null;
										} else
										{
											local o = new aArray
											if ( xml.attributes.length )
											{
												foreach ( it in xml.attributes.array )
												{
													o["@"+ it[1]] = it[2];
												}		
											}
											return o;
										}
									}
								}
								return null;
							}

							method tojson ()
							{
								local curNode = firstChild;
								for ( ;; )
								{
									switch ( curNode.nodeType )
									{
										case 1:
											{
												local o = new aArray();
												o[curNode.nodeName] = xml2Obj ( curNode )
												return objToJson ( o );
											}
										default:
											curNode = curNode.nextSibling;
											break;
									}
								}
							}
						)sl"
					 );
		END;
	END;
}
