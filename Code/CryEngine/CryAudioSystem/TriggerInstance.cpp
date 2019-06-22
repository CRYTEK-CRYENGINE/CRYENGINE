// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TriggerInstance.h"
#include "System.h"
#include "CallbackRequestData.h"

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
bool CTriggerInstance::IsPlayingInstanceFinished()
{
	CRY_ASSERT_MESSAGE(m_numPlayingConnectionInstances > 0, "Number of playing trigger instances must be at least 1 during %s", __FUNCTION__);

	return (--m_numPlayingConnectionInstances == 0) && (m_numPendingConnectionInstances == 0);
}

//////////////////////////////////////////////////////////////////////////
bool CTriggerInstance::IsPendingInstanceFinished()
{
	CRY_ASSERT_MESSAGE(m_numPendingConnectionInstances > 0, "Number of pending trigger instances must be at least 1 during %s", __FUNCTION__);

	return (--m_numPendingConnectionInstances == 0) && (m_numPlayingConnectionInstances == 0);
}

//////////////////////////////////////////////////////////////////////////
void CTriggerInstance::SetPendingToPlaying()
{
	CRY_ASSERT_MESSAGE(m_numPendingConnectionInstances > 0, "Number of pending trigger instances must be at least 1 during %s", __FUNCTION__);

	--m_numPendingConnectionInstances;
	++m_numPlayingConnectionInstances;
}

//////////////////////////////////////////////////////////////////////////
void CTriggerInstance::SendFinishedRequest()
{
	if ((m_flags& ERequestFlags::DoneCallbackOnExternalThread) != 0)
	{
		SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerInstance> const requestData(m_triggerId, m_entityId);
		CRequest const request(&requestData, ERequestFlags::CallbackOnExternalOrCallingThread, m_pOwner, m_pUserData, m_pUserDataOwner);
		g_system.PushRequest(request);
	}
	else if ((m_flags& ERequestFlags::DoneCallbackOnAudioThread) != 0)
	{
		SCallbackRequestData<ECallbackRequestType::ReportFinishedTriggerInstance> const requestData(m_triggerId, m_entityId);
		CRequest const request(&requestData, ERequestFlags::CallbackOnAudioThread, m_pOwner, m_pUserData, m_pUserDataOwner);
		g_system.PushRequest(request);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTriggerInstance::Release()
{
	m_numPlayingConnectionInstances = 0;
	m_numPendingConnectionInstances = 0;
}
} // namespace CryAudio
