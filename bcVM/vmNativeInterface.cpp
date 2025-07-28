/*
	instance structure for vm execution

*/

#include "bcVM.h"
#include "bcVMBuiltin.h"
#include "vmNativeInterface.h"

#include <source_location>

/*
*	comments of the form:
*
*	@func <functionName> description
*	@param paramName(displayName) description
*	...
*   @return description
*
*
*   will be processed and the appropriate documentation members set.
*
*	@class <className> description				// and sets class for @assign, @access, @ivar, @static, @const until the next @class occurs
*	@assign <assignName> description
*	@access <accessName> description
*   @op <operator> description
*	@ivar <varName>	description
*	@static <staticName> description
*	@const <cosntName>	description
*
*	@global <globalName> description
*/

#undef DEF

#define docElementDef \
DEF ("@param", Param ) \
DEF ("@func", Func ) \
DEF ("@class", Class) \
DEF ("@assign", Assign ) \
DEF ("@access", Access ) \
DEF ("@op", Op ) \
DEF ("@ivar", IVar ) \
DEF ("@static", Static ) \
DEF ("@const", Const ) \
DEF ("@global", Global ) \

enum class docElemType
{
#define DEF(a,b) b,
	docElementDef
#undef DEF
};

struct docList
{
	char const *name;
	int			 len;
	docElemType	 type;
};

const std::map<statementString, docList > docMap =
{
#define DEF(a,b) { statementString ( a, sizeof ( a ) - 1 ),  docList { a, sizeof ( a ) - 1, docElemType::b } },
	docElementDef
#undef DEF
};

static stringi getDocumentation ( source &src )
{
	stringi result;

	while ( *src )
	{
		auto it = docMap.find ( statementString ( src ) );

		if ( (it != docMap.end ()) && cmpTerminated ( src, it->second.name, it->second.len ) )
		{
			return result;
		}
		result += *src;
		src++;
	}

	return result;
}

static void commentParser ( opFile &file, char const *comment )
{
	sourceFile fileMap;
	source src ( &fileMap, file.sCache, "internal", comment, sourceFile::sourceFileType::none, 1 );
	stringi	currentClass;
	opFunction *currentFunc = nullptr;

	while ( *src )
	{
		src.bsAdvanceEol ( true );

		auto it = docMap.find ( statementString ( src ) );

		if ( (it != docMap.end ()) && cmpTerminated ( src, it->second.name, it->second.len ) )
		{
			src += it->second.len;
			src.bsAdvanceEol ( true );
			switch ( it->second.type )
			{
				case docElemType::Func:
					{
						stringi funcName = getSymbol ( src );
						src.bsAdvanceEol ( true );
						auto documentation = getDocumentation ( src );
						auto cFuncName = file.sCache.get ( funcName );
						auto f = file.functionList.find ( cFuncName );
						if ( f != file.functionList.end () )
						{
							currentFunc = f->second;
							currentFunc->documentation = documentation;
						} else
						{
							throw errorNum::scINTERNAL;
							currentFunc = nullptr;
						}
					}
					break;
				case docElemType::Param:
					if ( currentFunc )
					{
						auto paramName = file.sCache.get ( getSymbol ( src ) );
						stringi newParamName;
						src.bsAdvance ();
						if ( *src == '(' )
						{
							src++;
							newParamName = getSymbol ( src );
							src.bsAdvance ();
							if ( *src != ')' )
							{
								throw errorNum::scINTERNAL;
							}
							src++;
							src.bsAdvanceEol ( false );
						}
						auto documentation = getDocumentation ( src );
						for ( uint32_t loop = 0; loop < currentFunc->params.size (); loop++ )
						{
							auto def = currentFunc->params[loop];
							if ( def->getName () == paramName )
							{
								if ( newParamName.size () )
								{
									def->name = file.sCache.get ( newParamName );
								}
								def->documentation = documentation;
							}
						}
					} else
					{
						throw errorNum::scINTERNAL;
					}
					break;
				default:
					throw errorNum::scINTERNAL;	// TODO add missing features
			}
		}
	}
}

static uint32_t makeTypes ( bcFuncDef *bcFunc, vmDispatcher *func )
{
	uint32_t nParams;

	func->getPTypes ( bcFunc->pTypes );

	bcFunc->retType = bcFunc->pTypes[0];

	nParams = 0;
	for ( uint32_t loop = 1; loop <= bcFunc->nCParams; loop++ )
	{
		switch ( bcFunc->pTypes[loop] )
		{
			case bcFuncPType::bcFuncPType_Short_Int:
			case bcFuncPType::bcFuncPType_Long_Int:
			case bcFuncPType::bcFuncPType_Double:
			case bcFuncPType::bcFuncPType_Variant:
			case bcFuncPType::bcFuncPType_Object:
			case bcFuncPType::bcFuncPType_Array:
			case bcFuncPType::bcFuncPType_String:
			case bcFuncPType::bcFuncPType_Var_String:
				nParams++;
				break;
			default:
				break;
		}
	}

	return nParams;
}

