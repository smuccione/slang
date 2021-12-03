#pragma once

#include "Utility/settings.h"

#include <tuple>
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <functional>

#include "..\Utility\jsonParser.h"

class jsonRPCServerBase
{
public:
	using notificationID = size_t;

	virtual void sendNotification ( jsonRPCServerBase::notificationID id ) = 0;
	virtual void terminate () = 0;
};

template <typename T>
class guarded_queue
{
	std::mutex		guard;
	std::queue<T>	gq;

public:

	void push ( const T &value )
	{
		std::lock_guard lock ( guard );
		gq.push ( value );
	}

	void pop ()
	{
		std::lock_guard lock ( guard );
		gq.pop ();
	}

	auto front ()
	{
		std::lock_guard lock ( guard );
		return gq.front ();
	}

	auto size ()
	{
		std::lock_guard lock ( guard );
		return gq.size ();
	}

	auto empty ()
	{
		std::lock_guard lock ( guard );
		return gq.empty ();
	}
};

template <template<typename ...T> typename  WRITER, typename ...T>
class jsonRpcServer : public jsonRPCServerBase
{
	// public types
public:
	using funcType = jsonElement ( * )(jsonElement const &req, int64_t id, jsonRPCServerBase &server, T...);
	using notificationFunc = std::pair<stringi, jsonElement> ( * )( jsonRPCServerBase &server, T...);
	using methodMap = std::unordered_map<stringi, funcType>;
	using notificationArr = std::vector<notificationFunc>;

	bool								doTerminate = false;
private:
	methodMap							funcs;
	notificationArr 					notificationHandlers;
	std::tuple<T...>					userData;

	std::thread							rpcWorker;

	HANDLE												queueEvent;
	std::mutex											requestAccess;
	guarded_queue<int64_t>								cancelRequest;
	guarded_queue<std::pair<int64_t,jsonElement>>		requests;

	// method dispatching

	virtual void send ( BUFFER &buff ) = 0;

	jsonElement dispatch( stringi const &method, jsonElement const &req, int64_t id ) 
	{
		// start everything off for this call
//		printf ( "calling: %I64i   %s\r\n", id, method.c_str () );
		funcType func = funcs[method];
		if ( func )
		{
			return dispatch ( func, req, id, std::index_sequence_for<T...>());
		}
//		printf ( "need: %I64i   %s\r\n", id, method.c_str () );
		return jsonElement ();
		throw std::tuple {-32601, stringi ("method not found") };
	}

	template<std::size_t... seq>
	jsonElement dispatch ( funcType func, jsonElement const &req, int64_t id, std::index_sequence<seq...> )
	{
		// build the parameter list for the call
		return dispatch ( func, req, id, std::get<seq> ( userData )... );
	}

	jsonElement dispatch ( funcType func, jsonElement const &req, int64_t id, T... values)
	{
		// do the actual call
		return func ( req, id, *this, values... );
	}

	// notification dispatching
	std::pair<stringi, jsonElement> notificationDispatch( bool, notificationFunc note ) 
	{
		// start everyting off for the dispatch... build the sequnce list
		return notificationDispatch ( note, std::index_sequence_for<T...>());
	}

	template<std::size_t... seq>
	std::pair<stringi, jsonElement>notificationDispatch ( notificationFunc note, std::index_sequence<seq...> )
	{
		// recursively build the parameter list
		return notificationDispatch ( note, std::get<seq> ( userData )... );
	}

	std::pair<stringi, jsonElement> notificationDispatch ( notificationFunc note, T... values)
	{
		// now finally call the notification handler
		return note ( *this, values... );
	}

	HANDLE	newRequest = 0;

public:
	jsonRpcServer ( T &&...values ) : userData ( values... )
	{
		queueEvent = CreateEvent ( 0, FALSE, FALSE, nullptr );
	}

	jsonRpcServer ()
	{
		queueEvent = CreateEvent ( 0, FALSE, FALSE, nullptr );
	}

