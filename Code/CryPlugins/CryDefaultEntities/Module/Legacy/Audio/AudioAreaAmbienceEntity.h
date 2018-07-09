// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AudioEntitiesUtils.h"
#include "Legacy/Helpers/DesignerEntityComponent.h"
#include <CrySerialization/Decorators/ResourcesAudio.h>

class CAudioAreaAmbienceEntity final
	: public CDesignerEntityComponent<>
	  , public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CAudioAreaAmbienceEntity, "AudioAreaAmbience", "66f9fdba-753b-4a3c-9772-30560c65881b"_cry_guid);

	virtual ~CAudioAreaAmbienceEntity() {}

public:
	// CDesignerEntityComponent
	virtual void                  ProcessEvent(const SEntityEvent& event) override;
	virtual uint64                GetEventMask() const override { return CDesignerEntityComponent::GetEventMask() | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERNEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVENEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVEINSIDEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVENEARAREA); }

	virtual IEntityPropertyGroup* GetPropertyGroup() final      { return this; }

	virtual void                  OnResetState() override;
	// ~CDesignerEntityComponent

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "AudioAreaAmbience Properties"; }

	virtual void        SerializeProperties(Serialization::IArchive& archive) override;
	// ~IEntityPropertyGroup

protected:
	void       Play(CryAudio::ControlId triggerId);
	void       Stop();

	void       UpdateRtpc(float fadeValue);
	void       UpdateFadeValue(float distance);

	const bool IsPlaying() const { return m_playingTriggerId != CryAudio::InvalidControlId; }

protected:
	CryAudio::ControlId m_playTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_stopTriggerId = CryAudio::InvalidControlId;

	// Id of the trigger we are currently executing
	CryAudio::ControlId m_playingTriggerId = CryAudio::InvalidControlId;

	CryAudio::ControlId m_rtpcId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_globalRtpcId = CryAudio::InvalidControlId;

	CryAudio::ControlId m_environmentId = CryAudio::InvalidControlId;

	EAreaState          m_areaState = EAreaState::Outside;
	float               m_fadeValue = 0.0f;

protected:
	bool                     m_bEnabled = true;
	bool                     m_bTriggerAreasOnMove = false;

	string                   m_playTriggerName;
	string                   m_stopTriggerName;

	string                   m_rtpcName;
	string                   m_globalRtpcName;

	float                    m_rtpcDistance = 5.f;

	string                   m_environmentName;
	float                    m_environmentDistance = 5.f;

	CryAudio::EOcclusionType m_occlusionType = CryAudio::EOcclusionType::Ignore;
};
