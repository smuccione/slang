// bcVMBuiltin.h

#pragma once

#include "Utility/settings.h"

#include <stdint.h>
#include "compilerParser/fileParser.h"

enum class builtinFlags {					// must be power of 2... || together
	builtin_RegisterOnly		= 1,
	builtIn_MakeCompiler		= 2,
};

extern	std::tuple<uint8_t const *, size_t> builtinInit	( class vmInstance *instance, builtinFlags flag );

extern	void			 builtinCoroutineInit			( class vmInstance *instance, class opFile *file );
extern	void			 builtinGeneralInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinIOInit					( class vmInstance *instance, class opFile *file );
extern	void			 builtinStringInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinMathInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinDateTimeInit			( class vmInstance *instance, class opFile *file );
extern	void			 builtinPackInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinCompressionInit			( class vmInstance *instance, class opFile *file );
extern	void			 builtinCSV						( class vmInstance *instance, class opFile *file );

extern	void			 builtinSystemBufferInit		( class vmInstance *instance, class opFile *file );
extern	void			 builtinAArrayInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinTargetInit				( class vmInstance *instance, class opFile *file );

extern	void			 builtinMapInit					( class vmInstance *instance, class opFile *file );
extern	void			 builtinMultiMapInit			( class vmInstance *instance, class opFile *file );
extern	void			 builtinUnorderedMapInit		( class vmInstance *instance, class opFile *file );
extern	void			 builtinUnorderedMultiMapInit	( class vmInstance *instance, class opFile *file );

extern	void			 builtinSetInit					( class vmInstance *instance, class opFile *file );
extern	void			 builtinMultiSetInit			( class vmInstance *instance, class opFile *file );
extern	void			 builtinUnorderedSetInit		( class vmInstance *instance, class opFile *file );
extern	void			 builtinUnorderedMultiSetInit	( class vmInstance *instance, class opFile *file );

extern	void			 builtinQueryable				( class vmInstance *instance, class opFile *file );

extern	void			 builtinHTTPClientInit			( class vmInstance *instance, class opFile *file );
extern  void			 builtinTUSClientInit			( class vmInstance *instance, class opFile *file );

extern  void			 builtinUnitTest				( class vmInstance *instance, class opFile *file );

extern	void			 builtinOpenSSLInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinFTPClient				( class vmInstance *instance, class opFile *file );

extern	void			 builtinJSONInit				( class vmInstance *instance, class opFile *file );

extern	void			 builtinLegacyInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinImageInit				( class vmInstance *instance, class opFile *file );

extern	void			 builtinCom						( class vmInstance *instance, class opFile *file );

extern	void			 builtinRegexInit				( class vmInstance *instance, class opFile *file );
extern	void			 builtinRegexOldInit			( class vmInstance *instance, class opFile *file );

// special case... this is called directly from the interpreter and is more than a built in... it
// is designed to create static associative arrays.

extern	void			 aArrayNew						( class vmInstance *instance, int64_t nPairs );

// used by vm as well as directory watcher in the engine
extern VAR webSend ( vmInstance *instance, struct VAR_STR const &method, struct VAR_STR const &domain, struct VAR_STR const &url, int64_t port, struct VAR_STR const &headers, struct VAR_STR const &postData, bool isSSL );

