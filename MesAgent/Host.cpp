// Host.cpp : 구현 파일입니다.
//
#include "stdafx.h"
#include "MesAgent.h"
#include "Host.h"

#include "Common.h"
#include "LogFile.h"
#include "MesAgentDlg.h"
#include "Handler.h"

IMPLEMENT_DYNAMIC(CHost, CWnd)

CHost g_objHost;

const char STX = 0x02;
const char ETX = 0x03;
const CString CRLF = "\r\n";

// CHost

CHost::CHost()
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;
	m_strStFn = "";
	m_strRcmd = "";

	m_dwLastTime = GetTickCount();
}

CHost::~CHost()
{
}

BEGIN_MESSAGE_MAP(CHost, CWnd)
	ON_MESSAGE(UM_SERVER_ACCEPT, &CHost::OnServerAccept)
	ON_MESSAGE(UM_SERVER_REMOVE, &CHost::OnServerRemove)
	ON_MESSAGE(UM_SERVER_RECEIVE, &CHost::OnServerReceive)
END_MESSAGE_MAP()

// CHost 메시지 처리기입니다.

void CHost::Initialize()
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;
	m_nClientIdx = 0;
	m_Server.Listen_Socket(gData.nHostPort, this);
	m_nSendCmdCount = 0;
	gMes.nAHostCount = 0;
}

void CHost::Terminate()
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;
	m_Server.Close_Socket();
	if (g_objHandler.Is_Connected()) g_objHandler.Set_ControlState(2);	// 1:Online, 2:Offline
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CHost::OnServerAccept(WPARAM wClientIdx, LPARAM lServerPort)
{
	int nClient = (int)wClientIdx;
	int nServerPort = (int)lServerPort;

	CString strIP = "", strLog;
	UINT nPort = 0;
	if (!m_Server.Get_ClientInfo(nClient, strIP, nPort)) return 0;
	m_nClientIdx = nClient;

	strLog.Format("Host Connected. IP(%s), Port, %d", strIP, nPort);
	g_objLogFile.Save_HostLog(strLog);

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HostConnect(TRUE, strIP, nPort);

	m_dwLastTime = GetTickCount();
	m_bConnected = TRUE;

	if (g_objHandler.Is_Connected()) Set_S6F11_ControlState(1);	//1:Online, 2:Offline

	Set_S1F1();

	return 0;
}

LRESULT CHost::OnServerRemove(WPARAM wClientIdx, LPARAM lServerPort)
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;

	CString strLog = "Host Disconnected.";
	g_objLogFile.Save_HostLog(strLog);

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HostConnect(FALSE, "0.0.0.0", 0);

	if (g_objHandler.Is_Connected()) g_objHandler.Set_ControlState(2);	// 1:Online, 2:Offline

	return 0;
}

LRESULT CHost::OnServerReceive(WPARAM wClientIdx, LPARAM lServerPort)
{
	int nClient = (int)wClientIdx;
	int nServerPort = (int)lServerPort;

	CString strIP = "";
	UINT nPort = 0;
	if (!m_Server.Get_ClientInfo(nClient, strIP, nPort)) return 0;

	BYTE byRecv[1025] = { 0 };	// Buffer 1024 -> Last 0x00
	int nLen = m_Server.Read_Socket(nClient, byRecv);

	// Unicode Multibyte 공통 사용 /////////////////////////////////////////////
	char *pRecv = (char*)byRecv;
	CString strRecvSocket = CString(pRecv);
	m_strRecvCmd += strRecvSocket;

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	CString strLog, strMsg;

	while (!m_strRecvCmd.IsEmpty()) {
		int nStart = m_strRecvCmd.Find(STX);
		int nEnd = m_strRecvCmd.Find(ETX);

		if (nEnd < 0) break;	// 버퍼에 들어오는 중...

		if (nStart < 0 || nStart > nEnd) {
			strLog.Format("[OnServerReceive] <<Error>> - Start(%d), End(%d).\n%s", nStart, nEnd, m_strRecvCmd);
			g_objLogFile.Save_HostLog(strLog);
			m_strRecvCmd.Delete(0, nEnd + 1);	// 쓰레기값이 채워져 있어서...
			continue;
		}

		m_dwLastTime = GetTickCount();	// 시간 갱신

		CString strRecv = m_strRecvCmd.Mid(nStart + 1, nEnd - nStart - 1);
		m_strRecvCmd.Delete(0, nEnd + 1);

		// Host Log /////////////////////////////////////////////////////////////////
		strLog.Format("[<-] %s", strRecv);
		g_objLogFile.Save_HostLog(strLog);

		m_nRecvCmdCount = atoi(strRecv.Mid(8, 4));	// 4Byte

		if (strRecv.GetAt(12) == '0') {		// Heart Beat
			strMsg.Format("%s : [HeartBeat]", strLog.Left(18));
			pMainDlg->Set_HostMsg(strMsg);
			Reply_HeartBeat();

		} else {
			CString strXml = strRecv.Right(strRecv.GetLength() - 13);
			if (!Extract_Xml(strXml)) return 0;

			strMsg.Format("%s : %s,%s", strLog.Left(18), m_strStFn, m_strRcmd); 
			pMainDlg->Set_HostMsg(strMsg);

			if		(m_strStFn == "S1F2") Get_S1F2();	// Are You There Data ==> S1F1 응답
			else if (m_strStFn == "S1F3") Get_S1F3();	// Current Recipe Name Request
			else if (m_strStFn == "S2F3") Get_S2F3();	// Link Test Request
			else if (m_strStFn == "S2F31") Get_S2F31(); // Date and Time Set Request
			else if (m_strStFn == "S7F19") Get_S7F19();	// Recip List Request
			else if (m_strStFn == "S10F3") Get_S10F3();	// Terminal Display, Single
			else if (m_strStFn == "S2F49") {			// Remote Command
				if		(m_strRcmd == "LOT_START")		 Get_S2F49_LotStart();
				else if (m_strRcmd == "LOT_ID_FAIL")	 Get_S2F49_LotCancel();
				else if (m_strRcmd == "PRODUCT_DATA")	 Get_S2F49_ProductData();
				else if (m_strRcmd == "PRODUCT_ID_FAIL") Get_S2F49_Module_Fail();
				else if (m_strRcmd == "NG_LOT_START")	 Get_S2F49_NGLotStart();
				else if (m_strRcmd == "NG_LOT_ID_FAIL")	 Get_S2F49_NGLotFail();
/*
				if		(m_strRcmd == "MGZ_CONFIRM") Get_S2F49_MGZConfirm();
				else if (m_strRcmd == "MGZ_CANCEL") Get_S2F49_MGZCancel();
				else if (m_strRcmd == "TRAY_ID_CONFIRM") Get_S2F49_CarrierConfirm();
				else if (m_strRcmd == "TRAY_CANCEL") Get_S2F49_CarrierCancel();
*/
			}
		}
	}
	return 0;
}

