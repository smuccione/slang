/*/////////////////////////////////////////////////////////////////////
FTPclient.cpp (c) GDI 1999
V1.0.0 (10/4/99)
Phil Anderson. philip@gd-ind.com

Simple FTP client functionality. If you have any problems with it,
please tell me about them (or better still e-mail me the fixed
code). Please feel free to use this code however you wish, although
if you make changes please put your name in the source & comment what
you did.

Nothing awesome going on here at all (all sockets are used in
synchronous blocking mode), but it does the following
things WinInet doesn't seem to:
* Supports loads of different firewalls (I think, I don't
have access to all types so they haven't all been fully
tested yet)
* Allows you to execute any command on the FTP server
* Adds 10K to your app install rather than 1Mb #;-)

Functions return TRUE if everything went OK, FALSE if there was an,
error. A message describing the outcome (normally the one returned
from the server) will be in m_retmsg on return from the function.
There are a few error msgs in the app's string table that you'll
need to paste into your app, along with this file & FTPclient.h

If you created your app without checking the "Use Windows Sockets"
checkbox in AppWizard, you'll need to add the following bit of code
to you app's InitInstance()

if(!AfxSocketInit())
{
AfxMessageBox("Could not initialize Windows Sockets!");
return FALSE;
}

To use:

1/ Create an object of CFTPclient.

2/ Use LogOnToServer() to connect to the server. Any arguments
not used (e.g. if you're not using a firewall), pass an empty
string or zero for numeric args. You must pass a server port
number, use the FTP default of 21 if you don't know what it is.

3/ Use MoveFile() to upload/download a file, 1st arg is local file
path, 2nd arg is remote file path, 3rd arg is TRUE for a PASV
connection (required by some firewalls), FALSE otherwise, 4th arg
is TRUE to upload, FALSE to download file. MoveFile only works in
synchronous mode (ie the function will not return 'till the transfer
is finished). File transfers are always of type BINARY.

4/ You can use FTPcommand() to execute FTP commands (eg
FTPcommand("CWD /home/mydir") to change directory on the server),
note that this function will return FALSE unless the server response
is a 200 series code. This should work fine for most FTP commands,
otherwise you can use WriteStr() and ReadStr() to send commands &
interpret the response yourself. Use LogOffServer() to disconnect
when done.

/////////////////////////////////////////////////////////////////////*/

#include "bcVMBuiltinFTPClient.h"

#include <stdlib.h>
#include <stdlib.h>

#include "stdio.h"
#include "bcInterpreter/bcInterpreter.h"
#include "bcVM/bcVM.h"
#include "bcVM/Opcodes.h"
#include "bcVM/exeStore.h"
#include "bcVM/vmAtom.h"
#include "bcVM/fglTypes.h"
#include "bcVM/bcVMObject.h"
#include "bcVM/bcVMBuiltin.h"
#include "bcVM/vmDebug.h"
#include "bcVM/vmNativeInterface.h"
#include "bcVM/vmInstance.h"
#include "Utility/buffer.h"

CMySocket::CMySocket ( void )
{
	m_socket = INVALID_SOCKET;
	m_LastError = 0;
}

int CMySocket::Create ( UINT nSocketPort, int nSocketType, long lEvent, LPCTSTR lpszSocketAddress )

{
	if ( Socket ( nSocketType, lEvent ) )
	{
		if ( Bind ( nSocketPort, lpszSocketAddress ) )
		{
			return TRUE;
		}
		Close ( );
	}
	return FALSE;
}

BOOL CMySocket::Socket ( int nSocketType, long lEvent, int nProtocolType, int nAddressFormat )
{
	ASSERT ( m_socket == INVALID_SOCKET );

	m_socket = socket ( nAddressFormat, nSocketType, nProtocolType );

	if ( m_socket != INVALID_SOCKET )
	{
		return TRUE;
	}
	return FALSE;
}

BOOL CMySocket::Bind ( UINT nSocketPort, LPCTSTR lpszSocketAddress )
{
	SOCKADDR_IN sockAddr;
	memset ( &sockAddr, 0, sizeof ( sockAddr ) );

	sockAddr.sin_family = AF_INET;

	if ( lpszSocketAddress == NULL )
		sockAddr.sin_addr.s_addr = htonl ( INADDR_ANY );
	else
	{
		DWORD lResult = inet_addr ( lpszSocketAddress );
		if ( lResult == INADDR_NONE )
		{
			WSASetLastError ( WSAEINVAL );
			return FALSE;
		}
		sockAddr.sin_addr.s_addr = lResult;
	}

	sockAddr.sin_port = htons ( (u_short) nSocketPort );

	return (!(bind ( m_socket, (SOCKADDR*) &sockAddr, sizeof ( sockAddr ) ) == SOCKET_ERROR));
}

void CMySocket::Close ( )
{
	closesocket ( m_socket );
	m_socket = INVALID_SOCKET;
}

int CMySocket::Send ( const void *lpBuf, int nBufLen, int nFlags )

{
	return (send ( m_socket, (char *) lpBuf, nBufLen, nFlags ));
}

int CMySocket::Receive ( void *lpBuf, int bufLen, int nFlags )

