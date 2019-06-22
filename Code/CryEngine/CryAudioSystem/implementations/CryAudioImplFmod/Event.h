// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseTriggerConnection.h"
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
enum class EEventFlags : EnumFlagsType
{
	None                         = 0,
	HasProgrammerSound           = BIT(0),
	HasAbsoluteVelocityParameter = BIT(1),
	HasOcclusionParameter        = BIT(2),
	CheckedParameters            = BIT(3),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EEventFlags);

class CEvent final : public CBaseTriggerConnection, public CPoolObject<CEvent, stl::PSyncNone>
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

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	// For pure events.
	explicit CEvent(
		uint32 const id,
		EActionType const actionType,
		FMOD_GUID const guid,
		char const* const szName)
		: CBaseTriggerConnection(EType::Event, szName)
		, m_id(id)
		, m_actionType(actionType)
		, m_guid(guid)
		, m_flags(EEventFlags::None)
		, m_numInstances(0)
		, m_toBeDestructed(false)
		, m_key("")
		, m_pEventDescription(nullptr)
	{}

	// For keys/programmer sounds.
	explicit CEvent(
		uint32 const id,
		FMOD_GUID const guid,
		char const* const szKey,
		char const* const szName)
		: CBaseTriggerConnection(EType::Event, szName)
		, m_id(id)
		, m_actionType(EActionType::Start)
		, m_guid(guid)
		, m_flags(EEventFlags::HasProgrammerSound)
		, m_numInstances(0)
		, m_toBeDestructed(false)
		, m_key(szKey)
		, m_pEventDescription(nullptr)
	{}
#else
	// For pure events.
	explicit CEvent(
		uint32 const id,
		EActionType const actionType,
		FMOD_GUID const guid)
		: CBaseTriggerConnection(EType::Event)
		, m_id(id)
		, m_actionType(actionType)
		, m_guid(guid)
		, m_flags(EEventFlags::None)
		, m_numInstances(0)
		, m_toBeDestructed(false)
		, m_key("")
		, m_pEventDescription(nullptr)
	{}

	// For keys/programmer sounds.
	explicit CEvent(
		uint32 const id,
		FMOD_GUID const guid,
		char const* const szKey)
		: CBaseTriggerConnection(EType::Event)
		, m_id(id)
		, m_actionType(EActionType::Start)
		, m_guid(guid)
		, m_flags(EEventFlags::HasProgrammerSound)
		, m_numInstances(0)
		, m_toBeDestructed(false)
		, m_key(szKey)
		, m_pEventDescription(nullptr)
	{}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	virtual ~CEvent() override;

	// CryAudio::Impl::ITriggerConnection
	virtual ETriggerResult Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId) override;
	virtual void           Stop(IObject* const pIObject) override;
	// ~CryAudio::Impl::ITriggerConnection

	uint32                                       GetId() const               { return m_id; }
	EEventFlags                                  GetFlags() const            { return m_flags; }
	CryFixedStringT<MaxControlNameLength> const& GetKey() const              { return m_key; }

	FMOD::Studio::EventDescription*              GetEventDescription() const { return m_pEventDescription; }

	void                                         IncrementNumInstances()     { ++m_numInstances; }
	void                                         DecrementNumInstances();

	bool                                         CanBeDestructed() const { return m_toBeDestructed && (m_numInstances == 0); }
	void                                         SetToBeDestructed()     { m_toBeDestructed = true; }

private:

	uint32 const                                m_id;
	EActionType const                           m_actionType;
	FMOD_GUID const                             m_guid;
	EEventFlags                                 m_flags;
	uint16                                      m_numInstances;
	bool                                        m_toBeDestructed;
	CryFixedStringT<MaxControlNameLength> const m_key;
	FMOD::Studio::EventDescription*             m_pEventDescription;
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