BOOL CHost::Extract_Xml(CString sXmlData)
{
	m_strStFn = m_strRcmd = "";	// 초기화

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	if (!m_xml.LoadXml(sXmlData) ) {
		CString strLog, strMsg;

		strMsg.Format("[Extract_Xml] CXml Data Load Fail.");
		pMainDlg->Set_HostMsg(strMsg);

		strLog.Format("%s\n%s", strMsg, sXmlData);
		g_objLogFile.Save_HostLog(strLog);

		return FALSE;
	}

	CXmlNode node = m_xml.GetRoot();
	m_strStFn = node.GetAttribute("ID");

	if (m_strStFn == "S2F31") {
		CXmlNode nodeTime = m_xml.GetRoot()->GetChild("ITEM")->GetChild("TIME");
		m_strSetTime = nodeTime.GetAttribute("VALUE", "");

	} else if(m_strStFn == "S1F3") {
		CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("SVIDLIST")->GetChildren();//GetChild("CPLIST");
		m_nS1F4AckNo = nodes.GetCount();

	} else if(m_strStFn == "S10F3") {
		CXmlNode nodeTime = m_xml.GetRoot()->GetChild("ITEM");
		m_sHostMsg = nodeTime.GetChild("TEXT")->GetAttribute("VALUE");

	} else if (m_strStFn == "S2F49") {
		CXmlNode nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RCMD");
		m_strRcmd = nodeE.GetAttribute("VALUE", "");

		if (m_strRcmd == "LOT_START") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "LOTID")		gMes.sHostLotId = strData;
				if (strName == "PROCID")	gMes.sHostProcID = strData;
				if (strName == "MODEL")		gMes.sHostModel = strData;
				if (strName == "RECIPEID")	gMes.sHostRecipe = strData;
				if (strName == "TOTALQTY")	gMes.nHostCmCount = atoi(strData);
			}
			Set_AddInfor(gMes.sHostLotId, gMes.sHostProcID, gMes.sHostModel);

		} else if (m_strRcmd == "LOT_ID_FAIL") { //CANCEL") {
			CXmlNodes nodesF = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodesF.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodesF[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodesF[i]->GetChild("CPACKC")->GetAttribute("VALUE");
//				CString strData = nodesF[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "LOTID")		gMes.sCancelLotId = strData;
				if (strName == "RECIPEID")	gMes.sCancelRecipe = strData;
			}

			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		} else if (m_strRcmd == "PRODUCT_DATA") {
 			CXmlNodes nodesPD = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
 			int nCount = nodesPD.GetCount();
 
 			for (int i = 0; i < nCount; i++) {
 				CString strName = nodesPD[i]->GetChild("CPNAME")->GetAttribute("VALUE");
 				CString strData = nodesPD[i]->GetChild("CPVAL")->GetAttribute("VALUE");
 
 				if (strName == "LOTID")		gMes.sPDHostLotId = strData;
				if (strName == "PROCID")	gMes.sPDHostProcID = strData;
				if (strName == "MODEL")		gMes.sPDHostModel = strData;
 				if (strName == "MODULEID")	gMes.sPDHostCmId = strData;
 				if (strName == "RESULT")	gMes.sPDHostJudge = strData;
 				if (strName == "DETAIL")	gMes.sPDHostDetail = strData;
 			}

		} else if (m_strRcmd == "PRODUCT_ID_FAIL") {
			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelModule = nodeE.GetChild("MODULEID")->GetAttribute("VALUE");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		} else if (m_strRcmd == "NG_LOT_START") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "LOTID")		gMes.sHostNGLotId = strData;
				if (strName == "PROCID")	gMes.sHostNGProcID = strData;
				if (strName == "MODEL")		gMes.sHostNGModel = strData;
				if (strName == "RECIPEID")	gMes.sHostNGRecipe = strData;
			}

		} else if (m_strRcmd == "NG_LOT_ID_FAIL") { //CANCEL") {
			CXmlNodes nodesF = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodesF.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodesF[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodesF[i]->GetChild("CPACKC")->GetAttribute("VALUE");
//				CString strData = nodesF[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "LOTID")		gMes.sCancelLotId = strData;
				if (strName == "RECIPEID")	gMes.sCancelRecipe = strData;
			}

			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

/*
		} else if (m_strRcmd == "TRAY_CANCEL") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();
			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "LOTID")		gMes.sCancelLotId = strData;
				if (strName == "TRAYID")	gMes.sCancelCarrierId = strData;
			}

			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		} else if (m_strRcmd == "MGZ_CANCEL") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "LOTID")	gMes.sCancelLotId = strData;
				if (strName == "MGZID")	gMes.sCancelMGZId = strData;
			}

			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		} else if (m_strRcmd == "TRAY_ID_CONFIRM") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();
			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

// 				if (strName == "LOTID")		gMes.sHostLotId[gMes.nLotReadyPortNo-1] = strData;
				if (strName	== "TRAYID")	gMes.sTrayId_CarrierId = strData;
				if (strName == "TOTALQTY")	gMes.nHostCarrierCmCount[gMes.nTrayId_CarrierNo-1] = atoi(strData);
			}
			nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("MAPINFO")->GetChild("PRODUCTLIST")->GetChildren();
			nCount = nodes.GetCount();
			for (int i = 0; i < nCount; i++) {
				CString strPocket = nodes[i]->GetChild("POCKETID")->GetAttribute("VALUE");
				int nPocket =  atoi(strPocket);

				CString strBar = nodes[i]->GetChild("PRODUCTID")->GetAttribute("VALUE");
				CString strJudge = nodes[i]->GetChild("STATUS")->GetAttribute("VALUE");

				gMes.sHostPocketID[nPocket-1] = strPocket;
				gMes.sHostCmID[nPocket-1] = strBar;
				gMes.sHostCmJudge[nPocket-1] = strJudge;
// 				if (strName == "POCKETID")	gMes.sHostPocketID[i] = strData;
// 				if (strName == "PRODUCTID")	gMes.sHostCmID[i] = strData;
// 				if (strName == "STATUS")	gMes.sHostCmJudge[i] = strData;
			}

		} else if (m_strRcmd == "MGZ_CONFIRM") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "MGZID")		gMes.sHostMGZId[gMes.nReportUnloadMGZNo-1+3] = strData;
			}

// 		} else if (m_strRcmd == "PRODUCT_INFO_DATA_BY_PROCID") {
// 			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
// 			int nCount = nodes.GetCount();
// 
// 			for (int i = 0; i < nCount; i++) {
// 				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
// 				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");
// 
// 				if (strName == "LOTID")	  gMes.sLotInfoId = strData;
// 				if (strName == "M_MAKER") gMes.sLotInfo = strData;
// 			}
*/
		}
	}
	m_xml.Close();
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Get Command

