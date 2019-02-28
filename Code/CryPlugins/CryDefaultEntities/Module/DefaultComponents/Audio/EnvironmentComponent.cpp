// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EnvironmentComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CEnvironmentComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto const pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEnvironmentComponent::SetActive, "E8BB46CC-E7EF-4722-8600-3D2630728A4E"_cry_guid, "SetActive");
		pFunction->SetDescription("Enables/Disables the component.");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'val', "Activate");
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEnvironmentComponent::ReflectType(Schematyc::CTypeDesc<CEnvironmentComponent>& desc)
{
	desc.SetGUID("EEAC59D7-8EFE-4AA8-BBF3-53446B848386"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Environment");
	desc.SetDescription("Used to assign an environment to areas and set its efficiency.");
	desc.SetIcon("icons:Audio/component_environment.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

	desc.AddMember(&CEnvironmentComponent::m_environment, 'envi', "environment", "Environment", "The environment to assign.", SEnvironmentSerializeHelper());
	desc.AddMember(&CEnvironmentComponent::m_fadeDistance, 'fade', "fadeDistance", "FadeDistance", "The distance over which is faded.", 0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CEnvironmentComponent::Initialize()
{
	if (m_pIEntityAudioComponent == nullptr)
	{
		m_pIEntityAudioComponent = m_pEntity->GetOrCreateComponent<IEntityAudioComponent>();

		if (m_environment.m_id != CryAudio::InvalidEnvironmentId)
		{
			m_pIEntityAudioComponent->SetEnvironmentId(m_environment.m_id);
		}

		m_pIEntityAudioComponent->SetEnvironmentFadeDistance(m_fadeDistance);
		gEnv->pEntitySystem->GetAreaManager()->SetAreasDirty();
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CEnvironmentComponent::GetEventMask() const
{
	uint64 eventMask = ENTITY_EVENT_BIT(ENTITY_EVENT_DONE);

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
#endif// INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
	
	return eventMask;
}

//////////////////////////////////////////////////////////////////////////
void CEnvironmentComponent::ProcessEvent(const SEntityEvent& event)
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		// Update the environment Id and/or fade distance in case the user changed it.
		if (m_pIEntityAudioComponent != nullptr)
		{
			m_pIEntityAudioComponent->SetEnvironmentId(m_environment.m_id);
			m_pIEntityAudioComponent->SetEnvironmentFadeDistance(m_fadeDistance);
			gEnv->pEntitySystem->GetAreaManager()->SetAreasDirty();
		}
	}
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
	if(event.event == ENTITY_EVENT_DONE)
	{
		if (m_pIEntityAudioComponent != nullptr)
		{
			m_pIEntityAudioComponent->SetEnvironmentAmount(m_environment.m_id, 0.0f, CryAudio::InvalidAuxObjectId);
			m_pIEntityAudioComponent->SetEnvironmentId(CryAudio::InvalidEnvironmentId);
			m_pIEntityAudioComponent->SetEnvironmentFadeDistance(0.0f);
			m_pIEntityAudioComponent = nullptr;
			gEnv->pEntitySystem->GetAreaManager()->SetAreasDirty();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEnvironmentComponent::SetActive(bool const bValue)
{
	if (bValue)
	{
		if (m_environment.m_id != CryAudio::InvalidEnvironmentId)
		{
			m_pIEntityAudioComponent->SetEnvironmentId(m_environment.m_id);
		}

		m_pIEntityAudioComponent->SetEnvironmentFadeDistance(m_fadeDistance);
	}
	else
	{
		m_pIEntityAudioComponent->SetEnvironmentAmount(m_environment.m_id, 0.0f, CryAudio::InvalidAuxObjectId);
		m_pIEntityAudioComponent->SetEnvironmentId(CryAudio::InvalidEnvironmentId);
		m_pIEntityAudioComponent->SetEnvironmentFadeDistance(0.0f);
	}

	gEnv->pEntitySystem->GetAreaManager()->SetAreasDirty();
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
