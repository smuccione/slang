#pragma once

#include "Utility/settings.h"

#include <stdint.h>
#include <functional>
#include <type_traits>
#include <thread>
#include <unordered_map>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>
#include <variant>
#include <memory>
#include <cstdlib>
#include <mutex>

#include "Utility/stringi.h"
#include "Utility/buffer.h"
#include "Utility/funcky.h"
#include "Utility/jsonParser.h"
#include "Target/vmTask.h"

#include "../compilerParser/fileParser.h"

template <typename ...T>
class socketServerTaskControl;

template <typename S, typename ...T >
class socketServer : public  S 
{
	enum class serverState
	{
		wait,
		readHeader,
		processHeader,
		readBody,
		processBody,
	};

	SOCKET						listenSocket = INVALID_SOCKET;
	SOCKET						serverSocket = INVALID_SOCKET;
	std::mutex					sendMutex;

public:
	socketServer ( uint16_t port, T &&...values ) : S ( std::forward<T...> ( values... ) )
	{
		WORD				 wVersionRequested;
		WSADATA				 wsaData;

		/* startup winsock */
		wVersionRequested = MAKEWORD ( 2, 2 );
		WSAStartup ( wVersionRequested, &wsaData );

		listenSocket = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP );

		struct	sockaddr_in	 addr = { 0 };
		addr.sin_port = htons ( port );
		addr.sin_addr.S_un.S_addr = htonl ( INADDR_LOOPBACK );
		addr.sin_family = AF_INET;

