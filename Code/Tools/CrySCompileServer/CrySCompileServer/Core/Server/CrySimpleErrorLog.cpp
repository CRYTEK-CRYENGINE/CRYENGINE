// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../Common.h"
#include "../StdTypes.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../Mailer.h"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleErrorLog.hpp"
#include "CrySimpleServer.hpp"
#include "CrySimpleJob.hpp"
#include <string>
#include <map>
#include <algorithm>

#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#endif
#ifdef UNIX
#include <pthread.h>
#endif

static unsigned int volatile g_bSendingMail = false;
static unsigned int volatile g_nMailNum = 0;

CCrySimpleErrorLog& CCrySimpleErrorLog::Instance()
{
	static CCrySimpleErrorLog g_Cache;
	return g_Cache;
}

CCrySimpleErrorLog::CCrySimpleErrorLog()
{
	m_lastErrorTime = 0;
}

void CCrySimpleErrorLog::Init()
{
	
}

bool CCrySimpleErrorLog::Add(ICryError *err)
{
	CCrySimpleMutexAutoLock Lock(m_LogMutex);

  if (m_Log.size() > 150)
  {
    // too many, just throw this error away
    return false; // no ownership of this error
  }

  m_Log.push_back(err);

  m_lastErrorTime = GetTickCount();

  return true; // take ownership of this error
}

inline bool CmpError(ICryError *a, ICryError *b)
{
   return a->Compare(b);
}

void CCrySimpleErrorLog::SendMail()
{
	CSMTPMailer::tstrcol Rcpt;

	std::string mailBody;

  tdEntryVec RcptVec;
  CSTLHelper::Tokenize(RcptVec,SEnviropment::Instance().m_FailEMail,";");

  for(size_t i=0; i < RcptVec.size(); i++)
    Rcpt.insert(RcptVec[i]);

	tdErrorList tempLog;
	{
		CCrySimpleMutexAutoLock Lock(m_LogMutex);
		m_Log.swap(tempLog);
	}
#if defined(_MSC_VER)
	{
		char compName[256];
		DWORD size = ARRAYSIZE(compName);

		typedef BOOL (WINAPI *FP_GetComputerNameExA)(COMPUTER_NAME_FORMAT, LPSTR, LPDWORD);
		FP_GetComputerNameExA pGetComputerNameExA = (FP_GetComputerNameExA) GetProcAddress(LoadLibrary("kernel32.dll"), "GetComputerNameExA");

		if (pGetComputerNameExA)
			pGetComputerNameExA(ComputerNamePhysicalDnsFullyQualified, compName, &size);
		else
			GetComputerName(compName, &size);

		mailBody += std::string("Report sent from ") + compName + "...\n\n";
	}
#endif
	{
    bool dedupe = SEnviropment::Instance().m_DedupeErrors;
    
    std::vector<ICryError*> errors;

    if(!dedupe)
    {
      for (tdErrorList::const_iterator it = tempLog.begin(); it != tempLog.end(); ++it)
        errors.push_back(*it);
    }
    else
    {
      std::map<tdHash, ICryError*> uniqErrors;
      for (tdErrorList::const_iterator it = tempLog.begin(); it != tempLog.end(); ++it)
      {
        ICryError *err = *it;

        tdHash hash = err->Hash();

        std::map<tdHash, ICryError*>::iterator uniq = uniqErrors.find(hash);
        if(uniq != uniqErrors.end())
        {
          uniq->second->AddDuplicate(err);
          delete err;
        }
        else
        {
          uniqErrors[hash] = err;
        }
      }

      for (std::map<tdHash, ICryError*>::iterator it = uniqErrors.begin(); it != uniqErrors.end(); ++it)
        errors.push_back(it->second);
    }
    
    std::string body = mailBody;
    CSMTPMailer::tstrcol cc;
    CSMTPMailer::tattachlist Attachment;

    std::sort(errors.begin(), errors.end(), CmpError);
    
		int a = 0;
		for(uint32_t i=0; i < errors.size(); i++)
		{
      ICryError *err = errors[i];

      err->SetUniqueID(a+1);
      
      // doesn't have to be related to any job/error,
      // we just use it to differentiate "1-IlluminationPS.txt" from "1-IlluminationPS.txt"
      long req = CCrySimpleJob::GlobalRequestNumber();

      if(err->HasFile())
      {
        char Filename[1024];
        sprintf(Filename,"%d-req%d-thread%d-%s", a+1, req, GetCurrentThreadId(), err->GetFilename().c_str());
        
        char DispFilename[1024];
        sprintf(DispFilename,"%d-%s", a+1, err->GetFilename().c_str());

        std::string sErrorFile =	SEnviropment::Instance().m_Error+Filename;
        std::replace( sErrorFile.begin( ),sErrorFile.end( ), '/','\\' );

        std::vector<uint8_t> bytes;std::string text = err->GetFileContents();
        bytes.resize(text.size()+1); std::copy(text.begin(), text.end(), bytes.begin());
        while(bytes.size() && bytes[bytes.size()-1] == 0)
          bytes.pop_back();
    
	      CrySimple_SECURE_START

        CSTLHelper::ToFile( sErrorFile,bytes);

        Attachment.push_back( CSMTPMailer::tattachment(DispFilename, sErrorFile) );

        CrySimple_SECURE_END
      }

			body += std::string("=============================================================\n");
      body += err->GetErrorDetails(ICryError::OUTPUT_EMAIL) + "\n";
      
      err->AddCCs(cc);
			a++;

      if(i == errors.size()-1 || !err->CanMerge(errors[i+1]))
      {
        CSMTPMailer::tstrcol bcc;

        CSMTPMailer mail("", "", SEnviropment::Instance().m_MailServer);
        bool res = mail.Send("ShaderCompiler@crytek.de", Rcpt, cc, bcc, err->GetErrorName(), body, Attachment);

        a = 0;
        body = mailBody;
        cc.clear();
        
        for (CSMTPMailer::tattachlist::iterator attach = Attachment.begin(); attach != Attachment.end(); ++attach)
        {
          DeleteFile(attach->second.c_str());
        }

        Attachment.clear();
      }
        
      delete err;
		}
	}

	g_bSendingMail = false;
}

//////////////////////////////////////////////////////////////////////////
void CCrySimpleErrorLog::Tick()
{
	if (SEnviropment::Instance().m_MailInterval == 0)
		return;

  DWORD lastError = 0;
  bool forceFlush = false;

  {
	  CCrySimpleMutexAutoLock Lock(m_LogMutex);

    if(m_Log.size() == 0)
      return;

    // log has gotten pretty big, force a flush to avoid losing any errors
    if(m_Log.size() > 100)
      forceFlush = true;

    lastError = m_lastErrorTime;
  }

	DWORD t = GetTickCount();
	if (forceFlush || t < lastError || (t - lastError) > SEnviropment::Instance().m_MailInterval*1000)
	{
		if(!g_bSendingMail)
		{
			g_bSendingMail = true;
			g_nMailNum++;
			logmessage( "Sending Errors Mail %d\n", g_nMailNum);
			CCrySimpleErrorLog::Instance().SendMail();
		}
	}
}
