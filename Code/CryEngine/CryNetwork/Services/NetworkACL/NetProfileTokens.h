// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: keep list of profile and its token ids
             profile provides its token id during connection
             server uses this server to validate token per profile
   -------------------------------------------------------------------------
   History:
   - 29/09/2009 : Created by Sergii Rustamov
*************************************************************************/

#ifndef __NET_PROFILE_TOKENS_H__
#define __NET_PROFILE_TOKENS_H__

#pragma once

#include <CryNetwork/INetwork.h>
#include <CryNetwork/INetworkService.h>

class CNetProfileTokens : public INetworkService, public INetProfileTokens
{
private:
	typedef std::map<uint32, uint32> TTokenEntities;

public:
	CNetProfileTokens();

	// INetworkService
	virtual ENetworkServiceState GetState()                                                                  { return m_state; }
	virtual void                 Close();
	virtual void                 CreateContextViewExtensions(bool server, IContextViewExtensionAdder* adder) {};
	virtual IServerBrowser*      GetServerBrowser()                                                          { return 0; }
	virtual INetworkChat*        GetNetworkChat()                                                            { return 0; }
	virtual IServerReport*       GetServerReport()                                                           { return 0; }
	virtual IStatsTrack*         GetStatsTrack(int version)                                                  { return 0; }
	virtual INatNeg*             GetNatNeg(INatNegListener* const)                                           { return 0; }
	virtual INetworkProfile*     GetNetworkProfile()                                                         { return 0; }
	virtual ITestInterface*      GetTestInterface()                                                          { return 0; }
	virtual IFileDownloader*     GetFileDownloader()                                                         { return 0; }
	virtual void                 GetMemoryStatistics(ICrySizer* pSizer)                                      {}
	virtual IPatchCheck*         GetPatchCheck()                                                             { return 0; }
	virtual IHTTPGateway*        GetHTTPGateway()                                                            { return 0; }
	virtual INetProfileTokens*   GetNetProfileTokens()                                                       { return static_cast<INetProfileTokens*>(this); }

	// INetProfileTokens
	virtual void AddToken(uint32 profile, uint32 token);
	virtual bool IsValid(uint32 profile, uint32 token);
	virtual void Init();
	virtual void Lock();
	virtual void Unlock();

private:
	ENetworkServiceState m_state;
	TTokenEntities       m_tokens;
	CryCriticalSection   m_lockTokens;
};

#endif // __NET_PROFILE_TOKENS_H__
