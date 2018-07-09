// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioTriggerSpotEntity.h"
#include "AudioEntitiesUtils.h"
#include <CryMath/Random.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySerialization/Enum.h>

#define DELAY_TIMER_ID 0

class CAudioTriggerSpotRegistrator final : public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("AudioTriggerSpot") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class AudioTriggerSpot, overridden by game");
			return;
		}

		auto* pEntityClass = RegisterEntityWithDefaultComponent<CAudioTriggerSpotEntity>("AudioTriggerSpot", "Audio", "Sound.bmp");

		pEntityClass->SetFlags(pEntityClass->GetFlags() | ECLF_INVISIBLE);
	}
};

CAudioTriggerSpotRegistrator g_audioTriggerSpotRegistrator;

YASLI_ENUM_BEGIN(EDrawActivityRadius, "DrawActivityRadius")
YASLI_ENUM_VALUE(eDrawActivityRadius_Disabled, "Disabled")
YASLI_ENUM_VALUE(eDrawActivityRadius_PlayTrigger, "PlayTrigger")
YASLI_ENUM_VALUE(eDrawActivityRadius_StopTrigger, "StopTrigger")
YASLI_ENUM_END()

CRYREGISTER_CLASS(CAudioTriggerSpotEntity);

CAudioTriggerSpotEntity::CAudioTriggerSpotEntity()
{
	gEnv->pAudioSystem->AddRequestListener(&CAudioTriggerSpotEntity::OnAudioTriggerFinished, this, CryAudio::ESystemEvents::TriggerFinished);
}

CAudioTriggerSpotEntity::~CAudioTriggerSpotEntity()
{
	gEnv->pAudioSystem->RemoveRequestListener(&CAudioTriggerSpotEntity::OnAudioTriggerFinished, this);
	Stop();
}

void CAudioTriggerSpotEntity::ProcessEvent(const SEntityEvent& event)
{
	if (gEnv->IsDedicated())
		return;

	CDesignerEntityComponent::ProcessEvent(event);

	switch (event.event)
	{
	case ENTITY_EVENT_TIMER:
		{
			if (event.nParam[0] == DELAY_TIMER_ID)
			{
				Play();

				if (m_behavior == ePlayBehavior_TriggerRate)
				{
					GetEntity()->SetTimer(DELAY_TIMER_ID, static_cast<int>(cry_random(m_minDelay, m_maxDelay)));
				}
			}

		}
		break;
	case ENTITY_EVENT_UPDATE:
		{
			DebugDraw();
		}
		break;
	}
}

void CAudioTriggerSpotEntity::TriggerFinished(const CryAudio::ControlId trigger)
{
	// If in delay mode, set a timer to play again. Note that the play trigger
	// could have been changed  and this event refers the previous one finishing
	// playing, that instance we need to ignore.
	if (m_bEnabled && trigger == m_playTriggerId && m_behavior == ePlayBehavior_Delay)
	{
		GetEntity()->SetTimer(DELAY_TIMER_ID, static_cast<int>(cry_random(m_minDelay, m_maxDelay)));
	}
}

void CAudioTriggerSpotEntity::OnAudioTriggerFinished(CryAudio::SRequestInfo const* const pAudioRequestInfo)
{
	CAudioTriggerSpotEntity* pAudioTriggerSpot = static_cast<CAudioTriggerSpotEntity*>(pAudioRequestInfo->pOwner);
	pAudioTriggerSpot->TriggerFinished(pAudioRequestInfo->audioControlId);
}

void CAudioTriggerSpotEntity::OnResetState()
{
	IEntity& entity = *GetEntity();

	auto& audioProxy = *(entity.GetOrCreateComponent<IEntityAudioComponent>());

	// Get properties
	m_playTriggerId = CryAudio::StringToId(m_playTriggerName.c_str());
	m_stopTriggerId = CryAudio::StringToId(m_stopTriggerName.c_str());

	// Reset values to their default
	audioProxy.SetAudioAuxObjectOffset(Matrix34(IDENTITY));
	audioProxy.SetCurrentEnvironments(CryAudio::InvalidAuxObjectId);
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

	const auto& stateIds = AudioEntitiesUtils::GetObstructionOcclusionStateIds();
	audioProxy.SetSwitchState(AudioEntitiesUtils::GetObstructionOcclusionSwitch(), stateIds[IntegralValue(m_occlusionType)]);

	if (m_bEnabled)
	{
		if (!m_bEnabled || (m_playTriggerId != m_currentlyPlayingTriggerId) || (m_currentBehavior != m_behavior))
		{
			if (m_currentBehavior != m_behavior)
			{
				m_currentBehavior = m_behavior;

				// Have to stop all running instances if the behavior changes
				if (m_currentlyPlayingTriggerId != CryAudio::InvalidControlId)
				{
					audioProxy.StopTrigger(m_currentlyPlayingTriggerId);
				}
			}

			// Entity was enabled or an important property has changed
			StartPlayingBehaviour();
		}

	}
	else
	{
		// Entity was disabled
		Stop();
	}
}

