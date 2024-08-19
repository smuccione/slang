/*
SLANG support functions

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "bcInterpreter/op_variant.h"

enum class packElementType {
	packType_reference,
	packType_objectRoot,
	packType_object,
	packType_arrayRoot,
	packType_arraySparse,
	packType_arrayElem,
	packType_arrayFixed,
	packType_string,
	packType_long,
	packType_longLong,
	packType_double,
	packType_null,
	packType_codeBlock,
	packType_codeBlockRoot,
	packType_elemIndex,
};

enum class packVersion
{
	packVersion1 = (int)0xBABEFACE,
	packVersion2 = (int)0xCAFEBABE,
	packVersion3 = (int)0xCAFEBAB2,
	packVersion4 = (int)0x000B00BE
};

#pragma pack ( push, 1 )
struct PackHeader4
{
	packVersion	version;
	uint32_t	nVars;
	uint32_t	nXref;
	uint32_t	nClass;
	uint32_t	nLibraries;
	uint32_t	offsetClasses;
	uint32_t	offsetLibraries;
};
struct PackHeader3
{
	packVersion	version;
	uint32_t	nVars;
	uint32_t	offsetLibraries;
};
#pragma pack ( pop )

class PackParam
{
	public:
	BUFFER				 buff;
	BUFFER				 xRefBuff;
	class vmInstance	*instance = nullptr;

	PackParam() : buff ( 1024ull * 1024 ), xRefBuff ( 1024ull * 1024 )
	{
	}
	virtual ~PackParam()
	{
	}

	// delete the copy/move operators, this object should only be passed by reference
	PackParam( const PackParam &old ) = delete;
	PackParam( PackParam &&old ) = delete;
	PackParam &operator = ( const PackParam &old ) = delete;
	PackParam &operator = ( PackParam &&old ) = delete;
};

static void packData ( PackParam &packParam, VAR *var );

static void packDataCB ( VAR *var, void *param )
{
	packData ( *(PackParam *) param, var  );
}

static void packDataObj ( PackParam &packParam, VAR *obj )

{
	size_t			 nSymbolsOffset;
	uint32_t		 nSymbols;
	bcClass			*classDef;
	size_t			 nElem;

	classDef = obj->dat.obj.classDef;

	// make space for the element count
	nSymbolsOffset = bufferSize ( &packParam.buff );
	bufferAssume ( &packParam.buff, sizeof ( uint32_t ) );

	nSymbols = 0;
	
	for ( nElem = 0; nElem < classDef->nElements; nElem++ )
	{
		switch ( classDef->elements[nElem].type )
		{
			case fgxClassElementType::fgxClassType_iVar:
				// no reason to serialize NULL ivar's as they will default to NULL on unpack if not present
				if ( obj[classDef->elements[nElem].methodAccess.data].type != slangType::eNULL )
				{
					bufferPut8( &packParam.buff, (char)classDef->elements[nElem].type );
					bufferPut32( &packParam.buff, classDef->elements[nElem].fullNameLen );
					bufferWrite( &packParam.buff, classDef->elements[nElem].fullName, classDef->elements[nElem].fullNameLen );
					packData( packParam, &obj[classDef->elements[nElem].methodAccess.data] );
					nSymbols++;
				}
				break;
			case fgxClassElementType::fgxClassType_static:
				// no reason to serialize NULL ivar's as they will default to NULL on unpack if not present
				if ( obj[classDef->elements[nElem].methodAccess.data].type != slangType::eNULL )
				{
					bufferPut8( &packParam.buff, (char)classDef->elements[nElem].type );
					bufferPut32( &packParam.buff, classDef->elements[nElem].fullNameLen );
					bufferWrite( &packParam.buff, classDef->elements[nElem].fullName, classDef->elements[nElem].fullNameLen );
					packData( packParam, classDef->loadImage->globals[classDef->elements[nElem].methodAccess.data] );
					nSymbols++;
				}
				break;
			default:
				break;
		}
	}

	// fill in the element count
	*((uint32_t *)(bufferBuff ( &packParam.buff ) + nSymbolsOffset )) = nSymbols;
}

static void packData ( PackParam &packParam, VAR *var )
{
	size_t			  loop;
	bcClass			 *classDef;
	VAR				**xRef;
	uint32_t		  nXRef;

	xRef	= (VAR **)bufferBuff ( &packParam.xRefBuff );
	nXRef	= (uint32_t)bufferSize ( &packParam.xRefBuff ) / sizeof ( VAR ** );

	if ( !var )
	{
		bufferWrite ( &packParam.xRefBuff, &var, sizeof ( VAR * ) );  // NOLINT ( bugprone-sizeof-expression )
		bufferPut8 ( &packParam.buff, (char)packElementType::packType_null );
		return;
	}
	
	if ( (var->packIndex < nXRef) && (xRef[var->packIndex] == var) )
	{
		bufferPut8	( &packParam.buff, (char)packElementType::packType_elemIndex );
		bufferPut32	( &packParam.buff, var->packIndex );
		return;
	}

	var->packIndex = nXRef;
	bufferWrite ( &packParam.xRefBuff, &var, sizeof ( VAR *) );  // NOLINT ( bugprone-sizeof-expression )
	
	switch ( TYPE ( var ) )
	{
		case slangType::eREFERENCE:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_reference );
			packData ( packParam, var->dat.ref.obj );
			packData ( packParam, var->dat.ref.v );
			break;

		case slangType::eLONG:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_long);
			bufferPut64( &packParam.buff, var->dat.l );
			break;
			
		case slangType::eDOUBLE:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_double );
			bufferPutDouble( &packParam.buff, var->dat.d );
			break;
			
		case slangType::eNULL:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_null );
			break;
			
		case slangType::eSTRING:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_string );
			bufferPut32( &packParam.buff, (uint32_t)var->dat.str.len );
			bufferWrite ( &packParam.buff, var->dat.str.c, var->dat.str.len + 1);
			break;
			
		case slangType::eARRAY_ROOT:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_arrayRoot );
			packData ( packParam, var->dat.ref.obj );
			packData ( packParam, var->dat.ref.v );
			break;

		case slangType::eARRAY_SPARSE:
			bufferPut8	( &packParam.buff, (char)packElementType::packType_arraySparse );
			bufferPut64	( &packParam.buff, var->dat.aSparse.maxI );
			packData	( packParam, var->dat.aSparse.v );
			break;

		case slangType::eARRAY_ELEM:
			// no need for a reference here... this type is only single parent possible
			bufferUnWrite ( &packParam.xRefBuff, sizeof ( VAR * ) );  // NOLINT ( bugprone-sizeof-expression )

			while ( var )
			{
				bufferPut8 ( &packParam.buff, (char)packElementType::packType_arrayElem );
				bufferWrite ( &packParam.buff, &(var->dat.aElem.elemNum), sizeof ( var->dat.aElem.elemNum ) );
				
				packData ( packParam, var->dat.aElem.var );
				var = var->dat.aElem.next;
			}
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_null );
			break;

		case slangType::eARRAY_FIXED:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_arrayFixed );
			bufferPut64 ( &packParam.buff, var->dat.arrayFixed.startIndex );
			bufferPut64 ( &packParam.buff, var->dat.arrayFixed.endIndex );

			for ( loop = 0; loop < (size_t)(var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1); loop++ )
			{
				packData ( packParam, &var[loop + 1] );
			}
			break;

		case slangType::eCODEBLOCK_ROOT:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_codeBlockRoot );
			packData ( packParam, var->dat.ref.obj );
			break;
		case slangType::eCODEBLOCK:
			bufferPut8		( &packParam.buff, (char)packElementType::packType_codeBlock );
			bufferPut64		( &packParam.buff, var->dat.cb.cb->storageSize );							// code side
			bufferWrite		( &packParam.buff, var->dat.cb.cb, var->dat.cb.cb->storageSize );			// code itself

			for ( loop = 0; loop < static_cast<size_t>(var->dat.cb.cb->nSymbols) - var->dat.cb.cb->nParams; loop++ )
			{
				packData ( packParam, &var[loop + 1] );
			}
			break;
			
		case slangType::eOBJECT_ROOT:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_objectRoot );
			packData ( packParam, var->dat.ref.obj );
			packData ( packParam, var->dat.ref.v );		// this should resolve into a reference... so we can do inner references on pack
			break;

		case slangType::eOBJECT:
			bufferPut8 ( &packParam.buff, (char)packElementType::packType_object );

			classDef	= var->dat.obj.classDef;

			// mark it as being used
			packParam.instance->atomTable->markInUse ( classDef->loadImage->atom );

			bufferPut32 ( &packParam.buff, classDef->nameLen );
			bufferWrite ( &packParam.buff, classDef->name, classDef->nameLen );
			
			// call any user defined pre-packing routine to prepare for serialization
			classCallPack ( packParam.instance, var );

			// take care of any C pack callback
			if ( classDef->cPackCB )
			{
				int64_t	 sizeOffset;

				VAR_OBJ tmp(var, var);

				sizeOffset = (int64_t)bufferSize ( &packParam.buff );
				bufferWrite( &packParam.buff, &sizeOffset, sizeof( sizeOffset ) );

				classDef->cPackCB ( packParam.instance, &tmp, &packParam.buff, (void *)&packParam, packDataCB );

				// patch in the amount written by the C callback
				*((int64_t *)(bufferBuff ( &packParam.buff ) + sizeOffset)) = bufferSize ( &packParam.buff ) - sizeOffset;
			} else
			{
				// no data written out
				bufferPut64 ( &packParam.buff, (char)0 );
			}

			// now pack our object proper
			packDataObj ( packParam, var );
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

/*
	theory - traverse input data variant and serializes content
		creates:
			1 -	a count of non-object VAR's needed to unpack
			1 - a count of the total dat size needed to unpack
			1 - a list of all classes and the number of instantiations of each class
				this is used to increase the var and data size needed to unpack appropriately
				
			packing is the done into a buffer
*/

