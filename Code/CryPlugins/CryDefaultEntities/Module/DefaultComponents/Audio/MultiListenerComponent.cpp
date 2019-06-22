// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MultiListenerComponent.h"
#include <CryAudio/IAudioSystem.h>

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SSingleListenerSerializeHelper>& desc)
{
	desc.SetGUID("9DAF1238-4BA6-4255-BE64-CC7E542C1D33"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SSingleListenerSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioListener<string>(m_name), "name", "Name");

	if (archive.isInput())
	{
		m_id = CryAudio::StringToId(m_name.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SMultiListenerSerializeHelper>& desc)
{
	desc.SetGUID("B3B11959-6312-4833-969D-649607A9B536"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SMultiListenerSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(m_listeners, "listeners", "Listeners");
}

//////////////////////////////////////////////////////////////////////////
bool SMultiListenerSerializeHelper::operator==(SMultiListenerSerializeHelper const& other) const
{
	bool isEqual = true;
	size_t const numListeners = m_listeners.size();

	if (numListeners == other.m_listeners.size())
	{
		for (size_t i = 0; i < numListeners; ++i)
		{
			if (m_listeners[i] != other.m_listeners[i])
			{
				isEqual = false;
				break;
			}
		}
	}
	else
	{
		isEqual = false;
	}

	return isEqual;
}

//////////////////////////////////////////////////////////////////////////
void CMultiListenerComponent::ReflectType(Schematyc::CTypeDesc<CMultiListenerComponent>& desc)
{
	desc.SetGUID("6477010F-2841-4C99-9ED9-55B6E1F594AB"_cry_guid);
	desc.SetEditorCategory("Audio");
	desc.SetLabel("Multi-Listener");
	desc.SetDescription("Defines which Audio Listener(s) will be used on the audio object.");
	desc.SetIcon("icons:Audio/component_audio_listener.ico");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::HideFromInspector });

	desc.AddMember(&CMultiListenerComponent::m_listenerHelper, 'list', "listeners", "Listeners", "The listeners are set on the audio object.", SMultiListenerSerializeHelper());
}

//////////////////////////////////////////////////////////////////////////
void CMultiListenerComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
}

//////////////////////////////////////////////////////////////////////////
void CMultiListenerComponent::Initialize()
{
	m_pIEntityAudioComponent = m_pEntity->GetComponent<IEntityAudioComponent>();

	if (m_pIEntityAudioComponent != nullptr)
	{
		SetDefaultListener();
		SetListeners();
	}
	else
	{
		m_pIEntityAudioComponent = m_pEntity->CreateComponent<IEntityAudioComponent>();
		SetDefaultListener();
		SetListeners();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMultiListenerComponent::OnShutDown()
{
}

//////////////////////////////////////////////////////////////////////////
Cry::Entity::EventFlags CMultiListenerComponent::GetEventMask() const
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	return ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED;
#else
	return Cry::Entity::EventFlags();
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CMultiListenerComponent::ProcessEvent(const SEntityEvent& event)
{
#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	if ((event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED) && (m_pIEntityAudioComponent != nullptr))
	{
		for (auto const id : m_previousListenerIds)
		{
			m_pIEntityAudioComponent->RemoveListener(id);
		}

		m_previousListenerIds.clear();
		SetListeners();
	}
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CMultiListenerComponent::SetListeners()
{
	if (m_pIEntityAudioComponent != nullptr)
	{
		for (auto const& listener : m_listenerHelper.m_listeners)
		{
			if (listener.m_id != CryAudio::InvalidListenerId)
			{
				m_pIEntityAudioComponent->AddListener(listener.m_id);

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
				m_previousListenerIds.emplace_back(listener.m_id);
#endif        // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMultiListenerComponent::SetDefaultListener()
{
	if (m_pIEntityAudioComponent != nullptr)
	{
		m_pIEntityAudioComponent->RemoveListener(CryAudio::DefaultListenerId);

		if (m_listenerHelper.m_listeners.empty())
		{
			m_listenerHelper.m_listeners.emplace_back(CryAudio::g_szDefaultListenerName);

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
			m_previousListenerIds.emplace_back(CryAudio::DefaultListenerId);
#endif      // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
		}
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
