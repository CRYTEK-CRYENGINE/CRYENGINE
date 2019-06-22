// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SwitchStateComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CSwitchStateComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSwitchStateComponent::Set, "FB00D5B4-8EE1-4CF5-A5C1-83A7C414B06A"_cry_guid, "Set");
		pFunction->SetDescription("Sets a switch to a specific state");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSwitchStateComponent::ReflectType(Schematyc::CTypeDesc<CSwitchStateComponent>& desc)
{
	desc.SetGUID("66AEF05D-969F-49D5-9AD6-409D413C100F"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("SwitchState");
	desc.SetDescription("Allows for setting a switch state on all audio audio objects created by the component this component is attached to.");
	desc.SetIcon("icons:Audio/component_switch.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

	desc.AddMember(&CSwitchStateComponent::m_switchState, 'swit', "switchState", "SwitchState", "The switch state which value is applied to all audio objects.", SSwitchStateSerializeHelper());

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	desc.AddMember(&CSwitchStateComponent::m_setButton, 'btn1', "set", "Set", "Sets the switch to the specific state.", Serialization::FunctorActionButton<std::function<void()>>());
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSwitchStateComponent::Initialize()
{
	// Retrieve component created by Trigger component.
	m_pIEntityAudioComponent = m_pEntity->GetComponent<IEntityAudioComponent>();

	if (m_pIEntityAudioComponent != nullptr)
	{
		m_pIEntityAudioComponent->SetSwitchState(m_switchState.m_switchId, m_switchState.m_stateId, CryAudio::InvalidAuxObjectId);
	}

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	m_setButton = Serialization::ActionButton(std::function<void()>([this]() { Set(m_switchState); }));
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
Cry::Entity::EventFlags CSwitchStateComponent::GetEventMask() const
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	return ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED;
#else
	return Cry::Entity::EventFlags();
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSwitchStateComponent::ProcessEvent(const SEntityEvent& event)
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	if (m_pIEntityAudioComponent != nullptr)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			m_pIEntityAudioComponent->SetSwitchState(m_switchState.m_switchId, m_switchState.m_stateId, CryAudio::InvalidAuxObjectId);
			break;
		}
	}
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CSwitchStateComponent::Set(SSwitchStateSerializeHelper const& switchAndState)
{
	if ((m_pIEntityAudioComponent != nullptr) &&
	    (switchAndState.m_switchId != CryAudio::InvalidControlId) &&
	    (switchAndState.m_stateId != CryAudio::InvalidControlId))
	{
		m_pIEntityAudioComponent->SetSwitchState(switchAndState.m_switchId, switchAndState.m_stateId, CryAudio::InvalidAuxObjectId);
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
