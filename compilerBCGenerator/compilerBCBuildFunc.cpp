// This is the main DLL file.

#include "compilerBCGenerator.h"

void compFunc::serialize ( BUFFER *buff )
{
	fgxFuncEntry	*entry;
	uint32_t		 entryOffset;

	entryOffset = (uint32_t)bufferSize ( buff );
	bufferMakeRoom ( buff, sizeof ( fgxFuncEntry ) );
	bufferAssume ( buff, sizeof ( fgxFuncEntry ) );

	entry = (fgxFuncEntry *)(bufferBuff ( buff ) + entryOffset );
	entry->sourceIndex			= location.sourceIndex;
	entry->conv					= conv;
	entry->atom					= atom;
	entry->nParams				= nParams;
	entry->codeOffset			= codeOffset;
	entry->lastCodeOffset		= lastCodeOffset;
	entry->defaultParamsOffset	= defParamOffset;
	entry->tcOffset				= tcOffset;
	entry->isMethod				= isMethod;
	entry->isExtension			= isExtension;
	entry->isConst				= isConst;

	// debug stuff
	entry->symbolsOffset		= symOffset;
	entry->nSymbols				= nSymbols;

	entry->coRoutineAble		= 1;

	for ( uint32_t loop = isMethod ? 1 : 0; loop < params->size(); loop++ )
	{
		if ( (symbolTypeClass) params->getType ( (*params)[loop]->name, true ) != symbolType::symVariant )
		{
			entry->coRoutineAble = 0;
			break;
		}
	}

	entry = (fgxFuncEntry *)(bufferBuff ( buff ) + entryOffset );
	entry->nextFuncOffset = (uint32_t)bufferSize ( buff );
}