{
	return (recv ( m_socket, (char *) lpBuf, bufLen, nFlags ));
}

BOOL CMySocket::Accept ( CMySocket &rConnectedSocket, SOCKADDR* lpSockAddr, int* lpSockAddrLen )
{
	ASSERT ( rConnectedSocket.m_socket == INVALID_SOCKET );

	SOCKET hTemp = accept ( m_socket, lpSockAddr, lpSockAddrLen );

	if ( hTemp == INVALID_SOCKET )
	{
		rConnectedSocket.m_socket = INVALID_SOCKET;
		m_LastError = WSAGetLastError ( );
	} else
	{
		rConnectedSocket.m_socket = hTemp;
	}

	return (hTemp != INVALID_SOCKET);
}


int CMySocket::Connect ( LPCTSTR lpszHostAddress, UINT nHostPort )
{
	ASSERT ( lpszHostAddress != NULL );

	SOCKADDR_IN sockAddr;
	memset ( &sockAddr, 0, sizeof ( sockAddr ) );

	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr ( lpszHostAddress );

	if ( sockAddr.sin_addr.s_addr == INADDR_NONE )
	{
		LPHOSTENT lphost;

#if 0
		struct addrinfo hints = { 0 }, *addrs;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		char port[6];
		sprintf_s( port, sizeof( port ), "%us", nHostPort );

		struct addrinfo hints = { 0 }, *addrs;

		const int status = getaddrinfo( lpszHostAddress, port, &hints, &addrs );
#endif

		lphost = gethostbyname ( lpszHostAddress );
		if ( lphost != NULL )
			sockAddr.sin_addr.s_addr = ((LPIN_ADDR) lphost->h_addr)->s_addr;
		else
		{
			WSASetLastError ( WSAEINVAL );
			return FALSE;
		}
	}

	sockAddr.sin_port = htons ( (u_short) nHostPort );

	return (!(connect ( m_socket, (SOCKADDR*) &sockAddr, sizeof ( sockAddr ) ) == SOCKET_ERROR));
}

BOOL CMySocket::GetSockName ( CString& rSocketAddress, UINT& rSocketPort )
{
	SOCKADDR_IN sockAddr;
	memset ( &sockAddr, 0, sizeof ( sockAddr ) );

	int nSockAddrLen = sizeof ( sockAddr );
	BOOL bResult = getsockname ( m_socket, (SOCKADDR*) &sockAddr, &nSockAddrLen );
	if ( !bResult )
	{
		rSocketPort = ntohs ( sockAddr.sin_port );
		rSocketAddress = inet_ntoa ( sockAddr.sin_addr );
	}
	return !bResult;
}

BOOL CMySocket::Listen ( int nConnectionBacklog )

{
	return (!(listen ( m_socket, nConnectionBacklog ) == SOCKET_ERROR));
}

CMySocketFile::CMySocketFile ( CMySocket *socket, BOOL bArchiveCompatible )

{
	m_pSocket = socket;
	m_bArchiveCompatible = bArchiveCompatible;

}

CMySocketFile::~CMySocketFile ( void )
{

}

UINT CMySocketFile::Read ( void* lpBuf, UINT nCount )
{
	int nRead;

	if ( !m_bArchiveCompatible )
	{
		UINT nLeft = nCount;
		PBYTE pBuf = (PBYTE) lpBuf;

		while ( nLeft > 0 )
		{
			nRead = m_pSocket->Receive ( pBuf, static_cast<int>(nLeft) );
			if ( nRead == SOCKET_ERROR )
			{
				throw GetLastError ( );
			} else if ( nRead == 0 )
			{
				return nCount - nLeft;
			}

			nLeft -= nRead;
			pBuf += nRead;
		}
		return nCount - nLeft;
	}

	nRead = m_pSocket->Receive ( lpBuf, (int)nCount, 0 );
	if ( nRead == SOCKET_ERROR )
	{
		int nError = m_pSocket->GetLastError ( );
		AfxThrowFileException ( CFileException::genericException, nError );
		ASSERT ( FALSE );
	}
	return nRead;
}

void CMySocketFile::Write ( const void* lpBuf, UINT nCount )
{
	ASSERT ( m_pSocket != NULL );

	int nWritten = m_pSocket->Send ( lpBuf, (int)nCount );
	if ( nWritten == SOCKET_ERROR )
	{
		throw GetLastError ( );
	}
}

void CMySocketFile::Close ( )
{
	m_pSocket = NULL;
}


BOOL CMySocketFile::Open ( LPCTSTR /*lpszFileName*/, UINT /*nOpenFlags*/, CFileException* /*pError*/ )
{
	return FALSE;
}

CFile* CMySocketFile::Duplicate ( ) const
{
	return NULL;
}

//DWORD CMySocketFile::GetPosition() const
ULONGLONG CMySocketFile::GetPosition ( ) const
{
	return 0;
}

ULONGLONG CMySocketFile::Seek ( LONGLONG lOff, UINT nFrom )
{
	return 0;
}

void CMySocketFile::SetLength ( ULONGLONG /*dwNewLen*/ )
{
	return;
}

