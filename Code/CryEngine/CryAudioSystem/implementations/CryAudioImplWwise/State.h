// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ISwitchStateConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CState final : public ISwitchStateConnection, public CPoolObject<CState, stl::PSyncNone>
{
public:

	CState() = delete;
	CState(CState const&) = delete;
	CState(CState&&) = delete;
	CState& operator=(CState const&) = delete;
	CState& operator=(CState&&) = delete;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	explicit CState(
		AkUInt32 const stateGroupId,
		AkUInt32 const stateId,
		char const* const szStateGroupName,
		char const* const szStateName)
		: m_stateGroupId(stateGroupId)
		, m_stateId(stateId)
		, m_stateGroupName(szStateGroupName)
		, m_stateName(szStateName)
	{}
#else
	explicit CState(
		AkUInt32 const stateGroupId,
		AkUInt32 const stateId)
		: m_stateGroupId(stateGroupId)
		, m_stateId(stateId)
	{}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	virtual ~CState() override = default;

	// ISwitchStateConnection
	virtual void Set(IObject* const pIObject) override;
	virtual void SetGlobally() override;
	// ~ISwitchStateConnection

private:

	AkUInt32 const m_stateGroupId;
	AkUInt32 const m_stateId;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_stateGroupName;
	CryFixedStringT<MaxControlNameLength> const m_stateName;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