void CHost::Get_S1F2()
{
	// S1F1 에 대한 응답
}

void CHost::Get_S1F3()
{
	if (m_nS1F4AckNo == 1) g_objHandler.Set_RecipeListRequest(FALSE);
	if (m_nS1F4AckNo == 3) Set_S1F4_State();
}

void CHost::Get_S7F19()
{
	g_objHandler.Set_RecipeListRequest(TRUE);
}

void CHost::Get_S2F3()
{
	Set_S2F4();
}

void CHost::Get_S2F31()
{
	CString strLog;
	if (m_strSetTime.GetLength() < 14) {
		strLog.Format("[Get_S2F31] Time Value Error => Time [%s]", m_strSetTime);
		g_objLogFile.Save_HostLog(strLog);
		return;
	}

	// PC Time set처리
	// UAC disable: 컴푸터.속성.관리센터.사용자 계정 컨터롤 설정변경.알리지 않음.저장
	//ShellExecute(NULL, "open", "cmd.exe", "/c time hh:mm:ss.sss", NULL, SW_HIDE);

	SYSTEMTIME sysTime;
	GetLocalTime(&sysTime);

	//m_strSetTime = "20180601123040";	// 2018-06-01 12:30:40
	sysTime.wYear = atoi(m_strSetTime.Mid(0, 4));
	sysTime.wMonth = atoi(m_strSetTime.Mid(4, 2));
	sysTime.wDay = atoi(m_strSetTime.Mid(6, 2));
	sysTime.wHour = atoi(m_strSetTime.Mid(8, 2));
	sysTime.wMinute = atoi(m_strSetTime.Mid(10, 2));
	sysTime.wSecond = atoi(m_strSetTime.Mid(12, 2));

	// 사용프로잭트속성.구성속성.링커.매니페스트파일(asInvoker->highestAvailable)
//	BOOL bOk = SetLocalTime(&sysTime);

	Set_S2F32();
	g_objHandler.Set_TimeSync();

	strLog.Format("[Get_S2F31] Time Set => Time [%s]", m_strSetTime);
	g_objLogFile.Save_HostLog(strLog);
}

void CHost::Get_S10F3()
{
	g_objHandler.Set_HostMsg(m_sHostMsg);
	Set_S10F4();
}

void CHost::Get_S2F49_LotStart()
{
	Set_S2F50_LotStart();
	g_objHandler.Set_LotStart();
}

void CHost::Get_S2F49_LotCancel()
{
	Set_S2F50_LotCancel();
	g_objHandler.Set_LotCancel();
}

void CHost::Get_S2F49_ProductData()
{
	Set_S2F50_ProcuctData();
	g_objHandler.Set_CmResult();
}

void CHost::Get_S2F49_Module_Fail()
{
	Set_S2F50_Module_Fail();
	g_objHandler.Set_ModuleFail();
}

void CHost::Get_S2F49_NGLotStart()
{
	Set_S2F50_NGLotStart();
	g_objHandler.Set_NGLotStart();
}

void CHost::Get_S2F49_NGLotFail()
{
	Set_S2F50_NGLotCancel();
	g_objHandler.Set_NGLotCancel();
}

/*
void CHost::Get_S2F49_LotInfo()
{

}

void CHost::Get_S2F49_CarrierCancel()
{
	Set_S2F50_CarrierCancel();
	g_objHandler.Set_CarrierCancel();
}

void CHost::Get_S2F49_MGZCancel()
{
	Set_S2F50_MGZCancel();

	g_objHandler.Set_MGZCancel();
}

void CHost::Get_S2F49_MGZConfirm()
{
	Set_S2F50_MGZConfirm();
	g_objHandler.Set_MGZConfirm();
}

void CHost::Get_S2F49_CarrierConfirm()
{
	Set_S2F50_CarrierConfirm();
	g_objHandler.Set_CarrierInfo();
	g_objHandler.Set_CarrierConfirm();
}
*/
///////////////////////////////////////////////////////////////////////////////
// Set Command

void CHost::Set_S1F1()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S1F1\" NAME=\"Are You There Request\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S1F1");
}

void CHost::Set_S1F4_State()
{
	CString strControl, strEqiup, strVersion;

	strControl = g_objHandler.Is_Connected() ? "1" : "2";
	strEqiup.Format("%d", gData.nCurEquipState );
	strVersion.Format("%s", MAIN_VERSION);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S1F4\" NAME=\"Selected Equipment Status Data\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <SVLIST COUNT = \"3\">" + CRLF;
	strSend += "      <SV NAME=\"ControlState\" VALUE=\"" + strControl + "\" />" + CRLF;
	strSend += "      <SV NAME=\"EquipmentState\" VALUE=\"" + strEqiup + "\" />" + CRLF;
	strSend += "      <SV NAME=\"SWVersion\" VALUE=\"" + strVersion + "\" />" + CRLF;
	strSend += "    </SVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S1F4");
}

