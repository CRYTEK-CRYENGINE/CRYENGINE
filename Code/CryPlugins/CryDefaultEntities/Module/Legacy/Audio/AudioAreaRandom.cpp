// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioAreaRandom.h"
#include <CrySerialization/Enum.h>
#include <CryMath/Random.h>

class CAudioAreaRandomRegistrator : public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("AudioAreaRandom") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class AudioAreaRandom, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CAudioAreaRandom>("AudioAreaRandom", "Audio", "AudioAreaRandom.bmp");
		auto* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AudioAreaRandom");

		pEntityClass->SetFlags(pEntityClass->GetFlags() | ECLF_INVISIBLE);
	}
};

CAudioAreaRandomRegistrator g_audioAreaRandomRegistrator;

CRYREGISTER_CLASS(CAudioAreaRandom);

void CAudioAreaRandom::ProcessEvent(const SEntityEvent& event)
{
	if (gEnv->IsDedicated())
		return;

	switch (event.event)
	{
	case ENTITY_EVENT_ENTERNEARAREA:
		{
			if (m_areaState == EAreaState::Outside)
			{
				m_areaState = EAreaState::Near;

				const float distance = event.fParam[0];
				if (distance < m_parameterDistance)
				{
					Play();
					m_fadeValue = 0.0;
					UpdateRtpc();
				}
			}
		}
		break;

	case ENTITY_EVENT_MOVENEARAREA:
		{
			m_areaState = EAreaState::Near;

			const float distance = event.fParam[0];
			if (distance < m_parameterDistance)
			{
				if (!IsPlaying())
				{
					Play();
				}

				UpdateFadeValue(distance);
			}
			else if (IsPlaying() && distance > m_parameterDistance)
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
				Play();
			}

			m_areaState = EAreaState::Inside;

			m_fadeValue = 1.0f;
			UpdateRtpc();
		}
		break;

	case ENTITY_EVENT_MOVEINSIDEAREA:
		{
			const float distance = event.fParam[0];

			if ((abs(m_fadeValue - distance) > AudioEntitiesUtils::AreaFadeEpsilon) || (distance == 0.f && m_fadeValue != distance))
			{
				m_fadeValue = distance;
				UpdateRtpc();

				if (!IsPlaying() && distance > 0.f)
				{
					Play();
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
			m_areaState = EAreaState::Near;
		}
		break;

	case ENTITY_EVENT_LEAVENEARAREA:
		{
			m_areaState = EAreaState::Outside;

			if (IsPlaying())
			{
				Stop();
			}

			m_fadeValue = 0.0f;
			UpdateRtpc();
		}
		break;

	case ENTITY_EVENT_TIMER:
		{
			if (event.nParam[0] == m_timerId)
			{
				Play();
			}

		}
		break;

	}
}

void CAudioAreaRandom::OnResetState()
{
	IEntity& entity = *GetEntity();

	auto& audioEntityComponent = *(entity.GetOrCreateComponent<IEntityAudioComponent>());

	// Get properties
	m_playTriggerId = CryAudio::StringToId(m_playTriggerName.c_str());
	m_stopTriggerId = CryAudio::StringToId(m_stopTriggerName.c_str());
	m_parameterId = CryAudio::StringToId(m_parameterName.c_str());

	// Update values
	audioEntityComponent.SetFadeDistance(m_parameterDistance);
	const auto& stateIds = AudioEntitiesUtils::GetObstructionOcclusionStateIds();
	audioEntityComponent.SetSwitchState(AudioEntitiesUtils::GetObstructionOcclusionSwitch(), stateIds[IntegralValue(m_occlusionType)]);

	audioEntityComponent.SetCurrentEnvironments(CryAudio::InvalidAuxObjectId);
	audioEntityComponent.SetAudioAuxObjectOffset(Matrix34(IDENTITY));
	audioEntityComponent.AudioAuxObjectsMoveWithEntity(m_bMoveWithEntity);

	entity.SetFlags(entity.GetFlags() | ENTITY_FLAG_CLIENT_ONLY);
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
		if (!m_bPlaying)
		{
			// Entity was enabled
			entity.KillTimer(m_timerId);
			m_timerId = 0;

			Play();
		}
	}
	else if (m_bPlaying)
	{
		// Entity was disabled
		Stop();
	}
}

