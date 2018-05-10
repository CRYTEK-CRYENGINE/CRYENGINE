// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioAreaEntity.h"
#include "Legacy/Helpers/EntityFlowNode.h"
#include <CrySerialization/Enum.h>

class CAudioAreaEntityRegistrator : public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("AudioAreaEntity") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class AudioAreaEntity, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CAudioAreaEntity>("AudioAreaEntity", "Audio", "AudioAreaEntity.bmp");
		gEnv->pEntitySystem->GetClassRegistry()->FindClass("AudioAreaEntity");

		// Register flow node
		// Factory will be destroyed by flowsystem during shutdown
		pFlowNodeFactory = new CEntityFlowNodeFactory("entity:AudioAreaEntity");

		pFlowNodeFactory->m_inputs.push_back(InputPortConfig<bool>("Enable", ""));
		pFlowNodeFactory->m_inputs.push_back(InputPortConfig<bool>("Disable", ""));

		pFlowNodeFactory->m_activateCallback = CAudioAreaEntity::OnFlowgraphActivation;

		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<float>("FadeValue"));
		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<bool>("OnFarToNear"));
		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<bool>("OnNearToInside"));
		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<bool>("OnInsideToNear"));
		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<bool>("OnNearToFar"));

		pFlowNodeFactory->Close();

	}

public:
	~CAudioAreaEntityRegistrator()
	{
		if (pFlowNodeFactory)
			pFlowNodeFactory->UnregisterFactory();
		pFlowNodeFactory = nullptr;
	}

protected:
	_smart_ptr<CEntityFlowNodeFactory> pFlowNodeFactory = nullptr;
};

CAudioAreaEntityRegistrator g_audioAreaEntityRegistrator;

CRYREGISTER_CLASS(CAudioAreaEntity);

void CAudioAreaEntity::ProcessEvent(const SEntityEvent& event)
{
	if (gEnv->IsDedicated())
		return;

	CDesignerEntityComponent::ProcessEvent(event);

	switch (event.event)
	{
	case ENTITY_EVENT_ENTERNEARAREA:
		if (m_areaState == EAreaState::Outside)
		{
			const float distance = event.fParam[0];
			if (distance < m_fadeDistance)
			{
				m_bIsActive = true;
				m_fadeValue = 0.0f;
				ActivateFlowNodeOutput(eOutputPorts_OnFarToNear, TFlowInputData(true));
				ActivateFlowNodeOutput(eOutputPorts_FadeValue, TFlowInputData(m_fadeValue));
			}
		}
		else if (m_areaState == EAreaState::Inside)
		{
			ActivateFlowNodeOutput(eOutputPorts_OnInsideToNear, TFlowInputData(true));
			UpdateFadeValue(event.fParam[0]);
		}
		m_areaState = EAreaState::Near;
		break;

	case ENTITY_EVENT_MOVENEARAREA:
		{
			m_areaState = EAreaState::Near;
			const float distance = event.fParam[0];
			if (!m_bIsActive && distance < m_fadeDistance)
			{
				m_bIsActive = true;
				ActivateFlowNodeOutput(eOutputPorts_OnFarToNear, TFlowInputData(true));
			}
			else if (m_bIsActive && distance > m_fadeDistance)
			{
				m_bIsActive = false;
				ActivateFlowNodeOutput(eOutputPorts_OnNearToFar, TFlowInputData(true));
			}

			UpdateFadeValue(distance);

		}
		break;

	case ENTITY_EVENT_ENTERAREA:
		if (m_areaState == EAreaState::Outside)
		{
			// possible if the player is teleported or gets spawned inside the area
			// technically, the listener enters the Near Area and the Inside Area at the same time
			m_bIsActive = true;
			ActivateFlowNodeOutput(eOutputPorts_OnFarToNear, TFlowInputData(true));
		}

		m_areaState = EAreaState::Inside;
		m_fadeValue = 1.0f;
		ActivateFlowNodeOutput(eOutputPorts_OnNearToInside, TFlowInputData(true));
		ActivateFlowNodeOutput(eOutputPorts_FadeValue, TFlowInputData(m_fadeValue));
		break;

	case ENTITY_EVENT_MOVEINSIDEAREA:
		{
			m_areaState = EAreaState::Inside;
			const float fade = event.fParam[0];
			if ((fabsf(m_fadeValue - fade) > AudioEntitiesUtils::AreaFadeEpsilon) || ((fade == 0.0) && (m_fadeValue != fade)))
			{
				m_fadeValue = fade;
				ActivateFlowNodeOutput(eOutputPorts_FadeValue, TFlowInputData(m_fadeValue));

				if (!m_bIsActive && (fade > 0.0))
				{
					m_bIsActive = true;
					ActivateFlowNodeOutput(eOutputPorts_OnFarToNear, TFlowInputData(true));
					ActivateFlowNodeOutput(eOutputPorts_OnNearToInside, TFlowInputData(true));
				}
				else if (m_bIsActive && fade == 0.0)
				{
					ActivateFlowNodeOutput(eOutputPorts_OnInsideToNear, TFlowInputData(true));
					ActivateFlowNodeOutput(eOutputPorts_OnNearToFar, TFlowInputData(true));
					m_bIsActive = false;
				}
			}
		}
		break;

	case ENTITY_EVENT_LEAVEAREA:
		m_areaState = EAreaState::Near;
		ActivateFlowNodeOutput(eOutputPorts_OnInsideToNear, TFlowInputData(true));
		break;

	case ENTITY_EVENT_LEAVENEARAREA:
		m_areaState = EAreaState::Outside;
		m_fadeValue = 0.0f;
		if (m_bIsActive)
		{
			ActivateFlowNodeOutput(eOutputPorts_OnNearToFar, TFlowInputData(true));
			ActivateFlowNodeOutput(eOutputPorts_FadeValue, TFlowInputData(m_fadeValue));
			m_bIsActive = false;
		}
		break;

	}
}

