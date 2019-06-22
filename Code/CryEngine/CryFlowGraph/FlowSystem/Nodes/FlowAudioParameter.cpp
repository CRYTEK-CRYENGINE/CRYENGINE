// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_AudioParameter final : public CFlowBaseNode<eNCT_Instanced>
{
public:

	explicit CFlowNode_AudioParameter(SActivationInfo* pActInfo)
		: m_parameterId(CryAudio::InvalidControlId)
		, m_value(0.0f)
	{}

	virtual ~CFlowNode_AudioParameter() override = default;

	CFlowNode_AudioParameter(CFlowNode_AudioParameter const&) = delete;
	CFlowNode_AudioParameter(CFlowNode_AudioParameter&&) = delete;
	CFlowNode_AudioParameter& operator=(CFlowNode_AudioParameter const&) = delete;
	CFlowNode_AudioParameter& operator=(CFlowNode_AudioParameter&&) = delete;

	//////////////////////////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override
	{
		return new CFlowNode_AudioParameter(pActInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	enum INPUTS
	{
		eIn_ParameterName,
		eIn_ParameterValue,
	};

	enum OUTPUTS
	{
	};

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<string>("audioParameter_Name", _HELP("Parameter name"),  "Name"),
			InputPortConfig<float>("value",                _HELP("Parameter value"), "Value"),
			{ 0 } };

		static const SOutputPortConfig outputs[] =
		{
			{ 0 } };

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("This node sets parameter values.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override
	{
		float fValue = m_value;
		ser.BeginGroup("FlowAudioParameterNode");
		ser.Value("value", fValue);
		ser.EndGroup();

		if (ser.IsReading())
		{
			Init(pActInfo);
			m_value = fValue;
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
				if (IsPortActive(pActInfo, eIn_ParameterName))
				{
					GetParameterId(pActInfo);
				}

				if ((IsPortActive(pActInfo, eIn_ParameterValue)) && (m_parameterId != CryAudio::InvalidControlId))
				{
					SetValue(pActInfo->pEntity, GetPortFloat(pActInfo, eIn_ParameterValue));
				}

				break;
			}
		default:
			{
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
	void GetParameterId(SActivationInfo* const pActInfo)
	{
		string const& parameterName = GetPortString(pActInfo, eIn_ParameterName);

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
			GetParameterId(pActInfo);
		}
	}

	CryAudio::ControlId m_parameterId;
	float               m_value;
};

REGISTER_FLOW_NODE("Audio:Parameter", CFlowNode_AudioParameter);
