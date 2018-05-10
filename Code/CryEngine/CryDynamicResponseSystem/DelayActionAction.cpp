// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DelayActionAction.h"
#include "ResponseInstance.h"

using namespace CryDRS;

//--------------------------------------------------------------------------------------------------
DelayActionActionInstance::DelayActionActionInstance(float timeToDelay, DRS::IResponseActionSharedPtr pActionToDelay, CResponseInstance* pResponseInstance)
	: m_pDelayedAction(pActionToDelay)
	, m_pResponseInstance(pResponseInstance)
	, m_RunningInstance(nullptr)
{
	m_delayFinishTime = gEnv->pTimer->GetFrameStartTime() + CTimeValue(timeToDelay);
}
//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstance::eCurrentState DelayActionActionInstance::Update()
{
	if (m_RunningInstance)
	{
		return m_RunningInstance->Update();
	}

	if (!m_pResponseInstance)
	{
		return DRS::IResponseActionInstance::CS_CANCELED;
	}

	if (gEnv->pTimer->GetFrameStartTime() > m_delayFinishTime)  //finished delaying, start the actual action
	{
		m_RunningInstance = m_pDelayedAction->Execute(m_pResponseInstance);
		if (!m_RunningInstance)
			return DRS::IResponseActionInstance::CS_FINISHED;
		else
			return DRS::IResponseActionInstance::CS_RUNNING;
	}
	return DRS::IResponseActionInstance::CS_RUNNING;
}

//--------------------------------------------------------------------------------------------------
void DelayActionActionInstance::Cancel()
{
	if (m_RunningInstance)
		m_RunningInstance->Cancel();
	m_pResponseInstance = nullptr;
}

//--------------------------------------------------------------------------------------------------
DelayActionActionInstance::~DelayActionActionInstance()
{
	m_RunningInstance = nullptr;
}
