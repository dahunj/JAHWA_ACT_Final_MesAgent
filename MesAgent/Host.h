// Host.h : 헤더 파일
//
#pragma once

#include "./CXml/Xml.h"

using namespace JWXml;

// CHost

class CHost : public CWnd
{
	DECLARE_DYNAMIC(CHost)

public:
	CHost();
	virtual ~CHost();

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnServerAccept(WPARAM wClientIdx, LPARAM lServerPort);
	afx_msg LRESULT OnServerRemove(WPARAM wClientIdx, LPARAM lServerPort);
	afx_msg LRESULT OnServerReceive(WPARAM wClientIdx, LPARAM lServerPort);

private:
	CServerSocketCS m_Server;
	int		m_nClientIdx;
	BOOL	m_bConnected;
	BOOL	m_bHostOnline;

	CXml	m_xml;

	CString m_strRecvCmd;

	int		m_nRecvCmdCount;	// 4Byte
	int		m_nSendCmdCount;

	CString m_strStFn;	// StreamFunction (S1F1, S2F3, S2F31, S2F49, S6F12)
	CString m_strRcmd;	// RCMD Command (START, CANCEL, DATA, PERMIT)

	DWORD	m_dwLastTime;	// 마지막 통신 시간
	CString m_strSetTime;	// Host 설정 시간

	int		m_nS1F4AckNo;	
	CString m_sHostMsg;

private:
	BOOL Extract_Xml(CString sXmlData);

	void Get_S1F2();	// Are You There Data ==> S1F1 응답
	void Get_S1F3();	// 현재 Recipe ID 조회 요청
	void Get_S2F3();	// Link Test Request
	void Get_S2F31();	// Date and Time Set Request
	void Get_S7F19();	// Recipe ID List 요청 
	void Get_S10F3();	// Terminal Display, Single

	void Get_S2F49_LotStart();		// Enhanced Remote Command
	void Get_S2F49_LotCancel();		// Enhanced Remote Command
	void Get_S2F49_ProductData();	// Enhanced Remote Command
	void Get_S2F49_Module_Fail();	// Enhanced Remote Command

	void Get_S2F49_NGLotStart();
	void Get_S2F49_NGLotFail();

/*
	void Get_S2F49_LotInfo();
	void Get_S2F49_CarrierCancel();
	void Get_S2F49_MGZCancel();
	void Get_S2F49_MGZConfirm();
	void Get_S2F49_CarrierConfirm();
	void Get_S2F49_CmResult();	// Enhanced Remote Command
*/	
	void Reply_HeartBeat();	// Heart Beat
	void Send_Command(CString sSend, BOOL bReply, CString sStFn, CString sRcmd="");	// XML

public:
	void Initialize();
	void Terminate();

	BOOL Is_Connected() { return m_bConnected; }
	BOOL Is_HostOnline() { return m_bHostOnline; }
	DWORD Get_LastTime() { return m_dwLastTime; }

	void Set_S1F1();	// Are You There Request
	void Set_S1F4();	// S1F3 에 대한 응답 (현재 Recipe ID 회신)
	void Set_S1F4_State();
	void Set_S2F4();	// Link Test Response => S2F3 응답
	void Set_S2F32();	// Date and Time Set Acknowledge => S1F31 응답
	void Set_S5F1_Alarm(int nSet, int nErrNo);
	void Set_S7F20();	// S7F19에 대한 응답 (Recipe List 회신)
	void Set_S10F4();	// Terminal Display => S10F3 응답

	// Enhanced Remote Command Acknowledge, S2F49에 대한 응답
	void Set_S2F50_LotStart();			// LotStart Ack
	void Set_S2F50_LotCancel();			// LOT_ID_FAIL Ack
	void Set_S2F50_ProcuctData();		// PRODUCT_DATA Ack
	void Set_S2F50_Module_Fail();		// Module_Fail Ack
	void Set_S2F50_NGLotStart();
	void Set_S2F50_NGLotCancel();