void CHost::Set_S1F4()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S1F4\" NAME=\"Selected Equipment Status Data\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <SVLIST COUNT=\"1\">" + CRLF;
	strSend += "      <SV NAME=\"SV\" VALUE=\"" + gData.sCurrentRecipe + "\" />" + CRLF;
	strSend += "    </SVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S1F4");
}

void CHost::Set_S7F20()
{
	CString strCount;
	strCount.Format("%d", gData.nRcpCount);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S7F20\" NAME=\"Delete Process Program Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <PPIDLIST COUNT=\"" + strCount + "\" >" + CRLF;
	
	for (int i = 0; i < gData.nRcpCount; i++) {
	strSend += "      <PPID VALUE=\"" + gData.sRecipList[i] + "\" />" + CRLF;
	}
	
	strSend += "    </PPIDLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S7F20");
}

void CHost::Set_S2F4()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F4\" NAME=\"Link Test Response\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F4");
}

void CHost::Set_S2F32()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F32\" NAME=\"Date and Time Set Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <ACKC NAME=\"ACKC\" VALUE=\"" + m_strSetTime +"\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F32");
}

void CHost::Set_S10F4()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S10F4\" NAME=\"Terminal Display,Single Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <ACKC NAME=\"ACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S10F4");
}

// S2F49에 대한 응답으로 S2F50 송신
void CHost::Set_S2F50_LotStart()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"LOT_START\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "START");
}

void CHost::Set_S2F50_LotCancel()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"LOT_ID_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "CANCEL");
}

void CHost::Set_S2F50_ProcuctData()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"PRODUCT_DATA\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "PRODUCT_DATA");
}

void CHost::Set_S2F50_Module_Fail()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"PRODUCT_ID_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "PRODUCT_ID_FAIL");
}

void CHost::Set_S2F50_NGLotStart()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"NG_LOT_START\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "NG_LOT_START");
}

void CHost::Set_S2F50_NGLotCancel()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"NG_LOT_ID_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "NG_LOT_ID_FAIL");
}

void CHost::Set_S6F11_ControlState(int nState)
{
	CString strState;
	strState.Format("%d", nState);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"10101\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"10101\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"4\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"CONTROLSTATE\" VALUE=\"" + strState + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TEXT\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "10101");

	if (nState == 1) g_objHandler.Set_ControlState(1);	// 1:Online, 2:Offline
	m_bHostOnline = (nState == 1 ? TRUE : FALSE);
}
// 
// void CHost::Set_S6F11_EquipState(int nState, int nErrNo)
// {
// 	CString	strState, strErrNo, strOldState;
// 	strState.Format("%d", nState);
// 	strErrNo.Format("%d", nErrNo);
// 	if (nState != 6 || nErrNo < 1) { strErrNo = gData.sAlarmTxt = ""; }
// 
// 	gData.nPreEquipState = gData.nPreEquipState == 0 ? 1 : gData.nCurEquipState;
// 	gData.nCurEquipState = nState;
// 
// 	strOldState.Format("%d", gData.nPreEquipState);
// // 	strOldState = ((nState == 2 || nState == 6) ? "5" : "6");
// 
// 	SYSTEMTIME time;
// 	GetLocalTime(&time);
// 
// 	CString strTime;
// 	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
// 
// 	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;
// 
// 	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
// 	strSend += "  <ELEMENT>" + CRLF;
// 	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
// 	strSend += "  </ELEMENT>" + CRLF;
// 	strSend += "  <ITEM>" + CRLF;
// 	strSend += "    <CEID NAME=\"CEID\" VALUE=\"10102\" />" + CRLF;
// 	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"10102\" />" + CRLF;
// 	strSend += "    <DVLIST COUNT=\"7\">" + CRLF;
// 	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
// 	strSend += "      <DV NAME=\"PREVEQPSTATE\" VALUE=\"" + strOldState + "\" />" + CRLF;
// 	strSend += "      <DV NAME=\"CUREQPSTATE\" VALUE=\"" + strState + "\" />" + CRLF;
// 	strSend += "      <DV NAME=\"ALARMID\" VALUE=\"" + strErrNo + "\" />" + CRLF;
// 	strSend += "      <DV NAME=\"ALARMCODE\" VALUE=\"" + strErrNo + "\" />" + CRLF;
// 	strSend += "      <DV NAME=\"ALARMTEXT\" VALUE=\"" + gData.sAlarmTxt + "\" />" + CRLF;
// 	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
// 	strSend += "    </DVLIST>" + CRLF;
// 	strSend += "  </ITEM>" + CRLF;
// 	strSend += "</EIF>";
// 
// 	Send_Command(strSend, FALSE, "S6F11", "10102");
// }


void CHost::Set_S6F11_EquipState(int nState, int nErrNo)
{
	CString	strState, strErrNo, strOldState;
	strState.Format("%d", nState);
	strErrNo.Format("%d", nErrNo);
	if (nState != 6 || nErrNo < 1) { strErrNo = gData.sAlarmTxt = ""; }

	gData.nPreEquipState = gData.nPreEquipState == 0 ? 1 : gData.nCurEquipState;
	gData.nCurEquipState = nState;

	strOldState.Format("%d", gData.nPreEquipState);
	// 	strOldState = ((nState == 2 || nState == 6) ? "5" : "6");

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"10108\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"10108\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"8\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PREVNEWEQPSTATE\" VALUE=\"" + strOldState + "\" />" + CRLF;
	strSend += "      <DV NAME=\"CURRNEWEQPSTATE\" VALUE=\"" + strState + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ALARMLISTQTY\" VALUE=\"1\" />" + CRLF;
	strSend += "      <DV NAME=\"ALARMID#1\" VALUE=\"" + strErrNo + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ALARMCATEGORY#1\" VALUE=\"33\" />" + CRLF;
	strSend += "      <DV NAME=\"ALARMTEXT#1\" VALUE=\"" + gData.sAlarmTxt + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "10108");
}

