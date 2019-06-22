// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ITriggerConnection.h>
#include <PoolObject.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
enum class EEventFlags : EnumFlagsType
{
	None           = 0,
	ToBeDestructed = BIT(0),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EEventFlags);

class CEvent final : public ITriggerConnection, public CPoolObject<CEvent, stl::PSyncNone>
{
public:

	CEvent() = delete;
	CEvent(CEvent const&) = delete;
	CEvent(CEvent&&) = delete;
	CEvent& operator=(CEvent const&) = delete;
	CEvent& operator=(CEvent&&) = delete;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	explicit CEvent(AkUniqueID const id, float const maxAttenuation, char const* const szName)
		: m_id(id)
		, m_flags(EEventFlags::None)
		, m_maxAttenuation(maxAttenuation)
		, m_numInstances(0)
		, m_name(szName)
	{}
#else
	explicit CEvent(AkUniqueID const id, float const maxAttenuation)
		: m_id(id)
		, m_flags(EEventFlags::None)
		, m_maxAttenuation(maxAttenuation)
		, m_numInstances(0)
	{}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	virtual ~CEvent() override = default;

	// CryAudio::Impl::ITriggerConnection
	virtual ETriggerResult Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId) override;
	virtual void           Stop(IObject* const pIObject) override;
	// ~CryAudio::Impl::ITriggerConnection

	AkUniqueID GetId() const                   { return m_id; }
	float      GetMaxAttenuation() const       { return m_maxAttenuation; }

	void       SetFlag(EEventFlags const flag) { m_flags |= flag; }

	void       IncrementNumInstances()         { ++m_numInstances; }
	void       DecrementNumInstances();

	bool       CanBeDestructed() const { return ((m_flags& EEventFlags::ToBeDestructed) != 0) && (m_numInstances == 0); }

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

private:

	AkUniqueID const m_id;
	EEventFlags      m_flags;
	float const      m_maxAttenuation;
	uint16           m_numInstances;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
