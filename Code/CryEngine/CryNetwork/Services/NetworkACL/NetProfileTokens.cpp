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

#include "StdAfx.h"
#include "NetProfileTokens.h"

CNetProfileTokens::CNetProfileTokens()
{
	m_state = eNSS_Initializing;
}

void CNetProfileTokens::Init()
{
	CryAutoLock<CryCriticalSection> lock(m_lockTokens);
	m_tokens.clear();
	m_state = eNSS_Ok;
}

void CNetProfileTokens::Close()
{
	m_state = eNSS_Closed;
}

void CNetProfileTokens::AddToken(uint32 profile, uint32 token)
{
	CryAutoLock<CryCriticalSection> lock(m_lockTokens);
	m_tokens.insert(std::make_pair(profile, token));
}

bool CNetProfileTokens::IsValid(uint32 profile, uint32 token)
{
	CryAutoLock<CryCriticalSection> lock(m_lockTokens);

	bool isValid = false;

	TTokenEntities::iterator iter = m_tokens.find(profile);
	if (m_tokens.end() != iter)
	{
		isValid = (m_tokens[profile] == token);
	}
	return isValid;
}

void CNetProfileTokens::Lock()
{
	m_lockTokens.Lock();
}

void CNetProfileTokens::Unlock()
{
	m_lockTokens.Unlock();
}
