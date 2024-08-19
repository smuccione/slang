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

class dbgJsonRPCServerBase
{
	public:
	using notificationID = size_t;

	virtual void sendNotification ( dbgJsonRPCServerBase::notificationID id ) = 0;
	virtual void terminate ( ) = 0;
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
class dbgJsonRpcServer : public dbgJsonRPCServerBase
{
	// public types
public:
	using funcType = jsonElement ( * )(jsonElement const &req, dbgJsonRPCServerBase &server, T...);
	using notificationFunc = std::pair<stringi, jsonElement> ( * )(dbgJsonRPCServerBase &server, T...);
	using methodMap = std::unordered_map<stringi, funcType>;
	using notificationArr = std::vector<notificationFunc>;

public:
	bool								doTerminate = false;

	methodMap							funcs;
	notificationArr 					notificationHandlers;
	std::tuple<T...>					userData;

	std::thread							rpcWorker;

	HANDLE								queueEvent;
	std::mutex							requestAccess;
	guarded_queue<int64_t>				cancelRequest;
	guarded_queue<jsonElement>			requests;

	// method dispatching

	virtual void send ( BUFFER &buff ) = 0;

	jsonElement dispatch ( stringi const &command, jsonElement const &req )
	{
		// start everything off for this call
//		printf ( "dbgCalling: %s\r\n", command.c_str () );
		funcType func = funcs[command];
		if ( func )
		{
			return dispatch ( func, req, std::index_sequence_for<T...> () );
		}
//		printf ( "dbgNeed: %s\r\n", command.c_str () );
		return jsonElement ();
		throw std::tuple{ -32601, stringi ( "command not found" ) };
	}

	template<std::size_t... seq>
	jsonElement dispatch ( funcType func, jsonElement const &req, std::index_sequence<seq...> )
	{
		// build the parameter list for the call
		return dispatch ( func, req, std::get<seq> ( userData )... );
	}

	jsonElement dispatch ( funcType func, jsonElement const &req, T... values )
	{
		// do the actual call
		return func ( req, *this, values... );
	}

	// notification dispatching - note: the dummy value is there to differentiate us in case T is empty
	std::pair<stringi, jsonElement> notificationDispatch ( bool, notificationFunc note )
	{
		// start everyting off for the dispatch... build the sequnce list
		return notificationDispatch ( note, std::index_sequence_for<T...> () );
	}

	template<std::size_t... seq>
	std::pair<stringi, jsonElement>notificationDispatch ( notificationFunc note, std::index_sequence<seq...> )
	{
		// recursively build the parameter list
		return notificationDispatch ( note, std::get<seq> ( userData )... );
	}

	std::pair<stringi, jsonElement> notificationDispatch ( notificationFunc note, T... values )
	{
		// now finally call the notification handler
		return note ( *this, values... );
	}

	HANDLE	newRequest = 0;

public:
	dbgJsonRpcServer ( T &&...values ) : userData ( values... )
	{
		queueEvent = CreateEvent ( 0, FALSE, FALSE, nullptr );
	}

	dbgJsonRpcServer (  )
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
		std::thread jsonRPCWorker ( &dbgJsonRpcServer::template workerThread<TERMINATE>, this, term );
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
				terminate ();
				return;
			}

			while ( !requests.empty () )
			{
				auto const &req = requests.front ();
				requests.pop ();

				stringi command = req["command"];
				stringi type = req["type"];

				if ( type == "request" )
				{
					int64_t seq = req["seq"];

					if ( !cancelRequest.empty () )
					{
						if ( seq == cancelRequest.front () )
						{
							jsonElement rsp;

//							printf ( "*****   cancelling: %s\r\n", command.c_str () );

							cancelRequest.pop ();
							rsp["request_seq"] = seq;
							rsp["message"] = "cancelled";
							rsp["command"] = command;

							BUFFER out;
							rsp.serialize ( out, true );

							send ( out );
							continue;
						} else
						{
							while ( !cancelRequest.empty () && cancelRequest.front () < seq )
							{
								cancelRequest.pop ();
							}
							if ( !cancelRequest.empty () && seq == cancelRequest.front () )
							{
								continue;
							}
						}
					}
					jsonElement rsp;

					rsp["type"] = "response";
					rsp["request_seq"] = seq;
					rsp["command"] = command;

					try
					{
						rsp["body"] = dispatch ( command, req.has("arguments" ) ? req["arguments"] : jsonElement() );
						rsp["success"] = true;
					} catch ( std::tuple<int, stringi> err )
					{
						rsp["success"] = false;
					}
					if ( !rsp.isNull () )
					{
						BUFFER out;

						rsp.serialize ( out, true );

						send ( out );
					}

				} else
				{
					jsonElement rsp;
					printf ( "handling notification: %s\r\n", command.c_str () );
					rsp = dispatch ( command, req["arguments"] );
					assert ( rsp.isNull () );
				}
			}
		}
	}

	void call ( jsonElement const &req )
	{
		stringi const method = req["command"];

		if ( method == "cancel" && req.has ( "arguments" ) && req["arguments"].has ("requestId") )
		{
//			printf ( "****pushing cancel for %I64i\r\n", (int64_t) req["arguments"]["requestId"] );
			cancelRequest.push ( req["arguments"]["requestId"] );
		} else
		{
			requests.push ( req );
		}
		SetEvent ( queueEvent );
	}

	void sendNotification ( dbgJsonRPCServerBase::notificationID id ) override
	{
		auto note = notificationHandlers[id];
		auto [e, body] = notificationDispatch ( true, note );
		if ( e.size () )
		{
//			printf ( "sending notification: %s\r\n", e.c_str () );

			jsonElement		rsp;
			rsp["type"] = "event";
			rsp["event"] = e;
			if ( !body.isNull () )
			{
				rsp["body"] = body;
			}

			BUFFER out;
			rsp.serialize ( out, true );
			send ( out );
		}
	}

	void terminate ( ) override
	{
//		dispatch ( command, *req["arguments"] );
		doTerminate = true;
		SetEvent ( queueEvent );
	}

};