static VAR_STR vmPack ( class vmInstance *instance, VAR *var )
{
	PackParam	  packParam;
	PackHeader4	 *packHeader;

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
									 {
									   instance->atomTable->clearUseCount ( index );
									   return false;
									 }
	);

	packParam.instance = instance;

	bufferMakeRoom( &packParam.buff, sizeof( *packHeader ) );
	bufferAssume( &packParam.buff, sizeof( *packHeader ) );

	packHeader = (PackHeader4 *)bufferBuff( &packParam.buff );
	memset( packHeader, 0, sizeof( *packHeader ) );

	packData ( packParam, var );

	packHeader = (PackHeader4 *)bufferBuff( &packParam.buff );
	memset( packHeader, 0, sizeof( *packHeader ) );
	packHeader->nVars = (uint32_t)(bufferSize( &packParam.xRefBuff ) / sizeof( VAR ** ));
	packHeader->version = packVersion::packVersion4;

	packHeader->offsetLibraries = (uint32_t)bufferSize( &packParam.buff );
	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   if ( instance->atomTable->getInUse ( index ) )
									   {
										   auto img = instance->atomTable->atomGetLoadImage ( index );
										   if ( img->isLibrary )
										   {
											   packHeader = (PackHeader4 *) bufferBuff ( &packParam.buff );
											   packHeader->nLibraries++;
											   bufferWrite ( &packParam.buff, img->name, strlen ( img->name ) + 1 );
										   }
									   }
									   return false;
								   }
	);

	// do we need class information or is library source enough...
	packHeader->offsetClasses = (uint32_t)bufferSize( &packParam.buff );
#if 0
	instance->atomTable->typeMap ( atom::aCLASSDEF, [&]( uint32_t index )
								   {
									   if ( instance->atomTable->getInUse ( index ) )
									   {
										   packHeader = (PackHeader4 *) bufferBuff ( &packParam.buff );
										   packHeader->nClass++;
										   auto *cls = instance->atomTable->atomGetClass ( index );
										   bufferWrite ( &packParam.buff, &pAtm->useCount, sizeof ( uint32_t ) );
										   bufferWrite ( &packParam.buff, cls->name, cls->nameLen );
									   }
									   return false;
								   }
	);
#endif
	return VAR_STR ( instance, packParam.buff );
}

struct UnPackData
{
	vmInstance		 *instance;
	uint8_t			 *buff;
	size_t			  buffLen;
	VAR				**xRef;
	size_t			  nXRef;
	size_t			  nXRefMax;
	VAR				 *obj;

	UnPackData ( vmInstance *instance, size_t nXRef, uint8_t *buff, size_t buffLen, VAR *obj ) : instance ( instance ), buff ( buff ), buffLen ( buffLen ), nXRef ( 0 ), nXRefMax ( nXRef ), obj ( obj )
	{
		xRef = (VAR **)instance->malloc ( sizeof ( VAR * ) * nXRefMax );  // NOLINT ( bugprone-sizeof-expression )
	}

	virtual ~UnPackData ( )
	{
		instance->free ( xRef );
	}
};

struct UnPackParam
{
	class vmInstance	 *instance;
	UnPackData			 *data;
	VAR					 *var;
};

static VAR *unPackData ( class vmInstance *instance, VAR *var, UnPackData &data );

VAR *unPackDataCB ( unsigned char ** buff, uint64_t *len, void *param )
{
	UnPackParam *p = (UnPackParam *) param;
	return unPackData ( p->instance, 0, *p->data );
}

static void unPackDataObj ( class vmInstance *instance, VAR *var, UnPackData &data )
{
	bcClass				*classDef;
	bcClassSearchEntry	*entry;
	char const			*name;
	uint32_t			 nSymbols;
	uint32_t			 loop;
	uint32_t			 len;
	fgxClassElementType	 type;

	classDef = var->dat.obj.classDef;

	if ( data.buffLen < sizeof ( uint32_t ) ) throw errorNum::scINVALID_PARAMETER;
	nSymbols = *((uint32_t *) data.buff);
	data.buff += sizeof ( uint32_t );
	data.buffLen -= sizeof ( uint32_t );

	for ( loop = 0; loop < nSymbols; loop++ )
	{
		if ( !data.buffLen ) throw errorNum::scINVALID_PARAMETER;
		type = (fgxClassElementType)*(data.buff++);
		data.buffLen--;

		if ( data.buffLen < sizeof ( uint32_t ) ) throw errorNum::scINVALID_PARAMETER;
		len = *((uint32_t *) data.buff);
		data.buff += sizeof ( uint32_t );
		data.buffLen -= sizeof ( uint32_t );

		name = (char const *)data.buff;
		data.buff += len;
		data.buffLen -= len;
		switch ( type )
		{
			case fgxClassElementType::fgxClassType_iVar:
			case fgxClassElementType::fgxClassType_static:
				entry = findClassEntry ( classDef, name, fgxClassElemSearchType::fgxClassElemPrivateAccess );
				if ( entry )
				{
					if ( entry->isStatic )
					{
						classDef->loadImage->globals[entry->methodAccess.globalEntry] = unPackData ( instance, 0, data );
					} else
					{
						if ( entry->isVirtual )
						{
							var[var->dat.obj.vPtr[entry->methodAccess.vTableEntry].delta].type = slangType::eNULL;
							unPackData ( instance, &var[var->dat.obj.vPtr[entry->methodAccess.vTableEntry].delta], data );
						} else
						{
							var[entry->methodAccess.iVarIndex].type = slangType::eNULL;
							unPackData ( instance, &var[entry->methodAccess.iVarIndex], data );
						}
					}
				} else
				{
					// element doesn't exist... throw away any data for it, don't care about it
					unPackData ( instance, 0, data );
				}
				break;
			default:
				break;
		}
	}
	return;
}

