// MesAgentDlg.cpp : 구현 파일
//
#include "stdafx.h"
#include "MesAgent.h"
#include "MesAgentDlg.h"
#include "afxdialogex.h"

#include "Common.h"
#include "LogFile.h"
#include "Handler.h"
#include "Host.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMesAgentDlg 대화 상자

CMesAgentDlg::CMesAgentDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CMesAgentDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMesAgentDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LED_HANDLER_STATE, m_ledHandlerState);
	DDX_Control(pDX, IDC_LED_HOST_STATE, m_ledHostState);
}

BEGIN_MESSAGE_MAP(CMesAgentDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_WM_SHOWWINDOW()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_CHK_HANDLER_LOG, &CMesAgentDlg::OnBnClickedChkHandlerLog)
	ON_BN_CLICKED(IDC_CHK_HOST_LOG, &CMesAgentDlg::OnBnClickedChkHostLog)
	ON_BN_CLICKED(IDC_BTN_HANDLER_LISTEN, &CMesAgentDlg::OnBnClickedBtnHandlerListen)
	ON_BN_CLICKED(IDC_BTN_HOST_LISTEN, &CMesAgentDlg::OnBnClickedBtnHostListen)
	ON_BN_CLICKED(IDC_BTN_HANDLER_CLOSE, &CMesAgentDlg::OnBnClickedBtnHandlerClose)
	ON_BN_CLICKED(IDC_BTN_HOST_CLOSE, &CMesAgentDlg::OnBnClickedBtnHostClose)
	ON_LBN_DBLCLK(IDC_LST_HANDLER_MSG, &CMesAgentDlg::OnDblclkLstHandlerMsg)
	ON_LBN_DBLCLK(IDC_LST_HOST_MSG, &CMesAgentDlg::OnDblclkLstHostMsg)
	ON_BN_CLICKED(IDC_BTN_TEST, &CMesAgentDlg::OnBnClickedBtnTest)
END_MESSAGE_MAP()

// CMesAgentDlg 메시지 처리기

BOOL CMesAgentDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 이 대화 상자의 아이콘을 설정합니다. 응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	m_ledHandlerState.SetColor(RGB(0x00, 0xFF, 0x00), RGB(0x40, 0x40, 0x40));
	m_ledHostState.SetColor(RGB(0x00, 0xFF, 0x00), RGB(0x40, 0x40, 0x40));

	g_objCommon.Create(NULL, NULL, WS_CHILD, CRect(0,0,0,0), this, 0);
	g_objHandler.Create(NULL, NULL, WS_CHILD, CRect(0,0,0,0), this, 0);
	g_objHost.Create(NULL, NULL, WS_CHILD, CRect(0,0,0,0), this, 0);

	CString sTemp;
	sTemp.Format("MesAgent - Varo - Tray - %s", MAIN_VERSION);
	SetWindowText(sTemp);

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다. 문서/뷰 모델을 사용하는 MFC 응용 프로그램의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CMesAgentDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CMesAgentDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

BOOL CMesAgentDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
		return TRUE;

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CMesAgentDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	g_objHost.Terminate();
	g_objHandler.Terminate();
	g_objLogFile.Save_AgentLog("Program End ...............");

	g_objHost.DestroyWindow();
	g_objHandler.DestroyWindow();
	g_objCommon.DestroyWindow();
}

void CMesAgentDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow, nStatus);

	if (!bShow) return;

	g_objCommon.Read_Config();

	SetDlgItemInt(IDC_STC_HANDLER_PORT, HANDLER_PORT);
	((CButton*)GetDlgItem(IDC_CHK_HANDLER_LOG))->SetCheck(gData.bHandlerLog);
	m_ledHandlerState.Off();
	GetDlgItem(IDC_BTN_HANDLER_LISTEN)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_HANDLER_CLOSE)->EnableWindow(TRUE);

	SetDlgItemInt(IDC_STC_HOST_PORT, gData.nHostPort);
	((CButton*)GetDlgItem(IDC_CHK_HOST_LOG))->SetCheck(gData.bHostLog);
	m_ledHostState.Off();
	GetDlgItem(IDC_BTN_HOST_LISTEN)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_HOST_CLOSE)->EnableWindow(TRUE);
	SetDlgItemText(IDC_STC_HOST_IP, "0.0.0.0");

	CString sLog;
	sLog.Format("Program Begin - Varo - Tray ............... [%s]", MAIN_VERSION);
	g_objLogFile.Save_AgentLog(sLog);

	g_objHandler.Initialize();
	g_objHost.Initialize();
	gData.sOperId = "00000";
	gData.nPreEquipState = gData.nCurEquipState = 0; 

	SetTimer(0, 1000, NULL);
}

void CMesAgentDlg::OnTimer(UINT_PTR nIDEvent)
{
	KillTimer(0);

	Check_DeleteLog();	// 오래된 로그 삭제

	DWORD dwTerm = GetTickCount() - g_objHost.Get_LastTime();
	if (g_objHost.Is_Connected() && g_objHost.Is_HostOnline() && dwTerm > 20000) {
		g_objHost.Set_S6F11_ControlState(2);	// 1:Online, 2:Offline
		g_objHandler.Set_ControlState(2);		// 1:Online, 2:Offline
	}

	SetTimer(0, 1000, NULL);
	CDialogEx::OnTimer(nIDEvent);
}

void CMesAgentDlg::OnBnClickedChkHandlerLog()
{
	BOOL bCheck = ((CButton*)GetDlgItem(IDC_CHK_HANDLER_LOG))->GetCheck();
	gData.bHandlerLog = bCheck;

	CIniFileCS INI(gsCurrentDir + "\\Config.ini");
	if (!INI.Check_File()) { AfxMessageBox("Config.ini File Not Found!!!"); return; }
	INI.Set_Bool("DATA", "HANDLER_LOG", bCheck);
}