	void setValues ( T &&...values )
	{
		userData = std::tuple ( values... );
	}

	void setMethods ( methodMap const &funcs )
	{
		this->funcs = funcs;
	}

	void setNotificationHandler ( notificationArr const &arr )
	{
		this->notificationHandlers = arr;
	}

	template <typename TERMINATE, typename ...U>
	void startWorker ( TERMINATE term )
	{
		std::thread jsonRPCWorker ( &jsonRpcServer <WRITER, T...>::template workerThread<TERMINATE>, this, term );
		jsonRPCWorker.detach ();
	}

	template <typename TERMINATE >
	void workerThread ( TERMINATE terminate ) 
	{
		while ( 1 )
		{
			WaitForSingleObject ( queueEvent, INFINITE );

			if ( doTerminate )
			{
				doTerminate = false;
				terminate ();
				return;
			}

			while ( !requests.empty () )
			{
				auto event = requests.front (); 
				requests.pop ();

				auto id = std::get<0> ( event );
				auto const req = std::move ( std::get<1> ( event ) );
				stringi method = *req["method"];

				if ( !cancelRequest.empty () && id == cancelRequest.front () )
				{
					jsonElement rsp;

					//						printf ( "*****   cancelling: %s\r\n", method.c_str () );

					cancelRequest.pop ();
					rsp["id"] = id;
					rsp["error"]["jsonrpc"] = "2.0";
					rsp["error"]["code"] = -32800;
					rsp["error"]["message"] = "RequestCancelled";

					BUFFER out;
					rsp.serialize ( out, true );

					send ( out );
					continue;
				} else
				{
					while ( !cancelRequest.empty () && cancelRequest.front () < id )
					{
						cancelRequest.pop ();
					}
					if ( !cancelRequest.empty () && id == cancelRequest.front () ) continue;
				}

				jsonElement rsp;

				try
				{
					stringi method = *req["method"];
					if ( id != INT64_MAX )
					{
						rsp["result"] = dispatch ( method, *req["params"], id );
						rsp["jsonrpc"] = "2.0";
						rsp["id"] = id;
					} else
					{
						rsp = dispatch ( method, *req["params"], 0 );
						if ( !rsp.isNull () )
						{
							rsp["jsonrpc"] = "2.0";
						}
					}
				} catch ( std::tuple<int, stringi> err )
				{
					if ( id != INT64_MAX )
					{
						rsp["id"] = id;
						rsp["error"]["jsonrpc"] = "2.0";
						rsp["error"]["code"] = std::get<0> ( err );
						rsp["error"]["message"] = std::get<1> ( err );
					}
				}
				if ( !rsp.isNull () )
				{
					BUFFER out ( 4096 * 4096 );

					rsp.serialize ( out, true );

					send ( out );
				}
			}
		}
	}

	void call ( jsonElement const &req )
	{
		int64_t id = INT64_MAX;

		if ( req["id"] )
		{
			id = *req["id"];
		}

		stringi const method = *req["method"];

		if ( method == "$/cancelRequest" )
		{
//			printf ( "****pushing cancel for %I64i\r\n", (int64_t)*((*req["params"])["id"]) );
			cancelRequest.push ( *((*req["params"])["id"]) );
		} else
		{
			requests.push ( { id, req }  );
		}
		SetEvent ( queueEvent );
	}

	void sendNotification ( jsonRPCServerBase::notificationID id ) override
	{
		auto handler = notificationHandlers[id];
		auto [method, note] = notificationDispatch ( true, handler );
		if ( !note.isNull () )
		{
			jsonElement		rsp;
			rsp["jsonrpc"] = "2.0";
			rsp["params"] = note;
			rsp["method"] = method;

			BUFFER out;
			rsp.serialize ( out, true );

			send ( out );
		}
	}

	void terminate () override
	{
		doTerminate = true;
		SetEvent ( queueEvent );
	}
};