static VAR *unPackData ( class vmInstance *instance, VAR *var, UnPackData &data )
{
	int64_t			 loop;
	int64_t			 len;
	int64_t			 startIndex;
	int64_t			 endIndex;
	bcClass			*classDef;
	packElementType	 type;

	if ( !data.buffLen ) throw errorNum::scINVALID_PARAMETER;
	type = (packElementType)*(data.buff++);
	data.buffLen--;

	switch ( type )
	{
		case packElementType::packType_elemIndex:
			if ( data.buffLen < sizeof ( uint32_t ) ) throw errorNum::scINVALID_PARAMETER;
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			var = data.xRef[*((uint32_t *) data.buff)];
			data.buff += sizeof ( uint32_t );
			data.buffLen -= sizeof ( uint32_t );
			break;

		case packElementType::packType_reference:
			if ( var )
			{
				// MUST be done before we recurse as we may refer to this entry
				if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
				data.xRef[data.nXRef++] = var;
				switch ( var->type )
				{
					case slangType::eREFERENCE:
						unPackData ( instance, var->dat.ref.obj, data );
						unPackData ( instance, var->dat.ref.v, data );
						break;
					default:
						{
							auto nextType = (packElementType)*(data.buff);

							switch ( nextType )
							{
								case packElementType::packType_arrayRoot:
									if ( var->type == slangType::eARRAY_ROOT )
									{
										*var = *unPackData ( instance, var, data );
									} else
									{
										VAR *tmpVar = unPackData ( instance, 0, data );
										*var = VAR_REF ( tmpVar, tmpVar );
									}
									break;
								case packElementType::packType_objectRoot:
									if ( var->type == slangType::eOBJECT_ROOT )
									{
										*var = *unPackData ( instance, var, data );
									} else
									{
										VAR *tmpVar = unPackData ( instance, 0, data );
										*var = VAR_REF ( tmpVar, tmpVar );
									}
									break;
								default:
									VAR *tmpVar = unPackData ( instance, 0, data );
									*var = VAR_REF ( tmpVar, tmpVar );
									break;
							}
						}
						break;
				}
			} else
			{
				if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
				var = instance->om->allocVar ( 1 );
				VAR *tmpVar = unPackData ( instance, nullptr, data );
				*var = VAR_REF ( tmpVar, tmpVar );
				data.xRef[data.nXRef++] = var;
			}
			break;

		case packElementType::packType_arrayRoot:
			if ( var )
			{
				if ( var->type != slangType::eARRAY_ROOT || var->dat.ref.v->type != slangType::eARRAY_SPARSE )
				{
					*var = VAR_ARRAY ( instance );
				}
			} else
			{
				var = instance->om->allocVar ( 1 );
			}
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;
			var->type = slangType::eARRAY_ROOT;
			var->dat.ref.obj = unPackData ( instance, var->dat.ref.v, data );
			var->dat.ref.v = unPackData ( instance, nullptr, data );
			break;

		case packElementType::packType_arraySparse:
			if ( !var )
			{
				var = instance->om->allocVar ( 1 );
			}
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			var->type = slangType::eARRAY_SPARSE;
			var->dat.aSparse.v = 0;
			var->dat.aSparse.lastAccess = 0;
			var->dat.aSparse.maxI = 0;
			var->dat.aSparse.maxI = *((int64_t *) data.buff);

			data.buff += sizeof ( int64_t );
			data.buffLen -= sizeof ( int64_t );

			if ( data.buffLen < sizeof ( uint8_t ) ) throw errorNum::scINVALID_PARAMETER;
			if ( *data.buff != (char)packElementType::packType_arrayElem )
			{
				// in pack we would still store off the 0 so we have to do it here
				if ( *data.buff != (char)packElementType::packType_null )
				{
					throw errorNum::scINVALID_PARAMETER;
				}
			} else
			{
				while ( *data.buff == (char)packElementType::packType_arrayElem )
				{
					if ( data.buffLen < sizeof ( uint8_t ) ) throw errorNum::scINVALID_PARAMETER;
					data.buff++;
					data.buffLen -= 1;

					if ( data.buffLen < sizeof ( int64_t ) ) throw errorNum::scINVALID_PARAMETER;

					auto index = *((int64_t *) data.buff);
					data.buff += sizeof ( int64_t );
					data.buffLen -= sizeof ( int64_t );

					arrayGet ( instance, var, index ) = *unPackData ( instance, 0, data );
				}
			}
			data.buff++;
			data.buffLen -= 1;
			break;

		case packElementType::packType_arrayFixed:
			startIndex = *((int64_t *) data.buff);
			data.buff += sizeof ( int64_t );
			data.buffLen -= sizeof ( int64_t );

			endIndex = *((int64_t *) data.buff);
			data.buff += sizeof ( int64_t );
			data.buffLen -= sizeof ( int64_t );

			if ( var )
			{
				if ( var->type != slangType::eARRAY_FIXED || var->dat.arrayFixed.startIndex != startIndex || var->dat.arrayFixed.endIndex != endIndex )
				{
					var = instance->om->allocVar ( endIndex - startIndex + 1 + 1 ); // start/end is inclusive so + 1 for total count + 1 array (already have a var to use as root)
				}
			} else
			{
				var = instance->om->allocVar ( endIndex - startIndex + 1 + 1 );// start/end is inclusive so + 1 for total count + 2 for root and array
			}
			var->type = slangType::eARRAY_FIXED;
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( int64_t ) + sizeof ( int64_t ) ) throw errorNum::scINVALID_PARAMETER;

			var->dat.arrayFixed.startIndex = startIndex;
			var->dat.arrayFixed.endIndex = endIndex;

			if ( (long) data.buffLen < (var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1) ) throw errorNum::scINVALID_PARAMETER;

			for ( loop = 0; loop < endIndex - startIndex + 1; loop++ )
			{
				var[loop + 1].type = slangType::eNULL;
				unPackData ( instance, &var[loop + 1], data );
			}
			break;

		case packElementType::packType_long:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( int64_t ) ) throw errorNum::scINVALID_PARAMETER;

			var->type = slangType::eLONG;
			var->dat.l = *((int64_t *) data.buff);

			data.buff += sizeof ( int64_t );
			data.buffLen -= sizeof ( int64_t );
			break;

		case packElementType::packType_longLong:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( int64_t ) ) throw errorNum::scINVALID_PARAMETER;

			var->type = slangType::eLONG;
			var->dat.l = (uint32_t)*((int64_t *) data.buff);

			data.buff += sizeof ( int64_t );
			data.buffLen -= sizeof ( int64_t );
			break;

		case packElementType::packType_double:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( double ) ) throw errorNum::scINVALID_PARAMETER;
			var->type = slangType::eDOUBLE;
			var->dat.d = *((double *) data.buff);

			data.buff += sizeof ( double );
			data.buffLen -= sizeof ( double );
			break;

		case packElementType::packType_null:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			var->type = slangType::eNULL;
			break;

		case packElementType::packType_string:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( uint32_t ) ) throw errorNum::scINVALID_PARAMETER;

			var->type = slangType::eSTRING;
			var->dat.str.len = *((uint32_t  *) data.buff);

			data.buff += sizeof ( uint32_t );
			data.buffLen -= sizeof ( uint32_t );

			if ( var->dat.str.len > ( size_t )data.buffLen ) throw errorNum::scINVALID_PARAMETER;

			var->dat.str.c = (char *) data.buff;

			data.buff += var->dat.str.len + 1;		// +1 for terminating 0
			data.buffLen -= var->dat.str.len + 1;
			break;

		case packElementType::packType_codeBlockRoot:
			if ( !var )
			{
				var = instance->om->allocVar ( 1 );
			}
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;
			var->type = slangType::eCODEBLOCK_ROOT;
			var->dat.ref.obj = unPackData ( instance, var->dat.ref.v, data );
			var->dat.ref.v = var->dat.ref.obj;
			break;

		case packElementType::packType_codeBlock:
			if ( !var )
			{
				var = instance->om->allocVar ( 1 );
				var->type = slangType::eCODEBLOCK;
			}
			{
				if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
				data.xRef[data.nXRef++] = var;

				var->dat.ref.v = instance->om->allocVar ( static_cast<size_t>(var->dat.cb.cb->nSymbols) - var->dat.cb.cb->nParams + 1 );
				var->dat.ref.obj = var->dat.ref.v;
				memset ( &var->dat.ref.v[1], 0, sizeof ( VAR ) * (static_cast<size_t>(var->dat.cb.cb->nSymbols) - var->dat.cb.cb->nParams) );

				var->dat.ref.v->type = slangType::eCODEBLOCK;
				var->dat.ref.v->dat.cb.cb = (vmCodeBlock *) (data.buff + sizeof ( uint32_t ));

				data.buffLen -= *(uint32_t *) data.buff + sizeof ( unsigned long );
				data.buff += *(uint32_t *) data.buff + sizeof ( unsigned long );

				for ( loop = var->dat.cb.cb->nParams; loop < var->dat.cb.cb->nSymbols; loop++ )
				{
					unPackData ( instance, &var[loop + 1], data );
				}
			}
			break;

		case packElementType::packType_objectRoot:
			if ( var )
			{
				if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
				data.xRef[data.nXRef++] = var;
				if ( var->type != slangType::eOBJECT_ROOT )
				{
					var->type = slangType::eOBJECT_ROOT;
					var->dat.ref.obj = unPackData ( instance, 0, data );
					var->dat.ref.v  = unPackData ( instance, 0, data );
				} else
				{
					var->type = slangType::eOBJECT_ROOT;
					var->dat.ref.obj = unPackData ( instance, var->dat.ref.v, data );
					// in this case, the inner reference is pointing to the old object.. we've already rebuilt the old object into the new one.  unpack with a null will return the proper inner ref
					var->dat.ref.v = unPackData ( instance, nullptr, data );
				}
			} else
			{
				var = instance->om->allocVar ( 1 );
				if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
				data.xRef[data.nXRef++] = var;
				var->type = slangType::eOBJECT_ROOT;
				var->dat.ref.obj = unPackData ( instance, 0, data );
				var->dat.ref.v = unPackData ( instance, 0, data );
			}
			break;

		case packElementType::packType_object:
			if ( data.buffLen < sizeof ( int32_t ) ) throw errorNum::scINVALID_PARAMETER;
			len = *((int32_t *) data.buff);
			data.buff += sizeof ( int32_t );
			data.buffLen -= sizeof ( int32_t );

			if ( data.buffLen < (size_t) len ) throw errorNum::scINVALID_PARAMETER;

			if ( !var )
			{
				// no object so create a new one, otherwise just unpack in place
				classNew ( instance, (char *) data.buff, 0 );
				var = instance->result.dat.ref.v;
			} else
			{
				if ( strccmp ( var->dat.obj.classDef->name, (char *)data.buff ) )
				{
					throw errorNum::scINVALID_PARAMETER;
				}
			}

			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			classDef = var->dat.obj.classDef;

			// move us past the className
			data.buff += len;
			data.buffLen -= len;

			if ( data.buffLen < sizeof ( int64_t ) ) throw errorNum::scINVALID_PARAMETER;
			len = *((int64_t *) data.buff);
			data.buff += sizeof ( int64_t );
			data.buffLen -= sizeof ( int64_t );

			if ( len )
			{
				UnPackParam		 param{};
				unsigned char	*end;

				if ( !classDef->cUnPackCB )
				{
					throw errorNum::scINVALID_PARAMETER;
				}

				param.data = &data;
				param.var = var;
				param.instance = instance;

				if ( (int64_t) data.buffLen < len ) throw errorNum::scINVALID_PARAMETER;

				end = data.buff + len - sizeof ( int64_t );

				VAR_OBJ tmp(var, var);

				(classDef->cUnPackCB) (instance, &tmp, &data.buff, (uint64_t *) &data.buffLen, &param, unPackDataCB);

				if ( data.buff != end ) throw errorNum::scINVALID_PARAMETER;
			}

			unPackDataObj ( instance, var, data );

			// this does NOT unpack... it does any user-required code during the unpack operation (opening handles, etc.)
			classCallUnpack ( instance, var );
			break;

		default:
			printf ( "--------------------------------\r\n" );
			printf ( "error: var type %i\r\n", (int)type );
			printf ( "--------------------------------\r\n" );
			throw errorNum::scINTERNAL;
	}
	return var;
}