void CHost::Set_S5F1_Alarm(int nSet, int nErrNo)
{
	CString sAlCD, sErrNo;
	if (nSet == 0)	sAlCD = "1";	//Reset
	else			sAlCD = "129";	//Set
	sErrNo.Format("%04d", nErrNo);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S5F1\" NAME=\"Alarm Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <ALCD NAME=\"ALCD\" VALUE=\"" + sAlCD + "\" />" + CRLF;
	strSend += "    <ALID NAME=\"ALID\" VALUE=\"" + sErrNo + "\" />" + CRLF;
	strSend += "    <ALTX NAME=\"ALTX\" VALUE=\"" + gData.sAlarmTxt + "\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S5F1");
}

void CHost::Set_S6F11_LotReport(CString sLotId, CString sRecipeId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20106\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20106\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"6\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTTYPE\" VALUE=\"T\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + sRecipeId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20106");
}

void CHost::Set_S6F11_LotStart(CString sLotId, CString sRecipeId, int nCount)
{
	CString strCount;
	strCount.Format("%d", nCount);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	Get_LotInfor(sLotId);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20101\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20101\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"8\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + sRecipeId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOTALQTY\" VALUE=\"" + strCount + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20101");
}

void CHost::Set_S6F11_CmRequest(CString sLotId, CString sCmId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	Get_LotInfor(sLotId);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20403\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20403\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"7\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
//	strSend += "      <DV NAME=\"PRODUCTID\" VALUE=\"" + sCmId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MODULEID\" VALUE=\"" + sCmId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20403");
}

void CHost::Set_S6F11_CmEnd(CString sLotId, CString sCmId, CString sResult, CString sNgCode, int nNgPocket, CString sROSResult)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sPortID, sNGOut, sNull, sCEID;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	if (sResult == "OK") { sPortID = "201"; sNGOut = "0"; }
	else				 { sPortID = "301"; sNGOut.Format("%d", nNgPocket); }
//	if (sResult == "MOK") { sCEID = "20405"; sLotId = gMes.sHostNGLotId; }
//	else				  { sCEID = "20401"; }
	sCEID = "20401";
	Get_LotInfor(sLotId);	sNull = "";

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"" + sCEID + "\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"" + sCEID + "\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"21\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sNull + "\" />" + CRLF;
	strSend += "      <DV NAME=\"FROMMGZ\" VALUE=\"" + sNull + "\" />" + CRLF;
	strSend += "      <DV NAME=\"FROMTRAY\" VALUE=\"" + sNull + "\" />" + CRLF;
	strSend += "      <DV NAME=\"FROMPOCKET\" VALUE=\"" + sNull + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOMGZ\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"TOTRAY\" VALUE=\"" + sNull + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOPOCKET\" VALUE=\"" + sNull + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MODULEID\" VALUE=\"" + sCmId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RESULT\" VALUE=\"" + sResult + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGCODE\" VALUE=\"" + sNgCode + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGPOCKETID\" VALUE=\"" + sNGOut + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCESSDATAQTY\" VALUE=\"1\" />" + CRLF;
	strSend += "      <DV NAME=\"NAMEAPD1\" VALUE=\"ROS_JUDGE\" />" + CRLF;
	strSend += "      <DV NAME=\"VALUEAPD1\" VALUE=\"" + sROSResult + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", sCEID);
}

void CHost::Set_S6F11_LotEnd(CString sLotId,CString sRecipeId, int nHCount, int nOk, int nNg)
{
	CString strCount, strHCount, strOk, strNg;
	strOk.Format("%d", nOk);
	strNg.Format("%d", nNg);
	strHCount.Format("%d", nHCount);
	strCount.Format("%d", nOk + nNg);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	Get_LotInfor(sLotId);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20102\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20102\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"12\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + sRecipeId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOTALQTY\" VALUE=\"" + strHCount + "\" />" + CRLF;
	strSend += "      <DV NAME=\"INQTY\" VALUE=\"" + strCount + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OUTQTY\" VALUE=\"" + strOk + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGQTY\" VALUE=\"" + strNg + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ENDMODE\" VALUE=\"A\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20102");
	Set_DelInfor(sLotId);
}

void CHost::Set_S6F11_LotAbort(CString sLotId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20104\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20104\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"3\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20104");
	Set_DelInfor(sLotId);
}

void CHost::Set_S6F11_IdleReportSet(BOOL bSet)
{
	CString strCEID;

	int nCEID = bSet ? 50102 : 50103;
	strCEID.Format("%d", nCEID);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"" + strCEID + "\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"" + strCEID + "\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"3\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"REASONCODE\" VALUE=\"" + gIdle.sCode + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", strCEID);
}


void CHost::Set_S6F11_AccessModeChanged(CString sMode)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"10109\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"10109\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"3\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"EQUIPMENTACCESSMODE\" VALUE=\"" + sMode + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "10109");
}

void CHost::Set_S6F11_Terminal()
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"90101\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"90101\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"3\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TERMINALMSGACK\" VALUE=\"0\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "90101");
}

void CHost::Set_S6F11_NGLotRequest()
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20108\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20108\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"6\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTTYPE\" VALUE=\"T\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + gMes.sHostLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + gMes.sHostRecipe + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20108");
}

void CHost::Set_S6F11_NGLotEnd(CString sNGLotId, int nMOk, int nNg)
{
	CString strCount, strMOk, strNg;
	strMOk.Format("%d", nMOk);
	strNg.Format("%d", nNg);
	strCount.Format("%d", nMOk + nNg);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20109\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20109\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"12\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + gMes.sHostNGLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostNGProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostNGModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + gMes.sHostNGRecipe + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOTALQTY\" VALUE=\"" + strCount + "\" />" + CRLF;
	strSend += "      <DV NAME=\"INQTY\" VALUE=\"" + strCount + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OUTQTY\" VALUE=\"" + strMOk + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGQTY\" VALUE=\"" + strNg + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ENDMODE\" VALUE=\"A\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20109");
}

