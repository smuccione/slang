// op_variant.h

#pragma once

#include "bcVM/exeStore.h"
#include <Utility/util.h>

static __forceinline bcClassSearchEntry *findClassEntry ( bcClass *callerClass, char const *name, fgxClassElemSearchType type )
{
	if ( strchr ( name, ':' ) )
	{
		// use the namespace tree
		auto cEntry = callerClass->nsSearchTree[int(type)];
		while ( cEntry )
		{
			auto r = memcmpi ( name, cEntry->fullName, cEntry->fullNameLen );
			if ( !r )
			{
				return cEntry;
			} else if ( r < 0 )
			{
				cEntry = cEntry->leftNS[int(type)];
			} else
			{
				cEntry = cEntry->rightNS[int(type)];
			}
		}
	} else
	{
		auto cEntry = callerClass->searchTree[int(type)];
		while ( cEntry )
		{
			auto r = memcmpi ( name, cEntry->name, cEntry->nameLen );
			if ( !r )
			{
				return cEntry;
			} else if ( r < 0 )
			{
				cEntry = cEntry->left[int(type)];
			} else
			{
				cEntry = cEntry->right[int(type)];
			}
		}
	}
	return 0;
}

extern	void	 op_cbFixup				( class vmInstance *instance, struct bcFuncDef *funcDef, VAR *basePtr );
extern	void	 op_cbFixupNoPromote	( class vmInstance *instance, struct bcFuncDef *funcDef, VAR *basePtr );
extern	bool	 op_compile				( class vmInstance *instance, struct bcFuncDef *funcDef, bool allowSideEffects );
extern	void	 op_cbCall				( class vmInstance *instance, fglOp *ops, struct bcFuncDef *funcDef, VAR *basePtr, uint32_t nParams );

extern	void	 op_arrDeref		( class vmInstance *instance, fglOp *ops );
extern	void	 op_arrDerefRef		( class vmInstance *instance, fglOp *ops );
extern	void	 op_arrStore		( class vmInstance *instance, fglOp *ops );

extern	void	 op_addv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_subv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_mulv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_divv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_modv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_powv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_maxv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_minv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_ltv				( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_lteqv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_gtv				( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_gteqv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_aeqv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_eqv				( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_eqv1			( class vmInstance *instance, fglOp *ops );
extern	void	 op_neqv			( class vmInstance *instance, fglOp *ops, bool safeCall );

extern	void	 op_incv			( class vmInstance *instance, fglOp *ops, VAR *leftOperand, fgxOvOp ovOp );
extern	void	 op_incvPop			( class vmInstance *instance, fglOp *ops, VAR *leftOperand, fgxOvOp ovOp );
extern	void	 op_decv			( class vmInstance *instance, fglOp *ops, VAR *leftOperand, fgxOvOp ovOp );
extern	void	 op_decvPop			( class vmInstance *instance, fglOp *ops, VAR *leftOperand, fgxOvOp ovOp );
extern	void	 op_negv			( class vmInstance *instance, fglOp *ops, VAR *leftOperand, bool safeCall );
extern	void	 op_notv			( class vmInstance *instance, fglOp *ops, VAR *leftOperand, bool safeCall );
extern	void	 op_twocv			( class vmInstance *instance, fglOp *ops, VAR *leftOperand, bool safeCall );

extern	void	 op_orv				( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_andv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_bwOrv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_bwAndv			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_bwXor			( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_bwShLeft		( class vmInstance *instance, fglOp *ops, bool safeCall );
extern	void	 op_bwShRight		( class vmInstance *instance, fglOp *ops, bool safeCall );

extern	void	 op_assign			( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );

extern	void	 op_assignAdd		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignSub		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignMul		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignDiv		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignPow		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignMod		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignBwAnd		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignBwOr		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignBwXor		( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignShLeft	( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );
extern	void	 op_assignShRight	( class vmInstance *instance, fglOp *ops, VAR *dest, VAR *value );

extern	fglOp	*op_call			( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	fglOp	*op_callSafe		( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	fglOp	*op_callPPack		( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	fglOp	*op_callPPackSafe	( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	void	 op_pushFunc		( class vmInstance *instance, fglOp *ops, VAR *basePtr );

extern	void	 op_pushValue		( class vmInstance* instance, fglOp* ops, char const *className, char const *elemName );
extern	void	 op_pushValueRef	( class vmInstance* instance, fglOp* ops, char const *className, char const *elemName );
extern	void	 op_storeValue		( class vmInstance* instance, fglOp* ops, char const* className, char const* elemName );

extern	void	 op_objStore		( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, VAR *basePtr, uint8_t const *dsBase );
extern	void	 op_objStoreInd		( class vmInstance *instance, fglOp *ops, VAR *basePtr );
extern	void	 op_accessObj		( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, VAR *basePtr, uint8_t const *dsBase );
extern	void	 op_accessObjInd	( class vmInstance *instance, fglOp *ops, VAR *basePtr );
extern	void	 op_accessObjIndSafe ( class vmInstance *instance, fglOp *ops, VAR *basePtr );
extern	void	 op_accessObjRef	( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, VAR *basePtr, uint8_t const *dsBase );
extern	void	 op_accessObjRefInd ( class vmInstance *instance, fglOp *ops, VAR *basePtr );
extern	void	 op_objRelease		( class vmInstance *instance, fglOp *ops, VAR *basePtr );
extern	fglOp	*op_objCall			( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	fglOp	*op_objCallPPack	( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	void	 op_objCallFuncOv	( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );

extern	void	 op_objBuild		( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	void	 op_objNew			( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr );
extern	fglOp	*op_objNewV			( class vmInstance *instance, fglOp *ops, bcFuncDef *funcDef, uint32_t nParams, VAR *basePtr, bool safeCall );

extern	void	 op_vArrayNew ( class vmInstance *instance, int64_t num );

extern	vmCodeBlock  *_compileIndexCodeblock ( class vmInstance *instance, char const *expr );