static VAR *unPack3Data ( class vmInstance *instance, VAR *inPlace, UnPackData &data );

static VAR vmUnPack ( class vmInstance *instance, VAR_STR *var, VAR *obj )
{
	PackHeader4		*packHeader;
	VAR				*result;

	if ( TYPE ( obj ) == slangType::eNULL )
	{
		obj = 0;
	} else
	{
		if ( TYPE ( obj ) != slangType::eOBJECT_ROOT ) throw errorNum::scINVALID_PARAMETER;
		if ( TYPE ( obj->dat.ref.v ) != slangType::eOBJECT ) throw errorNum::scINVALID_PARAMETER;
	}

	packHeader = (PackHeader4 *)var->dat.str.c;

	switch ( packHeader->version )
	{
		case packVersion::packVersion3:
			{
				PackHeader3 *packHeader3 = (PackHeader3 *)var->dat.str.c;
				uint32_t nVars = packHeader3->nVars;

				UnPackData unpackData ( instance, nVars, (uint8_t *) var->dat.str.c + sizeof ( *packHeader3 ), var->dat.str.len - sizeof ( *packHeader3 ), obj );

				result = unPack3Data ( instance, obj, unpackData );
			}
			break;
		case packVersion::packVersion4:
			{
				uint32_t	nVars = packHeader->nVars;

				UnPackData	 unpackData ( instance, nVars, (uint8_t *) var->dat.str.c + sizeof ( *packHeader ), var->dat.str.len - sizeof ( *packHeader ), obj );

				result = unPackData ( instance, obj, unpackData );
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	};
	return *result;
}

VAR *unPack3DataCB ( unsigned char ** buff, uint64_t *len, void *param )
{
	UnPackParam *p = (UnPackParam *) param;
	return unPack3Data ( p->instance, 0, *p->data );
}

#define oIVAR			1
#define oACCESS			2
#define oDEFAULT		3
#define oINHERIT		4
#define oSTATIC			5
#define oDIRECTED		6
#define oCONSTANT		7

enum class oldPackType {
	eNULL = 0x00,
	eLONG = 0x01,
	eSTRING = 0x02,
	eDOUBLE = 0x03,
	eARRAY_SPARSE = 0x06,
	eARRAY_ELEM = 0x07,
	eCODEBLOCK = 0x08,
	eREFERENCE = 0x09,
	// = 0x0A not defined
	eOBJECT = 0x0B,
	eARRAY_FIXED = 0x20,
	eIPADDRESS = 0x21,
	eSTORED = 0xFF,
	eNOFREE = 0x80,
};

static void unPack3DataObj ( class vmInstance *instance, VAR *var, UnPackData &data, stringi const &nameSpace = "" )
{
	bcClass				*classDef;
	bcClassSearchEntry	*entry;
	uint32_t			 nSymbols;
	uint32_t			 loop;
	size_t				 len;

	classDef = var->dat.obj.classDef;

	if ( data.buffLen < sizeof ( uint32_t ) ) throw errorNum::scINVALID_PARAMETER;
	nSymbols = *((uint32_t *)data.buff);
	data.buff		+= sizeof ( uint32_t );
	data.buffLen	-= sizeof ( uint32_t );

	for ( loop = 0; loop < nSymbols; loop++ )
	{
		stringi name;

		len = strnlen ( (char const *)data.buff, data.buffLen ) + 1;
		if ( data.buffLen < len )
		{
			throw errorNum::scINVALID_PARAMETER;
		}

		if ( nameSpace.size ( ) )
		{
			name = nameSpace + "::" + (char const *)data.buff;
		} else
		{
			name = (char const *) data.buff;
		}

		data.buff += len;
		data.buffLen -= len;

		entry = findClassEntry ( classDef, (char const *) name, fgxClassElemSearchType::fgxClassElemPrivateAccess );
		if ( entry )
		{
			switch ( entry->type )
			{
				case fgxClassElementType::fgxClassType_iVar:
					if ( entry->isVirtual )
					{
						var[var->dat.obj.vPtr[entry->methodAccess.vTableEntry].delta].type = slangType::eNULL;
						unPack3Data ( instance, &var[var->dat.obj.vPtr[entry->methodAccess.vTableEntry].delta], data );
					} else
					{
						var[entry->methodAccess.iVarIndex].type = slangType::eNULL;
						unPack3Data ( instance, &var[entry->methodAccess.iVarIndex], data );
					}
					break;
				case fgxClassElementType::fgxClassType_static:
					classDef->loadImage->globals[entry->methodAccess.globalEntry] = unPack3Data ( instance, 0, data );
					break;
				case fgxClassElementType::fgxClassType_inherit:
					unPack3DataObj ( instance, var, data, name );
					break;
				default:
					throw errorNum::scINVALID_PARAMETER;
			}
		} else
		{
			// element doesn't exist... throw away any data for it, don't care about it
			unPack3Data ( instance, 0, data );
		}
	}
}

static VAR *unPack3Data ( class vmInstance *instance, VAR *var, UnPackData &data )
{
	int64_t			 loop;
	int64_t			 len;
	int64_t			 startIndex;
	int64_t			 endIndex;
	oldPackType		 type;

	if ( !data.buffLen ) throw errorNum::scINVALID_PARAMETER;
	type = (oldPackType) (*(data.buff++) & ~(char)oldPackType::eNOFREE);
	data.buffLen--;

	switch ( type )
	{
		case oldPackType::eSTORED:
			if ( data.buffLen < sizeof ( uint32_t ) ) throw errorNum::scINVALID_PARAMETER;
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			var = data.xRef[*((uint32_t *) data.buff)];
			data.buff    += sizeof ( uint32_t );
			data.buffLen -= sizeof ( uint32_t );
			if ( var->type == slangType::eREFERENCE )
			{
				switch ( var->dat.ref.v->type )
				{
					case slangType::eARRAY_ROOT:
					case slangType::eOBJECT_ROOT:
						// old engine required a reference to exist to ensure that all objects and arrays were present in OM memory.
						// this required additional code on dereferencing to ensure that the reference wasn't consumed.   It was code heavy
						// we do away with all of this by simply having a root type that is really a special case reference but is 
						// at once directly copyable and ensures that the object/array is never in the root set (always a reference type).
						// drasticly simplifies code
						// the below line simply eliminates the unnecessary reference
						//   NOTE: we still need to allocate this reference however, in order that any other elements pointing to it
						//      still have a valid reference.   however when building the resultant data structure we can bypass the element
						//		if nothing points to it it will simply be collected at the next garbage cycle
						var = var->dat.ref.v;
						break;
					default:
						break;
				}
			}
			break;

		case oldPackType::eREFERENCE:
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			if ( var )
			{
				data.xRef[data.nXRef++] = var;

				switch ( var->type )
				{
					case slangType::eREFERENCE:
						unPack3Data ( instance, var->dat.ref.obj, data ); 
						var->dat.ref.v = var->dat.ref.obj;
						break;
					default:
						{
							auto nextType = (oldPackType)*(data.buff);

							switch ( nextType )
							{
								case oldPackType::eARRAY_SPARSE:
								case oldPackType::eARRAY_FIXED:
									if ( var->type == slangType::eARRAY_ROOT )
									{
										*var = *unPack3Data ( instance, var, data );
									} else
									{
										VAR *tmpVar = unPack3Data ( instance, 0, data );
										*var = VAR_REF ( tmpVar, tmpVar );
									}
									break;
								case oldPackType::eOBJECT:
									if ( var->type == slangType::eOBJECT_ROOT )
									{
										*var = *unPack3Data ( instance, var, data );
									} else
									{
										VAR *tmpVar = unPack3Data ( instance, 0, data );
										*var = VAR_REF ( tmpVar, tmpVar );
									}
									break;
								default:
									VAR *tmpVar = unPack3Data ( instance, 0, data );
									*var = VAR_REF ( tmpVar, tmpVar );
									break;
							}
						}
						break;
				}
			} else
			{
				var = instance->om->allocVar ( 1 );

				VAR *tmpVar = unPack3Data ( instance, 0, data );
				*var = VAR_REF ( tmpVar, tmpVar );
				data.xRef[data.nXRef++] = var;
			}
			if ( var->type == slangType::eREFERENCE )
			{
				switch ( var->dat.ref.v->type )
				{
					case slangType::eARRAY_ROOT:
					case slangType::eOBJECT_ROOT:
						// old engine required a reference to exist to ensure that all objects and arrays were present in OM memory.
						// this required additional code on dereferencing to ensure that the reference wasn't consumed.   It was code heavy
						// we do away with all of this by simply having a root type that is really a special case reference but is 
						// at once directly copyable and ensures that the object/array is never in the root set (always a reference type).
						// drasticly simplifies code
						// the below line simply eliminates the unnecessary reference
						//   NOTE: we still need to allocate this reference however, in order that any other elements pointing to it
						//      still have a valid reference.   however when building the resultant data structure we can bypass the element
						//		if nothing points to it it will simply be collected at the next garbage cycle
						*var = *var->dat.ref.v;
						break;
					default:
						break;
				}
			}
			break;

		case oldPackType::eARRAY_SPARSE:
			if ( var )
			{
				if ( var->type != slangType::eARRAY_ROOT || var->dat.ref.v->type != slangType::eARRAY_SPARSE ) 
				{
					*var = VAR_ARRAY ( instance );
				}
			} else
			{
				var = instance->om->allocVar ( 1 );
				*var = VAR_ARRAY ( instance );
			}

			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( (oldPackType)*data.buff != oldPackType::eARRAY_ELEM )
			{
				// in pack we would still store off the 0 so we have to do it here
				if ( (oldPackType)*data.buff != (oldPackType)((uint8_t)oldPackType::eNULL | (uint8_t)oldPackType::eNOFREE) )
				{
					data.xRef[data.nXRef++] = 0;
				}
			} else
			{
				while ( (oldPackType)*data.buff == oldPackType::eARRAY_ELEM )
				{
					if ( data.buffLen  < sizeof ( char ) )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					data.buff++;
					data.buffLen--;

					if ( data.buffLen < sizeof ( int ) )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					int64_t indicie = *((int *)data.buff);
					data.buff += sizeof ( int );
					data.buffLen -= sizeof ( int );

					VAR *val = unPack3Data ( instance, 0, data );

					arrayGet ( instance, var, indicie ) = *val;
				}
			}

			if ( data.buffLen  < sizeof ( char ) )
			{
				throw errorNum::scINVALID_PARAMETER;
			}

			data.buff++;
			data.buffLen--;
			break;
		case oldPackType::eARRAY_FIXED:
			startIndex = *((int32_t *)data.buff);
			data.buff += sizeof ( int32_t );
			data.buffLen -= sizeof ( int32_t );

			endIndex = *((int32_t *)data.buff);
			data.buff += sizeof ( int32_t );
			data.buffLen -= sizeof ( int32_t );

			if ( var )
			{
				if ( var->type != slangType::eARRAY_ROOT || var->dat.ref.v->type != slangType::eARRAY_FIXED || var->dat.ref.v->dat.arrayFixed.startIndex != startIndex || var->dat.ref.v->dat.arrayFixed.endIndex != endIndex )
				{
					var->type = slangType::eARRAY_ROOT;
					var->dat.ref.v = instance->om->allocVar ( endIndex - startIndex + 1 + 1 ); // start/end is inclusive so + 1 for total count + 1 array (already have a var to use as root)
					var->dat.ref.obj = var->dat.ref.v;
					var->dat.ref.v->type = slangType::eARRAY_FIXED;
				}
			} else
			{
				var = instance->om->allocVar ( endIndex - startIndex + 1 + 2 );// start/end is inclusive so + 1 for total count + 2 for root and array
				var->type = slangType::eARRAY_ROOT;
				var->dat.ref.v = &var[1];
				var->dat.ref.obj = var->dat.ref.v;
				var->dat.ref.v->type = slangType::eARRAY_FIXED;
			}

			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			var->dat.ref.v->dat.arrayFixed.startIndex = startIndex;
			var->dat.ref.v->dat.arrayFixed.endIndex = endIndex;

			for ( loop = 0; loop < endIndex - startIndex + 1; loop++ )
			{
				var->dat.ref.v[loop + 1].type = slangType::eNULL;
				unPack3Data ( instance, &var->dat.ref.v[loop + 1], data );
			}
			break;

		case oldPackType::eDOUBLE:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( double ) ) throw errorNum::scINVALID_PARAMETER;
			var->type = slangType::eDOUBLE;
			var->dat.d = *((double *) data.buff);

			data.buff += sizeof ( double );
			data.buffLen -= sizeof ( double );
			break;

		case oldPackType::eLONG:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( int32_t ) ) throw errorNum::scINVALID_PARAMETER;

			var->type = slangType::eLONG;
			var->dat.l = *((int32_t *) data.buff);

			data.buff += sizeof ( int32_t );
			data.buffLen -= sizeof ( int32_t );
			break;

		case oldPackType::eSTRING:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;

			if ( data.buffLen < sizeof ( uint32_t ) ) throw errorNum::scINVALID_PARAMETER;

			var->type = slangType::eSTRING;
			var->dat.str.len = *((uint32_t  *) data.buff);

			data.buff += sizeof ( uint32_t );
			data.buffLen -= sizeof ( uint32_t );

			if ( var->dat.str.len > data.buffLen ) throw errorNum::scINVALID_PARAMETER;

			*var = VAR_STR ( instance, (char *) data.buff, var->dat.str.len );

			data.buff += var->dat.str.len;		// terminating zero isn't written out... would have made it a lot faster unpacking if i had as i could just point to the buffer directly... sigh...
			data.buffLen -= var->dat.str.len;
			break;

		case oldPackType::eNULL:
			if ( !var ) var = instance->om->allocVar ( 1 );
			if ( data.nXRef >= data.nXRefMax ) throw errorNum::scINVALID_PARAMETER;
			data.xRef[data.nXRef++] = var;
			var->type = slangType::eNULL;
			break;

		case oldPackType::eOBJECT:
			{
				if ( data.buffLen < strnlen ( (char const *)data.buff, data.buffLen ) + 1 )
				{
					throw errorNum::scINVALID_PARAMETER;
				}

				if ( var )
				{
					switch ( var->type )
					{
						case slangType::eOBJECT_ROOT:
							if ( strccmp ( var->dat.ref.v->dat.obj.classDef->name, (char const *)data.buff ) )
							{
								*var = *classNew ( instance, (char const *)data.buff, 0 );
							}
							break;
						default:
							*var = *classNew ( instance, (char const *)data.buff, 0 );
							break;
					}
				} else
				{
					var = instance->om->allocVar ( 1 );
					*var = *classNew ( instance, (char const *)data.buff, 0 );
				}

				auto classDef = var->dat.ref.v->dat.obj.classDef;

				data.buff += classDef->nameLen;
				data.buffLen -= classDef->nameLen;

				if ( data.buffLen < sizeof ( char ) )
				{
					throw errorNum::scINVALID_PARAMETER;
				}

				if ( *((uint8_t *) data.buff++) )
				{
					data.buffLen--;

					if ( !classDef->cUnPackCB )
					{
						throw errorNum::scINVALID_PARAMETER;
					}

					UnPackParam		 param{};
					unsigned char	*end;

					param.data = &data;
					param.var = var;
					param.instance = instance;

					if ( (unsigned int) data.buffLen < sizeof ( unsigned int ) ) throw errorNum::scINVALID_PARAMETER;
					len = *((unsigned int *) data.buff);
					data.buff += sizeof ( unsigned int );
					data.buffLen -= sizeof ( unsigned int );

					if ( (size_t)len > data.buffLen ) throw errorNum::scINVALID_PARAMETER;
					end = data.buff + len - sizeof ( uint32_t );

					(classDef->cUnPackCB) (instance, var, &data.buff, (uint64_t *) &data.buffLen, &param, unPack3DataCB);

					if ( data.buff != end )
					{
						throw errorNum::scINVALID_PARAMETER;
					}
				} else
				{
					data.buffLen--;
				}
				unPack3DataObj ( instance, var->dat.ref.v, data );
				classCallUnpack ( instance, var );
			}
			break;
		default:
			printf ( "--------------------------------\r\n" );
			printf ( "error: var type %i\r\n", (int)type );
			printf ( "--------------------------------\r\n" );
			throw errorNum::scINTERNAL;
	}

	return var;
}


