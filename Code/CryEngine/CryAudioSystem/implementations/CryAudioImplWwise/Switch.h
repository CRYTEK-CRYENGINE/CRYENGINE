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
class CSwitch final : public ISwitchStateConnection, public CPoolObject<CSwitch, stl::PSyncNone>
{
public:

	CSwitch() = delete;
	CSwitch(CSwitch const&) = delete;
	CSwitch(CSwitch&&) = delete;
	CSwitch& operator=(CSwitch const&) = delete;
	CSwitch& operator=(CSwitch&&) = delete;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	explicit CSwitch(
		AkUInt32 const switchGroupId,
		AkUInt32 const switchId,
		char const* const szSwitchGroupName,
		char const* const szSwitchName)
		: m_switchGroupId(switchGroupId)
		, m_switchId(switchId)
		, m_switchGroupName(szSwitchGroupName)
		, m_switchName(szSwitchName)
	{}
#else
	explicit CSwitch(
		AkUInt32 const switchGroupId,
		AkUInt32 const switchId)
		: m_switchGroupId(switchGroupId)
		, m_switchId(switchId)
	{}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	virtual ~CSwitch() override = default;

	// ISwitchStateConnection
	virtual void Set(IObject* const pIObject) override;
	virtual void SetGlobally() override;
	// ~ISwitchStateConnection

private:

	AkUInt32 const m_switchGroupId;
	AkUInt32 const m_switchId;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_switchGroupName;
	CryFixedStringT<MaxControlNameLength> const m_switchName;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
