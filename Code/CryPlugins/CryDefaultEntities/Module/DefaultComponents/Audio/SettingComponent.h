// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
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
struct SSettingSerializeHelper final
{
	void Serialize(Serialization::IArchive& archive);
	bool operator==(SSettingSerializeHelper const& other) const { return m_id == other.m_id; }

	CryAudio::ControlId m_id = CryAudio::InvalidControlId;
	string              m_name;
};

class CSettingComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void                    Initialize() override;
	virtual void                    OnShutDown() override                            {}
	virtual Cry::Entity::EventFlags GetEventMask() const override                    { return Cry::Entity::EventFlags(); }
	virtual void                    ProcessEvent(const SEntityEvent& event) override {}
	// ~IEntityComponent

public:

	CSettingComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CSettingComponent>& desc);

	void        Load();
	void        Unload();

protected:

	// Properties exposed to UI
	SSettingSerializeHelper m_setting;

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	Serialization::FunctorActionButton<std::function<void()>> m_loadButton;
	Serialization::FunctorActionButton<std::function<void()>> m_unloadButton;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
};

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SSettingSerializeHelper>& desc)
{
	desc.SetGUID("A7C4B97E-CDCB-417F-AFDA-ABDEFF0F1893"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SSettingSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioSetting<string>(m_name), "settingName", "^");

	if (archive.isInput())
	{
		m_id = CryAudio::StringToId(m_name.c_str());
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