/*
void CHost::Set_S2F50_CarrierConfirm()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, strTotal;
	strTotal.Format("%d", gMes.nHostCmCount[gMes.nReportLoadMGZNo-1]);
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"START\" />" + CRLF;
	strSend += "      <CPLIST COUNT=\"6\">" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TIME\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"LOTID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"MGZID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"RECIPEID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TOTALQTY\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\""+ strTotal + "\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"OPERATORID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "      </CPLIST>" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "TRAY_ID_CONFIRM");
}

void CHost::Set_S2F50_CarrierCancel()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"CANCEL\" />" + CRLF;
	strSend += "      <CPLIST COUNT=\"4\">" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TIME\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"CANCELTYPE\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"ID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"OPERATORID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "      </CPLIST>" + CRLF;
	strSend += "      <RESULT>" + CRLF;
	strSend += "        <CODE NAME=\"CODE\" VALUE=\"" + gMes.sCancelCode + "\" />" + CRLF;
	strSend += "        <TEXT VALUE=\"" + gMes.sCancelText + "\" />" + CRLF;
	strSend += "      </RESULT>" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "TRAY_CANCEL");
}

void CHost::Set_S2F50_MGZConfirm()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, strTotal;
	strTotal.Format("%d", gMes.nHostCmCount[gMes.nReportLoadMGZNo-1]);
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"START\" />" + CRLF;
	strSend += "      <CPLIST COUNT=\"6\">" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TIME\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"LOTID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"MGZID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"RECIPEID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TOTALQTY\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\""+ strTotal + "\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"OPERATORID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "      </CPLIST>" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "MGZ_CONFIRM");
}

void CHost::Set_S2F50_MGZCancel()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"CANCEL\" />" + CRLF;
	strSend += "      <CPLIST COUNT=\"4\">" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TIME\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"CANCELTYPE\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"ID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "        <CP>" + CRLF;
	strSend += "          <CPNAME NAME=\"CPNAME\" VALUE=\"OPERATORID\" />" + CRLF;
	strSend += "          <CPACKC NAME=\"CPACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "        </CP>" + CRLF;
	strSend += "      </CPLIST>" + CRLF;
	strSend += "      <RESULT>" + CRLF;
	strSend += "        <CODE NAME=\"CODE\" VALUE=\"" + gMes.sCancelCode + "\" />" + CRLF;
	strSend += "        <TEXT VALUE=\"" + gMes.sCancelText + "\" />" + CRLF;
	strSend += "      </RESULT>" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "MGZ_CANCEL");
}

void CHost::Set_S5F1_ErrorUpdate(int nFlag, CString sErrNo, CString sErrMsg)
{
// 	int	 nAlCD = (nFlag == 1 ? 129 : 1);
	int	 nAlCD = (nFlag == 1 ? 161 : 33);
	BYTE cAlCD = (nFlag == 1 ? 128 : 48);

	CString strAlCD;
	strAlCD.Format("%d", nAlCD);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S5F1\" NAME=\"Alarm Report Send\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
    strSend += "  <ITEM>" + CRLF;
	strSend += "    <ALCD VALUE=\"" + strAlCD + "\" />" + CRLF;
	strSend += "    <ALID VALUE=\"" + sErrNo + "\" />" + CRLF;
	strSend += "    <ALTX VALUE=\"" + sErrMsg + "\" />" + CRLF;
    strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S5F1");
}

void CHost::Set_S6F11_CarrierLoadReport(CString sMGZID, CString sMGZSlotID, CString sCarrierID)
{
	CString strCount;
// 	strCount.Format("%d", nCount);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20310\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20310\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"5\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
//	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"SLOTID\" VALUE=\"" + sMGZSlotID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20310");
}

void CHost::Set_S6F11_CarrierStart(CString sLotID, CString sPortID, CString sMGZID, CString sMGZSlotID, CString sCarrierID)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20302\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20302\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"7\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
//	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"SLOTID\" VALUE=\"" + sMGZSlotID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20302");
}

void CHost::Set_S6F11_CarrierIDReport(CString sLotID, CString sPortID, CString sMGZID, CString sCarrierID)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20301\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20301\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"7\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
//	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20301");
}

void CHost::Set_S6F11_MGZIDReport(CString sPortID, CString sPortType, CString sMGZId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20203\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20203\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"6\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTTYPE\" VALUE=\"" + sPortType + "\" />" + CRLF;
// 	strSend += "      <DV NAME=\"PORTTYPE\" VALUE=\"\" />" + CRLF;
//	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20203");
}

void CHost::Set_S6F11_CarrierEnd(CString sCarrierID, CString sMGZNo, CString sCarrierNo, CString sUnloadSlotNo,  CString sTotal, CString sGood, CString sNg, CString sLastModuleNo, CString sDataQty)
{
	int nLastModuleNo = atoi(sLastModuleNo);
	int nTotal = atoi(sTotal);
	int nMGZIdx = atoi(sMGZNo) - 1;
	int	nCarrierIdx = atoi(sCarrierNo) - 1;
//	int	nPortId = atoi(sMGZNo) + 3;
	int	nPortId = atoi(sMGZNo);

	CString strInfo, strNo, strDvListCount, strTime, sPortId;

	sPortId.Format("%d", nPortId);
	strDvListCount.Format("%d", nLastModuleNo * 4 + 13);

	SYSTEMTIME time;
	GetLocalTime(&time);

	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20303\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20303\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"" + strDvListCount + "\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + gData.sHandlerLotID[nMGZIdx] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortId + "\" />" + CRLF;
//	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + gData.sHandlerMGZID[nPortId-1] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"SLOTID\" VALUE=\"" + sUnloadSlotNo + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOTALQTY\" VALUE=\"" + sTotal + "\" />" + CRLF;
	strSend += "      <DV NAME=\"INQTY\" VALUE=\"" + sTotal + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OUTQTY\" VALUE=\"" + sGood + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGQTY\" VALUE=\"" + sNg + "\" />" + CRLF;
	strSend += "      <DV NAME=\"JUDGE\" VALUE=\"\" />" + CRLF;
	strSend += "      <DV NAME=\"DATAQTY\" VALUE=\"" + sDataQty + "\" />" + CRLF;
	for (int i = 0; i < nLastModuleNo; i++) {
		if (i >= 56) break;
		strNo.Format("%d", i + 1);
		strSend += "      <DV NAME=\"POKETID" + strNo + "\" VALUE=\"" +strNo + "\" />" + CRLF;
		strSend += "      <DV NAME=\"PRODUCTID" + strNo +"\" VALUE=\"" + gData.sBarcode[nMGZIdx][nCarrierIdx][i] + "\" />" + CRLF;
		strSend += "      <DV NAME=\"RESULT" + strNo + "\" VALUE=\"" + gData.sJudge[nMGZIdx][nCarrierIdx][i] + "\" />" + CRLF;
		strSend += "      <DV NAME=\"NGCODE" + strNo + "\" VALUE=\"" + gData.sNgCode[nMGZIdx][nCarrierIdx][i] + "\" />" + CRLF;
	}

	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20303");
}

void CHost::Set_S6F11_CarrierUnloadReport(CString sMGZID, CString sMGZSlotID, CString sCarrierID)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20311\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20311\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"5\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
//	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"SLOTID\" VALUE=\"" + sMGZSlotID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID=\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20311");
}

void CHost::Set_S6F11_CancelReport(CString sID, CString sCancelMode)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20103\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20103\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"6\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"CANCELTYPE\" VALUE=\"L\" />" + CRLF;
	strSend += "      <DV NAME=\"ID\" VALUE=\"" + sID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"CANCELMODE\" VALUE=\"" + sCancelMode + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20103");
}

void CHost::Set_S6F11_LotInfo(CString sLotId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20405\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20405\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"4\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"LM30650\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MODULEID\" VALUE=\"\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20405");
}

void CHost::Set_S2F61_IdleRequst()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F61\" NAME=\"Idle Reason Code Request\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S2F61");
}

void CHost::Set_S6F11_IdleReport()
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"50102\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"50102\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"4\">" + CRLF;
	strSend += "      <DV NAME=\"STARTTIME\" VALUE=\"" + gIdle.sStartTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ENDTIME\" VALUE=\"" + gIdle.sEndTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"REASONCODE\" VALUE=\"" + gIdle.sCode + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "50102");
}

void CHost::Set_S9F13()	// Conversation Timeout
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S9F13\" NAME=\"ConversationTimeout\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S9F13");
}
*/
///////////////////////////////////////////////////////////////////////////////