static uint32_t getParamCount ( vmDispatcher *func )
{
	bcFuncPType		pTypes[32];

	func->getPTypes ( pTypes );

	auto nCParams = func->getNParams ();

	uint32_t nParams = 0;
	for ( uint32_t loop = 1; loop <= nCParams; loop++ )
	{
		switch ( pTypes[loop] )
		{
			case bcFuncPType::bcFuncPType_Short_Int:
			case bcFuncPType::bcFuncPType_Long_Int:
			case bcFuncPType::bcFuncPType_Double:
			case bcFuncPType::bcFuncPType_Variant:
			case bcFuncPType::bcFuncPType_Object:
			case bcFuncPType::bcFuncPType_Array:
			case bcFuncPType::bcFuncPType_String:
			case bcFuncPType::bcFuncPType_Var_String:
				nParams++;
				break;
			default:
				break;
		}
	}

	return nParams;
}

static void makeTypes ( opFile *file, opFunction *funcDef, vmDispatcher *func )
{
	bcFuncPType		pTypes[32];
	char			paramName[16];

	func->getPTypes ( pTypes );

	// set the return type
	switch ( pTypes[0] )
	{
		case bcFuncPType::bcFuncPType_Short_Int:
		case bcFuncPType::bcFuncPType_Long_Int:
			funcDef->retType = symIntType;
			break;
		case bcFuncPType::bcFuncPType_Double:
			funcDef->retType = symDoubleType;
			break;
		case bcFuncPType::bcFuncPType_String:
		case bcFuncPType::bcFuncPType_Var_String:
			funcDef->retType = symStringType;
			break;
		case bcFuncPType::bcFuncPType_Variant:
			funcDef->retType = symVariantType;
			break;
		case bcFuncPType::bcFuncPType_Object:
			funcDef->retType = symObjectType;
			break;
		case bcFuncPType::bcFuncPType_Array:
			funcDef->retType = symArrayType;
			break;
		default:
			throw errorNum::scINTERNAL;
	}

	auto nCParams = func->getNParams ();

	uint32_t ctr = 1;
	for ( uint32_t loop = 1; loop <= nCParams; loop++ )
	{
		sprintf_s ( paramName, sizeof ( paramName ), "param%u", ctr++ );
		switch ( pTypes[loop] )
		{
			case bcFuncPType::bcFuncPType_Short_Int:
			case bcFuncPType::bcFuncPType_Long_Int:
				funcDef->params.add ( file->sCache.get ( paramName ), symIntType, funcDef->location, (new astNode ( file->sCache, astOp::symbolValue, paramName ))->setLocation ( funcDef->location ), false, stringi()				);
				break;
			case bcFuncPType::bcFuncPType_Double:
				funcDef->params.add ( file->sCache.get ( paramName ), symDoubleType, funcDef->location, (new astNode ( file->sCache, astOp::symbolValue, paramName ))->setLocation ( funcDef->location ), false, stringi () );
				break;
			case bcFuncPType::bcFuncPType_Variant:
				funcDef->params.add ( file->sCache.get ( paramName ), symVariantType, funcDef->location, (new astNode ( file->sCache, astOp::symbolValue, paramName ))->setLocation ( funcDef->location ), false, stringi () );
				break;
			case bcFuncPType::bcFuncPType_String:
			case bcFuncPType::bcFuncPType_Var_String:
				funcDef->params.add ( file->sCache.get ( paramName ), symStringType, funcDef->location, (new astNode ( file->sCache, astOp::symbolValue, paramName ))->setLocation ( funcDef->location ), false, stringi () );
				break;
			case bcFuncPType::bcFuncPType_Instance:
			case bcFuncPType::bcFuncPType_Workarea:
				break;
			case bcFuncPType::bcFuncPType_NParams:
				funcDef->isVariantParam = true;
				break;
			default:
				throw errorNum::scINTERNAL;
		}
	}
}

