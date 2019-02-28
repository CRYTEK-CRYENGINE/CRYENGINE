// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySerialization/Forward.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryString/CryString.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
namespace Audio
{
namespace DefaultComponents
{
struct SEnvironmentSerializeHelper
{
	void Serialize(Serialization::IArchive& archive);
	bool operator==(SEnvironmentSerializeHelper const& other) const { return m_name == other.m_name; }

	CryAudio::EnvironmentId m_id = CryAudio::InvalidEnvironmentId;
	string                  m_name;
};

class CEnvironmentComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() override;
	virtual uint64 GetEventMask() const override;
	virtual void   ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

	// Properties exposed to UI
	SEnvironmentSerializeHelper m_environment;
	float                       m_fadeDistance = 0.0f;

public:

	CEnvironmentComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CEnvironmentComponent>& desc);
	void        SetActive(bool const bValue);

private:

	IEntityAudioComponent* m_pIEntityAudioComponent = nullptr;
};

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SEnvironmentSerializeHelper>& desc)
{
	desc.SetGUID("AA1DBA00-A1D6-4A77-843C-612A7BE1DECD"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SEnvironmentSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioEnvironment<string>(m_name), "environmentName", "^");

	if (archive.isInput())
	{
		m_id = CryAudio::StringToId(m_name.c_str());
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
