// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_AudioRtpc final : public CFlowBaseNode<eNCT_Instanced>
{
public:

	explicit CFlowNode_AudioRtpc(SActivationInfo* pActInfo)
		: m_parameterId(CryAudio::InvalidControlId)
		, m_value(0.0f)
	{}

	virtual ~CFlowNode_AudioRtpc() override = default;

	CFlowNode_AudioRtpc(CFlowNode_AudioRtpc const&) = delete;
	CFlowNode_AudioRtpc(CFlowNode_AudioRtpc&&) = delete;
	CFlowNode_AudioRtpc& operator=(CFlowNode_AudioRtpc const&) = delete;
	CFlowNode_AudioRtpc& operator=(CFlowNode_AudioRtpc&&) = delete;

	//////////////////////////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_AudioRtpc(pActInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	enum INPUTS
	{
		eIn_RtpcName,
		eIn_RtpcValue,
	};

	enum OUTPUTS
	{
	};

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<string>("audioRTPC_Name", _HELP("RTPC name"),  "Name"),
			InputPortConfig<float>("value",           _HELP("RTPC value"), "Value"),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("This node sets RTPC values.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override
	{
		float fValue = m_value;
		ser.BeginGroup("FlowAudioRtpcNode");
		ser.Value("value", fValue);
		ser.EndGroup();

		if (ser.IsReading())
		{
			SetValue(pActInfo->pEntity, fValue);
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
				if (IsPortActive(pActInfo, eIn_RtpcValue))
				{
					SetValue(pActInfo->pEntity, GetPortFloat(pActInfo, eIn_RtpcValue));
				}

				if (IsPortActive(pActInfo, eIn_RtpcName))
				{
					GetRtpcId(pActInfo);
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

	//////////////////////////////////////////////////////////////////////////
	void GetRtpcId(SActivationInfo* const pActInfo)
	{
		string const& parameterName = GetPortString(pActInfo, eIn_RtpcName);

		if (!parameterName.empty())
		{
			m_parameterId = CryAudio::StringToId(parameterName.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SetValue(IEntity* const pIEntity, float const value)
	{
		// Don't optimize here!
		// We always need to set the value as it could have been manipulated by another entity.
		m_value = value;

		if (pIEntity != nullptr)
		{
			IEntityAudioComponent* const pIEntityAudioComponent = pIEntity->GetOrCreateComponent<IEntityAudioComponent>();

			if (pIEntityAudioComponent != nullptr)
			{
				pIEntityAudioComponent->SetParameter(m_parameterId, m_value);
			}
		}
		else
		{
			gEnv->pAudioSystem->SetParameter(m_parameterId, m_value);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Init(SActivationInfo* const pActInfo)
	{
		if (gEnv->pAudioSystem != nullptr)
		{
			GetRtpcId(pActInfo);
			SetValue(pActInfo->pEntity, 0.0f);
		}
	}

	CryAudio::ControlId        m_parameterId;
	float                      m_value;
	CryAudio::SRequestUserData m_requestUserData;
};

REGISTER_FLOW_NODE("Audio:Rtpc", CFlowNode_AudioRtpc);
