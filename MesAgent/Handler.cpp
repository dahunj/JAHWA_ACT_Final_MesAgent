// Handler.cpp : 구현 파일입니다.
//
#include "stdafx.h"
#include "MesAgent.h"
#include "Handler.h"

#include "Common.h"
#include "LogFile.h"
#include "MesAgentDlg.h"
#include "Host.h"

IMPLEMENT_DYNAMIC(CHandler, CWnd)

CHandler g_objHandler;

// CHandler

CHandler::CHandler()
{
	m_bConnected = FALSE;
	m_strRecvCmd = "";
}

CHandler::~CHandler()
{
}

BEGIN_MESSAGE_MAP(CHandler, CWnd)
	ON_MESSAGE(UM_SERVER_ACCEPT, OnServerAccept)
	ON_MESSAGE(UM_SERVER_REMOVE, OnServerRemove)
	ON_MESSAGE(UM_SERVER_RECEIVE, OnServerReceive)
END_MESSAGE_MAP()

// CHandler 메시지 처리기입니다.

void CHandler::Initialize()
{
	m_bConnected = FALSE;
	m_nClientIdx = 0;
	m_Server.Listen_Socket(HANDLER_PORT, this);
}

void CHandler::Terminate()
{
	m_bConnected = FALSE;
	m_Server.Close_Socket();
	if (g_objHost.Is_Connected()) g_objHost.Set_S6F11_ControlState(2);	//1:Online, 2:Offline
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CHandler::OnServerAccept(WPARAM wClientIdx, LPARAM lServerPort)
{
	int nClient = (int)wClientIdx;
	int nServerPort = (int)lServerPort;

	CString strIP = "";
	UINT nPort = 0;
	if (!m_Server.Get_ClientInfo(nClient, strIP, nPort)) return 0;
	m_nClientIdx = nClient;

	CString strLog = "Handler Connected.";
	g_objLogFile.Save_HandlerLog(strLog);

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HandlerConnect(TRUE);

	m_bConnected = TRUE;

	if (g_objHost.Is_Connected()) g_objHost.Set_S6F11_ControlState(1);	//1:Online, 2:Offline

	return 0;
}

LRESULT CHandler::OnServerRemove(WPARAM wClientIdx, LPARAM lServerPort)
{
	m_bConnected = FALSE;

	CString strLog = "Handler Disconnected.";
	g_objLogFile.Save_HandlerLog(strLog);

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HandlerConnect(FALSE);

	if (g_objHost.Is_Connected()) g_objHost.Set_S6F11_ControlState(2);	//1:Online, 2:Offline

	return 0;
}

LRESULT CHandler::OnServerReceive(WPARAM wClientIdx, LPARAM lServerPort)
{
	int nClient = (int)wClientIdx;
	int nServerPort = (int)lServerPort;

	CString strIP = "";
	UINT nPort = 0;
	if (!m_Server.Get_ClientInfo(nClient, strIP, nPort)) return 0;

	BYTE byRecv[1025] = { 0 };	// Buffer 1024 -> Last 0x00
	int nLen = m_Server.Read_Socket(nClient, byRecv);

	CString strRecvSocket, strLog;
	strRecvSocket.Format("%s", byRecv);
	m_strRecvCmd += strRecvSocket;

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	while (!m_strRecvCmd.IsEmpty()) {
		int nStart = m_strRecvCmd.Find("@");
		int nEnd = m_strRecvCmd.Find("\n");

		if (nEnd < 0) break;	// 버퍼에 들어오는 중...

		if (nStart < 0 || nStart > nEnd) {
			strLog.Format("[OnServerReceive] <<Error>> - Start(%d), End(%d).\n%s", nStart, nEnd, m_strRecvCmd);
			g_objLogFile.Save_HandlerLog(strLog);
			m_strRecvCmd.Delete(0, nEnd + 1);	// 쓰레기값이 채워져 있어서...
			continue;
		}

		CString strRecv = m_strRecvCmd.Mid(nStart + 1, nEnd - nStart - 1);
		m_strRecvCmd.Delete(0, nEnd + 1);

		// Handler Log //////////////////////////////////////////////////////////////
		strLog.Format("[<-] %s", strRecv);
		g_objLogFile.Save_HandlerLog(strLog);

		pMainDlg->Set_HandlerMsg(strLog);

		char chSep = ',';
		CString strCmd, strOp;

		AfxExtractSubString(strCmd, strRecv, 0, chSep);
		AfxExtractSubString(strOp, strRecv, 1, chSep);

		CString strArg[10];
		for (int i = 0; i < 10; i++) AfxExtractSubString(strArg[i], strRecv, i + 2, chSep);

		if (strCmd == "OPER") {
			if (strOp == "UPDATE") Get_OperUpdate(strArg[0]);

		} else if (strCmd == "EQUIP") {
			if (strOp == "STATE") Get_EquipState(strArg[0]);

		} else if (strCmd == "ERROR") {
			if (strOp == "UPDATE") Get_ErrorUpdate(strArg[0], strArg[1]);

		} else if (strCmd == "CONTROL") {
			if (strOp == "STATE") Get_ControlState(strArg[0], strArg[1]);

		} else if (strCmd == "LOT") {
			if (strOp == "START")	Get_LotStart(strArg[0], strArg[1], strArg[2], strArg[3]);
			if (strOp == "ABORT")	Get_LotAbort(strArg[0]);
			if (strOp == "END")		Get_LotEnd(strArg[0], strArg[1], strArg[2], strArg[3], strArg[4]);

//			if (strOp == "REPORT")	Get_LotIdReport(strArg[0], strArg[1], strArg[2]);
// 			if (strOp == "READY")	Get_LotReady(strArg[0], strArg[1]);
//			if (strOp == "CANCEL")	Get_Cancel(strArg[0], strArg[1]);

		} else if (strCmd == "CM") {
			if (strOp == "REQUEST")	Get_CmRequest(strArg[0], strArg[1]);
			if (strOp == "END")		Get_CmEnd(strArg[0], strArg[1], strArg[2], strArg[3], strArg[4], strArg[5]);

		} else if (strCmd == "RECIPE") {
			if (strOp == "REQUEST")	Get_RecipeList(strRecv);

		} else if (strCmd == "IDLE") {
// 			if (strOp == "REQUEST") Get_IdleRequest();
			if (strOp == "REPORT") 	Get_IdleReport(strArg[0], strArg[1], strArg[2], strArg[3], strArg[4]);

		} else if (strCmd == "TERMINAL") {
			if (strOp == "MSG") 	Get_TerminalOK();

		} else if (strCmd == "NGLOT") {
			if (strOp == "REQUEST")	Get_NGLotRequest();
			if (strOp == "END")		Get_NGLotEnd(strArg[0], strArg[1], strArg[2]);

//		} else if (strCmd == "MGZ") {
//			if (strOp == "ID")		Get_MGZIdReport(strArg[0], strArg[1], strArg[2]);
// 			if (strOp == "CANCEL")	Get_Cancel(strArg[0], strArg[1]);

//		} else if (strCmd == "CARRIER") {
//			if (strOp == "LOAD")	Get_CarrierLoad(strArg[0], strArg[1], strArg[2]);
//			if (strOp == "START")	Get_CarrierStart(strArg[0], strArg[1], strArg[2], strArg[3], strArg[4]);
//			if (strOp == "ID")		Get_CarrierIdReport(strArg[0], strArg[1], strArg[2], strArg[3],strArg[4], strArg[5]);
//			if (strOp == "END")		Get_CarrierEnd(strArg[0], strArg[1], strArg[2], strArg[3], strArg[4], strArg[5], strArg[6], strArg[7]);
//			if (strOp == "UNLOAD")	Get_CarrierUnload(strArg[0], strArg[1], strArg[2]);
// 			if (strOp == "CANCEL")	Get_Cancel(strArg[0], strArg[1]);
//			if (strOp == "INFO")	Get_CarrierInfo(strArg[0], strArg[1], strArg[2], strArg[3], strArg[4], strArg[5], strArg[6]);

		} 
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Get Command

void CHandler::Get_OperUpdate(CString sOperId)
{
	gData.sOperId = sOperId;
}

void CHandler::Get_EquipState(CString sState)
{
	int nState = atoi(sState);
	g_objHost.Set_S6F11_EquipState(nState, 0);
}

void CHandler::Get_ErrorUpdate(CString sFlag, CString sErrNo)
{
	int nFlag = atoi(sFlag);
	int nErrNo = atoi(sErrNo);
	CString strErrFile, strErrMsg;

	strErrFile.Format("%s\\%s", gsCurrentDir, gData.sErrFile);
	CIniFileCS INI(strErrFile);
	if (!INI.Check_File()) { AfxMessageBox(strErrFile + " File Not Found!!!"); return; }

	gData.sAlarmTxt = INI.Get_String("ERROR", sErrNo, "");

	if (nFlag == 1) {
		g_objHost.Set_S6F11_EquipState(3, nErrNo);	//Down
		g_objHost.Set_S5F1_Alarm(1, nErrNo);
	} else {
		g_objHost.Set_S5F1_Alarm(0, nErrNo);
		g_objHost.Set_S6F11_EquipState(1, 0);		//Run
	}
}

void CHandler::Get_ControlState(CString sFlag, CString sOperId)
{
	gData.sOperId = sOperId;
	int nState = atoi(sFlag);
	if (g_objHost.Is_Connected()) g_objHost.Set_S6F11_ControlState(nState);	// 1:Online, 2:Offline
	if (nState == 1)			  g_objHost.Set_S6F11_EquipState(2, 0);	// Idle
}

void CHandler::Get_LotStart(CString sType, CString sLotId, CString sRecipe, CString sCount)
{
	int nCount = atoi(sCount);
	int nType  = atoi(sType);

	if (nType == 0) g_objHost.Set_S6F11_LotReport(sLotId, sRecipe);
	if (nType == 1) g_objHost.Set_S6F11_LotStart(sLotId, sRecipe, nCount);
}

void CHandler::Get_LotEnd(CString sLotId, CString sRecipe, CString sHCount, CString sOk, CString sNg)
{
	int nOk = atoi(sOk);
	int nNg = atoi(sNg);
	int nHCount = atoi(sHCount);
	g_objHost.Set_S6F11_LotEnd(sLotId, sRecipe, nHCount, nOk, nNg);
}

void CHandler::Get_LotAbort(CString sLotId)
{
	g_objHost.Set_S6F11_LotAbort(sLotId);
}

void CHandler::Get_IdleReport(CString sOperId, CString sSTime, CString sETime, CString sCode, CString sType)
{
	gData.sOperId = sOperId;
// 	gIdle.nCount = nCount;	// CNS 요청으로 첫번째 1개만 전송
	gIdle.sStartTime = sSTime;
	gIdle.sEndTime = sETime;
	gIdle.sCode = sCode;
// 	gIdle.sText = sText;
	if (sType == "1") g_objHost.Set_S6F11_IdleReportSet(TRUE);	//Idle Start
	else			  g_objHost.Set_S6F11_IdleReportSet(FALSE);	//Idle End
}

void CHandler::Get_RecipeList(CString sRecipeData)
{
	char chSep = ',';
	CString sOption, sCount;

	AfxExtractSubString(sOption, sRecipeData, 2, chSep);
	AfxExtractSubString(sCount,  sRecipeData, 3, chSep);

	if (sOption == "1") {	//CurrentRecipe
		AfxExtractSubString(gData.sCurrentRecipe, sRecipeData, 4, chSep);
		g_objHost.Set_S1F4();
	} else {				//All Recipe
		gData.nRcpCount = atoi(sCount);
		for (int i = 0; i < 100; i++) {
			AfxExtractSubString(gData.sRecipList[i], sRecipeData, i + 4, chSep);
			if (i+1 >= gData.nRcpCount) break;
		}
		g_objHost.Set_S7F20();
	}
}

void CHandler::Get_CmRequest(CString sLotId, CString sCmId)
{
	g_objHost.Set_S6F11_CmRequest(sLotId, sCmId);
}

void CHandler::Get_CmEnd(CString sLotId, CString sCmId, CString sResult, CString sNgCode, CString sROSResult, CString sPocket)
{
	int nPocket = atoi(sPocket);
	g_objHost.Set_S6F11_CmEnd(sLotId, sCmId, sResult, sNgCode, nPocket, sROSResult);
}

void CHandler::Get_TerminalOK()
{
	g_objHost.Set_S6F11_Terminal();
}

void CHandler::Get_NGLotRequest()
{
	g_objHost.Set_S6F11_NGLotRequest();
}

void CHandler::Get_NGLotEnd(CString sLotId, CString sMOk, CString sNg)
{
	int nOk = atoi(sMOk);
	int nNg = atoi(sNg);
	g_objHost.Set_S6F11_NGLotEnd(sLotId, nOk, nNg);
}

/*
void CHandler::Get_LotReady(CString sLotId, CString sPortNo)
{
	int nPortIdx = atoi(sPortNo);

	g_objHost.Set_S6F11_EquipState(4, 0);	// Lot Ready

	Sleep(10);

	// 	gMes.-nLotReadyPortNo = nPortIdx + 1;
	// 	gMes.nHostCmCount[nPortIdx] = gMes.nHostCmCount[nPortIdx] = 0;
	// 	gMes.sHostLotId[nPortIdx] = "";
	// 	
	g_objHost.Set_S6F11_PPSelected(sLotId, );
}

void CHandler::Get_LotIdReport(CString sLotId, CString sPortNo, CString sRecipeId)
{
	// 	int nPortIdx = atoi(sPortNo) - 1;
	// 	gData.sEquipRecipe = sRecipeId;
	// 	gMes.sHostLotId[nPortIdx] = sLotId;
	// 
	// 	gMes.sHostModel[nPortIdx] = gMes.sHostRecipe = sRecipeId;
	// 	gMes.nHostCmCount[nPortIdx] = 0;
	// 	gMes.nLotReadyPortNo = nPortIdx + 1;
	// 	g_objHost.Set_S6F11_LotReport(sLotId, sRecipeId);
}

void CHandler::Get_Cancel(CString sLotId, CString sCancelMode)
{
	g_objHost.Set_S6F11_CancelReport(sLotId, sCancelMode);
	Sleep(50);
	g_objHost.Set_S6F11_EquipState(2, 0);	// Idle
}

void CHandler::Get_MGZIdReport(CString sPortID, CString sMGZNo, CString sMGZId)
{
	int nPortNo = atoi(sPortID);
	int nMGZNo = atoi(sMGZNo);
	CString sPortType;

	if (nPortNo < 4) sPortType = "L";
	else			 sPortType = "R";
	gMes.nReportLoadMGZNo = nMGZNo;
	gData.sHandlerPortID[nMGZNo-1] = sMGZId;

	g_objHost.Set_S6F11_MGZIDReport(sPortID, sPortType, sMGZId);
}

void CHandler::Get_CarrierIdReport(CString sMGZNo, CString sCarrierNo, CString sPortID, CString sLotId,  CString sMGZID, CString sCarrierID)
{
	int nMgzIdx = atoi(sMGZNo) - 1;
	int nCarrierIdx = atoi(sCarrierNo) - 1;

	gData.sHandlerPortID[nMgzIdx] = sMGZID;
	gData.sHandlerTrayID[nMgzIdx][nCarrierIdx] = sCarrierID;

	gMes.sTrayId_MGZId = sMGZID;
	gMes.nTrayId_MGZNo = atoi(sMGZNo);
	gMes.nTrayId_CarrierNo = atoi(sCarrierNo);
	gMes.sTrayId_CarrierId = sCarrierID;

	g_objHost.Set_S6F11_CarrierIDReport(sLotId, sPortID, sMGZID, sCarrierID);
}

void CHandler::Get_CarrierLoad(CString sMGZId, CString sSlotId, CString sCarrierId)
{
	g_objHost.Set_S6F11_CarrierLoadReport(sMGZId, sSlotId, sCarrierId);
}

void CHandler::Get_CarrierStart(CString sLotId, CString sPortId, CString sMGZId, CString sSlotId, CString sCarrierId)
{
	g_objHost.Set_S6F11_CarrierStart(sLotId, sPortId, sMGZId, sSlotId, sCarrierId);
}

void CHandler::Get_CarrierEnd(CString sCarrierID, CString sMGZNo, CString sCarrierNo, CString sUnloadSlotNo, CString sTotal, CString sGood, CString sNg, CString sDataQty)
{
	int nLastModuleNo = 0;
	int nMGZNo = atoi(sMGZNo);
	int nCarrierNo = atoi(sCarrierNo);

	for (int i = 39; i >= 0; i--) {
		if (gData.sJudge[nMGZNo-1][nCarrierNo-1][i] != "")  {nLastModuleNo = i + 1;break;}
	}
	
	CString strLastModuleNo;
	strLastModuleNo.Format("%d", nLastModuleNo);

	g_objHost.Set_S6F11_CarrierEnd(sCarrierID, sMGZNo, sCarrierNo, sUnloadSlotNo, sTotal, sGood, sNg, strLastModuleNo, sDataQty);

	for(int i = 0 ; i < 40; i++) {
		gData.sBarcode[nMGZNo-1][nCarrierNo-1][i] = gData.sJudge[nMGZNo-1][nCarrierNo-1][i] = gData.sNgCode[nMGZNo-1][nCarrierNo-1][i] = "";
	}
}

void CHandler::Get_CarrierUnload(CString sMGZId, CString sSlotId, CString sCarrierId)
{
	g_objHost.Set_S6F11_CarrierUnloadReport(sMGZId, sSlotId, sCarrierId);
}

void CHandler::Get_IdleRequest()
{
	g_objHost.Set_S2F61_IdleRequst();
}

void CHandler::Get_LotInfo(CString sLotId)
{
	g_objHost.Set_S6F11_LotInfo(sLotId);
}

*/

///////////////////////////////////////////////////////////////////////////////
// Set Command

void CHandler::Set_LotStart()
{
	CString strSend;
	strSend.Format("LOT,START,%s,%s,%d", gMes.sHostLotId, gMes.sHostRecipe, gMes.nHostCmCount);
	Send_Command(strSend);
}

void CHandler::Set_CmResult()
{
	char chSep = ',';
	CString strTemp, strSend;
	CString strNgCode = "", strNgText = "";

	if (gMes.sPDHostJudge == "NG") {
		strNgCode = "00"; strNgText = "NON";

		AfxExtractSubString(strTemp, gMes.sPDHostDetail, 1, chSep);	// 혼입검사 우선
		if (strTemp == "NG") {
			strSend.Format("CM,RESULT,%s,%s,%s,02,혼입검사", gMes.sPDHostLotId, gMes.sPDHostCmId, gMes.sPDHostJudge);
			Send_Command(strSend);
			return;
		}
		
		for(int i = 0; i < 7; i++) {
			AfxExtractSubString(strTemp, gMes.sPDHostDetail, i, chSep);
			if (strTemp != "NG") continue;

			strNgCode.Format("%02d", i + 1);
			if (i == 0) strNgText = "성능검사";
			if (i == 1) strNgText = "혼입검사";
			if (i == 2) strNgText = "VERSION 체크";
			if (i == 3) strNgText = "WEEK CODE 체크";
			if (i == 4) strNgText = "NA";
			if (i == 5) strNgText = "COSMECTIC 판정값";
			if (i == 6) strNgText = "NA";
			if (i == 7) strNgText = "Marginal";
			if (i == 8) strNgText = "NA";
			if (i == 9) strNgText = "등록된 모듈 체크";
			break;
		}
		if (strTemp == "NG") {
			strSend.Format("CM,RESULT,%s,%s,%s,%s,%s", gMes.sPDHostLotId, gMes.sPDHostCmId, gMes.sPDHostJudge, strNgCode, strNgText);
			Send_Command(strSend);
			return;
		}
		AfxExtractSubString(strTemp, gMes.sPDHostDetail, 9, chSep);	//등록된 모듈 체크
		if (strTemp == "NG") {
			if (gData.bJahwa) {
				gMes.sPDHostJudge = "OK"; strNgCode = strNgText = "";
			} else {
				strNgCode = "10"; strNgText = "등록된 모듈 체크";
			}
		}
	}

	AfxExtractSubString(strTemp, gMes.sPDHostDetail, 7, chSep);	// Marginal Check
	if (strTemp == "NG") {
		strSend.Format("CM,RESULT,%s,%s,NG,08,Marginal", gMes.sPDHostLotId, gMes.sPDHostCmId);
		Send_Command(strSend);
		return;
	}

	strSend.Format("CM,RESULT,%s,%s,%s,%s,%s", gMes.sPDHostLotId, gMes.sPDHostCmId, gMes.sPDHostJudge, strNgCode, strNgText);
	Send_Command(strSend);
}

void CHandler::Set_ModuleFail()
{
	CString strSend;
	strSend.Format("CM,FAIL,%s,%s,%s,%s", gMes.sCancelLotId, gMes.sCancelModule, gMes.sCancelCode, gMes.sCancelText);
	Send_Command(strSend);
}

void CHandler::Set_ControlState(int nFlag)
{
	CString strSend;
	strSend.Format("CONTROL,STATE,%d", nFlag);	// 1:Online, 2:Offline
	Send_Command(strSend);
}

void CHandler::Set_LotCancel()
{
	CString strSend;
	strSend.Format("LOT,CANCEL,%s,%s,%s", gMes.sCancelLotId, gMes.sCancelCode, gMes.sCancelText);
	Send_Command(strSend);
}

void CHandler::Set_RecipeListRequest(BOOL bList)
{
	CString strSend;
	if (bList)	strSend.Format("RECIPE,REQUEST,0");	//All Recipe
	else		strSend.Format("RECIPE,REQUEST,1");	//Current Recipe
	Send_Command(strSend);
}

void CHandler::Set_HostMsg(CString sMsg)
{
	CString strSend; 

	strSend.Format("HOST,MESSAGE,%s", sMsg);
	g_objHandler.Send_Command(strSend);
}

void CHandler::Set_TimeSync()
{
	CString strSend;
	strSend.Format("TIME,UPDATE");
	Send_Command(strSend);
}

void CHandler::Set_NGLotStart()
{
	CString strSend;
	strSend.Format("NGLOT,START,%s,%s", gMes.sHostNGLotId, gMes.sHostNGRecipe);
	Send_Command(strSend);
}

void CHandler::Set_NGLotCancel()
{
	CString strSend;
	strSend.Format("NGLOT,CANCEL,%s,%s,%s", gMes.sCancelLotId, gMes.sCancelCode, gMes.sCancelText);
	Send_Command(strSend);
}

/*
void CHandler::Set_MGZConfirm()
{
	CString strSend;
	strSend.Format("MGZ,CONFIRM,%s", gMes.sHostMGZId[gMes.nReportUnloadMGZNo-1+3]);
	Send_Command(strSend);
	gMes.nReportUnloadMGZNo = 0;
}

void CHandler::Set_MGZCancel()
{
	int nMGZIdx = -1;

	for (int i = 0; i < 6; i++) { if (gMes.sCancelMGZId == gData.sHandlerPortID[i]) nMGZIdx = i; }
	if (nMGZIdx = -1) return;

	CString strSend;
	strSend.Format("MGZ,CANCEL,%s,%s,%s", gMes.sCancelMGZId, gMes.sCancelCode, gMes.sCancelText);
	Send_Command(strSend);

	if (nMGZIdx < 3) gMes.nReportLoadMGZNo = 0;
	else			 gMes.nReportUnloadMGZNo = 0;
}

void CHandler::Set_CarrierConfirm()
{
	CString strSend;
	strSend.Format("CARRIER,CONFIRM,%s,%d", gMes.sTrayId_CarrierId, gMes.nHostCarrierCmCount[gMes.nTrayId_CarrierNo-1]);
	Send_Command(strSend);
}

void CHandler::Set_CarrierCancel()
{
	CString strSend;
	strSend.Format("CARRIER,CANCEL,%s,%s,%s", gMes.sCancelCarrierId, gMes.sCancelCode, gMes.sCancelText);
	Send_Command(strSend);
}

void CHandler::Get_CarrierInfo(CString sMGZNo, CString sCarrierNo, CString sModuleNo, CString sCarrierId, CString sBarcode, CString sJudge, CString sNgCode)
{
	int nMGZNo = atoi(sMGZNo) - 1;
	int nCarrierNo = atoi(sCarrierNo) - 1;
	int nModuleNo = atoi(sModuleNo) - 1;

	gData.sBarcode[nMGZNo][nCarrierNo][nModuleNo] = sBarcode;
	gData.sJudge[nMGZNo][nCarrierNo][nModuleNo] = sJudge;
	gData.sNgCode[nMGZNo][nCarrierNo][nModuleNo] = sNgCode;
}

void CHandler::Set_CarrierInfo()
{
	CString strSend; 
// 	for (int i = 0; i < gMes.nHostCarrierCmCount[gMes.nTrayId_CarrierNo-1]; i++) {
	for (int i = 0; i < 40; i++) {
		if (gMes.sHostPocketID[i] == "") continue;
		strSend.Format("CARRIER,INFO,%d,%d,%s,%s,%s", gMes.nTrayId_MGZNo, gMes.nTrayId_CarrierNo, gMes.sHostPocketID[i], gMes.sHostCmID[i], gMes.sHostCmJudge[i]);
		g_objHandler.Send_Command(strSend);
		gMes.sHostPocketID[i] = gMes.sHostCmID[i] = gMes.sHostCmJudge[i] = "";
	}
}

*/
///////////////////////////////////////////////////////////////////////////////

void CHandler::Send_Command(CString sSend)
{
	CString strLog, strMsg, strSendSocket;

	strSendSocket.Format("@%s\n", sSend);

	char chSend[2003] = { 0 };	// Max 1000
	int nLength = strSendSocket.GetLength();
	memcpy(chSend, (LPSTR)(LPCSTR)strSendSocket, nLength);

	if (!m_Server.Write_Socket(m_nClientIdx, (BYTE*)chSend, nLength)) return;

	// Handler Log ////////////////////////////////////////////////////////////
	strLog.Format("[->] %s", sSend);
	g_objLogFile.Save_HandlerLog(strLog);

	strMsg.Format("[->] %s", sSend);
	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HandlerMsg(strMsg);
}

///////////////////////////////////////////////////////////////////////////////




