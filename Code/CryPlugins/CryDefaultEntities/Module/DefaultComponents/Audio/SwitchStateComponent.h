// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySerialization/Forward.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CryString/CryString.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
struct SSwitchStateSerializeHelper final
{
	void Serialize(Serialization::IArchive& archive);
	bool operator==(SSwitchStateSerializeHelper const& other) const { return (m_switchId == other.m_switchId) && (m_stateId == other.m_stateId); }

	CryAudio::ControlId     m_switchId = CryAudio::InvalidControlId;
	CryAudio::SwitchStateId m_stateId = CryAudio::InvalidSwitchStateId;
	string                  m_switchName;
	string                  m_switchStateName;
};

class CSwitchStateComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void                    Initialize() override;
	virtual void                    OnShutDown() override {}
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void                    ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

public:

	CSwitchStateComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CSwitchStateComponent>& desc);

	void        Set(SSwitchStateSerializeHelper const& switchAndState);

protected:

	IEntityAudioComponent* m_pIEntityAudioComponent = nullptr;

	// Properties exposed to UI
	SSwitchStateSerializeHelper m_switchState;

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	Serialization::FunctorActionButton<std::function<void()>> m_setButton;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
};

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SSwitchStateSerializeHelper>& desc)
{
	desc.SetGUID("EDA7930D-32E8-4E44-8861-2D59AEEF9191"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SSwitchStateSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(m_switchName, "switch", "!Switch");
	archive(Serialization::AudioSwitchState<string>(m_switchStateName), "state", "State");

	if (archive.isInput())
	{
		int start = 0;
		string token = m_switchStateName.Tokenize(CryAudio::g_szSwitchStateSeparator, start);

		m_switchName = token;
		m_switchId = CryAudio::StringToId(token.c_str());

		token = m_switchStateName.Tokenize(CryAudio::g_szSwitchStateSeparator, start);
		m_stateId = CryAudio::StringToId(token.c_str());
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