static void packData3 ( PackParam &packParam, VAR *var );

static void packDataCB3 ( VAR *var, void *param )
{
	packData3 ( *(PackParam *) param, var );
}

static void packDataObj3 ( PackParam &packParam, VAR *obj )
{
	uint32_t		 nSymbols = 0;

	auto classDef = obj->dat.obj.classDef;
	auto nSymbolsOffset = bufferSize ( &packParam.buff );

	bufferAssume ( &packParam.buff, sizeof ( nSymbols ) );

	for ( uint32_t nElem = 0; nElem < classDef->nElements; nElem++ )
	{
		switch ( classDef->elements[nElem].type )
		{
			case fgxClassElementType::fgxClassType_iVar:
				if ( !classDef->elements[nElem].methodAccess.objectStart )
				{
					bufferWrite ( &packParam.buff, classDef->elements[nElem].name, classDef->elements[nElem].nameLen );
					packData3 ( packParam, &obj[classDef->elements[nElem].methodAccess.data] );
					nSymbols++;
				}
				break;
			case fgxClassElementType::fgxClassType_static:
				if ( !classDef->elements[nElem].methodAccess.objectStart )
				{
					bufferWrite ( &packParam.buff, classDef->elements[nElem].name, classDef->elements[nElem].nameLen );
					packData3 ( packParam, classDef->loadImage->globals[classDef->elements[nElem].methodAccess.data] );
					nSymbols++;
				}
				break;
			case fgxClassElementType::fgxClassType_inherit:
				// this tells us if its an element that its a non-inherited element... inherited will be searialized independantly on an object-by-object basis
				if ( classDef->elements[nElem].nameLen == classDef->elements[nElem].fullNameLen )
				{
					bufferWrite ( &packParam.buff, classDef->elements[nElem].name, classDef->elements[nElem].nameLen );
					if ( classDef->elements[nElem].isVirtual )
					{
						packDataObj3 ( packParam, &obj[obj->dat.obj.vPtr[classDef->elements[nElem].methodAccess.iVarIndex].delta] );
					} else
					{
						packDataObj3 ( packParam, &obj[classDef->elements[nElem].methodAccess.objectStart] );
					}
					nSymbols++;
				}
				break;
			default:
				break;
		}
	}
	bufferWriteAt ( &packParam.buff, nSymbolsOffset, &nSymbols, sizeof ( nSymbols ) );
}

static void packData3 ( PackParam &packParam, VAR *var )
{
	VAR			**xRef;
	uint32_t	  nXRef;

	if ( !var )
	{
		bufferPut8 ( &packParam.buff, (char) ((int)oldPackType::eNULL | (int)oldPackType::eNOFREE) );
		return;
	}

	xRef = (VAR **) bufferBuff ( &packParam.xRefBuff );
	nXRef = (uint32_t) bufferSize ( &packParam.xRefBuff ) / sizeof ( VAR ** );

	if ( (var->packIndex < nXRef) && (xRef[var->packIndex] == var) )
	{
		bufferPut8 ( &packParam.buff, 0xFF );
		bufferPut32 ( &packParam.buff, var->packIndex );
		return;
	}

	var->packIndex = nXRef;
	bufferWrite ( &packParam.xRefBuff, &var, sizeof ( VAR * ) );  // NOLINT ( bugprone-sizeof-expression )

	switch ( TYPE ( var ) )
	{
		case slangType::eREFERENCE:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eREFERENCE );

			// eliminate redundant references */
			while ( TYPE ( var->dat.ref.v ) == slangType::eREFERENCE )
			{
				var = var->dat.ref.v;
			}
			packData3 ( packParam, var->dat.ref.v );
			break;

		case slangType::eLONG:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eLONG );
			bufferPut32 ( &packParam.buff, (uint32_t)var->dat.l );
			break;

		case slangType::eDOUBLE:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eDOUBLE );
			bufferPutDouble ( &packParam.buff, var->dat.d );
			break;

		case slangType::eNULL:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eNULL );
			break;

		case slangType::eSTRING:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eSTRING );
			bufferPut32 ( &packParam.buff, (uint32_t) var->dat.str.len );
			bufferWrite ( &packParam.buff, var->dat.str.c, var->dat.str.len );
			break;

		case slangType::eARRAY_ROOT:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eREFERENCE );
			packData3 ( packParam, var->dat.ref.v );
			break;

		case slangType::eARRAY_SPARSE:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eARRAY_SPARSE );
			packData3 ( packParam, var->dat.aSparse.v );
			break;

		case slangType::eARRAY_ELEM:
			while ( var )
			{
				bufferPut8 ( &packParam.buff, (char) oldPackType::eARRAY_ELEM );
				bufferPut32 ( &packParam.buff, (int32_t) var->dat.aElem.elemNum );

				packData3 ( packParam, var->dat.aElem.var );
				var = var->dat.aElem.next;
			}
			bufferPut8 ( &packParam.buff, (char) oldPackType::eNULL );
			break;

		case slangType::eARRAY_FIXED:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eARRAY_FIXED );
			bufferPut32 ( &packParam.buff, (int32_t)var->dat.arrayFixed.startIndex );
			bufferPut32 ( &packParam.buff, (int32_t) var->dat.arrayFixed.endIndex );

			for ( size_t loop = 0; loop < (size_t) (var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex + 1); loop++ )
			{
				packData3 ( packParam, &var[loop + 1] );
			}
			break;

		case slangType::eOBJECT_ROOT:
			bufferPut8 ( &packParam.buff, (char) oldPackType::eREFERENCE );
			packData3 ( packParam, var->dat.ref.v );
			break;

		case slangType::eOBJECT:
			{
				bufferPut8 ( &packParam.buff, (char) oldPackType::eOBJECT );

				auto classDef = var->dat.obj.classDef;

				// mark it as being used
				packParam.instance->atomTable->markInUse ( classDef->loadImage->atom );

				bufferWrite ( &packParam.buff, classDef->name, classDef->nameLen );

				// call any user defined pre-packing routine to prepare for serialization
				classCallPack ( packParam.instance, var );

				// take care of any C pack callback
				if ( classDef->cPackCB )
				{
					int32_t	 sizeOffset;
					bufferPut8 ( &packParam.buff, (char) 1 );

					VAR_OBJ tmp(var, var);

					sizeOffset = (int32_t) bufferSize ( &packParam.buff );
					bufferAssume ( &packParam.buff, sizeof ( sizeOffset ) );

					classDef->cPackCB ( packParam.instance, &tmp, &packParam.buff, (void *) &packParam, packDataCB3 );

					// patch in the amount written by the C callback
					*((int32_t *) (bufferBuff ( &packParam.buff ) + sizeOffset)) = static_cast<int32_t>(bufferSize ( &packParam.buff ) - sizeOffset);
				} else
				{
					// no data written out
					bufferPut8 ( &packParam.buff, (char) 0 );
				}

				// now pack our object proper
				packDataObj3 ( packParam, var );
			}
			break;
		default:
			throw errorNum::scINTERNAL;
	}
}

/*
theory - traverse input data variant and serializes content
creates:
1 -	a count of non-object VAR's needed to unpack
1 - a count of the total dat size needed to unpack
1 - a list of all classes and the number of instantiations of each class
this is used to increase the var and data size needed to unpack appropriately

packing is the done into a buffer
*/

