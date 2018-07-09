// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"
#include "AudioEntitiesUtils.h"

#include <CryAudio/IAudioInterfacesCommonData.h>

#include <CrySerialization/Decorators/ResourcesAudio.h>
#include <CrySerialization/Math.h>

enum EDrawActivityRadius
{
	eDrawActivityRadius_Disabled = 0,
	eDrawActivityRadius_PlayTrigger,
	eDrawActivityRadius_StopTrigger,
};

class CAudioTriggerSpotEntity final
	: public CDesignerEntityComponent<>
	  , public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CAudioTriggerSpotEntity, "AudioTriggerSpot", "1009fa03-153c-459c-883d-33b33298813d"_cry_guid);

public:
	CAudioTriggerSpotEntity();
	virtual ~CAudioTriggerSpotEntity();

	// CDesignerEntityComponent
	virtual void                  ProcessEvent(const SEntityEvent& event) override;
	virtual uint64                GetEventMask() const override { return CDesignerEntityComponent::GetEventMask() | ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE) | ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER); }

	virtual IEntityPropertyGroup* GetPropertyGroup() final      { return this; }

	virtual void                  OnResetState() override;
	// ~CDesignerEntityComponent

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "AudioTriggerSpot Properties"; }

	virtual void        SerializeProperties(Serialization::IArchive& archive) override
	{
		archive(m_bEnabled, "Enabled", "Enabled");
		archive(Serialization::AudioTrigger(m_playTriggerName), "PlayTrigger", "PlayTrigger");
		archive(Serialization::AudioTrigger(m_stopTriggerName), "StopTrigger", "StopTrigger");
		archive(m_bTriggerAreasOnMove, "TriggerAreasOnMove", "TriggerAreasOnMove");

		archive(m_occlusionType, "OcclusionType", "Occlusion Type");

		if (archive.openBlock("PlayMode", "Play Mode"))
		{
			archive(m_behavior, "Behavior", "Behavior");
			archive(m_minDelay, "MinDelay", "MinDelay");
			archive(m_maxDelay, "MaxDelay", "MaxDelay");
			archive(m_randomizationArea, "RandomizationArea", "RandomizationArea");

			archive.closeBlock();
		}

		if (archive.openBlock("Debug", "Debug"))
		{
			archive(m_drawActivityRadius, "DrawActivityRadius", "DrawActivityRadius");
			archive(m_bDrawRandomizationArea, "DrawRandomizationArea", "DrawRandomizationArea");

			archive.closeBlock();
		}

		if (archive.isInput())
		{
			OnResetState();
		}
	}
	// ~IEntityPropertyGroup

protected:
	void        Stop();
	void        Play();
	void        StartPlayingBehaviour();
	Vec3        GenerateOffset();
	static void OnAudioTriggerFinished(CryAudio::SRequestInfo const* const pAudioRequestInfo);
	void        TriggerFinished(const CryAudio::ControlId trigger);

	void        DebugDraw();

	CryAudio::ControlId m_playTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_stopTriggerId = CryAudio::InvalidControlId;

	CryAudio::ControlId m_currentlyPlayingTriggerId = CryAudio::InvalidControlId;

	EPlayBehavior       m_currentBehavior = ePlayBehavior_Single;

protected:
	bool                     m_bEnabled = true;
	bool                     m_bTriggerAreasOnMove = false;

	string                   m_playTriggerName;
	string                   m_stopTriggerName;

	CryAudio::EOcclusionType m_occlusionType = CryAudio::EOcclusionType::Ignore;

	EPlayBehavior            m_behavior = ePlayBehavior_Single;

	float                    m_minDelay = 0.0f;
	float                    m_maxDelay = 1000.0f;

	Vec3                     m_randomizationArea = Vec3(ZERO);
	EDrawActivityRadius      m_drawActivityRadius = eDrawActivityRadius_Disabled;
	bool                     m_bDrawRandomizationArea = false;
};