void CHost::Reply_HeartBeat()
{
	CString strLog, strMsg, strSendSocket;

	strSendSocket.Format("%c%08d%04d0%c", STX, 0, m_nRecvCmdCount, ETX);

	char chSend[16] = { 0 };	// 마지막 0x00
	int nLength = strSendSocket.GetLength();
	memcpy(chSend, (LPSTR)(LPCSTR)strSendSocket, nLength);

	if (!m_Server.Write_Socket(m_nClientIdx, (BYTE*)chSend, nLength)) return;
/*
	// Host Log //////////////////////////////////////////////////////////////////
	strLog.Format("[->] %08d%04d0", 0, m_nRecvCmdCount);
	g_objLogFile.Save_HostLog(strLog);

	strMsg.Format("%s : [HeartBeat]", strLog);
	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HostMsg(strMsg);
*/
}

void CHost::Send_Command(CString sSend, BOOL bReply, CString sStFn, CString sRcmd)
{
	CString strLog, strMsg, strSendSocket, strTemp;

	int nLen = sSend.GetLength();

	if (!bReply) m_nSendCmdCount < 9999 ? m_nSendCmdCount++ : m_nSendCmdCount = 1;
	int nCount = (bReply ? m_nRecvCmdCount : m_nSendCmdCount);

	strSendSocket.Format("%c%08d%04d1%s%c", STX, nLen, nCount, sSend, ETX);

	char chSend[2000] = { 0 };	// Max 2000
	int nLength = strSendSocket.GetLength();

	if (nLength > 2000) {
		int nSendCount = strSendSocket.GetLength() / 2000 + 1;
		for (int i = 0; i < nSendCount; i++) {
			strTemp = strSendSocket.Mid(i * 2000, 2000);
			memset(chSend, 0x00, sizeof(char) * 2000);
			memcpy(chSend, strTemp, strTemp.GetLength());
			int nLenTemp = strTemp.GetLength();
			if (!m_Server.Write_Socket(m_nClientIdx, (BYTE*)chSend, nLenTemp)) return;
		}

	} else {
		memcpy(chSend, (LPSTR)(LPCSTR)strSendSocket, nLength);
		if (!m_Server.Write_Socket(m_nClientIdx, (BYTE*)chSend, nLength)) return;
	}

	// Host Log //////////////////////////////////////////////////////////////////
	strLog.Format("[->] %08d%04d1%s", nLen, nCount, sSend);
	g_objLogFile.Save_HostLog(strLog);

	strMsg.Format("%s : %s,%s", strLog.Left(18), sStFn, sRcmd);
	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HostMsg(strMsg);
}

///////////////////////////////////////////////////////////////////////////////

void CHost::Test_Send()
{
// 	CString strXml = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;
// 
// 	strXml += "<EIF VERSION=\"2.0\" ID=\"S2F49\" NAME=\"Enhanced Remote Command\">" + CRLF;
// 	strXml += "  <ELEMENT>" + CRLF;
// 	strXml += "    <EQPID VALUE=\"LM1CAV0110\" />" + CRLF;
// 	strXml += "  </ELEMENT>" + CRLF;
// 	strXml += "  <ITEM>" + CRLF;
// 	strXml += "    <RCMDCP>" + CRLF;
// 	strXml += "      <RCMD NAME=\"RCMD\" VALUE=\"START\" />" + CRLF;
// 	strXml += "      <CPLIST COUNT=\"6\">" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TIME\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"20180410101158\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"MODELID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"LOTID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"GPEZ612BJ4729A\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"RECIPEID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"NA\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TOTALQTY\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"1055\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"OPERATORID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"74596\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "      </CPLIST>" + CRLF;
// 	strXml += "      <RESULT>" + CRLF;
// 	strXml += "        <CODE NAME=\"CODE\" VALUE=\"\" />" + CRLF;
// 	strXml += "        <TEXT VALUE=\"\" />" + CRLF;
// 	strXml += "      </RESULT>" + CRLF;
// 	strXml += "    </RCMDCP>" + CRLF;
// 	strXml += "  </ITEM>" + CRLF;
// 	strXml += "</EIF>";
// 
// 	CString strHead, strSend;
// 
// 	int nLen = strXml.GetLength();
// 	int nCount = 30;
// 	int nType = 1;
// 	strHead.Format("%08d%04d%d", nLen, nCount, nType);
// 
// 	strSend.Format("%c%s%s%c", STX, strHead, strXml, ETX);
// 	Test_Receive(strSend);
// 	int aaa = 100;
}

