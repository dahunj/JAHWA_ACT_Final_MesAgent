// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이 
// 들어 있는 포함 파일입니다.
#pragma once

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // 일부 CString 생성자는 명시적으로 선언됩니다.

// MFC의 공통 부분과 무시 가능한 경고 메시지에 대한 숨기기를 해제합니다.
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC 핵심 및 표준 구성 요소입니다.
#include <afxext.h>         // MFC 확장입니다.

#include <afxdisp.h>        // MFC 자동화 클래스입니다.

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // Internet Explorer 4 공용 컨트롤에 대한 MFC 지원입니다.
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // Windows 공용 컨트롤에 대한 MFC 지원입니다.
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // MFC의 리본 및 컨트롤 막대 지원

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

// Library 추가
#include "CSIniFile.h" 
#include "CSServerSocket.h"
#ifdef _DEBUG
	#pragma comment(lib, "CSIniFileD.lib")
	#pragma comment(lib, "CSServerSocketD.lib")
#else
	#pragma comment(lib, "CSIniFileR.lib")
	#pragma comment(lib, "CSServerSocketR.lib")
#endif

#define MAIN_VERSION	"1.1.8f"	//Varo-Tray

extern CString gsCurrentDir;		// 현재 프로젝트 폴더

typedef struct {
	int			nHostPort;
	CString		sEquipId;
	BOOL		bHandlerLog;
	BOOL		bHostLog;
	CString		sErrFile;
	BOOL		bJahwa;
	CString		sOperId;

	CString		sCurrentRecipe;
	int			nRcpCount;
	CString		sRecipList[100];

	int			nPreEquipState;		// 1:init, 2:idle, 3:Setup, 4:Ready, 5:Run, 6:Pause(Down)
	int			nCurEquipState;

	CString		sHandlerLotID[6];
	CString		sHandlerPortID[6];
	CString		sHandlerCMCount[6];

	CString		sBarcode[6][20][40];
	CString		sJudge[6][20][40];
	CString		sNgCode[6][20][40];

	int			nAlarmID;
	CString		sAlarmTxt;
} GLOVAL_DATA;

typedef struct {
	CString		sHostLotId;
	CString		sHostProcID;
	CString		sHostModel;
	CString		sHostRecipe;
	int			nHostCmCount;

	CString		sCancelLotId;
	CString		sCancelRecipe;
	CString		sCancelCode;
	CString		sCancelText;
	CString		sCancelModule;

	CString		sPDHostLotId;
	CString		sPDHostProcID;
	CString		sPDHostModel;
	CString		sPDHostCmId;
	CString		sPDHostJudge;
	CString		sPDHostDetail;

	int			nAHostCount;
	CString		sAHostLotId[50];
	CString		sAHostProcID[50];
	CString		sAHostProdID[50];

	CString		sHostNGLotId;
	CString		sHostNGProcID;
	CString		sHostNGModel;
	CString		sHostNGRecipe;
} GLOVAL_MES;

typedef struct {
	int			nCount;
	CString		sStartTime;
	CString		sEndTime;
	CString		sCode;
	CString		sText;
} GLOVAL_IDLE;

extern  GLOVAL_DATA	gData;
extern  GLOVAL_MES	gMes;
extern  GLOVAL_IDLE	gIdle;