//DWORD CMySocketFile::GetLength() const
ULONGLONG CMySocketFile::GetLength ( ) const
{
	return 0;
}

void CMySocketFile::LockRange ( ULONGLONG /*dwPos*/, ULONGLONG /*dwCount*/ )
{
	return;
}

void CMySocketFile::UnlockRange ( ULONGLONG /*dwPos*/, ULONGLONG /*dwCount*/ )
{
	return;
}

void CMySocketFile::Flush ( )
{
	return;
}

void CMySocketFile::Abort ( )
{
	return;
}

IMPLEMENT_DYNAMIC ( CMySocketFile, CFile )


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFTPclient::CFTPclient ( )
{
	m_pCtrlsokfile = NULL;
	m_pCtrlTxarch = NULL;
	m_pCtrlRxarch = NULL;
	m_Ctrlsok = NULL;
	loggedOn = false;
}


CFTPclient::~CFTPclient ( )
{
	CloseControlChannel ( );
}


//////////////////////////////////////////////////////////////////////
// Public Functions
//////////////////////////////////////////////////////////////////////


// function to connect & log on to FTP server
BOOL CFTPclient::LogOnToServer ( CString const &hostnamep, int64_t hostport, CString const &username, CString const &password, CString const &acct, CString const &fwhost, CString const &fwusername, CString const &fwpassword, int64_t fwport, int64_t logontype ) {
	int64_t port, logonpoint = 0;
	const int LO = -2, ER = -1;
	CString buf, temp;
	CString hostname = hostnamep;
	const int NUMLOGIN = 9; // currently supports 9 different login sequences
	int logonseq[NUMLOGIN][100] =
	{
		// this array stores all of the logon sequences for the various firewalls 
		// in blocks of 3 nums. 1st num is command to send, 2nd num is next point in logon sequence array
		// if 200 series response is rec'd from server as the result of the command, 3rd num is next
		// point in logon sequence if 300 series rec'd
		{ 0,LO,3, 1,LO,6, 2,LO,ER }, // no firewall
		{ 3,6,3, 4,6,ER, 5,ER,9, 0,LO,12, 1,LO,15, 2,LO,ER }, // SITE hostname
		{ 3,6,3, 4,6,ER, 6,LO,9, 1,LO,12, 2,LO,ER }, // USER after logon
		{ 7,3,3, 0,LO,6, 1,LO,9, 2,LO,ER }, //proxy OPEN
		{ 3,6,3, 4,6,ER, 0,LO,9, 1,LO,12, 2,LO,ER }, // Transparent
		{ 6,LO,3, 1,LO,6, 2,LO,ER }, // USER with no logon
		{ 8,6,3, 4,6,ER, 0,LO,9, 1,LO,12, 2,LO,ER }, //USER fireID@remotehost
		{ 9,ER,3, 1,LO,6, 2,LO,ER }, //USER remoteID@remotehost fireID
		{ 10,LO,3, 11,LO,6, 2,LO,ER } // USER remoteID@fireID@remotehost
	};

	if ( logontype<0 || logontype >= NUMLOGIN )
	{
		return FALSE; // illegal connect code
	}
	// are we connecting directly to the host (logon type 0) or via a firewall? (logon type>0)
	if ( !logontype )
	{
		temp = hostname;
		port = hostport;
	} else
	{
		temp = fwhost;
		port = fwport;
	}
	if ( hostport != 21 )
	{
		hostname.Format ( CString (hostname + ":%d"), hostport ); // add port to hostname (only if port is not 21)
	}
	if ( !OpenControlChannel ( temp, (int) port ) )
	{
		return false;
	}
	if ( !FTPcommand ( "" ) )
	{
		return FALSE; // get initial connect msg off server
	}
	// go through appropriate logon procedure
	while ( 1 )
	{
		switch ( logonseq[logontype][logonpoint] )
		{
			case 0:
				temp = "USER " + username;
				break;
			case 1:
				temp = "PASS " + password;
				break;
			case 2:
				temp = "ACCT " + acct;
				break;
			case 3:
				temp = "USER " + fwusername;
				break;
			case 4:
				temp = "PASS " + fwpassword;
				break;
			case 5:
				temp = "SITE " + hostname;
				break;
			case 6:
				temp = "USER " + username + "@" + hostname;
				break;
			case 7:
				temp = "OPEN " + hostname;
				break;
			case 8:
				temp = "USER " + fwusername + "@" + hostname;
				break;
			case 9:
				temp = "USER " + username + "@" + hostname + " " + fwusername;
				break;
			case 10:
				temp = "USER " + username + "@" + fwusername + "@" + hostname;
				break;
			case 11:
				temp = "PASS " + password + "@" + fwpassword;
				break;
		}
		// send command, get response
		if ( !WriteStr ( temp ) )
		{
			return FALSE;
		}
		if ( !ReadStr ( ) )
		{
			return FALSE;
		}
		// only these responses are valid
		if ( m_fc != 2 && m_fc != 3 )
		{
			return FALSE;
		}

		logonpoint = logonseq[logontype][logonpoint + m_fc - 1]; //get next command from array

		switch ( logonpoint )
		{
			case ER: // ER means summat has gone wrong
					 //				m_retmsg.LoadString(IDS_FTPMSG1);
				return FALSE;
			case LO: // LO means we're fully logged on
				return TRUE;
		}
	}
}


