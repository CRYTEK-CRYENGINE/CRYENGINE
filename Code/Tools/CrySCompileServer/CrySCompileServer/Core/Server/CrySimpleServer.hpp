// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLESERVER__
#define __CRYSIMPLESERVER__

#include <string>

extern bool g_Success;

class CCrySimpleSock;

class	SEnviropment
{
public:
	std::string		m_Root;
	std::string		m_Compiler;
	std::string		m_Cache;
	std::string		m_Temp;
	std::string		m_Error;

	std::string   m_FailEMail;
	std::string   m_MailServer;
	uint32_t      m_port;
	uint32_t      m_MailInterval; // seconds since last error to flush error mails

	bool					m_Caching;
	bool					m_PrintErrors;
  bool          m_PrintListUpdates;
  bool          m_DedupeErrors;

	std::string		m_FallbackServer;
	long					m_FallbackTreshold;

	static SEnviropment&	Instance();
};

class CCrySimpleServer
{
	static volatile long 			ms_ExceptionCount;
	CCrySimpleSock*						m_pServerSocket;
	void			Init();
public:
						CCrySimpleServer(const char* pShaderModel,const char* pDst,const char* pSrc,const char* pEntryFunction);
						CCrySimpleServer();


	static volatile long 			GetExceptionCount() { return ms_ExceptionCount; }
	static void								IncrementExceptionCount();
};

#endif
