// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#endif
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleHTTP.hpp"
#include "CrySimpleSock.hpp"
#include "CrySimpleJobCompile.hpp"
#include "CrySimpleServer.hpp"
#include "CrySimpleCache.hpp"
#include <assert.h>
#include <io.h>
#include <algorithm>
#include <memory>


//////////////////////////////////////////////////////////////////////////
class CHTTPRequest
{
	CCrySimpleSock*	m_pSock;
public:
							CHTTPRequest(CCrySimpleSock*	pSock):
							m_pSock(pSock){}

							~CHTTPRequest(){delete m_pSock;}

	CCrySimpleSock*	Socket(){return m_pSock;}
};
#define HTML_HEADER "HTTP/1.1 200 OK\n\
Server: Shader compile server %s\n\
Content-Length: %d\n\
Content-Language: de (nach RFC 3282 sowie RFC 1766)\n\
Content-Type: text/html\n\
Connection: close\n\
\n\
<html><title>shader compile server %s</title><body>"

#define TABLE_START	"<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=2 WIDTH=640>\n\
<TR bgcolor=lightgrey><TH align=left>Desciption</TH><TH WIDTH=5></TH><TH>Value</TH><TH>Max</TH>\n\
<TH WIDTH=10>&nbsp;</TH><TH align=center>%</TH></TR>\n"
#define TABLE_INFO	"<TR><TD>%s</TD><TD>&nbsp;</TD><TD align=left>%s</TD><TD align=center></TD><TD>&nbsp;</TD><TD valign=middle>\n\
</TD></TR>\n\
</TD></TR>\n"
#define TABLE_BAR	"<TR><TD>%s</TD><TD>&nbsp;</TD><TD align=center>%d</TD><TD align=center>%d</TD><TD>&nbsp;</TD><TD valign=middle>\n\
<TABLE><TR><TD bgcolor=darkred   style=\"width: %d;\"  ></TD>\n\
<TD><FONT SIZE=1>%d%%</FONT></TD></TR>\n\
</TABLE></TD></TR>\n\
</TD></TR>\n"
#define TABLE_END "</TABLE>"

std::string CreateBar(const std::string& rName,int Value,int Max,int Percentage)
{
	char Text[sizeof(TABLE_BAR)+1024];
	sprintf(Text,TABLE_BAR,rName.c_str(),Value,Max,Percentage,Percentage);
	return Text;
}

std::string CreateInfoText(const std::string& rName,const std::string& rValue)
{
	char Text[sizeof(TABLE_INFO)+1024];
	sprintf(Text,TABLE_INFO,rName.c_str(),rValue.c_str());
	return Text;
}

std::string CreateInfoText(const std::string& rName,int Value)
{
	char Text[64];
	sprintf(Text,"%d",Value);
	return CreateInfoText(rName,Text);
}