// function to log off & close connection to FTP server
void CFTPclient::LogOffServer ( )
{
	WriteStr ( "QUIT" );
	CloseControlChannel ( );
	loggedOn = false;
}


// function to execute commands on the FTP server
BOOL CFTPclient::FTPcommand ( CString const &command )
{
	if ( command != "" && !WriteStr ( command ) )
	{
		return FALSE;
	}
	if ( (!ReadStr ( )) || (m_fc != 2) )
	{
		return FALSE;
	}
	return TRUE;
}

// function to execute commands on the FTP server
char *CFTPclient::FTPDataCommand ( CString const &command )
{
	CString		 lhost, temp, rhost;
	UINT		 localsock, i;
	int			 num;
	char		 cbuf[4096];
	BUFFER		 buff;

	CMySocket	sockSrvr;
	CMySocket	datachannel;

	if ( !FTPcommand ( "TYPE I" ) )
	{
		return 0; // request BINARY mode
	}

	// set up a ACTIVE type file transfer
	m_retmsg.LoadString ( IDS_FTPMSG6 );
	// get the local IP address off the control channel socket
	if ( !m_Ctrlsok->GetSockName ( lhost, localsock ) )
	{
		return 0;
	}
	while ( 1 )
	{
		// convert returned '.' in ip address to ','
		if ( (i = lhost.Find ( "." )) == -1 )
		{
			break;
		}
		lhost.SetAt ( static_cast<int>(i), ',' );
	}
	// create listen socket (let MFC choose the port) & start the socket listening
	if ( (!sockSrvr.Create ( 0, SOCK_STREAM, NULL )) || (!sockSrvr.Listen ( )) )
	{
		return 0;
	}
	if ( !sockSrvr.GetSockName ( temp, localsock ) )
	{
		return 0;// get the port that MFC chose
	}
	// convert the port number to 2 bytes + add to the local IP
	lhost.Format ( lhost + ",%d,%d", localsock / 256, localsock % 256 );

	if ( !FTPcommand ( "PORT " + lhost ) )
	{
		return 0;// send PORT cmd to server
	}

	// send RETR/STOR command to server
	if ( !WriteStr ( command ) )
	{
		return 0;
	}

	if ( !ReadStr ( ) || m_fc != 1 )
	{
		return 0; // get response to RETR/STOR command
	}

	if ( !sockSrvr.Accept ( datachannel ) )
	{
		return 0; // if !PASV accept inbound data connection from server
	}

	while ( 1 )
	{ // move data from/to server & read/write local file
		if ( !(num = datachannel.Receive ( cbuf, sizeof ( cbuf ), 0 )) || num == SOCKET_ERROR )
		{
			break; // (EOF||network error)
		} else
		{
			buff.write ( cbuf, num );
		}
	}

	buff.put ( (char)0 );

	datachannel.Close ( );

	if ( !FTPcommand ( "" ) )
	{
		return 0; // check transfer outcome msg from server
	}
	return (char *) buff.detach ( ); // oh goody it worked.
}



