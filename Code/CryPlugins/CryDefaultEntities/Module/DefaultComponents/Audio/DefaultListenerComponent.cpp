// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DefaultListenerComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CDefaultListenerComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDefaultListenerComponent::SetActive, "2EC40D99-6F72-49FA-95E5-23AB486C0FCA"_cry_guid, "SetActive");
		pFunction->SetDescription("Enables/Disables the component.");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'val', "Activate");
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void CDefaultListenerComponent::Initialize()
{
	if (m_pIListener == nullptr)
	{
		Matrix34 const tm = GetWorldTransformMatrix();
		CRY_ASSERT_MESSAGE(tm.IsValid(), "Invalid Matrix34 during CDefaultListenerComponent::Initialize");
		m_previousTransformation = tm;

		SetName("DefaultListener");
		m_pIListener = gEnv->pAudioSystem->GetListener();

		if (m_pIListener != nullptr)
		{
			m_pIListener->SetTransformation(m_previousTransformation);
			m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_TRIGGER_AREAS);
			m_pEntity->SetFlagsExtended(m_pEntity->GetFlagsExtended() | ENTITY_FLAG_EXTENDED_AUDIO_LISTENER);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CDefaultListenerComponent::OnShutDown()
{
	if (m_pIListener != nullptr)
	{
		gEnv->pEntitySystem->GetAreaManager()->ExitAllAreas(GetEntityId());
		m_pIListener = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
Cry::Entity::EventFlags CDefaultListenerComponent::GetEventMask() const
{
	return ENTITY_EVENT_XFORM;
}

//////////////////////////////////////////////////////////////////////////
void CDefaultListenerComponent::ProcessEvent(const SEntityEvent& event)
{
	if (m_isActive && (m_pIListener != nullptr))
	{
		switch (event.event)
		{
		case ENTITY_EVENT_XFORM:
			{
				auto const flags = static_cast<int>(event.nParam[0]);

				if ((flags & (ENTITY_XFORM_POS | ENTITY_XFORM_ROT)) != 0)
				{
					OnTransformChanged();
				}

				break;
			}
		default:
			{
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CDefaultListenerComponent::OnTransformChanged()
{
	Matrix34 const tm = GetWorldTransformMatrix();

	// Listener needs to move at least 1 cm to trigger an update.
	if (!m_previousTransformation.IsEquivalent(tm, 0.01f))
	{
		m_previousTransformation = tm;
		m_pIListener->SetTransformation(m_previousTransformation);

		// Add entity to the AreaManager for raising audio relevant events.
		gEnv->pEntitySystem->GetAreaManager()->MarkEntityForUpdate(m_pEntity->GetId());
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
