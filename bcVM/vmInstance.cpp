/*
	instance management code

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "vmOmGenerational.h"

void vmInstance::allocateEvalStack ( size_t nEntries )
{
	eval =(VAR *)::malloc ( sizeof ( VAR ) * nEntries );
}

void vmInstance::freeEvalStack ( void )
{
	::free ( eval );
}

VAR_STR::VAR_STR ( class vmInstance *instance, std::string const &str )
{
	// need to do this as "this" may change upon collection (e.g. the object this is pointing at may move so even though we're an instantiated object we can't access any ivars directly after an alloc call)
	VAR *self = this;

	type = slangType::eNULL;
	auto tmp = (char *) instance->om->alloc ( str.size () + 1 );
	self->dat.str.c = tmp;
	memcpy ( tmp, str.c_str (), str.size () + 1 );
	self->dat.str.len = str.size ();		// don't include terminating null
	type = slangType::eSTRING;
}

VAR_STR::VAR_STR ( class vmInstance *instance, stringi const &str )
{
	// need to do this as "this" may change upon collection (e.g. the object this is pointing at may move so even though we're an instantiated object we can't access any ivars directly after an alloc call)
	VAR *self = this;

	type = slangType::eNULL;
	auto tmp = (char *)instance->om->alloc ( str.size () + 1 );
	self->dat.str.c = tmp;
	memcpy ( tmp, str.c_str (), str.size () + 1 );
	self->dat.str.len = str.size ();		// don't include terminating null
	type = slangType::eSTRING;
}

VAR_STR::VAR_STR ( class vmInstance *instance, struct BUFFER &buff )
{
	// need to do this as "this" may change upon collection (e.g. the object this is pointing at may move so even though we're an instantiated object we can't access any ivars directly after an alloc call)
	VAR *self = this;

	type = slangType::eNULL;
	auto tmp = (char *) instance->om->alloc ( buff.size() + 1 );
	self->dat.str.c = tmp;
	memcpy ( tmp, buff.data<char *> (), buff.size ( ) + 1 );
	self->dat.str.len = buff.size ( );		// don't include terminating null
	tmp[self->dat.str.len] = 0;
	type = slangType::eSTRING;
}

VAR_STR::VAR_STR ( class vmInstance *instance, char *str )
{
	assert ( str );
	// need to do this as "this" may change upon collection (e.g. the object this is pointing at may move so even though we're an instantiated object we can't access any ivars directly after an alloc call)
	VAR *self = this;

	type = slangType::eNULL;
	self->dat.str.len = strlen ( str );
	auto tmp = (char *) instance->om->alloc ( self->dat.str.len + 1 );
	self->dat.str.c = tmp;
	memcpy ( tmp, str, self->dat.str.len + 1 );
	type = slangType::eSTRING;
}

VAR_STR::VAR_STR ( class vmInstance *instance, char const *str )
{
	assert ( str );
	// need to do this as "this" may change upon collection (e.g. the object this is pointing at may move so even though we're an instantiated object we can't access any ivars directly after an alloc call)
	VAR *self = this;

	type = slangType::eNULL;
	self->dat.str.len = strlen ( str );
	auto tmp = (char *) instance->om->alloc ( self->dat.str.len + 1 );
	self->dat.str.c = tmp;
	memcpy ( tmp, str, self->dat.str.len + 1 );
	type = slangType::eSTRING;
}

VAR_STR::VAR_STR ( class vmInstance *instance, char const *str, size_t len )
{
	assert ( str );
	// need to do this as "this" may change upon collection (e.g. the object this is pointing at may move so even though we're an instantiated object we can't access any ivars directly after an alloc call)
	VAR *self = this;

	type = slangType::eNULL;
	self->dat.str.len = len;
	auto tmp = (char *) instance->om->alloc ( len + 1 );
	self->dat.str.c = tmp;
	memcpy ( tmp, str , len );
	tmp[len] = 0;
	type = slangType::eSTRING;
}

VAR_ARRAY::VAR_ARRAY ( class vmInstance *instance )
{
	type = slangType::eARRAY_ROOT;
	dat.ref.obj = (VAR *)instance->om->allocVar ( 1 );
	dat.ref.v = dat.ref.obj;

	dat.ref.v->type = slangType::eARRAY_SPARSE;
	dat.ref.v->dat.aSparse.v = 0;
	dat.ref.v->dat.aSparse.lastAccess = 0;
	dat.ref.v->dat.aSparse.maxI = 0;

	dat.ref.obj = dat.ref.v;
}

VAR_OBJ::VAR_OBJ ( VAR *var )
{
	while ( var->type == slangType::eREFERENCE ) var = var->dat.ref.v;

	if ( var->type == slangType::eOBJECT )
	{
		type = slangType::eOBJECT_ROOT;
		dat.ref.v = var;
		dat.ref.obj = var;
	} else if ( var->type == slangType::eOBJECT_ROOT )
	{
		if ( !var->dat.ref.v ) throw errorNum::scSEND_TO_NONOBJECT;

		assert ( var->dat.ref.v->type == slangType::eOBJECT );

		*(static_cast<VAR *>(this)) = *var;
	}
}

VAR_OBJ::VAR_OBJ ( VAR *root, VAR *var )
{
	while ( var->type == slangType::eREFERENCE ) var = var->dat.ref.v;

	if ( var->type == slangType::eOBJECT )
	{
		assert ( root->type == slangType::eOBJECT_ROOT );
		type = slangType::eOBJECT_ROOT;
		dat.ref.v = var;
		dat.ref.obj = root->dat.ref.obj;
	} else if ( var->type == slangType::eOBJECT_ROOT )
	{
		if ( !var->dat.ref.v ) throw errorNum::scSEND_TO_NONOBJECT;

		assert ( var->dat.ref.v->type == slangType::eOBJECT );

		type = slangType::eOBJECT_ROOT;
		dat.ref.v = var->dat.ref.v;
		dat.ref.obj = root;

		*(static_cast<VAR *>(this)) = *var;
	}
}

VAR_OBJ::VAR_OBJ ( VAR &varP )
{
	auto var = &varP;
	while ( var->type == slangType::eREFERENCE ) var = var->dat.ref.v;

	if ( var->type == slangType::eOBJECT )
	{
		type = slangType::eOBJECT_ROOT;
		dat.ref.v = var;
		dat.ref.obj = var;
	} else if ( var->type == slangType::eOBJECT_ROOT )
	{
		if ( !var->dat.ref.v ) throw errorNum::scSEND_TO_NONOBJECT;

		assert ( var->dat.ref.v->type == slangType::eOBJECT );

		*(static_cast<VAR *>(this)) = *var;
	}
}

VAR *VAR_OBJ::operator [] ( char const *name )
{
	return classIVarAccess ( this, name );
}

void *VAR_OBJ::allocCargo ( class vmInstance *instance, size_t len )
{
	assert ( sizeof ( size_t ) < 16 );
	dat.ref.v->dat.obj.cargo = instance->om->allocGen ( sizeof ( uint8_t ) * (len + 16), 0 );
	*(size_t *) dat.ref.v->dat.obj.cargo = sizeof ( uint8_t ) * len;

	return (uint8_t *) dat.ref.v->dat.obj.cargo + 16;
}

void *VAR_OBJ::allocCargo ( size_t age, class vmInstance *instance, size_t len )
{
	assert ( sizeof ( size_t ) < 16 );
	dat.ref.v->dat.obj.cargo = instance->om->allocGen ( sizeof ( uint8_t ) * (len + 16), age );
	*(size_t *) dat.ref.v->dat.obj.cargo = sizeof ( uint8_t ) * len;

	return (uint8_t *) dat.ref.v->dat.obj.cargo + 16;
}

VAR *VAR::operator[] ( int64_t index )
{
	VAR		*val = this;
	VAR		*elem;
	VAR		*sparseRoot;

	while ( TYPE ( val ) == slangType::eREFERENCE )
	{
		val = val->dat.ref.v;
	}

	if ( TYPE ( val ) == slangType::eARRAY_ROOT ) val = val->dat.ref.v;

	sparseRoot = val;

	for ( ;; )
	{
		switch ( TYPE ( val ) )
		{
			case slangType::eARRAY_SPARSE:
				if ( val->dat.aSparse.v )
				{
					sparseRoot = val;
					val = val->dat.aSparse.v;
				} else
				{
					return 0;
				}
				break;

			case slangType::eARRAY_FIXED:
				if ( (index >= val->dat.arrayFixed.startIndex) && (index <= val->dat.arrayFixed.endIndex) )
				{
					return &val[index - val->dat.arrayFixed.startIndex + 1];
				} else
				{
					return 0;
				}
				break;

			case slangType::eARRAY_ELEM:
				if ( val->dat.aElem.elemNum > index )
				{
					return 0;
				}

				if ( sparseRoot->dat.aSparse.lastAccess && (sparseRoot->dat.aSparse.lastAccess->dat.aElem.elemNum <= index) )
				{
					elem = sparseRoot->dat.aSparse.lastAccess;
				} else
				{
					elem = sparseRoot->dat.aSparse.v;
				}

				while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < index) )
				{
					elem = elem->dat.aElem.next;
				}

				while ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum < index) )
				{
					elem = elem->dat.aElem.next;
				}

				// in all cases, after this if series, elem must point to the array element being accessed (so that the lastAccess can be set correctly
				if ( elem->dat.aElem.elemNum == index )
				{
				} else if ( elem->dat.aElem.next && (elem->dat.aElem.next->dat.aElem.elemNum == index) )
				{
					elem = elem->dat.aElem.next;
				} else
				{
					return 0;
				}
				sparseRoot->dat.aSparse.lastAccess = elem;
				return elem->dat.aElem.var;
			case slangType::eNULL:
				return 0;

			case slangType::eREFERENCE:
				val = val->dat.ref.v;
				break;

			default:
				throw errorNum::scNONARRAY;
		}
	}

	return val;
}

VAR *vmInstance::getGlobal ( char const *nSpace, char const *name ) const
{
	bcLoadImage	*image;
	uint32_t	 gEntry;
	int32_t	 r;

	image = atomTable->atomGetLoadImage ( nSpace );

	gEntry = image->globalSymbolTableRoot;

	while ( gEntry != (uint32_t )-1 )
	{
		r = strccmp ( name, image->globalSymbolTable[gEntry].name );
		if ( !r )
		{
			return image->globals[image->globalSymbolTable[gEntry].index];
		} if ( r < 0 )
		{
			gEntry = image->globalSymbolTable[gEntry].left;
		} else
		{
			gEntry = image->globalSymbolTable[gEntry].right;
		}
	}
	return nullptr;
}

std::tuple<VAR *, bcLoadImage *, uint32_t> vmInstance::findGlobal ( char const *name, bcLoadImage *ignoreImage ) const
{
	std::tuple<VAR *, bcLoadImage *, uint32_t>	ret = { nullptr, nullptr, 0 };
	atomTable->typeMap ( atom::atomType::aLOADIMAGE, [&]( uint32_t index )
						 {
							 auto image = atomTable->atomGetLoadImage ( index );

							 if ( image != ignoreImage )
							 {
								 auto gEntry = image->globalSymbolTableRoot;

								 while ( gEntry != (uint32_t) -1 )
								 {
									 auto r = strccmp ( name, image->globalSymbolTable[gEntry].name );
									 if ( !r )
									 {
										 ret = { image->globals[image->globalSymbolTable[gEntry].index], image, image->globalSymbolTable[gEntry].index };
										 return true;
									 } if ( r < 0 )
									 {
										 gEntry = image->globalSymbolTable[gEntry].left;
									 } else
									 {
										 gEntry = image->globalSymbolTable[gEntry].right;
									 }
								 }
							 }
							 return false;
						 }
	);
	return ret;
}

vmInstance::vmInstance ( char const *name )
{
	result.type			= slangType::eNULL;
}

void vmInstance::init ( void )
{
	outputDesc = 0;
	if ( !eval )
	{
		allocateEvalStack ( TASK_EVAL_STACK_SIZE );

		nStack				= TASK_EVAL_STACK_SIZE;

		// set our stack pointer
		stack				= eval;
	}

	// initialize the VM runtine stuff 
	if ( !atomTable ) atomTable	= new class atomTable	( MAX_ATOMS, this );
	if ( !om ) om = new omGenGc ( this );
	if ( !workareaTable ) workareaTable	= new vmWorkareaTable	( this, 256, 256 );

	initialized			= true;
}

void vmInstance::allocateAtomTable ( void )
{
	if ( !atomTable ) atomTable = new class atomTable ( MAX_ATOMS, this );
}


vmInstance::~vmInstance()
{
	vmInstance::release();
}

void vmInstance::release ( void )
{
	if ( initialized )
	{
		om->processDestructors();

		_resourceEndAll ();

		delete	om;
		delete	atomTable;
		delete  workareaTable;
	}
	initialized = false;
}

void vmInstance::duplicate ( class vmInstance *old )
{
	if ( debug && old->debug ) debug->combine ( old->debug.get() );
}

void vmInstance::reset ( void )
{
	if ( initialized )
	{
		error.valid = false;
		om->reset();
		if ( atomTable ) atomTable->reset();
		workareaTable->reset();
		_resourceEndAll ();
		if ( debug ) debug->reset ();

		// empty the return stack
		while ( !coRoutineStack.empty() )
		{
			coRoutineStack.pop();
		}

		currentCoroutine = mainCoroutine;
		stack = eval;
	}
}