// function to upload/download files
BOOL CFTPclient::MoveFile ( CString const &remotefile, CString const &localfile, BOOL pasv, BOOL get )
{
	CString		lhost, temp, rhost;
	UINT		localsock, serversock, i, j;
	CFile		datafile;
	int			num, numread, numsent;
	char		cbuf[4096];
	CMySocket	sockSrvr;
	CMySocket	datachannel;

	if ( !FTPcommand ( "TYPE I" ) )
	{
		return FALSE; // request BINARY mode
	}
	if ( pasv )
	{
		// set up a PASSIVE type file transfer
		if ( !FTPcommand ( "PASV" ) )
		{
			return FALSE;
		}
		// extract connect port number and IP from string returned by server
		if ( (i = m_retmsg.Find ( "(" )) == -1 || (j = m_retmsg.Find ( ")" )) == -1 )
		{
			return FALSE;
		}
		temp = m_retmsg.Mid ( (int)i + 1, (int)(j - i) - 1 );
		i = temp.ReverseFind ( ',' );
		serversock = atol ( temp.Right ( (int)(temp.GetLength ( ) - (i + 1) )) ); //get ls byte of server socket
		temp = temp.Left ( static_cast<int>(i) );
		i = temp.ReverseFind ( ',' );
		serversock += 256 * atol ( temp.Right ( (int)(temp.GetLength ( ) - (i + 1) )) ); // add ms byte to server socket
		rhost = temp.Left ( static_cast<int>(i) );
		while ( 1 )
		{
			// convert commas to dots in IP
			if ( (i = rhost.Find ( "," )) == -1 )
			{
				break;
			}
			rhost.SetAt ( static_cast<int>(i), '.' );
		}
	} else
	{
		// set up a ACTIVE type file transfer
		m_retmsg.LoadString ( IDS_FTPMSG6 );
		// get the local IP address off the control channel socket
		if ( !m_Ctrlsok->GetSockName ( lhost, localsock ) )
		{
			return FALSE;
		}
		while ( 1 )
		{
			// convert returned '.' in ip address to ','
			if ( (i = lhost.Find ( "." )) == -1 )
			{
				break;
			}
			lhost.SetAt ( static_cast<int>(i), ',' );
		}
		// create listen socket (let MFC choose the port) & start the socket listening
		if ( (!sockSrvr.Create ( 0, SOCK_STREAM, NULL )) || (!sockSrvr.Listen ( )) )
		{
			return FALSE;
		}
		if ( !sockSrvr.GetSockName ( temp, localsock ) ) return FALSE;// get the port that MFC chose
																	  // convert the port number to 2 bytes + add to the local IP
		lhost.Format ( lhost + ",%d,%d", localsock / 256, localsock % 256 );
		if ( !FTPcommand ( "PORT " + lhost ) ) return FALSE;// send PORT cmd to server
	}
	// send RETR/STOR command to server
	if ( !WriteStr ( (get ? "RETR " : "STOR ") + remotefile ) )
	{
		return FALSE;
	}

	if ( pasv )
	{
		// if PASV create the socket & initiate outbound data channel connection
		//		if(!datachannel.Create()) 
		{
			//			m_retmsg.LoadString(IDS_FTPMSG6);
			//			return FALSE;
		}
		datachannel.Connect ( rhost, serversock ); // attempt to connect asynchronously (server will tell us if/when we're connected)
	}

	if ( !ReadStr ( ) || m_fc != 1 )
	{
		return FALSE; // get response to RETR/STOR command
	}

	if ( !pasv && !sockSrvr.Accept ( datachannel ) )
	{
		return FALSE; // if !PASV accept inbound data connection from server
	}
#if 0
	// we're connected & ready to do the data transfer, so set blocking mode on data channel socket
	if ( (!datachannel.AsyncSelect ( 0 )) || (!datachannel.IOCtl ( FIONBIO, &lpArgument )) )
	{
		m_retmsg.LoadString ( IDS_FTPMSG6 );
		return FALSE;
	}
#endif

	// open local file
	if ( !datafile.Open ( localfile, (get ? CFile::modeWrite | CFile::modeCreate : CFile::modeRead) ) )
	{
		datachannel.Close ( );
		m_retmsg.LoadString ( IDS_FTPMSG4 );
		return FALSE;
	}

	while ( 1 )
	{ // move data from/to server & read/write local file
	  //		TRY 
		{
			if ( get )
			{
				if ( !(num = datachannel.Receive ( cbuf, sizeof ( cbuf ), 0 )) || num == SOCKET_ERROR )
				{
					break; // (EOF||network error)
				} else
				{
					datafile.Write ( cbuf, num );
				}
			} else
			{
				if ( !(numread = static_cast<int>(datafile.Read ( cbuf, sizeof ( cbuf ) ))) )
				{
					break; //EOF
				}
				if ( (numsent = datachannel.Send ( cbuf, numread, 0 )) == SOCKET_ERROR )
				{
					break;
				}
				// if we sent fewer bytes than we read from file, rewind file pointer
				if ( numread != numsent )
				{
					datafile.Seek ( static_cast<int64_t>(numsent) - numread, CFile::current );
				}
			}
		}
#if 0
		CATCH ( CException, e )
		{
			m_retmsg.LoadString ( IDS_FTPMSG5 );
			return FALSE;
		}
		END_CATCH
#endif
	}
	datachannel.Close ( );
	datafile.Close ( );
	if ( !FTPcommand ( "" ) )
	{
		return FALSE; // check transfer outcome msg from server
	}
	return TRUE; // oh goody it worked.
}

