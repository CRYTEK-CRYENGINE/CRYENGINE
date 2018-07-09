// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OcclusionComponent.h"

namespace CryAudio
{
static void ReflectType(Schematyc::CTypeDesc<EOcclusionType>& desc)
{
	// We don't reflect the enum items here as they are already registered through yasli.
	// Once yasli should be removed this needs adjustment to also incorporate the enum items.
	desc.SetGUID("8CF304D8-4E83-4EFD-BA66-34DE91E9FECE"_cry_guid);
	desc.SetLabel("Occlusion Type");
	desc.SetDescription("The type of occlusion used by this object.");
	desc.SetFlags(Schematyc::ETypeFlags::Switchable);
	desc.SetDefaultValue(EOcclusionType::Ignore);
}
} // namespace CryAudio

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void COcclusionComponent::ReflectType(Schematyc::CTypeDesc<COcclusionComponent>& desc)
{
	desc.SetGUID("F079E52E-FCDD-45AE-8860-FD4F292B0A1A"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Occlusion");
	desc.SetDescription("Used to set the occlusion type.");
	desc.SetIcon("icons:Audio/component_occlusion.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

	desc.AddMember(&COcclusionComponent::m_occlusionType, 'occl', "occlusionType", "Occlusion Type", "The occlusion type to be used by this object.", CryAudio::EOcclusionType::Ignore);
}

//////////////////////////////////////////////////////////////////////////
void COcclusionComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto const pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&COcclusionComponent::SetOcclusionType, "122658EB-9ED7-400D-B4AB-E2125486E172"_cry_guid, "SetOcclusionType");
		pFunction->SetDescription("Allows for setting the object's occlusion type.");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		pFunction->BindInput(1, 'occl', "Occlusion Type");
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void COcclusionComponent::Initialize()
{
	if (m_pIEntityAudioComponent == nullptr)
	{
		m_pIEntityAudioComponent = m_pEntity->GetOrCreateComponent<IEntityAudioComponent>();
		m_pIEntityAudioComponent->SetObstructionCalcType(m_occlusionType, CryAudio::InvalidAuxObjectId);
	}
}

//////////////////////////////////////////////////////////////////////////
void COcclusionComponent::OnShutDown()
{
	if (m_pIEntityAudioComponent != nullptr)
	{
		m_pIEntityAudioComponent->SetObstructionCalcType(CryAudio::EOcclusionType::Ignore, CryAudio::InvalidAuxObjectId);
		m_pIEntityAudioComponent = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 COcclusionComponent::GetEventMask() const
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
#else
	return 0;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void COcclusionComponent::ProcessEvent(const SEntityEvent& event)
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		if (m_pIEntityAudioComponent != nullptr)
		{
			m_pIEntityAudioComponent->SetObstructionCalcType(m_occlusionType, CryAudio::InvalidAuxObjectId);
		}
	}
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void COcclusionComponent::SetOcclusionType(CryAudio::EOcclusionType const occlusionType)
{
	m_occlusionType = occlusionType;
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
