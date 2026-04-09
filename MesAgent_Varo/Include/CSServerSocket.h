///////////////////////////////////////////////////////////////////////////////
// CSServerSocket.h
//
#pragma once

#include <afxsock.h>		// MFC socket extensions

#define UM_SERVER_ACCEPT	WM_USER+9008
#define UM_SERVER_RECEIVE	WM_USER+9009
#define UM_SERVER_REMOVE	WM_USER+9010

class CServerSocketCS;

class CDataSocketCS : public CAsyncSocket
{
public:
	CDataSocketCS(CServerSocketCS* pServerSocket);
	virtual ~CDataSocketCS();
	virtual void OnReceive(int nErrorCode);
	virtual void OnClose(int nErrorCode);

private:
	CServerSocketCS*	m_pServerSocket;
	int					m_nReadLen;
	BYTE*				m_pRecvBuff;

public:
	int Get_RecvByte(BYTE* pBuffer);
};

///////////////////////////////////////////////////////////////////////////////
class CServerSocketCS : public CAsyncSocket
{
public:
	CServerSocketCS();
	virtual ~CServerSocketCS();
	virtual void OnAccept(int nErrorCode);

private:
	CWnd*	m_pParent;
	int		m_nPort;
	CList<CDataSocketCS*, CDataSocketCS*> m_pListDataSocket;

public:
	BOOL Listen_Socket(UINT nLocalPort, CWnd* pParent = NULL);
	void Close_Socket();

	int Read_Socket(int nIndex, BYTE* pBuffer);
	BOOL Write_Socket(int nIndex, BYTE* pBuffer, int nLength);

	BOOL Get_ClientInfo(int nIndex, CString& strIp, UINT& nPort);
	int Get_ClientCount();

	void Receive_Client(CDataSocketCS *pDataSocket);
	void Remove_Client(CDataSocketCS *pDataSocket);
	void Clear_Client();
};

///////////////////////////////////////////////////////////////////////////////