void CAudioAreaEntity::OnResetState()
{
	IEntity& entity = *GetEntity();

	m_pProxy = entity.GetOrCreateComponent<IEntityAudioComponent>();

	// Get properties
	CryAudio::EnvironmentId const environmentId = CryAudio::StringToId(m_environmentName.c_str());

	const auto& stateIds = AudioEntitiesUtils::GetObstructionOcclusionStateIds();
	m_pProxy->SetSwitchState(AudioEntitiesUtils::GetObstructionOcclusionSwitch(), stateIds[IntegralValue(m_occlusionType)]);

	m_pProxy->SetFadeDistance(m_fadeDistance);
	m_pProxy->SetEnvironmentFadeDistance(m_environmentFadeDistance);

	entity.SetFlags(entity.GetFlags() | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_VOLUME_SOUND);
	if (m_bTriggerAreasOnMove)
	{
		entity.SetFlags(entity.GetFlags() | ENTITY_FLAG_TRIGGER_AREAS);
		entity.SetFlagsExtended(entity.GetFlagsExtended() | ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE);
	}
	else
	{
		entity.SetFlags(entity.GetFlags() & (~ENTITY_FLAG_TRIGGER_AREAS));
		entity.SetFlagsExtended(entity.GetFlagsExtended() & (~ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE));
	}

	if (m_bEnabled)
	{
		SetEnvironmentId(environmentId);
	}
	else
	{
		SetEnvironmentId(CryAudio::InvalidEnvironmentId);
	}
}

void CAudioAreaEntity::SetEnvironmentId(const CryAudio::ControlId environmentId)
{
	const CryAudio::EnvironmentId oldEnvironmentId = m_pProxy->GetEnvironmentId();
	m_pProxy->SetEnvironmentId(environmentId);

	//
	// TODO: The audio environment is being tampered with, we need to inform all entities affected by the area.
	//

}

void CAudioAreaEntity::UpdateFadeValue(const float distance)
{
	if (!m_bEnabled)
	{
		m_fadeValue = 0.0f;
		ActivateFlowNodeOutput(eOutputPorts_FadeValue, TFlowInputData(m_fadeValue));
	}
	else if (m_fadeDistance > 0.0f)
	{
		float fade = (m_fadeDistance - distance) / m_fadeDistance;
		fade = (fade > 0.0) ? fade : 0.0f;

		if (fabsf(m_fadeValue - fade) > AudioEntitiesUtils::AreaFadeEpsilon)
		{
			m_fadeValue = fade;
			ActivateFlowNodeOutput(eOutputPorts_FadeValue, TFlowInputData(m_fadeValue));
		}
	}
}

void CAudioAreaEntity::OnFlowgraphActivation(EntityId entityId, IFlowNode::SActivationInfo* pActInfo, const class CEntityFlowNode* pNode)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	auto* pAudioAreaEntity = pEntity->GetComponent<CAudioAreaEntity>();

	if (IsPortActive(pActInfo, eInputPorts_Enable))
	{
		pAudioAreaEntity->m_bEnabled = true;
	}
	else if (IsPortActive(pActInfo, eInputPorts_Disable))
	{
		pAudioAreaEntity->m_bEnabled = false;
	}

	pAudioAreaEntity->OnResetState();

}

void CAudioAreaEntity::SerializeProperties(Serialization::IArchive& archive)
{
	archive(m_bEnabled, "Enabled", "Enabled");
	archive(m_bTriggerAreasOnMove, "TriggerAreasOnMove", "TriggerAreasOnMove");

	archive(Serialization::AudioEnvironment(m_environmentName), "Environment", "Environment");
	archive(m_fadeDistance, "FadeDistance", "FadeDistance");
	archive(m_environmentFadeDistance, "EnvironmentDistance", "EnvironmentDistance");
	archive(m_occlusionType, "OcclusionType", "Occlusion Type");

	if (archive.isInput())
	{
		OnResetState();
	}
}
