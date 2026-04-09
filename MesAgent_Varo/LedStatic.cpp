// LedStatic.cpp : 구현 파일
//
#include "stdafx.h"
#include "LedStatic.h"

// const COLORREF CLOUD_BLUE = RGB(128, 184, 223);
// const COLORREF WHITE = RGB(255, 255, 255);
// const COLORREF BLACK = RGB(1, 1, 1);
// const COLORREF DK_GRAY = RGB(128, 128, 128);
// const COLORREF LT_GRAY = RGB(192, 192, 192);
// const COLORREF YELLOW = RGB(255, 255, 0);
// const COLORREF DK_YELLOW = RGB(128, 128, 0);
// const COLORREF RED = RGB(255, 0, 0);
// const COLORREF DK_RED = RGB(128, 0, 0);
// const COLORREF BLUE = RGB(0, 0, 255);
// const COLORREF DK_BLUE = RGB(0, 0, 128);
// const COLORREF CYAN = RGB(0, 255, 255);
// const COLORREF DK_CYAN = RGB(0, 128, 128);
// const COLORREF GREEN = RGB(0, 255, 0);
// const COLORREF DK_GREEN = RGB(0, 128, 0);
// const COLORREF MAGENTA = RGB(255, 0, 255);
// const COLORREF DK_MAGENTA = RGB(128, 0, 128);

IMPLEMENT_DYNAMIC(CLedStatic, CStatic)

CLedStatic::CLedStatic()
{
	m_colorOn = RGB(255, 255, 255);	// White (Default On Color)
	m_colorOff = RGB(0, 0, 0);		// Black (Default Off Color)
	m_bStatus = FALSE;
}

CLedStatic::~CLedStatic()
{
}

BEGIN_MESSAGE_MAP(CLedStatic, CStatic)
	ON_WM_PAINT()
END_MESSAGE_MAP()

// CLedStatic 메시지 처리기입니다.

void CLedStatic::OnPaint()
{
	CPaintDC dc(this);	// 그리기를 위한 디바이스 컨텍스트입니다.
	// TODO: 여기에 메시지 처리기 코드를 추가합니다.
	// 그리기 메시지에 대해서는 CStatic::OnPaint()을(를) 호출하지 마십시오.
	CRect rectctrl;
	GetClientRect(&rectctrl);

	CBitmap *pOldTempBitmap;
	CDC tempDC;
	tempDC.CreateCompatibleDC(&dc);

	CBitmap tempBitmap;
	tempBitmap.CreateCompatibleBitmap(&dc, rectctrl.Width(), rectctrl.Height());

	pOldTempBitmap = tempDC.SelectObject(&tempBitmap);

	if (m_bStatus) tempDC.FillRect(rectctrl, &CBrush(m_colorOn));
	else tempDC.FillRect(rectctrl, &CBrush(m_colorOff));

	dc.BitBlt(rectctrl.left, rectctrl.top, rectctrl.right, rectctrl.bottom,	&tempDC, 0, 0, SRCCOPY);

	tempDC.SelectObject(pOldTempBitmap);
}

void CLedStatic::SetColor(COLORREF colorOn, COLORREF colorOff)
{
	m_colorOn = colorOn;
	m_colorOff = colorOff;
}

BOOL CLedStatic::GetStatus()
{
	return m_bStatus;
}

void CLedStatic::On()
{
	m_bStatus = TRUE;
	Invalidate();
}

void CLedStatic::Off()
{
	m_bStatus = FALSE;
	Invalidate();
}

///////////////////////////////////////////////////////////////////////////////