BOOL CFTPclient::MoveBuffer ( CString const &remotefile, BUFFER *buff, BOOL pasv, BOOL get )
{
	CString		lhost, temp, rhost;
	UINT		localsock, serversock, i, j;
	CFile		datafile;
	size_t			num, numsent;
	const size_t	BUFSIZE = 4096;

	CMySocket	sockSrvr;
	CMySocket	datachannel;

	if ( !FTPcommand ( "TYPE I" ) )
	{
		return FALSE; // request BINARY mode
	}
	if ( pasv )
	{
		// set up a PASSIVE type file transfer
		if ( !FTPcommand ( "PASV" ) )
		{
			return FALSE;
		}
		// extract connect port number and IP from string returned by server
		if ( (i = m_retmsg.Find ( "(" )) == -1 || (j = m_retmsg.Find ( ")" )) == -1 )
		{
			return FALSE;
		}
		temp = m_retmsg.Mid ( (int)i + 1, (int)(j - i) - 1 );
		i = temp.ReverseFind ( ',' );
		serversock = atol ( temp.Right ( (int)(temp.GetLength ( ) - (i + 1) )) ); //get ls byte of server socket
		temp = temp.Left ( static_cast<int>(i) );
		i = temp.ReverseFind ( ',' );
		serversock += 256 * atol ( temp.Right ( (int)(temp.GetLength ( ) - (i + 1) )) ); // add ms byte to server socket
		rhost = temp.Left ( static_cast<int>(i) );
		while ( 1 )
		{
			// convert commas to dots in IP
			if ( (i = rhost.Find ( "," )) == -1 )
			{
				break;
			}
			rhost.SetAt ( static_cast<int>(i), '.' );
		}
	} else
	{
		// set up a ACTIVE type file transfer
		m_retmsg.LoadString ( IDS_FTPMSG6 );
		// get the local IP address off the control channel socket
		if ( !m_Ctrlsok->GetSockName ( lhost, localsock ) )
		{
			return FALSE;
		}
		while ( 1 )
		{
			// convert returned '.' in ip address to ','
			if ( (i = lhost.Find ( "." )) == -1 )
			{
				break;
			}
			lhost.SetAt ( static_cast<int>(i), ',' );
		}
		// create listen socket (let MFC choose the port) & start the socket listening
		if ( (!sockSrvr.Create ( 0, SOCK_STREAM, NULL )) || (!sockSrvr.Listen ( )) )
		{
			return FALSE;
		}
		if ( !sockSrvr.GetSockName ( temp, localsock ) ) return FALSE;// get the port that MFC chose
																	  // convert the port number to 2 bytes + add to the local IP
		lhost.Format ( lhost + ",%d,%d", localsock / 256, localsock % 256 );
		if ( !FTPcommand ( "PORT " + lhost ) ) return FALSE;// send PORT cmd to server
	}
	// send RETR/STOR command to server
	if ( !WriteStr ( (get ? "RETR " : "STOR ") + remotefile ) )
	{
		return FALSE;
	}

	if ( pasv )
	{
		// if PASV create the socket & initiate outbound data channel connection
		//		if(!datachannel.Create()) 
		{
			//			m_retmsg.LoadString(IDS_FTPMSG6);
			//			return FALSE;
		}
		datachannel.Connect ( rhost, serversock ); // attempt to connect asynchronously (server will tell us if/when we're connected)
	}

	if ( !ReadStr ( ) || m_fc != 1 )
	{
		return FALSE; // get response to RETR/STOR command
	}

	if ( !pasv && !sockSrvr.Accept ( datachannel ) )
	{
		return FALSE; // if !PASV accept inbound data connection from server
	}
#if 0
	// we're connected & ready to do the data transfer, so set blocking mode on data channel socket
	if ( (!datachannel.AsyncSelect ( 0 )) || (!datachannel.IOCtl ( FIONBIO, &lpArgument )) )
	{
		m_retmsg.LoadString ( IDS_FTPMSG6 );
		return FALSE;
	}
#endif

	while ( 1 )
	{ // move data from/to server & read/write local file
	  //		TRY 
		{
			if ( get )
			{
				bufferMakeRoom ( buff, BUFSIZE );
				if ( !(num = datachannel.Receive ( bufferBuff ( buff ) + bufferSize ( buff ), (int)bufferFree ( buff ), 0 )) || num == SOCKET_ERROR )
				{
					break; // (EOF||network error)
				} else
				{
					bufferAssume ( buff, num );
				}
			} else
			{
				if ( !bufferSize ( buff ) )
				{
					break;
				}
				if ( (numsent = datachannel.Send ( bufferBuff ( buff ), (int)bufferSize ( buff ), 0 )) == SOCKET_ERROR )
				{
					break;
				}
				// if we sent fewer bytes than we read from file, rewind file pointer
				if ( bufferSize ( buff ) != numsent )
				{
					bufferRemove ( buff, numsent );
				}
			}
		}
#if 0
		CATCH ( CException, e )
		{
			m_retmsg.LoadString ( IDS_FTPMSG5 );
			return FALSE;
		}
		END_CATCH
#endif
	}
	datachannel.Close ( );
	if ( !FTPcommand ( "" ) )
	{
		return FALSE; // check transfer outcome msg from server
	}
	return TRUE; // oh goody it worked.
}

// function to send a command string on the server control channel
BOOL CFTPclient::WriteStr ( CString const &outputstring )
{
	m_retmsg.LoadString ( IDS_FTPMSG6 ); // pre-load "network error" msg (in case there is one) #-)
	TRY
	{
		m_pCtrlTxarch->WriteString ( outputstring + "\r\n" );
	m_pCtrlTxarch->Flush ( );
	}
		CATCH ( CException, e ) {
		return FALSE;
	}
	END_CATCH
		return TRUE;
}


// this function gets the server response line
BOOL CFTPclient::ReadStr ( )
{
	int retcode;

	if ( !ReadStr2 ( ) ) return FALSE;
	if ( m_retmsg.GetLength ( )<4 || m_retmsg.GetAt ( 3 ) != '-' ) return TRUE;
	retcode = atol ( m_retmsg );
	while ( 1 )
	{ //handle multi-line server responses
		if ( m_retmsg.GetLength ( )>3 && (m_retmsg.GetAt ( 3 ) == ' '&&atol ( m_retmsg ) == retcode) ) return TRUE;
		if ( !ReadStr2 ( ) ) return FALSE;
	}
}



//////////////////////////////////////////////////////////////////////
// Private functions
//////////////////////////////////////////////////////////////////////


