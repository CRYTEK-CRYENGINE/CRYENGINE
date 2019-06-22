// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseTriggerConnection.h"
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CSnapshot final : public CBaseTriggerConnection, public CPoolObject<CSnapshot, stl::PSyncNone>
{
public:

	enum class EActionType : EnumFlagsType
	{
		None,
		Start,
		Stop,
	};

	CSnapshot() = delete;
	CSnapshot(CSnapshot const&) = delete;
	CSnapshot(CSnapshot&&) = delete;
	CSnapshot& operator=(CSnapshot const&) = delete;
	CSnapshot& operator=(CSnapshot&&) = delete;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	explicit CSnapshot(
		uint32 const id,
		EActionType const actionType,
		FMOD::Studio::EventDescription* const pEventDescription,
		char const* const szName)
		: CBaseTriggerConnection(EType::Snapshot, szName)
		, m_id(id)
		, m_actionType(actionType)
		, m_pEventDescription(pEventDescription)
	{}

#else
	explicit CSnapshot(
		uint32 const id,
		EActionType const actionType,
		FMOD::Studio::EventDescription* const pEventDescription)
		: CBaseTriggerConnection(EType::Snapshot)
		, m_id(id)
		, m_actionType(actionType)
		, m_pEventDescription(pEventDescription)
	{}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	virtual ~CSnapshot() override = default;

	// CryAudio::Impl::ITriggerConnection
	virtual ETriggerResult Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId) override;
	virtual void           Stop(IObject* const pIObject) override;
	// ~CryAudio::Impl::ITriggerConnection

private:

	uint32 const                          m_id;
	EActionType const                     m_actionType;
	FMOD::Studio::EventDescription* const m_pEventDescription;
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
