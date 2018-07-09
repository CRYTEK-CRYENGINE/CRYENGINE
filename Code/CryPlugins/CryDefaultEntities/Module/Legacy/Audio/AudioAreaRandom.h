// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AudioEntitiesUtils.h"
#include "Legacy/Helpers/DesignerEntityComponent.h"
#include <CrySerialization/Decorators/ResourcesAudio.h>

class CAudioAreaRandom final
	: public CDesignerEntityComponent<>
	  , public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CAudioAreaRandom, "AudioAreaRandom", "8952d4d6-3e23-47d5-86ec-4724bf34789f"_cry_guid);

public:
	CAudioAreaRandom() = default;
	virtual ~CAudioAreaRandom() {}

	// CNativeEntityBase
	virtual uint64                GetEventMask() const override { return CDesignerEntityComponent::GetEventMask() | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERNEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVENEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVEINSIDEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVENEARAREA); }
	virtual void                  ProcessEvent(const SEntityEvent& event) override;

	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }

	virtual void                  OnResetState() override;
	// ~CNativeEntityBase

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "AudioAreaRandom Properties"; }

	virtual void        SerializeProperties(Serialization::IArchive& archive) override;
	// ~IEntityPropertyGroup

protected:
	void       Play();
	void       Stop();
	Vec3       GenerateOffset();
	void       UpdateFadeValue(float distance);
	void       UpdateRtpc();
	const bool IsPlaying() const { return m_currentlyPlayingTriggerId != CryAudio::InvalidControlId; }

	CryAudio::ControlId      m_playTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId      m_stopTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId      m_parameterId = CryAudio::InvalidControlId;

	CryAudio::ControlId      m_currentlyPlayingTriggerId = CryAudio::InvalidControlId;

	string                   m_playTriggerName;
	string                   m_stopTriggerName;

	string                   m_parameterName;

	CryAudio::EOcclusionType m_occlusionType = CryAudio::EOcclusionType::Ignore;

	bool                     m_bTriggerAreasOnMove = false;
	bool                     m_bMoveWithEntity = false;

	float                    m_minDelay = 0.0f;
	float                    m_maxDelay = 1000.0f;
	float                    m_radius = 10.0f;
	float                    m_fadeValue = 0.0;
	bool                     m_bEnabled = true;
	EAreaState               m_areaState = EAreaState::Outside;
	float                    m_parameterDistance = 5.0f;

	bool                     m_bPlaying = false;
	int                      m_timerId = 0;
};
