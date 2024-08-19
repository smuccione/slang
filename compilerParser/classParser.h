/*

	class parser definitions

*/

#pragma once

#include "expressionParser.h"
#include "funcParser.h"
#include "symbolSpace.h"
#include "errorHandler.h"
#include "Utility/stringCache.h"

class opClassElement {
public:
	fgxClassElementType					 type;
	fgxClassElementScope				 scope;

	cacheString							 name;
	symbolTypeClass						 symType;

	stringi								 documentation;

	std::mutex							 accessorsAccess;
	std::unordered_set<accessorType>	 accessors;						// for type inferencing in case our type changes
	bool								 typeChanged = false;

	srcLocation							 location;
	bool								 isVirtual = false;
	bool								 isStatic = false;

	// due to the nature of the language and the type inferencing system, if we have a call to a virtual method, that call may call the virtual method, or any of the virtual overriders.
	// it's necessary to ensure that parameter types are propogated to all potential overriders

	struct data {
		struct {
			astNode							*initializer = nullptr;
			uint32_t						 index = 0;
			cacheString						 symbolName;
		} iVar;
		struct {
			cacheString						 func;
			std::set<class opFunction *>	 virtOverrides;				// for parameter type propagation; 
		} method;
		struct
		{
			cacheString						 access;
			cacheString						 assign;
			std::set<opFunction *>			 accessVirtOverrides;		// for parameter type propagation;
			std::set<opFunction *>			 assignVirtOverrides;		// for parameter type propagation;
		}  prop;
		struct {
			// new name is the name of THIS class element   this is ultimately what we add to the visible elements map/vector when building the class
			cacheString						 oldName;		//this is the decorated name of the thing to rename
		} message;

		data ()
		{}

		data ( data const &old )
		{
			if ( iVar.initializer ) iVar.initializer = new astNode ( *old.iVar.initializer );
			iVar.index = old.iVar.index;
			iVar.symbolName = old.iVar.symbolName;
			method.func = old.method.func;
			method.virtOverrides = old.method.virtOverrides;
			prop.access = old.prop.access;
			prop.accessVirtOverrides = old.prop.accessVirtOverrides;
			prop.assign = old.prop.assign;
			prop.assignVirtOverrides = old.prop.assignVirtOverrides;
			message.oldName = old.message.oldName;
		}
	} data;

	opClassElement ()
	{
		type = fgxClassElementType::fgxClassType_method;
		scope = fgxClassElementScope::fgxClassScope_public;
	}

	opClassElement ( opClassElement const &old )
	{
		name			= old.name;
		documentation	= old.documentation;
		type			= old.type;
		scope			= old.scope;
		location		= old.location;
		isVirtual		= old.isVirtual;
		isStatic		= old.isStatic;
		if ( old.data.iVar.initializer )
		{
			data.iVar.initializer = new astNode ( *old.data.iVar.initializer );
		}
		data = old.data;
	}

	opClassElement &operator = ( opClassElement &&old )	noexcept 
	{
		std::swap ( name, old.name );
		std::swap ( documentation, old.documentation );
		std::swap ( type, old.type );
		std::swap ( scope, old.scope );
		std::swap ( location, old.location );
		std::swap ( isVirtual, old.isVirtual );
		std::swap ( isStatic, old.isStatic );
		std::swap ( data, old.data );
		return *this;
	}

	~opClassElement()
	{
		switch ( type )
		{
			case fgxClassElementType::fgxClassType_iVar:
			case fgxClassElementType::fgxClassType_static:
			case fgxClassElementType::fgxClassType_const:
			case fgxClassElementType::fgxClassType_inherit:
				if ( data.iVar.initializer )
					delete data.iVar.initializer;
				break;
			default:
				break;
		}
	}

	opClassElement ( class opFile *file, char const ** expr, class opClass *classDef, class sourceFile *src );

	void	serialize ( BUFFER *buff );

	bool operator<(opClassElement &rhs)  
	{ 
		return ( strccmp ( name.c_str(), rhs.name.c_str() ) < 0 ? 1 : 0 );
	}

	bool setType ( bool isAccess, class symbolTypeClass const &sType, accessorType const &acc, unique_queue<accessorType> *scanQueue )
	{
		switch ( type )
		{
			case fgxClassElementType::fgxClassType_iVar:
				if ( (typeChanged = (symType &= sType)) )
				{
					if ( scanQueue ) scanQueue->push ( acc );
					for ( auto& it : accessors )
					{
						if ( scanQueue ) scanQueue->push ( it );
					}
					return true;
				}
				return false;
			default:
				return false;
		}
	}