static VAR_STR vmPack3 ( class vmInstance *instance, VAR *var )
{
	PackParam	  packParam;

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   instance->atomTable->clearUseCount ( index );
									   return false;
								   }
	);

	packParam.instance = instance;

	bufferPut32 ( &packParam.buff, 0xCAFEBAB2 );
	bufferPut32 ( &packParam.buff, 0 );	// nXREF
	bufferPut32 ( &packParam.buff, 0 );	// libOffset

	packData3 ( packParam, var );

	uint32_t nXRef = ( uint32_t ) (bufferSize ( &packParam.xRefBuff ) / sizeof ( VAR ** ));

	*((uint32_t *) ((char *) bufferBuff ( &packParam.buff ) + sizeof ( uint32_t ))) = nXRef;															// 4 = size of id above
	*((uint32_t *) ((char *) bufferBuff ( &packParam.buff ) + sizeof ( uint32_t ) + sizeof ( uint32_t ))) = (uint32_t)bufferSize ( &packParam.buff );	// 4 = size of id above + sizeof of nXref above

	instance->atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
								   {
									   if ( instance->atomTable->getInUse ( index ) )
									   {
										   auto img = instance->atomTable->atomGetLoadImage ( index );
										   if ( img->isLibrary )
										   {
											   bufferWrite ( &packParam.buff, img->name, strlen ( img->name ) + 1 );
										   }
									   }
									   return false;
								   }
	);
	bufferPut8 ( &packParam.buff, 0 );

	return VAR_STR ( instance, packParam.buff );
}
static VAR vmPackLibraries ( class vmInstance *instance, VAR_STR *var )
{
	VAR				 res = VAR_ARRAY ( instance );
	uint64_t		 libOff;
	packVersion		 ver;
	int64_t			 index = 1;

	ver = *((packVersion *)var->dat.str.c);

	switch ( ver )
	{
		case packVersion::packVersion3:
			libOff = *((uint32_t *)(var->dat.str.c + sizeof ( uint32_t ) + sizeof ( uint32_t )));

			while ( *(var->dat.str.c + libOff) )
			{
				auto len = strlen ( var->dat.str.c + libOff );

				arrayGet ( instance, &res, 1 ) = VAR_STR ( instance, var->dat.str.c + libOff, len );
				libOff += len + 1;
				index++;
			}
			return res;
		case packVersion::packVersion4:
			{
				auto packHeader = (PackHeader4 *)var->dat.str.c;

				libOff = packHeader->offsetLibraries;
				for ( int64_t loop = 1; loop < packHeader->nLibraries; loop++ )
				{
					auto len = strlen ( var->dat.str.c + libOff );

					arrayGet ( instance, &res, index ) = VAR_STR ( instance, var->dat.str.c + libOff, len );
					libOff += len + 1;
					index++;
				}
			}
			return res;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	throw errorNum::scINVALID_PARAMETER;
}

bool vmIsPackString ( VAR *var )
{
	packVersion id;

	if ( TYPE ( var ) != slangType::eSTRING ) throw errorNum::scINVALID_PARAMETER;

	id = *((packVersion *)var->dat.str.c);
	
	switch ( id )
	{
		case packVersion::packVersion1:
		case packVersion::packVersion2:
		case packVersion::packVersion3:
		case packVersion::packVersion4:
			return true;
		default:
			return false;
	}
}

struct IterateData
{
	vmInstance				*instance;
	std::vector<VAR *>		 varStack;
	bool					 showNull;
	bool					 showObjName;
	bool					 emitType;
	bool					 doIndent;
	int64_t					 indentLevel = 0;
	int64_t					 indentAmount = 0;
	VAR						*obj;

	IterateData ( vmInstance *instance, bool showNull, bool showObjName, VAR *obj ) : instance ( instance ), showNull ( showNull ), showObjName ( showObjName ), obj ( obj )
	{
	}

	virtual ~IterateData ( )
	{
	}
};

static void vmObjToArrayIterate ( vmInstance *instance, VAR *arr, VAR *var, BUFFER &name, IterateData &data )
{
	while ( var->type == slangType::eREFERENCE ) var = var->dat.ref.v;

	if ( (var->packIndex < data.varStack.size()) && (data.varStack[var->packIndex] == var) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	var->packIndex = (uint32_t) data.varStack.size ( );
	data.varStack.push_back ( var );

	int64_t indicie = _amaxi_ ( arr ) + 1;

	switch ( TYPE ( var ) )
	{
		case slangType::eDOUBLE:
		case slangType::eSTRING:
		case slangType::eLONG:
			{
				arrayGet ( instance, arr, indicie, 1 ) = VAR_STR ( instance, name.data<char *> (), name.size () );
				arrayGet ( instance, arr, indicie, 2 ) = *var;
			}
			break;
		case slangType::eARRAY_ROOT:
			vmObjToArrayIterate ( instance, arr, var->dat.ref.v, name, data );
			break;
		case slangType::eARRAY_FIXED:
			{
				size_t size = name.size ( );
				for ( int64_t loop = 0; loop <= var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex; loop++ )
				{
					name.printf ( "[%I64i]", var->dat.arrayFixed.startIndex + loop );
					vmObjToArrayIterate ( instance, arr, &var[loop + 1] , name, data );
					name.unWrite ( name.size ( ) - size );
				}
			}
			break;
		case slangType::eARRAY_SPARSE:
			if ( var->dat.aSparse.v )
			{
				vmObjToArrayIterate ( instance, arr, var->dat.aSparse.v, name, data );
			}
			break;
		case slangType::eARRAY_ELEM:
			{
				size_t size = name.size ( );

				while ( var )
				{
					name.printf ( "[%I64i]", var->dat.aElem.elemNum );
					vmObjToArrayIterate ( instance, arr, var->dat.aElem.var, name, data );
					name.unWrite ( name.size ( ) - size );
					var = var->dat.aElem.next;
				}
			}
			break;
		case slangType::eOBJECT_ROOT:
			{
				var = var->dat.ref.v;
				auto classDef = var->dat.obj.classDef;
				size_t nameLen = 0;
				if ( data.showObjName )
				{
					if ( name.size ( ) )
					{
						name.put ( '.' );
						nameLen++;
					}
					name.write ( classDef->name, static_cast<size_t>(classDef->nameLen) - 1);
					nameLen += static_cast<size_t>(classDef->nameLen) - 1;
				}
				for ( size_t loop = 0; loop < classDef->nElements; loop++ )
				{
					size_t	len = 0;
					auto elem = &classDef->elements[loop];
					switch ( classDef->elements[loop].type )
					{
						case fgxClassElementType::fgxClassType_iVar:
						case fgxClassElementType::fgxClassType_static:
							// add the element name to the name string
							name.makeRoom ( elem->nameLen );
							if ( name.size ( ) )
							{
								name.put ( '.' );
								len++;
							}
							name.write ( elem->name, static_cast<size_t>(elem->nameLen) - 1);
							len += static_cast<size_t>(elem->nameLen) - 1;
							if ( elem->isStatic )
							{
								vmObjToArrayIterate ( instance, arr, classDef->loadImage->globals[elem->methodAccess.globalEntry], name, data );
							} else
							{
								if ( elem->isVirtual )
								{
									vmObjToArrayIterate ( instance, arr, &var[var->dat.obj.vPtr[elem->methodAccess.vTableEntry].delta], name, data );
								} else
								{
									vmObjToArrayIterate ( instance, arr, &var[elem->methodAccess.iVarIndex], name, data );
								}
							}
							name.unWrite ( len );  // undo our name
							break;
						default:
							break;
					}
				}
				if ( data.showObjName )
				{
					name.unWrite ( nameLen );
				}
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}

	data.varStack.pop_back ( );
}


static VAR vmObjToArray ( vmInstance *instance, VAR_OBJ *var, bool showNull, bool showObjName )
{
	VAR arr = VAR_ARRAY ( instance );

	IterateData	data ( instance, showNull, showObjName, &arr );
	BUFFER name;

	vmObjToArrayIterate ( instance, &arr, var, name, data );

	return arr;
}

static void vmObjToXMLIterate ( vmInstance *instance, VAR *var, BUFFER &xml, IterateData &data )
{
	while ( var->type == slangType::eREFERENCE ) var = var->dat.ref.v;

	if ( (var->packIndex < data.varStack.size ( )) && (data.varStack[var->packIndex] == var) )
	{
		throw errorNum::scINVALID_PARAMETER;
	}
	var->packIndex = (uint32_t)data.varStack.size ( );
	data.varStack.push_back ( var );

	switch ( TYPE ( var ) )
	{
		case slangType::eLONG:
			data.indentLevel += data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}

			xml.write ( "<long xmlns:fgl='http://www.fgldev.org/schema'" );

			if ( data.emitType )
			{
				xml.write ( " fgl:type=\"long\">" );
			} else
			{
				xml.put ( ">" );
			}
			xml.printf ( "%I64i", var->dat.l );
			xml.write ( "<\\long>" );
			data.indentLevel -= data.indentAmount;
			if ( data.indentLevel )
			{
				xml.write ( "\r\n", 2 );
			}
			break;

		case slangType::eDOUBLE:
			data.indentLevel += data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}

			xml.write ( "<double xmlns:fgl='http://www.fgldev.org/schema'" );

			if ( data.emitType )
			{
				xml.write ( " fgl:type=\"double\">" );
			} else
			{
				xml.put ( ">" );
			}
			xml.printf ( "%f", var->dat.d );
			xml.write ( "<\\double>" );
			data.indentLevel -= data.indentAmount;
			if ( data.indentLevel )
			{
				xml.write ( "\r\n", 2 );
			}
			break;

		case slangType::eNULL:
			data.indentLevel += data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}

			xml.write ( "<null xmlns:fgl='http://www.fgldev.org/schema'" );

			if ( data.emitType )
			{
				xml.write ( " fgl:type=\"NULL\">" );
			} else
			{
				xml.put ( ">" );
			}
			xml.write ( "<\\null>" );
			data.indentLevel -= data.indentAmount;
			if ( data.indentLevel )
			{
				xml.write ( "\r\n", 2 );
			}
			break;

		case slangType::eSTRING:
			data.indentLevel += data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}

			xml.write ( "<string xmlns:fgl='http://www.fgldev.org/schema'" );

			if ( data.emitType )
			{
				xml.printf ( " fgl:type=\"encString\" fgl:len=\"%i\">", var->dat.str.len );
			} else
			{
				xml.put ( ">" );
			}

			for ( size_t index = 0; index < var->dat.str.len; index++ )
			{
				if ( var->dat.str.c[index] == ' ' )
				{
					xml.put ( '+' );
				} else if ( _isurl ( &var->dat.str.c[index] ) )
				{
					xml.printf ( "%%%02X", var->dat.str.c[index] );
				} else
				{
					xml.put ( var->dat.str.c[index] );
				}
			}
			xml.write ( "<\\string>" );
			data.indentLevel -= data.indentAmount;
			if ( data.indentLevel )
			{
				xml.write ( "\r\n", 2 );
			}
			break;

			// we CAN'T iterate locally unless we put our .v into the varStack (may have references to it)... easier just to recurse
		case slangType::eREFERENCE:
		case slangType::eOBJECT_ROOT:
		case slangType::eARRAY_ROOT:
			vmObjToXMLIterate ( instance, var->dat.ref.v, xml, data );
			break;

		case slangType::eARRAY_SPARSE:
			data.indentLevel += data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}
			xml.write ( "<array xmlns:fgl='http://www.fgldev.org/schema'" );
			if ( data.emitType )
			{
				xml.write ( " fgl:type=\"variablslangType::eARRAY_SPARSE\">\r\n" );
			} else
			{
				xml.write ( ">\r\n" );
			}
			data.indentLevel += data.indentAmount;
			vmObjToXMLIterate ( instance, var->dat.aSparse.v, xml, data );
			data.indentLevel -= data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}
			xml.write ( "<\\array>" );
			if ( data.indentLevel )
			{
				xml.write ( "\r\n", 2 );
			}
			break;

		case slangType::eARRAY_FIXED:
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}
			xml.write ( "<array xmlns:fgl='http://www.fgldev.org/schema'" );
			if ( data.emitType )
			{
				xml.write ( " fgl:type=\"variablslangType::eARRAY_SPARSE\">\r\n" );
			} else
			{
				xml.write ( ">\r\n" );
			}
			data.indentLevel += data.indentAmount;
			for ( int64_t index = 0; index <= var->dat.arrayFixed.endIndex - var->dat.arrayFixed.startIndex; index++ )
			{
				for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
				{
					xml.put ( ' ' );
				}
				xml.printf ( "<elem index=\"%i\">\r\n", var->dat.arrayFixed.startIndex + index );
				data.indentLevel += data.indentAmount;
				vmObjToXMLIterate ( instance, &var[index + 1], xml, data );
				data.indentLevel -= data.indentAmount;
				for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
				{
					xml.put ( ' ' );
				}
				xml.printf ( "</elem>\r\n", var->dat.arrayFixed.startIndex + index );
			}
			data.indentLevel -= data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}
			xml.write ( "<\\array>" );
			if ( data.indentLevel )
			{
				xml.write ( "\r\n", 2 );
			}
			break;

		case slangType::eARRAY_ELEM:
			/* undo the type storage... we'll do it in the loop */
			while ( var )
			{
				for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
				{
					xml.put ( ' ' );
				}
				xml.printf ( "<A%i>", var->dat.aElem.elemNum );
				data.indentLevel += data.indentAmount;
				vmObjToXMLIterate ( instance, var->dat.aElem.var, xml, data );
				data.indentLevel -= data.indentAmount;
				xml.printf ( "</A%i>\r\n", var->dat.aElem.elemNum );

				var = var->dat.aElem.next;
			}
			break;

		case slangType::eOBJECT:
			data.indentLevel += data.indentAmount;
			for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
			{
				xml.put ( ' ' );
			}

			xml.write ( "<object xmlns:fgl='http://www.fgldev.org/schema'" );

			if ( data.emitType )
			{
				xml.printf ( " fgl:type=\"object\" fgl:class=\"%s\">\r\n", var->dat.obj.classDef->name );
			} else
			{
				xml.put ( ">" );
			}

			{
				auto classDef = var->dat.obj.classDef;

				data.indentLevel += data.indentAmount;
				for ( size_t loop = 0; loop < classDef->nElements; loop++ )
				{
					auto elem = &classDef->elements[loop];
					switch ( classDef->elements[loop].type )
					{
						case fgxClassElementType::fgxClassType_iVar:
						case fgxClassElementType::fgxClassType_static:
							for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
							{
								xml.put ( ' ' );
							}
							xml.printf ( "<%s>\r\n", elem->fullName );
							data.indentLevel += data.indentAmount;
							if ( elem->isStatic )
							{
								vmObjToXMLIterate ( instance, classDef->loadImage->globals[elem->methodAccess.globalEntry], xml, data );
							} else
							{
								if ( elem->isVirtual )
								{
									vmObjToXMLIterate ( instance, &var[var->dat.obj.vPtr[elem->methodAccess.vTableEntry].delta], xml, data );
								} else
								{
									vmObjToXMLIterate ( instance, &var[elem->methodAccess.iVarIndex], xml, data );
								}
							}
							data.indentLevel -= data.indentAmount;
							for ( int64_t loop = 0; loop < data.indentLevel; loop++ )
							{
								xml.put ( ' ' );
							}
							xml.printf ( "<\\%s>\r\n", elem->fullName );
							break;
						default:
							break;
					}
				}
				data.indentLevel -= data.indentAmount;
			}
			xml.write ( "<\\object>" );
			data.indentLevel -= data.indentAmount;
			if ( data.indentLevel )
			{
				xml.write ( "\r\n", 2 );
			}
			break;
		default:
			throw errorNum::scINVALID_PARAMETER;
	}
	data.varStack.pop_back ( );
}

