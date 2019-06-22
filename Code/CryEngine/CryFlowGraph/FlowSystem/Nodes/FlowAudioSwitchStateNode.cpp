// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_AudioSwitchState final : public CFlowBaseNode<eNCT_Instanced>
{
public:

	explicit CFlowNode_AudioSwitchState(SActivationInfo* pActInfo)
		: m_switchId(CryAudio::InvalidControlId)
		, m_stateId(CryAudio::InvalidControlId)
	{}

	//////////////////////////////////////////////////////////////////////////
	virtual ~CFlowNode_AudioSwitchState() override = default;

	CFlowNode_AudioSwitchState(CFlowNode_AudioSwitchState const&) = delete;
	CFlowNode_AudioSwitchState(CFlowNode_AudioSwitchState&&) = delete;
	CFlowNode_AudioSwitchState& operator=(CFlowNode_AudioSwitchState const&) = delete;
	CFlowNode_AudioSwitchState& operator=(CFlowNode_AudioSwitchState&&) = delete;

	//////////////////////////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_AudioSwitchState(pActInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	enum INPUTS
	{
		eIn_SwitchStateName,
		eIn_SetSwitchState,
	};

	enum OUTPUTS
	{
	};

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<string>("audioSwitchAndState_Name", _HELP("Name of the Switch State"),                   "SwitchState"),

			InputPortConfig_Void("audioSwitchAndState_Set",     _HELP("Sets the switch to the corresponding state"), "Set"),
			{ 0 } };

		static const SOutputPortConfig outputs[] =
		{
			{ 0 } };

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("This node allows one to set an Audio SwitchState.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override
	{
		ser.BeginGroup("FlowAudioSwitchStateNode");
		ser.EndGroup();

		if (ser.IsReading())
		{
			Init(pActInfo);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				Init(pActInfo);

				break;
			}
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIn_SwitchStateName))
				{
					GetSwitchAndStateId(pActInfo);
				}

				if (IsPortActive(pActInfo, eIn_SetSwitchState))
				{
					SetSwitchState(pActInfo->pEntity);
				}

				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

private:

	static const int NUM_STATES = 4;

	//////////////////////////////////////////////////////////////////////////
	void GetSwitchAndStateId(SActivationInfo* const pActInfo)
	{
		string const& switchStateName = GetPortString(pActInfo, eIn_SwitchStateName);

		if (!switchStateName.empty())
		{
			int start = 0;
			string token = switchStateName.Tokenize(CryAudio::g_szSwitchStateSeparator, start);
			m_switchId = CryAudio::StringToId(token.c_str());

			token = switchStateName.Tokenize(CryAudio::g_szSwitchStateSeparator, start);
			m_stateId = CryAudio::StringToId(token.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Init(SActivationInfo* const pActInfo)
	{
		if (gEnv->pAudioSystem != nullptr)
		{
			GetSwitchAndStateId(pActInfo);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SetSwitchStateOnProxy(IEntity* const pIEntity)
	{
		IEntityAudioComponent* const pIEntityAudioComponent = pIEntity->GetOrCreateComponent<IEntityAudioComponent>();

		if (pIEntityAudioComponent != nullptr)
		{
			pIEntityAudioComponent->SetSwitchState(m_switchId, m_stateId);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SetSwitchStateOnGlobalObject()
	{
		gEnv->pAudioSystem->SetSwitchState(m_switchId, m_stateId);
	}

	//////////////////////////////////////////////////////////////////////////
	void SetSwitchState(IEntity* const pIEntity)
	{
		if ((m_switchId != CryAudio::InvalidControlId) && (m_stateId != CryAudio::InvalidControlId))
		{
			if (pIEntity != nullptr)
			{
				SetSwitchStateOnProxy(pIEntity);
			}
			else
			{
				SetSwitchStateOnGlobalObject();
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CryAudio::ControlId     m_switchId;
	CryAudio::SwitchStateId m_stateId;
};

REGISTER_FLOW_NODE("Audio:SwitchState", CFlowNode_AudioSwitchState);
