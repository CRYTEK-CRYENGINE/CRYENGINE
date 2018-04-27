// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioAreaAmbienceEntity.h"
#include "AudioEntitiesUtils.h"
#include <CryAnimation/ICryAnimation.h>
#include <CrySerialization/Enum.h>

class CAudioAreaAmbienceRegistrator
	: public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("AudioAreaAmbience") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class AudioAreaAmbience, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CAudioAreaAmbienceEntity>("AudioAreaAmbience", "Audio", "AudioAreaAmbience.bmp");
	}
};

CAudioAreaAmbienceRegistrator g_audioAreaAmbienceRegistrator;

CRYREGISTER_CLASS(CAudioAreaAmbienceEntity);

void CAudioAreaAmbienceEntity::ProcessEvent(const SEntityEvent& event)
{
	if (gEnv->IsDedicated())
		return;

	CDesignerEntityComponent::ProcessEvent(event);

	switch (event.event)
	{
	case ENTITY_EVENT_ENTERNEARAREA:
		{
			m_areaState = EAreaState::Near;

			const float distance = event.fParam[0];
			if (distance < m_rtpcDistance)
			{
				Play(m_playTriggerId);
				UpdateRtpc(0.f);
			}
		}
		break;
	case ENTITY_EVENT_MOVENEARAREA:
		{
			m_areaState = EAreaState::Near;

			const float distance = event.fParam[0];
			if (distance < m_rtpcDistance)
			{
				if (!IsPlaying())
				{
					Play(m_playTriggerId);
				}

				UpdateFadeValue(distance);
			}
			else if (IsPlaying() && distance > m_rtpcDistance)
			{
				Stop();
				UpdateFadeValue(distance);
			}
		}
		break;
	case ENTITY_EVENT_ENTERAREA:
		{
			if (m_areaState == EAreaState::Outside)
			{
				Play(m_playTriggerId);
			}

			m_areaState = EAreaState::Inside;

			UpdateRtpc(1.f);
		}
		break;
	case ENTITY_EVENT_MOVEINSIDEAREA:
		{
			const float distance = event.fParam[0];

			if ((abs(m_fadeValue - distance) > AudioEntitiesUtils::AreaFadeEpsilon) || (distance == 0.f && m_fadeValue != distance))
			{
				UpdateRtpc(distance);

				if (!IsPlaying() && distance > 0.f)
				{
					Play(m_playTriggerId);
				}
				else if (IsPlaying() && distance == 0.f)
				{
					Stop();
				}
			}

			m_areaState = EAreaState::Inside;
		}
		break;
	case ENTITY_EVENT_LEAVEAREA:
		{
			m_areaState = EAreaState::Outside;
		}
		break;
	case ENTITY_EVENT_LEAVENEARAREA:
		{
			m_areaState = EAreaState::Outside;

			if (IsPlaying())
			{
				Stop();
			}

			UpdateRtpc(0.f);
		}
		break;
	}
}

void CAudioAreaAmbienceEntity::OnResetState()
{
	IEntity& entity = *GetEntity();

	if (!m_bEnabled)
	{
		if (auto pAudioProxy = entity.GetComponent<IEntityAudioComponent>())
		{
			pAudioProxy->SetEnvironmentId(CryAudio::InvalidEnvironmentId);

			Stop();
		}

		return;
	}

	auto& audioProxy = *(entity.GetOrCreateComponent<IEntityAudioComponent>());

	m_playTriggerId = CryAudio::StringToId(m_playTriggerName.c_str());
	m_stopTriggerId = CryAudio::StringToId(m_stopTriggerName.c_str());
	m_rtpcId = CryAudio::StringToId(m_rtpcName.c_str());
	m_globalRtpcId = CryAudio::StringToId(m_globalRtpcName.c_str());
	m_environmentId = CryAudio::StringToId(m_environmentName.c_str());

	const auto& stateIds = AudioEntitiesUtils::GetObstructionOcclusionStateIds();
	audioProxy.SetSwitchState(AudioEntitiesUtils::GetObstructionOcclusionSwitch(), stateIds[IntegralValue(m_occlusionType)]);

	audioProxy.SetFadeDistance(m_rtpcDistance);
	audioProxy.SetEnvironmentFadeDistance(m_environmentDistance);
	audioProxy.SetEnvironmentId(m_environmentId);

	auto entityFlags = entity.GetFlags() | ENTITY_FLAG_VOLUME_SOUND | ENTITY_FLAG_CLIENT_ONLY;

	if (m_bTriggerAreasOnMove)
	{
		entityFlags |= ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE;
		entity.SetFlagsExtended(entity.GetFlagsExtended() | ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE);
	}
	else
	{
		entityFlags &= ~ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE;
		entity.SetFlagsExtended(entity.GetFlagsExtended() & ~ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE);
	}

	entity.SetFlags(entityFlags);

	if (m_playingTriggerId != m_playTriggerId)
	{
		Play(m_playTriggerId);
	}
}

