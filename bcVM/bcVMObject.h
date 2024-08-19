/*

   Object/Class support - SLANG compiler

*/

#pragma once

#include <stdint.h>

extern	struct	VAR_OBJ	*classNew			( class vmInstance *instance, char const *name, int nParams );
extern	struct	VAR		*classIVarAccess	( struct VAR *obj, char const *name );
extern	bool			 classCallUnpack	( class vmInstance *instance, struct VAR *obj );
extern	bool			 classCallPack		( class vmInstance *instance, struct VAR *obj );
extern	bool			 classCallRelease	( class vmInstance *instance, struct VAR *obj );
extern	bool			 classCallMethod	( class vmInstance *instance, struct VAR *obj, char const *name, int nParams );
extern	void			*classAllocateCargo ( class vmInstance *instance, struct VAR *var, size_t len );
extern	void			*classGetCargo		( struct VAR *var );
extern	size_t			 classGetCargoSize	( struct VAR *var );