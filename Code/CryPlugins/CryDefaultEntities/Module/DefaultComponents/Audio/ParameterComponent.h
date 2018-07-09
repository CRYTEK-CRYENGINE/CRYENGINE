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
struct SParameterSerializeHelper
{
	void Serialize(Serialization::IArchive& archive);
	bool operator==(SParameterSerializeHelper const& other) const { return m_name == other.m_name; }

	CryAudio::ControlId m_id = CryAudio::InvalidControlId;
	string              m_name;
};

class CParameterComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() override;
	virtual void   OnShutDown() override {}
	virtual uint64 GetEventMask() const override;
	virtual void   ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

public:

	CParameterComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CParameterComponent>& desc);

	void        Set(float const value);

protected:

	IEntityAudioComponent* m_pIEntityAudioComponent = nullptr;

	// Properties exposed to UI
	SParameterSerializeHelper                        m_parameter;
	Schematyc::Range<-100000, 100000, -10000, 10000> m_value = 0.0f;
};

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SParameterSerializeHelper>& desc)
{
	desc.SetGUID("5287D8F9-7638-41BB-BFDD-2F5B47DEEA07"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void SParameterSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioRTPC<string>(m_name), "parameterName", "^");

	if (archive.isInput())
	{
		m_id = CryAudio::StringToId(m_name.c_str());
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