void CAudioAreaAmbienceEntity::Play(CryAudio::ControlId triggerId)
{
	if (auto pAudioProxy = GetEntity()->GetComponent<IEntityAudioComponent>())
	{
		if (m_playingTriggerId != CryAudio::InvalidControlId)
		{
			pAudioProxy->StopTrigger(m_playingTriggerId);
		}

		pAudioProxy->SetCurrentEnvironments();
		pAudioProxy->ExecuteTrigger(triggerId);

		m_playingTriggerId = triggerId;
	}
}

void CAudioAreaAmbienceEntity::Stop()
{
	auto pAudioProxy = GetEntity()->GetComponent<IEntityAudioComponent>();
	if (pAudioProxy == nullptr)
		return;

	if (m_stopTriggerId != CryAudio::InvalidControlId)
	{
		pAudioProxy->ExecuteTrigger(m_stopTriggerId);
	}
	else if (m_playTriggerId != CryAudio::InvalidControlId)
	{
		pAudioProxy->StopTrigger(m_playingTriggerId);
	}

	m_playingTriggerId = CryAudio::InvalidControlId;
}

void CAudioAreaAmbienceEntity::UpdateRtpc(float fadeValue)
{
	auto pAudioProxy = GetEntity()->GetComponent<IEntityAudioComponent>();
	if (pAudioProxy == nullptr)
		return;

	if (m_rtpcId != CryAudio::InvalidControlId)
	{
		pAudioProxy->SetParameter(m_rtpcId, fadeValue);
	}

	if (m_globalRtpcId != CryAudio::InvalidControlId)
	{
		gEnv->pAudioSystem->SetParameter(m_globalRtpcId, fadeValue);
	}

	m_fadeValue = fadeValue;
}

void CAudioAreaAmbienceEntity::UpdateFadeValue(float distance)
{
	if (m_rtpcDistance > 0.f)
	{
		float fade = max((m_rtpcDistance - distance) / m_rtpcDistance, 0.f);

		if (abs(m_fadeValue - fade) > AudioEntitiesUtils::AreaFadeEpsilon)
		{
			UpdateRtpc(fade);
		}
	}
}

void CAudioAreaAmbienceEntity::SerializeProperties(Serialization::IArchive& archive)
{
	archive(m_bEnabled, "Enabled", "Enabled");
	archive(Serialization::AudioTrigger(m_playTriggerName), "PlayTrigger", "PlayTrigger");
	archive(Serialization::AudioTrigger(m_stopTriggerName), "StopTrigger", "StopTrigger");
	archive(m_bTriggerAreasOnMove, "TriggerAreasOnMove", "TriggerAreasOnMove");

	archive(Serialization::AudioRTPC(m_rtpcName), "Rtpc", "Rtpc");
	archive(Serialization::AudioRTPC(m_globalRtpcName), "GlobalRtpc", "GlobalRtpc");
	archive(m_rtpcDistance, "RtpcDistance", "RtpcDistance");

	archive(Serialization::AudioEnvironment(m_environmentName), "Environment", "Environment");
	archive(m_environmentDistance, "EnvironmentDistance", "EnvironmentDistance");

	archive(m_occlusionType, "OcclusionType", "Occlusion Type");

	if (archive.isInput())
	{
		OnResetState();
	}
}
