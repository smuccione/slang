// FTPclient.h: interface for the CFTPclient class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <afxsock.h>
#include <afx.h>
#include <stdint.h>
#include "engine\resource.h"

class CMySocket

{
	// Construction
	public:
	CMySocket ( );
	virtual ~CMySocket ( )
	{};
	BOOL Create ( UINT nSocketPort = 0, int nSocketType = SOCK_STREAM,
				  long lEvent = FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE,
				  LPCTSTR lpszSocketAddress = NULL );

	// Implementation
	public:
	//	virtual BOOL	Accept		( CAsyncSocket& rConnectedSocket, SOCKADDR* lpSockAddr = NULL, int* lpSockAddrLen = NULL);
	virtual void	Close ( );
	virtual int		Receive ( void* lpBuf, int nBufLen, int nFlags = 0 );
	virtual int		Send ( const void* lpBuf, int nBufLen, int nFlags = 0 );
	virtual BOOL	Accept ( CMySocket &socket, SOCKADDR* lpSockAddr = NULL, int* lpSockAddrLen = NULL );

	BOOL	Socket ( int nSocketType = SOCK_STREAM, long lEvent =
					 FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE,
					 int nProtocolType = 0, int nAddressFormat = PF_INET );
	BOOL	Bind ( UINT nSocketPort, LPCTSTR lpszSocketAddress = NULL );

	BOOL	Listen ( int nConnectionBacklog = 5 );

	int		GetLastError ( void ) { return (m_LastError); };
	BOOL	Connect ( LPCTSTR lpszHostAddress, UINT nHostPort );
	BOOL	Connect ( const SOCKADDR* lpSockAddr, int nSockAddrLen );

	BOOL	GetSockName ( CString& rSocketAddress, UINT& rSocketPort );

	private:
	int		m_LastError;
	SOCKET	m_socket;
};

class CMySocketFile : public CFile
{
	DECLARE_DYNAMIC ( CMySocketFile )
	public:
	//Constructors
	CMySocketFile ( CMySocket *socket, BOOL bArchiveCompatible = TRUE );

	// Implementation
	public:
	CMySocket *m_pSocket;
	BOOL	   m_bArchiveCompatible;

	virtual ~CMySocketFile ( );

	virtual UINT Read ( void* lpBuf, UINT nCount ) override;
	virtual void Write ( const void* lpBuf, UINT nCount ) override;
	virtual void Close ( ) override;

	// Unsupported APIs
	virtual BOOL Open ( LPCTSTR lpszFileName, UINT nOpenFlags, CFileException* pError = NULL ) override;
	virtual CFile* Duplicate ( ) const override;
	//	virtual DWORD GetPosition() const;
	virtual ULONGLONG GetPosition ( ) const override;
	virtual ULONGLONG Seek ( LONGLONG lOff, UINT nFrom ) override;
	virtual void SetLength ( ULONGLONG dwNewLen ) override;
	//	virtual DWORD GetLength() const;
	virtual ULONGLONG GetLength ( ) const override;
	virtual void LockRange ( ULONGLONG dwPos, ULONGLONG dwCount ) override;
	virtual void UnlockRange ( ULONGLONG dwPos, ULONGLONG dwCount ) override;
	virtual void Flush ( )override;
	virtual void Abort ( ) override;
};


class CFTPclient
{
	public:
	CString m_retmsg;
	BOOL MoveFile ( CString const &remotefile, CString const &localfile, BOOL pasv, BOOL get );
	BOOL MoveBuffer ( CString const &remotefile, struct BUFFER *buff, BOOL pasv, BOOL get );
	void LogOffServer ( );
	BOOL LogOnToServer ( CString const &hostname, int64_t hostport, CString const &username, CString const &password, CString const &acct, CString const &fwhost, CString const &fwusername, CString const &fwpassword, int64_t fwport, int64_t logontype );
	CFTPclient ( );
	virtual ~CFTPclient ( );
	BOOL FTPcommand ( CString const &command );
	char *FTPDataCommand ( CString const &command );
	BOOL ReadStr ( );
	BOOL WriteStr ( CString const &outputstring );

	bool			 loggedOn;

	private:
	CArchive		*m_pCtrlRxarch;
	CArchive		*m_pCtrlTxarch;
	CMySocketFile	*m_pCtrlsokfile;
	CMySocket	 	*m_Ctrlsok;
	int				 m_fc;
	BOOL			 ReadStr2 ( );
	BOOL			 OpenControlChannel ( CString const &serverhost, int serverport );
	void			 CloseControlChannel ( );

	protected:

};
