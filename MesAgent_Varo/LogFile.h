// LogFile.h : 헤더 파일
//
#pragma once

class CLogFile  
{
public:
	CLogFile();
	virtual ~CLogFile();

private:
	void Create_Folder(CString sPath);

public:
	void Save_AgentLog(CString sLog);
	void Save_HandlerLog(CString sLog);
	void Save_HostLog(CString sLog);
};

extern CLogFile g_objLogFile;

///////////////////////////////////////////////////////////////////////////////
