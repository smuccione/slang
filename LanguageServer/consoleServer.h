
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
#include <iostream>

#include "Utility/stringi.h"
#include "Utility/buffer.h"
#include "Utility/funcky.h"
#include "Utility/jsonParser.h"
#include "Target/vmTask.h"

#include "../compilerParser/fileParser.h"

template <typename  S>
class lspConsoleServerTaskControl;

template <typename S, typename ...T >
class lspConsoleServer : public S
{
	std::mutex					sendMutex;

	enum class serverState
	{
		wait,
		readHeader,
		processHeader,
		readBody,
		processBody,
	};

	public:
	lspConsoleServer ( T &&...values ) : S ( std::forward<T...> ( values... ) )
	{
	}

	~lspConsoleServer ()
	{
	}

	void send ( BUFFER &buff  )
	{
		std::string header;
		header.append ( "Content-Length: " + std::to_string ( buff.size () ) + "\r\n" );
		header.append ( "Content-Type: application/vscode-jsonrpc;charset=utf-8\r\n" );
		header.append ( "\r\n" );

		std::lock_guard<std::mutex> lock1 ( sendMutex );
		std::cout << header << buff;
		std::cout << std::flush;
	}

	void startServer ( lspConsoleServerTaskControl<T...> *tc );

	void mainLoop ()
	{
		BUFFER			rxBuff;
		size_t			bodyLength = 0;
		serverState		state = serverState::wait;
		opFile			file;
		std::mutex		sendMutex;

		auto termLambda = [=]() {return; };

		S::template startWorker<decltype (termLambda), T...> ( termLambda );

		while ( 1 )
		{
			switch ( state )
			{
				case serverState::wait:
				case serverState::readHeader:
					if ( std::cin.eof() )
					{
						return;
					} else
					{
						// --------------------------------------- read in the header
						char chr;
						std::cin.get( chr );
						rxBuff.put( (unsigned char)chr );
					}
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
									state = serverState::readBody;
									rxBuff.clear();
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
					rxBuff.makeRoom ( bodyLength );

					while ( rxBuff.size() != bodyLength )
					{
						if ( std::cin.eof() )
						{
							return;
						} else
						{
							// --------------------------------------- read in the header
							char chr;
							std::cin.get( chr );
							rxBuff.put( (unsigned char) chr );
						}
					}
					state = serverState::processBody;
					break;
				case serverState::processBody:
					{
						stringi body ( rxBuff.data<char const *> (), bodyLength );
						// remove request

						auto req = jsonParser ( body.c_str () );

						S::call ( req );

						rxBuff.clear();
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

template <typename S >
class lspConsoleServerTaskControl : public taskControl
{
	public:
	std::map<FILE *, std::thread>	 streamHandles;
	std::mutex						 access;

	S								 server = nullptr;

	lspConsoleServerTaskControl ( S server ) : server ( server )
	{}

	void end ( void ) override
	{
		std::lock_guard g1 ( access );

		for ( auto &it : streamHandles )
		{
			fclose ( it.first );
		}

		for ( auto &it : streamHandles )
		{
			it.second.join ();
		}

		delete server;

		streamHandles.clear ();
	}

	void operator += ( std::pair<FILE *, std::thread> streamHandle )
	{
		std::lock_guard g1 ( access );

		streamHandles[std::get<0> ( streamHandle )] = std::move ( std::get<1> ( streamHandle ) );
	}

	void operator -= ( FILE *streamHandle )
	{
		auto t = streamHandles.find ( streamHandle );
		if ( t != streamHandles.end () )
		{
			t->second.detach ();
		}
		streamHandles.erase ( streamHandle );
	}
};

template <typename S, typename ...T >
void lspConsoleServer <S, T...>::startServer ( lspConsoleServerTaskControl<T...> *tc )
{
	auto t = std::thread( &lspConsoleServer::mainLoop, this );
	*tc += std::pair( stdin, std::move( t ) );
}
