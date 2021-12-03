/*
bTreeClass

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"
#include "vmAllocator.h"
#include <map>

#define CONTAINER_NAME  multimap
#define IS_ORDERED		true
#define IS_SET			false
#define IS_MULTI		true

#include "bcVM/vmBuiltinContainer.h"

void builtinMultiMapInit ( class vmInstance *instance, opFile *file )
{
	REGISTER ( instance, file );
		CLASS ( MAKE_NAME ( CONTAINER_NAME ) );
			METHOD ( "new", cContainerNew, DEF ( 2, "null" ) );
			METHOD ( "add", cContainerAdd );
			METHOD ( "getEnumerator", cContainerGetEnumerator );
			METHOD ( "has", cContainerIsPresent ) CONST;
			METHOD ( "find", cContainerFind ) CONST;
			METHOD ( "size", cContainerLen ) CONST;
			METHOD ( "delete", cContainerDelete );
			METHOD ( "insert", cContainerInsert );
#if !IS_SET && !IS_MULTI
			ACCESS ( "default", cContainerAccess );
			ASSIGN ( "default", cContainerInsert );
			OP ( "[", cContainerAccess );
			OP ( "[", cContainerInsert );
#endif
			PRIVATE
				IVAR ( "$generation" );
				GCCB ( cContainerGcCb, cContainerCopyCb );
				PACKCB ( cContainerPackCb, cContainerUnPackCB );
				INSPECTORCB ( cContainerInspector );
		END;

		CLASS ( MAKE_NAME ( CONTAINER_NAME ) "Enumerator" );
			INHERIT ( "Queryable" );
			METHOD ( "new", containerEnumeratorNew );
			METHOD ( "getEnumerator", containerEnumeratorGetEnumerator );
			METHOD ( "moveNext", containerEnumeratorMoveNext );
			METHOD ( "reset", containerEnumeratorReset );
			ACCESS ( "current", containerEnumeratorCurrent ) CONST;
#if !IS_SET
			ACCESS ( "index", containerEnumeratorIndex ) CONST;
#endif
			IVAR ( "$container" );
			GCCB ( containerEnumeratorGcCb, containerEnumeratorCopyCb );
		END;
	END;
}