		bind ( listenSocket, (sockaddr *)&addr, sizeof ( SOCKADDR_IN ) );
		listen ( listenSocket, 1 );
	}

	void send ( BUFFER &buff )
	{
		BUFFER header;
		header.printf ( "Content-Type: application/vscode-jsonrpc; charset=utf-8\r\n" );
		header.printf ( "Content-Length: %I64u\r\n\r\n", buff.size () );

		std::lock_guard<std::mutex> lock1 ( sendMutex );
		::send ( serverSocket, bufferBuff ( &header ), (int)bufferSize ( &header ), 0 );
		::send ( serverSocket, bufferBuff ( &buff ), (int)bufferSize ( &buff ), 0 );
	}

	void startServer ( socketServerTaskControl<T...> *tc );

	void mainLoop ( SOCKET listenSocket )
	{
		BUFFER			rxBuff;
		size_t			bodyLength = 0;
		serverState		state = serverState::wait;
		opFile			file;
		int				len;
		fd_set			rxSet = {};
		fd_set			errSet = {};
		timeval			timeout = {};
		std::mutex		sendMutex;

		while ( listenSocket )
		{
			SOCKET connSocket = accept ( listenSocket, 0, 0 );

			if ( connSocket == INVALID_SOCKET )
			{
				break;
			}

			int value = 1;
			setsockopt ( connSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof ( value ) );

			FD_ZERO ( &rxSet );
			FD_ZERO ( &errSet );

			timeout.tv_sec = 0;
			timeout.tv_usec = 500;

			serverSocket = connSocket;

			auto termLambda = [=]() { return; };

			S::doTerminate = false;
			S::template startWorker<decltype (termLambda), T...> ( termLambda );

			state = serverState::wait;

			while ( connSocket )
			{
				switch ( state )
				{
					case serverState::wait:
						while ( state == serverState::wait )
						{
							FD_ZERO ( &rxSet );
							FD_ZERO ( &errSet );
							FD_SET ( connSocket, &rxSet );
							FD_SET ( connSocket, &errSet );
							FD_SET ( listenSocket, &errSet );
							if ( select ( 0, &rxSet, nullptr, &errSet, &timeout ) )
							{
								if ( FD_ISSET ( listenSocket, &errSet ) )
								{
									// listen socket has died so exit, either major failure or shutdown in progress, regardless we can not continue
									closesocket ( connSocket );
									closesocket ( listenSocket );
									S::terminate ();
									connSocket = 0;
									listenSocket = 0;
								}
								if ( FD_ISSET ( connSocket, &errSet ) )
								{
									// listen socket has died so exit, either major failure or shutdown in progress, regardless we can not continue
									closesocket ( connSocket );
									S::terminate ();
									connSocket = 0;
								}
								if ( FD_ISSET ( connSocket, &rxSet ) )
								{
									state = serverState::readHeader;
								}
							}
						}
						break;
					case serverState::readHeader:
						rxBuff.makeRoom ( 1024 );

						// --------------------------------------- read in the function name
						len = recv ( connSocket, rxBuff.readPoint<char *> (), (int)rxBuff.avail (), 0 );
						if ( len <= 0 )
						{
							// error on the socket or closed, don't care which, just close it and wait for a new connection
							closesocket ( connSocket );
							S::terminate ();
							connSocket = 0;
							break;
						}
						bufferAssume ( &rxBuff, len );
						state = serverState::processHeader;
						break;
					case serverState::processHeader:
						{
							auto ptr = rxBuff.data<char *> ();
							auto end = ptr + rxBuff.size ();

							state = serverState::readHeader;
							while ( ptr + 1 < end )
							{
								if ( !memcmpi ( ptr, "Content-Length: ", 16 ) )
								{
									ptr += 16;
									bodyLength = atoi ( ptr );
								}
								if ( ptr[0] == '\r' && ptr[1] == '\n' )
								{
									ptr += 2;
									if ( ptr[0] == '\r' && ptr[1] == '\n' )
									{
										ptr += 2;
										state = serverState::readBody;

										rxBuff.remove ( ptr - rxBuff.data<char *> () );
										break;
									}
								} else
								{
									ptr++;
								}
							}
						}
						break;
					case serverState::readBody:
						if ( rxBuff.size () >= bodyLength )
						{
							state = serverState::processBody;
						} else
						{
							if ( rxBuff.avail () <= bodyLength )
							{
								rxBuff.makeRoom ( bodyLength - rxBuff.avail () + 1024 );
							}

							// --------------------------------------- read in the function name
							len = recv ( connSocket, rxBuff.readPoint<char *> (), (int)rxBuff.avail (), 0 );
							if ( len < 0 )
							{
								// error on the socket or closed, don't care which, just close it and wait for a new connection
								closesocket ( connSocket );
								S::terminate ();
								connSocket = 0;
								break;
							}
							bufferAssume ( &rxBuff, len );
						}
						break;
					case serverState::processBody:
						{
							stringi body ( rxBuff.data<char const *> (), bodyLength );
							// remove request

							auto req = jsonParser ( body.c_str () );

							S::call ( req );

							rxBuff.remove ( bodyLength );
							if ( rxBuff.size () )
							{
								state = serverState::processHeader;
							} else
							{
								state = serverState::wait;
							}
						}
						break;
				}
			}
		}
		S::terminate ();
	}
};

template <typename ...T>
class socketServerTaskControl : public taskControl
{
public:
	std::map<SOCKET, std::thread>	 socketHandles;
	std::mutex						 access;

	HWND							 serverWindow;

	socketServerTaskControl ( HWND serverWindow ) : serverWindow ( serverWindow )
	{}

	void end ( void ) override
	{
		std::lock_guard g1 ( access );

		for ( auto &it : socketHandles )
		{
			closesocket ( it.first );
		}

		for ( auto &it : socketHandles )
		{
			it.second.join ();
		}

		if ( serverWindow ) CloseWindow ( serverWindow );

		socketHandles.clear ();
	}

	void operator += ( std::pair<SOCKET, std::thread> socketHandle )
	{
		std::lock_guard g1 ( access );

		socketHandles[std::get<0> ( socketHandle )] = std::move ( std::get<1> ( socketHandle ) );
	}

	void operator -= ( SOCKET socket )
	{
		socketHandles.erase ( socket );
	}
};

template <typename S,typename ...T >
void socketServer<S,T... >::startServer ( socketServerTaskControl<T...> *tc )
{
	auto t = std::thread ( &socketServer::mainLoop, this, listenSocket );
	*tc += std::pair ( listenSocket, std::move ( t ) );
}