void CMesAgentDlg::OnBnClickedChkHostLog()
{
	BOOL bCheck = ((CButton*)GetDlgItem(IDC_CHK_HOST_LOG))->GetCheck();
	gData.bHostLog = bCheck;

	CIniFileCS INI(gsCurrentDir + "\\Config.ini");
	if (!INI.Check_File()) { AfxMessageBox("Config.ini File Not Found!!!"); return; }
	INI.Set_Bool("DATA", "HOST_LOG", bCheck);
}

void CMesAgentDlg::OnBnClickedBtnHandlerListen()
{
	GetDlgItem(IDC_BTN_HANDLER_LISTEN)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_HANDLER_CLOSE)->EnableWindow(TRUE);

	g_objHandler.Initialize();
}

void CMesAgentDlg::OnBnClickedBtnHostListen()
{
	GetDlgItem(IDC_BTN_HOST_LISTEN)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_HOST_CLOSE)->EnableWindow(TRUE);

	g_objCommon.Read_Config();
	g_objHost.Initialize();
}

void CMesAgentDlg::OnBnClickedBtnHandlerClose()
{
	GetDlgItem(IDC_BTN_HANDLER_LISTEN)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_HANDLER_CLOSE)->EnableWindow(FALSE);

	m_ledHandlerState.Off();
	g_objHandler.Terminate();
}

void CMesAgentDlg::OnBnClickedBtnHostClose()
{
	GetDlgItem(IDC_BTN_HOST_LISTEN)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_HOST_CLOSE)->EnableWindow(FALSE);

	m_ledHostState.Off();
	g_objHost.Terminate();
}

void CMesAgentDlg::OnDblclkLstHandlerMsg()
{
	CString strMsg;
	int nIndex = ((CListBox *)GetDlgItem(IDC_LST_HANDLER_MSG))->GetCurSel();
	((CListBox *)GetDlgItem(IDC_LST_HANDLER_MSG))->GetText(nIndex, strMsg);
	AfxMessageBox(strMsg);
}

void CMesAgentDlg::OnDblclkLstHostMsg()
{
	CString strMsg;
	int nIndex = ((CListBox *)GetDlgItem(IDC_LST_HOST_MSG))->GetCurSel();
	((CListBox *)GetDlgItem(IDC_LST_HOST_MSG))->GetText(nIndex, strMsg);
	AfxMessageBox(strMsg);
}

///////////////////////////////////////////////////////////////////////////////
// User Function

void CMesAgentDlg::Check_DeleteLog()
{
	static BOOL bCheckLog = FALSE;
	SYSTEMTIME time;
	GetLocalTime(&time);

	if (time.wHour == 7 && time.wMinute == 0) {
		if (!bCheckLog) {
			g_objCommon.Delete_LogAll();
			bCheckLog = TRUE;
		}
	} else bCheckLog = FALSE;
}

void CMesAgentDlg::Set_HandlerConnect(BOOL bConnected)
{
	bConnected ? m_ledHandlerState.On() : m_ledHandlerState.Off();

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strMsg, strTemp;
	strTemp = (bConnected ? "Connected" : "Disconnected");
	strMsg.Format("[%02d:%02d:%02d] Handler %s.", time.wHour, time.wMinute, time.wSecond, strTemp);

	g_objLogFile.Save_AgentLog(strMsg);
	Set_HandlerMsg(strMsg);
}

void CMesAgentDlg::Set_HostConnect(BOOL bConnected, CString strIp, int nPort)
{
	bConnected ? m_ledHostState.On() : m_ledHostState.Off();

	SetDlgItemText(IDC_STC_HOST_IP, strIp);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strMsg, strTemp;
	if (!bConnected) strMsg.Format("[%02d:%02d:%02d] Host Disconnected.", time.wHour, time.wMinute, time.wSecond);
	else strMsg.Format("[%02d:%02d:%02d] Host Connected. - IP[%s] Port[%d]", time.wHour, time.wMinute, time.wSecond, strIp, nPort);

	g_objLogFile.Save_AgentLog(strMsg);
	Set_HostMsg(strMsg);
}

void CMesAgentDlg::Set_HandlerMsg(CString sMsg)
{
	int nCount = ((CListBox *)GetDlgItem(IDC_LST_HANDLER_MSG))->GetCount();
	if (nCount >= 25) ((CListBox *)GetDlgItem(IDC_LST_HANDLER_MSG))->DeleteString(0);

	((CListBox *)GetDlgItem(IDC_LST_HANDLER_MSG))->AddString(sMsg);

	nCount = ((CListBox *)GetDlgItem(IDC_LST_HANDLER_MSG))->GetCount();
	((CListBox *)GetDlgItem(IDC_LST_HANDLER_MSG))->SetCurSel(nCount - 1);
}

void CMesAgentDlg::Set_HostMsg(CString sMsg)
{
	int nCount = ((CListBox *)GetDlgItem(IDC_LST_HOST_MSG))->GetCount();
	if (nCount >= 25) ((CListBox *)GetDlgItem(IDC_LST_HOST_MSG))->DeleteString(0);

	((CListBox *)GetDlgItem(IDC_LST_HOST_MSG))->AddString(sMsg);

	nCount = ((CListBox *)GetDlgItem(IDC_LST_HOST_MSG))->GetCount();
	((CListBox *)GetDlgItem(IDC_LST_HOST_MSG))->SetCurSel(nCount - 1);
}

///////////////////////////////////////////////////////////////////////////////

void CMesAgentDlg::OnBnClickedBtnTest()
{
// 	g_objHost.Set_S6F11_LotInfo("AAAAAAA");
}
