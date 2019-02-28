// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 24:8:2004   10:58 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "GameContext.h"
#include "GameClientNub.h"
#include "GameClientChannel.h"

CGameClientNub::~CGameClientNub()
{
	delete m_pClientChannel;
}

void CGameClientNub::Release()
{
	// don't delete because it's not dynamically allocated
}

SCreateChannelResult CGameClientNub::CreateChannel(INetChannel* pChannel, const char* pRequest)
{
	if (pRequest)
	{
		GameWarning("CGameClientNub::CreateChannel: pRequest is non-null, it should not be");
		CRY_ASSERT(false);
		SCreateChannelResult res(eDC_GameError);
		cry_strcpy(res.errorMsg, "CGameClientNub::CreateChannel: pRequest is non-null, it should not be");
		return res;
	}

	if (m_pClientChannel)
	{
		GameWarning("CGameClientNub::CreateChannel: m_pClientChannel is non-null, it should not be");
		CRY_ASSERT(false);
		SCreateChannelResult res(eDC_GameError);
		cry_strcpy(res.errorMsg, "CGameClientNub::CreateChannel: m_pClientChannel is non-null, it should not be");
		return res;
	}

	if (CCryAction::GetCryAction()->IsGameSessionMigrating())
	{
		pChannel->SetMigratingChannel(true);
	}

	m_pClientChannel = new CGameClientChannel(pChannel, m_pGameContext, this);

	ICVar* pPass = gEnv->pConsole->GetCVar("sv_password");
	if (pPass && gEnv->bMultiplayer)
		pChannel->SetPassword(pPass->GetString());

	return SCreateChannelResult(m_pClientChannel);
}

void CGameClientNub::FailedActiveConnect(EDisconnectionCause cause, const char* description)
{
	GameWarning("Failed connecting to server: %s", description);
	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_connectFailed, int(cause), description));
}

INetChannel* CGameClientNub::GetNetChannel()
{
	return m_pClientChannel != nullptr ? m_pClientChannel->GetNetChannel() : nullptr; 
}

void CGameClientNub::Disconnect(EDisconnectionCause cause, const char* msg)
{
	if (m_pClientChannel)
		m_pClientChannel->GetNetChannel()->Disconnect(cause, msg);
}

void CGameClientNub::ClientChannelClosed()
{
	CRY_ASSERT(m_pClientChannel);
	m_pClientChannel = NULL;
}

void CGameClientNub::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	if (m_pClientChannel)
		m_pClientChannel->GetMemoryStatistics(s);
}