uint32_t __stdcall HTTPRequest(void* pData)
{
	std::auto_ptr<CHTTPRequest> pThreadData	=	std::auto_ptr<CHTTPRequest>(reinterpret_cast<CHTTPRequest*>(pData));

	std::string Ret;

	FILETIME IdleTime0,IdleTime1;
	FILETIME KernelTime0,KernelTime1;
	FILETIME UserTime0,UserTime1;

	int Ret0=GetSystemTimes(&IdleTime0,&KernelTime0,&UserTime0);
	Sleep(100);
	int Ret1=GetSystemTimes(&IdleTime1,&KernelTime1,&UserTime1);
	const int Idle		=	IdleTime1.dwLowDateTime-IdleTime0.dwLowDateTime;
	const int Kernel	=	KernelTime1.dwLowDateTime-KernelTime0.dwLowDateTime;
	const int User		=	UserTime1.dwLowDateTime-UserTime0.dwLowDateTime;
	//const int Idle		=	IdleTime1.dwHighDateTime-IdleTime0.dwHighDateTime;
	//const int Kernel	=	KernelTime1.dwHighDateTime-KernelTime0.dwHighDateTime;
	//const int User		=	UserTime1.dwHighDateTime-UserTime0.dwHighDateTime;
	const int Total	=	Kernel+User;
	Ret+=TABLE_START;

	Ret+=CreateInfoText("<b>Load</b>:","");
	if(Ret0&&Ret1&&Total)
		Ret+=CreateBar("CPU-Usage",Total-Idle,Total,100-Idle*100/Total);
	Ret+=CreateBar("CompileTasks",CCrySimpleJobCompile::GlobalCompileTasks(),
																CCrySimpleJobCompile::GlobalCompileTasksMax(),
																CCrySimpleJobCompile::GlobalCompileTasksMax()?
																CCrySimpleJobCompile::GlobalCompileTasks()*100/
																CCrySimpleJobCompile::GlobalCompileTasksMax():0);
	

	Ret+=CreateInfoText("<b>Setup</b>:","");
	Ret+=CreateInfoText("Root",SEnviropment::Instance().m_Root);
	Ret+=CreateInfoText("Compiler",SEnviropment::Instance().m_Compiler);
	Ret+=CreateInfoText("Cache",SEnviropment::Instance().m_Cache);
	Ret+=CreateInfoText("Temp",SEnviropment::Instance().m_Temp);
	Ret+=CreateInfoText("Error",SEnviropment::Instance().m_Error);
	Ret+=CreateInfoText("FailEMail",SEnviropment::Instance().m_FailEMail);
	Ret+=CreateInfoText("MailServer",SEnviropment::Instance().m_MailServer);
	Ret+=CreateInfoText("port",SEnviropment::Instance().m_port);
	Ret+=CreateInfoText("MailInterval",SEnviropment::Instance().m_MailInterval);
	Ret+=CreateInfoText("Caching",SEnviropment::Instance().m_Caching?"Enabled":"Disabled");
	Ret+=CreateInfoText("FallbackServer",SEnviropment::Instance().m_FallbackServer==""?"None":SEnviropment::Instance().m_FallbackServer);
	Ret+=CreateInfoText("FallbackTreshold",static_cast<int>(SEnviropment::Instance().m_FallbackTreshold));

	Ret+=CreateInfoText("<b>Cache</b>:","");
	Ret+=CreateInfoText("Entries",CCrySimpleCache::Instance().EntryCount());
	Ret+=CreateBar("Hits",CCrySimpleCache::Instance().Hit(),
												CCrySimpleCache::Instance().Hit()+CCrySimpleCache::Instance().Miss(),
												CCrySimpleCache::Instance().Hit()*100/max(1,(CCrySimpleCache::Instance().Hit()+CCrySimpleCache::Instance().Miss())));
	Ret+=CreateInfoText("Pending Entries",static_cast<int>(CCrySimpleCache::Instance().PendingCacheEntries().size()));
	


	Ret+=TABLE_END;

	Ret+="</Body></hmtl>";
	char Text[sizeof(HTML_HEADER)+1024];
	sprintf(Text,HTML_HEADER,__DATE__,Ret.size(),__DATE__);
	Ret=std::string(Text)+Ret;
	pThreadData->Socket()->Send(Ret);

//	Sleep(100);
	return 0;
}

uint32_t __stdcall HTTPServerRun(void* pData)
{
	reinterpret_cast<CCrySimpleHTTP*>(pData)->Run();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CCrySimpleHTTP::CCrySimpleHTTP():
m_pServerSocket(0)
{
	CrySimple_SECURE_START

		Init();

	CrySimple_SECURE_END
}

void CCrySimpleHTTP::Init()
{
	m_pServerSocket	=	new CCrySimpleSock(80);	//http
	m_pServerSocket->Listen();
	uint32_t Ret;
	CloseHandle(reinterpret_cast<HANDLE>(_beginthreadex(0,0,HTTPServerRun,(void*)this,0,&Ret)));
}

void CCrySimpleHTTP::Run()
{
	while(1)
	{
		CHTTPRequest* pData	=	new CHTTPRequest(m_pServerSocket->Accept());
		uint32_t Ret;
		CloseHandle(reinterpret_cast<HANDLE>(_beginthreadex(0,0,HTTPRequest,(void*)pData,0,&Ret)));
	}
}


