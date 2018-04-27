// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   TimeOfDayScheduler.cpp
//  Version:     v1.00
//  Created:     27/02/2007 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: TimeOfDayScheduler
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <Cry3DEngine/ITimeOfDay.h>
#include "TimeOfDayScheduler.h"

#include <CryFlowGraph/IFlowBaseNode.h>

CTimeOfDayScheduler::CTimeOfDayScheduler()
{
	m_nextId = 0;
	m_lastTime = 0.0f;
	m_bForceUpdate = true;
}

CTimeOfDayScheduler::~CTimeOfDayScheduler()
{
}

void CTimeOfDayScheduler::Reset()
{
	stl::free_container(m_entries);
	m_nextId = 0;
	m_lastTime = gEnv->p3DEngine->GetTimeOfDay()->GetTime();
	m_bForceUpdate = true;
}

CTimeOfDayScheduler::TimeOfDayTimerId CTimeOfDayScheduler::AddTimer(float time, CTimeOfDayScheduler::TimeOfDayTimerCallback callback, void* pUserData)
{
	assert(callback != 0);
	if (callback == 0)
		return InvalidTimerId;
	SEntry entryForTime(0, time, 0, 0);
	TEntries::iterator iter = std::lower_bound(m_entries.begin(), m_entries.end(), entryForTime);
	m_entries.insert(iter, SEntry(m_nextId, time, callback, pUserData));
	return m_nextId++;
}

void* CTimeOfDayScheduler::RemoveTimer(TimeOfDayTimerId id)
{
	TEntries::iterator iter = std::find(m_entries.begin(), m_entries.end(), id);
	if (iter == m_entries.end())
		return 0;
	void* pUserData = iter->pUserData;
	m_entries.erase(iter);
	return pUserData;
}

void CTimeOfDayScheduler::Update()
{
	static const float MIN_DT = 1.0f / 100.0f;

	float curTime = gEnv->p3DEngine->GetTimeOfDay()->GetTime();
	float lastTime = m_lastTime;
	const float dt = curTime - lastTime;

	// only evaluate if at least some time passed
	if (m_bForceUpdate == false && fabs_tpl(dt) < MIN_DT)
		return;
	m_bForceUpdate = false;

	// execute all entries between lastTime and curTime
	// if curTime < lastTime, we wrapped around and have to process two intervals
	// [lastTime, 24.0] and [0, curTime]
	TEntries processingEntries; // no need to make member var, as allocation/execution currently is NOT too often
	SEntry entryForTime(0, lastTime, 0, 0);
	TEntries::iterator iter = std::lower_bound(m_entries.begin(), m_entries.end(), entryForTime);
	TEntries::iterator iterEnd = m_entries.end();
	const bool bWrap = lastTime > curTime;
	const float maxTime = bWrap ? 24.0f : curTime;

	//CryLogAlways("CTOD: lastTime=%f curTime=%f", lastTime, curTime);

	// process interval [lastTime, curTime] or in case of wrap [lastTime, 24.0]
	while (iter != iterEnd)
	{
		const SEntry& entry = *iter;
		assert(entry.time >= lastTime);
		if (entry.time > maxTime)
			break;
		// CryLogAlways("Adding: %d time=%f", entry.id, entry.time);
		processingEntries.push_back(entry);
		++iter;
	}

	if (bWrap) // process interval [0, curTime]
	{
		iter = m_entries.begin();
		while (iter != iterEnd)
		{
			const SEntry& entry = *iter;
			if (entry.time > curTime)
				break;
			// CryLogAlways("Adding[wrap]: %d time=%f", entry.id, entry.time);
			processingEntries.push_back(entry);
			++iter;
		}
	}

	iter = processingEntries.begin();
	iterEnd = processingEntries.end();
	while (iter != iterEnd)
	{
		const SEntry& entry = *iter;
		entry.callback(entry.id, entry.pUserData, curTime);
		++iter;
	}

	m_lastTime = curTime;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CFlowNode_TimeOfDayTrigger : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		EIP_Enabled = 0,
		EIP_Time,
	};
	enum EOutputs
	{
		EOP_Trigger = 0,
	};

	CFlowNode_TimeOfDayTrigger(SActivationInfo* /* pActInfo */)
	{
		m_timerId = CTimeOfDayScheduler::InvalidTimerId;
	}

	~CFlowNode_TimeOfDayTrigger()
	{
		ResetTimer();
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_TimeOfDayTrigger(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Active", true, _HELP("Whether trigger is enabled")),
			InputPortConfig<float>("Time",  0.0f, _HELP("Time when to trigger")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("Trigger", _HELP("Triggered when TimeOfDay has been reached. Outputs current timeofday")),
			{ 0 }
		};

		config.sDescription = _HELP("TimeOfDay Trigger");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
		{
			ResetTimer();
			m_actInfo = *pActInfo;
			const bool bEnabled = IsBoolPortActive(pActInfo, EIP_Enabled);
			if (bEnabled)
				RegisterTimer(pActInfo);
		}
		break;
		}
	}

	void ResetTimer()
	{
		if (m_timerId != CTimeOfDayScheduler::InvalidTimerId)
		{
			CCryAction::GetCryAction()->GetTimeOfDayScheduler()->RemoveTimer(m_timerId);
			m_timerId = CTimeOfDayScheduler::InvalidTimerId;
		}
	}

	void RegisterTimer(SActivationInfo* pActInfo)
	{
		assert(m_timerId == CTimeOfDayScheduler::InvalidTimerId);
		const float time = GetPortFloat(pActInfo, EIP_Time);
		m_timerId = CCryAction::GetCryAction()->GetTimeOfDayScheduler()->AddTimer(time, OnTODCallback, (void*)this);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			ResetTimer();
			// re-enable if it's enabled
			const bool bEnabled = GetPortBool(pActInfo, EIP_Enabled);
			if (bEnabled)
				RegisterTimer(pActInfo);
		}
	}

protected:
	static void OnTODCallback(CTimeOfDayScheduler::TimeOfDayTimerId timerId, void* pUserData, float curTime)
	{
		CFlowNode_TimeOfDayTrigger* pThis = reinterpret_cast<CFlowNode_TimeOfDayTrigger*>(pUserData);
		if (timerId != pThis->m_timerId)
			return;
		ActivateOutput(&pThis->m_actInfo, EOP_Trigger, curTime);
	}

	SActivationInfo                       m_actInfo;
	CTimeOfDayScheduler::TimeOfDayTimerId m_timerId;
};
REGISTER_FLOW_NODE("Time:TimeOfDayTrigger", CFlowNode_TimeOfDayTrigger)
