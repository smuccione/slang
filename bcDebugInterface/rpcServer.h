
// rcpServer.cpp : Defines the entry point for the console application.
//


#include <stdint.h>
#include <functional>
#include <type_traits>
#include <thread>
#include <unordered_map>

namespace simpleRpc
{

	extern __declspec(align(64)) uint8_t	paramBuff[4096];

	class rpcDispatcher
	{
		public:
		virtual uint32_t operator () () = 0;
	};

	template <class ...> struct rpcParamTypes {};

	template <class T>
	struct rpcServerType {};

	template <typename R, typename ...Params>
	struct rpcServerType <R( Params... )> : public rpcDispatcher
	{
		R( *funcPtr ) (Params...);

		rpcServerType( R( *f )(Params...) )
		{
			funcPtr = f;
		}

		uint32_t operator () ()
		{
			return _call( rpcParamTypes<R>{}, paramBuff, rpcParamTypes<Params...>{} );
		}

		template<class Head, class ... Tail, class ...Vs>
		typename std::enable_if<std::is_pointer<Head>::value, uint32_t>::type
			_call( rpcParamTypes<R>, uint8_t *buff, rpcParamTypes<Head, Tail...>, Vs &&...vs )
		{
			auto nextBuff = buff + sizeof( Head );
			nextBuff = (uint8_t *)(((ptrdiff_t)nextBuff + 7) & (~7));
			return _call( rpcParamTypes<R>{}, nextBuff, rpcParamTypes<Tail...>{}, std::forward<Vs>( vs )..., buff );
		}

		template<class Head, class ...Tail, class ...Vs>
		typename std::enable_if<!std::is_pointer<Head>::value, uint32_t>::type
			_call( rpcParamTypes<R>, uint8_t *buff, rpcParamTypes<Head, Tail...>, Vs &&...vs )
		{
			auto nextBuff = buff + sizeof( Head );
			nextBuff = (uint8_t *)(((ptrdiff_t)nextBuff + 7) & (~7));
			return _call( rpcParamTypes<R>{}, nextBuff, rpcParamTypes<Tail...>{}, std::forward<Vs>( vs )..., *(Head *)buff );
		}

		template<typename R, class ...Vs>
		typename std::enable_if<std::is_same<R, void>::value, uint32_t>::type
			_call( rpcParamTypes<R>, uint8_t *buff, rpcParamTypes<>, Vs &&...vs )
		{
			funcPtr( std::forward <Vs>( vs )... );
			return 0;
		}

		template<typename R, class ...Vs>
		typename std::enable_if<!std::is_same<R, void>::value, uint32_t>::type
			_call( rpcParamTypes<R>, uint8_t *buff, rpcParamTypes<>, Vs &&...vs )
		{
			*(std::remove_pointer<R>::type *)paramBuff = funcPtr( std::forward <Vs>( vs )... );
			return (uint32_t)(buff - paramBuff);
		}
	};

	extern 	void rpcServer( void );
	extern std::unordered_map<std::string, rpcDispatcher *>	rpcFuncMap;

};

#define EXPORT(func) simpleRpc::rpcFuncMap[std::string(#func)] = new simpleRpc::rpcServerType<decltype(func)>(func);

