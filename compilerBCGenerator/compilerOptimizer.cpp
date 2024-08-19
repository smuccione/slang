
#include "compilerBCGenerator.h"
#include "compilerOptimizer.h"
#include "transform.h"

optimizer::~optimizer ()
{
	delete transform;
}

optimizer::optimizer ( opFile *file, compExecutable *compDef )
{
	this->file = file;
	this->compDef = compDef;
	transform = new opTransform ( file );

	doInline = vmConf.inlineCode;
}

void optimizer::go ( cacheString const &entry )
{
	try
	{
		if ( file->errHandler.isFatal () ) return;

		doTypeInference ( entry, false );

		if ( file->errHandler.isFatal () ) return;

		removeUnusedVariables ();

		if ( this->doInline )
		{
			funcInline ();
		}

		compDef->combineBlocks ();

		if ( file->errHandler.isFatal () ) return;

		if ( entry != "<NO MAIN>" )
		{
			makeKnown ( entry, false );		// need to make known and then type inference before dead-function removal.  That removes any potentially... status's
//			doTypeInference ( entry, false );
			removeUnused ( entry, false );

			// clear the atom table... this contains references to everything we've processed (have and need).   however it's too big as we've very likely
			// pruned things down from the libraries.
			compDef->atom.clear ();
			compDef->clear ();
			// we need to rebuild the class's as they create atom's and since we've likely pruned unused class's we need to redo this so we have the atoms we need
			compDef->makeClass ( false );

			file->buildSymbolSearch(true);
		}

		doTypeInference ( entry, false );

		removeUnusedVariables ();

		if ( file->errHandler.isFatal () ) return;

		// do this again after inlining to get additional gains
		constantFoldGlobals ();

		if ( file->errHandler.isFatal () ) return;

		compDef->fixupGoto ();		// this MUST be done AFTER removal of unused variables, otherwise goto/ret/slide, etc. stack computations will be wrong!

		if ( file->errHandler.isFatal () ) return;

		do
		{
			transformMade = false;
			doTypeInference ( entry, false );
			simpleDCEliminate ();
			constantFold ();
			simpleTransform ();

			if ( file->errHandler.isFatal () ) return;
		} while ( transformMade && !file->errHandler.isFatal () );
		makeKnown ( entry, false );
		compDef->compGenErrorsWarnings ( false );
	} catch ( errorNum err )
	{
		file->errHandler.throwFatalError ( false, err );
		throw;
	}
}

void optimizer::goLS ( cacheString const &entry )
{
	try
	{
		if ( !file->errHandler.isFatal() )
		{
			doTypeInference ( entry, true );
			makeKnown ( entry, true );
		}
		compDef->compGenErrorsWarnings ( true );
	} catch ( errorNum )
	{
	}
}

void optimizer::goLSError ( cacheString const &entry )
{
	try
	{
		if ( !file->errHandler.isFatal () )
		{
			makeKnown ( entry, true );		// parse error.. just make everything known.. don't worry about types as everything is likely fubar anyway
		}
		compDef->compGenErrorsWarnings ( true );
	} catch ( errorNum )
	{
	}
}

