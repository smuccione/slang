/*
HTTP Client functions

*/

#include "Utility/settings.h"

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

#include <string>
#include <algorithm>
#include <codecvt>
#include <mutex>
#include <vector>
#include <list>
#include <thread>

#include "vmBuiltinTusClient.h"

long long webHttpSendID = 0;

tusSendListManager	sendMgr;


static void tusWorkerThread ( )
{
	while ( 1 )
	{
		top_of_loop:
		sendMgr.sendListAccess.lock ( );
		for ( auto it = sendMgr.sendList.begin ( ); it != sendMgr.sendList.end ( ); it++ )
		{
			if ( !(*it)->error && !(*it)->complete && (*it)->enabled )
			{
				(*it)->running = true;
				auto entry = *it;
				sendMgr.sendListAccess.unlock ( );
				try
				{
					sendMgr.sendFile ( entry );
				} catch ( ... )
				{
					entry->error = true;
					entry->errorMessage = "Protocol Error 1";
				}
				entry->running = false;
				goto top_of_loop;
			}
		}
		sendMgr.sendListAccess.unlock ( );
		Sleep ( 1000 );
	}
}

void tusSendListManager::send ( std::string const &url, std::string const &fName )
{
	auto *entry = new tusSendListManager::sendListEntry;

	entry->url = url;
	entry->fileName = fName;
	entry->id = InterlockedIncrement64 ( &webHttpSendID );

	sendListAccess.lock ( );
	sendList.push_back ( entry );
	sendListAccess.unlock ( );
	updateState ( );
}

void tusSendListManager::init ( std::string const &fName )
{
	if ( this->fName.size ( ) )
	{
		printf ( "error: duplicate reinitialization of TUS client\r\n" );
	}
	if ( !fName.size ( ) )
	{
		return;
	}
	this->fName = fName;
	hFile = CreateFile ( fName.c_str ( ), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

	if ( hFile == INVALID_HANDLE_VALUE )	// NOLINT (performance-no-int-to-ptr)
	{
		return;
	}

	DWORD		 fileSize;
	uint8_t		*data;
	DWORD		 nRead;
	size_t		 nFiles;

	fileSize = GetFileSize ( hFile, 0 );
	data = new uint8_t[fileSize];
	ReadFile ( hFile, data, fileSize, &nRead, 0 );

	if ( nRead )
	{
		nFiles = *(decltype (nFiles) *) data;
		data += sizeof ( nFiles );

		for ( uint32_t loop = 0; loop < nFiles; loop++ )
		{
			auto entry = new sendListEntry ( );

			entry->id = InterlockedIncrement64 ( &webHttpSendID );

			entry->url = (char *) data;
			data += entry->url.size ( ) + 1;

			entry->fileName = (char *) data;
			data += entry->fileName.size ( ) + 1;

			entry->remoteFileName = (char *) data;
			data += entry->remoteFileName.size ( ) + 1;

			entry->errorMessage = (char *) data;
			data += entry->errorMessage.size ( ) + 1;

			entry->priority = *(decltype (entry->priority) *) data;
			data += sizeof ( entry->priority );

			entry->chunkSize = *(decltype (entry->chunkSize) *) data;
			data += sizeof ( entry->chunkSize );

			entry->startTime = *(decltype (entry->startTime) *) data;
			data += sizeof ( entry->startTime );

			entry->error = *(decltype (entry->error) *) data;
			data += sizeof ( entry->error );

			entry->complete = *(decltype (entry->complete) *) data;
			data += sizeof ( entry->complete );

			entry->completeTime = *(decltype (entry->completeTime) *) data;
			data += sizeof ( entry->completeTime );

			entry->enabled = *(decltype (entry->enabled) *) data;
			data += sizeof ( entry->enabled );

			sendList.push_back ( entry );
		}
	}
	std::thread t ( tusWorkerThread );
	t.detach ( );
}

static void sendFileTus ( vmInstance *instance, VAR_STR *url, VAR_STR *fName )
{
	sendMgr.send ( std::string ( url->dat.str.c) , std::string ( fName->dat.str.c ) );
}

VAR sendFileList ( vmInstance *instance )
{
	return sendMgr.sendFileList ( instance );
}

bool sendFileClearEntry ( int64_t id )
{
	return sendMgr.sendFileClearEntry ( id );
}

bool sendFileRetry ( int64_t id )
{
	return sendMgr.sendFileRetry ( id );
}

bool sendFilePause ( int64_t id )
{
	return sendMgr.sendFilePause ( id );
}

bool sendFileResume ( int64_t id )
{
	return sendMgr.sendFileResume ( id );
}

void builtinTUSClientInit(class vmInstance *instance, opFile *file)
{
	REGISTER(instance, file);
		CLASS("webSendFileEntry")
			IVAR ( "ID" );
			IVAR ( "URL" );
			IVAR ( "fileName" );
			IVAR ( "fileSize" );
			IVAR ( "totalSent" );
			IVAR ( "error" );
			IVAR ( "complete" );
		END;
		FUNC ( "webSendFileClearEntry", sendFileClearEntry );
		FUNC ( "webSendFileList", sendFileList );
		FUNC ( "webSendFileRetry", sendFileRetry );
		FUNC ( "webSendFilePause", sendFilePause );
		FUNC ( "webSendFileResume", sendFileResume);
		FUNC ( "webSendFileTus", sendFileTus );
	END;
}
