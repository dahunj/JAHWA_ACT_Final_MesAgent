// LedStatic.h : 헤더 파일
//
#pragma once

class CLedStatic : public CStatic
{
	DECLARE_DYNAMIC(CLedStatic)

public:
	CLedStatic();
	virtual ~CLedStatic();

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();

private:
	BOOL		m_bStatus;		// On=TRUE, Off=FALSE
	COLORREF	m_colorOn;
	COLORREF	m_colorOff;

public:
	void SetColor(COLORREF colorOn, COLORREF colorOff);
	BOOL GetStatus();
	void On();
	void Off();
};

///////////////////////////////////////////////////////////////////////////////