// read a single response line from the server control channel
BOOL CFTPclient::ReadStr2 ( )
{
	TRY{
		if ( !m_pCtrlRxarch->ReadString ( m_retmsg ) )
		{
			m_retmsg.LoadString ( IDS_FTPMSG6 );
			return FALSE;
		}
	}
		CATCH ( CException, e ) {
		m_retmsg.LoadString ( IDS_FTPMSG6 );
		return FALSE;
	}
	END_CATCH
		if ( m_retmsg.GetLength ( )>0 ) m_fc = m_retmsg.GetAt ( 0 ) - 48; // get 1st digit of the return code (indicates primary result)
	return TRUE;
}


// open the control channel to the FTP server
BOOL CFTPclient::OpenControlChannel ( CString const &serverhost, int serverport )
{
	m_retmsg.LoadString ( IDS_FTPMSG2 );
	if ( !(m_Ctrlsok = new CMySocket) )
	{
		return FALSE;
	}
	if ( !(m_Ctrlsok->Create ( )) )
	{
		return FALSE;
	}
	m_retmsg.LoadString ( IDS_FTPMSG3 );
	if ( !(m_Ctrlsok->Connect ( serverhost, serverport )) )
	{
		return FALSE;
	}
	m_retmsg.LoadString ( IDS_FTPMSG2 );
	if ( !(m_pCtrlsokfile = new CMySocketFile ( m_Ctrlsok )) )
	{
		return FALSE;
	}
	if ( !(m_pCtrlRxarch = new CArchive ( m_pCtrlsokfile, CArchive::load )) )
	{
		return FALSE;
	}
	if ( !(m_pCtrlTxarch = new CArchive ( m_pCtrlsokfile, CArchive::store )) )
	{
		return FALSE;
	}
	return TRUE;
}


// close the control channel to the FTP server
void CFTPclient::CloseControlChannel ( )
{
	if ( m_pCtrlTxarch ) delete m_pCtrlTxarch;
	m_pCtrlTxarch = NULL;
	if ( m_pCtrlRxarch ) delete m_pCtrlRxarch;
	m_pCtrlRxarch = NULL;
	if ( m_pCtrlsokfile ) delete m_pCtrlsokfile;
	m_pCtrlsokfile = NULL;
	if ( m_Ctrlsok ) delete m_Ctrlsok;
	m_Ctrlsok = NULL;
	return;
}

static VAR *resolveParam ( vmInstance *instance, VAR *obj, char const *name, slangType type, int fatal )
{
	VAR	*var;
	if ( !(var = classIVarAccess ( obj, name )) )
	{
		if ( fatal )
		{
			throw errorNum::scINVALID_PARAMETER;
		}
		return 0;
	}

	DEREF ( var );

	if ( var->type != type )
	{
		if ( fatal )
		{
			throw errorNum::scINVALID_PARAMETER;
		}
		return 0;
	}
	return var;
}

/* class initialization */

static VAR_STR cFtpMoveFile ( vmInstance *instance, VAR_OBJ *obj,  char const *fName, char const *rFile, int get )
{
	auto ftp = obj->getObj<CFTPclient> ();

	if ( !ftp->loggedOn )
	{
		return VAR_STR ("");
	}

	// move a file by FTP
	ftp->MoveFile ( rFile, fName, 0, get );

	return VAR_STR ( instance, (char const *) ftp->m_retmsg );
}

VAR_STR cFtpUploadFile ( VAR_OBJ *obj, vmInstance *instance, char const *fName, char const *rFile )
{
	return cFtpMoveFile ( instance, obj, fName, rFile, 0 );
}

VAR_STR cFtpDownloadFile ( VAR_OBJ *obj, vmInstance *instance, char const *fName, char const *rFile )
{
	return cFtpMoveFile ( instance, obj, fName, rFile, 1 );
}

VAR_STR cFtpMoveBuffer ( VAR_OBJ *obj, vmInstance *instance, char *fName, VAR_STR *data, int get )
{
	auto ftp = obj->getObj<CFTPclient> ();

	if ( !ftp->loggedOn )
	{
		throw errorNum::scINVALID_PARAMETER;
	}

	BUFFER buff;

	if ( get )
	{
		// move a file by FTP
		ftp->MoveBuffer ( fName, &buff, 0, get );

		return VAR_STR ( instance, buff );
	} else
	{
		// write file
		bufferWrite ( &buff, data->dat.str.c, data->dat.str.len );

		// move a file by FTP
		ftp->MoveBuffer ( fName, &buff, 0, get );

		return VAR_STR ( instance, ftp->m_retmsg.GetString() );
	}
}

VAR_STR cFtpUploadBuffer ( vmInstance *instance, VAR_OBJ *obj, char *fName, VAR_STR *data )
{
	return cFtpMoveBuffer ( obj, instance, fName, data, 0 );
}

VAR_STR cFtpDownloadBuffer ( vmInstance *instance, VAR_OBJ *obj, char *fName )
{
	return cFtpMoveBuffer ( obj, instance, fName, nullptr, 1 );
}

