/*
    SLANG support functions

*/


#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "builtinDatabase/builtinRemoteDatabase.h"
#include "builtinDOM/builtinDom.h"
#include "webServer/vmBuiltinSessionClass.h"

std::tuple<uint8_t const *, size_t> builtinInit ( class vmInstance *instance, builtinFlags flag )
{
	BUFFER		 buff;
	opFile		 file;

	//  initialize all the builin library functions

	// these are mandatory... required for the compiler
	builtinGeneralInit		( instance, &file );		// things like new, release, etc.
	builtinQueryable		( instance, &file );		// aArray inherits from queryable
	builtinAArrayInit		( instance,	&file );		// aArray, needed for { name:value } type initialization
	builtinSystemBufferInit ( instance, &file );		// needed by web-server

	// these are optional... won't effect the compiler
#if 1
	builtinMapInit			( instance, &file );
	builtinMultiMapInit		( instance, &file );
	builtinSetInit			( instance, &file );
	builtinMultiSetInit		( instance, &file );

	builtinUnorderedMapInit ( instance, &file );
	builtinUnorderedMultiMapInit ( instance, &file );
	builtinUnorderedSetInit ( instance, &file );
	builtinUnorderedMultiSetInit ( instance, &file );

	builtinIOInit			( instance, &file );
	builtinStringInit		( instance, &file );
	builtinMathInit			( instance, &file );
	builtinDateTimeInit		( instance, &file );
	builtinPackInit			( instance, &file );
	builtinRemoteDBInit		( instance, &file );
	builtinDOMInit			( instance, &file );

	builtinCoroutineInit	( instance, &file );

	builtinTargetInit		( instance, &file );
	builtinSessionClassInit	( instance, &file );

	builtinHTTPClientInit	( instance, &file );
	builtinTUSClientInit	( instance, &file );

	builtinUnitTest			( instance, &file );

	builtinOpenSSLInit		( instance, &file );
	builtinFTPClient		( instance, &file );

	builtinJSONInit			( instance, &file );
	builtinCompressionInit	( instance, &file );

	builtinCSV				( instance, &file );

	builtinLegacyInit		( instance, &file );
	builtinImageInit		( instance, &file );

	builtinRegexInit		( instance, &file );
	builtinRegexOldInit		( instance, &file );
#endif

	if ( int(flag) & int(builtinFlags::builtIn_MakeCompiler) )
	{
		file.serialize ( &buff, false );
		auto size = bufferSize ( &buff );
		return { (uint8_t const *) bufferDetach ( &buff ), size  };
	}

	return { nullptr, 0 };
}
