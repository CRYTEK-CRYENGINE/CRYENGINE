// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ServerTimer.h"

CServerTimer CServerTimer::m_this;

CServerTimer::CServerTimer()
{
	m_remoteFrameStartTime.SetSeconds(1);
	m_frameTime.SetSeconds(0);
	m_replicationTime.SetSeconds(0);
}

void CServerTimer::UpdateOnFrameStart()
{
	CTimeValue lastTime = m_remoteFrameStartTime;

	if (gEnv->bServer)
		m_remoteFrameStartTime = GetGTimer()->GetFrameStartTime();
	else if (INetChannel* pChannel = CCryAction::GetCryAction()->GetClientChannel())
	{
		if (pChannel->IsTimeReady())
			m_remoteFrameStartTime = pChannel->GetRemoteTime();
	}

	m_frameTime = m_remoteFrameStartTime - lastTime;
	m_frameTime = min(m_frameTime, gEnv->pConsole->GetCVar("t_MaxStep")->GetTime());

	m_replicationTime += m_frameTime;
}