vmNativeInterface::nativeClass::nativeClass ( vmNativeInterface *native, char const *className )
{
	this->native = native;
	nativeClassName = className;
	nativeStatic = false;
	nativeVirtual = false;
	nativeScope = fgxClassElementScope::fgxClassScope_public;

	if ( native->file )
	{
		auto oClassDef = new opClass ();

		oClassDef->isInterface = true;
		oClassDef->name = native->file->sCache.get ( className );
		oClassDef->location.sourceIndex = native->file->srcFiles.getIndex ( "(INTERNAL)", sourceFile::sourceFileType::none );
		oClassDef->usingList = native->file->ns.getUseList ();

		native->file->addElem ( oClassDef, false );
	}

	bcClass *classDef;

	classDef = new bcClass ();
	memset ( classDef, 0, sizeof ( *classDef ) );

	classDef->name = className;
	classDef->nameLen = (uint32_t) strlen ( classDef->name ) + 1;
	try
	{
		classDef->atom = native->instance->atomTable->atomPut ( className, classDef, true, true );
	} catch ( ... )
	{
		delete classDef;
		throw;
	}
}

void vmNativeInterface::nativeClass::property ( char const *propName, fgxClassElementType type, char const *initializer )
{
	opClass *cls;

	if ( (ptrdiff_t) this )
	{
		cls = native->file->findClass ( nativeClassName );

		if ( nativeStatic )
		{
			if ( type != fgxClassElementType::fgxClassType_iVar ) throw errorNum::scINTERNAL;
			type = fgxClassElementType::fgxClassType_static;
		}

		for ( auto it = cls->elems.begin (); it != cls->elems.end (); it++ )
		{
			if ( !strccmp ( (*it)->name.c_str (), propName ) )
			{
				if ( (*it)->type == type )
				{
					throw errorNum::scINTERNAL;
				}
			}
		}

		astNode *init = nullptr;

		if ( initializer )
		{
			char const *expr;
			expr = initializer;
			source	src ( &native->file->srcFiles, native->file->sCache, "(INTERNAL)", expr, sourceFile::sourceFileType::none );

			auto name = new astNode ( astOp::symbolValue, native->file->sCache.get ( propName ) );

			init = new astNode ( native->file->sCache, astOp::assign, name, native->file->parseExpr ( src, false, false, 0, true, false, false ) );
		} else
		{
			init = new astNode ( astOp::symbolValue, native->file->sCache.get ( propName ) );
		}

		// add the new type

		switch ( type )
		{
			case fgxClassElementType::fgxClassType_iVar:
				cls->addIVar ( init, type, nativeScope, stringi () );
				break;
			case fgxClassElementType::fgxClassType_inherit:
				delete init;
				cls->add ( native->file->sCache.get ( propName ), fgxClassElementType::fgxClassType_inherit, nativeScope, srcLocation (), nativeVirtual, stringi () );
				break;
			case fgxClassElementType::fgxClassType_const:
				cls->addConst ( init, type, nativeScope, stringi () );
				break;
			default:
				assert ( 0 );
				break;
		}
	}

	nativeVirtual = false;
	nativeStatic = false;
}

void vmNativeInterface::nativeClass::property ( char const *propName, fgxClassElementType type, int64_t initValue )
{
	char tmpBuff[65];
	sprintf_s ( tmpBuff, sizeof ( tmpBuff ), "\"%I64i\"", initValue );
	return property ( propName, type, tmpBuff );
}

void vmNativeInterface::nativeClass::require ( char const *name, ... )
{
	va_list		nameList;
	va_start ( nameList, name );

	auto cls = native->file->findClass ( nativeClassName );

	cls->requiredClasses.push_back ( native->file->sCache.get ( name ) );

	char const *nextName;
	do
	{
		nextName = va_arg ( nameList, char const * ); // NOLINT (clang-analyzer-valist.Uninitialized)
		if ( nextName )
		{
			cls->requiredClasses.push_back ( native->file->sCache.get ( nextName ) );
		}
	} while ( nextName );

	va_end ( nameList );
}

