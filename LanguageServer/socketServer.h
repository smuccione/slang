#include <WinSock2.h>
#include <Windows.h>

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

#include "rpcServer.h"

#include "../compilerParser/fileParser.h"

class lspSocketServerWriterData
{
	public:
	SOCKET		 socket;
	std::mutex &sendMutex;

	lspSocketServerWriterData ( SOCKET socket, std::mutex &sendMutex ) : socket ( socket ), sendMutex ( sendMutex )
	{}


	void send ( BUFFER &buff, lspSocketServerWriterData const &data )
	{
		BUFFER header;
		header.printf ( "Content-Length: %I64u\r\n\r\n", buff.size () );

		std::lock_guard<std::mutex> lock1 ( sendMutex );
		::send ( data.socket, bufferBuff ( &header ), (int) bufferSize ( &header ), 0 );
		::send ( data.socket, bufferBuff ( &buff ), (int) bufferSize ( &buff ), 0 );
	}
};

template <typename S, typename ...T>
class lspSocketServerTaskControl;

template <typename ...T >
class lspSocketServer : public jsonRpcServer<T...>
{

	enum class serverState
	{
		wait,
		readHeader,
		processHeader,
		readBody,
		processBody,
	};

	SOCKET						listenSocket;
	std::set<SOCKET>			serverSockets;

	public:
	lspSocketServer ( uint16_t port, S &&server, T &&...values ) : jsonRpcServer<T...> ( std::forward<T...> ( values... ) )
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

		bind ( listenSocket, (sockaddr *) &addr, sizeof ( SOCKADDR_IN ) );
		listen ( listenSocket, 1 );

		printf ( "Language Server started on port %i\r\n", port );
	}

	void startServer ( lspSocketServerTaskControl<S> *tc );

	void mainLoop ( SOCKET connSocket )
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

		FD_ZERO ( &rxSet );
		FD_ZERO ( &errSet );

		timeout.tv_sec = 0;
		timeout.tv_usec = 500;

		while ( 1 )
		{
			switch ( state )
			{
				case serverState::wait:
					while ( state == serverState::wait )
					{
						FD_SET ( connSocket, &rxSet );
						FD_SET ( connSocket, &errSet );
						if ( select ( 0, &rxSet, nullptr, &errSet, &timeout ) )
						{
							if ( FD_ISSET ( connSocket, &errSet ) )
							{
								// socket has died so exit
								printf ( "language server connection closed\r\n" );
								printf ( "errorno: %i\r\n", WSAGetLastError () );
								return;
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
					len = recv ( connSocket, rxBuff.readPoint<char *> (), (int) rxBuff.avail (), 0 );
					if ( len < 0 )
					{
						// error on the socket or closed, don't care which, just close it and wait for a new connection
						closesocket ( connSocket );
						return;
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
						len = recv ( connSocket, rxBuff.readPoint<char *> (), (int) rxBuff.avail (), 0 );
						if ( len < 0 )
						{
							// error on the socket or closed, don't care which, just close it and wait for a new connection
							closesocket ( connSocket );
							return;
						}
						bufferAssume ( &rxBuff, len );
					}
					break;
				case serverState::processBody:
					{
						stringi body ( rxBuff.data<char const *> (), bodyLength );
						// remove request

						auto req = jsonParser ( body.c_str () );

						lspSocketServerWriterData w ( connSocket, sendMutex );
						jsonRpcServer<lspSocketServerWriterData, S, T...>::call ( req, w );

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
};

template <typename S, typename ...T>
class lspSocketServerTaskControl : public taskControl
{
public:
	std::map<SOCKET, std::thread>	 socketHandles;
	std::mutex						 access;

	HWND							 serverWindow;

	debugSocketServerTaskControl ( HWND serverWindow ) : serverWindow ( serverWindow )
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

		CloseWindow ( serverWindow );

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

template <typename S, typename ...T >
void lspSocketServer<S, T...>::startServer ( lspSocketServerTaskControl<S> *tc )
{
	auto t = std::thread ( [this, tc]() {
		while ( 1 )
		{
			SOCKET connSocket = accept ( listenSocket, 0, 0 );

			if ( connSocket == INVALID_SOCKET )
			{
				return;
			}

			int value = 1;
			setsockopt ( connSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof ( value ) );

			serverSockets.insert ( connSocket );

			// start up the server
			auto t = std::thread ( &debugSocketServer::mainLoop, this, connSocket );
			*tc += std::pair ( connSocket, std::move ( t ) );
		}
						   }
	);
	*tc += std::pair ( listenSocket, std::move ( t ) );;
}