	bool setTypeNoThrow ( bool isAccess, class symbolTypeClass const &sType, accessorType const &acc, unique_queue<accessorType> *scanQueue )
	{
		switch ( type )
		{
			case fgxClassElementType::fgxClassType_iVar:
				if ( (typeChanged = (symType ^= sType)) )
				{
					if ( scanQueue ) scanQueue->push ( acc );
					for ( auto& it : accessors )
					{
						if ( scanQueue ) scanQueue->push ( it );
					}
					return true;
				}
				return false;
			case fgxClassElementType::fgxClassType_static:
//				sym->setType ( it.symbolName, false, rightType, acc, scanQueue );
				return false;
			default:
				return false;
		}
	}
	symbolTypeClass const &getType ( bool isAccess )
	{
		return symType;
	}

	void setAccessed ( class opFile *file, cacheString const &name, bool isAccess, accessorType const &acc, unique_queue<accessorType> *scanQueue );

	cacheString getSymbolName ()
	{
		return name;
	}
};

class opClass {
public:
	bool							  inUse = false;

	cacheString						  name;
	stringi							  documentation;

	symbolTypeClass					  type;

	bool							  isInterface = false;
	bool							  isFinal = false;

	srcLocation						  location;
	srcLocation						  nameLocation;

	std::vector<cacheString>	      requiredClasses;							// list of required classes for FFI functions, these may be instantiated without visbility so we need to 
	std::vector<opClassElement *>	  elems;
	std::vector<opClassElement *>	  opOverload;

	std::recursive_mutex			  accessorAccess;
	std::unordered_set<accessorType>  accessors;						// for type inferencing in case our type changes
	opUsingList						  usingList;

	class compClass					 *cClass = 0;

    void							(*cGarbageCollectionCB)	( class vmInstance *instance, class objectMemory *om, struct VAR *val, struct VAR *(*move)( class objectMemory *om, struct VAR *val, bool copy) ) = 0;
    void							(*cPackCB)				( class vmInstance *instance, struct VAR *var, struct BUFFER *buff, void *param, void (*pack)(struct VAR *val, void *param) ) = 0;
	void							(*cUnPackCB)			( class vmInstance *instance, struct VAR *obj, unsigned char const ** buff, uint32_t *len, void *param, struct VAR *(*unPack)( unsigned char const ** buff, uint32_t *len, void *param ) ) = 0;


	void sortElements()
	{
		sort ( elems.begin(), elems.end() );
	}

	uint32_t	getIndex ( cacheString const &name )
	{
		uint32_t index = 0;
		for ( auto it = elems.begin(); it != elems.end(); it++, index++ )
		{
			if ( name == (*it)->name )
			{
				return index;
			}
		}
		return -1;
	}

	opClass();
	opClass ( class opFile *file, sourceFile *srcFile, char const ** expr );
	~opClass();

	void			 serialize				( BUFFER *buff );

	errorNum		 add					( cacheString const &name,	fgxClassElementType type, fgxClassElementScope scope, srcLocation const &location, bool isVirtual, stringi const &documentation );
	errorNum		 addConst				( astNode *node,			fgxClassElementType type, fgxClassElementScope scope, stringi const &documentation );
	errorNum		 addIVar				( astNode *node,			fgxClassElementType type, fgxClassElementScope scope, stringi const &documentation );
	errorNum		 addMessage				( cacheString const &name,	cacheString const &oldName,  srcLocation const &location, stringi const &documentation );
	errorNum		 add					( cacheString const &name,	fgxClassElementType type,	fgxClassElementScope scope, astNode *initializer, stringi const &documentation );
	errorNum		 addProperty			( cacheString const &name,	bool isAccess,				fgxClassElementScope scope, bool isVirtual, opFunction *func, class opFile *file, symbolTypeClass const &symType, stringi const &documentation );
	errorNum		 addMethod				( cacheString const &name,								fgxClassElementType type, fgxClassElementScope scope, bool isVirtual, bool isStatic, bool isExtension, opFunction *func, class opFile *file, symbolTypeClass const &symType, bool addFunc, stringi const &documentation );
	errorNum		 addOp					( cacheString const &name,	fgxOvOp op,					fgxClassElementType type, fgxClassElementScope scope, opFunction *func, opFile *file, bool isVirtual, symbolTypeClass const &symType, stringi const &documentation );
};

