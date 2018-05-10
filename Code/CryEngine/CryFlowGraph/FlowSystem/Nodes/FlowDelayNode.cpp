// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowDelayNode.h"

#include <CryMath/Random.h>

CFlowDelayNode::CFlowDelayNode(SActivationInfo* pActInfo) : m_actInfo(*pActInfo)
{
}

CFlowDelayNode::~CFlowDelayNode()
{
	RemovePendingTimers();
}

void
CFlowDelayNode::RemovePendingTimers()
{
	// remove all old timers we have
	if (!gEnv->IsEditor() && !gEnv->pSystem->IsQuitting())
	{
		Activations::iterator iter;
		if (!m_activations.empty())
		{
			for (iter = m_activations.begin(); iter != m_activations.end(); ++iter)
			{
				gEnv->pGameFramework->RemoveTimer((*iter).first);
			}
		}
	}
	// clear in any case (Editor and Game mode)
	m_activations.clear();
}

IFlowNodePtr CFlowDelayNode::Clone(SActivationInfo* pActInfo)
{
	return new CFlowDelayNode(pActInfo);
}

void CFlowDelayNode::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();

	ser.BeginGroup("Local");
	// Editor mode: map with
	// key:  abs time in ms
	// data: SDelayData
	//
	// Game mode: map with
	// key:  timer id (we don't care about it)
	// data: SDelayData
	if (ser.IsWriting())
	{
		// when writing, currently relative values are stored!
		ser.Value("m_activations", m_activations);
#if 0
		CryLogAlways("CDelayNode write: current time(ms): %f", curTime.GetMilliSeconds());
		Activations::iterator iter = m_activations.begin();
		while (iter != m_activations.end())
		{
			CryLogAlways("CDelayNode write: ms=%d  timevalue(ms): %f", (*iter).first, (*iter).second.m_timeout.GetMilliSeconds());
			++iter;
		}
#endif
	}
	else
	{
		// FIXME: should we read the curTime from the file
		//        or is the FrameStartTime already set to the serialized value?
		// ser.Value("curTime", curTime);

		// when reading we have to differentiate between Editor and Game Mode
		if (gEnv->IsEditor())
		{
			// we can directly read into the m_activations array
			// regular update is handled by CFlowGraph
			ser.Value("m_activations", m_activations);
			Activations::iterator iter = m_activations.begin();
#if 0
			CryLogAlways("CDelayNode read: current time(ms): %f", curTime.GetMilliSeconds());
			while (iter != m_activations.end())
			{
				CryLogAlways("CDelayNode read: ms=%d  timevalue(ms): %f", (*iter).first, (*iter).second.m_timeout.GetMilliSeconds());
				++iter;
			}
#endif
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !m_activations.empty());
		}
		else
		{
			RemovePendingTimers();
			// read serialized activations and re-register at timer
			Activations::iterator iter;
			Activations activations;
			ser.Value("m_activations", activations);
			for (iter = activations.begin(); iter != activations.end(); ++iter)
			{
				CTimeValue relTime = (*iter).second.m_timeout - curTime;
				IGameFramework::TimerID timerId = gEnv->pGameFramework->AddTimer(relTime, false, functor(CFlowDelayNode::OnTimer),
				                                                                       this);
				m_activations[timerId] = (*iter).second;
			}
		}
	}
	ser.EndGroup();
}

void CFlowDelayNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig_AnyType("in",         _HELP("Value to be passed after [Delay] time"), _HELP("In")),
		InputPortConfig<float>("delay",       1.0f,                                           _HELP("Delay time in seconds"),                                                                                             _HELP("Delay")),
		InputPortConfig<bool>("resetOnInput", false,                                          _HELP("When true, the node is reseted with each input (delay counter is reseted to 0, and previous inputs are forgotten)")),
		{ 0 }
	};

	static const SOutputPortConfig outputs[] = {
		OutputPortConfig_AnyType("out", _HELP("Out")),
		{ 0 }
	};

	config.sDescription = _HELP("This node will delay passing the signal from [In] to [Out] for the specified number of seconds in [Delay]");
	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowDelayNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		RemovePendingTimers();
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		break;

	case eFE_Activate:
		// we request a final activation if the input value changed
		// thus in case we get several inputs in ONE frame, we always only use the LAST one!
		if (IsPortActive(pActInfo, INP_IN))
		{
			pActInfo->pGraph->RequestFinalActivation(pActInfo->myID);
		}
		break;

	case eFE_FinalActivate:
		{
			bool shouldReset = GetShouldReset(pActInfo);

			if (gEnv->IsEditor())
			{
				if (shouldReset)
					m_activations.clear();
				const float delay = GetDelayTime(pActInfo);
				CTimeValue finishTime = gEnv->pTimer->GetFrameStartTime() + delay;
				m_activations[(int)finishTime.GetMilliSeconds()] = SDelayData(finishTime, GetPortAny(pActInfo, 0));
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			}
			else
			{
				if (shouldReset)
				{
					for (Activations::iterator iter = m_activations.begin(); iter != m_activations.end(); ++iter)
					{
						IGameFramework::TimerID timerId = (*iter).first;
						gEnv->pGameFramework->RemoveTimer(timerId);
					}
					m_activations.clear();
				}

				const float delay = GetDelayTime(pActInfo);
				CTimeValue finishTime = gEnv->pTimer->GetFrameStartTime() + delay;
				IGameFramework::TimerID timerId = gEnv->pGameFramework->AddTimer(delay, false, functor(CFlowDelayNode::OnTimer),
				                                                                       this);
				m_activations[timerId] = SDelayData(finishTime, GetPortAny(pActInfo, 0));
			}
			break;
		}

	case eFE_Update:
		CRY_ASSERT(gEnv->IsEditor());
		CRY_ASSERT(!m_activations.empty());
		CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();

		while (!m_activations.empty() && m_activations.begin()->second.m_timeout < curTime)
		{
			ActivateOutput(pActInfo, 0, m_activations.begin()->second.m_data);
			m_activations.erase(m_activations.begin());
		}
		if (m_activations.empty())
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		break;
	}
}

