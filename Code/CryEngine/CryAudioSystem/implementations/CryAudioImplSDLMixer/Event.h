// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include "GlobalData.h"
#include <ITriggerConnection.h>
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
enum class EEventFlags : EnumFlagsType
{
	None                  = 0,
	IsAttenuationEnnabled = BIT(0),
	IsPanningEnabled      = BIT(1),
	ToBeDestructed        = BIT(2),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EEventFlags);

class CEvent final : public ITriggerConnection, public CPoolObject<CEvent, stl::PSyncNone>
{
public:

	enum class EActionType : EnumFlagsType
	{
		None,
		Start,
		Stop,
		Pause,
		Resume,
	};

	CEvent() = delete;
	CEvent(CEvent const&) = delete;
	CEvent(CEvent&&) = delete;
	CEvent& operator=(CEvent const&) = delete;
	CEvent& operator=(CEvent&&) = delete;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	explicit CEvent(
		char const* const szName,
		uint32 const id,
		EEventFlags const flags,
		EActionType const type,
		SampleId const sampleId,
		float const attenuationMinDistance = g_defaultMinAttenuationDist,
		float const attenuationMaxDistance = g_defaultMaxAttenuationDist,
		int const volume = 128,
		int const numLoops = 1,
		int const fadeInTime = 0,
		int const fadeOutTime = 0)
		: m_name(szName)
		, m_id(id)
		, m_flags(flags)
		, m_type(type)
		, m_sampleId(sampleId)
		, m_attenuationMinDistance(attenuationMinDistance)
		, m_attenuationMaxDistance(attenuationMaxDistance)
		, m_volume(volume)
		, m_numLoops(numLoops)
		, m_fadeInTime(fadeInTime)
		, m_fadeOutTime(fadeOutTime)
		, m_numInstances(0)
	{}
#else
	explicit CEvent(
		uint32 const id,
		EEventFlags const flags,
		EActionType const type,
		SampleId const sampleId,
		float const attenuationMinDistance = g_defaultMinAttenuationDist,
		float const attenuationMaxDistance = g_defaultMaxAttenuationDist,
		int const volume = 128,
		int const numLoops = 1,
		int const fadeInTime = 0,
		int const fadeOutTime = 0)
		: m_id(id)
		, m_flags(flags)
		, m_type(type)
		, m_sampleId(sampleId)
		, m_attenuationMinDistance(attenuationMinDistance)
		, m_attenuationMaxDistance(attenuationMaxDistance)
		, m_volume(volume)
		, m_numLoops(numLoops)
		, m_fadeInTime(fadeInTime)
		, m_fadeOutTime(fadeOutTime)
		, m_numInstances(0)
	{}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	virtual ~CEvent() override = default;

	// CryAudio::Impl::ITriggerConnection
	virtual ETriggerResult Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId) override;
	virtual void           Stop(IObject* const pIObject) override;
	// ~CryAudio::Impl::ITriggerConnection

	uint32      GetId() const                   { return m_id; }
	EActionType GetType() const                 { return m_type; }
	SampleId    GetSampleId() const             { return m_sampleId; }

	EEventFlags GetFlags() const                { return m_flags; }
	void        SetFlag(EEventFlags const flag) { m_flags |= flag; }

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	float GetAttenuationMinDistance() const { return m_attenuationMinDistance; }
	float GetAttenuationMaxDistance() const { return m_attenuationMaxDistance; }
	int   GetVolume() const                 { return m_volume; }
	int   GetFadeOutTime() const            { return m_fadeOutTime; }

	void  IncrementNumInstances()           { ++m_numInstances; }
	void  DecrementNumInstances();

	bool  CanBeDestructed() const { return ((m_flags& EEventFlags::ToBeDestructed) != 0) && (m_numInstances == 0); }

private:

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	uint32 const m_id;
	EEventFlags       m_flags;
	EActionType const m_type;
	SampleId const    m_sampleId;
	float const       m_attenuationMinDistance;
	float const       m_attenuationMaxDistance;
	int const         m_volume;
	int const         m_numLoops;
	int const         m_fadeInTime;
	int const         m_fadeOutTime;
	uint16            m_numInstances;
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio