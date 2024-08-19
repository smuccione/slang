/*
instance structure for vm execution

*/

#pragma once

#include "math.h"
#include <stack>
#include <map>
#include <utility>
#include <functional>
#include <filesystem>

#include "bcVM/fglTypes.h"
#include "bcVM/vmWorkarea.h"
#include "bcVM/vmObjectMemory.h"
#include "bcVM/vmDebug.h"
#include "bcVM/exeStore.h"
#include "bcVM/vmAtom.h"
#include "compilerParser/funcParser.h"
#include "vmInstance.h"
#include "Utility/stringi.h"

template<typename T, int = std::is_base_of<VAR_ARRAY,T>::value * 8 + std::is_base_of<VAR_OBJ,T>::value * 4 + std::is_integral<T>::value * 2 + std::is_floating_point<T>::value * 1>
struct getPType;

template<typename T>
struct getPType <T, 0>
{
	static inline bcFuncPType toPType ()
	{
		static_assert(dependent_false<T>, "invalid type");
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<typename T>
struct getPType <T, 1>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Double;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<typename T>
struct getPType <T, 2>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Long_Int;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<typename T>
struct getPType <T, 4>
{
	static inline bcFuncPType toPType ( void )
	{
		return bcFuncPType::bcFuncPType_Object;
	}
	static inline char const *getClass()
	{
		return T::getClass();
	}
};

template<typename T>
struct getPType <T, 8>
{
	static inline bcFuncPType toPType ( void )
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return T::getClass();
	}
};

template<>
struct getPType <void >
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_NULL;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <char *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <char const *> 
{
	static inline bcFuncPType toPType()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <std::string> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <std::string &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <std::string const &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi const>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <stringi const &>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR> 
{
	static inline bcFuncPType toPType()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_REF> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_REF *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_REF const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_STR> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_STR *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_STR const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_String;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_OBJ> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_OBJ *> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_OBJ const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Variant;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_ARRAY> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_ARRAY *> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <VAR_ARRAY const *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Array;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <vmInstance *>
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Instance;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <nParamType> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_NParams;
	}
	static inline char const *getClass()
	{
		return "";
	}
};

template<>
struct getPType <nWorkarea> 
{
	static inline bcFuncPType toPType ()
	{
		return bcFuncPType::bcFuncPType_Workarea;
	}
	static inline char const *getClass()
	{
		return "";
	}
};


struct vmDispatcher
{
	virtual ~vmDispatcher ()
	{};
	virtual void     operator() ( vmInstance *instance, uint32_t params ) = 0;
	virtual void     getPTypes ( bcFuncPType *pTypes ) = 0;
	virtual uint32_t getNParams () = 0;
	virtual char const *getClassName () = 0;
};

template<class T>
struct nativeDispatch : vmDispatcher
{};

template<class R, class ... Args> struct nativeDispatch < R ( Args... ) > : vmDispatcher
{
	nativeDispatch<R ( Args... )> ( R ( *func )(Args...) )
	{
		funcPtr = func;
	}
	virtual ~nativeDispatch<R (Args...)> ()
	{
	}

	void operator () ( vmInstance *instance, uint32_t nParams )  override
	{
		GRIP	 g1 ( instance );
		_call ( types<R>{}, instance, nParams, instance->stack - 1, types< Args... >{} );
	}
	void getPTypes ( bcFuncPType *pTypes )  override
	{
		if ( std::is_same<R, void>::value )
		{
			*pTypes = getPType<int>::toPType ();
		} else
		{
			*pTypes = getPType<R>().toPType();
		}
		_getpTypes ( ++pTypes, types<Args...>{}, types<R>{} );
	}

	char const *getClassName () override
	{
		return getPType<R>::getClass ();
	}
	uint32_t getNParams ()  override
	{
		return sizeof... (Args);
	}
	private:
	R ( *funcPtr ) (Args...);

	template<typename RET, class Head, class ... Tail, class ...Vs>
	typename std::enable_if<!(std::is_same<Head, char *>::value || std::is_same<Head, char const *>::value || std::is_same<Head, VAR_STR>::value || std::is_same<Head, VAR_STR *>::value), void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types< Head, Tail ...>, Vs && ...vs )
	{
		_call ( types<RET>{}, instance, nParams, param, types<Tail...>{}, std::forward<Vs> ( vs )..., vmInstance::varConvert<typename std::remove_const<Head>::type>::convert ( instance, &nParams, &param, 0 ) );
	}

