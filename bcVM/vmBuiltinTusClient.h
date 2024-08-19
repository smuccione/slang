/*
HTTP Client functions

*/

#pragma once

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <winsock2.h>
#include <Windows.h>
#include <windns.h>
#include <winhttp.h>
#include <WinDns.h>
#include <IpExport.h>
#include <string.h>
#include <filesystem>

#include <string>
#include <algorithm>
#include <codecvt>
#include <mutex>
#include <vector>
#include <list>
#include <thread>

#include "bcInterpreter/bcInterpreter.h"
#include "Utility/funcky.h"
#include "bcVM/bcVMObject.h"
#include "bcVM/bcVM.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmNativeInterface.h"
#include "Target/fileCache.h"
#include "Target/vmConf.h"

#include "Utility/buffer.h"
#include "Utility/funcky.h"
#include "Utility/util.h"

class hCloser {
	HINTERNET handle;
	public:
	hCloser(HINTERNET handle) : handle(handle)
	{

	}
	~hCloser( )
	{
		if ( handle ) WinHttpCloseHandle(handle);
	}
};

struct tusSendListManager {
	struct sendListEntry {
		std::string		 url;
		std::string		 fileName;
		std::string		 remoteFileName;
		std::string		 errorMessage;
		time_t			 startTime;
		time_t			 completeTime = 0;
		size_t			 priority = 5;
		int64_t			 id;
		DWORD			 chunkSize = 1000000;
		long long		 fileSize = 0;
		long long		 totalSent = 0;
		bool			 error = false;
		bool			 complete = false;
		bool			 enabled = true;
		bool			 running = false;

		virtual ~sendListEntry ( )
		{
		}
	};

	std::mutex					 sendListAccess;
	std::list<sendListEntry *>	 sendList;
	std::string					 fName;

	HANDLE						 hFile;

	~tusSendListManager ( )
	{
		for ( auto &i : sendList )
		{
			delete i;
		}
		CloseHandle ( hFile );
	}
	tusSendListManager ( ) 
	{
	};

	void sendFileTus ( vmInstance *instance, VAR_STR *url, VAR_STR *fName )
	{

	}

	bool sendFileClearEntry ( int64_t id )
	{
		std::lock_guard<std::mutex> l1 ( sendListAccess );

		for ( auto it = sendList.begin ( ); it != sendList.end ( ); it++ )
		{
			if ( (*it)->id == id && !(*it)->running )
			{
				sendList.erase ( it );
				delete *it;
				return true;
			}
		}

		updateState ( );
		return false;
	}

	bool sendFileRetry ( int64_t id )
	{
		std::lock_guard<std::mutex> l1 ( sendListAccess );

		for ( auto it = sendList.begin ( ); it != sendList.end ( ); it++ )
		{
			if ( (*it)->id == id )
			{
				if ( (*it)->error )
				{
					(*it)->error = false;
					return true;
				}
			}
		}
		updateState ( );
		return false;
	}

	bool sendFilePause ( int64_t id )
	{
		std::lock_guard<std::mutex> l1 ( sendListAccess );

		for ( auto it = sendList.begin ( ); it != sendList.end ( ); it++ )
		{
			if ( (*it)->id == id )
			{
				(*it)->enabled = false;
				updateState ( );
				return true;
			}
		}

		updateState ( );
		return false;
	}

	bool sendFileResume ( int64_t id )
	{
		std::lock_guard<std::mutex> l1 ( sendListAccess );

		for ( auto it = sendList.begin ( ); it != sendList.end ( ); it++ )
		{
			if ( (*it)->id == id )
			{
				(*it)->enabled = true;
				updateState ( );
				return true;
			}
		}
		updateState ( );
		return false;
	}

	VAR sendFileList ( vmInstance *instance )
	{
		std::lock_guard<std::mutex> l1 ( sendListAccess );

		VAR arr = VAR_ARRAY ( instance );

		int64_t ctr = 1;
		for ( auto &it : sendList )
		{
			auto val = classNew ( instance, "webSendFileEntry", 0 );

			arrayGet ( instance, &arr, ctr ) = *val;
			ctr++;

			*classIVarAccess ( val, "ID" ) = it->id;
			new (classIVarAccess ( val, "URL" )) VAR_STR ( instance, it->url );
			new (classIVarAccess ( val, "fileName" )) VAR_STR ( instance, it->fileName );
			*classIVarAccess ( val, "fileSize" ) = it->fileSize;
			*classIVarAccess ( val, "totalSent" ) = it->totalSent;
			*classIVarAccess ( val, "error" ) = it->error;
			*classIVarAccess ( val, "complete" ) = it->complete;
		}

		return arr;
	}

