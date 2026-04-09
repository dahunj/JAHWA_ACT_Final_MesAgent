// MesAgentDlg.h : 헤더 파일
//
#pragma once

#include "LedStatic.h"

// CMesAgentDlg 대화 상자
class CMesAgentDlg : public CDialogEx
{
// 생성입니다.
public:
	CMesAgentDlg(CWnd* pParent = NULL);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
	enum { IDD = IDD_MESAGENT_DIALOG };
	CLedStatic	m_ledHandlerState;
	CLedStatic	m_ledHostState;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnDestroy();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedChkHandlerLog();
	afx_msg void OnBnClickedChkHostLog();
	afx_msg void OnBnClickedBtnHandlerListen();
	afx_msg void OnBnClickedBtnHostListen();
	afx_msg void OnBnClickedBtnHandlerClose();
	afx_msg void OnBnClickedBtnHostClose();
	afx_msg void OnDblclkLstHandlerMsg();
	afx_msg void OnDblclkLstHostMsg();
	afx_msg void OnBnClickedBtnTest();

private:
	void Check_DeleteLog();

public:
	void Set_HandlerConnect(BOOL bConnected);
	void Set_HostConnect(BOOL bConnected, CString strIp, int nPort);
	void Set_HandlerMsg(CString sMsg);
	void Set_HostMsg(CString sMsg);
};