	template<typename RET, class Head, class ... Tail, class ...Vs>
	typename std::enable_if<std::is_same<Head, char *>::value || std::is_same<Head, char const *>::value || std::is_same<Head, VAR_STR >::value || std::is_same<Head, VAR_STR *>::value, void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types< Head, Tail ...>, Vs && ...vs )
	{
		char tmpStr[_CVTBUFSIZE]= "";
		_call ( types<RET>{}, instance, nParams, param, types<Tail...>{}, std::forward<Vs> ( vs )..., vmInstance::varConvert<typename std::remove_const<Head>::type>::convert ( instance, &nParams, &param, tmpStr ) );
	}

	template<typename RET, class ...Vs>
	typename std::enable_if<!std::is_same<RET, void>::value, void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types<>, Vs &&...vs )
	{
		instance->result = vmInstance::toVar<RET>::convert ( instance, (funcPtr) (std::forward <Vs> ( vs )...) );
		assert ( instance->result.type != slangType::eSTRING || instance->result.dat.str.c );
	}

	template<typename RET, class ...Vs>
	typename std::enable_if<std::is_same<RET, void>::value, void>::type
		_call ( types<RET>, vmInstance *instance, uint32_t nParams, VAR *param, types<>, Vs &&...vs )
	{
		funcPtr ( std::forward <Vs> ( vs )... );
	}

	template<class Head, class ... Tail, class RET>
	void _getpTypes ( bcFuncPType *pTypes, types< Head, Tail ...>, types<RET> )
	{
		if ( !std::is_void<Head>::value )
		{
			*pTypes = getPType<Head>::toPType ();
			_getpTypes ( ++pTypes, types<Tail...>{}, types<RET>{} );
		}
	}

	template<class RET>
	void _getpTypes ( bcFuncPType *pTypes, types<>, types<RET> )
	{
		//*pTypes = toPType<Head>();
	}
};

class slangDispatcher
{
	vmInstance *instance = 0;
	bcFuncDef  *callerFDef = 0;

	public:
	slangDispatcher ( vmInstance *instance, stringi name ) : instance ( instance )
	{
		callerFDef = instance->atomTable->atomGetFunc ( name.c_str ( ) );
	}

	slangDispatcher ()
	{
	}

	virtual ~slangDispatcher ()
	{};

	template<class ... Args>
	VAR * operator() ( vmInstance *instance, char const *name, Args && ...args )
	{
		this->instance = instance;
		callerFDef = instance->atomTable->atomGetFunc ( name );

		uint32_t nParams = sizeof...(Args);

		// if we don't have enough parameters push nulls
		for ( ; nParams < callerFDef->nParams; nParams++ )
		{
			auto pIndex = callerFDef->nParams - (nParams - sizeof...(Args)) - 1;
			if ( callerFDef->defaultParams && callerFDef->defaultParams[pIndex] )
			{
				instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex], 0, instance->stack, instance->stack );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}

		makeParams ( sizeof... (Args), types< Args... >{}, std::forward<Args> ( args )... );
		switch ( callerFDef->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				instance->interpretBC ( callerFDef, callerFDef->cs, nParams );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				instance->funcCall ( callerFDef, nParams );
				break;
		}
		instance->stack -= nParams;
		return &instance->result;
	}

	template<class ... Args>
	VAR * operator() (Args && ...args)
	{
		uint32_t nParams = sizeof...(Args);

		// if we don't have enough parameters push nulls
		for ( ;nParams < callerFDef->nParams; nParams++ )
		{
			auto pIndex = callerFDef->nParams - (nParams - sizeof...(Args)) - 1;
			if ( callerFDef->defaultParams && callerFDef->defaultParams[pIndex] )
			{
				instance->interpretBCParam ( callerFDef, callerFDef, callerFDef->loadImage->csBase + callerFDef->defaultParams[pIndex], 0, instance->stack, instance->stack );
			} else
			{
				(instance->stack++)->type = slangType::eNULL;
			}
		}

		makeParams ( sizeof... (Args), types< Args... >{}, std::forward<Args> ( args )... );
		switch ( callerFDef->conv )
		{
			case fgxFuncCallConvention::opFuncType_Bytecode:
				instance->interpretBC ( callerFDef, callerFDef->cs, nParams );
				break;
			case fgxFuncCallConvention::opFuncType_cDecl:
				instance->funcCall ( callerFDef, nParams );
				break;
		}
		instance->stack -= nParams;
		return &instance->result;
	}

