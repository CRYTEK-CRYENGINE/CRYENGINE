// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <queue>
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowDelayNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowDelayNode(SActivationInfo*);
	virtual ~CFlowDelayNode();

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo*);
	virtual void         GetConfiguration(SFlowNodeConfig&);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*);
	virtual void         Serialize(SActivationInfo*, TSerialize ser);

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
		s->AddObject(m_activations);
	}
	// ~IFlowNode

protected:
	// let derived classes decide how long to delay
	virtual float GetDelayTime(SActivationInfo*) const;
	virtual bool  GetShouldReset(SActivationInfo* pActInfo);

private:
	void RemovePendingTimers();

	enum
	{
		INP_IN = 0,
		INP_DELAY,
		INP_RESET_ON_EACH_INPUT
	};

	SActivationInfo m_actInfo;
	struct SDelayData
	{
		SDelayData() {}
		SDelayData(const CTimeValue& timeout, const TFlowInputData& data)
			: m_timeout(timeout), m_data(data) {}

		CTimeValue     m_timeout;
		TFlowInputData m_data;
		bool operator<(const SDelayData& rhs) const
		{
			return m_timeout < rhs.m_timeout;
		}

		void Serialize(TSerialize ser)
		{
			ser.Value("m_timeout", m_timeout);
			ser.Value("m_data", m_data);
		}

		void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
	};

	typedef std::map<IGameFramework::TimerID, SDelayData> Activations;
	Activations m_activations;
	static void OnTimer(void* pUserData, IGameFramework::TimerID ref);
};
