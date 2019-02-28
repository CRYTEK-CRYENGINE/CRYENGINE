// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AudioEntitiesUtils.h"
#include "Legacy/Helpers/DesignerEntityComponent.h"
#include <CryFlowGraph/IFlowSystem.h>
#include <CrySerialization/Decorators/ResourcesAudio.h>

class CEntityFlowNode;

class CAudioAreaEntity final
	: public CDesignerEntityComponent<>
	  , public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CAudioAreaEntity, "AudioAreaEntity", "a8ec43da-c8b0-46a8-9418-2c1c8c56f2d1"_cry_guid);

public:
	enum EFlowgraphInputPorts
	{
		eInputPorts_Enable = 0,
		eInputPorts_Disable,
	};

	enum EFlowgraphOutputPorts
	{
		eOutputPorts_FadeValue,
		eOutputPorts_OnFarToNear,
		eOutputPorts_OnNearToInside,
		eOutputPorts_OnInsideToNear,
		eOutputPorts_OnNearToFar,
	};

public:
	CAudioAreaEntity() = default;
	virtual ~CAudioAreaEntity() {}

	// CNativeEntityBase
	virtual uint64                GetEventMask() const override { return CDesignerEntityComponent::GetEventMask() | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERNEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVENEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_MOVEINSIDEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVENEARAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER); }
	virtual void                  ProcessEvent(const SEntityEvent& event) override;

	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }

	virtual void                  OnResetState() override;
	// ~CNativeEntityBase

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "AudioAreaEntity Properties"; }

	virtual void        SerializeProperties(Serialization::IArchive& archive) override;
	// ~IEntityPropertyGroup

public:
	static void OnFlowgraphActivation(EntityId entityId, IFlowNode::SActivationInfo* pActInfo, const class CEntityFlowNode* pNode);

protected:
	void SetEnvironmentId(const CryAudio::ControlId environmentId);
	void UpdateFadeValue(const float distance);

	bool                     m_bEnabled = true;
	bool                     m_bTriggerAreasOnMove = false;

	EAreaState               m_areaState = EAreaState::Outside;
	float                    m_fadeDistance = 5.0f;
	float                    m_environmentFadeDistance = 5.0f;
	CryAudio::EOcclusionType m_occlusionType = CryAudio::EOcclusionType::Ignore;

	string                   m_environmentName;

	IEntityAudioComponent*   m_pProxy = nullptr;
	bool                     m_bIsActive = false;
	float                    m_fadeValue = 0.0f;

};