private:
	template<class size, class Head, class ... Tail>
	void makeParams ( size pCount, types< Head, Tail ...>, Head &&head, Tail && ...args )
	{
		makeParams ( pCount, types<Tail...>{}, std::forward<Tail> ( args )... );
		*(instance->stack++) = vmInstance::toVar<Head>::convert ( instance, head );
	}

	template<class size, class Head >
	void makeParams ( size pCount, types< Head>, Head &&head )
	{
		*(instance->stack++) = vmInstance::toVar<Head>::convert ( instance, head );
	}
	template<class size>
	void makeParams ( size pCount, types<> )
	{
	}
};

class vmNativeInterface
{
	vmInstance					*instance;
	class opFile				*file;
	std::vector<std::string>	 ns;

	public:

	vmNativeInterface ( vmInstance *instance, class opFile *file ) : instance ( instance ), file ( file )
	{}

	std::string getNs ()
	{
		std::string res;
		for ( auto &it : ns )
		{
			res += "::" + it;
		}
		return res;
	}

	public:

	class nativeClass
	{
		vmNativeInterface			*native;
		char const					*nativeClassName;
		fgxClassElementScope		 nativeScope;
		bool						 nativeVirtual = false;
		bool						 nativeStatic = false;
		bool						 nativeExtension = false;

		public:

		nativeClass ( vmNativeInterface *native, char const *className );

		void Public ()
		{
			nativeScope = fgxClassElementScope::fgxClassScope_public;
		};
		void Private ()
		{
			nativeScope = fgxClassElementScope::fgxClassScope_private;
		};
		void Protected ()
		{
			nativeScope = fgxClassElementScope::fgxClassScope_protected;
		};
		void Virtual ()
		{
			nativeVirtual = true;

		};
		void Static ()
		{
			nativeStatic = true;
		};
		void Extension ()
		{
			nativeExtension = true;
		};

		void property ( char const *propName, fgxClassElementType type, char const *initializer = 0 );
		void property ( char const *propName, fgxClassElementType type, int64_t intializer );
		class opFunction *method ( char const *methodName, vmDispatcher *disp, ... );
		class opFunction *methodProp ( char const *methodName, bool isAccess, vmDispatcher *disp, ... );
		class opFunction *op ( char const *opName, vmDispatcher *disp );
		void require( char const* name, ... );
		void registerGcCb ( classGarbageCollectionCB cGarbageCollectionCB, classCopyCB cCopyCb );
		void registerPackCb ( void ( *cPackCB ) (class vmInstance *instance, struct VAR *var, struct BUFFER *buff, void *param, void ( *pack )(struct VAR *val, void *param)), void ( *cUnPackCB ) (class vmInstance *instance, struct VAR *obj, unsigned char ** buff, uint64_t *len, void *param, struct VAR *(*unPack)(unsigned char ** buff, uint64_t *len, void *param)) );
		void registerInspectorCb ( class vmInspectList *(*cInspectorCB) (class vmInstance *instance, struct bcFuncDef *funcDef, struct VAR *var, uint64_t start, uint64_t end) );

		friend class vmNativeInterface;
	};
	class nativeNamespace
	{
		vmNativeInterface  *native;

		public:
		nativeNamespace ( vmNativeInterface *native, std::string const &ns );
		~nativeNamespace ();

		friend class vmNativeInterface;
	};
	bcFuncDef		   *functionBuild ( char const *funcName, vmDispatcher*disp, bool makeFunc );
	class opFunction   *function ( char const *funcName, bool makeFunc, vmDispatcher*disp, ... );
	void				compile ( int cls, char const *code, std::source_location loc = std::source_location::current () );
	void				compile ( vmNativeInterface::nativeClass &cls, char const *code, std::source_location loc = std::source_location::current () );
	void				document ( char const *documentation );
};

// if NO_CALL is defined, it allows us to use all the registrations as interfaces only... the routines will NOT be referenced and
// subsequently not be callable.  However, when we're building the LINKER this is exactly what we want... it gives us all the
// function signatures, but no references to them.  If we build with function linking enabled we get a MUCH smaller executable
#if defined ( NO_CALL )
#define CALLBLE(func) 0
#else
#define CALLABLE(func) (func)
#endif

