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
struct STriggerSerializeHelper
{
	void Serialize(Serialization::IArchive& archive);
	bool operator==(STriggerSerializeHelper const& other) const { return m_name == other.m_name; }

	CryAudio::ControlId m_id = CryAudio::InvalidControlId;
	string              m_name;
};

class CTriggerComponent final : public IEntityComponent
{
protected:

	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() override;
	virtual uint64 GetEventMask() const override;
	virtual void   ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent

public:

	CTriggerComponent() = default;

	static void     ReflectType(Schematyc::CTypeDesc<CTriggerComponent>& desc);

	void SetAutoPlay(bool const bEnable);
	void Play();
	void Stop();
	void DetermineActivityRadius();
	void GetActivityRadius(float& radius);

	struct SFinishedSignal
	{
		SFinishedSignal() = default;
	};

protected:

	CryAudio::AuxObjectId  m_auxObjectId = CryAudio::InvalidAuxObjectId;
	IEntityAudioComponent* m_pIEntityAudioComponent = nullptr;
	bool                   m_bAutoPlay = true;
	uint32                 m_numActiveTriggerInstances = 0;
	float                  m_activityRadius = 0.0f;

	// Properties exposed to UI
	STriggerSerializeHelper m_playTrigger;
	STriggerSerializeHelper m_stopTrigger;

#if defined(INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE)
	Serialization::FunctorActionButton<std::function<void()>> m_playButton;
	Serialization::FunctorActionButton<std::function<void()>> m_stopButton;
	CryAudio::ControlId m_previousPlayTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_previousStopTriggerId = CryAudio::InvalidControlId;
#endif  // INCLUDE_DEFAULT_PLUGINS_PRODUCTION_CODE
};

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<STriggerSerializeHelper>& desc)
{
	desc.SetGUID("C5DE4974-ECAB-4D6F-A93D-02C1F5C55C31"_cry_guid);
}

//////////////////////////////////////////////////////////////////////////
inline void STriggerSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioTrigger<string>(m_name), "triggerName", "^");

	if (archive.isInput())
	{
		m_id = CryAudio::StringToId(m_name.c_str());
	}
}
} // namespace DefaultComponents
} // namespace Audio
} // namespace Cry
