/*
	WEB Server header file

*/

#include "Utility/settings.h"

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "winsock2.h"
#include <windows.h>

#include "webBuffer.h"
#include "Target/fileCache.h"
#include "Utility/encodingTables.h"
#include "webServer.h"
#include "webProtoH1.h"
#include "webTransactionLog.h"

void webProtocolH1::processBuffer ( class vmInstance *instance, int32_t len )
{
	size_t size;

	if ( len <= 0 )
	{
		switch ( state )
		{
			case connStateExecuteRequest:
			case connStateSendResponse:
				rsp.txBuffList.reset ( );
				rsp.body.release ( );
				QueryPerformanceCounter ( &rsp.reqEndTime );
				state = connStateLogWriteReset;
				break;
			case connStateParseRequest:
			case connStateLogWrite:
			case connStateLogWriteReset:
			case connStateReset:
				state = connStateReset;
				return;
			default:
				throw 500;
		}
	}

	do
	{
	reenter:
		try
		{
			switch ( state )
			{
				case connStateParseRequest:
					try
					{
						switch ( req.processBuffer ( len ) )
						{
							case wsParseURL:
							case wsParseHeaders:
							case wsReceiveBody:
							case wsParseBody:
								break;
							case wsProcess:
								state = connStateExecuteRequest;
								break;
							case wsSendResponse:
								rsp.bytesSent = 0;
								rsp.state = webServerResponseStart;
								state = connStateSendResponse;
								break;
							case wsReset:
								state = connStateReset;
								break;
							default:
								throw 500;
						}
					}
					catch ( int rspCode )
					{
						rsp.rspCode = rspCode;
						if ( !req.method )
						{
							req.method = &req.virtualServer->methodHandlers.find ( statString ( req.methodHeader, req.methodHeader ) )->second;
							if ( !req.method )
							{
								req.method = &req.virtualServer->methodHandlers.find ( statString ( "GET ", 4 ) )->second;
							}
						}
						state = connStateExecuteRequest;
						goto reenter;
					}
					break;
				case connStateExecuteRequest:
					switch ( proc.process ( instance, this ) )
					{
						case wsParseURL:
							state = connStateParseRequest;
							break;
						case wsSendResponse:
							rsp.bytesSent = 0;
							rsp.state = webServerResponseStart;
							state = connStateSendResponse;
							break;
						case wsReset:
							logIndex = 0;
							QueryPerformanceCounter ( &rsp.reqEndTime );
							state = connStateLogWriteReset;
							goto reenter;
						default:
							throw 500;
					}
					break;
				case connStateSendResponse:
					switch ( rsp.processBuffer ( len ) )
					{
						case wsParseURL:
							logIndex = 0;
							state = connStateLogWrite;
							QueryPerformanceCounter ( &rsp.reqEndTime );
							goto reenter;
						case wsReset:
							// finished writing out our response... write the log
							logIndex = 0;
							state = connStateLogWriteReset;
							QueryPerformanceCounter ( &rsp.reqEndTime );
							goto reenter;
						case wsSendResponse:
							break;
						default:
							throw 500;
					}
					break;
				case connStateLogWrite:
					if ( rsp.txBuffList.consume ( len ) )
					{
						if ( logIndex < req.virtualServer->logs.size ( ) )
						{
							rsp.body.buff.erase ( );
							size = req.virtualServer->logs[logIndex].writeLog ( this, rsp.body.buff.getBuffer ( ), rsp.body.buff.getFree ( ) );
							rsp.body.bodyType = webServerResponse::body::btServerBuffer;
							rsp.body.buff.assume ( size );
							logIndex++;
						} else
						{
							reset ( );
						}
					}
					break;
				case connStateLogWriteReset:
					if ( rsp.txBuffList.consume ( len ) )
					{
						if ( logIndex < req.virtualServer->logs.size ( ) )
						{
							rsp.body.buff.erase ( );
							size = req.virtualServer->logs[logIndex].writeLog ( this, rsp.body.buff.getBuffer ( ), rsp.body.buff.getFree ( ) );
							rsp.body.bodyType = webServerResponse::body::btServerBuffer;
							rsp.body.buff.assume ( size );
							logIndex++;
						} else
						{
							reset ( );
							state = connStateReset;
						}
					}
					break;
				case connStateReset:
					break;
				default:
					state = connStateReset;
					break;
			}
			len = 0;
		}
		catch ( ... )
		{
			state = connStateLogWriteReset;
			printf ( "ERROR: exception in webProtocolH1::processBuffer\r\n" );
			goto reenter;
		}
	} while ( getIoOp ( ) == needNone );
}

webProtocolH1::operator HANDLE ( )
{
	switch ( state )
	{
		case connStateExecuteRequest:
			break;
		case connStateLogWrite:
		case connStateLogWriteReset:
			return req.virtualServer->logs[(int64_t)logIndex - 1];
		default:
			printf ( "ERROR: webProtocolH1::operator HANDLE - invalid state - %i\r\n", state );
			throw 500;
	}
	return 0;
}

webBuffs *webProtocolH1::getBuffs ( void )
{
	switch ( state )
	{
		case connStateParseRequest:
			return req.getBuffer ( );
		case connStateExecuteRequest:
			return proc.getBuffer ( );
		case connStateSendResponse:
			return rsp.getBuffer ( );
		case connStateLogWrite:
		case connStateLogWriteReset:
			return rsp.getBufferLog ( );
		default:
			printf ( "ERROR: webProtocolH1::getBuffs - invalid state - %i\r\n", state );
			throw 500;
	}
}
