/*
	atom heade file

*/
#pragma once

#include <stdint.h>
#include <cassert>
#include <new>
#include <vector>
#include <map>
#include <set>
#include "compilerParser/fglErrors.h"
#include "Utility/stringi.h"

typedef void (*vmWorkareaCB)		( class vmInstance *instance, void *data, char const *name, struct VAR *var );
typedef void (*vmWorkareaCloseCB)	( class vmInstance *instance, void *data );

struct vmWorkarea {
	bool	 used = false;

	char	 alias[MAX_NAME_SZ];
	void	*data = nullptr;

	vmWorkareaCB		read = nullptr;
	vmWorkareaCB		write = nullptr;
	vmWorkareaCloseCB	close = nullptr;
};

class vmWorkareaTable {
	std::vector<vmWorkarea>			 entries;
	std::set<int64_t>				 freeEntries;
	std::map<stringi, int64_t>		 aliasMap;
	std::map<void *, int64_t>		 dataMap;
	class vmInstance				*instance;
	std::vector<int64_t>			 stack;
public:
	int64_t							 nStack;

	vmWorkareaTable ( class vmInstance *instance, int64_t nEntries, int64_t stackDepth );
	~vmWorkareaTable();

	void		 reset			( void );

	auto		 getNumAreas	( void ) { return entries.size(); }

	char		*getAlias		( int64_t entry );
	char		*getAlias		( void *data );
	void		 push			( int64_t entry );
	void		 push			( char const *alias );
	void		 push			( VAR *var );
	void		 pop			( int64_t num = 1 );
	void		 select			( int64_t entry );
	void		 select			( char const *alias );
	bool		 inUse			( int64_t num );
	int64_t		 alloc			( char const *alias, void *data, vmWorkareaCB read, vmWorkareaCB write, vmWorkareaCloseCB close );
	void		 free			( int64_t  entry );
	void		 free			( void *data );

	void		*getData		( void );
	void		 setData		( void *data );
	void		*getData		( int64_t entry );
	void		 setData		( int64_t entry, void *data );
	int64_t		 getDefault		( void ) { return stack[0]; }

	void		 read			( char const *fieldName, VAR *var );			// reads from the 
	void		 read			( VAR *fieldName, VAR *var );			// reads from the 
	void		 write			( char const *fieldName, VAR *var );			// writes to the field
	void		 write			( VAR *fieldName, VAR *var );			// writes to the field 
};