	void init ( std::string const &fName );
	void send ( std::string const &url, std::string const &fName );

	private:

	friend void tusWorkerThread ( );

	void updateState ( )
	{
		std::lock_guard<std::mutex> l1 ( sendListAccess );
		SetFilePointer ( hFile, 0, 0, FILE_BEGIN );

		BUFFER	buff;
		DWORD	nWritten;

		buff.put ( sendList.size ( ) );
		for ( auto &it : sendList )
		{
			buff.write ( it->url.c_str ( ), it->url.size ( ) + 1 );
			buff.write ( it->fileName.c_str ( ), it->fileName.size ( ) + 1 );
			buff.write ( it->remoteFileName.c_str ( ), it->remoteFileName.size ( ) + 1 );
			buff.write ( it->errorMessage.c_str ( ), it->errorMessage.size ( ) + 1 );
			buff.put ( it->priority );
			buff.put ( it->chunkSize );
			buff.put ( it->startTime );
			buff.put ( it->error );
			buff.put ( it->complete );
			buff.put ( it->completeTime );
			buff.put ( it->enabled );
		}
		WriteFile ( hFile, bufferBuff ( &buff ), (DWORD)bufferSize ( &buff ), &nWritten, 0 );
		FlushFileBuffers ( hFile );
	}

	enum class tusFileState {
		tusOpenConnection,
		tusSendOptions,
		tusSendHead,
		tusSendPost,
		tusSendPatch,
		tusComplete,
		tusTCPError,
		tusProtocolError,
	};

	std::string ResponseHeader ( HINTERNET hRequest, std::string header )
	{
		DWORD dwSize = 0;

		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

		WinHttpQueryHeaders ( hRequest, WINHTTP_QUERY_CUSTOM, converter.from_bytes ( header.c_str ( ) ).c_str ( ), WINHTTP_NO_OUTPUT_BUFFER, &dwSize, 0 );

		// Allocate memory for the buffer.
		if ( GetLastError ( ) == ERROR_INSUFFICIENT_BUFFER )
		{
			auto headerBuffer = new char[dwSize];

			// Now, use WinHttpQueryHeaders to retrieve the header.
			if ( !WinHttpQueryHeaders ( hRequest, WINHTTP_QUERY_CUSTOM, converter.from_bytes ( header.c_str ( ) ).c_str ( ), headerBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX ) )
			{
				delete[] headerBuffer;
				throw GetLastError ( );
			}

			std::string rsp = converter.to_bytes ( (LPWSTR) headerBuffer );

			delete[] headerBuffer;
			return rsp;
		} else
		{
			throw GetLastError ( );
		}
	}

