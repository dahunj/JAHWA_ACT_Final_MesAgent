// Common.h : 헤더 파일
//
#pragma once

// CCommon

class CCommon : public CWnd
{
	DECLARE_DYNAMIC(CCommon)

public:
	CCommon();
	virtual ~CCommon();

protected:
	DECLARE_MESSAGE_MAP()

public:
	BOOL Read_Config();
	void Delete_LogAll();
	void Delete_LogFile(CString sPath);
	void DoEvents(int nSleep = 0);
};

extern CCommon g_objCommon;

///////////////////////////////////////////////////////////////////////////////
