// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryAudio/IListener.h>
#include <CrySerialization/Forward.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntityComponent.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
struct SSingleListenerSerializeHelper final
{
	SSingleListenerSerializeHelper() = default;

	explicit SSingleListenerSerializeHelper(char const* const szName)
		: m_name(szName)
	{
		m_id = CryAudio::StringToId(m_name.c_str());
	}

	void Serialize(Serialization::IArchive& archive);
	bool operator==(SSingleListenerSerializeHelper const& other) const { return (m_id == other.m_id); }
	bool operator!=(SSingleListenerSerializeHelper const& other) const { return (m_id != other.m_id); }

	CryAudio::ListenerId m_id = CryAudio::InvalidListenerId;
	string               m_name;
};

struct SMultiListenerSerializeHelper final
{
	void Serialize(Serialization::IArchive& archive);
	bool operator==(SMultiListenerSerializeHelper const& other) const;

	std::vector<SSingleListenerSerializeHelper> m_listeners;
};

class CMultiListenerComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void                    Initialize() override;
	virtual void                    OnShutDown() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void                    ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

public:

	CMultiListenerComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CMultiListenerComponent>& desc);

private:

	void SetListeners();
	void SetDefaultListener();

	IEntityAudioComponent*        m_pIEntityAudioComponent = nullptr;
	SMultiListenerSerializeHelper m_listenerHelper;

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	std::vector<CryAudio::ListenerId> m_previousListenerIds;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
};
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