Vec3 CAudioAreaRandom::GenerateOffset()
{
	Vec3 offset = { 0, 0, 0 };
	offset.x = cry_random(-1.0f, 1.0f);
	offset.y = cry_random(-1.0f, 1.0f);
	offset *= m_radius;
	return offset;
}

void CAudioAreaRandom::Play()
{
	if (auto pIEntityAudioComponent = GetEntity()->GetComponent<IEntityAudioComponent>())
	{
		if (m_currentlyPlayingTriggerId != CryAudio::InvalidControlId && m_playTriggerId != m_currentlyPlayingTriggerId)
		{
			pIEntityAudioComponent->StopTrigger(m_currentlyPlayingTriggerId);
		}

		if (m_playTriggerId != CryAudio::InvalidControlId)
		{
			pIEntityAudioComponent->SetCurrentEnvironments();
			pIEntityAudioComponent->SetAudioAuxObjectOffset(Matrix34(IDENTITY, GenerateOffset()));

			CryAudio::SRequestUserData const userData(CryAudio::ERequestFlags::None, this);
			pIEntityAudioComponent->ExecuteTrigger(m_playTriggerId, CryAudio::DefaultAuxObjectId, userData);
		}

		m_currentlyPlayingTriggerId = m_playTriggerId;

		GetEntity()->SetTimer(m_timerId, static_cast<int>(cry_random(m_minDelay, m_maxDelay)));
		m_bPlaying = true;
	}
}

void CAudioAreaRandom::Stop()
{
	IEntity& entity = *GetEntity();
	entity.KillTimer(m_timerId);

	if (auto pAudioProxy = entity.GetComponent<IEntityAudioComponent>())
	{
		if (m_stopTriggerId != CryAudio::InvalidControlId)
		{
			pAudioProxy->ExecuteTrigger(m_stopTriggerId);
		}
		else if (m_currentlyPlayingTriggerId != CryAudio::InvalidControlId)
		{
			pAudioProxy->StopTrigger(m_currentlyPlayingTriggerId);
		}

		m_currentlyPlayingTriggerId = CryAudio::InvalidControlId;
	}

	m_bPlaying = false;
}

void CAudioAreaRandom::UpdateRtpc()
{
	if (m_parameterId != CryAudio::InvalidControlId)
	{
		if (auto pAudioProxy = GetEntity()->GetComponent<IEntityAudioComponent>())
		{
			pAudioProxy->SetParameter(m_parameterId, m_fadeValue);
		}
	}
}

void CAudioAreaRandom::UpdateFadeValue(float distance)
{
	if (!m_bEnabled)
	{
		m_fadeValue = 0.0f;
		UpdateRtpc();
	}
	else if (m_parameterDistance > 0.0f)
	{
		float fade = std::max((m_parameterDistance - distance) / m_parameterDistance, 0.f);
		fade = (fade > 0.0f) ? fade : 0.0f;
		if (abs(m_fadeValue - fade) > AudioEntitiesUtils::AreaFadeEpsilon)
		{
			m_fadeValue = fade;
			UpdateRtpc();
		}
	}
}

void CAudioAreaRandom::SerializeProperties(Serialization::IArchive& archive)
{
	archive(m_bEnabled, "Enabled", "Enabled");
	archive(Serialization::AudioTrigger(m_playTriggerName), "PlayTrigger", "PlayTrigger");
	archive(Serialization::AudioTrigger(m_stopTriggerName), "StopTrigger", "StopTrigger");
	archive(m_bTriggerAreasOnMove, "TriggerAreasOnMove", "TriggerAreasOnMove");
	archive(m_bMoveWithEntity, "MoveWithEntity", "Move with Entity");

	archive(m_occlusionType, "OcclusionType", "Occlusion Type");

	archive(Serialization::AudioRTPC(m_parameterName), "Rtpc", "Rtpc");
	archive(m_parameterDistance, "RTPCDistance", "RTPC Distance");
	archive(m_radius, "RadiusRandom", "Radius Random");

	archive(m_minDelay, "MinDelay", "MinDelay");
	archive(m_maxDelay, "MaxDelay", "MaxDelay");

	if (archive.isInput())
	{
		OnResetState();
	}
}
