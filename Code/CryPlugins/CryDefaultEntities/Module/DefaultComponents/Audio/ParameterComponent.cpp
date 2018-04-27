// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParameterComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CParameterComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CParameterComponent::Set, "D34C970E-AF2D-46AF-A6E0-211CE0395FC6"_cry_guid, "Set");
		pFunction->SetDescription("Sets a parameter to a specific value");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'val', "Value");
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParameterComponent::ReflectType(Schematyc::CTypeDesc<CParameterComponent>& desc)
{
	desc.SetGUID("634927FC-AE0E-4B6E-9846-99BF8CBE56E2"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Parameter");
	desc.SetDescription("Allows for setting a parameter on all audio audio objects created by the component this component is attached to.");
	desc.SetIcon("icons:Audio/component_parameter.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

	desc.AddMember(&CParameterComponent::m_parameter, 'para', "parameter", "Parameter", "The parameter which value is applied to all audio objects.", SParameterSerializeHelper());
}

//////////////////////////////////////////////////////////////////////////
void CParameterComponent::Initialize()
{
	// Retrieve component created by Trigger component.
	m_pIEntityAudioComponent = m_pEntity->GetComponent<IEntityAudioComponent>();

	if (m_pIEntityAudioComponent != nullptr)
	{
		m_pIEntityAudioComponent->SetParameter(m_parameter.m_id, m_value, CryAudio::InvalidAuxObjectId);
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CParameterComponent::GetEventMask() const
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
#else
	return 0;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CParameterComponent::ProcessEvent(const SEntityEvent& event)
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	if (m_pIEntityAudioComponent != nullptr)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
			m_pIEntityAudioComponent->SetParameter(m_parameter.m_id, m_value, CryAudio::InvalidAuxObjectId);
			break;
		}
	}
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CParameterComponent::Set(float const value)
{
	if (m_pIEntityAudioComponent != nullptr && m_parameter.m_id != CryAudio::InvalidControlId)
	{
		m_pIEntityAudioComponent->SetParameter(m_parameter.m_id, value, CryAudio::InvalidAuxObjectId);
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