void compExecutable::compileFunction ( opFunction *func )
{
	compFunc									*cFunc = 0;
	uint32_t									 loop;
	uint32_t									 symbolValidStart;
	compFixup fu;

	fixup = &fu;

	cFunc = new compFunc;

	try {
		cFunc->isInterface = func->isInterface;
		
		symbolStack	sym ( this->file, func);

		cFunc->isMethod		= func->classDef && !func->isExtension && !func->isStatic;
		cFunc->isExtension = func->isExtension;
		cFunc->isConst		= func->isConst;
		cFunc->name			= func->name;
		cFunc->nSpace		 = file->sCache.get ( file->srcFiles.getName ( func->location.sourceIndex ) );
		cFunc->conv			= func->conv;
		cFunc->location		= func->location;
		cFunc->atom			= atom.add ( cFunc->name, atomListType::atomFuncref );
		cFunc->symOffset	= (uint32_t)symbols.size();
		cFunc->params		= &func->params;
		cFunc->tcOffset		= (uint32_t)tcList.tcList.size();

		symbolValidStart	= codeSegment.getLen();

		if ( func->codeBlock )
		{
			listing.setLastLine ( func->location );
			listing.emitFuncInfo ( func );
			listing.emitContSource ( func->location );
		}

		// build the default parameter code
		cFunc->defParamOffset = dataSegment.addBuffer ( (uint32_t)func->params.size() * sizeof ( uint32_t ) );
		if (func->params.size ( ))
		{
			symbolStack	paramSym ( this->file, func );
			symbolSpaceNamespace ns ( file, &file->ns, &func->usingList );
			paramSym.push ( &ns );

			for (loop = 0; loop < func->params.size ( ); loop++)
			{
				if ( (func->params)[loop]->initializer->getOp () == astOp::assign )
				{
					if ((func->params)[loop]->name.size ( ))
					{
						listing.annotate ( &paramSym, "default value for %s ( %s )", func->name.c_str ( ), (func->params)[loop]->name.c_str ( ) );
					} else
					{
						listing.annotate ( &paramSym, "default value for %s ( %i )", func->name.c_str ( ), loop + 1 );
					}
					auto dPtr = (uint32_t *) dataSegment.getRawBuff ( );
					dPtr = (uint32_t *) (((char *) dPtr + cFunc->defParamOffset));
					dPtr[loop] = (uint32_t) (codeSegment.size ( ) / sizeof ( fglOp ));						// entry in DS of our code offset

					debug.addEntry ( (func->params)[loop]->initializer->right->location, codeSegment.getNumOps ( ), paramSym.getStackSize ( ) );
					compEmitNode ( func, (func->params)[loop]->initializer->right, &paramSym, (func->params)[loop]->initializer->right->getType ( &paramSym ), symVariantType, false, true );

					paramSym.popStack ( );
					codeSegment.putOp ( fglOpcodes::pRet );
					listing.emitOps ( &paramSym );
				}else
				{
					auto dPtr = (uint32_t *) dataSegment.getRawBuff ( );
					dPtr = (uint32_t *) (((char *) dPtr + cFunc->defParamOffset));
					dPtr[loop] = 0;
				}
			}
		}
		listing.emitOps ( &sym );

		cFunc->codeOffset = (uint32_t)(codeSegment.size() / sizeof ( fglOp ));

		if ( genProfiling && func->name == file->sCache.get ( "main" ) )
		{
			codeSegment.putOp ( fglOpcodes::profReset );
		}
		listing.emitOps ( &sym );

		if ( func->codeBlock )
		{
			if ( genDebug )
			{
				if ( genJustMyCode )
				{
					debug.addEntry ( func->location, codeSegment.getNumOps ( ), sym.getStackSize ( ) );
					codeSegment.putOp ( fglOpcodes::debugFuncStart );		// triggers on a step into a function
				}
			}
			if ( genProfiling ) codeSegment.putOp ( fglOpcodes::profFuncStart );

			if ( func->isVariantParam )
			{
				codeSegment.putOp( fglOpcodes::makeParamPack, (uint32_t)sym.getIndex( func->params.symbols[func->params.symbols.size() - 1].name, true ) );
			}
			codeSegment.emitPush();
			compEmitNode ( func, func->codeBlock, &sym, symVariantType, func->getReturnType(), false, false );
			codeSegment.emitPop();
		}
		listing.emitOps ( &sym );

		if ( func->codeBlock )
		{
			if ( genProfiling ) codeSegment.putOp ( fglOpcodes::profFuncEnd );	// shoud be as close to btExit as possible

			astNode exitNode ( file->sCache, astOp::btExit );
			exitNode.setLocation ( func->location );
			compEmitNode ( func, &exitNode, &sym, symVariantType, func->getReturnType(), false, false );
			listing.emitOps ( &sym );
		}
		// symbols must be generated AFTER any possible code is generated so their valid ranges are correct

		// we need to invert the order of the parameters when emitting so that the order appears correct to the user
		for ( loop = (uint32_t) func->params.size(); loop; loop-- )
		{
			visibleSymbols	fgxSym;

			fgxSym.index		= sym.getIndex ( func->params[loop - 1]->name, true );
			fgxSym.validStart	= symbolValidStart;
			fgxSym.validEnd		= codeSegment.getLen() - 1;
			fgxSym.isStackBased	= true;
			fgxSym.noRef		= false;
			fgxSym.name			= func->params[loop - 1]->name;
			fgxSym.isParameter	= true;
			if ( fgxSym.name == file->selfValue || fgxSym.name == file->_selfValue )
			{
				fgxSym.noRef = true;
			}
			symbols.push_back ( fgxSym );
		}

		cFunc->nSymbols = (uint32_t)(symbols.size() - cFunc->symOffset);

		fixup->applyFixups ( codeSegment );
		fixup = 0;

		cFunc->nParams		= (uint32_t) func->params.size();
		funcDef.push_back ( cFunc );

		cFunc->lastCodeOffset = (uint32_t) (codeSegment.size () / sizeof ( fglOp ));

		tcList.addEndOfFunction ();
		listing.emitOps ( &sym );
		listing.emitContSource ( func->location );
	} catch ( ... )
	{
		throw;
	}
}