void CAudioTriggerSpotEntity::StartPlayingBehaviour()
{
	IEntity& entity = *GetEntity();
	entity.KillTimer(DELAY_TIMER_ID);

	Play();

	if (m_behavior == ePlayBehavior_TriggerRate)
	{
		entity.SetTimer(DELAY_TIMER_ID, static_cast<int>(cry_random(m_minDelay, m_maxDelay)));
	}
}

void CAudioTriggerSpotEntity::Play()
{
	if (auto pAudioProxy = GetEntity()->GetComponent<IEntityAudioComponent>())
	{
		if (m_currentlyPlayingTriggerId != CryAudio::InvalidControlId && m_playTriggerId != m_currentlyPlayingTriggerId)
		{
			pAudioProxy->StopTrigger(m_currentlyPlayingTriggerId);
		}

		if (m_playTriggerId != CryAudio::InvalidControlId)
		{
			pAudioProxy->SetCurrentEnvironments();
			pAudioProxy->SetAudioAuxObjectOffset(Matrix34(IDENTITY, GenerateOffset()));

			CryAudio::SRequestUserData const userData(CryAudio::ERequestFlags::None, this);
			pAudioProxy->ExecuteTrigger(m_playTriggerId, CryAudio::DefaultAuxObjectId, userData);
		}

		m_currentlyPlayingTriggerId = m_playTriggerId;
	}
}

void CAudioTriggerSpotEntity::Stop()
{
	IEntity& entity = *GetEntity();
	entity.KillTimer(DELAY_TIMER_ID);

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
}

Vec3 CAudioTriggerSpotEntity::GenerateOffset()
{
	Vec3 offset = { 0, 0, 0 };
	offset.x = cry_random(-m_randomizationArea.x / 2.0f, m_randomizationArea.x / 2.0f);
	offset.y = cry_random(-m_randomizationArea.y / 2.0f, m_randomizationArea.y / 2.0f);
	offset.z = cry_random(-m_randomizationArea.z / 2.0f, m_randomizationArea.z / 2.0f);
	return offset;
}

void CAudioTriggerSpotEntity::DebugDraw()
{
#if !defined(_RELEASE)
	if (m_drawActivityRadius > eDrawActivityRadius_Disabled || m_bDrawRandomizationArea)
	{
		IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		if (pRenderAuxGeom)
		{
			const SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
			SAuxGeomRenderFlags newFlags = oldFlags;
			newFlags.SetCullMode(e_CullModeNone);
			newFlags.SetFillMode(e_FillModeWireframe);
			newFlags.SetAlphaBlendMode(e_AlphaBlended);
			pRenderAuxGeom->SetRenderFlags(newFlags);

			const Vec3 pos = GetEntity()->GetWorldPos();

			// Activity Radius
			if (m_drawActivityRadius > eDrawActivityRadius_Disabled)
			{
				const CryAudio::ControlId triggerId = m_drawActivityRadius == eDrawActivityRadius_PlayTrigger ? m_playTriggerId : m_stopTriggerId;
				CryAudio::STriggerData triggerData;
				gEnv->pAudioSystem->GetTriggerData(triggerId, triggerData);

				pRenderAuxGeom->DrawSphere(pos, triggerData.radius, ColorB(250, 100, 100, 100), false);
			}

			// Randomization Area
			if (m_bDrawRandomizationArea)
			{
				newFlags.SetFillMode(e_FillModeSolid);
				pRenderAuxGeom->SetRenderFlags(newFlags);

				const AABB bbox(-m_randomizationArea * 0.5f, m_randomizationArea * 0.5f);
				const OBB obb = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(GetEntity()->GetWorldAngles()), bbox);
				pRenderAuxGeom->DrawOBB(obb, pos, true, ColorB(255, 128, 128, 128), eBBD_Faceted);
			}

			pRenderAuxGeom->SetRenderFlags(oldFlags);
		}
	}
#endif
}