static VAR_OBJ vmObjUnpackWeb ( vmInstance *instance, VAR_OBJ *obj, VAR_STR *strP, VAR_STR *sep )
{
	auto str = strP->dat.str.c;

	while ( *str )
	{
		stringi name;

		// parse name
		while ( *str )
		{
			if ( *str == '=' )
			{
				str++;
				break;
			} else if ( *str == sep->dat.str.c[0] )
			{
				break;
			} else
			name += *str;
		}

		BUFFER value;

		while ( *str && *str != sep->dat.str.c[0] )
		{
			if ( *str == '%' )
			{
				size_t val = 0 ;
				str++;
				if ( *str )
				{
					val = hexDecodingTable[(unsigned char)*str];
					str++;
					if ( *str )
					{
						val = val << 4 | hexDecodingTable[(unsigned char)*str];
						str++;
					}
				}
				value.put ( static_cast<uint8_t>(val) );
			} else if ( *str == '+' )
			{
				value.put ( ' ' );
				str++;
			} else
			{
				value.put ( *str );
				str++;
			}
		}

		if ( value.size() )
		{
			*(*obj)[name] = VAR_STR ( instance, value );
		}
		if ( *str )
		{
			str++;
		}
	}
	return obj;
}

static VAR_OBJ vmUnpack2 ( vmInstance *instance, VAR_OBJ *obj, VAR_STR *strP, VAR_STR *sep, VAR_STR *fieldsP )
{
	auto str = strP->dat.str.c;
	auto fields = fieldsP->dat.str.c;

	while ( *str && *fields )
	{
		stringi name;

		// parse name
		while ( *fields )
		{
			if ( *fields == ' ' )
			{
				fields++;
				break;
			} else
			{
				name += *fields;
			}
		}

		BUFFER value;

		while ( *str && *str != sep->dat.str.c[0] )
		{
			value.put ( *str );
			str++;
		}

		if ( value.size () )
		{
			*(*obj)[name] = VAR_STR ( instance, value );
		}
		if ( *str )
		{
			str++;
		}
	}
	return obj;
}


static VAR_STR vmObjToXML ( vmInstance *instance, VAR_OBJ *var, bool emitType, int64_t indentAmount )
{
	VAR arr = VAR_ARRAY ( instance );

	IterateData	data ( instance, true, true, &arr );
	data.emitType = emitType;
	data.indentLevel = -indentAmount;
	data.indentAmount = indentAmount;

	BUFFER name;

	vmObjToXMLIterate ( instance, var, name, data );

	return VAR_STR ( instance, name.data<char *>( ), name.size ( ) );
}

void builtinPackInit ( class vmInstance *instance, opFile *file )

{
	REGISTER( instance, file );
		FUNC ( "serialize", vmPack );
		FUNC ( "deserialize", vmUnPack, DEF ( 2, "null" ) );
		FUNC ( "pack", vmPack3 );
		FUNC ( "unPack", vmUnPack, DEF ( 2, "null" ) );
		FUNC ( "packLibraries", vmPackLibraries ) CONST PURE;
		FUNC ( "isPackString", vmIsPackString ) CONST PURE;
		FUNC ( "objToArray", vmObjToArray, DEF ( 2, "0" ), DEF ( 3, "1" ) ) CONST PURE;
		FUNC ( "objToXML", vmObjToXML, DEF ( 2, "1" ), DEF ( 3, "2" ) ) CONST PURE;

		FUNC ( "objUnpackWeb", vmObjUnpackWeb );
		FUNC ( "__unpack2", vmUnpack2 );
	END
}
