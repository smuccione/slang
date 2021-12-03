/*

	document object model 

*/

#pragma once

extern	void domParse		( class vmInstance *instance, struct VAR_OBJ *docRef, char *pageData, int64_t tagsOnly );
extern	void xmlParse		( class vmInstance *instance, struct VAR_OBJ *docRef, char *pageData, bool tagsOnly );
extern	void builtinDOMInit	( class vmInstance *instance, class opFile *file );