template<typename T, typename std::enable_if_t<std::is_move_constructible<T>::value>* = nullptr>
static void cGenericGcCb ( class vmInstance *instance, VAR_OBJ *obj, objectMemory::collectionVariables *col )
{
	auto image = obj->getObj<T> ( );
	if ( col->doCopy ( image ) )
	{
		obj->makeObj<T> ( col->getAge ( ), instance, std::move ( *image ) );
	}
}

template<typename T, typename std::enable_if_t<!std::is_move_constructible<T>::value> * = nullptr>
static void cGenericGcCb ( class vmInstance *instance, VAR_OBJ *obj, objectMemory::collectionVariables *col )
{
	auto image = obj->getObj<T> ( );
	if ( col->doCopy ( image ) )
	{
		classAllocateCargo ( instance, obj, sizeof ( T ) );
		memcpy ( obj->getCargo ( ), image, sizeof ( T ) );
	}
}

template<typename T, typename std::enable_if<std::is_copy_constructible<T>::value, int>::type * = nullptr>
static void cGenericCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	auto image = VAR_OBJ ( val ).getObj<T> ( );
	reinterpret_cast<VAR_OBJ *>(val)->makeObj<T> ( instance, *image );
}

template<typename T, typename std::enable_if<!std::is_copy_constructible<T>::value, int>::type * = nullptr>
static void cGenericCopyCb ( class vmInstance *instance, VAR *val, vmCopyCB copy, objectMemory::copyVariables *copyVar )
{
	// TODO: fix defaults that are causing this!!!
//	static_assert (dependent_false<T>, "unsupported generic copyCB" );
	throw errorNum::scINTERNAL;
}

#define REGISTER(instance, file) { vmNativeInterface vm(instance, file); auto &vm2 = vm; volatile int cls = 1;
#define END vm;vm2;cls;}
#define CLASS(name) { vmNativeInterface::nativeClass cls(&vm, name); volatile int vm=1;	/* defining vm here is to prevent non-class (FUNC) definitions from working */
#define NAMESPACE(name) { vmNativeInterface::nativeNamespace ns(&vm, name);
#define METHOD(name,func,...) cls.method ( name, new nativeDispatch<decltype(func)>(CALLABLE(func)), ##__VA_ARGS__, -1 )
#define REQUIRE(name,...) cls.require ( name, ##__VA_ARGS__, nullptr )
#define ACCESS(name, func) cls.methodProp ( name, true, new nativeDispatch<decltype(func)>(CALLABLE(func)), -1 )
#define ASSIGN(name, func) cls.methodProp ( name, false, new nativeDispatch<decltype(func)>(CALLABLE(func)), -1 )
#define OP(name, func) cls.op ( name, new nativeDispatch<decltype(func)>(CALLABLE(func)) )
#define IVAR(name,...) cls.property ( name, fgxClassElementType::fgxClassType_iVar, ##__VA_ARGS__ )
#define INHERIT(name,...) cls.property ( name, fgxClassElementType::fgxClassType_inherit, ##__VA_ARGS__ )
#define CONSTANT(name,...) cls.property ( name, fgxClassElementType::fgxClassType_const, ##__VA_ARGS__ )
#define PUBLIC cls.Public();
#define PRIVATE cls.Private();
#define PROTECTED cls.Protected();
#define VIRTUAL cls.Virtual();
#define STATIC cls.Static();
#define EXTENSION cls.Extension();
#define DEF(num,value) num, value
#if defined ( PURE )
#undef PURE
#endif
#define PURE ->setPure()
#if defined ( CONST )
#undef CONST
#endif
#define CONST ->setConst()

#define GCCB(cb, cb2) cls.registerGcCb ( cb, cb2 );
#define PACKCB(cbPack, cbUnpack) cls.registerPackCb ( cbPack, cbUnpack );
#define INSPECTORCB(cb) cls.registerInspectorCb ( cb );

#define FUNC(name, func,...)	 vm.function ( name, true, new nativeDispatch<decltype(func)>(CALLABLE(func)), ##__VA_ARGS__, -1 )
#define FUNC_RPC(name, func,...) vm.function ( name, true, new nativeDispatch<decltype(func::operator())>(CALLABLE(func)), ##__VA_ARGS__, -1 )

#define DOC(doc) vm.document ( doc );

/* note: for this macro to work correctly the opening ( MUST be on the same line as the string defining the code and the closing ) MUST be on the last line of the string */
//#define CODE(code) vm2.compile ( cls, __LINE__, std::filesystem::absolute ( __FILE__ ).generic_string ( ).c_str(), code );
#define CODE(code) vm2.compile ( cls, code );