VAR_STR cFtpCommand ( vmInstance *instance, VAR *obj, char const *cmd )
{
	char					*ret;
	CFTPclient				*ftp;

	ftp = (CFTPclient *) classGetCargo ( obj );

	if ( !ftp->loggedOn )
	{
		return VAR_STR ( "" );
	}

	if ( !(ret = ftp->FTPDataCommand ( cmd )) )
	{
		return VAR_STR ( instance, (char const *)ftp->m_retmsg );
	}
	VAR_STR r ( instance, ret );
	free ( ret );
	return r;
}

VAR_STR cFtpLogon ( vmInstance *instance, VAR *obj )
{
	VAR						*var;
	char const				*hostName;
	int64_t					 hostPort;
	char const				*userName;
	char const				*password;
	char const				*account;
	char const				*fwPassword;
	char const				*fwUserName;
	char const				*fwHost;
	int64_t					 fwPort;
	int64_t					 logonType;
	CFTPclient				*ftp;

	ftp = (CFTPclient *) classGetCargo ( obj );

	if ( !(var = resolveParam ( instance, obj, "HOSTPORT", slangType::eLONG, 0 )) )
	{
		hostPort = 21;
	} else
	{
		hostPort = var->dat.l;
	}

	if ( !(var = resolveParam ( instance, obj, "HOSTNAME", slangType::eSTRING, 1 )) )
	{
		return 0;
	}
	hostName = var->dat.str.c;

	if ( !(var = resolveParam ( instance, obj, "USERNAME", slangType::eSTRING, 1 )) )
	{
		return 0;
	}
	userName = var->dat.str.c;

	if ( !(var = resolveParam ( instance, obj, "PASSWORD", slangType::eSTRING, 1 )) )
	{
		return 0;
	}
	password = var->dat.str.c;

	if ( !(var = resolveParam ( instance, obj, "ACCOUNT", slangType::eSTRING, 0 )) )
	{
		account = "";
	} else
	{
		account = var->dat.str.c;
	}

	if ( !(var = resolveParam ( instance, obj, "FWHOST", slangType::eSTRING, 0 )) )
	{
		fwHost = "";
	} else
	{
		fwHost = var->dat.str.c;
	}

	if ( !(var = resolveParam ( instance, obj, "FWPORT", slangType::eLONG, 0 )) )
	{
		fwPort = 21;
	} else
	{
		fwPort = var->dat.l;
	}

	if ( !(var = resolveParam ( instance, obj, "FWUSERNAME", slangType::eSTRING, 0 )) )
	{
		fwUserName = "";
	} else
	{
		fwUserName = var->dat.str.c;
	}

	if ( !(var = resolveParam ( instance, obj, "FWPASSWORD", slangType::eSTRING, 0 )) )
	{
		fwPassword = "";
	} else
	{
		fwPassword = var->dat.str.c;
	}

	if ( !(var = resolveParam ( instance, obj, "LOGONTYPE", slangType::eSTRING, 0 )) )
	{
		logonType = 0;
	} else
	{
		logonType = var->dat.l;
	}

	if ( !ftp->LogOnToServer ( hostName,
							   hostPort,
							   userName,
							   password,
							   account,
							   fwHost,
							   fwUserName,
							   fwPassword,
							   fwPort,
							   logonType ) )
	{
		return VAR_STR ( instance, (char const *)ftp->m_retmsg );
	}

	ftp->loggedOn = true;

	return VAR_STR ( instance, (char const *) ftp->m_retmsg );
}

VAR_STR cFtpLogoff ( vmInstance *instance, VAR *obj )

{
	CFTPclient				*ftp;

	ftp = (CFTPclient *) classGetCargo ( obj );	// disconnect from server
	ftp->LogOffServer ( );
	ftp->loggedOn = false;

	return VAR_STR ( instance, (char const *) ftp->m_retmsg );
}

void cFtpNew ( vmInstance *instance, VAR_OBJ *var )
{
	var->makeObj<CFTPclient> ( instance );
}

void cFtpRelease ( vmInstance *instance, VAR *obj )
{
	CFTPclient		*ftp;

	ftp = (CFTPclient *) classGetCargo ( obj );

	if ( ftp->loggedOn )
	{
		ftp->LogOffServer ( );
	}

	ftp->~CFTPclient ( );
}

void builtinFTPClient ( class vmInstance *instance, opFile *file )
{
	REGISTER ( instance, file );
		CLASS ( "FTPCLIENT" );
			IVAR ( "logonType" );
			IVAR ( "hostName" );
			IVAR ( "hostPort" );
			IVAR ( "userName" );
			IVAR ( "passWord" );
			IVAR ( "account" );
			IVAR ( "FWHost" );
			IVAR ( "FWPort" );
			IVAR ( "FWUserName" );
			IVAR ( "FWPassword" );

			METHOD ( "new", cFtpNew );
			METHOD ( "release", cFtpRelease );
			METHOD ( "uploadFile", cFtpUploadFile );
			METHOD ( "uploadBuffer", cFtpUploadBuffer );
			METHOD ( "downloadFile", cFtpDownloadFile );
			METHOD ( "command", cFtpCommand );
			METHOD ( "logOn", cFtpLogon );
			METHOD ( "logOff", cFtpLogoff );
			GCCB ( cGenericGcCb<CFTPclient>, cGenericCopyCb<CFTPclient> );
		END;
	END;
}
