// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ActionWait.h"
#include "ResponseSystem.h"

#include <CryString/StringUtils.h>
#include <CryDynamicResponseSystem/IDynamicResponseAction.h>
#include <CryMath/Random.h>

using namespace CryDRS;
//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstanceUniquePtr CActionWait::Execute(DRS::IResponseInstance* pResponseInstance)
{
	return DRS::IResponseActionInstanceUniquePtr(new CActionWaitInstance(cry_random(m_minTimeToWait, m_maxTimeToWait)));
}

//--------------------------------------------------------------------------------------------------
string CActionWait::GetVerboseInfo() const
{
	return string().Format("min '%f', max '%f'", m_minTimeToWait, m_maxTimeToWait);
}

//--------------------------------------------------------------------------------------------------
void CActionWait::Serialize(Serialization::IArchive& ar)
{
	ar(m_minTimeToWait, "Time", "^Min Time (in seconds)");
	ar(m_maxTimeToWait, "MaxTime", "^Max Time (in seconds)");
	if (m_maxTimeToWait < m_minTimeToWait)
	{
		m_maxTimeToWait = m_minTimeToWait;
	}
}

//--------------------------------------------------------------------------------------------------
CActionWaitInstance::CActionWaitInstance(float timeToWait)
{
	m_finishTime = CResponseSystem::GetInstance()->GetCurrentDrsTime() + timeToWait;
}

//--------------------------------------------------------------------------------------------------
DRS::IResponseActionInstance::eCurrentState CActionWaitInstance::Update()
{
	if (CResponseSystem::GetInstance()->GetCurrentDrsTime() > m_finishTime)
	{
		return DRS::IResponseActionInstance::CS_FINISHED;
	}
	return DRS::IResponseActionInstance::CS_RUNNING;
}