int CHost::Test_Receive(CString strRecvSocket)
{
// 	CString strLog, strMsg;
// 	m_strRecvCmd += strRecvSocket;
// 
// 	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
// 	while (!m_strRecvCmd.IsEmpty()) {
// 		int nStart = m_strRecvCmd.Find(STX);
// 		int nEnd = m_strRecvCmd.Find(ETX);
// 
// 		if (nEnd < 0) break;	// 버퍼에 들어오는 중...
// 
// 		if (nStart < 0 || nStart > nEnd) {
// 			strMsg.Format("[OnServerReceive] - Start(%d), End(%d).", nStart, nEnd);
// 			pMainDlg->Set_HostMsg(strMsg);
// 
// 			strLog.Format("%s\n%s", strMsg, m_strRecvCmd);
// 			g_objLogFile.Save_HostLog(strLog);
// 
// 			m_strRecvCmd.Delete(0, nEnd + 1);	// 쓰레기값이 채워져 있어서...
// 			continue;
// 		}
// 
// 		m_dwLastRecvTime = GetTickCount();	// 시간 갱신
// 
// 		CString strRecv = m_strRecvCmd.Mid(nStart + 1, nEnd - nStart - 1);
// 		m_strRecvCmd.Delete(0, nEnd + 1);
// 
// 		// Host Log /////////////////////////////////////////////////////////////////
// 		strLog.Format("[<-] %s", strRecv);
// 		g_objLogFile.Save_HostLog(strLog);
// 
// 		pMainDlg->Set_HostMsg(strLog.Left(18));
// 
// 		m_nRecvCmdCount = atoi(strRecv.Mid(8, 4));	// 4Byte
// 
// 		if (strRecv.GetAt(12) == '0') { Send_Command("", FALSE, 0); return 0; }	// Heart Beat
// 
// 		CString strRecvXml = strRecv.Right(strRecv.GetLength() - 13);
// 
// 		if (!Extract_Xml(strRecvXml)) return 0;
// 
// 		if (m_strStFn == "S1F1")       Get_S1F1();	// NAME="Are You There Request"
// 		else if (m_strStFn == "S2F3")  Get_S2F3();	// NAME="Link Test Request"
// 		else if (m_strStFn == "S2F31") Get_S2F31(); // NAME="Date and Time Set Request"
// 		else if (m_strStFn == "S2F49" && m_strRcmd == "START")         Get_S2F49_Start();	// NAME="Enhanced Remote Command"
// 		else if (m_strStFn == "S2F49" && m_strRcmd == "CANCEL")        Get_S2F49_Cancel();
// 		else if (m_strStFn == "S2F49" && m_strRcmd == "MODULE_RESULT") Get_S2F49_CmResult();
// 	}
	return 0;
}

void CHost::Test_WriteLog()
{
// 	Get_S1F1();				// 2.
// 	Get_S2F3();				// 3.
// 	Get_S2F31();			// 4.
// 	Get_S2F49_Start();		// 5.
// 	Get_S2F49_Cancel();		// 6.
// 	Get_S2F49_CmResult();	// 7.
// 	Set_S1F1();				// 8.
// 	Set_S5F1_ErrorUpdate(1, "2111", "Test Error Message");	// 9.
// 	Set_S6F11_ControlState(1);								// 10.
// 	Set_S6F11_EquipState(2, 0);								// 11.
// 	Set_S6F11_LotReady("LOT_ID_SAMPLE");	// 12.
// 	Set_S6F11_LotStart("LOT_ID_SAMPLE", 1000);	// 13.
// 	Set_S6F11_LotAbort("LOT_ID_SAMPLE");	// 15.
// 	Set_S6F11_CmRequest("LOT_ID_SAMPLE", "CM_ID_SAMPLE");	// 16.
// 	Set_S6F11_CmEnd("LOT_ID_SAMPLE", "CM_ID_SAMPLE", "OK", "NG_CODE1", 3,"00", 1.0, 2.0, 3.0);	// 17.
// 	Set_S2F61_IdleRequst();	// 18.
// 	Set_S6F11_IdleReport();	// 19.
// 	Set_S9F13();	// 20.
}

void CHost::Set_AddInfor(CString sLotId, CString sProcID, CString sProdID)
{
	for(int i=0; i<10; i++) {
		if(sLotId = gMes.sAHostLotId[i]) {
			gMes.sAHostLotId[i] = sLotId;
			gMes.sAHostProcID[i] = sProcID;
			gMes.sAHostProdID[i] = sProdID;
			return;
		}
	}

	gMes.sAHostLotId[gMes.nAHostCount] = sLotId;
	gMes.sAHostProcID[gMes.nAHostCount] = sProcID;
	gMes.sAHostProdID[gMes.nAHostCount] = sProdID;
	gMes.nAHostCount++;
	if (gMes.nAHostCount >= 10) gMes.nAHostCount = 0;
}

void CHost::Set_DelInfor(CString sLotId)
{
	for(int i=0; i<10; i++) {
		if(sLotId == gMes.sAHostLotId[i]) {
			gMes.sAHostLotId[i] = "";
			gMes.sAHostProcID[i] = "";
			gMes.sAHostProdID[i] = "";
			return;
		}
	}
}

void CHost::Get_LotInfor(CString sLotId)
{
	for(int i=0; i<10; i++) {
		if(sLotId == gMes.sAHostLotId[i]) {
			gMes.sHostProcID = gMes.sAHostProcID[i];
			gMes.sHostModel  = gMes.sAHostProdID[i];
			return;
		}
	}
}