	void sendFile ( sendListEntry *entry )
	{
		HINTERNET		 hSession = 0;
		HINTERNET 		 hConnect = 0;
		HINTERNET		 hRequest = 0;
		tusFileState	 state = tusFileState::tusOpenConnection;
		fileCacheEntry	*cacheEntry = globalFileCache.read ( entry->fileName.c_str ( ) );
		int64_t			 ulOffset = 0;
		DWORD			 sendLength;
		std::string		 rh;

		entry->error = false;
		entry->errorMessage.clear ( );

		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

		if ( !cacheEntry )
		{
			return;
		}

		entry->fileSize = cacheEntry->getDataLen ( );

		auto uri = Uri::Parse ( converter.from_bytes ( entry->url ) );

		std::wstring uploadLocation = converter.from_bytes ( entry->remoteFileName );

		while ( 1 )
		{
			switch ( state )
			{
				case tusFileState::tusOpenConnection:
					if ( !(hSession = WinHttpOpen ( L"Engine", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 )) )
					{
						state = tusFileState::tusTCPError;
						break;
					}
					{
						unsigned long  value = 60 * 60 * 1000;
						WinHttpSetOption ( hSession, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &value, sizeof ( value ) );
					}
					// Specify an HTTP server.
					if ( !(hConnect = WinHttpConnect ( hSession, uri.Host.c_str ( ), uri.Port, 0 )) )
					{
						state = tusFileState::tusTCPError;
						break;
					}
					WinHttpCloseHandle ( hRequest );
					state = tusFileState::tusSendOptions;
					break;
				case tusFileState::tusTCPError:
					updateState ( );
					Sleep ( 10 * 1000 );
					state = tusFileState::tusSendHead;
					break;
				case tusFileState::tusSendOptions:
					// Create an HTTP request handle.
					if ( !(hRequest = WinHttpOpenRequest ( hConnect, L"OPTIONS", uri.Req.c_str ( ), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uri.isSecure ? WINHTTP_FLAG_SECURE : 0 )) )
					{
						delete entry;
						throw GetLastError ( );
					}
					WinHttpAddRequestHeaders ( hRequest, L"Tus-Resumable: 1.0.0", -1L, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD );
					if ( !WinHttpSendRequest ( hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, NULL ) )
					{
						state = tusFileState::tusTCPError;
						break;
					}
					if ( !WinHttpReceiveResponse ( hRequest, NULL ) )
					{
						state = tusFileState::tusTCPError;
						break;
					}
					WinHttpCloseHandle ( hRequest );
					state = tusFileState::tusSendHead;
					break;
				case tusFileState::tusSendHead:
					if ( entry->remoteFileName.size ( ) )
					{
						// Create an HTTP request handle.
						if ( !(hRequest = WinHttpOpenRequest ( hConnect, L"HEAD", uploadLocation.c_str ( ), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uri.isSecure ? WINHTTP_FLAG_SECURE : 0 )) )
						{
							delete entry;
							throw GetLastError ( );
						}
						WinHttpAddRequestHeaders ( hRequest, L"Tus-Resumable: 1.0.0", -1L, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD );
						if ( !WinHttpSendRequest ( hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, NULL ) )
						{
							state = tusFileState::tusTCPError;
							break;
						}
						if ( !WinHttpReceiveResponse ( hRequest, NULL ) )
						{
							state = tusFileState::tusTCPError;
							break;
						}
						{
							DWORD dwStatusCode = 0;
							DWORD dwSize = sizeof ( dwStatusCode );
							if ( !WinHttpQueryHeaders ( hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &dwStatusCode, &dwSize, NULL ) )
							{
								state = tusFileState::tusTCPError;
								break;
							}
							if ( dwStatusCode != 200 )
							{
								// no complete or partial upload... create remote file and start sending
								state = tusFileState::tusSendPost;
								break;
							}
						}

						auto x = ResponseHeader ( hRequest, "Upload-Offset" );
						ulOffset = _atoi64 ( x.c_str ( ) );
						if ( ulOffset == entry->fileSize )
						{
							state = tusFileState::tusComplete;
						} else
						{
							state = tusFileState::tusSendPatch;
						}

						WinHttpCloseHandle ( hRequest );
					} else
					{
						state = tusFileState::tusSendPost;
					}
					break;
				case tusFileState::tusSendPatch:
					if ( !(hRequest = WinHttpOpenRequest ( hConnect, L"PATCH", uploadLocation.c_str ( ), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uri.isSecure ? WINHTTP_FLAG_SECURE : 0 )) )
					{
						delete entry;
						throw GetLastError ( );
					}
					WinHttpAddRequestHeaders ( hRequest, L"Tus-Resumable: 1.0.0", -1L, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD );
					{
						std::string len = std::string ( "Upload-Offset: " ) + std::to_string ( ulOffset );
						WinHttpAddRequestHeaders ( hRequest, converter.from_bytes ( len ).c_str ( ), -1L, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD );
					}
					sendLength = cacheEntry->getDataLen ( ) - ulOffset > entry->chunkSize ? entry->chunkSize : (DWORD) (cacheEntry->getDataLen ( ) - ulOffset);
					if ( !WinHttpSendRequest ( hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, const_cast<uint8_t *>(cacheEntry->getData ( )) + ulOffset, sendLength, sendLength, NULL ) )
					{
						state = tusFileState::tusTCPError;
						break;
					}
					if ( !WinHttpReceiveResponse ( hRequest, NULL ) )
					{
						state = tusFileState::tusTCPError;
						break;
					}
					{
						DWORD dwStatusCode = 0;
						DWORD dwSize = sizeof ( dwStatusCode );
						if ( !WinHttpQueryHeaders ( hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &dwStatusCode, &dwSize, NULL ) )
						{
							state = tusFileState::tusTCPError;
							break;
						}
						if ( dwStatusCode != 204 )
						{
							// no complete or partial upload... create remote file and start sending
							state = tusFileState::tusSendPost;
							break;
						}
					}
					rh = ResponseHeader ( hRequest, "Upload-Offset" );
					ulOffset = _atoi64 ( rh.c_str ( ) );
					if ( ulOffset == entry->fileSize )
					{
						state = tusFileState::tusComplete;
					} else
					{
						state = tusFileState::tusSendPatch;
					}
					entry->totalSent = ulOffset;
					WinHttpCloseHandle ( hRequest );
					break;
				case tusFileState::tusSendPost:
					if ( !(hRequest = WinHttpOpenRequest ( hConnect, L"POST", uri.Req.c_str ( ), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uri.isSecure ? WINHTTP_FLAG_SECURE : 0 )) )
					{
						delete entry;
						throw GetLastError ( );
					}
					{
						std::filesystem::path p ( entry->fileName.c_str () );
						std::string meta = std::string ( "Upload-Metadata: filename " ) + base64Encode ( std::string ( p.filename().generic_string ().c_str() ) );
						WinHttpAddRequestHeaders ( hRequest, converter.from_bytes ( meta ).c_str ( ), -1L, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD );
					}
					{
						std::string len = std::string ( "Upload-Length: " ) + std::to_string ( cacheEntry->getDataLen ( ) );
						WinHttpAddRequestHeaders ( hRequest, converter.from_bytes ( len ).c_str ( ), -1L, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD );
					}
					{
						WinHttpAddRequestHeaders ( hRequest, L"Tus-Resumable: 1.0.0", -1L, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD );
						if ( !WinHttpSendRequest ( hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, NULL ) )
						{
							state = tusFileState::tusTCPError;
							break;
						}
						if ( !WinHttpReceiveResponse ( hRequest, NULL ) )
						{
							state = tusFileState::tusTCPError;
							break;
						}
						{
							DWORD dwStatusCode = 0;
							DWORD dwSize = sizeof ( dwStatusCode );
							if ( !WinHttpQueryHeaders ( hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX ) )
							{
								state = tusFileState::tusTCPError;
								break;
							}
							if ( dwStatusCode != 201 )
							{
								state = tusFileState::tusProtocolError;
								entry->error = true;
								entry->errorMessage = "Unable to create file on server";
								goto error;
							}
						}
						{
							entry->remoteFileName = ResponseHeader ( hRequest, "Location" );
							Uri u = Uri::Parse ( converter.from_bytes ( entry->remoteFileName ) );
							entry->remoteFileName = converter.to_bytes ( u.Req );
							uploadLocation = converter.from_bytes ( entry->remoteFileName );
							updateState ( );
						}
						ulOffset = 0;
						WinHttpCloseHandle ( hRequest );
						state = tusFileState::tusSendPatch;
						break;
					}
				case tusFileState::tusComplete:
					if ( hConnect ) WinHttpCloseHandle ( hConnect );
					if ( hSession ) WinHttpCloseHandle ( hSession );
					entry->complete = true;
					updateState ( );
					return;
				default:
					if ( hConnect ) WinHttpCloseHandle ( hConnect );
					if ( hSession ) WinHttpCloseHandle ( hSession );
					printf ( "tus error-  aborting\r\n" );
					entry->complete = true;
					updateState ( );
					return;
			}
		}
	error:
		globalFileCache.free ( cacheEntry );
		if ( entry )
		{
			entry->error = true;
		}
		if ( hRequest ) WinHttpCloseHandle ( hRequest );
		if ( hConnect ) WinHttpCloseHandle ( hConnect );
		if ( hSession ) WinHttpCloseHandle ( hSession );
		throw GetLastError ( );
	}
};

extern tusSendListManager	sendMgr;

