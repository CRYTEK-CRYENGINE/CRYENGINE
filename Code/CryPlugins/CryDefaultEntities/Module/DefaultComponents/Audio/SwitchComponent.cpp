// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SwitchComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CSwitchComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSwitchComponent::Set, "9B0A40F9-2375-4E56-A3AA-665F31427912"_cry_guid, "Set");
		pFunction->SetDescription("Sets a switch to a specific state");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSwitchComponent::ReflectType(Schematyc::CTypeDesc<CSwitchComponent>& desc)
{
	desc.SetGUID("EDCC5BA5-F4A7-486A-9BB7-3C2F1D7F9684"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Switch");
	desc.SetDescription("Allows for setting a switch on all audio audio objects created by the component this component is attached to.");
	desc.SetIcon("icons:Audio/component_switch.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

	desc.AddMember(&CSwitchComponent::m_switch, 'swit', "switch", "Switch", "The switch which value is applied to all audio objects.", SSwitchWithStateSerializeHelper());
}

//////////////////////////////////////////////////////////////////////////
void CSwitchComponent::Initialize()
{
	// Retrieve component created by Trigger component.
	m_pIEntityAudioComponent = m_pEntity->GetComponent<IEntityAudioComponent>();

	if (m_pIEntityAudioComponent != nullptr)
	{
		m_pIEntityAudioComponent->SetSwitchState(m_switch.m_switchId, m_switch.m_switchStateId, CryAudio::InvalidAuxObjectId);
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CSwitchComponent::GetEventMask() const
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
#else
	return 0;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSwitchComponent::ProcessEvent(const SEntityEvent& event)
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	if (m_pIEntityAudioComponent != nullptr)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			m_pIEntityAudioComponent->SetSwitchState(m_switch.m_switchId, m_switch.m_switchStateId, CryAudio::InvalidAuxObjectId);
			break;
		}
	}
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSwitchComponent::Set(SSwitchWithStateSerializeHelper const& switchAndState)
{
	if (m_pIEntityAudioComponent != nullptr && switchAndState.m_switchId != CryAudio::InvalidControlId && switchAndState.m_switchStateId != CryAudio::InvalidControlId)
	{
		m_pIEntityAudioComponent->SetSwitchState(switchAndState.m_switchId, switchAndState.m_switchStateId, CryAudio::InvalidAuxObjectId);
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
