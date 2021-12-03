/*

	object memory header

*/

#pragma once

#include <vector>
#include <tuple>

#include "bcVM/fglTypes.h"

#if 1
constexpr size_t OM_MAX_ROUNDS = 4;
constexpr size_t OM_VAR_SIZE_DEFAULT = 1024 * 1024 * 4;	/* individual page size for garbage collector, should be relatively small to minimize page detachment in the server		*/
constexpr size_t OM_PAGE_SIZE_DEFAULT = 1024 * 1024 * 4;	/* individual page size for garbage collector, should be relatively small to minimize page detachment in the server		*/
constexpr size_t OM_NEW_PAGES_BEFORE_COLLECTION = 16;			/* should be relatively big to minimize garbage collection (size so that the majority of web pages never need collection	*/
constexpr size_t OM_NEW_VAR_PAGES_BEFORE_COLLECTION = 16;			/* should be relatively big to minimize garbage collection (size so that the majority of web pages never need collection	*/
constexpr size_t OM_GENGC_VARS_BEFORE_COLLECTION = OM_VAR_SIZE_DEFAULT * OM_NEW_VAR_PAGES_BEFORE_COLLECTION;
#else
constexpr size_t OM_MAX_ROUNDS = 4;
constexpr size_t OM_VAR_SIZE_DEFAULT = 32;// 1024 * 1024 * 4;	/* individual page size for garbage collector, should be relatively small to minimize page detachment in the server		*/
constexpr size_t OM_PAGE_SIZE_DEFAULT = 1024 * 1024 * 4;	/* individual page size for garbage collector, should be relatively small to minimize page detachment in the server		*/
constexpr size_t OM_NEW_PAGES_BEFORE_COLLECTION = 1;//6;			/* should be relatively big to minimize garbage collection (size so that the majority of web pages never need collection	*/
constexpr size_t OM_NEW_VAR_PAGES_BEFORE_COLLECTION = 1;//6;			/* should be relatively big to minimize garbage collection (size so that the majority of web pages never need collection	*/
constexpr size_t OM_GENGC_VARS_BEFORE_COLLECTION = OM_VAR_SIZE_DEFAULT * OM_NEW_VAR_PAGES_BEFORE_COLLECTION;
#endif

typedef void ( *omCollectionCallback )(void *userParam);

class omCallback
{
	public:
	omCollectionCallback	cb;
	void					*param;
};

class objectMemoryPage
{
};

struct omDestructorInfo
{
};

class GRIP
{
	class objectMemory	*om;
	public:
	GRIP ()
	{
		om = 0;
	}
	GRIP ( vmInstance *instance );
	GRIP ( objectMemory *om );
	GRIP ( GRIP &&old ) noexcept;
	GRIP ( GRIP const &old ) = delete;
	virtual ~GRIP ();
};

class objectMemory
{
	protected:
	std::vector<std::tuple<omCollectionCallback, void *> >	collectionCb;

	void callCollectionCb ()
	{
		for ( auto &it : collectionCb )
		{
			std::get<0> ( it )(std::get<1> ( it ));
		}
	}

	virtual void				collectionDisable ( void ) = 0;
	virtual void				collectionEnable ( void ) = 0;

	public:
	virtual ~objectMemory () {};

	virtual void				 reset ( void ) = 0;

	virtual void				*alloc ( size_t size ) = 0;
	/* note: we never need to allocate into a specific generation... the mere act of CONNECTING to something into an older generation changes the older entry
			 and generates a hit into the card table which will force us to move it into the older generation upon collection
	*/
	virtual VAR					*allocVar ( size_t count ) = 0;

	// should ONLY ever be called when in a collection (e.g. from a GC callback)
	virtual	void				*allocGen ( size_t len, size_t gen ) = 0;
	virtual VAR					*allocVarGen ( size_t count, size_t gen, size_t round ) = 0;	// allocat a VAR from a specific generation

	virtual char				*strDup ( char const *str ) = 0;
	virtual bool				 isInGCMemory ( void *ptr ) = 0;

	virtual	size_t				 getAge ( char const * ptr ) const { return 0; }
	virtual	size_t				 getAge ( VAR const *var ) const { return 0; }
	virtual	size_t				 getRound ( VAR const *var ) const { return 0; }

	virtual void				 registerDestructor ( VAR *obj ) = 0;
	virtual void				 processDestructors () = 0;
	virtual objectMemoryPage	*detachPage ( void *ptr ) = 0;
	virtual size_t				 memoryState ( void ) = 0;
	virtual void				 freeMemoryPage ( objectMemoryPage	*page ) = 0;
	virtual void				 collect ( size_t p = 0 ) = 0;

	virtual void				 printStats ( void ) {};

	struct collectionVariables
	{
		collectionVariables ( )
		{};
		virtual bool doCopy ( void const *ptr ) 
		{ 
			return true;
		};
		virtual size_t getAge ( )
		{
			return 0;
		}
	};

	struct copyVariables
	{
	};

	// support for garbage collection of vars
	virtual	VAR *move ( vmInstance *instance, VAR **newAddr, struct VAR *val, bool copy, collectionVariables *col ) = 0;

	friend GRIP::GRIP ( class vmInstance *instance );
	friend GRIP::GRIP ( class objectMemory *om );
	friend GRIP::~GRIP ();
};
