// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ListenerComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
void CListenerComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CListenerComponent::SetActive, "1ECF05D6-7E0B-4954-AC2B-087488E42F2B"_cry_guid, "SetActive");
		pFunction->SetDescription("Enables/Disables the component.");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		pFunction->BindInput(1, 'val', "Activate");
		componentScope.Register(pFunction);
	}
}

//////////////////////////////////////////////////////////////////////////
void CListenerComponent::Initialize()
{
	if (m_pIListener == nullptr)
	{
		CryFixedStringT<64> name;
		name.Format("%s(%d)", m_pEntity->GetName(), static_cast<int>(m_pEntity->GetId()));
		SetName(name.c_str());

		m_pIListener = gEnv->pAudioSystem->CreateListener(name.c_str());

		if (m_pIListener != nullptr)
		{
			m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_TRIGGER_AREAS);
			m_pEntity->SetFlagsExtended(m_pEntity->GetFlagsExtended() | ENTITY_FLAG_EXTENDED_AUDIO_LISTENER);

			Matrix34 const& tm = m_pEntity->GetWorldTM();
			CRY_ASSERT_MESSAGE(tm.IsValid(), "Invalid Matrix34 during CListenerComponent::Initialize");
			m_previousTransformation = tm;
			m_pIListener->SetTransformation(m_previousTransformation);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CListenerComponent::OnShutDown()
{
	if (m_pIListener != nullptr)
	{
		gEnv->pEntitySystem->GetAreaManager()->ExitAllAreas(GetEntityId());
		gEnv->pAudioSystem->ReleaseListener(m_pIListener);
		m_pIListener = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CListenerComponent::GetEventMask() const
{
	return BIT64(ENTITY_EVENT_XFORM);
}

//////////////////////////////////////////////////////////////////////////
void CListenerComponent::ProcessEvent(const SEntityEvent& event)
{
	if (m_bActive && m_pIListener != nullptr)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_XFORM:
			{
				int const flags = static_cast<int>(event.nParam[0]);

				if ((flags & (ENTITY_XFORM_POS | ENTITY_XFORM_ROT)) != 0)
				{
					Matrix34 const& tm = m_pEntity->GetWorldTM();

					// Listener needs to move at least 1 cm to trigger an update.
					if (!m_previousTransformation.IsEquivalent(tm, 0.01f))
					{
						m_previousTransformation = tm;
						m_pIListener->SetTransformation(m_previousTransformation);

						// Add entity to the AreaManager for raising audio relevant events.
						gEnv->pEntitySystem->GetAreaManager()->MarkEntityForUpdate(m_pEntity->GetId());
					}
				}

				break;
			}
		}
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
