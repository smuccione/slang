// rcpClient.cpp : Defines the entry point for the console application.
//

#pragma once

#include <WinSock2.h>
#include <Windows.h>

#include <stdint.h>
#include <functional>
#include <type_traits>
#include <mutex>

#include "rpcClientImport.h"

namespace rpcClient
{
	SOCKET serverSocket;

	template <class ...> struct rpcParamTypes {};

	template <class T>
	struct rpcClientType {};

	template <typename R, typename ...Params>
	struct rpcClientType <R( Params... )>
	{
		std::string name;
		uint32_t	end_point;
		__declspec(align(64)) uint8_t	*paramBuff;
		std::mutex	mutex;

		rpcClientType (void)
		{
			paramBuff = new uint8_t[buff_size (rpcParamTypes<Params...>{})];
		}

		~rpcClientType (void)
		{
			delete paramBuff;
		}
		 
		template<class Head, class ... Tail>
		size_t buff_size ( rpcParamTypes<Head, Tail...> )
		{
			return sizeof (Head) + buff_size (rpcParamTypes<Tail...>{});
		}

		size_t buff_size ( rpcParamTypes<> )
		{
			return 0;
		}

		R operator () ( uint32_t end_point, Params &&...vs )
		{
			std::lock_guard<std::mutex> lock(mutex);
			return _call( end_point, rpcParamTypes<R>{}, paramBuff, rpcParamTypes<Params...>{}, std::forward<Params>( vs )... );
		}

		template<typename R, class Head, class ... Tail, class ...Vs>
		R _call( uint32_t end_point, rpcParamTypes<R>, uint8_t *buff, rpcParamTypes<Head, Tail...>, Head p, Vs &&...vs )
		{
			*(Head *)buff = p;
			buff += sizeof( Head );
			buff = (uint8_t *)(((ptrdiff_t)buff + 7) & (~7));
			return _call( end_point, rpcParamTypes<R>{}, buff, rpcParamTypes<Tail...>{}, std::forward<Vs>( vs )... );
		}

		void sendRequest( uint32_t end_point, uint32_t paramLen )
		{
			// send the endpoint
			if (send (serverSocket, (char *)&end_point, sizeof (end_point), 0) < 0)
			{
				throw "communication error";
			}

			// send name length
			uint32_t len = (uint32_t)name.length();
			if ( send( serverSocket,(char *)&len, sizeof( uint32_t ), 0 ) < 0 )
			{
				throw "communication error";
			}

			// send name
			if ( send( serverSocket, name.c_str(), len, 0 ) < 0 )
			{
				throw "communication error";
			}

			// send the length of the paramter buffer
			if ( send( serverSocket, (char *)&paramLen, sizeof( uint32_t ), 0 ) < 0 )
			{
				throw "communication error";
			}

			// send the buffer itself
			if ( send( serverSocket, (char *)paramBuff, paramLen, 0 ) < 0 )
			{
				throw "communication error";
			}
		}
		template<typename R, class ...Vs>
		typename std::enable_if<!std::is_same<R, void>::value, R>::type
			_call( uint32_t end_point, rpcParamTypes<R>, uint8_t *buff, rpcParamTypes<> )
		{
			uint32_t len;
			sendRequest( end_point, (uint32_t)(buff - paramBuff) );
			if ( recv( serverSocket, (char *)&len, sizeof ( uint32_t ), 0 ) < 0 )
			{
				throw "communication error";
			}
			if ( len > sizeof( paramBuff ) )
			{
				throw "protocol error";
			}
			if ( recv( serverSocket, (char *)paramBuff, len, 0 ) < 0 )
			{
				throw "communication error";
			}
			// at this point paramBuff has our return value... resolve and return it
			return *(R *)(paramBuff);
		}

		template<typename R, class ...Vs>
		typename std::enable_if<std::is_same<R, void>::value, R>::type
			_call( uint32_t end_point, rpcParamTypes<R>, uint8_t *buff, rpcParamTypes<> )
		{
			sendRequest( end_point, (uint32_t)(buff - paramBuff) );
		}
	};

	void rpcClientInit( void );
}

#undef IMPORT
#define IMPORT(func) extern rpcClient::rpcClientType<decltype(rpcImport::func)> func;
IMPORT_LIST