	void Set_S6F11_ControlState(int nState);			// 1:Online, 2:Offline
	void Set_S6F11_EquipState(int nState, int nErrNo);	// 2:Idle, 5:Run, 6:Down
	void Set_S6F11_IdleReportSet(BOOL bSet);
	void Set_S6F11_AccessModeChanged(CString sMode);


	void Set_S6F11_LotReport(CString sLotId, CString sRecipeId);
	void Set_S6F11_LotStart(CString sLotId, CString sRecipeId, int nCount);
	void Set_S6F11_LotEnd(CString sLotId, CString sRecipeId, int nHCount, int nOk, int nNg);
	void Set_S6F11_LotAbort(CString sLotId);				
	void Set_S6F11_CmRequest(CString sLotId, CString sCmId);	// Module=CM 전공정 DATA 요청
	void Set_S6F11_CmEnd(CString sLotId, CString sCmId, CString sResult, CString sNgCode, int nNgPocket, CString sROSResult);
	void Set_S6F11_Terminal();
	void Set_S6F11_NGLotRequest();
	void Set_S6F11_NGLotEnd(CString sLotId, int nMOk, int nNg);

	void Test_Send();
	void Test_WriteLog();
	int  Test_Receive(CString strRecvSocket);
	void Set_AddInfor(CString sLotId, CString sProcID, CString sProdID);
	void Set_DelInfor(CString sLotId);
	void Get_LotInfor(CString sLotId);

	// 미사용
/*
	void Set_S5F1_ErrorUpdate(int nFlag, CString sErrNo, CString sErrMsg);	// nFlag(1:Alarm, 0:해제) Alarm Report Send
	
	void Set_S6F11_IdleReport();	// Idle Report

	void Set_S6F11_MGZIDReport(CString sPortNo, CString sPortType, CString sMGZId);													// Magazine Barcode Reading 하면 보고
	void Set_S6F11_CarrierLoadReport(CString sMGZID, CString sMGZSlotID, CString sCarrierID);										// Load MGZ에서 Carrier가 배출되면 보고
	void Set_S6F11_CarrierStart(CString sLotID, CString sPortID, CString sMGZID, CString sMGZSlotID, CString sCarrierID);			// Carrier 작업 시작시 보고
	void Set_S6F11_CarrierIDReport(CString sLotID, CString sPortID, CString sMGZID, CString sCarrierID);							// Carrier Barcode Reading 하면 보고
	void Set_S6F11_CarrierEnd(CString sCarrierID, CString sMGZNo, CString sCarrierNo, CString sUnloadSlotNo, CString sTotal, CString sGood, CString sNg, CString sLastModuleNo, CString sDataQty);	// Carrier 작업 완료시 보고
	void Set_S6F11_CarrierUnloadReport(CString sMGZID, CString sMGZSlotID, CString sCarrierID);										// Unload MGZ에 Carrier가 투입되면 보고	
	void Set_S6F11_CancelReport(CString sId, CString sCancelMode);																	// CacelType M(aterial)/L(ot)/R(ecipe), ID: Cancel되는 ID, CancelMode H(ost)/O(perator)
	void Set_S6F11_LotInfo(CString sLotId);																							// Lot Info Request

	void Set_S9F13();	// Conversation Timeout

	// Enhanced Remote Command Acknowledge, S2F49에 대한 응답
	void Set_S2F50_MGZCancel();			// MGZ_CANCEL
	void Set_S2F50_MGZConfirm();		// MGZ_CONFIRM
	void Set_S2F50_CarrierCancel();		// TRAY_CANCEL Ack
	void Set_S2F50_CarrierConfirm();	// TRAY_ID_CONFIRM Ack

	void Set_S2F61_IdleRequst();	// Idle Reason Code Request
	
*/
};

extern CHost g_objHost;

///////////////////////////////////////////////////////////////////////////////