opFunction *vmNativeInterface::nativeClass::method ( char const *methodName, vmDispatcher *disp, ... )
{
	opFunction *funcDef = nullptr;
	opFunction *newFunc = nullptr;;
	opFunction *releaseFunc = nullptr;;
	opClassElement *elem = nullptr;;

	if ( native->file )
	{
		auto cls = native->file->findClass ( nativeClassName );

		// there must always be a $new method when we have a new ourselves or inherit from other class's.   the $new is the actual function first called
		// during object creation... it will call "new" if needed, but this is via compiler generated code.
		if ( !strccmp ( methodName, "new" ) )
		{
			char const *newMeth;

			methodName = "$new";

			newMeth = "(){}";
			source src ( &native->file->srcFiles, native->file->sCache, "(INTERNAL)", newMeth, sourceFile::sourceFileType::none );
			newFunc = native->file->parseMethod ( src, cls, buildString ( nativeClassName, "new", "method" ).c_str (), true, false, false, srcLocation () );
			newFunc->isInterface = true;
			cls->addMethod ( native->file->newValue, fgxClassElementType::fgxClassType_method, fgxClassElementScope::fgxClassScope_private, false, false, false, newFunc, native->file, symVariantType, true, stringi () );
		}

		if ( !strccmp ( methodName, "release" ) )
		{
			char const *newMeth;

			methodName = "$release";
			newMeth = "(){}";
			source src ( &native->file->srcFiles, native->file->sCache, "(INTERNAL)", newMeth, sourceFile::sourceFileType::none );
			releaseFunc = native->file->parseMethod ( src, cls, buildString ( nativeClassName, "release", "method" ).c_str (), true, false, false, srcLocation () );
			releaseFunc->isInterface = true;
			cls->addMethod ( native->file->sCache.get ( "release" ), fgxClassElementType::fgxClassType_method, fgxClassElementScope::fgxClassScope_public, false, false, false, releaseFunc, native->file, symVariantType, true, stringi () );
		}

		// check for duplicates
		for ( auto it2 = cls->elems.begin (); it2 != cls->elems.end (); it2++ )
		{
			if ( !strccmp ( (*it2)->name.c_str (), methodName ) )
			{
				throw errorNum::scINTERNAL;
			}
		}

		funcDef = new opFunction ( native->file );
		funcDef->isInterface = true;
		funcDef->conv = fgxFuncCallConvention::opFuncType_cDecl;
		funcDef->location = cls->location;
		funcDef->name = native->file->sCache.get ( buildString ( nativeClassName, methodName, "method" ) );
		funcDef->isProcedure = false;

		auto bcFunc = native->functionBuild ( funcDef->name.c_str (), disp, native->instance ? true : false );

		makeTypes ( native->file, funcDef, disp );
		if ( funcDef->retType == symObjectType )
		{
			auto tmp = disp->getClassName ();
			funcDef->retType = symbolTypeClass ( symObjectType, native->file->sCache.get ( tmp ) );
		}

		// our byte-code wrapper must have the same parameters and types as our c function.
		bool selfProcessed = false;
		uint32_t sParam = 0;
		for ( uint32_t loop = 1; loop <= bcFunc->nCParams; loop++ )
		{
			switch ( bcFunc->pTypes[loop] )
			{
				case bcFuncPType::bcFuncPType_Short_Int:
				case bcFuncPType::bcFuncPType_Long_Int:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symIntType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					sParam++;
					break;
				case bcFuncPType::bcFuncPType_Double:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symDoubleType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					sParam++;
					break;
				case bcFuncPType::bcFuncPType_Variant:
					if ( selfProcessed )
					{
						if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symVariantType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					} else
					{
						selfProcessed = true;
						funcDef->params[sParam]->name = native->file->selfValue;
						funcDef->params[sParam]->type = symbolTypeClass ( symObjectType, native->file->sCache.get ( nativeClassName ) );
						if ( newFunc && !newFunc->params.size () ) newFunc->params.add ( native->file->selfValue, symbolTypeClass ( symObjectType, native->file->sCache.get ( nativeClassName ) ), funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
						if ( releaseFunc && !releaseFunc->params.size () ) releaseFunc->params.add ( native->file->selfValue, symObjectType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					}
					sParam++;
					break;
				case bcFuncPType::bcFuncPType_String:
				case bcFuncPType::bcFuncPType_Var_String:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symStringType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					sParam++;
					break;
				default:
					break;
			}
		}

		va_list		defParams;
		va_start ( defParams, disp );

		for ( ;; )
		{
			size_t	nParam;
			char const *param;

			nParam = va_arg ( defParams, int32_t ); // NOLINT (clang-analyzer-valist.Uninitialized)
			if ( nParam == -1 ) break;

			param = va_arg ( defParams, char const * );

			if ( nParam <= (int) funcDef->params.size () )
			{
				source src ( &native->file->srcFiles, native->file->sCache, "(INTERNAL)", param, sourceFile::sourceFileType::none );
				astNode *ini = native->file->parseExpr ( src, false, true, 0, true, false, false );

				if ( newFunc )
				{
					// this is the one acutually called by the vm.  This must have the defaults.  There's no need for the others since all parameters will always be forwarded by the code generator
					newFunc->params[nParam - 1]->initializer = new astNode ( native->file->sCache, astOp::assign, newFunc->params[nParam - 1]->initializer, ini );
				} else
				{
					funcDef->params[nParam - 1]->initializer = new astNode ( native->file->sCache, astOp::assign, funcDef->params[nParam - 1]->initializer, ini );
				}
			}
		}

		va_end ( defParams );

		// attach the class def to the function
		funcDef->classDef = cls;
		funcDef->isExtension = nativeExtension;

		// create the new class element
		elem = new opClassElement;
		elem->type = fgxClassElementType::fgxClassType_method;
		elem->scope = nativeScope;
		elem->name = native->file->sCache.get ( methodName );
		elem->location = funcDef->location;
		elem->isVirtual = nativeVirtual;
		elem->isStatic = nativeStatic;
		elem->data.method.func = funcDef->name;

		// add the new method element to the class info
		cls->elems.push_back ( elem );

		// add it to the vmInstance function listing
		native->file->addElem ( funcDef, funcDef->retType, false );
		nativeVirtual = false;
		nativeStatic = false;

		return funcDef;
	} else
	{
		stringi	newMeth;
		if ( !strccmp ( methodName, "new" ) )
		{
			newMeth = "$new";
		}
		if ( !strccmp ( methodName, "release" ) )
		{
			newMeth = "$release";
		}
		newMeth = buildString ( nativeClassName, methodName, "method" );
		// register it with the runtime atom system;

		native->function ( newMeth.c_str (), false, disp );
		nativeVirtual = false;
		nativeStatic = false;
		return nullptr;
	}
}

opFunction *vmNativeInterface::nativeClass::methodProp ( char const *methodName, bool isAccess, vmDispatcher *disp, ... )
{
	opFunction *funcDef = nullptr;
	opFunction *newFunc = nullptr;;
	opFunction *releaseFunc = nullptr;;
	opClassElement *elem = nullptr;;

	if ( native->file )
	{
		auto cls = native->file->findClass ( nativeClassName );

		// check for duplicates
		for ( auto &it2 : cls->elems )
		{
			if ( !strccmp ( it2->name.c_str (), methodName ) )
			{
				if ( isAccess )
				{
					if ( it2->data.prop.access.size () )
					{
						throw errorNum::scINTERNAL;
					}
				} else
				{
					if ( it2->data.prop.assign.size () )
					{
						throw errorNum::scINTERNAL;
					}
				}
				elem = it2;
			}
		}

		funcDef = new opFunction ( native->file );

		funcDef->isInterface = true;
		funcDef->conv = fgxFuncCallConvention::opFuncType_cDecl;
		funcDef->location = cls->location;
		funcDef->isProcedure = false;

		if ( isAccess )
		{
			funcDef->name = native->file->sCache.get ( buildString ( nativeClassName, methodName, "access" ) );
		} else
		{
			funcDef->name = native->file->sCache.get ( buildString ( nativeClassName, methodName, "assign" ) );
		}
		auto bcFunc = native->functionBuild ( funcDef->name.c_str (), disp, native->instance ? true : false );

		makeTypes ( native->file, funcDef, disp );
		if ( funcDef->retType == symObjectType )
		{
			auto tmp = disp->getClassName ();
			funcDef->retType = symbolTypeClass ( symObjectType, native->file->sCache.get ( tmp ) );
		}

		// our byte-code wrapper must have the same parameters and types as our c function.
		bool selfProcessed = false;
		uint32_t sParam = 0;
		for ( uint32_t loop = 1; loop <= bcFunc->nCParams; loop++ )
		{
			switch ( bcFunc->pTypes[loop] )
			{
				case bcFuncPType::bcFuncPType_Short_Int:
				case bcFuncPType::bcFuncPType_Long_Int:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symIntType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					sParam++;
					break;
				case bcFuncPType::bcFuncPType_Double:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symDoubleType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					sParam++;
					break;
				case bcFuncPType::bcFuncPType_Variant:
					if ( selfProcessed )
					{
						if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symVariantType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					} else
					{
						selfProcessed = true;
						funcDef->params[sParam]->name = native->file->selfValue;
						funcDef->params[sParam]->type = symbolTypeClass ( symObjectType, native->file->sCache.get ( nativeClassName ) );
						if ( newFunc && !newFunc->params.size () ) newFunc->params.add ( native->file->selfValue, symObjectType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
						if ( releaseFunc && !releaseFunc->params.size () ) releaseFunc->params.add ( native->file->selfValue, symbolTypeClass ( symObjectType, native->file->sCache.get ( nativeClassName ) ), funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					}
					sParam++;
					break;
				case bcFuncPType::bcFuncPType_String:
				case bcFuncPType::bcFuncPType_Var_String:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					if ( newFunc ) newFunc->params.add ( funcDef->params[sParam]->getName (), symStringType, funcDef->params[sParam]->location, (new astNode ( native->file->sCache, astOp::symbolValue, funcDef->params[sParam]->getName () ))->setLocation ( funcDef->location ), false, stringi () );
					sParam++;
					break;
				default:
					break;
			}
		}

		va_list	  defParams;
		va_start ( defParams, disp );

		for ( ;; )
		{
			size_t	nParam;
			char const *param;

			nParam = va_arg ( defParams, int32_t ); // NOLINT (clang-analyzer-valist.Uninitialized)
			if ( nParam == -1 ) break;

			param = va_arg ( defParams, char const * );

			if ( nParam <= (int) funcDef->params.size () )
			{
				source src ( &native->file->srcFiles, native->file->sCache, "(INTERNAL)", param, sourceFile::sourceFileType::none );
				astNode *ini = native->file->parseExpr ( src, false, true, 0, true, false, false );

				if ( newFunc )
				{
					// this is the one acutually called by the vm.  This must have the defaults.  There's no need for the others since all parameters will always be forwarded by the code generator
					newFunc->params[nParam - 1]->initializer = new astNode ( native->file->sCache, astOp::assign, newFunc->params[nParam - 1]->initializer, ini );
				} else
				{
					funcDef->params[nParam - 1]->initializer = new astNode ( native->file->sCache, astOp::assign, funcDef->params[nParam - 1]->initializer, ini );
				}
			}
		}

		va_end ( defParams );

		// attach the class def to the function
		funcDef->classDef = cls;
		funcDef->isExtension = nativeExtension;

		if ( !elem )
		{
			// create the new class element
			elem = new opClassElement;
			elem->type = fgxClassElementType::fgxClassType_prop;
			elem->scope = nativeScope;
			elem->name = native->file->sCache.get ( methodName );
			elem->location = funcDef->location;
			elem->isVirtual = nativeVirtual;
			elem->isStatic = nativeStatic;

			// add the new method element to the class info
			cls->elems.push_back ( elem );
		}
		if ( isAccess )
		{
			elem->data.prop.access = funcDef->name;
		} else
		{
			elem->data.prop.assign = funcDef->name;
		}

		// add it to the vmInstance function listing
		native->file->addElem ( funcDef, funcDef->retType, false );
		nativeVirtual = false;
		nativeStatic = false;
		return funcDef;
	} else
	{
		stringi	newMeth;
		if ( !strccmp ( methodName, "new" ) )
		{
			newMeth = "$new";
		}
		if ( !strccmp ( methodName, "release" ) )
		{
			newMeth = "$release";
		}

		if ( isAccess )
		{
			newMeth = buildString ( nativeClassName, methodName, "access" );
		} else
		{
			newMeth = buildString ( nativeClassName, methodName, "assign" );
		}
		// register it with the runtime atom system;

		native->function ( newMeth.c_str (), false, disp );
		nativeVirtual = false;
		nativeStatic = false;
		return nullptr;
	}
}

opFunction *vmNativeInterface::nativeClass::op ( char const *op, vmDispatcher *disp )
{
	opFunction *funcDef = nullptr;
	opClassElement *elem = nullptr;;
	opListDef *ol = nullptr;;

	if ( nativeStatic ) throw errorNum::scDEFINITION;

	if ( native->file )
	{
		auto cls = native->file->findClass ( nativeClassName );

		funcDef = new opFunction ( native->file );

		funcDef->isInterface = true;
		funcDef->conv = fgxFuncCallConvention::opFuncType_cDecl;
		funcDef->location = cls->location;
		funcDef->isProcedure = false;

		// temporary bcFunc to allow us to decode the dispatcher parameter count
		if ( !(ol = native->file->getOperator ( op, getParamCount ( disp ) - 1 )) )
		{
			delete funcDef;
			throw errorNum::scNO_SUCH_OPERATOR_SIGNATURE;
		}
		if ( ol->overloadOp == fgxOvOp::ovNone )
		{
			delete funcDef;
			throw errorNum::scNO_SUCH_OPERATOR_SIGNATURE;
		}

		if ( cls->opOverload[int ( ol->overloadOp )] )
		{
			delete funcDef;
			throw errorNum::scDUPLICATE_METHOD;
		}

		funcDef->name = native->file->sCache.get ( buildString ( nativeClassName, ol->c, ol->opCat == astOpCat::opBinary ? "binary" : "unary" ) );

		auto bcFunc = native->functionBuild ( funcDef->name.c_str (), disp, native->instance ? true : false );

		makeTypes ( native->file, funcDef, disp );
		if ( funcDef->retType == symObjectType )
		{
			auto tmp = disp->getClassName ();
			funcDef->retType = symbolTypeClass ( symObjectType, native->file->sCache.get ( tmp ) );
		}

		// our byte-code wrapper must have the same parameters and types as our c function.
		bool selfProcessed = false;
		uint32_t sParam = 0;
		for ( uint32_t loop = 1; loop <= bcFunc->nCParams; loop++ )
		{
			switch ( bcFunc->pTypes[loop] )
			{
				case bcFuncPType::bcFuncPType_Short_Int:
				case bcFuncPType::bcFuncPType_Long_Int:
				case bcFuncPType::bcFuncPType_Double:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					break;
				case bcFuncPType::bcFuncPType_Variant:
					if ( !selfProcessed )
					{
						selfProcessed = true;
						funcDef->params[sParam]->name = native->file->selfValue;
						funcDef->params[sParam++]->type = symbolTypeClass ( symObjectType, native->file->sCache.get ( nativeClassName ) );
					}
					break;
				case bcFuncPType::bcFuncPType_String:
				case bcFuncPType::bcFuncPType_Var_String:
					if ( !selfProcessed ) throw errorNum::scTYPE_CONVERSION;
					break;
				default:
					break;
			}
		}

		// attach the class def to the function
		funcDef->classDef = cls;
		funcDef->isExtension = nativeExtension;

		// create the new class element
		elem = new opClassElement;
		elem->type = fgxClassElementType::fgxClassType_method;
		elem->scope = nativeScope;
		elem->name = funcDef->name;
		elem->isVirtual = nativeVirtual;
		elem->isStatic = false;
		elem->location = funcDef->location;
		elem->data.method.func = funcDef->name;

		cls->opOverload[int ( ol->overloadOp )] = elem;

		// add it to the vmInstance function listing
		native->file->addElem ( funcDef, funcDef->retType, false );
		nativeVirtual = false;
		nativeStatic = false;

		return funcDef;
	} else
	{
		// temporary bcFunc to allow us to decode the dispatcher parameter count
		bcFuncDef bcFuncTmp;
		bcFuncTmp.nCParams = disp->getNParams ();
		bcFuncTmp.nParams = makeTypes ( &bcFuncTmp, disp );

		if ( !(ol = opFile ().getOperator ( op, bcFuncTmp.nParams - 1 )) )
		{
			throw errorNum::scNO_SUCH_OPERATOR_SIGNATURE;
		}
		if ( ol->overloadOp == fgxOvOp::ovNone )
		{
			throw errorNum::scNO_SUCH_OPERATOR_SIGNATURE;
		}

		stringi newOp = buildString ( nativeClassName, ol->c, ol->opCat == astOpCat::opBinary ? "binary" : "unary" );

		native->functionBuild ( newOp.c_str (), disp, native->instance ? true : false );
		nativeVirtual = false;
		nativeStatic = false;
		return nullptr;
	}
}

void vmNativeInterface::nativeClass::registerGcCb ( classGarbageCollectionCB gcCb, classCopyCB copyCB )
{
	auto cls = native->file->findClass ( nativeClassName );
	auto classDef = native->instance->atomTable->atomGetClass ( native->instance->atomTable->atomFind ( cls->name.c_str () ) );
	classDef->cGarbageCollectionCB = gcCb;
	classDef->cCopyCB = copyCB;
}
void vmNativeInterface::nativeClass::registerPackCb ( void(*cPackCB) (class vmInstance *instance, struct VAR *var, struct BUFFER *buff, void *param, void(*pack)(struct VAR *val, void *param)), void(*cUnPackCB) (class vmInstance *instance, struct VAR *obj, unsigned char **buff, uint64_t *len, void *param, struct VAR *(*unPack)(unsigned char **buff, uint64_t *len, void *param)) )
{
	auto cls = native->file->findClass ( nativeClassName );
	auto classDef = native->instance->atomTable->atomGetClass ( native->instance->atomTable->atomFind ( cls->name.c_str () ) );
	classDef->cPackCB = cPackCB;
	classDef->cUnPackCB = cUnPackCB;
}
void vmNativeInterface::nativeClass::registerInspectorCb ( class vmInspectList *(*cInspectorCB) (class vmInstance *instance, struct bcFuncDef *funcDef, struct VAR *var, uint64_t start, uint64_t end) )
{
	auto cls = native->file->findClass ( nativeClassName );
	auto classDef = native->instance->atomTable->atomGetClass ( native->instance->atomTable->atomFind ( cls->name.c_str () ) );
	classDef->cInspectorCB = cInspectorCB;
}

void vmNativeInterface::document ( char const *documentation )
{
	commentParser ( *file, documentation );
}

opFunction *vmNativeInterface::function ( char const *funcName, bool makeFunc, vmDispatcher *disp, ... )
{
	auto bcFunc = functionBuild ( funcName, disp, instance ? true : false );

	if ( makeFunc && file )
	{
		opFunction *funcDef;

		// create entry for the compiler/linker/loader 
		funcDef = new opFunction ( file );

		try
		{
			funcDef->isInterface = true;
			funcDef->conv = fgxFuncCallConvention::opFuncType_cDecl;
			funcDef->name = file->sCache.get ( bcFunc->name );
			funcDef->location = srcLocation ( file->srcFiles.getIndex ( "(INTERNAL)", sourceFile::sourceFileType::none ), 0, 0, 0 );
			funcDef->isProcedure = false;

			makeTypes ( file, funcDef, disp );

			va_list	  defParams;
			va_start ( defParams, disp );

			for ( ;; )
			{
				size_t	nParam;
				char const *param;
				nParam = va_arg ( defParams, int32_t ); // NOLINT (clang-analyzer-valist.Uninitialized)
				if ( nParam == -1 ) break;
				param = va_arg ( defParams, char const * );

				if ( nParam <= (int32_t) funcDef->params.size () )
				{
					source src ( &file->srcFiles, file->sCache, "(INTERNAL)", param, sourceFile::sourceFileType::none );
					auto ini = file->parseExpr ( src, false, true, 0, true, false, false );
					funcDef->params[nParam - 1]->initializer = new astNode ( file->sCache, astOp::assign, funcDef->params[nParam - 1]->initializer, ini );
				}
			}
			va_end ( defParams );

			file->addElem ( funcDef, funcDef->retType, false );

			return funcDef;
		} catch ( ... )
		{
			delete funcDef;
			throw;
		}
	}
	return 0;
}

bcFuncDef *vmNativeInterface::functionBuild ( char const *funcName, vmDispatcher *func, bool makeFunc )
{
	if ( !makeFunc ) return nullptr;

	bcFuncDef *bcFunc;

	auto atom = instance->atomTable->atomFind ( funcName );
	if ( atom )
	{
		bcFunc = instance->atomTable->atomGetFunc ( atom );
	} else
	{
		bcFunc = new bcFuncDef ();
		if ( !bcFunc ) throw errorNum::scMEMORY;

		bcFunc->atom = instance->atomTable->atomPut ( funcName, bcFunc, true, true );
		bcFunc->name = instance->atomTable->atomGetName ( bcFunc->atom );
	}

	bcFunc->dispatcher.reset ( func );
	bcFunc->nCParams = func->getNParams ();
	bcFunc->nParams = makeTypes ( bcFunc, func );
	bcFunc->conv = fgxFuncCallConvention::opFuncType_cDecl;
	return bcFunc;
}

vmNativeInterface::nativeNamespace::nativeNamespace ( vmNativeInterface *native, std::string const &ns ) : native ( native )
{
	native->ns.push_back ( ns );
}

vmNativeInterface::nativeNamespace::~nativeNamespace ()
{
	native->ns.pop_back ();
}

void vmNativeInterface::compile ( int cls, char const *code, std::source_location loc )
{
	if ( file )
	{
		source src ( &file->srcFiles, file->sCache, loc.file_name (), code, sourceFile::sourceFileType::internal, (uint32_t) loc.line () );
		file->srcFiles.getIndex ( loc.file_name (), sourceFile::sourceFileType::internal );
		file->parseFile ( src, true, false, false );
	}
}

void vmNativeInterface::compile ( vmNativeInterface::nativeClass &cls, char const *code, std::source_location loc )
{
	if ( file )
	{
		auto c = file->findClass ( cls.nativeClassName );
		source src ( &file->srcFiles, file->sCache, loc.file_name (), code, sourceFile::sourceFileType::internal, (uint32_t) loc.line () );
		file->srcFiles.getIndex ( loc.file_name (), sourceFile::sourceFileType::internal );
		if ( file->parseInnerClass ( src, true, c, false, false, srcLocation () ) )
		{
			throw errorNum::scINTERNAL;
		}
	}
}

void vmInstance::funcCall ( uint32_t atomName, uint32_t nParams )
{
	bcFuncDef *bcFunc;

	bcFunc = atomTable->atomGetFunc ( atomName );
	_ASSERT ( bcFunc->conv == fgxFuncCallConvention::opFuncType_cDecl );
	(*bcFunc->dispatcher)(this, nParams);
}

void vmInstance::funcCall ( bcFuncDef *bcFunc, uint32_t nParams )
{
	_ASSERT ( bcFunc->conv == fgxFuncCallConvention::opFuncType_cDecl );
	(*bcFunc->dispatcher)(this, nParams);
}