float CFlowDelayNode::GetDelayTime(SActivationInfo* pActInfo) const
{
	return GetPortFloat(pActInfo, INP_DELAY);
}

void CFlowDelayNode::OnTimer(void* pUserData, IGameFramework::TimerID ref)
{
	CRY_ASSERT(gEnv->IsEditor() == false);

	CFlowDelayNode* pThis = static_cast<CFlowDelayNode*>(pUserData);
	Activations::iterator iter = pThis->m_activations.find(ref);
	if (iter == pThis->m_activations.end())
	{
		GameWarning("CFlowDelayNode::OnTimer: Stale reference %d", ref);
		return;
	}
	ActivateOutput(&pThis->m_actInfo, 0, iter->second.m_data);
	pThis->m_activations.erase(iter);
}

bool CFlowDelayNode::GetShouldReset(SActivationInfo* pActInfo)
{
	return GetPortBool(pActInfo, INP_RESET_ON_EACH_INPUT);
}

//////////////////////////////////////////////////////////////////////////
class CFlowRandomDelayNode : public CFlowDelayNode
{
public:
	CFlowRandomDelayNode(SActivationInfo* pActInfo) : CFlowDelayNode(pActInfo)
	{
	}

	virtual ~CFlowRandomDelayNode()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowRandomDelayNode(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("In",      _HELP("Value to be passed after delay time")),
			InputPortConfig<float>("MinDelay", 1.0f,                                         _HELP("Minimum random delay time in seconds")),
			InputPortConfig<float>("MaxDelay", 2.0f,                                         _HELP("Maximum random delay time in seconds")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_AnyType("Out"),
			{ 0 }
		};

		config.sDescription = _HELP("This node will delay passing the signal from the [In] to [Out] for a random amount of time in the interval [MinDelay,MaxDelay]");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

protected:
	/* virtual */ float GetDelayTime(SActivationInfo* pActInfo) const
	{
		return cry_random(GetPortFloat(pActInfo, 1), GetPortFloat(pActInfo, 2));
	}

	bool GetShouldReset(SActivationInfo* pActInfo)
	{
		return false;
	}

};

class CFlowFrameDelayNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowFrameDelayNode(SActivationInfo* pActInfo) : m_frameStamp(0) {}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowFrameDelayNode(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("in", _HELP("Value to be passed after one frame delay"), _HELP("In")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_AnyType("out", _HELP("Out")),
			{ 0 }
		};

		config.sDescription = _HELP("This node will delay passing the signal from [In] to [Out] for one frame");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				m_InputData = GetPortAny(pActInfo, 0);
				m_frameStamp = gEnv->nMainFrameID;
			}
			break;
		case eFE_Update:
			if (m_frameStamp != gEnv->nMainFrameID - 1)
			{
				CryLogAlways("Frame delay node didn't skip 1 frame - activated frame %d, updated frame %d", m_frameStamp, gEnv->nMainFrameID);
			}
			ActivateOutput(pActInfo, 0, m_InputData);
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		}
	}
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	TFlowInputData m_InputData;
	int            m_frameStamp;
};

REGISTER_FLOW_NODE("Time:Delay", CFlowDelayNode);
REGISTER_FLOW_NODE("Time:RandomDelay", CFlowRandomDelayNode);
REGISTER_FLOW_NODE("Time:FrameDelay", CFlowFrameDelayNode);
