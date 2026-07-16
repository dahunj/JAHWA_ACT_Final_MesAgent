// Handler.h : 헤더 파일
//
#pragma once

// CHandler

#define HANDLER_PORT	10000	// Local Port for Handler
// #define HANDLER_PORT	10001	// Local Port for Handler

class CHandler : public CWnd
{
	DECLARE_DYNAMIC(CHandler)

public:
	CHandler();
	virtual ~CHandler();

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnServerAccept(WPARAM wClientIdx, LPARAM lServerPort);
	afx_msg LRESULT OnServerRemove(WPARAM wClientIdx, LPARAM lServerPort);
	afx_msg LRESULT OnServerReceive(WPARAM wClientIdx, LPARAM lServerPort);

private:
	CServerSocketCS m_Server;
	int		m_nClientIdx;
	BOOL	m_bConnected;
	CString m_strRecvCmd;

private:
	void Get_OperUpdate(CString sOperId);
	void Get_EquipState(CString sState);	// 1:Idle, 2:Run, 3:Down
	void Get_ErrorUpdate(CString sFlag, CString sErrNo);
	void Get_ControlState(CString sFlag, CString sOperId);	// 1:Online, 2:Offline
	void Get_LotReady(CString sLotId, CString sPortNo);
	void Get_LotStart(CString sType, CString sLotId, CString sRecipe, CString sCount);
	void Get_LotEnd(CString sLotId, CString sRecipe, CString sHCount, CString sOk, CString sNg);

	void Get_LotAbort(CString sLotId);
	void Get_CmRequest(CString sLotId, CString sCmId);
	void Get_CmEnd(CString sLotId, CString sCmId, CString sResult, CString sNgCode, CString sROSResult, CString sPocket);
	void Get_IdleReport(CString sOperId, CString sSTime, CString sETime, CString sCode, CString nType);
	void Get_RecipeList(CString sRecipeData);
	void Get_TerminalOK();

	void Get_NGLotRequest();
	void Get_NGLotEnd(CString sLotId, CString sMOk, CString sNg);

	void Get_AccessChanged(CString sAccessMode);
	void Get_UnitState(CString sState);

//	void Get_LotInfo(CString sLotId);
//	void Get_IdleRequest();
// 	void Get_IdleReport(CString strSet, CString sOperId, CString sCode);	// strSet : "SET" or "RESET"
//	void Get_LotIdReport(CString sLotId, CString sPortNo, CString sRecipeId);

	// SMT 추가
//	void Get_Cancel(CString sLotId, CString sCancelMode);
//	void Get_MGZIdReport(CString sMGZNo, CString sPortID, CString sMGZId);
//	void Get_CarrierIdReport(CString sMGZNo, CString sCarrierNo, CString sLotId, CString sPortID, CString sMGZID, CString sCarrierID);
//	void Get_CarrierLoad(CString sMGZId, CString sSlotId, CString sCarrierId);
//	void Get_CarrierStart(CString sLotId, CString sPortId, CString sMGZId, CString sSlotId, CString sCarrierId);
//	void Get_CarrierUnload(CString sMGZId, CString sSlotId, CString sCarrierId);
//	void Get_CarrierInfo(CString sMGZNo, CString sCarrierNo, CString sModuleNo, CString sCarrierId, CString sBarcode, CString sJudge, CString sNgCode);
//	void Get_CarrierEnd(CString sCarrierID, CString sMGZNo, CString sCarrierNo, CString sUnloadSlotNo, CString sTotal, CString sGood, CString sNg, CString sDataQty);

// 	void Send_Command(CString sSend);

public:
	void Send_Command(CString sSend);
	void Initialize();
	void Terminate();

	BOOL Is_Connected() { return m_bConnected; }

	void Set_ControlState(int nFlag);	// 1:Online, 2:Offline
	void Set_LotStart();
	void Set_LotCancel();
	void Set_CmResult();
	void Set_TimeSync();
	void Set_ModuleFail();

	void Set_RecipeListRequest(BOOL bList);
	void Set_HostMsg(CString sMsg);

	void Set_NGLotStart();
	void Set_NGLotCancel();

//	void Set_MGZConfirm();
//	void Set_MGZCancel();
//	void Set_CarrierConfirm();
//	void Set_CarrierCancel();
//	void Set_CarrierInfo();
};

extern CHandler g_objHandler;

///////////////////////////////////////////////////////////////////////////////
